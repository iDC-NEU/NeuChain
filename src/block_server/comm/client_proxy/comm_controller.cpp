//
// Created by peng on 2021/3/18.
//

#include "block_server/comm/client_proxy/comm_controller.h"
#include "block_server/transaction/transaction.h"
#include "common/payload/mock_utils.h"
#include "common/yaml_config.h"
#include "common/msp/crypto_helper.h"
#include "common/sha256_hepler.h"
#include "comm.pb.h"
#include "oe.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"

CommController::CommController(epoch_size_t startWithEpoch) :currentEpoch(startWithEpoch), finishSignal(false) {
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
    static epoch_size_t currentEpoch = 0;
    Transaction *transaction;
    if (processedTransaction.empty()) {
        receivedTransactionQueue.pop(transaction);
        if (transaction->getEpoch() > currentEpoch)
            currentEpoch = transaction->getEpoch();
        CHECK(transaction->getEpoch() == currentEpoch);
        processedTransaction.push(transaction);
    }
    while (receivedTransactionQueue.try_pop(transaction)) {
        if (transaction->getEpoch() > currentEpoch)
            currentEpoch = transaction->getEpoch();
        CHECK(transaction->getEpoch() == currentEpoch);
        processedTransaction.push(transaction);
    }
}

void CommController::blockReceiver(const std::string &requestRaw) {
    const auto formTransaction = [this](const std::string& userRequestRaw) {
        // from client proxy
        // 1.1 deserialize user request
        comm::UserRequest userRequest;
        userRequest.ParseFromString(userRequestRaw);
        // 1.2.1 check digest
        const CryptoSign *userSigner = CryptoHelper::getUserSigner(userRequest.ip());
        if (!userSigner->rsaDecrypt(userRequest.payload(), userRequest.digest())) {
            CHECK(false);
        }
        // 1.2.2 add the signature to hash helper, for generate batch signature
        SHA256Helper sha256Helper;
        if (!sha256Helper.append(userRequest.digest())) {
            CHECK(false);
        }
        // 1.3 construct transaction
        auto *transaction = Utils::makeTransaction();
        transaction->readOnly(false);
        // note: we dont set transaction id here!
        TransactionPayload *transactionPayload = transaction->getTransactionPayload();
        transactionPayload->ParseFromString(userRequest.payload());
        transactionPayload->set_digest(userRequest.digest());
        // continue processing
        transaction->setEpoch(currentEpoch);
        // we use user signature to set tx id.
        const auto &digest = transactionPayload->digest();
        transaction->setTransactionID(*reinterpret_cast<const tid_size_t *>(digest.data()));
        // 1.4 add to transaction list
        return transaction;
    };
    comm::UserRequest userRequest;
    if (!userRequest.ParseFromString(requestRaw)) {
        CHECK(false);
    }
    auto* tx = formTransaction(requestRaw);
    receivedTransactionQueue.push(tx);
}

void CommController::increaseEpoch() {
    currentEpoch++;
    auto *transaction = Utils::makeTransaction();
    transaction->setEpoch(currentEpoch);
    transaction->setTransactionID(UINT64_MAX);
    receivedTransactionQueue.push(transaction);
}

void TransactionRecycler::clearTransactionForEpoch(epoch_size_t epoch) {
    // first add all tx from queue to map
    Transaction* tx = nullptr;
    while (addingTxQueue.try_dequeue(tx)) {
        transactionMap[tx->getEpoch()].push_back(tx);
    }
    std::vector<Transaction*>& previousEpochTransaction = transactionMap[epoch];
    std::vector<Transaction*>& nextEpochTransaction = transactionMap[epoch + 1];
    for (auto tr: previousEpochTransaction) {
        if(tr->getEpoch() == epoch) {
            delete tr;
            continue;
        }
        CHECK(tr->getEpoch() == epoch + 1);
        nextEpochTransaction.push_back(tr);
    }
    transactionMap.erase(epoch);
}