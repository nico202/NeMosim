{- | Generic synapse
 -
 - Due to memory issues synapses are generally stored in more compact formats
 - internally (see "AxonTerminal" and "Construction.Axon").
 -}

module Construction.Synapse (
        Synaptic(..),
        Synapse(..),
        AxonTerminal(AxonTerminal, atAux),
        Static,
        excitatory,
        inhibitory,
        retarget,
        strip,
        unstrip,
        withTarget,
        withWeight
    ) where

import Types (Source, Target, Delay, Weight, Idx)


class Synaptic s where
    target :: s -> Target
    delay :: s -> Delay
    weight :: s -> Weight


{- | In addition to the four main fields of the synapse there is an auxillary
 - "payload" field which can store additional data, such as plasticity
 - parameters, etc -}
data Synapse s = Synapse {
        -- TODO: rename function
        source :: {-# UNPACK #-} !Source,
        synapseTarget :: {-# UNPACK #-} !Target,
        synapseDelay  :: {-# UNPACK #-} !Delay,
        synapseWeight :: {-# UNPACK #-} !Weight,
        -- TODO: rename function
        sdata  :: {-# UNPACK #-} !s
    } deriving (Eq, Show, Ord)


instance Synaptic (Synapse s) where
    target = synapseTarget
    delay = synapseDelay
    weight = synapseWeight


excitatory, inhibitory :: Synapse s -> Bool
excitatory s = weight s > 0
inhibitory s = weight s < 0


retarget :: Target -> Synapse s -> Synapse s
retarget tgt' (Synapse src tgt d w pl) = Synapse src tgt' d w pl



{- | If the synapse is stored along with the presynaptic neuron, we don't need
 - to store the source -}
data AxonTerminal s = AxonTerminal {
        atTarget :: {-# UNPACK #-} !Target,
        atDelay  :: {-# UNPACK #-} !Delay,
        atWeight :: {-# UNPACK #-} !Weight,
        -- TODO: may want variable payload, but with specialisation for just a double
        atAux    :: {-# UNPACK #-} !s
    } deriving (Eq, Show, Ord)


instance Synaptic (AxonTerminal s) where
    target = atTarget
    delay = atDelay
    weight = atWeight


type Static = ()


strip :: Synapse s -> AxonTerminal s
strip s = AxonTerminal (target s) (delay s) (weight s) (sdata s)

-- TODO: remove
unstrip :: Source -> AxonTerminal s -> Synapse s
unstrip src (AxonTerminal t d w a) = Synapse src t d w a

withTarget f (AxonTerminal t d w a) = AxonTerminal (f t) d w a
withWeight f (AxonTerminal t d w a) = AxonTerminal t d (f w) a
