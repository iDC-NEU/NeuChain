//
// Created by peng on 2021/5/6.
//

#include <thread>
#include "user/block_bench/db_user_base.h"
#include "common/payload/ycsb_payload.h"
#include "common/payload/mock_utils.h"
#include "common/zmq/zmq_client.h"
#include "common/yaml_config.h"
#include "common/timer.h"
#include "block_server/transaction/transaction.h"
#include "comm.pb.h"
#include "block.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"

BlockBench::DBUserBase::DBUserBase() :trCount(0) {
    auto* yamlConfig = YAMLConfig::getInstance();
    queryClient.emplace_back(yamlConfig->getLocalBlockServerIP(),
                             std::make_unique<ZMQClient>(yamlConfig->getLocalBlockServerIP(), "7003"));
    if(yamlConfig->sendToAllClientProxy()) {
        // load average
        for(const auto& ip: yamlConfig->getBlockServerIPs()) {
            invokeClient.emplace_back(ip, std::make_unique<ZMQClient>(ip, "5001", zmq::socket_type::pub));
            // queryClient.emplace_back(ip, std::make_unique<ZMQClient>(ip, "7003"));
        }
        sendInvokeRequest = [this](const Utils::Request &request) {
            size_t clientCount = invokeClient.size();
            size_t id = trCount % clientCount;
            std::string&& payloadRaw = Utils::getTransactionPayloadRaw(trCount++, request);
            auto&& invokeRequest = Utils::getUserInvokeRequest(payloadRaw, invokeClient[id].first);
            addPendingTransactionHandle(invokeRequest.digest());
            invokeClient[id].second->sendRequest(invokeRequest.SerializeAsString());
        };
    } else {
        // single user
        const auto& ip = yamlConfig->getLocalBlockServerIP();
        invokeClient.emplace_back(ip, std::make_unique<ZMQClient>(ip, "5001"));
        sendInvokeRequest = [this](const Utils::Request &request){
            std::string&& payloadRaw = Utils::getTransactionPayloadRaw(trCount++, request);
            auto&& invokeRequest = Utils::getUserInvokeRequest(payloadRaw, invokeClient[0].first);
            addPendingTransactionHandle(invokeRequest.digest());
            invokeClient[0].second->sendRequest(invokeRequest.SerializeAsString());
        };
    }
}

BlockBench::DBUserBase::~DBUserBase() = default;

std::queue<BlockBench::DBUserBase::pollTxType> BlockBench::DBUserBase::pollTx(epoch_size_t block_number) {
    CHECK(block_number > 0);
    auto id = 0;
    // auto id = block_number % queryClient.size();
    queryClient[id].second->sendRequest(Utils::getUserQueryRequestRaw("block_query", std::to_string(block_number)));
    std::string replyRaw;
    queryClient[id].second->getReply(replyRaw);
    Block::Block block;
    block.ParseFromString(replyRaw);
    auto* transaction = Utils::makeTransaction();
    std::map<tid_size_t, bool> committedTidList;
    std::queue<pollTxType> pollTxQueue;
    for(const auto& transactionRaw: block.data().data()) {
        transaction->deserializeResultFromString(transactionRaw);
        if(transaction->getTransactionResult() == TransactionResult::ABORT) {
            pollTxQueue.push(std::make_pair(transaction->getTransactionPayload()->digest(), false));
        } else {
            pollTxQueue.push(std::make_pair(transaction->getTransactionPayload()->digest(), true));
        }
    }
    return pollTxQueue;
}

size_t BlockBench::DBUserBase::getTipBlockNumber() {
    queryClient.front().second->sendRequest(Utils::getUserQueryRequestRaw("tip_query", ""));
    std::string replyRaw;
    queryClient.front().second->getReply(replyRaw);
    return strtol(replyRaw.data(), nullptr, 10);
}

