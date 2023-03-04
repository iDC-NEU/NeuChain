//
// Created by peng on 2021/4/29.
//

#include "user/block_bench/aria_small_bank_db.h"
#include "common/payload/ycsb_payload.h"
#include "common/base64.h"
#include "glog/logging.h"

BlockBench::AriaSmallBankDB::AriaSmallBankDB() {
    DLOG(INFO) << "an AriaSmallBankDB started.";
}

void BlockBench::AriaSmallBankDB::amalgamate(uint32_t acc1, uint32_t acc2) {
    Utils::Request request;
    request.funcName = "amalgamate";
    request.reads = { std::to_string(acc1), std::to_string(acc2) };
    sendInvokeRequest(request);
}

void BlockBench::AriaSmallBankDB::getBalance(uint32_t acc) {
    Utils::Request request;
    request.funcName = "getBalance";
    request.reads = { std::to_string(acc) };
    sendInvokeRequest(request);
}

void BlockBench::AriaSmallBankDB::updateBalance(uint32_t acc, uint32_t amount) {
    Utils::Request request;
    request.funcName = "updateBalance";
    request.reads = { std::to_string(acc), std::to_string(amount) };
    sendInvokeRequest(request);
}

void BlockBench::AriaSmallBankDB::updateSaving(uint32_t acc, uint32_t amount) {
    Utils::Request request;
    request.funcName = "updateSaving";
    request.reads = { std::to_string(acc), std::to_string(amount) };
    sendInvokeRequest(request);
}

void BlockBench::AriaSmallBankDB::sendPayment(uint32_t acc1, uint32_t acc2, unsigned int amount) {
    Utils::Request request;
    request.funcName = "sendPayment";
    request.reads = { std::to_string(acc1), std::to_string(acc2), std::to_string(amount) };
    sendInvokeRequest(request);
}

void BlockBench::AriaSmallBankDB::writeCheck(uint32_t acc, uint32_t amount) {
    Utils::Request request;
    request.funcName = "writeCheck";
    request.reads = { std::to_string(acc), std::to_string(amount) };
    sendInvokeRequest(request);
}
