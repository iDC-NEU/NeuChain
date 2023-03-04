//
// Created by peng on 2021/3/9.
//

#ifndef NEUBLOCKCHAIN_RESULTS_ITERATOR_IMPL_H
#define NEUBLOCKCHAIN_RESULTS_ITERATOR_IMPL_H

#include "block_server/database/orm/query.h"
#include <pqxx/pqxx>
#include <set>

class KVRWSet;

namespace AriaORM {

    class ResultsIteratorImpl : virtual public ResultsIterator {
    public:
        explicit ResultsIteratorImpl(const pqxx::result& _result);

        void init(KVRWSet* _kvRWSet, const std::string& _table) override { kvRWSet = _kvRWSet; table = _table; }

        bool next() override;
        bool hasNext() override;

        int getInt(const std::string& attr) override;
        std::string getString(const std::string& attr) override;

    private:
        std::set<std::string> readKeys;
        std::string table;
        int index = 0;
        KVRWSet* kvRWSet{};
        pqxx::result result;
    };
}

#endif //NEUBLOCKCHAIN_RESULTS_ITERATOR_IMPL_H
