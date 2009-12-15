#include "cycle.cu"
#include "dispatchTable.cu"

//=============================================================================
// Double buffering
//=============================================================================

/* The current cycle indicates which half of the double buffer is for reading
 * and which is for writing */
__device__
uint
readBuffer(uint cycle)
{
    return (cycle & 0x1) ^ 0x1;
}


__device__
uint
writeBuffer(uint cycle)
{
    return cycle & 0x1;
}



//=============================================================================
// Firing
//=============================================================================


/*! The external firing stimulus is densely packed with one bit per neuron.
 * Thus only the low-order threads need to read this data, and we need to
 * sync.  */
__device__
void
loadExternalFiring(
        bool hasExternalInput,
		int s_partitionSize,
		size_t pitch,
		uint32_t* g_firing,
		uint32_t* s_firing)
{
	if(threadIdx.x < DIV_CEIL(s_partitionSize, 32)) {
		if(hasExternalInput) {
			s_firing[threadIdx.x] =
                g_firing[blockIdx.x * pitch + threadIdx.x];
		} else {
			s_firing[threadIdx.x] = 0;
		}
	}
	__syncthreads();
}



template<typename T>
__device__
void
loadSharedArray(int partitionSize, size_t pitch, T* g_arr, T* s_arr)
{
	for(uint nbase=0; nbase < partitionSize; nbase += THREADS_PER_BLOCK) {
		uint neuron = nbase + threadIdx.x;
		if(neuron < partitionSize) {
			s_arr[neuron] = g_arr[(blockIdx.x * pitch) + neuron];
		}
	}
}



//=============================================================================
// Shared memory buffers
//=============================================================================



/* Set shared memory array to fixed value */
__device__
void
setSharedArray(uint32_t* s_mem, uint32_t val)
{
	// the compiler should unroll this
	for(int i=0; i<DIV_CEIL(MAX_PARTITION_SIZE, THREADS_PER_BLOCK); ++i) {
		s_mem[i*THREADS_PER_BLOCK + threadIdx.x] = val;
	}
}



__device__
void
updateHistory(uint s_partitionSize, uint64_t* s_recentFiring, uint64_t* g_recentFiring)
{
	for(uint nbase=0; nbase < s_partitionSize; nbase += THREADS_PER_BLOCK) {
		uint neuron = nbase + threadIdx.x;
		if(neuron < s_partitionSize) {
			/* Need to update firing history here as we need it in L1 delivery,
			 * so we can handle 1-cycle delay */
			s_recentFiring[neuron] = (s_recentFiring[neuron] << 1) | (didFire(neuron) ? 0x1 : 0x0);
			g_recentFiring[neuron] = s_recentFiring[neuron];
		}
	}
	__syncthreads();
}




__device__
void
fire(
	uint s_partitionSize,
	uint substeps,
	float substepMult, // substepMul * substeps = 1
	size_t pitch1, //! \todo move into shared memory along with other pitches
	float* g_neuronParameters,
	size_t neuronParametersSize,
	// input
	float* s_current,    // input current
	// buffers
	uint32_t* s_fstim)   // s_T16, so larger than needed
{
	float* g_a = g_neuronParameters + PARAM_A * neuronParametersSize;
	float* g_b = g_neuronParameters + PARAM_B * neuronParametersSize;
	float* g_c = g_neuronParameters + PARAM_C * neuronParametersSize;
	float* g_d = g_neuronParameters + PARAM_D * neuronParametersSize;
	float* g_u = g_neuronParameters + STATE_U * neuronParametersSize;
	float* g_v = g_neuronParameters + STATE_V * neuronParametersSize;

	for(uint nbase=0; nbase < s_partitionSize; nbase += THREADS_PER_BLOCK) {

		uint neuron = nbase + threadIdx.x;

		if(neuron < s_partitionSize) {

			float v = g_v[neuron];
			float u = g_u[neuron];
			float a = g_a[neuron];
			float b = g_b[neuron];
			float I = s_current[neuron];

			/* n sub-steps for numerical stability, with u held */
			bool fired = false;
			for(int j=0; j<substeps; ++j) {
				if(!fired) { 
					v += substepMult * ((0.04f*v + 5.0f) * v + 140.0f - u + I);
					/*! \todo: could pre-multiply this with a, when initialising memory */
					u += substepMult * (a * ( b*v - u ));
					fired = v >= 30.0f;
				} 
			}

			/* s_fstim accessed using broadcast */
			bool forceFiring = (s_fstim[neuron/32] >> (neuron % 32)) & 0x1;

			if(fired || forceFiring) {

				/* Only a subset of the neurons fire and thus require c/d
				 * fetched from global memory. One could therefore deal with
				 * all the fired neurons separately. This was found, however,
				 * to slow down the fire step by 50%, due to extra required
				 * synchronisation.  */
				//! \todo could probably hard-code c
				v = g_c[neuron];
				u += g_d[neuron];

				DEBUG_MSG("c%u %u-%u fired (forced: %u) (thread %u)\n",
						s_cycle, CURRENT_PARTITION, neuron,
						forceFiring, threadIdx.x);
				setFiringOutput(neuron);
			}

			g_v[neuron] = v;
			g_u[neuron] = u;
		}
	}
}



__device__
size_t
synapseAddress(
		uint presynaptic,
		uint maxDelay,
		uint delay,
		uint f0_pitch,
		uint synapseIdx)
{
	return (presynaptic * maxDelay + delay) * f0_pitch + synapseIdx;
}



