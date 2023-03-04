//
// Created by anna on 1/29/21.
//

#ifndef NEUBLOCKCHAIN_DBSTORAGE_H
#define NEUBLOCKCHAIN_DBSTORAGE_H

#include "block_server/database/db_storage.h"
#include "block_server/database/db_connection.h"
#include "common/lru_cache.hpp"
#include <map>

class DBStorageImpl: virtual public DBStorage, virtual private DBConnection{
public:
    void createTable(const std::string& tableName, const std::string& key, const std::string& value) override;
    bool getTableStructure(const std::string& table, std::map<std::string, uint>& tableStructure) override;
    std::unique_ptr<AriaORM::ResultsIterator> selectDB(const std::string& key, const std::string& table) override;

    void removeWriteSet(const std::string& key, const std::string& table) override;
    void updateWriteSet(const std::string& key, const std::string& value, const std::string& table) override;

private:
    lru11::Cache<std::string, std::shared_ptr<pqxx::result>, std::mutex> tableCache;
};

#endif //NEUBLOCKCHAIN_DBSTORAGE_H
