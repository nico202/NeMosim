/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CycleCounters.hpp"

#include <algorithm>
#include <numeric>
#include <iterator>
#include <iostream>
#include <iomanip>

#include "kernel.cu_h"

namespace nemo {
	namespace cuda {


CycleCounters::CycleCounters(size_t partitionCount, bool stdpEnabled) :
	m_ccMain(partitionCount, CC_MAIN_COUNT-1, true),
	m_ccApplySTDP(partitionCount, 1, stdpEnabled),
	m_partitionCount(partitionCount),
	m_stdpEnabled(stdpEnabled)
{ }


const char* durationNames[] = {
	"init",
	"spike gather",
	"current/firing input",
	"random input",
	"fire",
	"local scatter",
	"global scatter",
	"STDP accumulation"
};



void
printLine(
		const char* label,
		cycle_counter_t cycles,
		cycle_counter_t total,
		cycle_counter_t clockRateKHz,
		std::ostream& outfile)
{
	cycle_counter_t timeMs = CC_MULT * cycles / clockRateKHz;
	outfile << std::setw(15) << label << ":" 
		<< std::setw(10) << timeMs << "ms, "
		<< std::setw(15) << cycles << "cycles, "; 
	if(total != 0)
		outfile << std::setw(4) << 100.0*(double(cycles)/double(total)) << "%";
	outfile << std::endl;
}


static
int
clockRate()
{
	int device;
	cudaGetDevice(&device);
	cudaDeviceProp prop;
	cudaGetDeviceProperties(&prop, device);
	return prop.clockRate;
}


void
CycleCounters::printCounterSet(
		NVector<cycle_counter_t>& cc_in,
		size_t counters,
		const char* setName,
		const char* names[], // for intermediate counters
		std::ostream& outfile)
{
	const cycle_counter_t* cc = cc_in.copyFromDevice();
	//! \todo average over all partitions
	/* The data return by copyFromDevice is the raw device data, including any
	 * padding. Using cc.end() would therefore read too far */ 
	cycle_counter_t totalCycles = std::accumulate(cc, cc + counters, 0l);

	int clockRateKHz = clockRate();
	printLine(setName, totalCycles, totalCycles, clockRateKHz, outfile);
	outfile << std::endl;

	if(names != NULL) {
		for(const cycle_counter_t*i = cc, *i_end = cc + counters;
				i != i_end; ++i) {
			cycle_counter_t cycles = *i;
			printLine(names[i-cc], cycles, totalCycles, clockRateKHz, outfile);
		}
		outfile << std::endl;
	}
}



void
CycleCounters::printCounters(std::ostream& outfile)
{
	printCounterSet(m_ccMain, CC_MAIN_COUNT-1, "Main", durationNames, outfile);
	if(m_stdpEnabled) {
		printCounterSet(m_ccApplySTDP, 1, "STDP (apply)", NULL, outfile);
	}
}



cycle_counter_t*
CycleCounters::data() const
{
	//! \todo return data for different sets
	return m_ccMain.deviceData();
}


size_t
CycleCounters::pitch() const
{
	//! \todo return data for different sets
	return m_ccMain.wordPitch();
}

	} // end namespace cuda
} // end namespace nemo