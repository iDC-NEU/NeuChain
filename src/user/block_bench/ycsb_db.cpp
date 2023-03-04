//
// Created by peng on 2021/5/6.
//

#include "common/timer.h"
#include "user/block_bench/ycsb_db.h"
#include "user/block_bench/aria_ycsb_db.h"
#include "user/block_bench/ycsb_workload.h"

std::unique_ptr<BlockBench::YCSB_DB>
BlockBench::YCSB_DB::CreateDB(const std::string &type, const std::vector<std::string> &args) {
    if(type == "aria") {
        return std::make_unique<AriaYCSB_DB>();
    }
    return nullptr;
}

void BlockBench::YCSB_DB::ClientThread(std::unique_ptr<YCSB_DB> db, uint32_t benchmarkTime, uint32_t txRate) {
    pthread_setname_np(pthread_self(), "ycsb_client");
    // this workload is used to generate different workload type from config
    YCSBWorkload workload;
    // we use these five func to control the chaincode.
    auto insertFunc = [&]() -> bool {
        std::vector<KVPair> pairs;
        workload.generateRandomValues(pairs);
        return db->Insert(workload.getNextTableName(), workload.nextSequenceKey(), pairs);
    };

    auto readFunc = [&] () -> bool {
        std::vector<KVPair> result;
        std::vector<std::string> fields;
        if (!workload.getReadAllFields()) {
            fields.push_back(workload.nextFieldName());
        }
        // default, read all fields
        return db->Read(workload.getNextTableName(), workload.nextTransactionKey(), &fields, result);
    };

    auto updateFunc = [&] () -> bool {
        std::vector<KVPair> values;
        if (!workload.getWriteAllFields()) {
            workload.generateRandomUpdate(values);
        } else {
            // write each fields
            workload.generateRandomValues(values);
        }
        return db->Update(workload.getNextTableName(), workload.nextTransactionKey(), values);
    };

    auto scanFunc = [&] () -> bool {
        std::vector<std::vector<KVPair>> result;
        std::vector<std::string> fields;
        if (!workload.getReadAllFields()) {
            fields.push_back(workload.nextFieldName());
        }
        return db->Scan(workload.getNextTableName(), workload.nextTransactionKey(), workload.getNextScanLength(), &fields, result);
    };

    auto readModifyWriteFunc = [&] () -> bool {
        const std::string &table = workload.getNextTableName();
        const std::string &key = workload.nextTransactionKey();
        std::vector<KVPair> result;
        std::vector<std::string> fields;

        if (!workload.getReadAllFields()) {
            fields.push_back(workload.nextFieldName());
        }
        if(!db->Read(table, key, &fields, result)) {
            LOG(ERROR) << "read failed!";
            CHECK(false);
        }

        std::vector<KVPair> values;
        if (!workload.getWriteAllFields()) {
            workload.generateRandomUpdate(values);
        } else {
            // write each fields
            workload.generateRandomValues(values);
        }
        return db->Update(workload.getNextTableName(), workload.nextTransactionKey(), values);
    };

    DBUserBase::RateGenerator<std::function<void()>>(benchmarkTime, txRate, [&]() {
        auto op = workload.nextOperation();
        switch (op) {
            case Operation::READ:
                readFunc();
                break;
            case Operation::READ_MODIFY_WRITE:
                readModifyWriteFunc();
                break;
            case Operation::SCAN:
                scanFunc();
                break;
            case Operation::UPDATE:
                updateFunc();
                break;
            case Operation::INSERT:
                insertFunc();
                break;
            default:
                LOG(ERROR) << "wrong op number.";
        }
    });
}
