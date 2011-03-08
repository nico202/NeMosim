#include <sstream>
#include <stdexcept>

#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include <nemo.hpp>

#include "docstrings.h" // auto-generated

using namespace boost::python;


/* The simulation is only created via a factory and only accessed throught the
 * returned pointer */
boost::shared_ptr<nemo::Simulation>
makeSimulation(const nemo::Network& net, const nemo::Configuration& conf)
{
	return boost::shared_ptr<nemo::Simulation>(simulation(net, conf));
}



template<typename T>
std::string
std_vector_str(std::vector<T>& self)
{
	std::stringstream out;

	if(self.size() > 0) {
		out << "[";
		if(self.size() > 1) {
			std::copy(self.begin(), self.end() - 1, std::ostream_iterator<T>(out, ", "));
		}
		out << self.back() << "]";
	}

	return out.str();
}



/* We use uchar to stand in for booleans */
std::string
std_bool_vector_str(std::vector<unsigned char>& self)
{
	std::stringstream out;

	if(self.size() > 0) {
		out << "[";
		for(std::vector<unsigned char>::const_iterator i = self.begin(); i != self.end() - 1; ++i) {
			out << (*i ? "True" : "False") << ", ";

		}
		out << (self.back() ? "True" : "False") << ", ";
	}
	return out.str();
}



/* Converter from python pair to std::pair */
template<typename T1, typename T2>
struct from_py_list_of_pairs
{
	typedef std::pair<T1, T2> pair_t;
	typedef std::vector<pair_t> vector_t;

	from_py_list_of_pairs() {
		converter::registry::push_back(
			&convertible,
			&construct,
			boost::python::type_id<vector_t>()
		);
	}

	static void* convertible(PyObject* obj_ptr) {
		if (!PyList_Check(obj_ptr)) {
			return 0;
		}
		/* It's possible for the list to contain data of different type. In
         * that case, fall over later, during the actual conversion. */
		return obj_ptr;
	}

	/* Convert obj_ptr into a std::vector */
	static void construct(
			PyObject* list,
			boost::python::converter::rvalue_from_python_stage1_data* data)
	{
		/* grab pointer to memory into which to construct vector */
		typedef converter::rvalue_from_python_storage<vector_t> storage_t;
		void* storage = reinterpret_cast<storage_t*>(data)->storage.bytes;

		/* in-place construct vector */
		Py_ssize_t len = PyList_Size(list);
		// vector_t* instance = new (storage) vector_t(len, 0);
		vector_t* instance = new (storage) vector_t(len);

		/* stash the memory chunk pointer for later use by boost.python */
		data->convertible = storage;

		/* populate the vector */
		vector_t& vec = *instance;
		for(unsigned i=0, i_end=len; i != i_end; ++i) {
			PyObject* pair = PyList_GetItem(list, i);
			PyObject* first = PyTuple_GetItem(pair, 0);
			PyObject* second = PyTuple_GetItem(pair, 1);
			vec[i] = std::make_pair<T1, T2>(extract<T1>(first), extract<T2>(second));
		}
	}
};


/* Python list to std::vector convertor */
template<typename T>
struct from_py_list
{
	typedef std::vector<T> vector_t;

	from_py_list() {
		converter::registry::push_back(
			&convertible,
			&construct,
			boost::python::type_id<vector_t>()
		);
	}

	static void* convertible(PyObject* obj_ptr) {
		if (!PyList_Check(obj_ptr)) {
			return 0;
		}
		/* It's possible for the list to contain data of different type. In
         * that case, fall over later, during the actual conversion. */
		return obj_ptr;
	}

	/* Convert obj_ptr into a std::vector */
	static void construct(
			PyObject* list,
			boost::python::converter::rvalue_from_python_stage1_data* data)
	{
		/* grab pointer to memory into which to construct vector */
		typedef converter::rvalue_from_python_storage<vector_t> storage_t;
		void* storage = reinterpret_cast<storage_t*>(data)->storage.bytes;

		/* in-place construct vector */
		Py_ssize_t len = PyList_Size(list);
		vector_t* instance = new (storage) vector_t(len, 0);

		/* stash the memory chunk pointer for later use by boost.python */
		data->convertible = storage;

		/* populate the vector */
		vector_t& vec = *instance;
		for(unsigned i=0, i_end=len; i != i_end; ++i) {
			vec[i] = extract<T>(PyList_GetItem(list, i));
		}
	}
};



