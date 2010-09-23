#ifndef FIRING_OUTPUT_HPP
#define FIRING_OUTPUT_HPP

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <boost/shared_ptr.hpp>

#include <nemo/internal_types.h>
#include <nemo/FiringBuffer.hpp>
#include "Mapper.hpp"

//! \file FiringBuffer.hpp

/*! \brief Data and functions for reading firing data from device to host
 *
 * \author Andreas Fidjeland
 */

namespace nemo {
	namespace cuda {

class FiringBuffer {

	public:

		/*! Set up data on both host and device for probing firing */
		FiringBuffer(const Mapper& mapper);

		/*! Read firing data from device to host buffer. This should be called
		 * every simulation cycle */
		void sync();

		/*! Return oldest buffered cycle's worth of firing */
		FiredList readFiring();

		/*! \return device pointer to the firing buffer */
		uint32_t* d_buffer() const { return md_buffer.get(); }

		/*! \return bytes of allocated device memory */
		size_t d_allocated() const { return md_allocated; }

		size_t wordPitch() const { return m_pitch; }

	private:

		/* Dense firing buffers on device and host */
		boost::shared_ptr<uint32_t> md_buffer;
		boost::shared_ptr<uint32_t> mh_buffer; // pinned, same size as device buffer
		size_t m_pitch; // in words

		size_t md_allocated;

		void populateSparse(const uint32_t* hostBuffer);

		Mapper m_mapper;

		nemo::FiringBuffer m_outputBuffer;
};

	} // end namespace cuda
} // end namespace nemo

#endif
