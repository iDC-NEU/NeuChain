//
// Created by peng on 2/20/21.
//

#include "block_server/database/impl/database_impl.h"
#include "block_server/database/impl/db_storage_impl.h"
#include "block_server/database/impl/level_db_storage.h"
#include "block_server/database/impl/hash_map_storage.h"
#include "block_server/reserve_table/mock/mock_reserve_table.h"

#include "block_server/database/orm/table_definition.h"
#include "block_server/database/orm/fields/orm_field.h"


#include "glog/logging.h"
#include "kv_rwset.pb.h"

VersionedDB *DatabaseImpl::createInstance() {
    return new DatabaseImpl;
}


DatabaseImpl::DatabaseImpl()
        :commitPhase(false), storage(new DB_STORAGE_TYPE) {
    storage->bufferUpdates = [this] (tid_size_t tid, KVRWSet* kvRWSet,
                                  std::vector<std::shared_ptr<AriaORM::ORMTableDefinition>> tableList) ->bool {
        // notice: reentry protection
        // all the worker has one unique coordinator, so it does not happen when one worker is
        // committing while other is buffering updates.
        if(commitPhase)
            clearBufferThreadSafe();
        // DLOG(INFO) << tid <<"write";
        rwSetBuffer[tid] = kvRWSet;
        tableListBuffer[tid] = std::move(tableList);
        return true;
    };
}

bool DatabaseImpl::commitUpdate(tid_size_t tid) {
    commitPhase = true;
    DCHECK(rwSetBuffer.contains(tid));
    DCHECK(tableListBuffer.contains(tid));
    // first, create table
    for (const auto& _table: tableListBuffer[tid]) {
        auto* table = dynamic_cast<AriaORM::ORMTableDBDefinition*>(_table.get());
        CHECK(table->fieldSize() == 2);
        storage->createTable(table->getTableName(),
                          table->getField(0)->getColumnName(),
                          table->getField(1)->getColumnName());
    }
    // then, update writes
    for(const KVWrite& kvWrite: rwSetBuffer[tid]->writes()) {
        if (kvWrite.is_delete()) {
            storage->removeWriteSet(kvWrite.key(), kvWrite.table());
        } else {
            storage->updateWriteSet(kvWrite.key(), kvWrite.value(), kvWrite.table());
        }
    }
    return true;
}

void DatabaseImpl::abortUpdate(tid_size_t tid) {
    commitPhase = true;
}

void DatabaseImpl::clearBufferThreadSafe() {
    static std::mutex mutex;
    if(commitPhase) {  // performance
        std::unique_lock<std::mutex> lock(mutex);
        if(commitPhase) {  // prevent for reentry
            DLOG(INFO) << "clear buffer";
            rwSetBuffer.clear();
            tableListBuffer.clear();
            commitPhase = false;    // move flag to last, thread safe.
        }
    }
}
