//
// Created by peng on 2021/4/22.
//

#include "block_server/worker/impl/transaction_query_executor.h"
#include "block_server/worker/chaincode_manager.h"

#include "block_server/transaction/transaction.h"

#include "glog/logging.h"

bool TransactionQueryExecutor::executeList(const std::vector<Transaction*>& transactionList) {
    for(auto transaction: transactionList){
        // if recv a ! readonly tr, abort it.
        if(!transaction->readOnly()) {
            DLOG(ERROR) << "For read-only trs invoke, abort!";
            transaction->setTransactionResult(TransactionResult::ABORT_NO_RETRY);
            continue;
        }
        // TODO: using try... catch block
        // invoke chaincode
        auto dbTestChaincode = ChaincodeManager::getChaincodeInstance(transaction);
        if (!dbTestChaincode->InvokeChaincode(*transaction->getTransactionPayload())) {
            transaction->setTransactionResult(TransactionResult::ABORT_NO_RETRY);
            DLOG(INFO) << "Read-only transaction invoke failed!";
            return false;
        }
        transaction->setTransactionResult(TransactionResult::COMMIT);
    }
    return true;
}
