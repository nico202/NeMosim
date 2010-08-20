/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ConnectivityMatrix.hpp"

#include <algorithm>
#include <utility>
#include <stdlib.h>

#include <boost/tuple/tuple_comparison.hpp>
#include <boost/format.hpp>

#include <nemo/config.h>
#include "ConfigurationImpl.hpp"
#include "NetworkImpl.hpp"
#include "exception.hpp"
#include "fixedpoint.hpp"


namespace nemo {


Row::Row(const std::vector<AxonTerminal<nidx_t, weight_t> >& ss, unsigned fbits) :
	len(ss.size())
{
	FAxonTerminal<fix_t>* ptr;
#ifdef HAVE_POSIX_MEMALIGN
	//! \todo factor out the memory aligned allocation
	int error = posix_memalign((void**)&ptr,
			ASSUMED_CACHE_LINE_SIZE,
			ss.size()*sizeof(FAxonTerminal<fix_t>));
	if(error) {
		throw nemo::exception(NEMO_ALLOCATION_ERROR, "Failed to allocate CM row");
	}
#else
	ptr = (FAxonTerminal<fix_t>*) malloc(ss.size()*sizeof(FAxonTerminal<fix_t>));
#endif

	data = boost::shared_array< FAxonTerminal<fix_t> >(ptr, free);

	/* static/plastic flag is not needed in forward matrix */
	for(std::vector<nemo::AxonTerminal<nidx_t, weight_t> >::const_iterator si = ss.begin();
			si != ss.end(); ++si) {
		size_t i = si - ss.begin();
		ptr[i] = FAxonTerminal<fix_t>(fx_toFix(si->weight, fbits), si->target);
	}
}



ConnectivityMatrix::ConnectivityMatrix(const ConfigurationImpl& conf) :
	m_fractionalBits(0),
	m_maxDelay(0)
{
	//! \todo implement auto-configuration of fixed-point format
	if(!conf.fractionalBitsSet()) {
		throw nemo::exception(NEMO_LOGIC_ERROR,
				"connectivity matrix class does not currently support auto-configuration of fixed-point format. Please call Configuration::setFractionalBits before creating simulation");
	}

	m_fractionalBits = conf.fractionalBits();
}



ConnectivityMatrix::ConnectivityMatrix(
		const NetworkImpl& net,
		const ConfigurationImpl& conf,
		const mapper_t& mapper) :
	m_fractionalBits(conf.fractionalBitsSet() ? conf.fractionalBits() : net.fractionalBits()),
	m_maxDelay(0)
{
	for(std::map<nidx_t, NetworkImpl::axon_t>::const_iterator ni = net.m_fcm.begin();
			ni != net.m_fcm.end(); ++ni) {
		nidx_t source = mapper.localIdx(ni->first);
		const NetworkImpl::axon_t& axon = ni->second;
		for(NetworkImpl::axon_t::const_iterator ai = axon.begin();
				ai != axon.end(); ++ai) {
			setRow(source, ai->first, ai->second, mapper);
		}
	}

	finalize(mapper);
}



Row&
ConnectivityMatrix::setRow(
		nidx_t source,
		delay_t delay,
		const std::vector<AxonTerminal<nidx_t, weight_t> >& ss,
		const mapper_t& mapper)
{
	using boost::format;

	if(delay < 1) {
		throw nemo::exception(NEMO_INVALID_INPUT,
				str(format("Neuron %u has synapses with delay < 1 (%u)") % source % delay));
	}

	std::pair<std::map<fidx, Row>::iterator, bool> insertion =
		m_acc.insert(std::make_pair<fidx, Row>(fidx(source, delay), Row(ss, m_fractionalBits)));

	if(!insertion.second) {
		throw nemo::exception(NEMO_INVALID_INPUT, "Double insertion into connectivity matrix");
	}
	m_delays[source].insert(delay);
	m_maxDelay = std::max(m_maxDelay, delay);

	Row& row = insertion.first->second;
	for(size_t s=0; s < row.len; ++s) {
		nidx_t target = mapper.localIdx(row.data[s].target);
		row.data[s].target = target;
	}

	return row;
}


void
ConnectivityMatrix::finalize(const mapper_t& mapper)
{
	finalizeForward(mapper);
}



/* The fast lookup is indexed by source and delay. */
void
ConnectivityMatrix::finalizeForward(const mapper_t& mapper)
{
	if(mapper.neuronCount() == 0)
		return;

	nidx_t maxIdx = mapper.maxLocalIdx();
	m_cm.resize((maxIdx+1) * m_maxDelay);

	for(nidx_t n=0; n <= maxIdx; ++n) {
		for(delay_t d=1; d <= m_maxDelay; ++d) {
			std::map<fidx, Row>::const_iterator row = m_acc.find(fidx(n, d));
			if(row != m_acc.end()) {
				m_cm.at(addressOf(n,d)) = row->second;
			} else {
				/* Insertion into map does not invalidate existing iterators */
				m_cm.at(addressOf(n,d)) = Row(); // defaults to empty row
			}
			//! \todo can delete the map now
		}
	}
}



const Row&
ConnectivityMatrix::getRow(nidx_t source, delay_t delay) const
{
	return m_cm.at(addressOf(source, delay));
}


void
ConnectivityMatrix::getSynapses(
		unsigned source,
		std::vector<unsigned>& targets,
		std::vector<unsigned>& delays,
		std::vector<float>& weights,
		std::vector<unsigned char>& plastic) const
{
	targets.clear();
	delays.clear();
	weights.clear();
	plastic.clear();

	unsigned fbits = fractionalBits();

	for(delay_iterator d = delay_begin(source), d_end = delay_end(source);
			d != d_end; ++d) {
		const Row& ss = getRow(source, *d);
		for(unsigned i = 0; i < ss.len; ++i) {
			FAxonTerminal<fix_t> s = ss.data[i];
			targets.push_back(s.target);
			weights.push_back(fx_toFloat(s.weight, fbits));
			delays.push_back(*d);
			plastic.push_back(0);
		}
		//! \todo set plastic correctly as well
	}

}



ConnectivityMatrix::delay_iterator
ConnectivityMatrix::delay_begin(nidx_t source) const
{
	std::map<nidx_t, std::set<delay_t> >::const_iterator found = m_delays.find(source);
	if(found == m_delays.end()) {
		throw nemo::exception(NEMO_INVALID_INPUT, "Invalid source neuron");
	}
	return found->second.begin();
}



ConnectivityMatrix::delay_iterator
ConnectivityMatrix::delay_end(nidx_t source) const
{
	std::map<nidx_t, std::set<delay_t> >::const_iterator found = m_delays.find(source);
	if(found == m_delays.end()) {
		throw nemo::exception(NEMO_INVALID_INPUT, "Invalid source neuron");
	}
	return found->second.end();
}


} // namespace nemo
