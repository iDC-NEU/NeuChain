//
// Created by peng on 2021/3/26.
//

#ifndef NEUBLOCKCHAIN_LEVEL_DB_RESULTS_ITERATOR_H
#define NEUBLOCKCHAIN_LEVEL_DB_RESULTS_ITERATOR_H

#include "block_server/database/orm/query.h"
#include <pqxx/pqxx>
#include <set>

class KVRWSet;

namespace AriaORM {

    class LevelDBResultsIterator : virtual public ResultsIterator {
    public:
        explicit LevelDBResultsIterator(std::string _key, std::string _value)
                :key(std::move(_key)), value(std::move(_value)) { }
        ~LevelDBResultsIterator() override = default;

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
        const std::string key;
        const std::string value;
    };
}

#endif //NEUBLOCKCHAIN_LEVEL_DB_RESULTS_ITERATOR_H
