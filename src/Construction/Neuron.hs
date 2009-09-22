{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE BangPatterns #-}

module Construction.Neuron (
    -- * Construction
    Neuron,
    ndata,
    unconnected,
    neuron,
    -- * Query
    synapses,
    synapsesUnordered,
    synapsesByDelay,
    synapseCount,
    targets,
    -- * Modify
    connect,
    connectMany,
    disconnect,
    replaceSynapse,
    maxDelay,
    -- * Traversal
    withTargets,
    withSynapses,
    -- * Pretty-printing
    hPrintConnections
) where

import Control.Parallel.Strategies (NFData, rnf)
import System.IO (Handle)

import qualified Construction.Axon as Axon
import Construction.Synapse
import Types


data Neuron n s = Neuron {
        ndata :: n,
        axon :: Axon.Axon s
    } deriving (Eq)



{- | Create a neuron with no connections -}
unconnected :: n -> Neuron n s
unconnected n = Neuron n Axon.unconnected


{- | Create a neuron from a list of connections -}
neuron :: n -> [Synapse s] -> Neuron n s
-- neuron !n !ss = Neuron n $! Axon.fromList ss
neuron n ss = Neuron n $! Axon.fromList ss


{- | Apply function to synapses of a neuron -}
withAxon :: (Axon.Axon s -> Axon.Axon s) -> Neuron n s -> Neuron n s
withAxon f (Neuron n ss) = Neuron n $ f ss


withAxonM
    :: (Monad m)
    => (Axon.Axon s -> m (Axon.Axon s)) -> Neuron n s -> m (Neuron n s)
withAxonM f (Neuron n s) = f s >>= return . Neuron n


{- | Return unordered list of all synapses -}
synapses :: Idx -> Neuron n s -> [Synapse s]
synapses src n = Axon.synapses src $ axon n

synapsesUnordered :: Idx -> Neuron n s -> [Synapse s]
synapsesUnordered src n = Axon.synapsesUnordered src $ axon n

synapsesByDelay :: Neuron n s -> [(Delay, [(Idx, s)])]
synapsesByDelay = Axon.synapsesByDelay . axon


synapseCount :: Neuron n s -> Int
synapseCount = Axon.size . axon


{- | Return list of target neurons, including duplicates -}
targets :: Neuron n s -> [Target]
targets = Axon.targets . axon


{- | Add a single synapse to a neuron -}
connect :: Synapse s -> Neuron n s -> Neuron n s
connect s = withAxon (Axon.connect s)


{- | Add multiple synapses to a neuron -}
connectMany :: [Synapse s] -> Neuron n s -> Neuron n s
connectMany ss = withAxon (Axon.connectMany ss)


{- | Disconnect the first matching synapse -}
disconnect :: (Eq s) => Synapse s -> Neuron n s -> Neuron n s
disconnect s = withAxon (Axon.disconnect s)


{- | Replace the *first* matching synapse -}
replaceSynapse
    :: (Monad m, Show s, Eq s)
    => Synapse s -> Synapse s -> Neuron n s -> m (Neuron n s)
replaceSynapse old new = withAxonM (Axon.replaceM old new)


maxDelay :: Neuron n s -> Delay
maxDelay = Axon.maxDelay . axon


withTargets :: (Idx -> Idx) -> Neuron n s -> Neuron n s
withTargets f = withAxon (Axon.withTargets f)


withSynapses :: (s -> s) -> Neuron n s -> Neuron n s
withSynapses f = withAxon (Axon.withSynapses f)


hPrintConnections :: (Show s) => Handle -> Idx -> Neuron n s -> IO ()
hPrintConnections hdl source n = Axon.hPrintConnections hdl source $ axon n


instance (NFData n, NFData s) => NFData (Neuron n s) where
    rnf (Neuron n ss) = rnf n `seq` rnf ss `seq` ()


instance (Show n, Show s) => Show (Neuron n s) where
    showsPrec _ n = shows (ndata n) . showChar '\n' . shows (axon n)
