#ifndef NEMO_CONNECTIVITY_MATRIX_HPP
#define NEMO_CONNECTIVITY_MATRIX_HPP

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <map>
#include <set>

#include <boost/tuple/tuple.hpp>
#include <boost/shared_array.hpp>
#include <boost/optional.hpp>

#include <nemo/config.h>
#include "types.hpp"
#include "Mapper.hpp"
#include "STDP.hpp"
#include "OutgoingDelays.hpp"

#define ASSUMED_CACHE_LINE_SIZE 64

namespace nemo {


/* The AxonTerminal in types.hpp includes 'plastic' specification. It's not
 * needed here. */
struct FAxonTerminal
{
	FAxonTerminal(nidx_t t, fix_t w) : target(t), weight(w) {}

	nidx_t target; 
	fix_t weight;
};



/* A row contains a number of synapses with a fixed source and delay. A
 * fixed-point format is used internally. The caller needs to specify the
 * format.  */
struct Row
{
	Row() : len(0) {}

	/* \post synapse order is the same as in input vector */
	Row(const std::vector<FAxonTerminal>& ss);

	size_t len;
	//! \todo do own memory management here, and use raw pointers
	boost::shared_array< FAxonTerminal> data;

	const FAxonTerminal& operator[](unsigned i) const { return data[i]; }
};


namespace network {
	class Generator;
}

class ConfigurationImpl;
struct AxonTerminalAux;


/*! \todo Split this into a construction-time and run-time class. Currently
 * using this class is a bit clumsy, as some functions should only really be
 * accessed at construction time, while others should only be accessed at
 * run-time. This constraint is not enforced by the interface */

/* Generic connectivity matrix
 *
 * Data in this class is organised for optimal cache performance. A
 * user-defined fixed-point format is used.
 */
class NEMO_BASE_DLL_PUBLIC ConnectivityMatrix
{
	public:

		typedef Mapper<nidx_t, nidx_t> mapper_t;

		ConnectivityMatrix(const ConfigurationImpl& conf, const mapper_t&);

		/*! Populate runtime CM from existing network.
		 *
		 * The mapper can translate neuron indices (both source and target)
		 * from one index space to another. All later accesses to the CM data
		 * are assumed to be in terms of the translated indices.
		 *
		 * 'finalize' must be called prior to use. This slightly clumsy
		 * interface is there so that we can ensure that the mapper will have a
		 * complete list of valid neuron indices by the time of finalization,
		 * so that we can report invalid synapse terminals.
		 */
		ConnectivityMatrix(
				const network::Generator& net,
				const ConfigurationImpl& conf,
				const mapper_t&);

		/* Add synapse but use the provided source and target values rather
		 * than the ones provided in the underlying synapse. The caller can
		 * thus provide an appropriate mapping of either index. */
		void addSynapse(nidx_t source, nidx_t target, const Synapse&);

		const std::vector<synapse_id>& getSynapsesFrom(unsigned neuron);

		/*! \return all synapses for a given source and delay */
		const Row& getRow(nidx_t source, delay_t) const;

		/*! \copydoc nemo::Simulation::getTargets */
		const std::vector<unsigned>& getTargets(const std::vector<synapse_id>&);

		/*! \copydoc nemo::Simulation::getDelays */
		const std::vector<unsigned>& getDelays(const std::vector<synapse_id>&);

		/*! \copydoc nemo::Simulation::getWeights */
		const std::vector<float>& getWeights(const std::vector<synapse_id>&);

		/*! \copydoc nemo::Simulation::getPlastic */
		const std::vector<unsigned char>& getPlastic(const std::vector<synapse_id>&);

		void finalize(const mapper_t& mapper, bool verifySources);

		typedef OutgoingDelays::const_iterator delay_iterator;

		/*! \param source
		 * 		global neuron index of source neuron
		 *  \return
		 *  	iterator pointing to first delay for the \a neuron
		 */
		delay_iterator delay_begin(nidx_t source) const;