/*!
 * Determine if input is scalar or vector. If it is a vector, verify that the
 * vector length is the same as other vectors (whose length is already set in
 * \a vectorLength.
 *
 * \param obj either a scalar or a vector
 * \param vectorLength length of any other vectors in the same parameter list,
 * 		or '0' if there are no others.
 * \return true if the object is vector (Python list), false if it's a scalar
 */
inline
bool
checkInputVector(PyObject* obj, unsigned &vectorLength)
{
	unsigned length = PyList_Check(obj) ? PyList_Size(obj) : 0;
	if(length > 0) {
		if(vectorLength > 0 && length != vectorLength) {
			throw std::invalid_argument("input vectors of different length");
		}
		vectorLength = length;
	}
	return length > 0;
}



/*! Add one or more synapses
 *
 * \return synapse id
 *
 * The arguments (other than net) may be either scalar or vector. All vectors
 * must be of the same length. If any of the inputs are vectors, the scalar
 * arguments are replicated for each synapse.
 */
PyObject*
add_synapse(nemo::Network& net, PyObject* sources, PyObject* targets,
		PyObject* delays, PyObject* weights, PyObject* plastics)
{
	unsigned len = 0;

	bool vectorSources = checkInputVector(sources, len);
	bool vectorTargets = checkInputVector(targets, len);
	bool vectorDelays = checkInputVector(delays, len);
	bool vectorWeights = checkInputVector(weights, len);
	bool vectorPlastics = checkInputVector(plastics, len);

	to_python_value<synapse_id&> get_id;

	if(len == 0) {
		/* All inputs are scalars */
		return get_id(net.addSynapse(
					extract<unsigned>(sources),
					extract<unsigned>(targets),
					extract<unsigned>(delays),
					extract<float>(weights),
					extract<unsigned char>(plastics))
				);
	} else {
		/* At least some inputs are vectors, so we need to return a list */
		PyObject* list = PyList_New(len);
		for(unsigned i=0; i != len; ++i) {
			unsigned source = extract<unsigned>(vectorSources ? PyList_GetItem(sources, i) : sources);
			unsigned target = extract<unsigned>(vectorTargets ? PyList_GetItem(targets, i) : targets);
			unsigned delay = extract<unsigned>(vectorDelays ? PyList_GetItem(delays, i) : delays);
			float weight = extract<float>(vectorWeights ? PyList_GetItem(weights, i) : weights);
			unsigned char plastic = extract<unsigned char>(vectorPlastics ? PyList_GetItem(plastics, i) : plastics);
			PyList_SetItem(list, i, get_id(net.addSynapse(source, target, delay, weight, plastic)));
		}
		return list;
	}
}



unsigned
set_neuron_x_length(PyObject* a, PyObject* b)
{
	unsigned len = 0;

	bool vectorA = checkInputVector(a, len);
	bool vectorB = checkInputVector(b, len);

	if(vectorA != vectorB) {
		throw std::invalid_argument("first and third argument must either both be scalar or lists of same length");
	}
	return len;
}



/*! Set neuron parameters for one or more neurons
 *
 * On the Python side the syntax is net.set_neuron_parameter(neurons, param,
 * values). Either these are all scalar, or neurons and values are both lists
 * of the same length.
 */
template<class T>
void
set_neuron_parameter(T& obj, PyObject* neurons, unsigned param, PyObject* values)
{
	const unsigned len = set_neuron_x_length(neurons, values);
	if(len == 0) {
		obj.setNeuronParameter(extract<unsigned>(neurons), param, extract<float>(values));
	} else {
		for(unsigned i=0; i < len; ++i) {
			unsigned neuron = extract<unsigned>(PyList_GetItem(neurons, i));
			float value = extract<float>(PyList_GetItem(values, i));
			obj.setNeuronParameter(neuron, param, value);
		}
	}
}



/*! Set neuron state for one or more neurons
 *
 * On the Python side the syntax is net.set_neuron_state(neurons, param,
 * values). Either these are all scalar, or neurons and values are both lists
 * of the same length.
 */
template<class T>
void
set_neuron_state(T& obj, PyObject* neurons, unsigned param, PyObject* values)
{
	const unsigned len = set_neuron_x_length(neurons, values);
	if(len == 0) {
		obj.setNeuronState(extract<unsigned>(neurons), param, extract<float>(values));
	} else {
		for(unsigned i=0; i < len; ++i) {
			unsigned neuron = extract<unsigned>(PyList_GetItem(neurons, i));
			float value = extract<float>(PyList_GetItem(values, i));
			obj.setNeuronState(neuron, param, value);
		}
	}
}



