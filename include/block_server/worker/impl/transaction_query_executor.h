//
// Created by peng on 2021/4/22.
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_QUERY_EXECUTOR_H
#define NEUBLOCKCHAIN_TRANSACTION_QUERY_EXECUTOR_H

#include "block_server/worker/transaction_executor.h"

class TransactionQueryExecutor: public TransactionExecutor {
public:
    bool executeList(const std::vector<Transaction*>& transactionList) override;
    bool commitList(const std::vector<Transaction*>& transactionList) override { return false; }

private:
};


#endif //NEUBLOCKCHAIN_TRANSACTION_QUERY_EXECUTOR_H
