/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

//! \file nemo_c.cpp

/*! C API for libnemo
 *
 * This simply wrapes the API exposed in nemo::Simulation
 */

#include <cassert>

#include <nemo.h>
#include <nemo.hpp>
#include "exception.hpp"

/* We cannot propagate exceptions via the C API, so we catch all and convert to
 * error codes instead. Error descriptions are stored on a per-process basis. */

static std::string g_lastError;
static nemo_status_t g_lastCallStatus = NEMO_OK;


void
setResult(const char* msg, nemo_status_t status) {
	g_lastError = msg;
	g_lastCallStatus = status;
}




/* Call method on wrapped object, and /set/ status and error */
#define CALL(call) {                                                          \
        g_lastCallStatus = NEMO_OK;                                           \
        try {                                                                 \
            call;                                                             \
        } catch (nemo::exception& e) {                                        \
            setResult(e.what(), e.errorNumber());                             \
        } catch (std::exception& e) {                                         \
            setResult(e.what(), NEMO_UNKNOWN_ERROR);                          \
        } catch (...) {                                                       \
            setResult("unknown exception", NEMO_UNKNOWN_ERROR);               \
        }                                                                     \
    }

/* Call method on wrapper object, and return status and error */
#define CATCH_(T, ptr, call) {                                                \
        nemo::T* obj = static_cast<nemo::T*>(ptr);                            \
        CALL(obj->call)                                                       \
        return g_lastCallStatus;                                              \
	}

/* Call method on wrapper object, set output value, and return status and error */
#define CATCH(T, ptr, call, ret) {                                            \
        nemo::T* obj = static_cast<nemo::T*>(ptr);                            \
        CALL(ret = obj->call);                                                \
        return g_lastCallStatus;                                              \
	}

#define NOCATCH(T, ptr, call) static_cast<nemo::T*>(ptr)->call


const char*
nemo_version()
{
	return nemo::version();
}


nemo_status_t
nemo_cuda_device_count(unsigned* count)
{
	*count = 0;
	CALL(*count = nemo::cudaDeviceCount());
	return g_lastCallStatus;
}


nemo_status_t
nemo_cuda_device_description(unsigned device, const char** descr)
{
	CALL(*descr = nemo::cudaDeviceDescription(device));
	return g_lastCallStatus;
}


nemo_network_t
nemo_new_network()
{
	return static_cast<nemo_network_t>(new nemo::Network());
}


void
nemo_delete_network(nemo_network_t net)
{
	delete static_cast<nemo::Network*>(net);
}



nemo_configuration_t
nemo_new_configuration()
{
	try {
		return static_cast<nemo_configuration_t>(new nemo::Configuration());
	} catch(nemo::exception& e) {
		setResult(e.what(), e.errorNumber());
		return NULL;
	} catch(std::exception& e) {
		setResult(e.what(), NEMO_UNKNOWN_ERROR);
		return NULL;
	}
}



void
nemo_delete_configuration(nemo_configuration_t conf)
{
	delete static_cast<nemo::Configuration*>(conf);
}



nemo_simulation_t
nemo_new_simulation(nemo_network_t net_ptr, nemo_configuration_t conf_ptr)
{
	try {
		nemo::Network* net = static_cast<nemo::Network*>(net_ptr);
		nemo::Configuration* conf = static_cast<nemo::Configuration*>(conf_ptr);
		return static_cast<nemo_simulation_t>(nemo::simulation(*net, *conf));
	} catch(nemo::exception& e) {
		setResult(e.what(), e.errorNumber());
		return NULL;
	} catch(std::exception& e) {
		setResult(e.what(), NEMO_UNKNOWN_ERROR);
		return NULL;
	} catch(...) {
		setResult("Unknown error", NEMO_UNKNOWN_ERROR);
		return NULL;

	}
}



void
nemo_delete_simulation(nemo_simulation_t sim)
{
	delete static_cast<nemo::Simulation*>(sim);
}



nemo_status_t
nemo_add_neuron(nemo_network_t net,
		unsigned idx,
		float a, float b, float c, float d,
		float u, float v, float sigma)
{
	CATCH_(Network, net, addNeuron(idx, a, b, c, d, u, v, sigma));
}



nemo_status_t
nemo_add_synapse(nemo_network_t net,
		unsigned source,
		unsigned target,
		unsigned delay,
		float weight,
		unsigned char is_plastic)
{
	CATCH_(Network, net, addSynapse(source, target, delay, weight, is_plastic));
}



nemo_status_t
nemo_add_synapses(nemo_network_t net,
		unsigned sources[],
		unsigned targets[],
		unsigned delays[],
		float weights[],
		unsigned char is_plastic[],
		size_t length)
{
	CATCH_(Network, net, addSynapses(sources, targets, delays, weights, is_plastic, length));
}