template<class T>
PyObject*
get_neuron_parameter(T& obj, PyObject* neurons, unsigned param)
{
	const Py_ssize_t len = PyList_Check(neurons) ? PyList_Size(neurons) : 1;
	if(len == 1) {
		return PyFloat_FromDouble(obj.getNeuronParameter(extract<unsigned>(neurons), param));
	} else {
		PyObject* list = PyList_New(len);
		for(Py_ssize_t i=0; i < len; ++i) {
			const unsigned neuron = extract<unsigned>(PyList_GetItem(neurons, i));
			const float val = obj.getNeuronParameter(neuron, param);
			PyList_SetItem(list, i, PyFloat_FromDouble(val));
		}
		return list;
	}
}



template<class T>
PyObject*
get_neuron_state(T& obj, PyObject* neurons, unsigned param)
{
	const Py_ssize_t len = PyList_Check(neurons) ? PyList_Size(neurons) : 1;
	if(len == 1) {
		return PyFloat_FromDouble(obj.getNeuronState(extract<unsigned>(neurons), param));
	} else {
		PyObject* list = PyList_New(len);
		for(Py_ssize_t i=0; i < len; ++i) {
			const unsigned neuron = extract<unsigned>(PyList_GetItem(neurons, i));
			const float val = obj.getNeuronState(neuron, param);
			PyList_SetItem(list, i, PyFloat_FromDouble(val));
		}
		return list;
	}
}



/* This wrappers for overloads of nemo::Simulation::step */
const std::vector<unsigned>&
step(nemo::Simulation& sim)
{
	return sim.step();
}


const std::vector<unsigned>&
step_f(nemo::Simulation& sim, const std::vector<unsigned>& fstim)
{
	return sim.step(fstim);
}


const std::vector<unsigned>&
step_i(nemo::Simulation& sim, const std::vector< std::pair<unsigned, float> >& istim)
{
	return sim.step(istim);
}


const std::vector<unsigned>&
step_fi(nemo::Simulation& sim,
		const std::vector<unsigned>& fstim,
		const std::vector< std::pair<unsigned, float> >& istim)
{
	return sim.step(fstim, istim);
}



void
initializeConverters()
{
	// register the from-python converter
	from_py_list<synapse_id>();
	from_py_list<unsigned>();
	from_py_list<unsigned char>();
	from_py_list<float>();
	from_py_list_of_pairs<unsigned, float>();
}



