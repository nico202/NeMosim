/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <iostream>

#include <boost/tuple/tuple_comparison.hpp>

#include "WarpAddressTable.hpp"
#include "kernel.cu_h"


namespace boost {
	namespace tuples {


template<typename T1, typename T2, typename T3>
std::size_t
hash_value(const tuple<T1, T2, T3>& k)
{
	std::size_t seed = 0;
	boost::hash_combine(seed, boost::tuples::get<0>(k));
	boost::hash_combine(seed, boost::tuples::get<1>(k));
	boost::hash_combine(seed, boost::tuples::get<2>(k));
	return seed;
}

template<typename T1, typename T2, typename T3, typename T4>
std::size_t
hash_value(const tuple<T1, T2, T3, T4>& k)
{
	std::size_t seed = 0;
	boost::hash_combine(seed, boost::tuples::get<0>(k));
	boost::hash_combine(seed, boost::tuples::get<1>(k));
	boost::hash_combine(seed, boost::tuples::get<2>(k));
	boost::hash_combine(seed, boost::tuples::get<3>(k));
	return seed;
}

	} // end namespace tuples
} // end namespace boost


namespace nemo {
	namespace cuda {



SynapseAddress
WarpAddressTable::addSynapse(
		const DeviceIdx& source,
		pidx_t targetPartition,
		delay_t delay1,
		size_t nextFreeWarp)
{
	row_key rk(source.partition, source.neuron, targetPartition, delay1);
	unsigned& rowSynapses = m_rowSynapses[rk];
	unsigned column = rowSynapses % WARP_SIZE;
	rowSynapses += 1;

	key k(source.partition, source.neuron, delay1);
	std::vector<size_t>& warps = m_warps[k][targetPartition];

	if(column == 0) {
		/* Add synapse to a new warp */
		warps.push_back(nextFreeWarp);
		m_warpsPerNeuronDelay[k] += 1;
		return SynapseAddress(nextFreeWarp, column);
	} else {
		/* Add synapse to an existing partially-filled warp */
		return SynapseAddress(*warps.rbegin(), column);
	}
}



void
WarpAddressTable::reportWarpSizeHistogram(std::ostream& out) const
{
	unsigned total = 0;
	std::vector<unsigned> hist(WARP_SIZE+1, 0);

	for(boost::unordered_map<row_key, unsigned>::const_iterator i = m_rowSynapses.begin();
			i != m_rowSynapses.end(); ++i) {
		unsigned fullWarps = i->second / WARP_SIZE;
		unsigned partialWarp = i->second % WARP_SIZE;
		hist.at(WARP_SIZE) += fullWarps;
		total += fullWarps;
		if(partialWarp != 0) {
			hist.at(partialWarp) += 1;
			total += 1;
		}
	}
	for(unsigned size=1; size < WARP_SIZE+1; ++size) {
		unsigned count = hist.at(size);
		double percentage = double(100 * count) / double(total);
		out << size << ": " << count << "(" << percentage << "%)\n";
	}
	out << "total: " << total << std::endl;
}



unsigned
WarpAddressTable::warpsPerNeuronDelay(pidx_t p, nidx_t n, delay_t delay1) const
{
	typedef boost::unordered_map<key, unsigned>::const_iterator it;
	it i = m_warpsPerNeuronDelay.find(key(p,n, delay1));
	if(i != m_warpsPerNeuronDelay.end()) {
		return i->second;
	} else {
		return 0;
	}
}


	} // end namespace cuda
} // end namespace nemo