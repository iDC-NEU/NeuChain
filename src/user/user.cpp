//
// Created by ada on 1/19/21.
//

#include "common/msp/crypto_helper.h"
#include "common/zmq/zmq_client.h"
#include "common/payload/ycsb_payload.h"
#include "common/timer.h"
#include "common/base64.h"
#include "common/yaml_config.h"
#include "common/concurrent_queue/concurrent_queue.hpp"

#include "user/block_bench/small_bank_db.h"
#include "user/block_bench/ycsb_db.h"
#include "user/block_bench/custom_db.h"

#include "glog/logging.h"

#include <thread>
#include <atomic>
#include <random>

void SmallBankTestcase(uint32_t threadNum = 10, uint32_t txRate = 3000, size_t benchmarkTime = 30) {
    using trElemType = std::pair<std::string, time_t>;
    moodycamel::ConcurrentQueue<trElemType> concurrentQueue;
    auto addTransaction = [&](const std::string& trSignature) {
        concurrentQueue.enqueue(std::make_pair(trSignature ,BlockBench::Timer::timeNow()));
    };
    auto updateTransactionMap = [&](std::unordered_map<std::string, time_t>& map) {
        trElemType t;
        while(concurrentQueue.try_dequeue(t)) {
            DCHECK(map.count(t.first) == 0);
            map.insert(t);
        }
    };
    // workers
    std::vector<std::thread> threads;
    threads.reserve(threadNum);
    LOG(INFO) << "starting workers for small bank benchmark, warming up..";
    for (size_t i = 0; i < threadNum; ++i) {
        auto pdb = BlockBench::SmallBankDB::CreateDB("aria", {});
        pdb->addPendingTransactionHandle = addTransaction;
        threads.emplace_back(&BlockBench::SmallBankDB::ClientThread, std::move(pdb), benchmarkTime, txRate);
        //BlockBench::Timer::sleep(1.0);
    }
    LOG(INFO) << "all worker started.";
    // monitors
    std::atomic<bool> finishSignal = false;
    // block height start from 1.
    auto pdb = BlockBench::SmallBankDB::CreateDB("aria", {});
    pdb->updateTransactionMapHandle = updateTransactionMap;
    std::thread monitor(&BlockBench::SmallBankDB::StatusThread, std::move(pdb), &finishSignal, 1.0, 1);
    for (auto& t : threads) {
        t.join();
    }
    LOG(INFO) << "all ClientThreads exited.";
    finishSignal = true;
    monitor.join();
}

void CustomTestcase(uint32_t threadNum = 10, uint32_t txRate = 3000, size_t benchmarkTime = 30) {
    using trElemType = std::pair<std::string, time_t>;
    moodycamel::ConcurrentQueue<trElemType> concurrentQueue;
    auto addTransaction = [&](const std::string& trSignature) {
        concurrentQueue.enqueue(std::make_pair(trSignature ,BlockBench::Timer::timeNow()));
    };
    auto updateTransactionMap = [&](std::unordered_map<std::string, time_t>& map) {
        trElemType t;
        while(concurrentQueue.try_dequeue(t)) {
            DCHECK(map.count(t.first) == 0);
            map.insert(t);
        }
    };
    // workers
    std::vector<std::thread> threads;
    threads.reserve(threadNum);
    LOG(INFO) << "starting workers for custom benchmark, warming up..";
    for (size_t i = 0; i < threadNum; ++i) {
        auto pdb = BlockBench::CustomDB::CreateDB("aria", {});
        pdb->addPendingTransactionHandle = addTransaction;
        threads.emplace_back(&BlockBench::CustomDB::ClientThread, std::move(pdb), benchmarkTime, txRate);
        //BlockBench::Timer::sleep(1.0);
    }
    LOG(INFO) << "all worker started.";
    // monitors
    std::atomic<bool> finishSignal = false;
    // block height start from 1.
    auto pdb = BlockBench::CustomDB::CreateDB("aria", {});
    pdb->updateTransactionMapHandle = updateTransactionMap;
    std::thread monitor(&BlockBench::CustomDB::StatusThread, std::move(pdb), &finishSignal, 1.0, 1);
    for (auto& t : threads) {
        t.join();
    }
    LOG(INFO) << "all ClientThreads exited.";
    finishSignal = true;
    monitor.join();
}

