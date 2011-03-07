#ifndef NEMO_CUDA_MAPPER_HPP
#define NEMO_CUDA_MAPPER_HPP

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <set>
#include <nemo/Mapper.hpp>

#include "types.h"

namespace nemo {

	namespace network {
		class Generator;
	}

	namespace cuda {

/*! Neuron indices as used on CUDA devices
 *
 * The network is split into partitions when moved onto the device. Neurons on
 * the device are thus addressed using a two-level address. */
struct DeviceIdx
{
	public:

		pidx_t partition;
		nidx_t neuron;

		DeviceIdx(pidx_t p, nidx_t n) : partition(p), neuron(n) {}
};


inline
bool
operator<(const DeviceIdx& lhs, const DeviceIdx& rhs)
{
	return lhs.partition < rhs.partition ||
		(lhs.partition == rhs.partition && lhs.neuron < rhs.neuron);
}



/*! Maps between local and global indices.
 *
 * The local indices can be either 2D (partition/neuron) or 1D with
 * straightforward mappings between them.
 */
class Mapper : public nemo::Mapper<nidx_t, nidx_t> {

	public :

		Mapper(const nemo::network::Generator& net, unsigned partitionSize);

		~Mapper() {}

		/*! Convert from device index (2D) to local 1D index */
		nidx_t localIdx(const DeviceIdx& d) const;

		/*! \copydoc nemo::Mapper::localIdx */
		nidx_t localIdx(const nidx_t& global) const {
			return localIdx(deviceIdx(global));
		}

		/*! \copydoc nemo::Mapper::globalIdx */
		nidx_t globalIdx(const nidx_t& local) const {
			return m_offset + local;
		}

		nidx_t globalIdx(const DeviceIdx& d) const {
			return m_offset + localIdx(d);
		}

		nidx_t globalIdx(pidx_t p, nidx_t n) const;

		/*! \copydoc nemo::Mapper::addGlobal */
		nidx_t addGlobal(const nidx_t& global) {
			return localIdx(addIdx(global));
		}

		/*! Add a neuron to the set of existing neurons and return the device
		 * index. */
		DeviceIdx addIdx(nidx_t global);

		/*! \return device index corresponding to the given global neuron index
		 *
		 * The global neuron index refers to a neuron which may or may not exist.
		 *
		 * \see existingDeviceIdx for a safer function.
		 */
		DeviceIdx deviceIdx(nidx_t global) const;

		/*! \return
		 * 		device index corresponding to the given global index of an
		 * 		existing neuron
		 *
		 * Throw if the neuron does not exist
		 *
		 * \see deviceIdx
		 */
		DeviceIdx existingDeviceIdx(nidx_t global) const;

		unsigned partitionSize() const { return m_partitionSize; }

		unsigned partitionCount() const { return m_partitionCount; }

		unsigned maxLocalIdx() const { return partitionSize() * partitionCount() - 1; }

		/*! \return minimum global indexed supported by this mapper */
		unsigned minHandledGlobalIdx() const { return m_offset; }

		/*! \return maximum global indexed supported by this mapper */
		unsigned maxHandledGlobalIdx() const;

		/*! \copydoc nemo::Mapper::existingGlobal */
		bool existingGlobal(const nidx_t& global) const {
			return m_existingGlobal.count(global) == 1;
		}

		/*! \copydoc nemo::Mapper::existingLocal */
		bool existingLocal(const nidx_t& local) const {
			return existingGlobal(globalIdx(local));
		}

		/*! \copydoc nemo::Mapper::neuronsInValidRange */
		unsigned neuronsInValidRange() const {
			return partitionCount() * partitionSize();
		}

	private :

		unsigned m_partitionSize;

		unsigned m_partitionCount;

		unsigned m_offset;

		std::set<nidx_t> m_existingGlobal;
};

	} // end namespace cuda
} // end namespace nemo

#endif
