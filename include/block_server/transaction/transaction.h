//
// Created by peng on 2021/1/17.
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_H
#define NEUBLOCKCHAIN_TRANSACTION_H

#include <memory>
#include <vector>
#include "common/aria_types.h"

class KVRWSet;
class TransactionPayload;

enum class TransactionResult { COMMIT, PENDING, ABORT, ABORT_NO_RETRY};

class Transaction {
public:
    virtual ~Transaction() = default;
    // notice: tid start from 1, not equal 0
    virtual tid_size_t getTransactionID() = 0;
    virtual void setTransactionID(tid_size_t tid) = 0;
    // search op, and execute op must be done with worker or db!
    // transaction object can not hold a db reference!

    virtual void resetRWSet() = 0;
    virtual KVRWSet* getRWSet() = 0;

    virtual void resetTransaction() = 0;    // if a transaction abort, it must be reset.
    virtual bool readOnly() = 0;
    virtual void readOnly(bool flag) = 0;

    virtual void setEpoch(epoch_size_t epoch) = 0;
    virtual epoch_size_t getEpoch() = 0;

    virtual TransactionResult getTransactionResult() = 0;
    virtual void setTransactionResult(TransactionResult transactionResult) = 0;

    virtual TransactionPayload* getTransactionPayload() = 0;
    virtual void resetTransactionPayload() = 0;

    virtual bool serializeToString(std::string* transactionRaw) = 0;
    virtual bool deserializeFromString(const std::string& transactionRaw) = 0;

};


#endif //NEUBLOCKCHAIN_TRANSACTION_H
