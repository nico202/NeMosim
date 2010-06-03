/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CudaSimulationImpl.hpp"

#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <assert.h>

#include <exception.hpp>
#include <NetworkImpl.hpp>

#include "CycleCounters.hpp"
#include "DeviceAssertions.hpp"
#include "ThalamicInput.hpp"
#include "fixedpoint.hpp"
#include "bitvector.hpp"
#include "except.hpp"

#include "device_assert.cu_h"
#include "kernel.cu_h"
#include "kernel.hpp"



namespace nemo {
	namespace cuda {


SimulationImpl::SimulationImpl(
		const nemo::NetworkImpl& net,
		const nemo::ConfigurationImpl& conf) :
	m_mapper(net, conf.cudaPartitionSize()),
	m_conf(conf),
	//! \todo get rid of member variable
	m_partitionCount(0),
	m_neurons(net, m_mapper),
	m_cm(net, m_mapper, conf.cudaPartitionSize(), conf.loggingEnabled()),
	m_recentFiring(m_mapper.partitionCount(), conf.cudaPartitionSize(), false, 2),
	//! \todo remove redundant argument
	//! \todo seed properly from configuration
	m_thalamicInput(net, m_mapper, m_mapper.partitionCount(), conf.cudaPartitionSize()),
	m_firingStimulus(m_mapper.partitionCount(), BV_WORD_PITCH, false),
	m_firingOutput(m_mapper, conf.cudaFiringBufferLength()),
	m_cycleCounters(m_mapper.partitionCount(), usingStdp()),
	m_deviceAssertions(m_mapper.partitionCount()),
	m_pitch32(0),
	m_pitch64(0)
{
	configureStdp(conf.stdpFunction());

	//! \todo remove m_partitionCount member variable
	m_partitionCount = m_mapper.partitionCount();

	setPitch();
	//! \todo do this configuration as part of CM setup
	CUDA_SAFE_CALL(configureKernel(m_cm.maxDelay(), m_pitch32, m_pitch64));
}



SimulationImpl::~SimulationImpl()
{
	finishSimulation();
}


#ifdef __DEVICE_EMULATION__
int SimulationImpl::s_device = 0;
#else
int SimulationImpl::s_device = -1;
#endif

int
SimulationImpl::selectDevice()
{
	/*! \todo might want to use thread-local, rather than process-local storage
	 * for s_device in order to support multiple threads */
	if(s_device != -1) {
		return s_device;
	}

	int dev;
	cudaDeviceProp prop;
	prop.major = 1;
	prop.minor = 2;

	CUDA_SAFE_CALL(cudaChooseDevice(&dev, &prop));
	CUDA_SAFE_CALL(cudaGetDeviceProperties(&prop, dev));

	/* 9999.9999 is the 'emulation device' which is always present. Unless the
	 * library was built specifically for emulation mode, this should be
	 * considered an error. */
	if(prop.major == 9999 || prop.minor == 9999) {
		//! \todo perhaps throw exception instead?
		std::cerr << "No physical devices available" << std::endl;
		return -1;
	}

	// 1.2 required for shared memory atomics
	if(prop.major <= 1 && prop.minor < 2) {
		std::cerr << "No device with compute capability 1.2 available" << std::endl;
		return -1;
	}

	CUDA_SAFE_CALL(cudaSetDevice(dev));
	s_device = dev;
	return dev;
}



int
SimulationImpl::setDevice(int dev)
{
	cudaDeviceProp prop;
	cudaGetDeviceProperties(&prop, dev);

	//! \todo throw exceptions instead here?
	// 9999.9999 is the 'emulation device' which is always present
	if(prop.major == 9999 || prop.minor == 9999) {
		std::cerr << "No physical devices available" << std::endl;
		return -1;
	}

	// 1.2 required for shared memory atomics
	if(prop.major <= 1 && prop.minor < 2) {
		std::cerr << "Device has compute capability less than 1.2" << std::endl;
		return -1;
	}

	CUDA_SAFE_CALL(cudaSetDevice(dev));
	s_device = dev;
	return dev;

}



void
SimulationImpl::configureStdp(const STDP<float>& stdp)
{
	if(!stdp.enabled()) {
		return;
	}

	m_stdpFn = stdp;

	const std::vector<float>& flfn = m_stdpFn.function();
	std::vector<fix_t> fxfn(flfn.size());
	unsigned fb = m_cm.fractionalBits();
	for(unsigned i=0; i < fxfn.size(); ++i) {
		fxfn.at(i) = fx_toFix(flfn[i], fb);
	}
	CUDA_SAFE_CALL(
		::configureStdp(m_stdpFn.preFireWindow(),
			m_stdpFn.postFireWindow(),
			m_stdpFn.potentiationBits(),
			m_stdpFn.depressionBits(),
			const_cast<fix_t*>(&fxfn[0])));
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
SimulationImpl::setFiringStimulus(const std::vector<unsigned>& nidx)
{
	if(nidx.empty())
		return NULL;

	//! \todo use internal host buffer with pinned memory instead
	size_t pitch = m_firingStimulus.wordPitch();
	std::vector<uint32_t> hostArray(m_firingStimulus.size(), 0);

	for(std::vector<unsigned>::const_iterator i = nidx.begin();
			i != nidx.end(); ++i) {
		//! \todo should check that this neuron exists
		DeviceIdx dev = m_mapper.deviceIdx(*i);
		size_t word = dev.partition * pitch + dev.neuron / 32;
		size_t bit = dev.neuron % 32;
		hostArray[word] |= 1 << bit;
	}

	CUDA_SAFE_CALL(cudaMemcpy(
				m_firingStimulus.deviceData(),
				&hostArray[0],
				m_partitionCount * m_firingStimulus.bytePitch(),
				cudaMemcpyHostToDevice));

	return m_firingStimulus.deviceData();
}



void
checkPitch(size_t expected, size_t found)
{
	if(expected != found) {
		std::ostringstream msg;
		msg << "Simulation::checkPitch: pitch mismatch in device memory allocation. "
			"Found " << found << ", expected " << expected << std::endl;
		throw nemo::exception(NEMO_CUDA_MEMORY_ERROR, msg.str());
	}
}


size_t
SimulationImpl::d_allocated() const
{
	return m_firingStimulus.d_allocated()
		+ m_recentFiring.d_allocated()
		+ m_neurons.d_allocated()
		+ m_firingOutput.d_allocated()
		+ m_thalamicInput.d_allocated()
		+ m_cm.d_allocated();
}


/* Set common pitch and check that all relevant arrays have the same pitch. The
 * kernel uses a single pitch for all 32-bit data */ 
void
SimulationImpl::setPitch()
{
	size_t pitch1 = m_firingStimulus.wordPitch();
	m_pitch32 = m_neurons.wordPitch();
	m_pitch64 = m_recentFiring.wordPitch();
	//! \todo fold thalamic input into neuron parameters
	checkPitch(m_pitch32, m_thalamicInput.wordPitch());
	checkPitch(pitch1, m_firingOutput.wordPitch());
	CUDA_SAFE_CALL(bv_setPitch(pitch1));
}



//-----------------------------------------------------------------------------
// STDP
//-----------------------------------------------------------------------------


bool
SimulationImpl::usingStdp() const
{
	return m_stdpFn.enabled();
}




void
SimulationImpl::step(const std::vector<unsigned>& fstim)
{
	m_timer.step();

	uint32_t* d_fstim = setFiringStimulus(fstim);
	uint32_t* d_fout = m_firingOutput.step();
	::stepSimulation(
			m_partitionCount,
			usingStdp(),
			m_timer.elapsedSimulation(),
			m_recentFiring.deviceData(),
			m_neurons.deviceData(),
			m_thalamicInput.deviceRngState(),
			m_thalamicInput.deviceSigma(),
			d_fstim, 
			d_fout,
			m_cm.d_fcm(),
			m_cm.outgoingCount(),
			m_cm.outgoing(),
			m_cm.incomingHeads(),
			m_cm.incoming(),
			m_cycleCounters.data(),
			m_cycleCounters.pitch());

	cudaError_t status = cudaGetLastError();
	if(status != cudaSuccess) {
		//! \todo add cycle number?
		throw KernelInvocationError(status);
	}

	m_deviceAssertions.check(m_timer.elapsedSimulation());
}


void
SimulationImpl::applyStdp(float reward)
{
	if(!usingStdp()) {
		//! \todo issue a warning here?
		return;
	}

	if(reward == 0.0f) {
		m_cm.clearStdpAccumulator();
	} else  {
		::applyStdp(
				m_cycleCounters.dataApplySTDP(),
				m_cycleCounters.pitchApplySTDP(),
				m_partitionCount,
				m_cm.fractionalBits(),
				m_cm.d_fcm(),
				m_stdpFn.maxWeight(),
				m_stdpFn.minWeight(),
				reward);
	}

	m_deviceAssertions.check(m_timer.elapsedSimulation());
}



void
SimulationImpl::getSynapses(unsigned sn,
		const std::vector<unsigned>** tn,
		const std::vector<unsigned>** d,
		const std::vector<float>** w,
		const std::vector<unsigned char>** p)
{
	return m_cm.getSynapses(sn, tn, d, w, p);
}



unsigned
SimulationImpl::readFiring(
		const std::vector<unsigned>** cycles,
		const std::vector<unsigned>** nidx)
{
	return m_firingOutput.readFiring(cycles, nidx);
}


void
SimulationImpl::flushFiringBuffer()
{
	m_firingOutput.flushBuffer();
}


void
SimulationImpl::finishSimulation()
{
	//! \todo perhaps clear device data here instead of in dtor
	if(m_conf.loggingEnabled()) {
		m_cycleCounters.printCounters(std::cout);
		//! \todo add time summary
	}
}


unsigned long
SimulationImpl::elapsedWallclock() const
{
	CUDA_SAFE_CALL(cudaThreadSynchronize());
	return m_timer.elapsedWallclock();
}


unsigned long
SimulationImpl::elapsedSimulation() const
{
	return m_timer.elapsedSimulation();
}



void
SimulationImpl::resetTimer()
{
	CUDA_SAFE_CALL(cudaThreadSynchronize());
	m_timer.reset();
}



unsigned
SimulationImpl::defaultPartitionSize()
{
	return MAX_PARTITION_SIZE;
}



unsigned
SimulationImpl::defaultFiringBufferLength()
{
	return FiringOutput::defaultBufferLength();
}

	} // end namespace cuda
} // end namespace nemo
