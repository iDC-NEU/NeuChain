//
// Created by peng on 2022/2/28.
//

#ifndef NEU_BLOCKCHAIN_CONSENSUS_CONTROL_BLOCK_H
#define NEU_BLOCKCHAIN_CONSENSUS_CONTROL_BLOCK_H

#include "epoch_server/raft/consensus_manager.h"
#include "common/aria_types.h"
#include "common/cv_wrapper.h"
#include "common/timer.h"
#include "common/hash_map.h"
#include <comm.pb.h>
#include <thread>
#include <utility>

class ProxyBlock {
public:
    epoch_size_t latestEpoch = 0;
    time_t lastHeartbeat = 0;
    bool isValid = true;
    // for invalid proxies
    std::unordered_map<epoch_size_t, std::set<std::string>> epochVoters;
    epoch_size_t firstInvalidEpoch = 0;
};

class BatchAtomicBroadcaster: public ConsensusManager<BatchAtomicBroadcaster> {
public:
    BatchAtomicBroadcaster(const consensus_param &param, raft::Chain* chainPtr, raft::ChainService* servicePtr, std::function<void(const std::string &)> receive_from_raft_callback, bool isLeader)
            : ConsensusManager(chainPtr, servicePtr, param.group_id, param.batch_size), isLeader(isLeader) {
        // todo:current impl only support MAX_EPOCH size
        this->chain->get_user_request_handler = std::move(receive_from_raft_callback);
    }

    inline void sendEpochFinishSignal(const std::string& msg) {
        CHECK(isLeader);
        // atomic broadcast to all client proxies
        push_message_to_raft(std::string(msg));
    }

    inline void sendViewChangeVote(const std::string& msg) {
        CHECK(isLeader);
        // atomic broadcast to all client proxies
        push_message_to_raft(std::string(msg));
    }

    // leave for blank
    void receiver_from_user() { }
    void receiver_from_raft() { }

private:
    const bool isLeader;
};

class ConsensusControlBlock {
public:
    static ConsensusControlBlock* getInstance() {
        if (ConsensusControlBlock::instance == nullptr) {
            ConsensusControlBlock::instance = new ConsensusControlBlock("block_server_raft");
        }
        return ConsensusControlBlock::instance;
    }

    ~ConsensusControlBlock() {
        start = false;
        spinCheckThread.join();
        raftReceiverThread.join();
        server->Stop(0);
        server->Join();
    }

    // init
    void setConsensusCount(int total) {
        CHECK(!start.load(std::memory_order_acquire));
        totalServerCount = total;
    }

    // init
    void startService() {
        CHECK(!start.load(std::memory_order_acquire));
        start.store(true, std::memory_order_release);
        spinCheckThread = std::thread(&ConsensusControlBlock::spinCheckTimeout, this);
        raftReceiverThread = std::thread(&ConsensusControlBlock::receiver_from_multi_raft_instance, this);
    }

    // for client proxy thread
    void recvProofSet(const comm::TransactionsWithProof& proofSet) {
        if(!start.load(std::memory_order_acquire)) {
            LOG(INFO) << "ConsensusControlBlock is not ready!";
            return; // not ready
        }

        const auto& emitter = proofSet.ip();
        std::lock_guard guard(proxiesTimeoutMutex);
        if (proxiesTimeout.count(emitter) != 0 && !proxiesTimeout[emitter].isValid) {
            return; // we do not accept invalid proxies message
        }    // when all server is ok, we move on
        switch (proofSet.type()) {
            case comm::TransactionsWithProof_Type_HEARTBEAT: {
                // create new msg
                auto& blk = proxiesTimeout[emitter];
                blk.lastHeartbeat = BlockBench::Timer::timeNow();
                CHECK(blk.isValid == true);
                break;
            }
            case comm::TransactionsWithProof_Type_CON: {
                // create new msg
                auto& blk = proxiesTimeout[emitter];
                blk.latestEpoch = proofSet.epoch();
                blk.lastHeartbeat = BlockBench::Timer::timeNow();
                CHECK(blk.isValid == true);
                confirmedClientProxy(emitter, proofSet.epoch());
                break;
            }
            case comm::TransactionsWithProof_Type_MALICIOUS: {
                const auto& targetId = proofSet.proof();
                auto& blk = proxiesTimeout[emitter];
                if (!blk.isValid)
                    return;
                obtainMalicious(emitter, targetId, proofSet.epoch());
                break;
            }
            default:
                // TODO: bug here
                // CHECK(false);
                break;
        }
    }

    // for client proxy thread
    inline void sendEpochFinishSignal(const std::string& msg) {
        local_leader->sendEpochFinishSignal(msg);
    }

