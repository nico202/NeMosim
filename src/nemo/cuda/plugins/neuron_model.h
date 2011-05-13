#ifndef NEMO_CUDA_PLUGIN_NEURON_MODEL_HPP
#define NEMO_CUDA_PLUGIN_NEURON_MODEL_HPP

/* Common API for CUDA neuron model plugins */

#include <cuda_runtime.h>
#include <nemo/internal_types.h>
#include <nemo/cuda/types.h>
#include <nemo/cuda/parameters.cu_h>
#include <nemo/cuda/rng.cu_h>
#include <nemo/cuda/rcm.cu_h>

#ifdef __cplusplus
extern "C" {
#endif

typedef cudaError_t cuda_update_neurons_t(
		cudaStream_t stream,
		unsigned cycle,
		unsigned partitionCount,
		unsigned* d_partitionSize,
		param_t* d_globalParameters,
		float* df_neuronParameters,
		float* df_neuronState,
		nrng_t d_nrng,
		uint32_t* d_valid,
		uint32_t* d_fstim,
		fix_t* d_istim,
		fix_t* d_current,
		uint32_t* d_fout,
		unsigned* d_nFired,
		nidx_dt* d_fired,
		rcm_dt* d_rng);

#ifdef __cplusplus
}
#endif

#endif