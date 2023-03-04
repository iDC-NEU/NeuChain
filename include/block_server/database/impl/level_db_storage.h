//
// Created by peng on 2021/3/26.
//

#ifndef NEUBLOCKCHAIN_LEVEL_DB_STORAGE_H
#define NEUBLOCKCHAIN_LEVEL_DB_STORAGE_H

#include "block_server/database/db_storage.h"
#include "common/hash_map.h"
#include <mutex>

namespace leveldb{
    class DB;
}

class LevelDBStorage :public DBStorage {
public:
    void createTable(const std::string& tableName, const std::string& key, const std::string& value) override;
    std::unique_ptr<AriaORM::ResultsIterator> selectDB(const std::string& key, const std::string& table) override;
    bool getTableStructure(const std::string& table, std::map<std::string, uint>& tableStructure) override;

    void removeWriteSet(const std::string& key, const std::string& table) override;
    void updateWriteSet(const std::string& key, const std::string& value, const std::string& table) override;

protected:
    leveldb::DB* getTable(const std::string& table);

private:
    HashMap<std::string, std::shared_ptr<leveldb::DB>> db;
    std::mutex mutex;
};


#endif //NEUBLOCKCHAIN_LEVEL_DB_STORAGE_H
