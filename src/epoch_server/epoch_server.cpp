//
// Created by ada on 1/19/21.
//

#include "epoch_server/epoch_server.h"
#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"
#include <butil/at_exit.h>
#include <brpc/server.h>
#include "epoch_server/ev_consensus_manager.h"

namespace braft {
    DECLARE_bool(raft_enable_append_entries_cache);
}

/*
 * Usage:
 *  -t: epoch duration, integer (1s = 1000ms)
 *  default epoch duration: 1.0s
 */
int main(int argc, char *argv[]) {
    braft::FLAGS_raft_enable_append_entries_cache = false;
    butil::AtExitManager exit_manager;
    CryptoHelper::initCryptoSign(CryptoHelper::ServerType::epochServer);
    double duration = 0.05;
    int raft_port = 8100;
    int batch_size = 1;
    int request_receiver_port = 6002;
    for (int i = 0; i < argc; i++) {
        if (argv[i] == std::string("--duration")) {
            CHECK(argc > i + 1);
            duration = static_cast<double>(strtol(argv[i + 1], nullptr, 10)) / 1000;
            LOG(INFO) << "Set duration to " << duration << " txs.";
        }
        if (argv[i] == std::string("--raft_port")) {
            CHECK(argc > i + 1);
            raft_port = static_cast<int>(strtol(argv[i + 1], nullptr, 10));
            LOG(INFO) << "Set raft port to " << raft_port << " .";
            if (raft_port == 8100) {
                request_receiver_port = 6002;
            }
            if (raft_port == 8101) {
                request_receiver_port = 6003;
            }
            if (raft_port == 8102) {
                request_receiver_port = 6004;
            }
        }
    }
    auto param = EVConsensusManager::GenerateEpochServerConsensusParam(YAMLConfig::getInstance()->getLocalBlockServerIP(), raft_port, batch_size, duration, request_receiver_port);
    brpc::Server server;
    EVConsensusManager manager(param, &server);
    while (!brpc::IsAskedToQuit()) {
        manager.print_status();
        sleep(1);
    }
    server.Stop(0);
    server.Join();
}
