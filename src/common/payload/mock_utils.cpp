//
// Created by peng on 4/25/21.
//

#include "common/payload/mock_utils.h"
#include "block_server/transaction/mock/mock_transaction.h"
#include "glog/logging.h"

Transaction* Utils::makeTransaction(epoch_size_t epoch, tid_size_t tid, bool readOnly) {
    Transaction *transaction = new MockTransaction;
    transaction->setEpoch(epoch);
    transaction->setTransactionID(tid);
    transaction->readOnly(readOnly);
    return transaction;
}

Transaction* Utils::makeTransaction(const std::string& transactionRaw) {
    Transaction *transaction = new MockTransaction;
    if(!transaction->deserializeFromString(transactionRaw)) {
        LOG(ERROR) << "deserialize transaction failed!";
        CHECK(false);
    }
    return transaction;
}

Transaction* Utils::makeTransaction() {
    return new MockTransaction;
}