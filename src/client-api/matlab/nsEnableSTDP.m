% nsEnableSTDP: enable and configure STDP
%
%   nsEnableSTDP(PRE_FIRE, POST_FIRE, MAX_WEIGHT)
%
% If set before nsStart is called, the backend is configured to run with STDP.
% It will gather synapses modification statistics continously. To update the
% synapses using the accumulated values, call nsApplySTDP.
%
% Synapses are modified either when a spike arrives either shortly before or
% shortly after the postsynaptic neuron fires.
%
% The vectors PRE_FIRE and POST_FIRE specify the value that is added to the
% synapses weight (when nsApplySTDP is called) in the two cases for different
% values of dt+1 (where dt is time difference between spike arrival and
% firing). The +1 is due to the 1-based indexing used in Matlab;
% it's possible to have dt=0. For example PRE_FIRE[2] specifies
% the term to add to a synapse for which a spike arrived one cycle before the
% postsynaptic fired.
%
% The length of each array specify the time window during which STDP has an
% effect.
%
% In the regular asymetric STDP, PRE_FIRE leads to potentiation and is hence
% positive, whereas POST_FIRE leads to depression and is hence negative.
%
% Only excitatory synapses are affectd by STDP. The weights of the affected
% synapses are never allowed to increase above MAX_WEIGHT, and likewise not
% allowed to go negative. However, it is possible for a synapse to recover
% after reaching weight 0.
%
% STDP is disabled by default. When nsEnableSTDP is called it is enabled for
% all subsequent simulations until nsDisableSTDP is called. 

% Just store the values, configuration is done in nsStart
function nsEnableSTDP(prefire, postfire, maxWeight)
    global NS_STDP_ACTIVE;
    global NS_STDP_PRE_FIRE;
    global NS_STDP_POST_FIRE;
    global NS_STDP_MAX_WEIGHT;

    NS_STDP_ACTIVE = int32(1);
    NS_STDP_PRE_FIRE = prefire;
    NS_STDP_POST_FIRE = postfire;
    NS_STDP_MAX_WEIGHT = maxWeight;
end
