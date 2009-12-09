#undef STDP_FN
#ifdef STDP
#define STDP_FN(f) f ## _STDP
#else
#define STDP_FN(f) f ## _static
#endif

#define MAX_PARTITION_SIZE_static MAX_PARTITION_SIZE



//=============================================================================
// Shared memory buffers
//=============================================================================



/* Set shared memory array to fixed value */
__device__
void
STDP_FN(setSharedArray)(uint32_t* s_mem, uint32_t val)
{
	// the compiler should unroll this
	for(int i=0; i<DIV_CEIL(STDP_FN(MAX_PARTITION_SIZE), THREADS_PER_BLOCK); ++i) {
		s_mem[i*THREADS_PER_BLOCK + threadIdx.x] = val;
	}
}



__device__
void
STDP_FN(updateHistory)(uint s_partitionSize, uint64_t* s_recentFiring, uint64_t* g_recentFiring)
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
STDP_FN(fire)(
#ifdef __DEVICE_EMULATION__
	uint cycle,
#endif
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
						cycle, CURRENT_PARTITION, neuron,
						forceFiring, threadIdx.x);
				setFiringOutput(neuron);
			}

			g_v[neuron] = v;
			g_u[neuron] = u;
		}
	}
}



__device__
void
STDP_FN(deliverSpike)(
		//! \todo change to uint
		int presynaptic,
		uint maxDelay,
		//! \todo change to uint
		uint32_t delay,
		uint f0_pitch,
		uint synapseIdx,
		uint* gf0_address,
		float* gf0_weight,
		float* s_current)
{
	//! \todo factor addressing out into separate function
	size_t synapseAddress = (presynaptic * maxDelay + delay) * f0_pitch + synapseIdx;
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
				cycle, presynaptic, postsynaptic, weight);
	}
}



//! \todo move out of kernel.cu, so as to avoid STDP_FN
__device__
void
STDP_FN(deliverL0Spikes_)(
#ifdef __DEVICE_EMULATION__
	uint cycle,
#endif
	uint maxDelay,
	uint partitionSize,
	uint sf0_maxSynapses,
	uint* gf0_cm, uint f0_pitch, uint f0_size,
	uint64_t* s_recentFiring,
	uint64_t* g_firingDelays,
	float* s_current,
	uint16_t* s_firingIdx,
	uint32_t* s_arrivalBits,
	uint32_t* s_arrivals)
{
	uint*  gf0_address =          gf0_cm + FCM_ADDRESS  * f0_size;
	float* gf0_weight  = (float*) gf0_cm + FCM_WEIGHT   * f0_size;

	__shared__ int s_chunksPerDelay;

	if(threadIdx.x == 0) {
		s_chunksPerDelay = DIV_CEIL(sf0_maxSynapses, THREADS_PER_BLOCK);
	}
	__syncthreads();

	for(int preOffset=0; preOffset < partitionSize; preOffset += THREADS_PER_BLOCK) {

		__shared__ int s_firingCount;
		if(threadIdx.x == 0) {
			s_firingCount = 0;
		}
		__syncthreads();

		int candidate = preOffset + threadIdx.x;

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

			int presynaptic = s_firingIdx[i];

			__shared__ uint s_delays[MAX_DELAY];
			__shared__ uint s_delayCount;

			if(threadIdx.x == 0) {
				s_delayCount = 0;
			}
			__syncthreads();

			/* The common situation will be for there not to be too many delay
			 * blocks due for delivery. It's thus better to do this in parallel
			 * with shared atomics rather than a loop for a single thread. */
			if(threadIdx.x < MAX_DELAY) {
				if(s_arrivalBits[i] & (0x1 << threadIdx.x)) {
					int nextFree = atomicAdd(&s_delayCount, 1);
					s_delays[nextFree] = threadIdx.x;
				}
			}
			__syncthreads();

			/* The delay pitch may vary between networks, or between partitions.
			 * Even with sequential processing of presynaptic neurons, we want to
			 * maximise parallel processing of incoming spikes from different
			 * delays. We have two situations: 
			 *
			 * 1) if the delay pitch is more than half the block size we process
			 *    each delay sequentially
			 * 2) if the delay pitch is less than or equal to half the block size
			 *    we process multiple delays in parallel 
			 */
			for(uint delayIdx = 0; delayIdx < s_delayCount; ++delayIdx) {

				uint32_t delay = s_delays[delayIdx];
				for(uint chunk = 0; chunk < s_chunksPerDelay; ++chunk) {

					uint synapseIdx = chunk * THREADS_PER_BLOCK + threadIdx.x;

					//! \todo consider using per-neuron maximum here instead
					if(synapseIdx < sf0_maxSynapses) {
						STDP_FN(deliverSpike)(presynaptic, maxDelay, delay, f0_pitch, synapseIdx,
								gf0_address, gf0_weight, s_current);
					}
					__syncthreads();
				}
			}
		}
	}
	__syncthreads();
}




