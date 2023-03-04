//
// Created by peng on 2021/1/15.
//

#include "block_server/server.h"
#include "block_server/comm/client_proxy/comm_controller.h"

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
    // initDatabase();
    // return 0;
    // disable pipeline in braft is better in the same az
    braft::FLAGS_raft_enable_append_entries_cache = true;
    DDChainServer server;
    server.initServer();
    server.startServer();
    server.joinServer();
    return 0;
}