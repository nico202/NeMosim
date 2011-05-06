#ifndef CONNECTIVITY_MATRIX_CU
#define CONNECTIVITY_MATRIX_CU

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nemo/util.h>

#include "kernel.cu_h"
#include "connectivityMatrix.cu_h"

#define NEURON_MASK MASK(NEURON_BITS)
#define PARTITION_MASK MASK(PARTITION_BITS)
#define DELAY_MASK MASK(DELAY_BITS)

#define PARTITION_SHIFT NEURON_BITS

__host__
synapse_t
f_nullSynapse()
{
	return 0;
}


__host__ __device__
unsigned
targetNeuron(unsigned synapse)
{
#ifdef __DEVICE_EMULATION__
    return synapse & NEURON_MASK;
#else
	return synapse;
#endif
}


#endif