__device__
void
deliverSpike(
		size_t synapseAddress,
		uint presynaptic,
		uint* gf0_address,
		float* gf0_weight,
		float* s_current)
{
	//! \todo factor addressing out into separate function
	//size_t synapseAddress = (presynaptic * maxDelay + delay) * f0_pitch + synapseIdx;
	float weight = gf0_weight[synapseAddress];

	/*! \todo only load address if it will actually be used.  For benchmarks
	 * this made little difference, presumably because all neurons have same
	 * number of synapses.  Experiment! */
	uint sdata = gf0_address[synapseAddress];
	uint postsynaptic = targetNeuron(sdata);

	bool doCommit = weight != 0.0f;

	/* Since multiple spikes may terminate at the same postsynaptic neuron,
	 * some care must be taken to avoid a race condition in the current update.
	 *
	 * Since we only deal with a single delay at a time, there should be no
	 * race conditions resulting from multiple synapses terminating at the same
	 * postsynaptic neuron.  Within a single delay, there should be no race
	 * conditions, if the mapper has done its job */

	if(doCommit) {
		s_current[postsynaptic] += weight;
		DEBUG_MSG("c%u L0 n%u -> n%u %+f\n",
				s_cycle, presynaptic, postsynaptic, weight);
	}
}




/*! For a given word containing arrival bits, i.e. bits indicating for what
 * delays spikes are due for delivery, set a vector of relevant delays.
 *
 * \param s_delayCount
 *		Shared memory scalar
 * \param s_delays
 *		Shared memory vector at least as long as maximum delays
 */
__device__
void
listDelays_(uint64_t arrivalBits, uint* s_delayCount, uint* s_delays)
{
	if(threadIdx.x == 0) {
		*s_delayCount = 0;
	}
	__syncthreads();

	/* The common situation will be for there not to be too many delay blocks
	 * due for delivery. It's thus better to do this in parallel with shared
	 * atomics rather than a loop for a single thread. */
	if(threadIdx.x < MAX_DELAY) {
		if(arrivalBits & (0x1 << threadIdx.x)) {
			int nextFree = atomicAdd(s_delayCount, 1);
			s_delays[nextFree] = threadIdx.x;
		}
	}
	__syncthreads();
}



//! \todo move out of kernel.cu, so as to avoid STDP_FN
__device__
void
deliverL0Spikes_(
	uint maxDelay,
	uint partitionSize,
	uint sf0_maxSynapses,
	uint* gf0_cm, uint f0_pitch_in, uint f0_size,
	uint64_t* s_recentFiring,
	uint64_t* g_firingDelays,
	float* s_current,
	uint16_t* s_firingIdx,
	uint32_t* s_arrivalBits,
	uint32_t* s_arrivals)
{
#ifndef NEW_FCM
	uint*  gf0_address =          gf0_cm + FCM_ADDRESS  * f0_size;
	float* gf0_weight  = (float*) gf0_cm + FCM_WEIGHT   * f0_size;
#endif

	__shared__ int s_chunksPerDelay;

	if(threadIdx.x == 0) {
		s_chunksPerDelay = DIV_CEIL(sf0_maxSynapses, THREADS_PER_BLOCK);
	}
	__syncthreads();

	//! \todo load dispatch tables from tmem to smem here

	for(uint preOffset=0; preOffset < partitionSize; preOffset += THREADS_PER_BLOCK) {

		__shared__ int s_firingCount;
		if(threadIdx.x == 0) {
			s_firingCount = 0;
		}
		__syncthreads();

		uint candidate = preOffset + threadIdx.x;

		/* It might seem a good idea to load firing delays from global memory
		 * inside the if-clause, so as to avoid memory traffic when little
		 * firing occurs.  In practice, however, this was found to increase
		 * execution time (when not firing) by 68%. It's not clear why this is
		 * so. */ 
		uint64_t arrivals = s_recentFiring[candidate] & g_firingDelays[candidate];
		if(arrivals && candidate < partitionSize) {
			int nextFree = atomicAdd(&s_firingCount, 1);
			s_firingIdx[nextFree] = candidate;
			s_arrivalBits[nextFree] = arrivals;
		}
		__syncthreads();

		/* We now have the indices of the firing of THREADS_PER_BLOCK
		 * presynaptic neurons */
		for(int i=0; i<s_firingCount; ++i) {

			uint presynaptic = s_firingIdx[i];

			__shared__ uint s_delays[MAX_DELAY];
			__shared__ uint s_delayCount;

			listDelays_(s_arrivalBits[i], &s_delayCount, s_delays);

			for(uint delayIdx = 0; delayIdx < s_delayCount; ++delayIdx) {

				uint delay = s_delays[delayIdx];

#ifdef NEW_FCM
				//! \todo load this for all relevant delays earlier, at the beginning of the function
				// share the loading code with applyStdp
				__shared__ fcm_ref_t fcm;
				if(threadIdx.x == 0) {
					fcm = getFCM(CURRENT_PARTITION, delay);
					ASSERT(f0_base(fcm) != 0x0);
				}
				__syncthreads();
#endif

				for(uint chunk = 0; chunk < s_chunksPerDelay; ++chunk) {

					uint synapseIdx = chunk * THREADS_PER_BLOCK + threadIdx.x;

					//! \todo consider using per-neuron maximum here instead (or as well)
#ifdef NEW_FCM
					if(synapseIdx < f0_pitch(fcm)) {
						deliverSpike(
								f_synapseOffset(presynaptic, f0_pitch(fcm), synapseIdx),
								presynaptic, f0_address(fcm), f0_weights(fcm), s_current);
					}
#else
					if(synapseIdx < sf0_maxSynapses) {
						deliverSpike(
								synapseAddress(presynaptic, maxDelay, delay, f0_pitch_in, synapseIdx),
								presynaptic, gf0_address, gf0_weight, s_current);
					}
#endif
					__syncthreads();
				}
			}
		}
	}
	__syncthreads();
}
