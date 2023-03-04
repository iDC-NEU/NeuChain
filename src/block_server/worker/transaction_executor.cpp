//
// Created by peng on 2021/6/17.
//

#include "block_server/worker/transaction_executor.h"
#include "block_server/worker/impl/transaction_executor_impl.h"
#include "block_server/worker/impl/aggregation_transaction_executor.h"
#include "block_server/worker/impl/transaction_query_executor.h"

#include "block_server/database/database.h"
#include "block_server/transaction/transaction.h"
#include "block_server/reserve_table/reserve_table.h"
#include "block_server/reserve_table/trasnaction_dependency.h"

#include "glog/logging.h"

bool TransactionExecutor::commitList(const std::vector<Transaction *> &transactionList) {  //reentry
    auto dbInstance = VersionedDB::getDBInstance();
    for(auto transaction: transactionList) {
        if (transaction->getTransactionResult() == TransactionResult::ABORT_NO_RETRY) {
            // 1. transaction internal error, abort it without dealing with reserve table
            dbInstance->abortUpdate(transaction->getTransactionID());
            continue;
        }
        // 2. analyse dependency
        auto&& trDependency = reserveTable->dependencyAnalysis(transaction->getRWSet(), transaction->getTransactionID());
        if (trDependency.waw) { // waw, abort the transaction.
            transaction->setTransactionResult(TransactionResult::ABORT);
            continue;
        } else if (!trDependency.war || !trDependency.raw) {    //  war / raw / no dependency, commit it.
            if(dbInstance->commitUpdate(transaction->getTransactionID())) {
                transaction->setTransactionResult(TransactionResult::COMMIT);
            } else {
                LOG(ERROR) << "tr abort with rw set error: " << transaction->getTransactionID();
                transaction->setTransactionResult(TransactionResult::ABORT_NO_RETRY);
            }
            continue;
        } else {    // war and raw, abort the transaction.
            dbInstance->abortUpdate(transaction->getTransactionID());
            transaction->setTransactionResult(TransactionResult::ABORT);
        }
    }
    return true;
}

std::unique_ptr<TransactionExecutor>
TransactionExecutor::transactionExecutorFactory(TransactionExecutor::ExecutorType type) {
    std::unique_ptr<TransactionExecutor> executor = nullptr;
    switch (type) {
        case ExecutorType::normal:
            executor = std::make_unique<TransactionExecutorImpl>();
            break;
        case ExecutorType::aggregate:
            executor = std::make_unique<AggregationTransactionExecutor>();
            break;
        case ExecutorType::query:
            executor = std::make_unique<TransactionQueryExecutor>();
            break;
        default:
            break;
    }
    return executor;
}

bool TransactionExecutor::reserveList(const std::vector<Transaction *> &transactionList) {
    for(auto transaction: transactionList){
        reserveTable->reserveRWSet(transaction->getRWSet(), transaction->getTransactionID());
    }
    return true;
}
