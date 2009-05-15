{-# LANGUAGE CPP #-}

module Test.Examples.Ring (tests) where

import Control.Monad (when)
import Data.Maybe
import Data.IORef
import Test.HUnit

import Construction.Construction (build)
import Construction.Izhikevich
import Construction.Network (Network)
import Construction.Synapse (Static)
import Examples.Ring (ring)
import Options
import Simulation.Common hiding (cycles)
import Simulation.FiringStimulus
import Simulation.Run (runSim)
import Types


data RingState = RingState {
        active :: Idx,
        size   :: Int,
        t      :: Time
    }


-- The ring network should produce a fixed pattern of just a single neuron
-- firing
checkFiring :: Int -> Idx -> Int -> IORef RingState -> ProbeData -> Assertion
checkFiring size impulse delay ref = \p -> do
    s <- readIORef ref
    let s' = next s
    writeIORef ref s'
    check p s
    return ()
    where
        next (RingState a sz t) =
            if t `mod` delay == 0
                then RingState ((a+1) `mod` sz) sz (t+1)
                else RingState a sz (t+1)
        check (NeuronState _) _ = error "non-firing data in ring network test"
        check (FiringData fs) (RingState expected _ t) = do
            when (delay == 1 && length fs /= 1) $ assertFailure $
                "ring network did not produce exactly one firing during cycle "
                ++ (show t) ++ ", instead it produced " ++ (show fs)
            if t `mod` delay == 0
                then assertEqual "ring network firing mistmatch" [expected] fs
                else assertEqual "ring network firing mistmatch" [] fs



testRing :: Int -> Idx -> Int -> Backend -> Maybe Int -> Assertion
testRing size impulse delay backend clusterSz = do
    let net = build 123456 $ ring size delay :: Network (IzhNeuron FT) Static
        duration = Until $ size * 2
        fstim = FiringList [(0, [impulse])]
        -- test = checkFiring size impulse delay
        probefn = Firing :: ProbeFn IzhState
#if defined(CUDA_ENABLED)
        opts = defaultOptions { optCuPartitionSz = clusterSz }
#else
        opts = defaultOptions
#endif
    ref <- newIORef $ RingState impulse size 0
    let test = checkFiring size impulse delay ref
    runSim backend duration net All probefn 4 fstim test opts Nothing



tests = TestList [
#if defined(CUDA_ENABLED)
        TestLabel "1000-sized ring on gpu"
           (TestCase $ testRing 1000 0 1 CUDA $ Just 1000),

        -- excercise 10 clusters
        TestLabel "1000-sized ring on gpu using partition size of 128"
           (TestCase $ testRing 1000 0 1 CUDA $ Just 128),

        -- excercise 40 clusters, requiring timeslicing of MPs
        TestLabel "4000-sized ring on gpu using partition size of 128, testing spikes crosing cluster boundaries"
           (TestCase $ testRing 1000 0 1 CUDA $ Just 128),

        TestLabel "2000-sized ring on gpu with spike injection into cluster outside cluster 0 (neuron 1500)"
           (TestCase $ testRing 2000 1500 1 CUDA $ Just 1000),

        TestLabel "2000-sized ring on gpu with spike injection into last neuron in cluster 14 (of 16)"
           (TestCase $ testRing 2000 1792 1 CUDA $ Just 128),

        TestLabel "2000-sized ring on gpu using cluster size of 128, with delays of 3"
           (TestCase $ testRing 2000 0 3 CUDA $ Just 128),
#endif
        TestLabel "1000-sized ring on cpu, delays of 1"
           (TestCase $ testRing 1000 0 1 CPU Nothing),

        TestLabel "1000-sized ring on cpu, delays of 3"
           (TestCase $ testRing 1000 0 3 CPU Nothing)
    ]
