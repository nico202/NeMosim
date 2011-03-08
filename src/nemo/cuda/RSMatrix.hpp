#ifndef RS_MATRIX_HPP
#define RS_MATRIX_HPP

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

//! \file RSMatrix.hpp

#include <stddef.h>
#include <vector>
#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>

#include "kernel.cu_h"
#include "types.h"
#include "Mapper.hpp"

namespace nemo {
	namespace cuda {

/*! \brief Sparse synapse matrix in reverse format for a single partition
 *
 * Synapses in this matrix are stored on a per-target basis.
 *
 * The reverse matrix has two planes: one for reverse addressing and one for
 * accumulating STDP statistics (LTP and LTD).
 *
 * \see SMatrix
 *
 * \author Andreas Fidjeland
 */

class RSMatrix
{
	public:

		RSMatrix(size_t partitionSize);

		void addSynapse(
				const DeviceIdx& source,
				unsigned targetNeuron,
				unsigned delay,
				uint32_t forwardAddress);

		void moveToDevice();

		void clearStdpAccumulator();

		/*! \return bytes allocated on the device */
		size_t d_allocated() const { return mb_allocated; }

		/*! \return word pitch, i.e. max number of synapses per neuron */
		size_t pitch() const { return mw_pitch; }

		/*! \return device address of reverse address matrix */
		uint32_t* d_address() const;

		/*! \return device address of STDP accumulator matrix */
		weight_dt* d_stdp() const;

		uint32_t* d_faddress() const;

	private:

		boost::shared_ptr<uint32_t> md_data;

		typedef std::vector< std::vector<uint32_t> > host_plane;

		/* Source neuron information */
		host_plane mh_source;

		/* The full address (in the FCM) to this particular synapse */
		host_plane mh_sourceAddress;

		size_t m_partitionSize;

		size_t mw_pitch;

		/* Number of bytes of allocated device memory */
		size_t mb_allocated;

		/* Indices of the two planes of the matrix */
		enum {
			RCM_ADDRESS = 0, // source information
			RCM_STDP,
			RCM_FADDRESS,    // actual word address of synapse
			RCM_SUBMATRICES
		};

		/*! \return size (in words) of a single plane of the matrix */
		size_t planeSize() const;

		bool onDevice() const;

		size_t maxSynapsesPerNeuron() const;

		boost::shared_ptr<uint32_t>& allocateDeviceMemory();

		/* Move /one/ plane to device */
		void moveToDevice(host_plane& h_mem, size_t plane, uint32_t dflt, uint32_t* d_mem);
};

	} // end namespace cuda
} // end namespace nemo

#endif