//
// Created by peng on 2021/1/19.
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_EXECUTOR_H
#define NEUBLOCKCHAIN_TRANSACTION_EXECUTOR_H

#include <vector>
#include <memory>
#include "common/aria_types.h"

class Transaction;
class ReserveTable;

class TransactionExecutor {
public:
    virtual ~TransactionExecutor() = default;

    enum class ExecutorType {
        normal = 0,
        aggregate = 1,
        query = 2,
    };

    // for query executor handle must not be null
    static std::unique_ptr<TransactionExecutor> transactionExecutorFactory(ExecutorType type);

    virtual bool executeList(const std::vector<Transaction*>& transactionList) = 0;
    virtual bool commitList(const std::vector<Transaction*>& transactionList);

protected:
    ReserveTable* reserveTable{};
};


#endif //NEUBLOCKCHAIN_TRANSACTION_EXECUTOR_H
