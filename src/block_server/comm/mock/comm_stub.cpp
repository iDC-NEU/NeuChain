//
// Created by peng on 2021/1/22.
//

#include "block_server/comm/mock/comm_stub.h"
#include "block_server/transaction/transaction.h"

#include "common/payload/ycsb_payload.h"
#include "common/payload/mock_utils.h"
#include "glog/logging.h"
#include "transaction.pb.h"

std::unique_ptr<std::vector<Transaction*>> CommStub::getTransaction(epoch_size_t _epoch, uint32_t maxSize, uint32_t minSize) {
    if(_epoch < this->epoch)
        return nullptr;
    auto trWrapper = std::make_unique<std::vector<Transaction*>>();
    trWrapper->reserve(maxSize);
    while(this->epochTrSentNum != EPOCH_MAX_TR_NUM && trWrapper->size() < minSize) {
        auto makeTransaction = [&] () -> Transaction* {
            static uint64_t tid = 0; tid++;
            auto* transaction = Utils::makeTransaction(epoch, tid, false);
            *transaction->getTransactionPayload() = Utils::getRandomTransactionPayload(tid);
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

void CommStub::clearTransactionForEpoch(epoch_size_t _epoch) {
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

CommStub::CommStub(epoch_size_t startWithEpoch) {
    epoch = startWithEpoch;
}