    // for coordinator thread
    void waitForEpochFinish(epoch_size_t epoch) {
        epochContinueCVWrapper.wait([&]() -> bool {
            // if true, all vote yes, continue
            if((int)epochApproveMap[epoch].size() == totalServerCount) {
                DLOG(INFO) << "consensus of epoch=" << epoch << " has reach, all vote yes.";
                return true;
            }
            DLOG(INFO) << "consensus of epoch=" << epoch << " not reach, waiting.";
            return false;
        });
    }

    // coordinator call this func after call waitForEpochFinish
    bool needReReserve(epoch_size_t epoch) {
        return epochStaleBit[epoch];
    }

    // if epoch is invalid, epoch+1 is invalid too
    bool isCurrentEpochInvalid(const std::string& ip, epoch_size_t epoch) {
        std::lock_guard guard(proxiesTimeoutMutex);
        if(proxiesTimeout[ip].isValid)
            return true;
        return proxiesTimeout[ip].firstInvalidEpoch <= epoch;
    }

    // the callback func should:
    // 1. broadcast blocking client proxy signal.
    // 2. invalid every txn received from that client proxy since that epoch.
    // the callback process must be sync.
    // set the function twice
    void setMaliciousCallbackHandle(const std::function<void(const std::string&, epoch_size_t)>& callback) {
        CHECK(!start.load(std::memory_order_acquire));
        maliciousBroadcastHandle = callback;
    }

    void broadcastMaliciousViewChange(const std::string &maliciousIP, epoch_size_t maliciousEpoch) {
        LOG(WARNING) << "Emit view change request for " << maliciousIP << " at epoch = " << maliciousEpoch;
        // emit the malicious request
        auto* configPtr = YAMLConfig::getInstance();
        comm::TransactionsWithProof proofSet;
        proofSet.set_epoch(maliciousEpoch);
        proofSet.set_ip(configPtr->getLocalBlockServerIP());
        proofSet.set_proof(maliciousIP); // remote ip
        proofSet.set_type(comm::TransactionsWithProof_Type_MALICIOUS);
        // broadcast to all block servers
        const auto rawProofSet = proofSet.SerializeAsString();
        local_leader->sendViewChangeVote(rawProofSet);
    }

protected:
    explicit ConsensusControlBlock(const std::string& group_name) {
        server = std::make_unique<brpc::Server>();
        // todo:current impl only support MAX_EPOCH size
        auto* configPtr = YAMLConfig::getInstance();

        // 1. obtain local id
        size_t localID = 0;
        // assert block servers ip are in the same order
        for(const auto& ip: configPtr->getBlockServerIPs()) {
            // compare with the ips in cluster to determine the sequence of local ip.
            if(ip == configPtr->getLocalBlockServerIP()) {
                break;
            }
            localID++;
        }
        // 2.1 init
        const int raft_port = 18100;
        auto* chain = new raft::Chain;
        chain->setLeaderGroupId((int)localID);
        auto* service = new raft::ChainServiceImpl(chain);
        BatchAtomicBroadcaster::BrpcAddService(server.get(),  raft_port, service);
        all_raft_instance.resize(configPtr->getBlockServerCount());
        for (size_t i=0; i<configPtr->getBlockServerCount(); i++) {
            auto param = BatchAtomicBroadcaster::GenerateBlockServerConsensusParam(configPtr->getLocalBlockServerIP(), raft_port, (int)i);
            all_raft_instance[i] = std::make_unique<BatchAtomicBroadcaster>(param, chain, service, [this](const std::string &user_request_raw) {
                // we get consensus user_request_raw here
                this->multi_raft_receiver.push(std::string(user_request_raw));
                DLOG(INFO) << "ConsensusControlBlock a msg from raft.";
            }, localID == i);
        }
        LOG(INFO) << "LOCAL id = " << localID;
        local_leader = all_raft_instance[localID].get();
        // 2.2 start server
        BatchAtomicBroadcaster::StartBrpcServer(server.get(), raft_port);
        CHECK(all_raft_instance.size() == configPtr->getBlockServerCount());
        // 2.3 spin up all servers
        for (size_t i=0; i<configPtr->getBlockServerCount(); i++) {
            auto param = BatchAtomicBroadcaster::GenerateBlockServerConsensusParam(configPtr->getLocalBlockServerIP(), raft_port, (int)i);
            auto actual_group_name = group_name + "_" + std::to_string(i);
            all_raft_instance[i]->startChainInstance(param.initial_conf, param.raft_ip, raft_port, actual_group_name.data(), param.group_id);
        }
        sleep(5);
        chain->refreshLeader();
        epochApproveMap.resize(MAX_EPOCH);
        epochStaleBit.resize(MAX_EPOCH);
    }

