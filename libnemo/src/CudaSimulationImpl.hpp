#ifndef NEMO_CUDA_SIMULATION_IMPL_HPP
#define NEMO_CUDA_SIMULATION_IMPL_HPP

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>

#include <nemo_config.h>
#include <STDP.hpp>
#include <Timer.hpp>
#include <types.h>
#include <ConfigurationImpl.hpp>

#include "Mapper.hpp"
#include "NVector.hpp"
#include "ConnectivityMatrix.hpp"
#include "CycleCounters.hpp"
#include "DeviceAssertions.hpp"
#include "FiringOutput.hpp"
#include "NeuronParameters.hpp"
#include "ThalamicInput.hpp"

namespace nemo {

	class NetworkImpl;

	namespace cuda {

class SimulationImpl
{
	public :

		SimulationImpl(
				const nemo::NetworkImpl& net,
				const nemo::ConfigurationImpl& conf);

		~SimulationImpl();

		/* CONFIGURATION */

		static int selectDevice();
		static int setDevice(int dev);

		static unsigned defaultPartitionSize();
		static unsigned defaultFiringBufferLength();

		unsigned getFiringBufferLength() const { return m_conf.cudaFiringBufferLength(); }

		/* NETWORK SIMULATION */

		/*! Copy firing stimulus from host to device, setting the (member
		 * variable) devce pointer containing firing stimulus. If there is no
		 * input data the pointer is NULL. Array indices only tested in
		 * debugging mode.
		 *
		 * \param nidx
		 * 		Neuron indices of neurons whose firing should be forced
		 */
		void setFiringStimulus(const std::vector<unsigned>& nidx);

		/*! Set per-neuron input current on the device and set the relevant
		 * member variable containing the device pointer. If there is no input
		 * the device pointer is NULL.
		 *
		 * This function should only be called once per cycle. */
		void setCurrentStimulus(const std::vector<float>& current);

		/*! Set per-neuron input current on the device and set the relevant
		 * member variable containing the device pointer. If there is no input
		 * the device pointer is NULL.
		 *
		 * This function should only be called once per cycle.
		 *
		 * Pre: the input vector uses the same fixed-point format as the device */
		void setCurrentStimulus(const std::vector<fix_t>& current);

		/*! Perform a single simulation step, using the any stimuli (firing and
		 * current) provided by the caller after the previous call to step */
		void step();

		void applyStdp(float reward);

		void getSynapses(unsigned sourceNeuron,
				const std::vector<unsigned>** targetNeuron,
				const std::vector<unsigned>** delays,
				const std::vector<float>** weights,
				const std::vector<unsigned char>** plastic);

		unsigned readFiring(const std::vector<unsigned>** cycles, const std::vector<unsigned>** nidx);

		void flushFiringBuffer();

		void finishSimulation();

		/* TIMING */
		unsigned long elapsedWallclock() const;
		unsigned long elapsedSimulation() const;
		void resetTimer();

	private :

		Mapper m_mapper;

		nemo::ConfigurationImpl m_conf;

		//! \todo add this to logging output
		/*! \return
		 * 		number of bytes allocated on the device
		 *
		 * It seems that cudaMalloc*** does not fail properly when running out
		 * of memory, so this value could be useful for diagnostic purposes */
		size_t d_allocated() const;

		NeuronParameters m_neurons;

		ConnectivityMatrix m_cm;

		NVector<uint64_t> m_recentFiring;

		ThalamicInput m_thalamicInput;

		/* Densely packed, one bit per neuron */
		NVector<uint32_t> m_firingStimulus;
		void clearFiringStimulus();

		NVector<fix_t> m_currentStimulus;
		void clearCurrentStimulus();

		/* The firing buffer keeps data for a certain duration. One bit is
		 * required per neuron (regardless of whether or not it's firing */
		FiringOutput m_firingOutput;

		CycleCounters m_cycleCounters;

		DeviceAssertions m_deviceAssertions;

		void setPitch();

		size_t m_pitch32;
		size_t m_pitch64;

		STDP<float> m_stdpFn;
		void configureStdp(const STDP<float>& stdp);
		bool usingStdp() const;

		static int s_device;

		Timer m_timer;

		/* Device pointers to simulation stimulus. The stimulus may be set
		 * separately from the step, hence member variables */
		uint32_t* md_fstim;
		fix_t* md_istim;
};

	} // end namespace cuda
} // end namespace nemo

#endif
