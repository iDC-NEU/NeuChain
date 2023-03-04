//
// Created by peng on 3/31/21.
//

#ifndef NEUBLOCKCHAIN_HASH_MAP_STORAGE_H
#define NEUBLOCKCHAIN_HASH_MAP_STORAGE_H

#include "block_server/database/db_storage.h"
#include "common/hash_map.h"
#include "common/random.h"

class HashMapStorage :public DBStorage {
public:
    HashMapStorage();
    void createTable(const std::string& tableName, const std::string& key, const std::string& value) override;
    std::unique_ptr<AriaORM::ResultsIterator> selectDB(const std::string& key, const std::string& table) override;
    bool getTableStructure(const std::string& table, std::map<std::string, uint>& tableStructure) override;

    void removeWriteSet(const std::string& key, const std::string& table) override;
    void updateWriteSet(const std::string& key, const std::string& value, const std::string& table) override;

private:
    HashMap<std::string, std::string> db;
    Random random;
};


#endif //NEUBLOCKCHAIN_HASH_MAP_STORAGE_H
