//
// Created by peng on 2021/3/12.
//

#include "block_server/database/orm/cc_orm_helper.h"
#include "block_server/database/orm/impl/table_impl.hpp"
#include "block_server/database/orm/impl/query_impl.h"
#include "block_server/database/orm/impl/insert_impl.h"

#include "block_server/database/db_storage.h"

#include "kv_rwset.pb.h"
#include "glog/logging.h"

AriaORM::CCORMHelper::CCORMHelper(tid_size_t _tid, KVRWSet* _kvRWSet, bool _readOnly)
        : tid(_tid), readOnly(_readOnly) {
    kvRWSet = _kvRWSet;
    storage = VersionedDB::getDBInstance()->getStorage();
    DCHECK(storage);
    DCHECK(kvRWSet->reads().empty());
    DCHECK(kvRWSet->writes().empty());
}

AriaORM::CCORMHelper::~CCORMHelper() {
    execute();
}

AriaORM::ORMTableDefinition *AriaORM::CCORMHelper::createTable(const std::string &tableName) {
    std::unique_ptr<AriaORM::ORMTableImpl> table = std::make_unique<AriaORM::ORMTableImpl>(tableName);
    KVWrite* kvWrite = kvRWSet->add_writes();
    kvWrite->set_table(ARIA_SYS_TABLE);
    kvWrite->set_key(tableName);
    kvWrite->set_value("place holder of " + std::to_string(tid));
    tableList.push_back(std::move(table));
    return tableList.back().get();
}

AriaORM::ORMQuery *AriaORM::CCORMHelper::newQuery(const std::string &tableName) {
    std::unique_ptr<AriaORM::QueryImpl> query = std::make_unique<AriaORM::QueryImpl>(tableName, storage, kvRWSet);
    queryList.push_back(std::move(query));
    return queryList.back().get();
}

AriaORM::ORMInsert *AriaORM::CCORMHelper::newInsert(const std::string &tableName) {
    if (readOnly) {
        LOG(ERROR) << "try to invoke a rw chaincode with method read-only!";
        CHECK(false);
    }
    std::unique_ptr<AriaORM::InsertImpl> insert = std::make_unique<AriaORM::InsertImpl>(tableName, storage, kvRWSet);
    insertList.push_back(std::move(insert));
    return insertList.back().get();
}

bool AriaORM::CCORMHelper::execute() {
    if (readOnly) {
        return false;
    }
    return storage->bufferUpdates(tid, kvRWSet, tableList);
}
