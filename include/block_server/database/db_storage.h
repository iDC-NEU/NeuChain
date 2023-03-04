//
// Created by peng on 2021/3/9.
//

#ifndef NEUBLOCKCHAIN_DB_STORAGE_H
#define NEUBLOCKCHAIN_DB_STORAGE_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "common/aria_types.h"

namespace AriaORM {
    class ORMTableDefinition;
    class ResultsIterator;
}

class KVRWSet;
class KVRead;

class DBStorage{
public:
    virtual ~DBStorage() = default;
    virtual void createTable(const std::string& tableName, const std::string& key, const std::string& value) = 0;
    virtual std::unique_ptr<AriaORM::ResultsIterator> selectDB(const std::string& key, const std::string& table) = 0;
    virtual bool getTableStructure(const std::string& table, std::map<std::string, uint>& tableStructure) = 0;

    virtual void removeWriteSet(const std::string& key, const std::string& table) = 0;
    virtual void updateWriteSet(const std::string& key, const std::string& value, const std::string& table) = 0;

    // this func is impl in database impl
    std::function<bool(tid_size_t tid, KVRWSet* kvRWSet,
                       std::vector<std::shared_ptr<AriaORM::ORMTableDefinition>> tableList)> bufferUpdates;
};
#endif //NEUBLOCKCHAIN_DB_STORAGE_H
