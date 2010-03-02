#ifndef INCOMING_HPP
#define INCOMING_HPP

#include <boost/shared_ptr.hpp>

#include "incoming.cu_h"

namespace nemo {

class Incoming
{
	public :

		Incoming();

		/*! Allocate space on device to hold the per neuron/delay incoming
		 * spike groups
		 *
		 * \param maxIncomingWarps
		 * 		Maximum number of incoming warps (regardless of delay) for any
		 * 		partition,
		 * \param sizeMultiplier
		 * 		To be completely safe against buffer overflow, base incoming
		 * 		buffer sizing on the assumption that all neurons may fire
		 * 		continously for some time. This is unlikely to happen in
		 * 		practice, however, so we can relax this. The size multiplier
		 * 		specifies how large the buffer should be wrt the most
		 * 		conservative case.
		 */
		void allocate(size_t partitionCount,
				size_t maxIncomingWarps,
				double sizeMultiplier = 1.0);

		incoming_t* buffer() const { return m_buffer.get(); }

		uint* heads() const { return m_count.get(); }

		/*! \return bytes of allocated memory */
		size_t allocated() const { return m_allocated; }

	private :

		/* On the device there a buffer for incoming spike group for each
		 * (target) partition */
		boost::shared_ptr<incoming_t> m_buffer;

		/* At run-time, we keep track of how many incoming spike groups are
		 * queued for each target partition */
		boost::shared_ptr<uint> m_count;

		size_t m_allocated;
};

} // end namespace nemo

#endif