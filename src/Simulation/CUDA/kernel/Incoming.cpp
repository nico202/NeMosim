#include "Incoming.hpp"

#include <stdexcept>
#include <sstream>

#include "util.h"
#include "kernel.cu_h"


void
Incoming::allocate(size_t partitionCount, size_t maxIncomingWarps)
{
	// allocate space for the incoming count
	uint* d_count;
	size_t len = ALIGN(partitionCount * MAX_DELAY, 32) * sizeof(uint);
	CUDA_SAFE_CALL(cudaMalloc((void**)&d_count, len));
	CUDA_SAFE_CALL(cudaMemset(d_count, 0, len));
	m_count = boost::shared_ptr<uint>(d_count, cudaFree);

	/* The queue has one entry for incoming spikes for each partition */
	assert(partitionCount < MAX_PARTITION_COUNT);
	size_t height = partitionCount * MAX_DELAY;

	/* Each buffer entry (for a particular source partition) is of a fixed size
	 * to simplify the rotating buffer code. This is very conservative. In fact
	 * the buffer is large enough that every neuron can fire every cycle */
	/*! \todo relax this constraint. We'll end up using a very large amount of
	 * space when using a large number of partitions */
	size_t width = maxIncomingWarps * sizeof(incoming_t);

	incoming_t* d_buffer;
	size_t bpitch;

	cudaError err = cudaMallocPitch((void**)&d_buffer, &bpitch, width, height);
	if(cudaSuccess != err) {
		std::ostringstream msg;
		msg << "Failed to allocate " << width * height
			<< " bytes for incoming spike queue\n"
			<< cudaGetErrorString(err);
		throw std::runtime_error(msg.str());
	}

	m_buffer = boost::shared_ptr<incoming_t>(d_buffer);

	/* We don't need to clear the queue. It will generally be full of garbage
	 * anyway. The queue heads must be used to determine what's valid data */

	size_t wpitch = bpitch / sizeof(incoming_t);
	setIncomingPitch(wpitch);
}
