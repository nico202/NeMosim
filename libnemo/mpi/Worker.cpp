/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Worker.hpp"

#include <stdexcept>

#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>

#include "nemo_mpi_common.hpp"
#include "Mapper.hpp"
#include <NetworkImpl.hpp>


namespace nemo {
	namespace mpi {

void addNeuron(boost::mpi::communicator& world, nemo::NetworkImpl& net);


Worker::Worker(boost::mpi::communicator& world) :
	m_world(world),
	m_rank(world.rank()),
	ml_scount(0),
	mg_scount(0),
	m_ncount(0)
{
	int workers = m_world.size() - 1;
	Mapper mapper(workers);

	bool constructionDone = false;
	nemo::NetworkImpl net;
	int buf;

	while(!constructionDone) {
		boost::mpi::status msg = m_world.probe();
		switch(msg.tag()) {
			case NEURON_SCALAR: addNeuron(net); break;
			case SYNAPSE_VECTOR: addSynapseVector(mapper, net); break;
			case END_CONSTRUCTION: 
				world.recv(MASTER, END_CONSTRUCTION, buf);
				constructionDone = true;
				break;
			default:
				//! \todo deal with errors properly
				throw std::runtime_error("unknown tag");
		}
	}

	//! \todo now split net into local and global networks. Just strip net in-place.
	//! \todo exchange connectivity between nodes now

	m_world.barrier();

	std::clog << "Worker " << m_rank << " " << m_ncount << " neurons" << std::endl;
	std::clog << "Worker " << m_rank << " " << ml_scount << " local synapses" << std::endl;
	std::clog << "Worker " << m_rank << " " << mg_scount << " global synapses" << std::endl;
}


void
Worker::addNeuron(nemo::NetworkImpl& net)
{
	std::pair<nidx_t, nemo::Neuron<float> > n;
	m_world.recv(MASTER, NEURON_SCALAR, n);
	net.addNeuron(n.first, n.second);
	m_ncount++;
}



void
Worker::addSynapseVector(const Mapper& mapper, nemo::NetworkImpl& net)
{
	m_ss.clear();
	m_world.recv(MASTER, SYNAPSE_VECTOR, m_ss);
	for(std::vector<Synapse<unsigned, unsigned, float> >::const_iterator i = m_ss.begin();
			i != m_ss.end(); ++i) {
		const AxonTerminal<unsigned, float>& t = i->terminal;
		int targetRank = mapper.rankOf(t.target);
		if(targetRank == m_rank) {
			//! \todo add alternative addSynapse method which uses AxonTerminal directly
			net.addSynapse(i->source, t.target, i->delay, t.weight, t.plastic);
			ml_scount++;
		} else {
			m_targets.insert(t.target);
			m_fcm[i->source].insert(t.target);
			mg_scount++;
			//! \todo store L2 synapses on a per-rank basis for later exchange
		}
	}
}

	} // end namespace mpi
} // end namespace nemo