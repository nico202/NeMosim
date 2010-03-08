#ifndef TARGET_PARTITIONS_CU
#define TARGET_PARTITIONS_CU

/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "outgoing.cu_h"

__constant__ size_t c_outgoingPitch; // word pitch



__host__
outgoing_t
make_outgoing(pidx_t partition, delay_t delay, uint warpOffset)
{
	//! \todo could share pointer packing with dispatchTable code
	assert(partition < MAX_PARTITION_COUNT);
	assert(delay < MAX_DELAY);

	uint targetData =
	       ((uint(partition) & MASK(PARTITION_BITS)) << (DELAY_BITS))
	     |  (uint(delay)     & MASK(DELAY_BITS));

	return make_uint2(targetData, (uint) warpOffset);
}




__host__
void
setOutgoingPitch(size_t targetPitch)
{
	CUDA_SAFE_CALL(cudaMemcpyToSymbol(c_outgoingPitch,
				&targetPitch, sizeof(size_t), 0, cudaMemcpyHostToDevice));
}



__host__ __device__
size_t
outgoingRow(pidx_t partition, nidx_t neuron, size_t pitch)
{
	//! \todo factor out addressing function and share with the 'counts' function
	return (partition * MAX_PARTITION_SIZE + neuron) * pitch;
}



__device__
uint
outgoingTargetPartition(outgoing_t out)
{
	return uint((out.x >> (DELAY_BITS)) & MASK(PARTITION_BITS));
}



__device__
uint
outgoingDelay(outgoing_t out)
{
	return uint(out.x & MASK(DELAY_BITS));
}



__device__ uint outgoingWarpOffset(outgoing_t out) { return out.y; }



__device__
outgoing_t
outgoing(uint presynaptic,
		uint jobIdx,
		outgoing_t* g_targets)
{
	size_t addr = outgoingRow(CURRENT_PARTITION, presynaptic, c_outgoingPitch);
	return g_targets[addr + jobIdx];
}



/*! \return
 *		the number of jobs for a particular firing neuron in the current
 *		partition */
__device__
uint
outgoingCount(uint presynaptic, uint* g_counts)
{
	return g_counts[CURRENT_PARTITION * MAX_PARTITION_SIZE + presynaptic];
}


#endif