BOOST_PYTHON_MODULE(_nemo)
{
	def("init", initializeConverters);

	class_<std::vector<unsigned> >("std_vector_unsigned")
		.def(vector_indexing_suite<std::vector<unsigned> >())
		.def("__str__", &std_vector_str<unsigned>)
	;

	class_<std::vector<float> >("std_vector_float")
		.def(vector_indexing_suite<std::vector<float> >())
		.def("__str__", &std_vector_str<float>)
	;

	class_<std::vector<unsigned char> >("std_vector_uchar")
		.def(vector_indexing_suite<std::vector<unsigned char> >())
		.def("__str__", &std_bool_vector_str)
	;

	class_<std::vector<uint64_t> >("std_vector_uint64")
		.def(vector_indexing_suite<std::vector<uint64_t> >())
		.def("__str__", &std_vector_str<uint64_t>)
	;

	class_<nemo::Configuration>("Configuration", CONFIGURATION_DOC)
		//.def("enable_logging", &nemo::Configuration::enableLogging)
		//.def("disable_logging", &nemo::Configuration::disableLogging)
		//.def("logging_enabled", &nemo::Configuration::loggingEnabled)
		.def("set_stdp_function", &nemo::Configuration::setStdpFunction, CONFIGURATION_SET_STDP_FUNCTION_DOC)
		.def("set_cuda_backend", &nemo::Configuration::setCudaBackend, CONFIGURATION_SET_CUDA_BACKEND_DOC)
		.def("set_cpu_backend", &nemo::Configuration::setCpuBackend, CONFIGURATION_SET_CPU_BACKEND_DOC)
		.def("backend_description", &nemo::Configuration::backendDescription, CONFIGURATION_BACKEND_DESCRIPTION_DOC)
	;

	class_<nemo::Network, boost::noncopyable>("Network", NETWORK_DOC)
		.def("add_neuron", &nemo::Network::addNeuron, NETWORK_ADD_NEURON_DOC)
		.def("add_synapse", add_synapse, NETWORK_ADD_SYNAPSE_DOC)
		.def("set_neuron", &nemo::Network::setNeuron, CONSTRUCTABLE_SET_NEURON_DOC)
		.def("get_neuron_state", get_neuron_state<nemo::Network>, CONSTRUCTABLE_GET_NEURON_STATE_DOC)
		.def("get_neuron_parameter", get_neuron_parameter<nemo::Network>, CONSTRUCTABLE_GET_NEURON_PARAMETER_DOC)
		.def("set_neuron_state", set_neuron_state<nemo::Network>, CONSTRUCTABLE_SET_NEURON_STATE_DOC)
		.def("set_neuron_parameter", set_neuron_parameter<nemo::Network>, CONSTRUCTABLE_SET_NEURON_PARAMETER_DOC)
		.def("get_synapse_source", &nemo::Network::getSynapseSource)
		.def("neuron_count", &nemo::Network::neuronCount, NETWORK_NEURON_COUNT_DOC)
	;

	class_<nemo::Simulation, boost::shared_ptr<nemo::Simulation>, boost::noncopyable>(
			"Simulation", SIMULATION_DOC, no_init)
		.def("__init__", make_constructor(makeSimulation))
		// .def("step", &nemo::Simulation::step, return_internal_reference<1>(), SIMULATION_STEP_DOC)
			/* May want to make a copy here, for some added safety:
			 * return_value_policy<copy_const_reference>() */
		.def("step", step, return_internal_reference<1>(), SIMULATION_STEP_DOC)
		//! \todo add back these functions. Currently cannot distinguish between them
		//.def("step", step_f, return_internal_reference<1>(), SIMULATION_STEP_DOC)
		//.def("step", step_i, return_internal_reference<1>(), SIMULATION_STEP_DOC)
		.def("step", step_fi, return_internal_reference<1>(), SIMULATION_STEP_DOC)
		.def("apply_stdp", &nemo::Simulation::applyStdp, SIMULATION_APPLY_STDP_DOC)
		.def("set_neuron", &nemo::Simulation::setNeuron, CONSTRUCTABLE_SET_NEURON_DOC)
		.def("get_neuron_state", get_neuron_state<nemo::Simulation>, CONSTRUCTABLE_GET_NEURON_STATE_DOC)
		.def("get_neuron_parameter", get_neuron_parameter<nemo::Simulation>, CONSTRUCTABLE_GET_NEURON_PARAMETER_DOC)
		.def("set_neuron_state", set_neuron_state<nemo::Simulation>, CONSTRUCTABLE_SET_NEURON_STATE_DOC)
		.def("set_neuron_parameter", set_neuron_parameter<nemo::Simulation>, CONSTRUCTABLE_SET_NEURON_PARAMETER_DOC)
		.def("get_membrane_potential", &nemo::Simulation::getMembranePotential, SIMULATION_GET_MEMBRANE_POTENTIAL_DOC)
		.def("get_synapses_from", &nemo::Simulation::getSynapsesFrom, return_value_policy<copy_const_reference>(), SIMULATION_GET_SYNAPSES_FROM_DOC)
		.def("get_targets", &nemo::Simulation::getTargets, return_value_policy<copy_const_reference>(), SIMULATION_GET_TARGETS_DOC)
		.def("get_delays", &nemo::Simulation::getDelays, return_value_policy<copy_const_reference>(), SIMULATION_GET_DELAYS_DOC)
		.def("get_weights", &nemo::Simulation::getWeights, return_value_policy<copy_const_reference>(), SIMULATION_GET_WEIGHTS_DOC)
		.def("get_plastic", &nemo::Simulation::getPlastic, return_value_policy<copy_const_reference>(), SIMULATION_GET_PLASTIC_DOC)
		.def("elapsed_wallclock", &nemo::Simulation::elapsedWallclock, SIMULATION_ELAPSED_WALLCLOCK_DOC)
		.def("elapsed_simulation", &nemo::Simulation::elapsedSimulation, SIMULATION_ELAPSED_SIMULATION_DOC)
		.def("reset_timer", &nemo::Simulation::resetTimer, SIMULATION_RESET_TIMER_DOC)
	;
}