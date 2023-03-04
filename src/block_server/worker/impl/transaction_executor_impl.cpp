//
// Created by peng on 2021/1/22.
//

#include <common/sha256_hepler.h>
#include "block_server/database/database.h"
#include "block_server/database/db_storage.h"
#include "block_server/worker/impl/transaction_executor_impl.h"
#include "block_server/worker/chaincode_manager.h"

#include "block_server/transaction/transaction.h"
#include "block_server/reserve_table/reserve_table.h"
#include "block_server/database/orm/query.h"
#include "kv_rwset.pb.h"
#include "glog/logging.h"

bool analysisInterBlockReadConflict(Transaction* transaction, DBStorage* storage) {
    const auto& rwSet = transaction->getRWSet();
    for(const auto& read: rwSet->reads()) {
        auto iter = storage->selectDB(read.key(), read.table());
        if(!iter->hasNext()) {
            // error
            return false;
        }
        SHA256Helper hash;
        std::string buffer;
        hash.hash(iter->getString("value"), &buffer);    // version
        if(buffer != read.value()) {
            // dirty read
            return false;
        }
    }
    // valid
    storage->bufferUpdates(transaction->getTransactionID(), transaction->getRWSet(), {});
    return true;
}

bool TransactionExecutorImpl::executeList(const std::vector<Transaction*>& transactionList) {
    auto storage = VersionedDB::getDBInstance()->getStorage();
    if (transactionList.empty())
        return false;
    // since all tx in list are from the same epoch, we only get reserve table once.
    if (reserveTable = ReserveTableProvider::getHandle()->getReserveTable(transactionList.front()->getEpoch()), !reserveTable) {
        LOG(ERROR) << "internal error, reserveTable not found";
        CHECK(false);
    }
    for(auto transaction: transactionList){
        if (transaction->getTransactionResult() == TransactionResult::ABORT_NO_RETRY ||
            !analysisInterBlockReadConflict(transaction, storage)) {
            // 1. read conflict
            transaction->setTransactionResult(TransactionResult::ABORT);
            continue;
        }
        // 2. reserve rw set
        reserveTable->reserveRWSet(transaction->getRWSet(), transaction->getTransactionID());
    }
    return true;
}
