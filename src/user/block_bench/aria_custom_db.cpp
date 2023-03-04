//
// Created by peng on 2021/6/14.
//

#include "user/block_bench/aria_custom_db.h"
#include "common/payload/ycsb_payload.h"

BlockBench::AriaCustomDB::AriaCustomDB() = default;

void BlockBench::AriaCustomDB::timeConsuming() {
    Utils::Request request;
    request.funcName = "time";
    request.reads = { "" };
    sendInvokeRequest(request);
}

void BlockBench::AriaCustomDB::calculateConsuming() {
    Utils::Request request;
    request.funcName = "calculate";
    request.reads = { "" };
    sendInvokeRequest(request);
}

void BlockBench::AriaCustomDB::normalTransaction() {
    Utils::Request request;
    request.funcName = "normal";
    request.reads = { "" };
    sendInvokeRequest(request);
}
