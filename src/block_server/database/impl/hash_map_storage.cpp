//
// Created by peng on 3/31/21.
//

#include "block_server/database/impl/hash_map_storage.h"
#include "block_server/database/orm/impl/level_db_results_iterator.h"
#include "glog/logging.h"


HashMapStorage::HashMapStorage() {
    LOG(INFO) << "hash map database start.";
}

void HashMapStorage::createTable(const std::string &tableName, const std::string &key, const std::string &value) {
    LOG(INFO) << "level db does not support create table func.";
}

std::unique_ptr<AriaORM::ResultsIterator> HashMapStorage::selectDB(const std::string &key, const std::string &table) {
    return std::make_unique<AriaORM::LevelDBResultsIterator>(key, db[key]);
}

bool HashMapStorage::getTableStructure(const std::string &table, std::map<std::string, uint> &tableStructure) {
    LOG(INFO) << "level db does not support get table structure func.";
    return true;
}

void HashMapStorage::removeWriteSet(const std::string &key, const std::string &table) {
    DCHECK(db.remove(key));
}

void HashMapStorage::updateWriteSet(const std::string &key, const std::string &value, const std::string &table) {
    db[key] = value;
}
