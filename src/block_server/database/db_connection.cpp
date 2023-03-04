//
// Created by peng on 2021/3/13.
//

#include "block_server/database/db_connection.h"
#include <pqxx/pqxx>
#include "glog/logging.h"

pqxx::connection* DBConnection::createConnection() {
    try{
        auto* pConnection = new pqxx::connection("dbname=postgres user=postgres password=123456 hostaddr=127.0.0.1 port=5432");
        if (!pConnection->is_open()){
            LOG(WARNING) << "Can't open database";
        }
        return pConnection;
    } catch (const std::exception& e){
        LOG(ERROR) << e.what();
    }
    CHECK(false);
    return nullptr;
}

pqxx::connection *DBConnection::getConnection() {
    pqxx::connection* pConnection;
    connPool.wait_dequeue(pConnection);
    //if(!connPool.try_dequeue(pConnection)) {
    //    return createConnection();
    //}
    return pConnection;
}

void DBConnection::restoreConnection(pqxx::connection* pConnection) {
    connPool.enqueue(pConnection);
    // LOG(INFO) << "restore: " << pConnection->dbname();
}

DBConnection::DBConnection() {
    DCHECK(connPool.is_lock_free());
    for (int i=0; i<CONN_POOL_INIT_SIZE; i++) {
        restoreConnection(createConnection());
    }
}

DBConnection::~DBConnection() {
    try{
        pqxx::connection* pConnection;
        while (connPool.try_dequeue(pConnection)) {
            pConnection->close();
        }
    } catch (const std::exception& e){
        LOG(ERROR) << e.what();
    }
}

bool DBConnection::executeTransaction(const std::string& sql, pqxx::result* result) {
    return executeTransaction([=](pqxx::connection * connection) -> std::string {
        return sql;
        }, result);
}

bool DBConnection::executeTransaction(const std::function<std::string(pqxx::connection *)>& callback, pqxx::result *result) {
    pqxx::connection* pConnection = getConnection();
    try{
        pqxx::work tnx(*pConnection);
        if (result)
            *result = tnx.exec(callback(pConnection));
        else
            tnx.exec(callback(pConnection));
        tnx.commit();
        restoreConnection(pConnection);
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        pConnection->close();
        restoreConnection(createConnection());
    }
    return false;
}