void YCSBTestcase(uint32_t threadNum = 10, uint32_t txRate = 3000, size_t benchmarkTime = 30) {
    using trElemType = std::pair<std::string, time_t>;
    moodycamel::ConcurrentQueue<trElemType> concurrentQueue;
    auto addTransaction = [&](const std::string& trSignature) {
        concurrentQueue.enqueue(std::make_pair(trSignature ,BlockBench::Timer::timeNow()));
    };
    auto updateTransactionMap = [&](std::unordered_map<std::string, time_t>& map) {
        trElemType t;
        while(concurrentQueue.try_dequeue(t)) {
            DCHECK(map.count(t.first) == 0);
            map.insert(t);
        }
    };
    // workers
    std::vector<std::thread> threads;
    threads.reserve(threadNum);
    LOG(INFO) << "starting workers for ycsb benchmark, warming up..";
    for (size_t i = 0; i < threadNum; ++i) {
        auto pdb = BlockBench::YCSB_DB::CreateDB("aria", {});
        pdb->addPendingTransactionHandle = addTransaction;
        threads.emplace_back(&BlockBench::YCSB_DB::ClientThread, std::move(pdb), benchmarkTime, txRate);
        //BlockBench::Timer::sleep(1.0);
    }
    LOG(INFO) << "all worker started.";
    // monitors
    std::atomic<bool> finishSignal = false;
    // block height start from 1.
    auto pdb = BlockBench::YCSB_DB::CreateDB("aria", {});
    pdb->updateTransactionMapHandle = updateTransactionMap;
    std::thread monitor(&BlockBench::YCSB_DB::StatusThread, std::move(pdb), &finishSignal, 1.0, 1);
    for (auto& t : threads) {
        t.join();
    }
    LOG(INFO) << "all ClientThreads exited.";
    finishSignal = true;
    monitor.join();
}

/*
 * Usage:
 *  default: low cost mode, to maintain heart beat of block server to epoch server.
 *  -q: query mode, send a query key directly to a block server.
 *  -b [thread_number] [tps_per_thread] [total_ops]: small bank test mode / ycsb test mode
 */
int main(int argc, char *argv[]) {
    std::random_device randomDevice;
    srand(randomDevice());
    CryptoHelper::initCryptoSign(CryptoHelper::ServerType::user);
    const auto&& remoteIP = YAMLConfig::getInstance()->getLocalBlockServerIP();
    bool queryFlag = false;
    std::string read;
    for (int i = 0; i < argc; i++) {
        if (argv[i] == std::string("-q")) {
            queryFlag = true;
            CHECK(argc > i + 1);
            LOG(INFO) << "query mode on, send a query of a key..";
            read = argv[i + 1];
        }
        if (argv[i] == std::string("-b")) {
            if(argc <= i + 3) {
                LOG(INFO) << "usage: -b thread_number tps_per_thread total_ops";
                return -1;
            } else {
                const auto&& ccType = YAMLConfig::getInstance()->getCCType();
                if(ccType == "ycsb") {
                    YCSBTestcase(strtol(argv[i + 1], nullptr, 10),
                                 strtol(argv[i + 2], nullptr, 10),
                                 strtol(argv[i + 3], nullptr, 10));
                } else if (ccType == "small_bank") {
                    SmallBankTestcase(strtol(argv[i + 1], nullptr, 10),
                                      strtol(argv[i + 2], nullptr, 10),
                                      strtol(argv[i + 3], nullptr, 10));
                } else if (ccType == "test") {
                    CustomTestcase(strtol(argv[i + 1], nullptr, 10),
                                   strtol(argv[i + 2], nullptr, 10),
                                   strtol(argv[i + 3], nullptr, 10));
                }
                return 0;
            }
        }
    }

    uint64_t trCount = 0;
    if (queryFlag) {
        ZMQClient client(remoteIP, "7003");
        Utils::Request request;
        // use default table, and default cc_func
        request.reads = {read};
        std::string&& payloadRaw = Utils::getTransactionPayloadRaw(trCount, request);
        client.sendRequest(Utils::getUserQueryRequestRaw("key_query", payloadRaw));

        std::string replyRaw;
        client.getReply(replyRaw);
        Utils::printQueryResult(replyRaw);
    }
    else {
        LOG(INFO) << "low cost mode on, reduce to 10 tx per sec..";
        ZMQClient client(remoteIP, "5001", zmq::socket_type::pub);
        while (++trCount) {
            std::string&& payloadRaw = Utils::getEmptyPayloadRaw(trCount);
            client.sendRequest(Utils::getUserInvokeRequestRaw(payloadRaw));
            usleep(100000);
        }
    }
    return 0;
}
