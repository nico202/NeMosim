#ifndef NEMO_ERROR_H
#define NEMO_ERROR_H

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

/*! The call resulted in no errors */
#define NEMO_OK 0

/*! The CUDA driver reported an error */
#define NEMO_CUDA_INVOCATION_ERROR 1

/*! An assertion failed on the CUDA backend. Note that these assertions are not
 * enabled by default. Build library with -DDEVICE_ASSERTIONS to enable these */
#define NEMO_CUDA_ASSERTION_FAILURE 2

/*! A memory allocation failed on the CUDA device. */
#define NEMO_CUDA_MEMORY_ERROR 3

#define NEMO_API_UNSUPPORTED 4

#define NEMO_INVALID_INPUT 5

#define NEMO_UNKNOWN_ERROR 6



#endif
