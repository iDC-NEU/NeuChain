//
// Created by anna on 1/29/21.
//

#include "block_server/database/impl/db_storage_impl.h"
#include <pqxx/pqxx>
#include "block_server/database/orm/impl/results_iterator_impl.h"
#include "glog/logging.h"

void DBStorageImpl::createTable(const std::string& tableName, const std::string& key, const std::string& value){
    std::string sql = "create table " + tableName + "(" + key + " varchar(255) primary key not null," + value + " varchar(255) not null)";
    executeTransaction(sql);
}

//perform simple database query, for code reusing
std::unique_ptr<AriaORM::ResultsIterator> DBStorageImpl::selectDB(const std::string& key, const std::string& table){
    pqxx::result r;
    executeTransaction("select * from " + table + " where key =\'"+key+"\'", &r);
    return std::make_unique<AriaORM::ResultsIteratorImpl>(r);
}

bool DBStorageImpl::getTableStructure(const std::string& table, std::map<std::string, uint>& tableStructure) {
    static std::mutex mutex;
    std::shared_ptr<pqxx::result> result = std::make_shared<pqxx::result>();
    if(!tableCache.tryGet(table, result)) {
        if(!executeTransaction("select * from " + table + " limit 1", result.get())) {
            return false;
        }
        // TODO: is this thread safe need more test
        std::unique_lock<std::mutex> lock(mutex);
        if(!tableCache.tryGet(table, result)) {
            tableCache.insert(table, result);
        }
    }
    tableStructure["key"] = result->column_type("key");  //1043 = string
    tableStructure["value"] = result->column_type("value");
    return true;
}

//if write is a delete
void DBStorageImpl::removeWriteSet(const std::string& key, const std::string& table){
    executeTransaction("UPDATE " + table + " SET key = NULL, value = NULL WHERE key = \'" + key + "\'");
}

//if write is a update
void DBStorageImpl::updateWriteSet(const std::string& key, const std::string& value, const std::string& table){
    executeTransaction("INSERT INTO " + table +
                       " VALUES (\'" + key + "\' , \'" + value + "\')\n" +
                       "ON CONFLICT (key) DO UPDATE SET key =\'" + key + "\' , value =\'" + value + "\'");
}

