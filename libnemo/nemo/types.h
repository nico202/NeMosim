#ifndef NEMO_TYPES_H
#define NEMO_TYPES_H

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

/*! Catch-all CUDA error */
#define NEMO_CUDA_ERROR 4

#define NEMO_API_UNSUPPORTED 5

#define NEMO_INVALID_INPUT 6
#define NEMO_BUFFER_OVERFLOW 7
#define NEMO_BUFFER_UNDERFLOW 8
#define NEMO_LOGIC_ERROR 9
#define NEMO_ALLOCATION_ERROR 10

#define NEMO_MPI_ERROR 11

/*! Dynamic library loading error */
#define NEMO_DL_ERROR 12

#define NEMO_UNKNOWN_ERROR 13


enum {
	NEMO_BACKEND_CUDA,
	NEMO_BACKEND_CPU
};

typedef unsigned backend_t;
typedef unsigned long long cycle_t;

#endif
