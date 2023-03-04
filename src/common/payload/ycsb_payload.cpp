//
// Created by peng on 2021/4/24.
//

#include "common/payload/ycsb_payload.h"
#include "common/yaml_config.h"
#include "common/random.h"
#include "common/msp/crypto_helper.h"

#include "transaction.pb.h"
#include "tpc-c.pb.h"
#include "comm.pb.h"

#include "glog/logging.h"

std::string Utils::getYCSBPayloadRaw(const Request& request) {
    return getYCSBPayload(request).SerializeAsString();
}

YCSB_PAYLOAD Utils::getYCSBPayload(const Request& request) {
    YCSB_PAYLOAD ycsbPayload;
    // 1. set table name
    if(request.tableName.empty()) {
        ycsbPayload.set_table("test_table");
    } else {
        ycsbPayload.set_table(request.tableName);
    }
    // 2. set rw key
    for (const auto &read: request.reads) {
        *ycsbPayload.add_reads() = read;
    }
    for (const auto &write: request.writes) {
        *ycsbPayload.add_update() = write;
    }
    return ycsbPayload;
}

std::string Utils::getRandomTransactionPayloadRaw(size_t seed) {
    return getRandomTransactionPayload(seed).SerializeAsString();
}

TransactionPayload Utils::getRandomTransactionPayload(size_t seed) {
    static Random randomHelper;
    static const auto ccConfigPtr = YAMLConfig::getInstance()->getHeartBeatConfig();
    static const auto ccConfig = *ccConfigPtr;
    TransactionPayload payload;
    payload.set_header(ccConfig["func_name"].as<std::string>());
    // construct request
    Request request;
    if(!ccConfig["table_name"].IsDefined()) {
        LOG(WARNING) << "missing table_name node, use default";
    } else {
        request.tableName = ccConfig["table_name"].as<std::string>();
    }
    for (size_t i = 0; i < ccConfig["reads"].as<size_t>(); i++) {
        request.reads.push_back(std::to_string(randomHelper.uniform_dist(0, 200000)));
    }
    for (size_t i = 0; i < ccConfig["writes"].as<size_t>(); i++) {
        request.writes.push_back(std::to_string(randomHelper.uniform_dist(0, 200000)));
    }
    //set message
    payload.set_payload(getYCSBPayloadRaw(request));
    payload.set_nonce(random() + (seed << 32));
    return payload;
}

std::string Utils::getEmptyPayloadRaw(size_t seed) {
    static Random randomHelper;
    TransactionPayload payload;
    payload.set_header("empty");
    // construct request
    Request request;
    request.tableName = "test_table";
    //set message
    payload.set_payload(getYCSBPayloadRaw(request));
    payload.set_nonce(random() + (seed << 32));
    return payload.SerializeAsString();
}

std::string Utils::getTransactionPayloadRaw(size_t seed, const Request& request) {
    return getTransactionPayload(seed, request).SerializeAsString();
}

TransactionPayload Utils::getTransactionPayload(size_t seed, const Request& request) {
    static const auto ccConfigPtr = YAMLConfig::getInstance()->getHeartBeatConfig();
    static const auto ccConfig = *ccConfigPtr;
    TransactionPayload payload;
    // 1. set func name
    if (request.funcName.empty()) {
        payload.set_header(ccConfig["func_name"].as<std::string>());
    } else {
        payload.set_header(request.funcName);
    }
    // 2. set payload and nonce
    payload.set_payload(getYCSBPayloadRaw(request));
    payload.set_nonce(random() + (seed << 32));
    return payload;
}

std::string Utils::getUserInvokeRequestRaw(const std::string &payloadRaw) {
    return getUserInvokeRequest(payloadRaw).SerializeAsString();
}

comm::UserRequest Utils::getUserInvokeRequest(const std::string& payloadRaw) {
    // init crypto
    static const CryptoSign* userSignHelper = CryptoHelper::getLocalUserSigner();
    // calculate digest
    std::string digest;
    userSignHelper->rsaEncrypt(payloadRaw, &digest);
    // sign payload
    comm::UserRequest request;
    request.set_payload(payloadRaw);
    request.set_digest(digest);
    return request;
}

comm::UserRequest Utils::getUserInvokeRequest(const std::string& payloadRaw, const std::string& ip) {
    // init crypto
    const CryptoSign* userSignHelper = CryptoHelper::getUserSigner(ip);
    // calculate digest
    std::string digest;
    userSignHelper->rsaEncrypt(payloadRaw, &digest);
    // sign payload
    comm::UserRequest request;
    request.set_payload(payloadRaw);
    request.set_digest(digest);
    return request;
}

std::string Utils::getUserQueryRequestRaw(const std::string& queryType, const std::string& payloadRaw) {
    return getUserQueryRequest(queryType, payloadRaw).SerializeAsString();
}

comm::UserQueryRequest Utils::getUserQueryRequest(const std::string& queryType, const std::string& payloadRaw) {
    // init crypto
    static const CryptoSign* userSignHelper = CryptoHelper::getLocalUserSigner();
    // calculate digest
    std::string digest;
    userSignHelper->rsaEncrypt(payloadRaw, &digest);
    // sign payload
    comm::UserQueryRequest request;
    request.set_type(queryType);
    request.set_payload(payloadRaw);
    request.set_digest(digest);
    return request;
}

void Utils::printQueryResult(const std::string &queryResultRaw) {
    comm::QueryResult results;
    results.ParseFromString(queryResultRaw);
    for (const auto &result: results.reads()) {
        LOG(INFO) << "key: " << result.key() << ", value: " << result.value();
    }
}

void Utils::printQueryResult(const comm::QueryResult &results) {
    for (const auto &result: results.reads()) {
        LOG(INFO) << "key: " << result.key() << ", value: " << result.value();
    }
}
