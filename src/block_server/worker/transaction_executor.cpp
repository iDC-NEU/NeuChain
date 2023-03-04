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
    auto db = VersionedDB::getDBInstance();
    for(auto txn: transactionList) {
        if (txn->getTransactionResult() == TransactionResult::ABORT_NO_RETRY) {
            // 1. txn internal error, abort it without dealing with reserve table
            db->abortUpdate(txn->getTransactionID());
            continue;
        }
        // 2. analyse dependency
        auto&& dep = reserveTable->dependencyAnalysis(txn->getRWSet(), txn->getTransactionID());
        if (dep.waw) { // waw, abort the txn.
            txn->setTransactionResult(TransactionResult::ABORT);
            continue;
        } else if (!dep.war || !dep.raw) {    //  war / raw / no dependency, commit it.
            if(db->commitUpdate(txn->getTransactionID())) {
                txn->setTransactionResult(TransactionResult::COMMIT);
            } else {
                LOG(ERROR) << "tx abort with rw set error: " << txn->getTransactionID();
                txn->setTransactionResult(TransactionResult::ABORT_NO_RETRY);
            }
        } else {    // war and raw, abort the txn.
            db->abortUpdate(txn->getTransactionID());
            txn->setTransactionResult(TransactionResult::ABORT);
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
