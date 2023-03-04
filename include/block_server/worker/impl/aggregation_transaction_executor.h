//
// Created by peng on 4/27/21.
//

#ifndef NEUBLOCKCHAIN_AGGREGATION_TRANSACTION_EXECUTOR_H
#define NEUBLOCKCHAIN_AGGREGATION_TRANSACTION_EXECUTOR_H

#include "block_server/worker/transaction_executor.h"

class AggregationTransactionExecutor: public TransactionExecutor {
public:
    bool executeList(const std::vector<Transaction*>& transactionList) override;
};


#endif //NEUBLOCKCHAIN_AGGREGATION_TRANSACTION_EXECUTOR_H
