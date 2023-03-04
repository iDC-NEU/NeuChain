//
// Created by peng on 2021/9/10.
//

#ifndef NEU_BLOCKCHAIN_OE_CONSENSUS_MANAGER_H
#define NEU_BLOCKCHAIN_OE_CONSENSUS_MANAGER_H

#include "raft/consensus_manager.h"

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

private:
    int block_size;
    double duration;
    // port config
    int block_broadcaster_port;
    int request_receiver_port;
};


#endif //NEU_BLOCKCHAIN_OE_CONSENSUS_MANAGER_H
