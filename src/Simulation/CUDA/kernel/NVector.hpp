//! \file NVector.hpp

#ifndef N_VECTOR_HPP
#define N_VECTOR_HPP

#include <vector>


/*! \brief Per-neuron data array 
 *
 * Neuron data are organised on a per-partition basis. 
 *
 * \author Andreas Fidjeland
 */
template<typename T>
class NVector
{
    public :

		/*! Initialise a 1D parameter vector, potentially for several
		 * partitions. 
		 *
		 * The data is organised in a 2D data struture such that one row
		 * contains all the 1D data for a single cluster. 
		 *
		 * Crashes application on errors 
		 *
		 * \param maxPartitionSize 
		 * 		max size of all partitions in part of network simulated on
		 * 		device 
		 * \param partitionCount
		 * 		total number of partitionCount simulated on device 
		 */
        NVector(size_t partitionCount,
				size_t maxPartitionSize,
				bool allocHostData,
				size_t subvectorCount=1);
        
        ~NVector();

		/*! \return pointer to device data */
		T* deviceData() const;

		/*! \return number of words of data in each subvector, including padding */
		size_t size() const;

		/*! \return number of bytes of data in all vectors, including padding */
		size_t bytes() const;
		size_t d_allocated() const;

		/*! \return word pitch for vector, i.e. number of neurons (including
		 * padding) for each partition */
		size_t wordPitch() const;

		//! \todo remove byte pitch if not in use
		/*! \return byte pitch for vector, i.e. number of neurons (including
		 * padding) for each partition */
		size_t bytePitch() const;

		const std::vector<T>& copyFromDevice();

		/*! Copy entire host buffer to device and deallocote host memory */
		void moveToDevice();
		
		/*! Copy entire host buffer to the device */
		void copyToDevice();
		
		/*! Set row of data (in host buffer) for a single partition */ 
		//! \todo change parameter order, with vector first
		void setPartition(size_t partitionIdx, const T* arr, size_t length, size_t subvector=0);

        /*! Set value (in host buffer) for a single neuron */
		//! \todo change parameter order, with vector first
        void setNeuron(size_t partitionIdx, size_t neuronIdx, const T& val, size_t subvector=0);

        T getNeuron(size_t partitionIdx, size_t neuronIdx, size_t subvector=0) const;

    private :

		T* m_deviceData;
		std::vector<T> m_hostData;

		const size_t m_partitionCount;

		size_t m_pitch;

		size_t m_subvectorCount;

		size_t offset(size_t subvector, size_t partitionIdx, size_t neuronIdx) const;
};

#include "NVector.ipp"

#endif
