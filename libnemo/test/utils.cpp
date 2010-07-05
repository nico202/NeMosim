#include <vector>
#include <boost/test/unit_test.hpp>
#include <nemo.hpp>

void
runSimulation(
		const nemo::Network* net,
		const nemo::Configuration& conf,
		unsigned seconds,
		std::vector<unsigned>* fcycles,
		std::vector<unsigned>* fnidx)
{
	nemo::Simulation* sim = nemo::simulation(*net, conf);

	fcycles->clear();
	fnidx->clear();

	//! todo vary the step size between reads to firing buffer
	
	for(unsigned s = 0; s < seconds; ++s)
	for(unsigned ms = 0; ms < 1000; ++ms) {
		sim->step();

		//! \todo could modify API here to make this nicer
		const std::vector<unsigned>* cycles_tmp;
		const std::vector<unsigned>* nidx_tmp;

		sim->readFiring(&cycles_tmp, &nidx_tmp);

		// push data back onto local buffers
		std::copy(cycles_tmp->begin(), cycles_tmp->end(), back_inserter(*fcycles));
		std::copy(nidx_tmp->begin(), nidx_tmp->end(), back_inserter(*fnidx));
	}

	delete sim;
}



void
compareSimulationResults(
		const std::vector<unsigned>& cycles1,
		const std::vector<unsigned>& nidx1,
		const std::vector<unsigned>& cycles2,
		const std::vector<unsigned>& nidx2)
{
	BOOST_CHECK_EQUAL(cycles1.size(), nidx1.size());
	BOOST_CHECK_EQUAL(cycles2.size(), nidx2.size());
	BOOST_CHECK_EQUAL(cycles1.size(), cycles2.size());

	for(size_t i = 0; i < cycles1.size(); ++i) {
		// no point continuing after first divergence, it's only going to make
		// output hard to read.
		BOOST_CHECK_EQUAL(cycles1.at(i), cycles2.at(i));
		BOOST_CHECK_EQUAL(nidx1.at(i), nidx2.at(i));
		if(nidx1.at(i) != nidx2.at(i)) {
			BOOST_FAIL("c" << cycles1.at(i) << "/" << cycles2.at(i));
		}
	}
}