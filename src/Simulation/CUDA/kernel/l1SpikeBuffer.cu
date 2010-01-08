#ifndef L1_SPIKE_BUFFER_CU
#define L1_SPIKE_BUFFER_CU

#include "kernel.cu_h"
#include "l1SpikeBuffer.cu_h"


__constant__ size_t c_l1BufferPitch; // word pitch


__host__
void
setBufferPitch(size_t pitch)
{
	CUDA_SAFE_CALL(cudaMemcpyToSymbol(c_l1BufferPitch,
				&pitch, sizeof(size_t), 0, cudaMemcpyHostToDevice));
}



/*! \return the buffer number to use for the given delay, given current cycle */
__device__
uint
l1BufferIndex(uint cycle, uint delay1)
{
	return (cycle + delay1) % MAX_DELAY;
}



/* Return offset into full buffer data structure to beginning of buffer for a
 * particular targetPartition and a particular delay. */
__device__
uint
l1BufferStart(uint targetPartition, uint cycle, uint delay1)
{
	return (targetPartition * MAX_DELAY + l1BufferIndex(cycle, delay1)) * c_l1BufferPitch;
}



/*! \return incoming spike group from a particular source */
__device__
l1spike_t
spikeBatch(uint sourcePartition, uint sourceNeuron, uint delay)
{
	ASSERT(sourcePartition < (1<<8));
	ASSERT(sourceNeuron < (1<<16));
	ASSERT(delay < (1<<8));
	return make_uchar4(
			(uchar) sourcePartition,
			(uchar) sourceNeuron >> 8,   // MSB
			(uchar) sourceNeuron & 0xff, // LSB
			(uchar) delay);
}


#endif