void BlockBench::DBUserBase::StatusThread(std::unique_ptr<DBUserBase> db, std::atomic<bool> *finishSignal,
                                          double printInterval, int startBlockHeight) {
    pthread_setname_np(pthread_self(), "status_thread");
    const uint32_t confirmDuration = 0;
    const double retryDelay = 0;

    std::unordered_map<std::string, time_t> txMap;
    epoch_size_t curBlockHeight = startBlockHeight;

    volatile size_t txCountCommit = 0;
    volatile size_t txCountAbort = 0;
    volatile size_t pendingTxSize = 0;
    volatile size_t latency = 0;

    auto printFunc = [&] {
        pthread_setname_np(pthread_self(), "print_thread");
        BlockBench::Timer timer;
        size_t lastTimeCommit = 0;
        size_t lastTimeAbort = 0;
        size_t lastTimePending = 0;
        time_t lastTimeLatency = 0;
        auto lastPrintTime = BlockBench::Timer::timeNow();
        // 1 sec data
        bool firstSec = true;
        size_t firstSecCommit = 0;
        size_t firstSecAbort = 0;
        time_t firstSecLatency = 0;

        while(!*finishSignal){
            auto currentPrintTime = BlockBench::Timer::timeNow();
            double timeLeft = printInterval - static_cast<double>(currentPrintTime - lastPrintTime) / 1e9;
            BlockBench::Timer::sleep(timeLeft);
            lastPrintTime = BlockBench::Timer::timeNow();
            auto currentSecCommit = txCountCommit - lastTimeCommit;
            auto currentSecAbort = txCountAbort - lastTimeAbort;
            auto currentSecLatency = latency - lastTimeLatency;
            int64_t currentTimePending = pendingTxSize - lastTimePending;
            LOG(INFO) << "In the last " << timeLeft << "s, commit+abort_no_retry: " << currentSecCommit
                      << ", abort: " << currentSecAbort
                      << ", send rate: " << currentSecCommit + currentSecAbort + currentTimePending
                      << ", latency: " << static_cast<double>(currentSecLatency) / currentSecCommit / 1e9
                      << ", pendingTx: " << pendingTxSize;
            lastTimeCommit = txCountCommit;
            lastTimeAbort = txCountAbort;
            lastTimePending = pendingTxSize;
            lastTimeLatency = latency;
            if (firstSec) {
                firstSec = false;
                firstSecCommit = lastTimeCommit;
                firstSecAbort = lastTimeAbort;
                firstSecLatency = lastTimeLatency;
            }
        }
        LOG(INFO) << "Detail summary:";
        LOG(INFO) << "# Transaction throughput (KTPS): " << txCountCommit / timer.end() / 1e3;
        LOG(INFO) << "  Abort rate (KTPS): " << txCountAbort / timer.end() / 1e3;
        LOG(INFO) << "  Send rate (KTPS): " << static_cast<double>(txCountCommit + txCountAbort + pendingTxSize) / timer.end() / 1e3;
        LOG(INFO) << "Avg committed latency: " << latency / 1e9 / txCountCommit << " sec.";
        LOG(INFO) << "Detail without first 1s:";
        LOG(INFO) << "# Transaction throughput (KTPS): " << static_cast<double>(txCountCommit - firstSecCommit) / (timer.end() - 1) / 1e3;
        LOG(INFO) << "  Abort rate (KTPS): " << static_cast<double>(txCountAbort - firstSecAbort) / (timer.end() - 1) / 1e3;
        LOG(INFO) << "Avg committed latency: " << static_cast<double>(latency - firstSecLatency) / 1e9 / static_cast<double>(txCountCommit - firstSecCommit) << " sec.";
        LOG(INFO) << static_cast<double>(txCountCommit + txCountAbort + pendingTxSize) / timer.end() / 1e3 << "\t" <<
                  static_cast<double>(txCountCommit - firstSecCommit) / (timer.end() - 1) / 1e3 << "\t" <<
                  static_cast<double>(txCountAbort - firstSecAbort) / (timer.end() - 1) / 1e3 << "\t" <<
                  static_cast<double>(latency - firstSecLatency) / 1e9 / static_cast<double>(txCountCommit - firstSecCommit);
    };
    // wait for block server ready(generate first block)
    while(!db->getTipBlockNumber()) {
        Timer::sleep(retryDelay);
    }
    // start monitor thread
    std::thread printThread(printFunc);
    while(!*finishSignal){
        // get the tip first
        epoch_size_t tip = db->getTipBlockNumber();
        // we have already get the tip, so we can start timer here.
        if (!tip || curBlockHeight + confirmDuration > tip) { // fail to get tip or no new block
            Timer::sleep(retryDelay);
            continue;
        }
        time_t tipStartTime = Timer::timeNow();
        // update current tx map, not in the loop
        db->updateTransactionMapHandle(txMap);
        while (curBlockHeight + confirmDuration <= tip) {
            auto&& txs = db->pollTx(curBlockHeight);
            DLOG(INFO) << "polled curBlockHeight: " << curBlockHeight << ", txs size: " << txs.size();
            curBlockHeight++;
            pendingTxSize = txMap.size();
            while(!txs.empty()) {
                const auto& tx = txs.front();
                if(txMap.count(tx.first)) { // in the map, emit by this user.
                    if (tx.second) {     // true, commit
                        txCountCommit++;
                        latency += (tipStartTime - txMap[tx.first]);
                    } else {    // false, abort
                        txCountAbort++;
                    }
                    // pop this when we finished using tx, otherwise may cause mem leak!
                    txMap.erase(tx.first);
                } else {    // unknown, not in the map
                    DLOG(INFO) << "missing tx in tx map, please check if all txs is generated in a user.";
                }
                txs.pop();
            }
        }
    }
    printThread.join();
}
