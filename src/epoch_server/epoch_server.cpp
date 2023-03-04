//
// Created by ada on 1/19/21.
//

#include "epoch_server/epoch_server.h"
#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"
#include <butil/at_exit.h>
#include <brpc/server.h>
#include "epoch_server/eov_consensus_manager.h"
namespace braft {
    DECLARE_bool(raft_enable_append_entries_cache);
}
/*
 * Usage:
 *  -t: epoch duration, integer (1s = 1000ms)
 *  default epoch duration: 1.0s
 */
int main(int argc, char *argv[]) {
    braft::FLAGS_raft_enable_append_entries_cache = true;
    butil::AtExitManager exit_manager;
    CryptoHelper::initCryptoSign(CryptoHelper::ServerType::epochServer);
    double duration = 0.05;
    bool local_test = false;
    size_t block_size = 1000;
    size_t batch_size = 1000;
    size_t raft_port = 8100;
    size_t request_receiver_port = 9002;
    size_t block_broadcaster_port = 9003;
    for (int i = 0; i < argc; i++) {
        if (argv[i] == std::string("--block_size")) {
            CHECK(argc > i + 1);
            block_size = static_cast<size_t>(strtol(argv[i + 1], nullptr, 10));
            LOG(INFO) << "Set block size to " << block_size << " txs.";
        }
        if (argv[i] == std::string("--raft_port")) {
            CHECK(argc > i + 1);
            raft_port = static_cast<size_t>(strtol(argv[i + 1], nullptr, 10));
            LOG(INFO) << "Set raft port to " << raft_port << " .";
            if (raft_port == 8100) {
                request_receiver_port = 9002;
                block_broadcaster_port = 9003;
            }
            if (raft_port == 8101) {
                request_receiver_port = 9004;
                block_broadcaster_port = 9005;
            }
            if (raft_port == 8102) {
                request_receiver_port = 9006;
                block_broadcaster_port = 9007;
            }
        }
        if (argv[i] == std::string("--receiver_port")) {
            CHECK(argc > i + 1);
            request_receiver_port = static_cast<size_t>(strtol(argv[i + 1], nullptr, 10));
            LOG(INFO) << "Set request_receiver_port to " << request_receiver_port << " .";
        }
        if (argv[i] == std::string("--broadcaster_port")) {
            CHECK(argc > i + 1);
            block_broadcaster_port = static_cast<size_t>(strtol(argv[i + 1], nullptr, 10));
            LOG(INFO) << "Set block_broadcaster_port to " << block_broadcaster_port << " .";
        }
        if (argv[i] == std::string("--batch_size")) {
            CHECK(argc > i + 1);
            batch_size = static_cast<size_t>(strtol(argv[i + 1], nullptr, 10));
            LOG(INFO) << "Set batch_size to " << batch_size << " .";
        }
        if (argv[i] == std::string("--duration")) {
            CHECK(argc > i + 1);
            duration = static_cast<double>(strtol(argv[i + 1], nullptr, 10)) / 1000;
            LOG(INFO) << "Set duration to " << duration << " s.";
        }
        if (argv[i] == std::string("--local")) {
            local_test = true;
        }
    }
    // construct init confing
    std::string initial_conf;   // 127.0.1.1:8100:0,127.0.1.1:8101:0,127.0.1.1:8102:0
    //initial_conf.clear();
    for (const auto& ip : YAMLConfig::getInstance()->getEpochServerIPs()) {
        initial_conf += ip + ":8100:0,";
    }
    initial_conf.pop_back();
    consensus_param param;
    param.initial_conf = initial_conf;
    param.raft_ip = YAMLConfig::getInstance()->getLocalBlockServerIP();
    if (local_test)
        param.initial_conf = "127.0.0.1:8100:0";
    param.block_size = block_size;
    param.batch_size = batch_size;
    param.duration = duration;
    param.raft_port = raft_port;
    param.request_receiver_port = request_receiver_port;
    param.block_broadcaster_port = block_broadcaster_port;
    EOVConsensusManager manager(param);
    while (!brpc::IsAskedToQuit()) {
        sleep(1);
        manager.print_status();
    }
}