#include "ConnectivityMatrixImpl.hpp"

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <boost/tuple/tuple_comparison.hpp>

#include "util.h"
#include "log.hpp"
#include "RSMatrix.hpp"
#include "except.hpp"
#include "SynapseGroup.hpp"
#include "connectivityMatrix.cu_h"
#include "dispatchTable.cu_h"
#include "connectivityMatrix.cu_h"


ConnectivityMatrixImpl::ConnectivityMatrixImpl(
        size_t partitionCount,
        size_t maxPartitionSize,
		bool setReverse) :
    m_partitionCount(partitionCount),
    m_maxPartitionSize(maxPartitionSize),
    m_maxDelay(0),
	m_setReverse(setReverse),
	md_allocatedFCM(0)
{
	for(uint p = 0; p < partitionCount; ++p) {
		m0_rsynapses.push_back(new RSMatrix(maxPartitionSize));
		m1_rsynapses.push_back(new RSMatrix(maxPartitionSize));
	}
}



std::vector<class RSMatrix*>&
ConnectivityMatrixImpl::rsynapses(size_t lvl)
{
	switch(lvl) {
		case 0: return m0_rsynapses;
		case 1: return m1_rsynapses;
		default : ERROR("invalid connectivity matrix index");
	}
}



const std::vector<class RSMatrix*>&
ConnectivityMatrixImpl::const_rsynapses(size_t lvl) const
{
	switch(lvl) {
		case 0: return m0_rsynapses;
		case 1: return m1_rsynapses;
		default : ERROR("invalid connectivity matrix index");
	}
}




boost::tuple<pidx_t, pidx_t, delay_t>
make_fcm_key(pidx_t source, pidx_t target, delay_t delay)
{
	return boost::tuple<pidx_t, pidx_t, delay_t>(source, target, delay);
}


void
ConnectivityMatrixImpl::addSynapse(size_t lvl, pidx_t sp, nidx_t sn, delay_t delay,
		pidx_t tp, nidx_t tn, weight_t w, uchar plastic)
{
	//! \todo make sure caller checks validity of sourcePartition
	if(delay > MAX_DELAY || delay == 0) {
		ERROR("delay (%u) out of range (1-%u)", delay, m_maxDelay);
	}

	SynapseGroup& fgroup = m_fsynapses[make_fcm_key(sp, tp, delay)];

	/* targetPartition not strictly needed here, but left in (in place of
	 * padding) for better code re-use */
	sidx_t sidx = fgroup.addSynapse(sn, tp, tn, w, plastic);

	if(m_setReverse && plastic) {
		RSMatrix* rgroup = rsynapses(lvl)[tp];
		rgroup->addSynapse(sp, sn, sidx, tn, delay);
	}

	m_maxDelay = std::max(m_maxDelay, delay);
}



void
ConnectivityMatrixImpl::setRow(
		size_t level, // 0 or 1
		uint sourcePartition,
		uint sourceNeuron,
		uint delay,
		const uint* targetPartition,
		const uint* targetNeuron,
		const float* weights,
		const uchar* isPlastic,
		size_t f_length)
{
	//! \todo do the mapping into l0 and l1 here directly

    if(f_length == 0)
        return;

	if(sourcePartition >= m_partitionCount) {
		ERROR("source partition index out of range");
	}

	if(sourceNeuron >= m_maxPartitionSize) {
		ERROR("source neuron index out of range");
	}

	for(size_t i=0; i<f_length; ++i) {
		addSynapse(level, sourcePartition, sourceNeuron, delay,
				targetPartition[i], targetNeuron[i], weights[i], isPlastic[i]);
		m_outgoing.addSynapse(sourcePartition, sourceNeuron, delay, targetPartition[i]);
	}
}




void
ConnectivityMatrixImpl::moveFcmToDevice()
{
	/* We add 1 extra warp here, so we can leave a null warp at the beginning */
	size_t totalWarpCount = 1 + m_outgoing.totalWarpCount();

	size_t height = totalWarpCount * 2; // *2 as we keep address and weights separately
	size_t desiredBytePitch = WARP_SIZE * sizeof(synapse_t);

	size_t bpitch;
	synapse_t* d_data;

	// allocate device memory
	cudaError err = cudaMallocPitch((void**) &d_data,
				&bpitch,
				desiredBytePitch,
				height);
	if(cudaSuccess != err) {
		throw DeviceAllocationException("forward connectivity matrix",
				height * desiredBytePitch, err);
	}
	md_fcm = boost::shared_ptr<synapse_t>(d_data, cudaFree);

	if(bpitch != desiredBytePitch) {
		std::cerr << "Returned byte pitch (" << desiredBytePitch
			<< ") did  not match requested byte pitch (" << bpitch
			<< ") when allocating forward connectivity matrix" << std::endl;
		/* This only matters, as we'll waste memory otherwise, and we'd expect the
		 * desired pitch to always match the returned pitch, since pitch is defined
		 * in terms of warp size */
	}

	// allocate and intialise host memory
	size_t wpitch = bpitch / sizeof(synapse_t);
	std::vector<synapse_t> h_data(height * wpitch, 0);

	size_t woffset = 1; // leave space for the null warp
	for(fcm_t::iterator i = m_fsynapses.begin(); i != m_fsynapses.end(); ++i) {
		woffset += i->second.fillFcm(woffset, totalWarpCount, h_data);
	}

	md_allocatedFCM = height * bpitch;
	CUDA_SAFE_CALL(cudaMemcpy(d_data, &h_data[0], md_allocatedFCM, cudaMemcpyHostToDevice));

	setFcmPlaneSize(totalWarpCount * wpitch);
}



