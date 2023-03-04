//
// Created by peng on 2021/1/22.
//

#include "block_server/worker/impl/transaction_executor_impl.h"
#include "block_server/worker/chaincode_manager.h"

#include "block_server/transaction/transaction.h"
#include "block_server/reserve_table/reserve_table.h"

#include "glog/logging.h"


bool TransactionExecutorImpl::executeList(const std::vector<Transaction*>& transactionList) {
    if (transactionList.empty())
        return false;
    // since all tx in list are from the same epoch, we only get reserve table once.
    if (reserveTable = ReserveTableProvider::getHandle()->getReserveTable(transactionList.front()->getEpoch()), !reserveTable) {
        LOG(ERROR) << "internal error, reserveTable not found";
        CHECK(false);
    }
    for(auto transaction: transactionList){
        auto dbTestChaincode = ChaincodeManager::getChaincodeInstance(transaction);
        if (!dbTestChaincode->InvokeChaincode(*transaction->getTransactionPayload())) {
            // 1. transaction internal error, abort it without adding reserve table
            transaction->setTransactionResult(TransactionResult::ABORT_NO_RETRY);
            continue;
        }
        // 2. reserve rw set
        reserveTable->reserveRWSet(transaction->getRWSet(), transaction->getTransactionID());
    }
    return true;
}
