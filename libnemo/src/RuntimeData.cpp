#include "RuntimeData.hpp"

#include <vector>
#include <assert.h>

#include "FiringOutput.hpp"
#include "ConnectivityMatrix.hpp"
#include "CycleCounters.hpp"
#include "NeuronParameters.hpp"
#include "ThalamicInput.hpp"
#include "util.h"
#include "log.hpp"
#include "fixedpoint.hpp"
#include "bitvector.hpp"
#include "except.hpp"

#include "partitionConfiguration.cu_h"
#include "kernel.cu_h"
#include "error.cu_h"




RuntimeData::RuntimeData(
		size_t maxPartitionSize,
		bool setReverse,
		unsigned int maxReadPeriod) :
	m_partitionCount(0),
	m_maxPartitionSize(maxPartitionSize),
	m_neurons(new NeuronParameters(maxPartitionSize)),
	m_cm(new ConnectivityMatrix(maxPartitionSize, setReverse)),
	m_recentFiring(NULL),
	m_thalamicInput(NULL),
	m_firingStimulus(NULL),
	m_firingOutput(NULL),
	m_cycleCounters(NULL),
	m_pitch32(0),
	m_pitch64(0),
	m_deviceDirty(true),
	m_maxReadPeriod(maxReadPeriod)
{

	int device;
	cudaGetDevice(&device);
	cudaGetDeviceProperties(&m_deviceProperties, device);
}



RuntimeData::~RuntimeData()
{
	//! \todo used shared_ptr instead to deal with this
	if(m_firingOutput) delete m_firingOutput;
	if(m_recentFiring) delete m_recentFiring;
	if(m_firingStimulus) delete m_firingStimulus;
	if(m_thalamicInput) delete m_thalamicInput;
	if(m_cycleCounters) delete m_cycleCounters;
	delete m_cm;
	delete m_neurons;
}



extern void
configureStdp(
		uint preFireWindow,
		uint postFireWindow,
		uint64_t potentiationBits,
		uint64_t depressionBits,
		weight_dt* stdpFn);



void
RuntimeData::configureStdp()
{
	if(!stdpFn.enabled()) {
		return;
	}

	const std::vector<float>& flfn = stdpFn.function();
	std::vector<fix_t> fxfn(flfn.size());
	uint fb = m_cm->fractionalBits();
	for(uint i=0; i < fxfn.size(); ++i) {
		fxfn.at(i) = fixedPoint(flfn[i], fb);
	}
	::configureStdp(stdpFn.preFireWindow(),
			stdpFn.postFireWindow(),
			stdpFn.potentiationBits(),
			stdpFn.depressionBits(),
			const_cast<fix_t*>(&fxfn[0]));
}



void
RuntimeData::moveToDevice()
{
	if(m_deviceDirty) {
		m_cm->moveToDevice();
		m_neurons->moveToDevice();
		configureStdp();
		m_partitionCount = m_neurons->partitionCount();
		m_firingOutput = new FiringOutput(m_partitionCount, m_maxPartitionSize, m_maxReadPeriod);
		m_recentFiring = new NVector<uint64_t>(m_partitionCount, m_maxPartitionSize, false, 2);
		//! \todo seed properly from outside function
		m_thalamicInput = new ThalamicInput(m_partitionCount, m_maxPartitionSize, 0);
		m_neurons->setSigma(*m_thalamicInput);
		m_thalamicInput->moveToDevice();
		m_cycleCounters = new CycleCounters(m_partitionCount, m_deviceProperties.clockRate);
		m_firingStimulus = new NVector<uint32_t>(m_partitionCount, BV_WORD_PITCH, false);

		setPitch();
	    m_deviceDirty = false;
	}
}



bool
RuntimeData::deviceDirty() const
{
	return m_deviceDirty;
}



/*! Copy firing stimulus from host to device. Array indices only tested in
 * debugging mode.
 * 
 * \param count
 *		Number of neurons whose firing should be forced
 * \param nidx
 * 		Neuron indices of neurons whose firing should be forced
 *
 * \return 
 *		Pointer to pass to kernel (which is NULL if there's no firing data).
 */
uint32_t*
RuntimeData::setFiringStimulus(size_t count, const uint* nidx)
{
	if(count == 0) 
		return NULL;

	//! \todo use internal host buffer with pinned memory instead
	size_t pitch = m_firingStimulus->wordPitch();
	std::vector<uint32_t> hostArray(m_firingStimulus->size(), 0);

	for(size_t i=0; i < count; ++i){
		//! \todo share this translation with NeuronParameters and CMImpl
		size_t nn = nidx[i] % m_maxPartitionSize;
		size_t pn = nidx[i] / m_maxPartitionSize;
		//! \todo should check the size of this particular partition
		assert(nn < m_maxPartitionSize );
		assert(pn < m_partitionCount);
		size_t word = pn * pitch + nn / 32;
		size_t bit = nn % 32;
		hostArray[word] |= 1 << bit;
	}

	CUDA_SAFE_CALL(cudaMemcpy(
				m_firingStimulus->deviceData(),
				&hostArray[0],
				m_partitionCount * m_firingStimulus->bytePitch(),
				cudaMemcpyHostToDevice));

	return m_firingStimulus->deviceData();
}



void
checkPitch(size_t expected, size_t found)
{
	if(expected != found) {
		ERROR("RuntimeData::checkPitch: pitch mismatch in device memory allocation. Found %d, expected %d\n",
				(int) found, (int) expected);
	}
}


