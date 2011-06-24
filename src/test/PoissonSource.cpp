namespace nemo {
	namespace test {
		namespace poisson {

/* Crudely test that the average rate over a long run approaches the expected value */
//! \todo test for both backends
void
testRate(unsigned duration, bool otherNeurons)
{
	nemo::Network net;
	nemo::Configuration conf;
	if(otherNeurons) {
		/* This population will never fire */
		createRing(&net, 1024, 1);
	}
	unsigned poisson = net.addNeuronType("PoissonSource");
	float rate = 0.010;
	net.addNeuron(poisson, 0, 1, &rate);
	boost::scoped_ptr<nemo::Simulation> sim(nemo::simulation(net, conf));
	unsigned nfired = 0;
	for(unsigned t=0; t<duration; ++t) {
		const std::vector<unsigned>& fired = sim->step();
		nfired += fired.size();
	}
	unsigned expected = abs(duration*rate);
	unsigned deviation = nfired - expected;
	//! \todo use a proper statistical test over a large number of runs
	BOOST_REQUIRE(nfired > 0);
	BOOST_REQUIRE(deviation < expected * 2);
}


		}
	}
}

BOOST_AUTO_TEST_SUITE(poisson)
	BOOST_AUTO_TEST_CASE(rate1s) { nemo::test::poisson::testRate(1000, false); }
	BOOST_AUTO_TEST_CASE(rate10s) { nemo::test::poisson::testRate(10000, false); }
	BOOST_AUTO_TEST_CASE(rate1sMix) { nemo::test::poisson::testRate(1000, true); }
	BOOST_AUTO_TEST_CASE(rate10sMix) { nemo::test::poisson::testRate(10000, true); }
BOOST_AUTO_TEST_SUITE_END()
