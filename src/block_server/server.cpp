//
// Created by ada on 1/19/21.
//

#include "block_server/server.h"
#include "block_server/comm/client_proxy/comm_controller.h"
#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"
#include <butil/at_exit.h>
#include <brpc/server.h>
#include "epoch_server/oe_consensus_manager.h"

#include <user/block_bench/ycsb_workload.h>
#include "block_server/database/db_storage.h"
#include <tpc-c.pb.h>
#include <common/thread_pool.h>

namespace braft {
    DECLARE_bool(raft_enable_append_entries_cache);
}

void initDatabase() {
    //ThreadPool pool((int)sysconf(_SC_NPROCESSORS_ONLN));
    auto* storage = VersionedDB::getDBInstance()->getStorage();
    const std::string savingTab = "saving";
    const std::string checkingTab = "checking";
    const std::string BALANCE = "100000";
    LOG(INFO) << "init smallbank.";
    for(int account=1; account<=100000; account++) {
        storage->updateWriteSet(savingTab + "_" + std::to_string(account), BALANCE, "small_bank");
        storage->updateWriteSet(checkingTab + "_" + std::to_string(account), BALANCE, "small_bank");
    }
    BlockBench::YCSBWorkload workload(63890);
    // we use these five func to control the chaincode.
    LOG(INFO) << "init ycsb.";
    for(int i=0; i<1000000; i++) {
        std::vector<BlockBench::YCSBWorkload::KVPair> pairs;
        workload.generateRandomValues(pairs);
        YCSB_FOR_BLOCK_BENCH payload;
        for (const auto& pair: pairs) {
            auto* value = payload.add_values();
            value->set_key(pair.first);
            value->set_value(pair.second);
        }
        auto key = workload.nextSequenceKey();
        auto tableName = workload.getNextTableName();
        storage->updateWriteSet(key, payload.SerializeAsString(), tableName);
    }
    LOG(INFO) << "db init complete.";
}

int main(int argc, char *argv[]) {
    // pipelining only increases latency
    braft::FLAGS_raft_enable_append_entries_cache = false;
    // initDatabase();
    butil::AtExitManager exit_manager;
    CommController* comm_controller;
    // block server
    DDChainServer server ([&](epoch_size_t startWithEpoch) {
        comm_controller = new CommController(startWithEpoch);
        return std::unique_ptr<CommController>(comm_controller);
    });
    server.initServer();
    server.startServer();
    // epoch server
    double duration = 0.1;
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
    for (const auto& ip : YAMLConfig::getInstance()->getBlockServerIPs()) {
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
    OEConsensusManager manager(param);
    manager.pushTxHandle = [comm_controller](const std::string& data) {
        comm_controller->blockReceiver(data);
    };
    manager.increaseEpochHandle =  [comm_controller]() {
        comm_controller->increaseEpoch();
    };
    manager.getLatestStoredEpoch = [&server]()->epoch_size_t {
        return server.generatorPtr->getLatestSavedEpoch();
    };
    while (!brpc::IsAskedToQuit()) {
        sleep(1);
        manager.print_status();
    }
    server.joinServer();
}