size_t
RuntimeData::d_allocated() const
{
	size_t total = 0;
	total += m_firingStimulus ? m_firingStimulus->d_allocated()   : 0;
	total += m_recentFiring   ? m_recentFiring->d_allocated()     : 0;
	total += m_neurons        ? m_neurons->d_allocated()        : 0;
	total += m_firingOutput   ? m_firingOutput->d_allocated()     : 0;
	total += m_thalamicInput  ? m_thalamicInput->d_allocated()    : 0;
	total += m_cm             ? m_cm->d_allocated()             : 0;
	return total;
}


/* Set common pitch and check that all relevant arrays have the same pitch. The
 * kernel uses a single pitch for all 32-bit data */ 
void
RuntimeData::setPitch()
{
	size_t pitch1 = m_firingStimulus->wordPitch();
	m_pitch32 = m_neurons->wordPitch();
	m_pitch64 = m_recentFiring->wordPitch();
	//! \todo fold thalamic input into neuron parameters
	checkPitch(m_pitch32, m_thalamicInput->wordPitch());
	checkPitch(pitch1, m_firingOutput->wordPitch());
	bv_setPitch(pitch1);
}



//-----------------------------------------------------------------------------
// Timing
//-----------------------------------------------------------------------------


long
RuntimeData::elapsed()
{
    syncSimulation();
	return m_timer.elapsed();
}



void
RuntimeData::setStart()
{
	m_timer.reset();
}




//-----------------------------------------------------------------------------
// STDP
//-----------------------------------------------------------------------------


bool
RuntimeData::usingStdp() const
{
	return stdpFn.enabled();
}



void
RuntimeData::addNeuron(
		unsigned int idx,
		float a, float b, float c, float d,
		float u, float v, float sigma)
{
	m_neurons->addNeuron(idx, a, b, c, d, u, v, sigma);
}



void
RuntimeData::addSynapses(
		uint source,
		uint targets[],
		uint delays[],
		float weights[],
		uchar is_plastic[],
		size_t length)
{
	m_cm->setRow(source, targets, delays, weights, is_plastic, length);
}



void
RuntimeData::syncSimulation()
{
	CUDA_SAFE_CALL(cudaThreadSynchronize());
}



void
RuntimeData::startSimulation()
{
	if(deviceDirty()) {
		::clearAssertions();
		moveToDevice();
		//! \todo do this configuration as part of CM setup
		::configureKernel(m_cm->maxDelay(), m_pitch32, m_pitch64);
		setStart();
	}
}



//! \todo put this into separate header
void
stepSimulation(
		uint partitionCount,
		bool usingStdp,
		uint cycle,
		uint64_t* d_recentFiring,
		float* d_neuronState,
		unsigned* d_rngState,
		float* d_rngSigma,
		uint32_t* d_fstim,
		uint32_t* d_fout,
		synapse_t* d_fcm,
		uint* d_outgoingCount,
		outgoing_t* d_outgoing,
		uint* d_incomingHeads,
		incoming_t* d_incoming,
		unsigned long long* d_cc,
		size_t ccPitch);


status_t
RuntimeData::stepSimulation(size_t fstimCount, const uint* fstimIdx)
{
	startSimulation(); // only has effect on first cycle

	/* A 32-bit counter can count up to around 4M seconds which is around 1200
	 * hours or 50 days */
	//! \todo use a 64-bit counter instead
	if(m_cycle == ~0) {
		throw std::overflow_error("Cycle counter overflow");
	}
	m_cycle += 1;

	uint32_t* d_fstim = setFiringStimulus(fstimCount, fstimIdx);
	uint32_t* d_fout = m_firingOutput->step();
	::stepSimulation(
			m_partitionCount,
			usingStdp(),
			m_cycle,
			m_recentFiring->deviceData(),
			m_neurons->deviceData(),
			m_thalamicInput->deviceRngState(),
			m_thalamicInput->deviceSigma(),
			d_fstim, 
			d_fout,
			m_cm->d_fcm(),
			m_cm->outgoingCount(),
			m_cm->outgoing(),
			m_cm->incomingHeads(),
			m_cm->incoming(),
			m_cycleCounters->data(),
			m_cycleCounters->pitch());

    if(assertionsFailed(m_partitionCount, m_cycle)) {
        clearAssertions();
		//! \todo move these errors to libnemo.cpp as they are
		//only part of the C layer. 
        return KERNEL_ASSERTION_FAILURE;
    }

	cudaError_t status = cudaGetLastError();
	if(status != cudaSuccess) {
		//! \todo add cycle number?
		throw KernelInvocationError(status);
	}

	return KERNEL_OK;
}


//! \todo put this in proper header
void
applyStdp(
		unsigned long long* d_cc,
		size_t ccPitch,
		uint partitionCount,
		uint fractionalBits,
		synapse_t* d_fcm,
		const nemo::STDP<float>& stdpFn,
		float reward);

void
RuntimeData::applyStdp(float reward)
{
	if(deviceDirty()) {
		//! \todo issue a warning here?
		return; // we haven't even started simulating yet
	}

	if(!usingStdp()) {
		//! \todo issue a warning here?
		return;
	}

	if(reward == 0.0f) {
		m_cm->clearStdpAccumulator();
	} else  {
		::applyStdp(
				m_cycleCounters->dataApplySTDP(),
				m_cycleCounters->pitchApplySTDP(),
				m_partitionCount,
				m_cm->fractionalBits(),
				m_cm->d_fcm(),
				stdpFn,
				reward);
	}
}


void
RuntimeData::printCycleCounters()
{
	m_cycleCounters->printCounters();
}


void
RuntimeData::readFiring(uint** cycles,
		uint** neuronIdx,
		uint* nfired,
		uint* ncycles)
{
	m_firingOutput->readFiring(cycles, neuronIdx, nfired, ncycles);
}


void
RuntimeData::flushFiringBuffer()
{
	m_firingOutput->flushBuffer();
}