nemo_status_t
nemo_neuron_count(nemo_network_t net, unsigned* ncount)
{
	CATCH(Network, net, neuronCount(), *ncount);
}



nemo_status_t
nemo_get_synapses(nemo_simulation_t ptr,
		unsigned source,
		unsigned* targets_[],
		unsigned* delays_[],
		float* weights_[],
		unsigned char* plastic_[],
		size_t* len)
{
	const std::vector<unsigned>* targets;
	const std::vector<unsigned>* delays;
	const std::vector<float>* weights;
	const std::vector<unsigned char>* plastic;
	nemo::Simulation* sim = static_cast<nemo::Simulation*>(ptr);
	CALL(sim->getSynapses(source, &targets, &delays, &weights, &plastic));
	if(NEMO_OK == g_lastCallStatus) {
		*targets_ = targets->empty() ? NULL : const_cast<unsigned*>(&(*targets)[0]);
		*delays_ = delays->empty() ? NULL : const_cast<unsigned*>(&(*delays)[0]);
		*weights_ = weights->empty() ? NULL : const_cast<float*>(&(*weights)[0]);
		*plastic_ = plastic->empty() ? NULL : const_cast<unsigned char*>(&(*plastic)[0]);
		*len = targets->size();
	}
	return g_lastCallStatus;
}




void
step(nemo::Simulation* sim, const std::vector<unsigned>& fstim, unsigned *fired[], size_t* fired_len)
{
	const std::vector<unsigned>& fired_ = sim->step(fstim);
	if(fired != NULL) {
		*fired = fired_.empty() ? NULL : const_cast<unsigned*>(&fired_[0]);
	}
	if(fired_len != NULL) {
		*fired_len = fired_.size();
	}
}



nemo_status_t
nemo_step(nemo_simulation_t sim_ptr,
		unsigned fstim[], size_t fstim_count,
		unsigned* fired[], size_t* fired_count)
{
	nemo::Simulation* sim = static_cast<nemo::Simulation*>(sim_ptr);
	CALL(step(sim, std::vector<unsigned>(fstim, fstim + fstim_count), fired, fired_count));
	return g_lastCallStatus;
}



nemo_status_t
nemo_apply_stdp(nemo_simulation_t sim, float reward)
{
	CATCH_(Simulation, sim, applyStdp(reward));
}



//-----------------------------------------------------------------------------
// Timing
//-----------------------------------------------------------------------------


nemo_status_t
nemo_log_stdout(nemo_configuration_t conf)
{
	CATCH_(Configuration, conf, enableLogging());
}



nemo_status_t
nemo_elapsed_wallclock(nemo_simulation_t sim, unsigned long* elapsed)
{
	CATCH(Simulation, sim, elapsedWallclock(), *elapsed);
}



nemo_status_t
nemo_elapsed_simulation(nemo_simulation_t sim, unsigned long* elapsed)
{
	CATCH(Simulation, sim, elapsedSimulation(), *elapsed);
}



nemo_status_t
nemo_reset_timer(nemo_simulation_t sim)
{
	CATCH_(Simulation, sim, resetTimer());
}



//-----------------------------------------------------------------------------
// CONFIGURATION
//-----------------------------------------------------------------------------


nemo_status_t
nemo_set_stdp_function(nemo_configuration_t conf,
		float* pre_fn, size_t pre_len,
		float* post_fn, size_t post_len,
		float w_min,
		float w_max)
{
	CATCH_(Configuration, conf, setStdpFunction(
				std::vector<float>(pre_fn, pre_fn+pre_len),
				std::vector<float>(post_fn, post_fn+post_len),
				w_min, w_max));
}


nemo_status_t
nemo_set_cpu_backend(nemo_configuration_t conf, int threadCount)
{
	CATCH_(Configuration, conf, setCpuBackend(threadCount));
}



nemo_status_t
nemo_cpu_thread_count(nemo_configuration_t conf, int* threadCount)
{
	CATCH(Configuration, conf, cpuThreadCount(), *threadCount);
}



nemo_status_t
nemo_set_cuda_backend(nemo_configuration_t conf, int dev)
{
	CATCH_(Configuration, conf, setCudaBackend(dev));
}



nemo_status_t
nemo_cuda_device(nemo_configuration_t conf, int* dev)
{
	CATCH(Configuration, conf, cudaDevice(), *dev);
}



nemo_status_t
nemo_backend(nemo_configuration_t conf, backend_t* backend)
{
	CATCH(Configuration, conf, backend(), *backend);
}



nemo_status_t
nemo_backend_description(nemo_configuration_t conf, const char** descr)
{
	CATCH(Configuration, conf, backendDescription().c_str(), *descr);
}


const char*
nemo_strerror()
{
	return g_lastError.c_str();
}
