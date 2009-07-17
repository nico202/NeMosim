module Network.Server (serveSimulation) where

-- Based on example in Real World Haskell, Chapter 27

import Control.Exception (try)
import Control.Parallel.Strategies (NFData)
import Data.Binary (Binary)
import Data.List (sort)
import Network.Socket
-- import Control.Concurrent
-- import Control.Concurrent.MVar
import System.IO (Handle, hPutStrLn)
import System.Time (getClockTime)

import Control.Exception(assert)
import Control.Monad (zipWithM)

import Network.Protocol hiding (getWeights)
import Simulation.Common
import Simulation.STDP
import Types
import qualified Util.Assocs as A (densify)
import qualified Util.List as L (chunksOf)



serveSimulation
    :: (Show n, Binary n, Show s, Binary s, NFData n, NFData s)
    => Handle           -- ^ Log output
    -> String           -- ^ Port number or name
    -> Bool             -- ^ Verbose
    -> SimulationInit n s
    -> IO ()
serveSimulation loghdl port verbose initfn = withSocketsDo $ do

    -- Look up the port. This either raises an exception or returns a non-empty
    -- list
    addrinfos <- getAddrInfo
        (Just (defaultHints {
                addrFlags = [AI_PASSIVE],
                addrFamily = AF_UNSPEC }))
        Nothing
        (Just port)
    let serveraddr = head addrinfos

    -- create a socket
    sock <- socket (addrFamily serveraddr) Stream defaultProtocol

    -- TODO: remove in production code. This makes TCP less reliable
    setSocketOption sock ReuseAddr 1

    bindSocket sock (addrAddress serveraddr)

    -- start listening for connection requests
    let maxQueueLength = 5
    listen sock maxQueueLength
    -- create a lock to use for synchronising access to the handler
    -- lock <- newMVar ()

    -- loop forever waiting for connections. Ctrl-C to abort
    procRequests sock loghdl verbose initfn



-- | Process incoming connection requests (only handle one at a time)
-- TODO: add back handling of multiple requests, but make sure to send
-- back the correct status code if busy
procRequests
    :: (Binary n, Show n, Binary s, Show s, NFData n, NFData s)
    => Socket
    -> Handle
    -> Bool
    -> SimulationInit n s
    -> IO ()
procRequests mastersock hdl verbose initfn = do
    (connsock, clientaddr) <- accept mastersock
    logMsg hdl clientaddr "Server.hs: client connected"
    (catch
        (procSim connsock verbose initfn (logMsg hdl clientaddr))
        (\e -> logMsg hdl clientaddr $
            "Server.hs: exception caught, simulation terminated\n\t" ++ show e))
    sClose connsock
    procRequests mastersock hdl verbose initfn



-- TODO: add back verbosity
-- | Process potential simulation request
procSim sock _ initfn log = do
    -- TODO: remove runfn from Protocol
    -- TODO: catch protocol errors here, esp invalid request
    ret <- startSimulationHost sock initfn
    case ret of
        Nothing     -> return ()
        Just sim -> procSimReq sock sim log


-- | Process user requests during running simulation
-- TODO: move some of this to Protocol.hs
procSimReq sock sim log = do
    req <- recvCommand sock
    case req of
        (CmdSync nsteps fstim applySTDP) -> do
            try (procSynReq nsteps sim fstim applySTDP) >>= either
                (\err -> do
                    sendResponse sock $ RspError $ show err
                    log $ "Server.hs: error: " ++ show err
                    stop sim)
                (\(probed, elapsed) -> do
                    sendResponse sock $ RspData probed elapsed
                    procSimReq sock sim log)
        CmdStop  -> stop sim
        CmdGetWeights -> do
            log "Server.hs: returning weight matrix"
            weights <- getWeights sim
            sendResponse sock $ RspWeights weights
            procSimReq sock sim log
        (CmdError c) -> do
            log $ "Server.hs: invalid simulation request: " ++ show c
            stop sim
    where
        stop sim = do
            closeSim sim
            log "Server.hs: stopping simulation"



procSynReq nsteps sim sparseFstim applySTDP = do
    -- We should do this by whatever increments are requested by the step
    -- TODO: should deal with nsteps that don't work out exactly
    resetTimer sim
    probed1 <- zipWithM (\f s -> (runStep sim) f s) fstim stdpApplications
    e <- elapsed sim
    putStrLn $ "Simulated " ++ (show nsteps) ++ " steps in " ++ (show e) ++ "ms"
    let probed = concat probed1
    assert ((length probed) == nsteps) $ do
    return (map getFiring probed, fromIntegral e)
    where
        sz = stepSize sim
        fstim = L.chunksOf sz $ A.densify 0 nsteps [] sparseFstim

        -- | We only deal with firing data here
        getFiring (NeuronState _) = error "Server.hs: simulation returned non-firing data"
        getFiring (FiringData firing) = sort firing

        -- only a single STDP application per request supported (in first cycle)
        stdpApplications = (applySTDP : tail nostdp) : repeat nostdp
        nostdp = replicate sz STDPIgnore





-- | Log message along with client information
logMsg :: Handle -> SockAddr -> String -> IO ()
logMsg hdl addr msg = do
    t <- getClockTime
    hPutStrLn hdl $ show t ++ " From " ++ show addr ++ ": " ++ msg
