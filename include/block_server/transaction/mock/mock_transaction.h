//
// Created by peng on 2021/1/18.
//

#ifndef NEUBLOCKCHAIN_MOCKTRANSACTION_H
#define NEUBLOCKCHAIN_MOCKTRANSACTION_H

#include "block_server/transaction/transaction.h"

enum class TransactionResult;

class MockTransaction : public Transaction{
public:
    MockTransaction();
    ~MockTransaction() override;

    tid_size_t getTransactionID() override;
    void setTransactionID(tid_size_t tid) override;

    void resetRWSet() override;
    KVRWSet* getRWSet() override;

    void resetTransaction() override;    // if a transaction abort, it must be reset.
    bool readOnly() override;
    void readOnly(bool flag) override;

    void setEpoch(epoch_size_t epoch) override;
    epoch_size_t getEpoch() override;

    TransactionResult getTransactionResult() override;
    void setTransactionResult(TransactionResult _transactionResult) override;

    TransactionPayload* getTransactionPayload() override;
    void resetTransactionPayload() override;

    bool serializeToString(std::string* transactionRaw) override;
    bool deserializeFromString(const std::string& transactionRaw) override;

private:
    epoch_size_t epoch;
    tid_size_t tid;
    bool readOnlyFlag;
    KVRWSet* kvRWSet;
    TransactionResult transactionResult;
    TransactionPayload* transactionPayload;
};


#endif //NEUBLOCKCHAIN_MOCKTRANSACTION_H
