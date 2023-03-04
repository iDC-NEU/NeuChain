//
// Created by peng on 2021/1/22.
//

#ifndef NEUBLOCKCHAIN_TRANSACTIONEXECUTORIMPL_H
#define NEUBLOCKCHAIN_TRANSACTIONEXECUTORIMPL_H

#include "block_server/worker/transaction_executor.h"

class ReserveTable;

class TransactionExecutorImpl: public TransactionExecutor {
public:
    bool executeList(const std::vector<Transaction*>& transactionList) override;
};



#endif //NEUBLOCKCHAIN_TRANSACTIONEXECUTORIMPL_H