void
ConnectivityMatrixImpl::moveToDevice()
{
	try {
		moveFcmToDevice();

		for(uint p=0; p < m_partitionCount; ++p){
			m0_rsynapses[p]->moveToDevice(m_fsynapses, p);
			m1_rsynapses[p]->moveToDevice(m_fsynapses, p);
		}

		//! \todo remove this copy. It's no longer needed.
		for(fcm_t::iterator i = m_fsynapses.begin();
				i != m_fsynapses.end(); ++i) {
			i->second.moveToDevice();
		}

		f_setDispatchTable();

		size_t maxWarps = m_outgoing.moveToDevice(m_partitionCount, m_fsynapses);
		m_incoming.allocate(m_partitionCount, maxWarps);

		configureReverseAddressing(
				r_partitionPitch(0),
				r_partitionAddress(0),
				r_partitionStdp(0),
				r_partitionFAddress(0),
				r_partitionPitch(1),
				r_partitionAddress(1),
				r_partitionStdp(1),
				r_partitionFAddress(1));

	} catch (DeviceAllocationException& e) {
		FILE* out = stderr;
		fprintf(out, e.what());
		printMemoryUsage(out);
		throw;
	}
}



void
ConnectivityMatrixImpl::printMemoryUsage(FILE* out)
{
	const size_t MEGA = 1<<20;
	fprintf(out, "forward matrix:     %6luMB (%lu groups out of max %lu)\n",
			d_allocatedFCM() / MEGA, m_fsynapses.size(),
			m_partitionCount*m_partitionCount*MAX_DELAY);
	fprintf(out, "forward matrix (2): %6luMB\n",
			md_allocatedFCM / MEGA);
	fprintf(out, "reverse matrix (0): %6luMB (%lu groups)\n",
			d_allocatedRCM0() / MEGA, m0_rsynapses.size());
	fprintf(out, "reverse matrix (1): %6luMB (%lu groups)\n",
			d_allocatedRCM1() / MEGA, m1_rsynapses.size());
	//! \todo dispatch table
	fprintf(out, "incoming:           %6luMB\n", m_incoming.allocated() / MEGA);
	fprintf(out, "outgoing:           %6luMB\n", m_outgoing.allocated() / MEGA);
}



size_t
ConnectivityMatrixImpl::getRow(
		pidx_t sourcePartition,
		nidx_t sourceNeuron,
		delay_t delay,
		uint currentCycle,
		pidx_t* partition[],
		nidx_t* neuron[],
		weight_t* weight[],
		uchar* plastic[])
{
	mf_targetPartition.clear();
	mf_targetNeuron.clear();
	mf_weights.clear();
	mf_plastic.clear();

	size_t rowLength = 0;

	// get all synapse groups for which this neuron is present
	for(pidx_t targetPartition = 0; targetPartition < m_partitionCount; ++targetPartition) {
		fcm_t::iterator group = m_fsynapses.find(make_fcm_key(sourcePartition, targetPartition, delay));
		if(group != m_fsynapses.end()) {

			pidx_t* pbuf;
			nidx_t* nbuf;
			weight_t* wbuf;
			uchar* sbuf;

			size_t len = group->second.getWeights(sourceNeuron, currentCycle, &pbuf, &nbuf, &wbuf, &sbuf);

			std::copy(pbuf, pbuf+len, back_inserter(mf_targetPartition));
			std::copy(nbuf, nbuf+len, back_inserter(mf_targetNeuron));
			std::copy(wbuf, wbuf+len, back_inserter(mf_weights));
			std::copy(sbuf, sbuf+len, back_inserter(mf_plastic));

			rowLength += len;
		}
	}

	*partition = &mf_targetPartition[0];
	*neuron = &mf_targetNeuron[0];
	*weight = &mf_weights[0];
	*plastic = &mf_plastic[0];
	return rowLength;
}



void
ConnectivityMatrixImpl::clearStdpAccumulator()
{
	//! \todo this might be done better in a single kernel, to reduce bus traffic
	for(uint p=0; p < m_partitionCount; ++p){
		m0_rsynapses[p]->clearStdpAccumulator();
		m1_rsynapses[p]->clearStdpAccumulator();
	}
}



