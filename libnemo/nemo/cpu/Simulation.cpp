#include "Simulation.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

#ifdef NEMO_CPU_MULTITHREADED
#include <boost/thread.hpp>
#endif

#include <nemo/internals.hpp>
#include <nemo/exception.hpp>
#include <nemo/bitops.h>
#include <nemo/fixedpoint.hpp>

#define SUBSTEPS 4
#define SUBSTEP_MULT 0.25

#ifdef NEMO_CPU_DEBUG_TRACE

#include <cstdio>
#include <cstdlib>

#define LOG(...) fprintf(stdout, __VA_ARGS__);

#else

#define LOG(...)

#endif


namespace nemo {
	namespace cpu {



Simulation::Simulation(
		const nemo::NetworkImpl& net,
		const nemo::ConfigurationImpl& conf) :
	m_mapper(net),
	//! \todo remove redundant member?
	m_neuronCount(m_mapper.neuronCount()),
	m_a(m_neuronCount, 0),
	m_b(m_neuronCount, 0),
	m_c(m_neuronCount, 0),
	m_d(m_neuronCount, 0),
	m_u(m_neuronCount, 0),
	m_v(m_neuronCount, 0),
	m_sigma(m_neuronCount, 0),
	m_valid(m_neuronCount, false),
	m_fired(m_neuronCount, 0),
	m_recentFiring(m_neuronCount, 0),
	m_cm(conf),
	m_current(m_neuronCount, 0),
	m_fstim(m_neuronCount, 0),
	m_rng(m_neuronCount),
	m_lastFlush(0),
	m_stdp(conf.stdpFunction())
{
	nemo::initialiseRng(m_mapper.minLocalIdx(), m_mapper.maxLocalIdx(), m_rng);
	setNeuronParameters(net);
	//! \todo Determine fixedpoint format based on input network
	setConnectivityMatrix(net);
#ifdef NEMO_CPU_MULTITHREADED
	initWorkers(m_neuronCount, conf.cpuThreadCount());
#endif
	resetTimer();
}




void
Simulation::setNeuronParameters(const nemo::NetworkImpl& net)
{
	for(std::map<nidx_t, NetworkImpl::neuron_t>::const_iterator i = net.m_neurons.begin();
			i != net.m_neurons.end(); ++i) {
		nidx_t nidx = m_mapper.localIdx(i->first);
		NetworkImpl::neuron_t n = i->second;
		m_a.at(nidx) = n.a;	
		m_b.at(nidx) = n.b;	
		m_c.at(nidx) = n.c;	
		m_d.at(nidx) = n.d;	
		m_u.at(nidx) = n.u;	
		m_v.at(nidx) = n.v;	
		m_sigma.at(nidx) = n.sigma;	
		m_valid.at(nidx) = true;
	}
}



//! \todo fold this into CM class
void
Simulation::setConnectivityMatrix(const nemo::NetworkImpl& net)
{
	for(std::map<nidx_t, nemo::NetworkImpl::axon_t>::const_iterator ni = net.m_fcm.begin();
			ni != net.m_fcm.end(); ++ni) {
		nidx_t source = m_mapper.localIdx(ni->first);
		const nemo::NetworkImpl::axon_t& axon = ni->second;
		for(nemo::NetworkImpl::axon_t::const_iterator ai = axon.begin();
				ai != axon.end(); ++ai) {
			//! \todo modify indices *before* setting
			Row& row = m_cm.setRow(source, ai->first, ai->second);

			/* Convert global indices to local indices */
			for(size_t s=0; s < row.len; ++s) {
				row.data[s].target = m_mapper.localIdx(row.data[s].target);
			}
		}
	}

	m_cm.finalize();
}




#ifdef NEMO_CPU_MULTITHREADED

/* Allocate work to each thread */
void
Simulation::initWorkers(size_t neurons, unsigned threads)
{
	//! \todo log level of hardware concurrency
	size_t jobSize = neurons / threads;
	for(unsigned t=0; t < threads; ++t) {
		m_workers.push_back(Worker(t, jobSize, neurons, this));
	}
}

#endif



unsigned
Simulation::getFractionalBits() const
{
	return m_cm.fractionalBits();
}



void 
Simulation::step()
{
	const current_vector_t& current = deliverSpikes();
	update(m_fstim, current);
	setFiring();
	m_timer.step();
	//! \todo add separate unset so as to only touch minimal number of words
	//each iteration. Alternatively, just unset after use.
	std::fill(m_fstim.begin(), m_fstim.end(), 0);
}



void
Simulation::setFiringStimulus(const std::vector<unsigned>& fstim)
{
	// m_fstim should already be clear at this point
	for(std::vector<unsigned>::const_iterator i = fstim.begin();
			i != fstim.end(); ++i) {
		m_fstim.at(m_mapper.localIdx(*i)) = true;
	}
}



void
Simulation::setCurrentStimulus(const std::vector<fix_t>& current)
{
	if(current.empty()) {
		//! do we need to clear current?
		return;
	}
	/*! \todo We need to deal with the mapping from global to local neuron
	 * indices. Before doing this, we should probably change the interface
	 * here. Note that this function is only used internally (see mpi::Worker),
	 * so we might be able to use the existing interface, and make sure that we
	 * only use local indices. */
	throw nemo::exception(NEMO_API_UNSUPPORTED, "setting current stimulus vector not supported for CPU backend");
#if 0
	if(current.size() != m_current.size()) {
		throw nemo::exception(NEMO_INVALID_INPUT, "current stimulus vector not of expected size");
	}
	m_current = current;
#endif
}



void
Simulation::updateRange(int start, int end)
{
	unsigned fbits = getFractionalBits();

	for(int n=start; n < end; n++) {

		if(!m_valid[n]) {
			continue;
		}

		float current = fx_toFloat(m_current[n], fbits);
		m_current[n] = 0;

		if(m_sigma[n] != 0.0f) {
			current += m_sigma[n] * (float) m_rng[n].gaussian();
		}

		m_fired[n] = 0;

		for(unsigned int t=0; t<SUBSTEPS; ++t) {
			if(!m_fired[n]) {
				m_v[n] += SUBSTEP_MULT * ((0.04* m_v[n] + 5.0) * m_v[n] + 140.0 - m_u[n] + current);
				/*! \todo: could pre-multiply this with a, when initialising memory */
				m_u[n] += SUBSTEP_MULT * (m_a[n] * (m_b[n] * m_v[n] - m_u[n]));
				m_fired[n] = m_v[n] >= 30.0;
			}
		}

		m_fired[n] |= m_fstim[n];
		m_recentFiring[n] = (m_recentFiring[n] << 1) | (uint64_t) m_fired[n];

		if(m_fired[n]) {
			m_v[n] = m_c[n];
			m_u[n] += m_d[n];
			LOG("c%lu: n%u fired\n", elapsedSimulation(), m_mapper.globalIdx(n));
		}

	}
}



void
Simulation::update(
		const stimulus_vector_t& fstim,
		const current_vector_t& current)
{
#ifdef NEMO_CPU_MULTITHREADED
	if(m_workers.size() > 1) {
		/* It's possible to reduce thread creation overheads here by creating
		 * threads in the nemo::Simulation ctor and send signals to activate the
		 * threads. However, this was found to not produce any measurable speedup
		 * over this simpler implementation */
		boost::thread_group threads;
		for(std::vector<Worker>::const_iterator i = m_workers.begin();
				i != m_workers.end(); ++i) {
			threads.create_thread(*i);
		}
		/* All threads work here, filling in different part of the simulation data */
		threads.join_all();
	} else
#else
		updateRange(0, m_neuronCount);
#endif

	if(m_stdp.enabled()) {
		accumulateStdp();
	}
}



//! \todo use per-thread buffers and just copy these in bulk
void
Simulation::setFiring()
{
	unsigned t = elapsedSimulation() - m_lastFlush;
	for(unsigned n=0; n < m_neuronCount; ++n) { 
		if(m_fired[n]) {
			m_firedCycle.push_back(t);
			m_firedNeuron.push_back(m_mapper.globalIdx(n));
		}
	}
}



unsigned
Simulation::readFiring(
		const std::vector<unsigned>** cycles,
		const std::vector<unsigned>** nidx)
{		
	unsigned ret = m_timer.elapsedSimulation() - m_lastFlush;
	m_lastFlush = m_timer.elapsedSimulation();
	m_firedCycleExt = m_firedCycle;
	m_firedNeuronExt = m_firedNeuron;
	m_firedCycle.clear();
	m_firedNeuron.clear();
	*cycles = &m_firedCycleExt;
	*nidx = &m_firedNeuronExt;
	return ret;
}



void
Simulation::flushFiringBuffer()
{
	m_firedCycle.clear();
	m_firedNeuron.clear();
}



void
Simulation::applyStdp(float reward)
{
	throw nemo::exception(NEMO_API_UNSUPPORTED, "nemo::cpu::Simulation::applyStdp is not implemented");
	// m_cm.applyStdp(m_stdp.minWeight(), m_stdp.maxWeight(), reward);
}



Simulation::current_vector_t&
Simulation::deliverSpikes()
{
	/* Ignore spikes outside of max delay. We keep these older spikes as they
	 * may be needed for STDP */
	uint64_t validSpikes = ~(((uint64_t) (~0)) << m_cm.maxDelay());

	for(size_t source=0; source < m_neuronCount; ++source) {

		//! \todo make use of delay bits here to avoid looping
		uint64_t f = m_recentFiring[source] & validSpikes;

		int delay = 0;
		while(f) {
			int shift = 1 + ctz64(f);
			delay += shift;
			f = f >> shift;
			deliverSpikesOne(source, delay);
		}
	}

	return m_current;
}



void
Simulation::deliverSpikesOne(nidx_t source, delay_t delay)
{
	const nemo::Row& row = m_cm.getRow(source, delay);

	for(unsigned s=0; s < row.len; ++s) {
		const FAxonTerminal<fix_t>& terminal = row.data[s];
		assert(terminal.target < m_current.size());
		m_current.at(terminal.target) += terminal.weight;
		LOG("c%lu: n%u -> n%u: %+f (delay %u)\n",
				elapsedSimulation(),
				m_mapper.globalIdx(source),
				m_mapper.globalIdx(terminal.target),
				fx_toFloat(terminal.weight, getFractionalBits()), delay);
	}
}



unsigned
closestPreFire(const STDP<float>& stdp, uint64_t arrivals)
{
	uint64_t validArrivals = arrivals & stdp.preFireBits();
	int dt =  ctz64(validArrivals >> stdp.postFireWindow());
	return validArrivals ? (unsigned) dt : STDP<float>::STDP_NO_APPLICATION;
}



unsigned
closestPostFire(const STDP<float>& stdp, uint64_t arrivals)
{
	uint64_t validArrivals = arrivals & stdp.postFireBits();
	int dt = clz64(validArrivals << uint64_t(64 - stdp.postFireWindow()));
	return validArrivals ? (unsigned) dt : STDP<float>::STDP_NO_APPLICATION;
}



weight_t
Simulation::updateRegion(
		uint64_t spikes,
		nidx_t source,
		nidx_t target)
{
	throw nemo::exception(NEMO_API_UNSUPPORTED, "nemo::cpu::Simulation::updateRegion not implemented");
#if 0
	/* The potentiation can happen on either side of the firing. We want to
	 * find the one closest to the firing. We therefore need to compute the
	 * prefire and postfire dt's separately. */
	weight_t w_diff = 0.0;

	if(spikes) {

		uint dt_pre = closestPreFire(m_stdp, spikes);
		uint dt_post = closestPostFire(m_stdp, spikes);

		if(dt_pre < dt_post) {
			w_diff = m_stdp.lookupPre(dt_pre);
			LOG("c%u %s: %u -> %u %+f (dt=%d)\n",
					elapsedSimulation(), "ltp", source, target, w_diff, dt_pre);
		} else if(dt_post < dt_pre) {
			w_diff = m_stdp.lookupPost(dt_post);
			LOG("c%u %s: %u -> %u %+f (dt=%d)\n",
					elapsedSimulation(), "ltd", source, target, w_diff, dt_post);
		}
		// if neither is applicable dt_post == dt_pre == STDP_NO_APPLICATION
	}
	return w_diff;
#endif
}



void
Simulation::accumulateStdp()
{
	throw nemo::exception(NEMO_API_UNSUPPORTED, "nemo::cpu::Simulation::accumulateStdp not implemented");
#if 0
	/* Every cycle we process potentiation/depression relating to postsynaptic
	 * firings in the middle of the STDP window */
	uint64_t MASK = uint64_t(1) << m_stdp.postFireWindow();

	for(nidx_t post = 0; post < m_neuronCount; ++post) {
		if(m_recentFiring[post] & MASK) {

			const ConnectivityMatrix::Incoming& addresses = m_cm.getIncoming(post);
			ConnectivityMatrix::Accumulator& w_diffs = m_cm.getWAcc(post);

			for(size_t s = 0; s < addresses.size(); ++s) {

				nidx_t pre = addresses[s].source;
				uint64_t preFiring = m_recentFiring[pre] >> addresses[s].delay;
				uint64_t p_spikes = preFiring & m_stdp.potentiationBits();
				uint64_t d_spikes = preFiring & m_stdp.depressionBits();

				weight_t w_diff =
						updateRegion(p_spikes, pre, post) +
						updateRegion(d_spikes, pre, post);
				if(w_diff != 0.0) {
					w_diffs[s] += w_diff;
				}
			}

		}
	}
#endif
}



void
Simulation::getSynapses(
		unsigned sourceNeuron,
		const std::vector<unsigned>** targets,
		const std::vector<unsigned>** delays,
		const std::vector<float>** weights,
		const std::vector<unsigned char>** plastic)
{
	m_cm.getSynapses(sourceNeuron, m_targetsOut, m_delaysOut, m_weightsOut, m_plasticOut);
	for(std::vector<unsigned>::iterator i = m_targetsOut.begin();
			i != m_targetsOut.end(); ++i) {
		*i = m_mapper.globalIdx(*i);
	}
	*targets = &m_targetsOut;
	*delays = &m_delaysOut;
	*weights = &m_weightsOut;
	*plastic = &m_plasticOut;
}



unsigned long
Simulation::elapsedWallclock() const
{
	return m_timer.elapsedWallclock();
}



unsigned long
Simulation::elapsedSimulation() const
{
	return m_timer.elapsedSimulation();
}



void
Simulation::resetTimer()
{
	m_timer.reset();
}



void
chooseHardwareConfiguration(nemo::ConfigurationImpl& conf, int threadCount)
{
	conf.setBackend(NEMO_BACKEND_CPU);
	/*! \todo get processor name */
#ifdef NEMO_CPU_MULTITHREADED
	if(threadCount < 1) {
		conf.setCpuThreadCount(std::max(1U, boost::thread::hardware_concurrency()));
	} else {
		conf.setCpuThreadCount(threadCount);
	}
#else
	if(threadCount > 1) {
		throw nemo::exception(NEMO_INVALID_INPUT, "nemo compiled without multithreading support.");
	} else {
		conf.setCpuThreadCount(1);
	}
#endif
}


	} // namespace cpu
} // namespace nemo
