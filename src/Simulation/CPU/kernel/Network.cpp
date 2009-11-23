#ifdef PTHREADS_ENABLED
#include <sched.h> // for setting thread affinity
#endif
#include <assert.h>

#include <cmath>
#include <algorithm>

#include "Network.hpp"


#define SUBSTEPS 4
#define SUBSTEP_MULT 0.25


namespace nemo {
	namespace cpu {



Network::Network() :
	m_constructing(true),
	m_neuronCount(0),
	m_cycle(0)
{ }



Network::Network(
		fp_t a[],
		fp_t b[],
		fp_t c[],
		fp_t d[],
		fp_t u[],
		fp_t v[],
		fp_t sigma[], //set to 0 if thalamic input not required
		size_t ncount) :
	m_constructing(false),
	m_neuronCount(ncount),
	m_cycle(0)
{
	allocateNeuronData(ncount);
	allocateRuntimeData(ncount);

	std::copy(a, a + ncount, m_a.begin());
	std::copy(b, b + ncount, m_b.begin());
	std::copy(c, c + ncount, m_c.begin());
	std::copy(d, d + ncount, m_d.begin());
	std::copy(u, u + ncount, m_u.begin());
	std::copy(v, v + ncount, m_v.begin());
	std::copy(sigma, sigma + ncount, m_sigma.begin());

#ifdef PTHREADS_ENABLED
	initThreads(ncount);
#endif
}


Network::~Network()
{
#ifdef PTHREADS_ENABLED
	for(int i=0; i<m_nthreads; ++i) {
		pthread_attr_destroy(&m_thread_attr[i]);
	}
#endif
}


#ifdef PTHREADS_ENABLED

/* Initialise threads, but do not start them. Set the attributes, esp. thread
 * affinity, and pre-allocate work */
void
Network::initThreads(size_t ncount)
{
	int nthreads = m_nthreads;
	for(int i=0; i<nthreads; ++i) {
		pthread_attr_init(&m_thread_attr[i]);
		pthread_attr_setdetachstate(&m_thread_attr[i], PTHREAD_CREATE_JOINABLE);
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(i, &cpuset);
		pthread_attr_setaffinity_np(&m_thread_attr[i], sizeof(cpu_set_t), &cpuset);

		size_t jobSize = ncount/nthreads;
		size_t start = i * jobSize;
		//! \todo deal with special cases for small number of neurons
		size_t end = std::min((i+1) * jobSize, ncount);
		m_job[i] = new Job(start, end, ncount, this);
	}
}

#endif



void
Network::addNeuron(nidx_t neuronIndex,
				fp_t a, fp_t b, fp_t c, fp_t d,
				fp_t u, fp_t v, fp_t sigma)
{
	//! \todo use exceptions here instead
	assert(m_constructing);
	assert(m_acc.find(neuronIndex) == m_acc.end());
	m_acc[neuronIndex] = Neuron(a, b, c, d, u, v, sigma);
}



void
Network::finalize()
{
	if(m_constructing) {

		m_constructing = false;

		m_neuronCount = m_acc.size();
		nidx_t minIdx = m_acc.begin()->first;
		nidx_t maxIdx = m_acc.end()->first - 1;

		/* The simulator assumes a contingous range of neuron indices. We ought
		 * to be able to deal with invalid neurons, but should make sure to set
		 * the values to some sensible default. For now, just throw an error if
		 * the range of neuron indices is non-contigous. */
		assert(m_neuronCount == maxIdx + 1 - minIdx);

		allocateNeuronData(m_neuronCount);

		for(std::map<nidx_t, Neuron>::const_iterator i = m_acc.begin();
				i != m_acc.end(); ++i) {
			nidx_t idx = i->first;
			const Neuron& n = i->second;
			m_a.at(idx) = n.a;
			m_b.at(idx) = n.b;
			m_c.at(idx) = n.c;
			m_d.at(idx) = n.d;
			m_u.at(idx) = n.u;
			m_v.at(idx) = n.v;
			m_sigma.at(idx) = n.sigma;
		}

		/* We don't support further incremental construction, so we can clear the data */
		m_acc.clear();

		allocateRuntimeData(m_neuronCount);

#ifdef PTHREADS_ENABLED
		initThreads(m_neuronCount);
#endif
	}
}



void
Network::allocateNeuronData(size_t ncount)
{
	m_a.resize(m_neuronCount);
	m_b.resize(m_neuronCount);
	m_c.resize(m_neuronCount);
	m_d.resize(m_neuronCount);
	m_u.resize(m_neuronCount);
	m_v.resize(m_neuronCount);
	m_sigma.resize(m_neuronCount);
}



void
Network::allocateRuntimeData(size_t ncount)
{
	m_pfired.resize(m_neuronCount, 0);
	m_recentFiring.resize(m_neuronCount, 0);
	m_current.resize(m_neuronCount, 0);
}




void
Network::addSynapses(nidx_t source, delay_t delay,
			const nidx_t* targets, const weight_t* weights, size_t length)
{
	m_cm.setRow(source, delay, targets, weights, length);
}



void
Network::startSimulation()
{
	finalize();
	m_cm.finalize();
}



void
Network::step(unsigned int fstim[])
{
	startSimulation();
	deliverSpikes();
	update(fstim);
}



void
Network::updateRange(int start, int end, const unsigned int fstim[], RNG* rng)
{
	for(int n=start; n < end; n++) {

		if(m_sigma[n] != 0.0f) {
			m_current[n] += m_sigma[n] * (fp_t) rng->gaussian();
		}

		m_pfired[n] = 0;

		for(unsigned int t=0; t<SUBSTEPS; ++t) {
			if(!m_pfired[n]) {
				m_v[n] += SUBSTEP_MULT * ((0.04* m_v[n] + 5.0) * m_v[n] + 140.0 - m_u[n] + m_current[n]);
				/*! \todo: could pre-multiply this with a, when initialising memory */
				m_u[n] += SUBSTEP_MULT * (m_a[n] * (m_b[n] * m_v[n] - m_u[n]));
				m_pfired[n] = m_v[n] >= 30.0;
			}
		}

		m_pfired[n] |= fstim[n];
		m_recentFiring[n] = (m_recentFiring[n] << 1) | (uint64_t) m_pfired[n];

		if(m_pfired[n]) {
			m_v[n] = m_c[n];
			m_u[n] += m_d[n];
#ifdef DEBUG_TRACE
			fprintf(stderr, "c%u: n%u fired\n", m_cycle, n);
#endif
		}

	}
}



#ifdef PTHREADS_ENABLED

void*
start_thread(void* job_in)
{
	Job* job = static_cast<Job*>(job_in);
	(job->net)->updateRange(job->start, job->end, job->fstim, &job->rng);
	pthread_exit(NULL);
}

#endif



void
Network::update(unsigned int fstim[])
{
	startSimulation();

#ifdef PTHREADS_ENABLED
	for(int i=0; i<m_nthreads; ++i) {
		m_job[i]->fstim = &fstim[0];
		pthread_create(&m_thread[i], &m_thread_attr[i], start_thread, (void*) m_job[i]);
	}
	for(int i=0; i<m_nthreads; ++i) {
		pthread_join(m_thread[i], NULL);
	}
#else
	updateRange(0, m_neuronCount, fstim, &m_rng);
#endif

	m_cycle++;
}



const std::vector<unsigned int>&
Network::readFiring()
{
	m_fired.clear();
	for(int n=0; n < m_neuronCount; ++n) {
		if(m_pfired[n]) {
			m_fired.push_back(n);
		}
	}
	return m_fired;
}



void
Network::deliverSpikes()
{
	startSimulation();
	//! \todo reset the timer somewhere more sensible
	if(m_cycle == 0) {
		resetTimer();
	}

	/* Ignore spikes outside of max delay. We keep these older spikes as they
	 * may be needed for STDP */
	uint64_t validSpikes = ~(((uint64_t) (~0)) << m_cm.maxDelay());

	std::fill(m_current.begin(), m_current.end(), 0);

	for(size_t source=0; source < m_neuronCount; ++source) {

		//! \todo make use of delay bits here to avoid looping
		uint64_t f = m_recentFiring[source] & validSpikes;

		//! \todo add sanity check to make sure that ffsll takes 64-bit
		int delay = 0;
		while(f) {
			//! \todo do this in a way that's 64-bit safe.
			int shift = ffsll(f);
			delay += shift;
			f = f >> shift;
			deliverSpikesOne(source, delay);
		}
	}
}


void
Network::deliverSpikesOne(nidx_t source, delay_t delay)
{
	const Row& row = m_cm.getRow(source, delay);
	const Synapse* ss = row.data;

	/* could use pointer-arithmetic here, but profiling seems to indicate that
	 * it gives no advantage */
	for(int i=0; i < row.len; ++i) {
		const Synapse& s = ss[i];
		m_current[s.target] += s.weight;
#ifdef DEBUG_TRACE
		fprintf(stderr, "c%u: n%u -> n%u: %+f (delay %u)\n",
				m_cycle, source, s.target, s.weight, delay);
#endif
	}
}



long
Network::elapsed()
{
	return m_timer.elapsed();
}



void
Network::resetTimer()
{
	m_timer.reset();
}


	} // namespace cpu
} // namespace nemo
