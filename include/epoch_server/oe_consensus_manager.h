//
// Created by peng on 2021/9/10.
//

#ifndef NEU_BLOCKCHAIN_OE_CONSENSUS_MANAGER_H
#define NEU_BLOCKCHAIN_OE_CONSENSUS_MANAGER_H

#include "raft/consensus_manager.h"
#include "common/aria_types.h"
#include <cassert>
#include <common/zmq/zmq_server.h>
#include "common/concurrent_queue/light_weight_semaphore.hpp"

struct consensus_param {
    std::string initial_conf;
    std::string raft_ip;
    int block_size;
    int batch_size;
    double duration;
    int raft_port;
    int block_broadcaster_port;
    int request_receiver_port;
};

class OEConsensusManager : public ConsensusManager<OEConsensusManager> {
public:
    explicit OEConsensusManager(const consensus_param& param);
    // CRTP: these method is called by base class
    // create a thread for receiving message from raft cluster
    void receiver_from_raft();
    // create a thread to send message to raft cluster
    void receiver_from_user();

    std::function<void(const std::string& tx)> pushTxHandle;
    std::function<void()> increaseEpochHandle;
    std::function<epoch_size_t()> getLatestStoredEpoch;

    void recv_barrier() {
        auto serverCount = YAMLConfig::getInstance()->getBlockServerCount();
        ZMQServer txReceiver("0.0.0.0", "9989", zmq::socket_type::sub);
        std::vector<int> epochFinishCount;
        epochFinishCount.resize(10000);
        for (int i=1;i < (int)epochFinishCount.size(); i++) {
            while(epochFinishCount[i] <= 1) {
                std::string epoch;
                txReceiver.getRequest(epoch);
                DLOG(INFO) <<"A Peer finished epoch = " << epoch << ".";
                int actual_epoch = (int) std::stol(epoch);
                if (!actual_epoch)  // msg is wrong
                    continue;
                epochFinishCount[actual_epoch]++;
            }
            LOG(INFO) << "proceed to epoch =" << i+1;
            sema.signal();
        }
    }

private:
    std::unique_ptr<std::thread> recvThread;
    moodycamel::LightweightSemaphore sema;
    int block_size;
    double duration;
    // port config
    int block_broadcaster_port;
    int request_receiver_port;
};


#endif //NEU_BLOCKCHAIN_OE_CONSENSUS_MANAGER_H
