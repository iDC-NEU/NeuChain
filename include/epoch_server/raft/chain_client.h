//
// Created by peng on 2021/9/9.
//

#ifndef NEUBLOCKCHAIN_CHAIN_CLIENT_H
#define NEUBLOCKCHAIN_CHAIN_CLIENT_H

#include <braft/raft.h>
#include "common/compile_config.h"
#include "chain.pb.h"

namespace raft {
    class ChainClient {
    public:
        explicit ChainClient(const char* group_name);

        ~ChainClient();

        bool init_config(const std::string &initial_conf);

        // if return false, user must re-push it
        bool
        push_user_request(const braft::PeerId &leader, const raft::PushRequest *request, raft::ChainResponse *response);

        // if response is false, leader may changed
        bool process_response(const ChainResponse *response);

        // get the current leader from client buffer
        static bool get_leader_from_route_table(const char* group, braft::PeerId &leader);

        // update leader in client buffer
        static bool update_leader_in_route_table(const char* group, const braft::PeerId &leader);

        void print_status();

    private:
        bvar::LatencyRecorder latency_recorder;
        const std::string group_name;
        static const int timeout_ms = RAFT_TIMEOUT_MS;    // Start election in such milliseconds if disconnect with the leader
    };
}

#endif //NEUBLOCKCHAIN_CHAIN_CLIENT_H
