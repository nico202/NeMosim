/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "NeuronParameters.hpp"

#include <vector>

#include <nemo/network/Generator.hpp>

#include "types.h"
#include "exception.hpp"
#include "kernel.hpp"
#include "Mapper.hpp"


namespace nemo {
	namespace cuda {


NeuronParameters::NeuronParameters(const network::Generator& net, Mapper& mapper) :
	m_param(mapper.partitionCount(), mapper.partitionSize(), true, false),
	m_state(mapper.partitionCount(), mapper.partitionSize(), true, false)
{
	std::map<pidx_t, nidx_t> maxPartitionNeuron;

	for(network::neuron_iterator i = net.neuron_begin(), i_end = net.neuron_end();
			i != i_end; ++i) {

		DeviceIdx dev = mapper.addIdx(i->first);
		const nemo::Neuron<float>& n = i->second;

		m_param.setNeuron(dev.partition, dev.neuron, n.a, PARAM_A);
		m_param.setNeuron(dev.partition, dev.neuron, n.b, PARAM_B);
		m_param.setNeuron(dev.partition, dev.neuron, n.c, PARAM_C);
		m_param.setNeuron(dev.partition, dev.neuron, n.d, PARAM_D);
		m_state.setNeuron(dev.partition, dev.neuron, n.u, STATE_U);
		m_state.setNeuron(dev.partition, dev.neuron, n.v, STATE_V);

		maxPartitionNeuron[dev.partition] =
			std::max(maxPartitionNeuron[dev.partition], dev.neuron);
	}

	m_param.copyToDevice();
	m_state.copyToDevice();
	configurePartitionSizes(maxPartitionNeuron);
}



void
NeuronParameters::configurePartitionSizes(const std::map<pidx_t, nidx_t>& maxPartitionNeuron)
{
	if(maxPartitionNeuron.size() == 0) {
		return;
	}

	size_t maxPidx = maxPartitionNeuron.rbegin()->first;
	std::vector<unsigned> partitionSizes(maxPidx+1, 0);

	for(std::map<pidx_t, nidx_t>::const_iterator i = maxPartitionNeuron.begin();
			i != maxPartitionNeuron.end(); ++i) {
		partitionSizes.at(i->first) = i->second + 1;
	}

	CUDA_SAFE_CALL(configurePartitionSize(&partitionSizes[0], partitionSizes.size()));
}



size_t
NeuronParameters::wordPitch() const
{
	size_t param_pitch = m_param.wordPitch();
	size_t state_pitch = m_state.wordPitch();
	if(param_pitch != state_pitch) {
		throw nemo::exception(NEMO_LOGIC_ERROR, "State and parameter data have different pitch");
	}
	return param_pitch;
}


	} // end namespace cuda
} // end namespace nemo
