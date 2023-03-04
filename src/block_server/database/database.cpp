//
// Created by peng on 2021/1/20.
//
#include "block_server/database/database.h"
#include "block_server/database/impl/database_impl.h"

#include <mutex>

VersionedDB* VersionedDB::getDBInstance() {
    static VersionedDB* db = nullptr;
    static std::mutex mutex;
    if(db== nullptr) {  // performance
        std::unique_lock<std::mutex> lock(mutex);
        if(db== nullptr) {  // prevent for reentry
            db = DatabaseImpl::createInstance();
        }
    }
    return db;
}
