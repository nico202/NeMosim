#ifndef NEMO_EXCEPTION_HPP
#define NEMO_EXCEPTION_HPP

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdexcept>
#include <string>

#include "nemo_error.h"

namespace nemo {

/* Minor extension of std::exception which adds return codes (for use in the C
 * API). The error codes are listed in nemo_error.h. */
//! \todo should probably inherit virtually here
class exception : public std::runtime_error
{
	public :

		explicit exception(int errno, const std::string& msg) : 
			std::runtime_error(msg),
			m_errno(errno) {}

		~exception() throw () {}

		int errno() const { return m_errno; }

	private :

		int m_errno;
};


} // end namespace nemo

#endif
