% nemoConfiguration
%  
%  
% Methods:
%     nemoConfiguration (constructor)
%     setCpuBackend
%     setCudaBackend
%     setStdpFunction
%   
classdef nemoConfiguration < handle

    properties
        % the MEX layer keeps track of the actual pointers;
        id = -1;
    end

    methods

        function obj = nemoConfiguration()
        	obj.id = nemo_mex(uint32(14));
        end

        function delete(obj)
            nemo_mex(uint32(15), obj.id);
        end

        function setCpuBackend(obj, tcount)
        % setCpuBackend - Specify that the CPU backend should be used
        %  
        % Synopsis:
        %   setCpuBackend(tcount)
        %  
        % Inputs:
        %   tcount  - number of threads
        %    
        % Specify that the CPU backend should be used and optionally specify
        % the number of threads to use. If the default thread count of -1 is
        % used, the backend will choose a sensible value based on the
        % available hardware concurrency. 
            nemo_mex(uint32(16), obj.id, int32(tcount));
        end

        function setCudaBackend(obj, deviceNumber)
        % setCudaBackend - Specify that the CUDA backend should be used
        %  
        % Synopsis:
        %   setCudaBackend(deviceNumber)
        %  
        % Inputs:
        %   deviceNumber -
        %    
        % Specify that the CUDA backend should be used and optionally specify
        % a desired device. If the (default) device value of -1 is used the
        % backend will choose the best available device. If the cuda backend
        % (and the chosen device) cannot be used for whatever reason, an
        % exception is raised. The device numbering is the numbering used
        % internally by nemo (see cudaDeviceCount and cudaDeviceDescription).
        % This device numbering may differ from the one provided by the CUDA
        % driver directly, since nemo ignores any devices it cannot use. 
            nemo_mex(uint32(17), obj.id, int32(deviceNumber));
        end

        function setStdpFunction(obj, prefire, postfire, minWeight, maxWeight)
        % setStdpFunction - Enable STDP and set the global STDP function
        %  
        % Synopsis:
        %   setStdpFunction(prefire, postfire, minWeight, maxWeight)
        %  
        % Inputs:
        %   prefire - STDP function values for spikes arrival times before the
        %             postsynaptic firing, starting closest to the postsynaptic firing
        %   postfire -
        %             STDP function values for spikes arrival times after the
        %             postsynaptic firing, starting closest to the postsynaptic firing
        %   minWeight -
        %             Lowest (negative) weight beyond which inhibitory synapses are not
        %             potentiated
        %   maxWeight -
        %             Highest (positive) weight beyond which excitatory synapses are not
        %             potentiated
        %    
        % The STDP function is specified by providing the values sampled at
        % integer cycles within the STDP window. 
            nemo_mex(...
                    uint32(18),...
                    obj.id,...
                    double(prefire),...
                    double(postfire),...
                    double(minWeight),...
                    double(maxWeight)...
            );
        end
    end
end
