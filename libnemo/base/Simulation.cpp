/* Copyright 2010 Imperial College London
 *
 * This file is part of nemo.
 *
 * This software is licenced for non-commercial academic use under the GNU
 * General Public Licence (GPL). You should have received a copy of this
 * licence along with nemo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Simulation.hpp"
#include "SimulationBackend.hpp"

namespace nemo {

Simulation*
Simulation::create(const Network& net, const Configuration& conf)
{
	return dynamic_cast<Simulation*>(SimulationBackend::create(net, conf));
}



Simulation::~Simulation()
{
	;
}

} // end namespace nemo
