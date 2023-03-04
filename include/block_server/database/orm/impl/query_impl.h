//
// Created by peng on 2021/3/7.
//

#ifndef NEUBLOCKCHAIN_QUERY_IMPL_H
#define NEUBLOCKCHAIN_QUERY_IMPL_H

#include "block_server/database/orm/query.h"
#include "block_server/database/database.h"

class DBStorage;
class KVRWSet;
class ResultsIterator;

namespace AriaORM {
    class QueryImpl : public ORMQuery {
    public:
        // call by cc_orm_helper
        QueryImpl(std::string _table, DBStorage* _storage, KVRWSet* _kvRWSet);

        ~QueryImpl() override;

        // int filter
        void filter(const std::string &attr, NumFilter range, int arg1, int arg2) override;

        // float filter
        void filter(const std::string &attr, NumFilter range, double arg1, double arg2) override;

        // str filter
        void filter(const std::string &attr, StrFilter range, const std::string &arg) override;

        // order by
        void orderBy(std::vector<std::string> orderAttr) override;

        // get raw query string
        [[nodiscard]] std::string raw() const override;

        // ExecuteQuery executes the given query and returns an iterator that contains results of type *VersionedKV.
        ResultsIterator* executeQuery(const std::string &query) override;    //ResultsIterator = pqcc::result + cc helper + result iterator

        ResultsIterator* executeQuery() override;

    private:
        std::string table;
        DBStorage* storage;
        KVRWSet* kvRWSet;
        std::string col{};
        std::unique_ptr<AriaORM::ResultsIterator> iter;
    };
}


#endif //NEUBLOCKCHAIN_QUERY_IMPL_H