/*! Combined integrate and fire using sparse connectivity matrix, a single step
* updates the state (u and v) of each neuron and produces spikes to be used in
* the next simulation cycle. 
* 
* The number of neurons per block provided to the kernel is always
* warp-aligned. This means that some threads do useless work, but at no cost.
* Using a warp-aligned neuron number simplifies the control when the number of
* neurons is not an exact multiple of the number of threads per block.
*
 * The parameters (a, b, c, and d) can be set for each individual neuron and
 * should be pre-loaded in global memory before this kernel is invoked.
 */
__global__
void
STDP_FN(step) (
		int substeps,
		uint32_t cycle,
		uint64_t* g_recentFiring,
		// neuron state
		float* g_neuronParameters,
		unsigned* g_rngState,
		//! \todo combine with g_neuronParameters
		float* g_sigma,
		size_t neuronParametersSize,
		// connectivity
		uint* gf0_cm, uint64_t* gf0_delays,
		uint* gf1_cm, uint64_t* gf1_delays,
		// L1 spike queue
		uint2* gSpikeQueue,
		size_t sqPitch,
		unsigned int* gSpikeQueueHeads,
		size_t sqHeadPitch,
		// firing stimulus
		uint32_t* g_fstim,
		size_t pitch1,
#ifdef KERNEL_TIMING
		// cycle counting
		unsigned long long* g_cycleCounters,
		size_t ccPitch,
#endif
		uint32_t* firingOutput) // already offset to current cycle
{
	SET_COUNTER(s_ccMain, 0);

	/* The shared memory is allocated in fixed-sized blocks. During the
	 * different stages of the kernel each block may be used for different
	 * purposes. */

	/* Per-neuron buffers */
	__shared__ uint32_t s_M1KA[STDP_FN(MAX_PARTITION_SIZE)];
	__shared__ uint64_t s_M1KB[STDP_FN(MAX_PARTITION_SIZE)];

	/* Per-thread buffers */
	__shared__ uint16_t s_T16[THREADS_PER_BLOCK];
	__shared__ uint32_t s_T32[THREADS_PER_BLOCK];

	/* Per-delay buffer */
	__shared__ uint32_t s_D32[MAX_DELAY];

	/* Per-partition buffer */
	__shared__ uint32_t s_P32[MAX_PARTITION_COUNT];

	uint64_t* s_recentFiring = s_M1KB;

	/* Per-partition parameters */
	__shared__ uint s_partitionSize;
	__shared__ uint sf0_maxSynapsesPerDelay;
	__shared__ uint sf1_maxSynapsesPerDelay;
	__shared__ float s_substepMult;

	if(threadIdx.x == 0) {
		s_partitionSize = c_partitionSize[CURRENT_PARTITION];
		sf0_maxSynapsesPerDelay = cf0_maxSynapsesPerDelay[CURRENT_PARTITION];
		sf1_maxSynapsesPerDelay = cf1_maxSynapsesPerDelay[CURRENT_PARTITION];
		s_substepMult = 1.0f / __int2float_rn(substeps);
    }
	__syncthreads();

	loadNetworkParameters();

#ifdef STDP
	loadStdpParameters();
#endif
	/* Within a connection matrix plane, partitionRow is the row offset of the
	 * current partition. The offset in /words/ differ between forward/reverse
	 * and level 0/1 as they have different row pitches */
	size_t f_partitionRow = CURRENT_PARTITION * s_maxPartitionSize * s_maxDelay;

	SET_COUNTER(s_ccMain, 1);

    //! \todo no need to clear array here, if loading thalamic input
	STDP_FN(setSharedArray)(s_M1KA, 0);
	float* s_current = (float*) s_M1KA;
    if(g_rngState != NULL && g_sigma != NULL) {
        thalamicInput(s_partitionSize,
                neuronParametersSize,
                s_pitch32,
                g_rngState,
                g_sigma,
                s_current);
    }

	SET_COUNTER(s_ccMain, 2);

	loadSharedArray(s_partitionSize,
			s_pitch64,
			g_recentFiring + readBuffer(cycle) * PARTITION_COUNT * s_pitch64,
			s_recentFiring);
	__syncthreads();

	SET_COUNTER(s_ccMain, 3);

	bool haveL1 = gSpikeQueue != NULL;
	if(haveL1) {
		STDP_FN(gatherL1Spikes_JIT_)(
				readBuffer(cycle),
				gSpikeQueue,
				sqPitch,
				gSpikeQueueHeads,
				sqHeadPitch,
				s_current,
				s_P32);
	}

	SET_COUNTER(s_ccMain, 4);

	STDP_FN(deliverL0Spikes_)(
#ifdef __DEVICE_EMULATION__
			cycle,
#endif
			s_maxDelay,
			s_partitionSize,
			sf0_maxSynapsesPerDelay,
			gf0_cm + f_partitionRow * sf0_pitch, sf0_pitch, sf0_size,
			s_recentFiring,
			gf0_delays + CURRENT_PARTITION * s_pitch64,
			s_current, s_T16, s_T32, s_D32);

	SET_COUNTER(s_ccMain, 5);

	/* The dense firing output is staged in shared memory before being written
	 * to global memory */
	clearFiringOutput();

	//__shared__ uint32_t s_fstim[DIV_CEIL(STDP_FN(MAX_PARTITION_SIZE), 32)];
	//! \todo use the same buffer for both input and output
	/* Make sure s_T16 is large enough */
	uint32_t* s_fstim = (uint32_t*) s_T16;
	bool hasExternalInput = g_fstim != 0;
	ASSERT(THREADS_PER_BLOCK/2 >= DIV_CEIL(STDP_FN(MAX_PARTITION_SIZE), 32));
	loadExternalFiring(hasExternalInput, s_partitionSize, pitch1, g_fstim, s_fstim);

	STDP_FN(fire)(
#ifdef __DEVICE_EMULATION__
			cycle,
#endif
			s_partitionSize,
			substeps, s_substepMult,
			pitch1,
			g_neuronParameters + CURRENT_PARTITION * s_pitch32,
			neuronParametersSize,
			s_current, 
			s_fstim);

	__syncthreads();

	writeFiringOutput(firingOutput + CURRENT_PARTITION * pitch1, pitch1);

	SET_COUNTER(s_ccMain, 6);

#ifdef STDP
	updateSTDP_(
			cycle,
			false,
			s_recentFiring,
			s_recentFiring,
			s_pitch32,
			s_partitionSize,
			cr0_address, cr0_stdp, cr0_pitch,
			s_T32);
#endif
	SET_COUNTER(s_ccMain, 7);
#ifdef STDP
	if(haveL1) {
		updateSTDP_(
				cycle,
				true,
				g_recentFiring + readBuffer(cycle) * PARTITION_COUNT * s_pitch64,
				s_recentFiring,
				s_pitch64,
				s_partitionSize,
				cr1_address, cr1_stdp, cr1_pitch,
				s_T32);
	}
#endif
	SET_COUNTER(s_ccMain, 8);

	/* We need the (updated) recent firing history for L1 spike
	 * delivery later, but won't update this further, so we can write
	 * back to global memory now. */
	STDP_FN(updateHistory)(s_partitionSize, s_recentFiring,
			g_recentFiring
				+ writeBuffer(cycle) * PARTITION_COUNT * s_pitch64
				+ CURRENT_PARTITION * s_pitch64);
	//! \todo add an additional counter?

	if(haveL1) {
		STDP_FN(deliverL1Spikes_JIT)(
				s_maxDelay,
                writeBuffer(cycle),
				s_partitionSize,
				//! \todo need to call this differently from wrapper
				sf1_maxSynapsesPerDelay,
				gf1_cm + f_partitionRow * sf1_pitch, sf1_pitch, sf1_size,
				s_recentFiring,
				gf1_delays + CURRENT_PARTITION * s_pitch64,
				(uint2*) s_M1KA, // used for s_current previously, now use for staging outgoing spikes
				//! \todo compile-time assertions to make sure we're not overflowing here
				//! \todo fix naming!
				gSpikeQueue,
				sqPitch,
				gSpikeQueueHeads,
				sqHeadPitch,
				s_T16, s_T32, s_D32, s_P32);
	}

	SET_COUNTER(s_ccMain, 9);
	WRITE_COUNTERS(s_ccMain, g_cycleCounters, ccPitch, CC_MAIN_COUNT);
}

