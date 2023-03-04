//
// Created by peng on 2021/4/24.
//

#ifndef NEUBLOCKCHAIN_YCSB_PAYLOAD_H
#define NEUBLOCKCHAIN_YCSB_PAYLOAD_H

#include <string>
#include <vector>

class Transaction;
class YCSB_PAYLOAD;
class TransactionPayload;

namespace comm {
    class QueryResult;
    class UserRequest;
    class UserQueryRequest;
}

namespace Utils {
    struct Request {
        // if empty = test_table
        std::string tableName;
        // if empty = use config file default
        std::string funcName;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
    };
    // payload (transaction payload) = ycsb_payload + header (func name) + nonce (associate with tid)
    // request (user request) = payload + digest

    // input: tid, read and write keys
    // output: serialized ycsb_payload
    std::string getYCSBPayloadRaw(const Request& request);
    YCSB_PAYLOAD getYCSBPayload(const Request& request);

    // input: seed
    // table name and cc_func will be get from config file
    // output: serialized payload, size: configure from file
    std::string getRandomTransactionPayloadRaw(size_t seed);
    TransactionPayload getRandomTransactionPayload(size_t seed);
    std::string getEmptyPayloadRaw(size_t seed);

    // input: seed, read and write keys
    // table name will be get from config file
    // output: serialized payload
    std::string getTransactionPayloadRaw(size_t seed, const Request& request);
    TransactionPayload getTransactionPayload(size_t seed, const Request& request);

    // input: serialized payload
    // input: signed serialized payload
    // this should be invoke by a user
    std::string getUserInvokeRequestRaw(const std::string& payloadRaw);
    comm::UserRequest getUserInvokeRequest(const std::string& payloadRaw);
    comm::UserRequest getUserInvokeRequest(const std::string& payloadRaw, const std::string& ip);
    std::string getUserQueryRequestRaw(const std::string& queryType, const std::string& payloadRaw);
    comm::UserQueryRequest getUserQueryRequest(const std::string& queryType, const std::string& payloadRaw);

    // input: query result raw
    // output: none
    void printQueryResult(const std::string& queryResultRaw);
    void printQueryResult(const comm::QueryResult &results);
}

#endif //NEUBLOCKCHAIN_YCSB_PAYLOAD_H