    void spinCheckTimeout() {
        auto* configPtr = YAMLConfig::getInstance();
        while(start.load(std::memory_order_acquire)) {
            BlockBench::Timer::sleep(1.0);
            {
                std::lock_guard guard(proxiesTimeoutMutex);
                auto currentTime = BlockBench::Timer::timeNow();
                for (auto& pair: proxiesTimeout) {   // check timeout func
                    if (pair.second.isValid && BlockBench::Timer::span(pair.second.lastHeartbeat, currentTime) > 3) {
                        // pair.second.latestEpoch is valid, broadcast for v+1
                        broadcastMaliciousViewChange(pair.first, pair.second.latestEpoch + 1);
                    }
                }   // the server itself can not be malicious
            }
            // send heart beat package
            comm::TransactionsWithProof proofSet;
            proofSet.set_ip(configPtr->getLocalBlockServerIP());
            proofSet.set_type(comm::TransactionsWithProof_Type_HEARTBEAT);
            const auto rawProofSet = proofSet.SerializeAsString();
            local_leader->sendEpochFinishSignal(rawProofSet);
        }
    }

    void receiver_from_multi_raft_instance() {
        while(start.load(std::memory_order_acquire)) {
            std::string epoch_request_raw;
            multi_raft_receiver.pop(epoch_request_raw);
            comm::TransactionsWithProof proofSet;
            if(!proofSet.ParseFromString(epoch_request_raw))
                continue;
            recvProofSet(proofSet);
        }
    }

    // the observerId thinks targetId is malicious since epoch
    void obtainMalicious(const std::string& observerId, const std::string& targetId, epoch_size_t epoch) {
        // epoch is invalid
        // make sure we have locked proxiesTimeoutMutex
        auto& targetCB = proxiesTimeout[targetId];
        if (!targetCB.isValid) {
            return;
        }
        auto& voters = targetCB.epochVoters[epoch];
        voters.insert(observerId);
        if (static_cast<int>(voters.size()) >= (totalServerCount - 1) / 3 +1) {  // >f+1, total 3f+1
            // invalid proxy
            maliciousTarget(targetId, epoch);
        }
    }

    // we have processed txn received from that client proxy, wakeup coordinator
    void confirmedClientProxy(const std::string& observerId, epoch_size_t epoch) {
        epochContinueCVWrapper.notify_all([&]{
            DLOG(INFO) << observerId << " approve for epoch=" << epoch;
            epochApproveMap[epoch].insert(observerId);
            if((int)epochApproveMap[epoch].size() == totalServerCount) {
                LOG(INFO) << "consensus of epoch=" << epoch << " has reach, all vote yes.";
            }
        });
    }

    // set a proxy server invalid
    void maliciousTarget(const std::string& targetId, epoch_size_t epoch) {
        // make sure we have locked proxiesTimeoutMutex
        LOG(WARNING) << "Marking target " << targetId << " malicious since epoch: " << epoch;
        auto& targetCB = proxiesTimeout[targetId];
        targetCB.isValid = false;
        targetCB.firstInvalidEpoch = epoch;
        maliciousBroadcastHandle(targetId, epoch);
        // set that server approve every epoch after that
        epochContinueCVWrapper.notify_all([&]{
            epochStaleBit[epoch] = true;
            for (auto i=epoch; i<MAX_EPOCH; i++) {
                epochApproveMap[i].insert(targetId);
            }
        });
        // TODO: remove the server from the cluster
    }

private:
    static ConsensusControlBlock* instance;
    std::atomic<bool> start = false;
    int totalServerCount = 0;
    // to increase epoch, for live ness
    CVWrapper epochContinueCVWrapper;
    std::vector<std::set<std::string>> epochApproveMap;
    std::vector<bool> epochStaleBit;
    // for safety
    std::function<void(const std::string&, epoch_size_t)> maliciousBroadcastHandle;
    // timeout == 0 means the server is down (malicious), else means the last heartbeat
    std::mutex proxiesTimeoutMutex;
    std::unordered_map<std::string, ProxyBlock> proxiesTimeout;
    std::thread spinCheckThread;
    std::thread raftReceiverThread;

    // for multi raft protocols
    BatchAtomicBroadcaster* local_leader;
    std::vector<std::unique_ptr<BatchAtomicBroadcaster>> all_raft_instance;
    BlockingMPMCQueue<std::string> multi_raft_receiver;
    std::unique_ptr<brpc::Server> server;
};


class InvalidClientProxyException : public std::exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "the client proxy has already invalid by another process.";
    }
};

class MaliciousClientProxyException : public std::exception {
public:
    [[nodiscard]] const char* what() const noexcept override {
        return "the client proxy is malicious, must be disabled.";
    }
};

#endif //NEU_BLOCKCHAIN_CONSENSUS_CONTROL_BLOCK_H
