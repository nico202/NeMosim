#ifndef KERNEL_H
#define KERNEL_H

#include "kernel.cu_h"


#include <cuda_runtime.h>
#include <stdint.h>
#include <stdbool.h>


//-----------------------------------------------------------------------------
// DEBUGGING
//-----------------------------------------------------------------------------

typedef int status_t;

#define KERNEL_OK 0
#define KERNEL_INVOCATION_ERROR 1
#define KERNEL_ASSERTION_FAILURE 2


//-----------------------------------------------------------------------------
// KERNEL CONFIGURATION
//-----------------------------------------------------------------------------


/*! Set per-partition configuration parameter specifying the number of neurons
 * in that partition. */
void configurePartitionSize(size_t clusters, const unsigned int* maxIdx);


/*! \return the maximum partition size, for the given configuration */
unsigned int maxPartitionSize(int useSTDP);


//-----------------------------------------------------------------------------
// RUNTIME DATA
//-----------------------------------------------------------------------------


typedef struct RuntimeData* RTDATA;


/*!
 * \param maxReadPeriod
 * 		maximum period (in cycles) between reads to the device firing buffer
 * \param maxL1Delay
 * 		maximum synaptic delay (in cycles) for any L1 synapse
 */
RTDATA
allocRuntimeData(
		size_t partitionCount,
		size_t maxPartitionSize,
		unsigned int maxDelay,
		size_t maxL0SynapsesPerDelay,
		size_t maxL0RevSynapsesPerNeuron,
		size_t maxL1SynapsesPerDelay,
		size_t maxL1RevSynapsesPerNeuron,
		//! \todo determine the entry size inside allocator
		size_t l1SQEntrySize,
		unsigned int maxReadPeriod);

void freeRuntimeData(RTDATA);


//-----------------------------------------------------------------------------
// LOADING NEURON DATA
//-----------------------------------------------------------------------------

void 
loadParam(RTDATA rt,
		size_t paramIdx,
		size_t partitionIdx,
		size_t partitionSize,
		float* arr);


//! \todo could merge this with loadParam
void
loadThalamicInputSigma(RTDATA rt,
		size_t partitionIdx,
		size_t partitionSize,
		float* arr);



//-----------------------------------------------------------------------------
// LOADING SYNAPSE DATA
//-----------------------------------------------------------------------------


#define CM_L0 0
#define CM_L1 1
#define CM_COUNT 2

/*! Copy connectivity data for a specific delay for a single presynaptic neuron
 * */
void
setCMDRow(RTDATA rtdata,
		size_t cmIdx,
		unsigned int sourceCluster,
		unsigned int sourceNeuron,
		unsigned int delay,
		float* h_weights,
		unsigned int* h_targetPartition,
		unsigned int* h_targetNeuron,
		size_t length);


/*! Read connectivity matrix back from device. */
void
getCM(RTDATA rtdata,
		size_t cmIdx,
		int** targetPartitions,
		int** targetNeurons,
		float** weights,
		size_t* pitch);


//-----------------------------------------------------------------------------
// FIRING PROBE
//-----------------------------------------------------------------------------


/*!
 * \return number of fired neurons since last read 
 *
 * The three arrays together form a vector of 3-tuples with specifying cycle,
 * partition index, and neuron index for all the fired neurons. 
 */
size_t
readFiring(RTDATA rtdata,
		unsigned int** cycles,
		unsigned int** partitionIdx,
		unsigned int** neuronIdx);


/* Step can be asynchronous. sync forces completion of all steps */
void syncSimulation(RTDATA rtdata);


//-----------------------------------------------------------------------------
// TIMING
//-----------------------------------------------------------------------------

void printCycleCounters(RTDATA rtdata);


/*! \return number of milliseconds elapsed between beginning of first kernel
 * invocation and the end of the last */
long int elapsedMs(RTDATA rtdata);


void
resetTimer(RTDATA rtdata);


//-----------------------------------------------------------------------------
// STDP
//-----------------------------------------------------------------------------


/*! Enable spike-timing dependent plasticity in the simulation.
 *
 * \param preFireWindow
 * 		Length, in cycles, of the part of the STDP window that precedes the
 * 		postsynaptic firing.
 * \param postFireWindow
 * 		Length, in cycles, of the part of the STDP window that comes after the
 * 		postsynaptic firing.
 * \param potentiationMask
 * 		Bit mask indicating what cycles during the STDP for which potentiation
 * 		takes place. Bit 0 is the end of the STDP window.
 * \param depressionMask
 * 		Bit mask indicating what cycles during the STDP for which depression
 * 		takes place. Bit 0 is the end of the STDP window.
 * \param stdpFn
 * 		STDP function sampled at integer cycle intervals. Length should be
 * 		preFireWindow + postFireWindow. The first entry corresponds to the
 * 		beginning of the STDP window.
 * \param maxWeight
 * 		Weight beyond which excitatory synapses are not allowed to move
 */
void
enableStdp(RTDATA,
		unsigned int preFireWindow,
		unsigned int postFireWindow,
		uint64_t potentiationBits,
		uint64_t depressionBits,
		float* stdpFn,
		float maxWeight);


//-----------------------------------------------------------------------------
// SIMULATION STEPPING
//-----------------------------------------------------------------------------

status_t
step(RTDATA rtdata,
		unsigned short cycle,
		int substeps,               // number of substeps per normal 1ms step
		// External firing (sparse)
		size_t extFiringCount,
		const int* extFiringCIdx,   // cluster indices
		const int* extFiringNIdx);  // neuron indices


void applyStdp(RTDATA rtdata, float stdpReward);


/* Force all allocated memory onto the device. Calling this is not required
 * during normal operation, as step invokes it on first call, but can be used
 * for testing */
void
copyToDevice(RTDATA);


/* Return the number of bytes allocated on the device so far */
size_t
allocatedDeviceMemory(RTDATA);


//-----------------------------------------------------------------------------
// DEVICE PROPERTIES
//-----------------------------------------------------------------------------

//! \return number of cuda-enabled devices of compute capability 1.0 or greater
int deviceCount(void);

//! \return pointer to data structure containing device properties
struct cudaDeviceProp* deviceProperties(int device);

// ... in bytes
size_t totalGlobalMem(struct cudaDeviceProp* prop);

// ... in bytes
size_t sharedMemPerBlock(struct cudaDeviceProp* prop);

int regsPerBlock(struct cudaDeviceProp* prop);

//! \return maximum pitch allowed by the memory copy functions
size_t memPitch(struct cudaDeviceProp* prop);

int maxThreadsPerBlock(struct cudaDeviceProp* prop);

//! \return total amount of constant memory available on the device (in bytes)
size_t totalConstMem(struct cudaDeviceProp* prop);

//! \return major revision of device's compute capability
//int major(struct cudaDeviceProp* prop);

//! \return minor revision of device's compute capability
//int minor(struct cudaDeviceProp* prop);

//! \return clock rate in kilohertz
int clockRate(struct cudaDeviceProp* prop);

#endif //CU_IZHIKEVICH_H
