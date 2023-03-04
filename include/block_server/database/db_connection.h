//
// Created by peng on 2021/3/13.
//

#ifndef NEUBLOCKCHAIN_DB_CONNECTION_H
#define NEUBLOCKCHAIN_DB_CONNECTION_H

#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include <functional>

#define CONN_POOL_INIT_SIZE 50

namespace pqxx{
    class connection;
    class result;
}

class DBConnection {
protected:
    DBConnection();
    virtual ~DBConnection();

    bool executeTransaction(const std::string& sql, pqxx::result* result = nullptr);
    bool executeTransaction(const std::function<std::string(pqxx::connection*)>& callback, pqxx::result* result = nullptr);

private:
    static pqxx::connection* createConnection();
    pqxx::connection* getConnection();
    void restoreConnection(pqxx::connection*);

private:
    moodycamel::BlockingConcurrentQueue<pqxx::connection*> connPool;
};


#endif //NEUBLOCKCHAIN_DB_CONNECTION_H
