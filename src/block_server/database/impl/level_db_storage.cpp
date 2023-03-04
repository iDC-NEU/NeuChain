//
// Created by peng on 2021/3/26.
//

#include "block_server/database/impl/level_db_storage.h"
#include "block_server/database/orm/impl/level_db_results_iterator.h"
#include "leveldb/db.h"
#include "glog/logging.h"

void LevelDBStorage::createTable(const std::string &tableName, const std::string &key, const std::string &value) {
    LOG(INFO) << "level db does not support create table func.";
}

bool LevelDBStorage::getTableStructure(const std::string &table, std::map<std::string, uint> &tableStructure) {
    LOG(INFO) << "level db does not support get table structure func.";
    return true;
}

void LevelDBStorage::removeWriteSet(const std::string &key, const std::string &table) {
    auto* partition = getTable(table);
    leveldb::Status status = partition->Delete(leveldb::WriteOptions(), key);
    DCHECK(status.ok());
}

void LevelDBStorage::updateWriteSet(const std::string &key, const std::string &value, const std::string &table) {
    auto* partition = getTable(table);
    leveldb::Status status = partition->Put(leveldb::WriteOptions(), key, value);
    DCHECK(status.ok());
}

std::unique_ptr<AriaORM::ResultsIterator> LevelDBStorage::selectDB(const std::string &key, const std::string &table) {
    std::string value;
    auto* partition = getTable(table);
    leveldb::Status status = partition->Get(leveldb::ReadOptions(), key, &value);
    return std::make_unique<AriaORM::LevelDBResultsIterator>(key, value);
}

leveldb::DB *LevelDBStorage::getTable(const std::string &table) {
    if(!db.contains(table)) {
        std::unique_lock<std::mutex> lock(mutex);
        if(!db.contains(table)) {
            leveldb::Options options;
            options.create_if_missing = true;
            leveldb::DB* partition;
            leveldb::Status status = leveldb::DB::Open(options, table, &partition);
            CHECK(status.ok());
            DLOG(INFO) << "opened database partition: " << table;
            db[table] = std::shared_ptr<leveldb::DB>(partition);
            return partition;
        }
    }
    return db[table].get();
}
