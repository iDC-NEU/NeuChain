//
// Created by peng on 2021/6/29.
//

#ifndef NEUBLOCKCHAIN_BLOCK_SERVER_TEST_H
#define NEUBLOCKCHAIN_BLOCK_SERVER_TEST_H

#include <gtest/gtest.h>
#include <common/payload/ycsb_payload.h>
#include <transaction.pb.h>
#include <common/random_generator.h>
#include "block_server/database/impl/level_db_storage.h"
#include "block_server/coordinator/aggregation_coordinator.h"
#include "block_server/database/database.h"
#include "block_server/transaction/impl/transaction_manager_impl.h"
#include "block_server/worker/chaincode_manager.h"
#include "block_server/comm/query/query_controller.h"
#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"
#include "common/fnv_hash.h"

#include "block_server/database/impl/block_generator_impl.h"

#include <thread>

class TransactionGenerator {
public:
    TransactionGenerator() :acc_gen(1, account_count, random()), bal_gen(50, 100, random()) {}

    inline TransactionPayload generateTransferTransaction(size_t tid) {
        Utils::Request request;
        request.tableName = "small_bank";
        auto acc1 = std::to_string(acc_gen.next());
        auto acc2 = std::to_string(acc_gen.next());
        while (acc1 == acc2) {
            acc2 = std::to_string(acc_gen.next());
        }
        request.reads = { acc1, acc2, std::to_string(bal_gen.next()) };
        //set message
        TransactionPayload payload;
        payload.set_header("sendPayment");
        payload.set_payload(getYCSBPayloadRaw(request));
        payload.set_nonce(tid);
        return payload;
    }

    static inline TransactionPayload initTransaction(size_t tid) {
        // construct request
        Utils::Request request;
        request.tableName = "small_bank";
        request.reads = { std::to_string(tid), std::to_string(init_balance) };
        //set message
        TransactionPayload payload;
        payload.set_header("updateBalance");
        payload.set_payload(getYCSBPayloadRaw(request));
        payload.set_nonce(tid);
        return payload;
    }

    static const size_t account_count = 1000;
    static const size_t init_balance = 1000;    // 2000 total

private:
    BlockBench::UniformGenerator acc_gen;
    BlockBench::UniformGenerator bal_gen;
};


#include "block_server/comm/comm.h"
#include "block_server/transaction/transaction.h"
#include "common/payload/mock_utils.h"
#include <map>

class MockTransactionComm: public Comm{
public:
    explicit MockTransactionComm(epoch_size_t startWithEpoch) { epoch = startWithEpoch;}

    std::unique_ptr<std::vector<Transaction*>> getTransaction(epoch_size_t _epoch, uint32_t maxSize, uint32_t minSize) override {
        if(_epoch < this->epoch)
            return nullptr;
        auto trWrapper = std::make_unique<std::vector<Transaction*>>();
        trWrapper->reserve(maxSize);
        while(this->epochTrSentNum != EPOCH_MAX_TR_NUM && trWrapper->size() < minSize) {
            auto makeTransaction = [&] () -> Transaction* {
                static uint64_t nonce = 0; nonce++;
                auto tid = FNVHash64(nonce);
                Transaction* transaction = Utils::makeTransaction(epoch, tid, false);
                *transaction->getTransactionPayload() = generator.generateTransferTransaction(tid);
                trWrapper->push_back(transaction);
                return transaction;
            };
            transactionMap[this->epoch].push_back(makeTransaction());
            this->epochTrSentNum += 1;
        }

        if (this->epochTrSentNum == EPOCH_MAX_TR_NUM) {
            epochTrSentNum = 0;
            this->epoch += 1;
        }

        return trWrapper;
    }

    void clearTransactionForEpoch(epoch_size_t _epoch) override {
        DCHECK(_epoch < this->epoch);
        std::vector<Transaction*>& previousEpochTransaction = transactionMap[_epoch];
        for (auto tr: previousEpochTransaction) {
            if(tr->getEpoch() == _epoch) {
                delete tr;
                continue;
            }
            transactionMap[tr->getEpoch()].push_back(tr);
        }
        transactionMap.erase(_epoch);
    }

private:
    epoch_size_t epoch;
    // for restore test, this need to be changed as well.
    // because the impl of this dont need epoch, the creation of trs, which is the usage of epoch, belongs to client proxy
    size_t epochTrSentNum = 0;
    std::map<epoch_size_t ,std::vector<Transaction*>> transactionMap;

private:
    static const size_t EPOCH_MAX_TR_NUM = 1000;
    TransactionGenerator generator;
};


class BlockServerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

#endif //NEUBLOCKCHAIN_BLOCK_SERVER_TEST_H
