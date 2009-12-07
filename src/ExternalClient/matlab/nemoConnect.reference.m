% just set up all the globals
function nemoConnect(host, port)

	% TODO: move initialisation to setNetwork
	% Neuron parameters
	global NEMO_NEURONS_A;
	global NEMO_NEURONS_B;
	global NEMO_NEURONS_C;
	global NEMO_NEURONS_D;
	global NEMO_NEURONS_U;
	global NEMO_NEURONS_V;

	NEMO_NEURONS_A = [];
	NEMO_NEURONS_B = [];
	NEMO_NEURONS_C = [];
	NEMO_NEURONS_D = [];
	NEMO_NEURONS_U = [];
	NEMO_NEURONS_V = [];

	% Connectivity matrix
	global NEMO_CM;
	global NEMO_MAX_DELAY;

	NEMO_CM = {};
	NEMO_MAX_DELAY = 0;

	% STDP parameters
	global NEMO_STDP_ENABLED;
	global NEMO_STDP_PREFIRE;
	global NEMO_STDP_POSTFIRE;
	global NEMO_STDP_MAX_WEIGHT;

	NEMO_STDP_ENABLED = false;
	NEMO_STDP_PREFIRE = [];
	NEMO_STDP_POSTFIRE = [];
	NEMO_STDP_MAX_WEIGHT = 0;

	% Runtime data
	global NEMO_RECENT_FIRINGS;
	global NEMO_CYCLE;

	NEMO_CYCLE = 1;

	% Runtime statistics
	global NEMO_RTS_FIRED;
	global NEMO_RTS_LTP;
	global NEMO_RTS_LTD;
	global NEMO_RTS_STDP;

	NEMO_RTS_FIRED = 0;
	NEMO_RTS_LTP = 0;
	NEMO_RTS_LTD = 0;
	NEMO_RTS_STDP = 0;
end