		/*! \param source
		 * 		global neuron index of source neuron
		 *  \return
		 *  	iterator pointing beyond the last delay for the \a neuron
		 */
		delay_iterator delay_end(nidx_t source) const;

		unsigned fractionalBits() const { return m_fractionalBits; }

		delay_t maxDelay() const { return m_maxDelay; }

		void accumulateStdp(const std::vector<uint64_t>& recentFiring);

		void applyStdp(float reward);

		/*! \return bit-mask indicating the delays at which the given neuron
		 * has *any* outgoing synapses. If the source neuron is invalid 0 is
		 * returned.
		 *
		 * Only call this after finalize has been called. */
		uint64_t delayBits(nidx_t l_source) const { return m_delays.delayBits(l_source); }

	private:

		const mapper_t& m_mapper;

		unsigned m_fractionalBits;

		/* During network construction we accumulate data in a map. This way we
		 * don't need to know the number of neurons or the number of delays in
		 * advance */
		typedef boost::tuple<nidx_t, delay_t> fidx_t;
		typedef std::vector<FAxonTerminal> row_t;
		std::map<fidx_t, row_t> m_acc;

		/* At run-time, however, we want a fast lookup of the rows. We
		 * therefore use a vector with linear addressing.  This is constructed
		 * in \a finalize which must be called prior to getRow being called */
		std::vector<Row> m_cm;
		void finalizeForward(const mapper_t&, bool verifySources);

		/* For the reverse matrix we don't need to group by delay */
		//! \todo move into std::vector when finalizing
		typedef std::vector<RSynapse> Incoming;
		std::map<nidx_t, Incoming> m_racc;
		boost::optional<StdpProcess> m_stdp;

		OutgoingDelaysAcc m_delaysAcc;
		OutgoingDelays m_delays;
		delay_t m_maxDelay;

		/*! \return linear index into CM, based on 2D index (neuron,delay) */
		size_t addressOf(nidx_t, delay_t) const;

		void verifySynapseTerminals(fidx_t idx,
				const row_t& row, const mapper_t&, bool verifySource) const;

		/*! \return address of the synapse weight in the forward matrix, given
		 * a synapse in the reverse matrix */
		fix_t* weight(const RSynapse&) const;

		/* Internal buffers for synapse queries */
		std::vector<synapse_id> m_queriedSynapseIds;
		std::vector<unsigned> m_queriedTargets;
		std::vector<unsigned> m_queriedDelays;
		std::vector<float> m_queriedWeights;
		std::vector<unsigned char> m_queriedPlastic;

		/*! \todo We could save both time and space here by doing the same as
		 * in the cuda backend, namely
		 *
		 * 1. making use of the writeOnlySynapses flag
		 * 2. make the aux_map use unordered_map
		 */

		/* Additional synapse data which is only needed for runtime queries.
		 * This is kept separate from m_cm so that we can make m_cm fast and
		 * compact. The query information is not crucial for performance.  */
		typedef std::vector<AxonTerminalAux> aux_row;
		typedef std::map<nidx_t, aux_row> aux_map;
		aux_map m_cmAux;

		/* Look up auxillary synapse data and report invalid lookups */
		const AxonTerminalAux& axonTerminalAux(nidx_t neuron, id32_t synapse) const;
		const AxonTerminalAux& axonTerminalAux(const synapse_id&) const;

		bool m_writeOnlySynapses;
};



inline
size_t
ConnectivityMatrix::addressOf(nidx_t source, delay_t delay) const
{
	return source * m_maxDelay + delay - 1;
}



/* The parts of the synapse data is only needed if querying synapses at
 * run-time. This data is stored separately */
struct AxonTerminalAux
{
	/* We need to store the synapse address /within/ a row. The row number
	 * itself can be computed on-the-fly based on the delay. */
	sidx_t idx;

	unsigned delay;
	bool plastic;

	AxonTerminalAux(sidx_t idx, unsigned delay, bool plastic) :
		idx(idx), delay(delay), plastic(plastic) { }

	AxonTerminalAux() :
		idx(~0), delay(~0), plastic(false) { }
};



} // end namespace nemo

#endif