% nemoSimulation
%  
% A simulation is created from a network and a configuration object.
% The simulation is run by stepping through it, providing stimulus as
% appropriate. It is possible to read back synapse data at run time.
% The simulation also maintains a timer for both simulated time and
% wallclock time.
%  
% Methods:
%     nemoSimulation (constructor)
%     step
%     applyStdp
%     getTargets
%     getDelays
%     getWeights
%     getPlastic
%     elapsedWallclock
%     elapsedSimulation
%     resetTimer
%   
classdef nemoSimulation < handle

    properties
        % the MEX layer keeps track of the actual pointers;
        id = -1;
    end

    methods

        function obj = nemoSimulation(net, conf)
        % nemoSimulation - create a new simulation
        %
        % Inputs:
        %	net - an existing, populated network (nemoNetwork)
        %	conf - simulation configuration (nemoConfiguration)
        	obj.id = nemo_mex(uint32(12), uint32(net.id), uint32(conf.id));
        end

        function delete(obj)
            nemo_mex(uint32(13), obj.id);
        end
        function fired = step(sim, fstim)
        % step - run simulation for a single cycle (1ms)
        %
        % Synopsis:
        %	fired = step()
        %	fired = step(fstim)
        %
        % Inputs:
        % 	 fstim - An optional list of neurons, which will be forced to fire this cycle
        %
        % Output:
        %	fired - A list of the neurons which fired this cycle
            if nargin < 2
                fired = nemo_mex(uint32(14), sim.id, uint32(zeros(1, 0)));
            else
                fired = nemo_mex(uint32(14), sim.id, uint32(fstim));
            end
        end


        function applyStdp(obj, reward)
        % applyStdp - update synapse weights using the accumulated STDP statistics
        %  
        % Synopsis:
        %   applyStdp(reward)
        %  
        % Inputs:
        %   reward  - Multiplier for the accumulated weight change
        %     
            nemo_mex(uint32(15), obj.id, double(reward));
        end

        function targets = getTargets(obj, synapses)
        % getTargets - return the targets for the specified synapses
        %  
        % Synopsis:
        %   targets = getTargets(synapses)
        %  
        % Inputs:
        %   synapses -
        %             synapse ids (as returned by addSynapse)
        %    
        % Outputs:
        %   targets - indices of target neurons
        %     
            targets = nemo_mex(uint32(16), obj.id, uint64(synapses));
        end

        function delays = getDelays(obj, synapses)
        % getDelays - return the conductance delays for the specified synapses
        %  
        % Synopsis:
        %   delays = getDelays(synapses)
        %  
        % Inputs:
        %   synapses -
        %             synapse ids (as returned by addSynapse)
        %    
        % Outputs:
        %   delays  - conductance delays of the specified synpases
        %     
            delays = nemo_mex(uint32(17), obj.id, uint64(synapses));
        end

        function weights = getWeights(obj, synapses)
        % getWeights - return the weights for the specified synapses
        %  
        % Synopsis:
        %   weights = getWeights(synapses)
        %  
        % Inputs:
        %   synapses -
        %             synapse ids (as returned by addSynapse)
        %    
        % Outputs:
        %   weights - weights of the specified synapses
        %     
            weights = nemo_mex(uint32(18), obj.id, uint64(synapses));
        end

        function plastic = getPlastic(obj, synapses)
        % getPlastic - return the boolean plasticity status for the specified synapses
        %  
        % Synopsis:
        %   plastic = getPlastic(synapses)
        %  
        % Inputs:
        %   synapses -
        %             synapse ids (as returned by addSynapse)
        %    
        % Outputs:
        %   plastic - plasticity status of the specified synpases
        %     
            plastic = nemo_mex(uint32(19), obj.id, uint64(synapses));
        end

        function elapsed = elapsedWallclock(obj)
        % elapsedWallclock - 
        %  
        % Synopsis:
        %   elapsed = elapsedWallclock()
        %  
        % Outputs:
        %   elapsed - number of milliseconds of wall-clock time elapsed since
        %             first simulation step (or last timer reset)
        %     
            elapsed = nemo_mex(uint32(20), obj.id);
        end

        function elapsed = elapsedSimulation(obj)
        % elapsedSimulation - 
        %  
        % Synopsis:
        %   elapsed = elapsedSimulation()
        %  
        % Outputs:
        %   elapsed - number of milliseconds of simulation time elapsed since
        %             first simulation step (or last timer reset)
        %     
            elapsed = nemo_mex(uint32(21), obj.id);
        end

        function resetTimer(obj)
        % resetTimer - reset both wall-clock and simulation timer
        %  
        % Synopsis:
        %   resetTimer()
        %   
            nemo_mex(uint32(22), obj.id);
        end
    end
end
