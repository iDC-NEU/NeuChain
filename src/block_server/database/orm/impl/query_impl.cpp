//
// Created by peng on 2021/3/7.
//

#include "block_server/database/orm/impl/query_impl.h"
#include "block_server/database/orm/impl/results_iterator_impl.h"

#include "block_server/database/db_storage.h"

AriaORM::QueryImpl::QueryImpl(std::string _table, DBStorage* _storage, KVRWSet* _kvRWSet)
    :table(std::move(_table)), storage(_storage), kvRWSet(_kvRWSet) { }

AriaORM::QueryImpl::~QueryImpl() = default;

void AriaORM::QueryImpl::filter(const std::string &attr, AriaORM::NumFilter range, int arg1, int arg2) {

}

void AriaORM::QueryImpl::filter(const std::string &attr, AriaORM::NumFilter range, double arg1, double arg2) {

}

void AriaORM::QueryImpl::filter(const std::string &attr, AriaORM::StrFilter range, const std::string &arg) {
    this->col = arg;
}

void AriaORM::QueryImpl::orderBy(std::vector<std::string> orderAttr) {

}

std::string AriaORM::QueryImpl::raw() const {
    return std::string();
}

AriaORM::ResultsIterator *AriaORM::QueryImpl::executeQuery(const std::string &query) {
    return nullptr;
}

AriaORM::ResultsIterator *AriaORM::QueryImpl::executeQuery() {
    iter = storage->selectDB(col, table);
    iter->init(kvRWSet, table);
    return iter.get();
}
