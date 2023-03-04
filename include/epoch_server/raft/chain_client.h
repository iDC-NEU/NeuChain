//
// Created by peng on 2021/9/9.
//

#ifndef NEUBLOCKCHAIN_CHAIN_CLIENT_H
#define NEUBLOCKCHAIN_CHAIN_CLIENT_H

#include <braft/raft.h>
#include "chain.pb.h"

namespace raft {
    class ChainClient {
    public:
        ChainClient();
        ~ChainClient();

        bool init_config(const std::string& initial_conf);
        // if return false, user must re-push it
        bool push_user_request(const braft::PeerId &leader, const raft::PushRequest *request, raft::ChainResponse *response);

        // if response is false, leader may changed
        bool process_response(const ChainResponse* response);

        // get the current leader from client buffer
        static bool get_leader_from_route_table(braft::PeerId& leader);
        // update leader in client buffer
        static bool update_leader_in_route_table(const braft::PeerId& leader);

        void print_status();

    private:
        bvar::LatencyRecorder latency_recorder;
        static const char* group_name;
        static const int timeout_ms = 5000;    // Start election in such milliseconds if disconnect with the leader
    };
}

#endif //NEUBLOCKCHAIN_CHAIN_CLIENT_H