size_t
ConnectivityMatrixImpl::d_allocatedFCM() const
{
	size_t bytes = 0;
	for(fcm_t::const_iterator i = m_fsynapses.begin(); i != m_fsynapses.end(); ++i) {
		bytes += i->second.d_allocated();
	}
	return bytes;
}



size_t
ConnectivityMatrixImpl::d_allocatedRCM0() const
{
	size_t bytes = 0;
	for(std::vector<RSMatrix*>::const_iterator i = m0_rsynapses.begin();
			i != m0_rsynapses.end(); ++i) {
		bytes += (*i)->d_allocated();
	}
	return bytes;
}



size_t
ConnectivityMatrixImpl::d_allocatedRCM1() const
{
	size_t bytes = 0;
	for(std::vector<RSMatrix*>::const_iterator i = m1_rsynapses.begin();
			i != m1_rsynapses.end(); ++i) {
		bytes += (*i)->d_allocated();
	}
	return bytes;
}



size_t
ConnectivityMatrixImpl::d_allocated() const
{

	return d_allocatedFCM()
		+ d_allocatedRCM0()
		+ d_allocatedRCM1()
		+ m_incoming.allocated()
		+ m_outgoing.allocated();
	//! \todo + dispatch table
}



/* Pack a device pointer to a 32-bit value */
//! \todo replace with non-template version
template<typename T>
DEVICE_UINT_PTR_T
devicePointer(T ptr)
{
	uint64_t ptr64 = (uint64_t) ptr;
#ifndef __DEVICE_EMULATION__
	//! \todo: look up this data at runtime
	//! \todo assert that we can fit all device addresses in 32b address.
	const uint64_t MAX_ADDRESS = 4294967296LL; // on device
	if(ptr64 >= MAX_ADDRESS) {
		throw std::range_error("Device pointer larger than 32 bits");
	}
#endif
	return (DEVICE_UINT_PTR_T) ptr64;

}



/* Map function over vector of reverse synapse matrix */
template<typename T, class S>
const std::vector<DEVICE_UINT_PTR_T>
mapDevicePointer(const std::vector<S*>& vec, std::const_mem_fun_t<T, S> fun)
{
	std::vector<DEVICE_UINT_PTR_T> ret(vec.size(), 0);
	for(uint p=0; p < vec.size(); ++p){
		T ptr = fun(vec[p]);
		ret[p] = devicePointer(ptr);
	}
	return ret;
}



const std::vector<DEVICE_UINT_PTR_T>
ConnectivityMatrixImpl::r_partitionPitch(size_t lvl) const
{
	return mapDevicePointer(const_rsynapses(lvl), std::mem_fun(&RSMatrix::pitch));
}



const std::vector<DEVICE_UINT_PTR_T>
ConnectivityMatrixImpl::r_partitionAddress(size_t lvl) const
{
	return mapDevicePointer(const_rsynapses(lvl), std::mem_fun(&RSMatrix::d_address));
}



const std::vector<DEVICE_UINT_PTR_T>
ConnectivityMatrixImpl::r_partitionStdp(size_t lvl) const
{
	return mapDevicePointer(const_rsynapses(lvl), std::mem_fun(&RSMatrix::d_stdp));
}



const std::vector<DEVICE_UINT_PTR_T>
ConnectivityMatrixImpl::r_partitionFAddress(size_t lvl) const
{
	return mapDevicePointer(const_rsynapses(lvl), std::mem_fun(&RSMatrix::d_faddress));
}



void
ConnectivityMatrixImpl::f_setDispatchTable()
{
	size_t delayCount = MAX_DELAY;

	size_t xdim = m_partitionCount;
	size_t ydim = m_partitionCount;
	size_t zdim = delayCount;
	size_t size = xdim * ydim * zdim;

	fcm_ref_t null = fcm_packReference(0, 0);
	std::vector<fcm_ref_t> table(size, null);

	for(fcm_t::const_iterator i = m_fsynapses.begin(); i != m_fsynapses.end(); ++i) {

		fcm_key_t fidx = i->first;
		const SynapseGroup& sg = i->second;

		// x: delay, y : target partition, z : source partition
		pidx_t source = boost::tuples::get<0>(fidx);
		pidx_t target = boost::tuples::get<1>(fidx);
		delay_t delay = boost::tuples::get<2>(fidx);
		size_t addr = (source * m_partitionCount + target) * delayCount + (delay-1);

		void* fcm_addr = sg.d_address();
		size_t fcm_pitch = sg.wpitch();
		table.at(addr) = fcm_packReference(fcm_addr, fcm_pitch);
	}

	cudaArray* f_dispatch = ::f_setDispatchTable(m_partitionCount, delayCount, table);
	mf_dispatch = boost::shared_ptr<cudaArray>(f_dispatch, cudaFreeArray);
}
