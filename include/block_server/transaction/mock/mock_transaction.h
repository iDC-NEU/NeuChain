//
// Created by peng on 2021/1/18.
//

#ifndef NEUBLOCKCHAIN_MOCKTRANSACTION_H
#define NEUBLOCKCHAIN_MOCKTRANSACTION_H

#include "block_server/transaction/transaction.h"

enum class TransactionResult;

class MockTransaction : public Transaction {
public:
    MockTransaction();
    ~MockTransaction() override;

    tid_size_t getTransactionID() override {
        return tid;
    }
    void setTransactionID(tid_size_t _tid) override {
        tid = _tid;
    }

    void resetRWSet() override;
    KVRWSet* getRWSet() override {
        return this->kvRWSet;
    }

    void resetTransaction() override;    // if a transaction abort, it must be reset.
    bool readOnly() override {
        return readOnlyFlag;
    }
    void readOnly(bool flag) override {
        readOnlyFlag = flag;
    }

    void setEpoch(epoch_size_t _epoch) override {
        this->epoch = _epoch;
    }
    [[nodiscard]] epoch_size_t getEpoch() const override {
        return epoch;
    }

    TransactionResult getTransactionResult() override {
        return transactionResult;
    }
    void setTransactionResult(TransactionResult _transactionResult) override {
        this->transactionResult = _transactionResult;
    }

    TransactionPayload* getTransactionPayload() override {
        return transactionPayload;
    }
    void resetTransactionPayload() override;

    bool serializeToString(std::string* transactionRaw) override;
    bool deserializeFromString(const std::string& transactionRaw) override;

    bool serializeResultToString(std::string* transactionRaw) override;
    bool deserializeResultFromString(const std::string& transactionRaw) override;

private:
    epoch_size_t epoch;
    tid_size_t tid;
    bool readOnlyFlag;
    KVRWSet* kvRWSet;
    TransactionResult transactionResult;
    TransactionPayload* transactionPayload;
};


#endif //NEUBLOCKCHAIN_MOCKTRANSACTION_H
