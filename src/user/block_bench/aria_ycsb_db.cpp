//
// Created by peng on 2021/5/6.
//

#include "user/block_bench/aria_ycsb_db.h"
#include "common/payload/ycsb_payload.h"
#include "common/base64.h"
#include "tpc-c.pb.h"
#include "glog/logging.h"

BlockBench::AriaYCSB_DB::AriaYCSB_DB() {
    DLOG(INFO) << "an AriaYCSB_DB started.";
}

// param result not used
// if fields.size()==0, means select *
int BlockBench::AriaYCSB_DB::Read(const std::string &table, const std::string &key,
                                  const std::vector<std::string> *fields, std::vector<KVPair> &result) {
    Utils::Request request;
    request.funcName = "read";
    request.tableName = table;
    YCSB_FOR_BLOCK_BENCH payload;
    for (const auto& pair: *fields) {
        auto* value = payload.add_values();
        value->set_key(pair);
        value->set_value("_");
    }
    request.reads = {key, payload.SerializeAsString()};
    sendInvokeRequest(request);
    return true;
}

int BlockBench::AriaYCSB_DB::Update(const std::string &table, const std::string &key, std::vector<KVPair> &values) {
    Utils::Request request;
    request.funcName = "write";
    request.tableName = table;
    YCSB_FOR_BLOCK_BENCH payload;
    for (const auto& pair: values) {
        auto* value = payload.add_values();
        value->set_key(pair.first);
        value->set_value(pair.second);
    }
    request.reads = {key, payload.SerializeAsString()};
    sendInvokeRequest(request);
    return true;
}

int BlockBench::AriaYCSB_DB::Scan(const std::string &table, const std::string &key, int record_count,
                                  const std::vector<std::string> *fields, std::vector<std::vector<KVPair>> &result) {
    LOG(WARNING) << "AriaYCSB_DB does not support Scan op.";
    return false;
}

int BlockBench::AriaYCSB_DB::Insert(const std::string &table, const std::string &key, std::vector<KVPair> &values) {
    return Update(table, key, values);
}

int BlockBench::AriaYCSB_DB::Delete(const std::string &table, const std::string &key) {
    Utils::Request request;
    request.funcName = "del";
    request.tableName = table;
    request.reads = {key};
    sendInvokeRequest(request);
    return true;
}
