//
// Created by peng on 2021/3/18.
//

#include "block_server/comm/client_proxy/comm_controller.h"
#include "block_server/comm/client_proxy/client_proxy.h"
#include "block_server/transaction/transaction.h"
#include "glog/logging.h"

CommController::CommController(epoch_size_t startWithEpoch, ClientProxy* clientProxy)
        :clientProxy(clientProxy) {
    clientProxy->epochBroadcastTransactionsHandle = [this] (Transaction* tx) {
        broadcastTransactions.push(tx);
    };
    clientProxy->startService();
}

CommController::~CommController() = default;

std::unique_ptr<std::vector<Transaction*>> CommController::getTransaction(epoch_size_t epoch, uint32_t maxSize, uint32_t minSize) {
    // 0. check if we already gave all trs in _epoch
    if(processedTransaction.empty()) {
        // 1. all trs in _epoch has not finished
        // 2. all trs in _epoch has finished, but we didnt received any _epoch+1 trs
        // for both 1 and 2, the processedTransaction queue must be empty,
        // so pull at least one transaction, check its epoch
        DCHECK(processedTransaction.empty() || processedTransaction.front()->getEpoch() == epoch);
        // we must wait for a transaction to fill in processedTransaction queue
        transferTransaction();
    }
    // check condition 2
    if(processedTransaction.front()->getEpoch() != epoch) {
        DCHECK(processedTransaction.front()->getEpoch() == epoch + 1 ||
               processedTransaction.front()->getEpoch() == epoch + 2);
        return nullptr;
    }
    // create new tr wrapper and reserve size.
    auto trWrapper = std::make_unique<std::vector<Transaction*>>();
    trWrapper->reserve(maxSize);
    // deal with condition 1
    // 1.1 we have emptied processedTransaction queue but still not / just collect all trs
    // 1.2 we have collected all trs in _epoch and queue is not empty
    // for 1.1
    // 1.1.1 greater than minSize, less than maxSize, return
    // 1.1.2 greater than maxSize, return
    // 1.1.3 less than minSize, block wait
    // the loop condition is 1.1.2
    while(trWrapper->size() < maxSize) {
        Transaction* transaction = processedTransaction.front();
        // 1.2
        if(transaction->getEpoch() != epoch) {
            break;
        }
        processedTransaction.pop();
        trWrapper->push_back(transaction);
        addingTxQueue.enqueue(transaction);
        if(processedTransaction.empty()) {
            // 1.1.1
            if(trWrapper->size() > minSize) {
                break;
            }
            // 1.1.3
            transferTransaction();
        }
    }
    return trWrapper;
}

void CommController::transferTransaction() {
    // this func is a adaptor to processedTransaction, because concurrent queue does not support front() func.
    // when func return, make sure that processedTransaction has at least 1 tr
    Transaction *transaction;
    static epoch_size_t currentEpoch = 0;
    if (processedTransaction.empty()) {
        broadcastTransactions.pop(transaction);
        if (transaction->getEpoch() > currentEpoch)
            currentEpoch = transaction->getEpoch();
        CHECK(transaction->getEpoch() == currentEpoch);
        processedTransaction.push(transaction);
    }
    while (broadcastTransactions.try_pop(transaction)) {
        if (transaction->getEpoch() > currentEpoch)
            currentEpoch = transaction->getEpoch();
        CHECK(transaction->getEpoch() == currentEpoch);
        processedTransaction.push(transaction);
    }
}

void TransactionRecycler::clearTransactionForEpoch(epoch_size_t epoch) {
    // first add all tx from queue to map
    Transaction* tx = nullptr;
    // pre-alloc memory
    transactionMap[epoch + 20].reserve(10000);
    while (addingTxQueue.try_dequeue(tx)) {
        transactionMap[tx->getEpoch()].push_back(tx);
    }
    auto& previousEpochTransaction = transactionMap[epoch];
    for (auto tr: previousEpochTransaction) {
        if(tr->getEpoch() == epoch) {
            delete tr;
            continue;
        }
        transactionMap[tr->getEpoch()].push_back(tr);
    }
    transactionMap.erase(epoch);
}
