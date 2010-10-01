#define NETWORK_ADD_NEURON_DOC "\n\nadd a single neuron to network\n\nInputs:\nidx - Neuron index (0-based)\na - Time scale of the recovery variable\nb - Sensitivity to sub-threshold fluctuations in the membrane potential v\nc - After-spike value of the membrane potential v\nd - After-spike reset of the recovery variable u\nu - Initial value for the membrane recovery variable\nv - Initial value for the membrane potential\nsigma - Parameter for a random gaussian per-neuron process which generates random input current drawn from an N(0, sigma) distribution. If set to zero no random input current will be generated\n\nThe neuron uses the Izhikevich neuron model. See E. M. Izhikevich \"Simple model of spiking neurons\", IEEE Trans. Neural Networks, vol 14, pp 1569-1572, 2003 for a full description of the model and the parameters."
#define NETWORK_ADD_SYNAPSE_DOC "\n\nadd a single synapse to the network\n\nInputs:\nsource - Index of source neuron\ntarget - Index of target neuron\ndelay - Synapse conductance delay in milliseconds\nweight - Synapse weights\nplastic - Boolean specifying whether or not this synapse is plastic"
#define NETWORK_ADD_SYNAPSES_DOC "\n\nadd multiple synapses to the network\n\nInputs:\nsources - Source neuron indices\ntargets - Vector of target indices\ndelays - Vector of delays (in milliseconds)\nweights - Vector of weights\nplastic - Vector of booleans specifying whether each neuron is plastic\n\nThe input vectors should all have the same length"
#define NETWORK_NEURON_COUNT_DOC "\n\n"
#define CONFIGURATION_SET_CPU_BACKEND_DOC "\n\nspecify that the CPU backend should be used\n\nInputs:\ntcount - number of threads\n\nSpecify that the CPU backend should be used and optionally specify the number of threads to use. If the default thread count of -1 is used, the backend will choose a sensible value based on the available hardware concurrency."
#define CONFIGURATION_SET_CUDA_BACKEND_DOC "\n\nspecify that the CUDA backend should be used\n\nInputs:\ndeviceNumber\n\nSpecify that the CUDA backend should be used and optionally specify a desired device. If the (default) device value of -1 is used the backend will choose the best available device.   If the cuda backend (and the chosen device) cannot be used for  whatever reason, an exception is raised.   The device numbering is the numbering used internally by nemo (see  cudaDeviceCount and cudaDeviceDescription). This device  numbering may differ from the one provided by the CUDA driver  directly, since nemo ignores any devices it cannot use. "
#define CONFIGURATION_SET_STDP_FUNCTION_DOC "\n\nenable STDP and set the global STDP function\n\nInputs:\nprefire - STDP function values for spikes arrival times before the postsynaptic firing, starting closest to the postsynaptic firing\npostfire - STDP function values for spikes arrival times after the postsynaptic firing, starting closest to the postsynaptic firing\nminWeight - Lowest (negative) weight beyond which inhibitory synapses are not potentiated\nmaxWeight - Highest (positive) weight beyond which excitatory synapses are not potentiated\n\nThe STDP function is specified by providing the values sampled at integer cycles within the STDP window."
#define CONFIGURATION_BACKEND_DESCRIPTION_DOC "\n\nDescription of the currently selected simulation backend\n\nThe backend can be changed using setCudaBackend or setCpuBackend"
#define SIMULATION_STEP_DOC "\n\nrun simulation for a single cycle (1ms)\n\nInputs:\nfstim - An optional list of neurons, which will be forced to fire this cycle"
#define SIMULATION_APPLY_STDP_DOC "\n\nupdate synapse weights using the accumulated STDP statistics\n\nInputs:\nreward - Multiplier for the accumulated weight change"
#define SIMULATION_GET_TARGETS_DOC "\n\nreturn the targets for the specified synapses\n\nInputs:\nsynapses - synapse ids (as returned by addSynapse)"
#define SIMULATION_GET_DELAYS_DOC "\n\nreturn the conductance delays for the specified synapses\n\nInputs:\nsynapses - synapse ids (as returned by addSynapse)"
#define SIMULATION_GET_WEIGHTS_DOC "\n\nreturn the weights for the specified synapses\n\nInputs:\nsynapses - synapse ids (as returned by addSynapse)"
#define SIMULATION_GET_PLASTIC_DOC "\n\nreturn the boolean plasticity status for the specified synapses\n\nInputs:\nsynapses - synapse ids (as returned by addSynapse)"
#define SIMULATION_ELAPSED_WALLCLOCK_DOC "\n\n"
#define SIMULATION_ELAPSED_SIMULATION_DOC "\n\n"
#define SIMULATION_RESET_TIMER_DOC "\n\nreset both wall-clock and simulation timer"
