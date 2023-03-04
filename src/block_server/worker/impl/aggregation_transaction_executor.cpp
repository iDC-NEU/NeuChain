//
// Created by peng on 4/27/21.
//

#include "block_server/worker/impl/aggregation_transaction_executor.h"
#include "block_server/reserve_table/reserve_table.h"
#include "block_server/transaction/transaction.h"

#include "glog/logging.h"

bool AggregationTransactionExecutor::executeList(const std::vector<Transaction *> &transactionList) {
    if (transactionList.empty())
        return false;
    // since all tx in list are from the same epoch, we only get reserve table once.
    if (reserveTable = ReserveTableProvider::getHandle()->getReserveTable(transactionList.front()->getEpoch()), !reserveTable) {
        LOG(ERROR) << "internal error, reserveTable not found";
        CHECK(false);
    }
    for(auto transaction: transactionList){
        if (transaction->getTransactionResult() == TransactionResult::ABORT_NO_RETRY)
            continue;
        DCHECK(transaction->getTransactionResult() == TransactionResult::PENDING);
        reserveTable->reserveRWSet(transaction->getRWSet(), transaction->getTransactionID());
    }
    return true;
}
