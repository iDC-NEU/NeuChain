//
// Created by peng on 2021/3/9.
//

#include "block_server/database/orm/impl/results_iterator_impl.h"
#include "kv_rwset.pb.h"
#include <pqxx/pqxx>

AriaORM::ResultsIteratorImpl::ResultsIteratorImpl(const pqxx::result& _result)
    :result(_result) { }

bool AriaORM::ResultsIteratorImpl::next() {
    if(hasNext())
        return ++index;
    return false;
}

bool AriaORM::ResultsIteratorImpl::hasNext() {
    return std::size(result) - index;
}

int AriaORM::ResultsIteratorImpl::getInt(const std::string& attr) {
    return 0;
}

std::string AriaORM::ResultsIteratorImpl::getString(const std::string& attr) {
    pqxx::row const row = result[index];
    auto valueOfKey = row[0].as<std::string>();
    if(readKeys.count(valueOfKey) == 0) {
        // prevent re-call for get the same key
        // TODO: the buffer need to be primary key.
        KVRead* read = kvRWSet->add_reads();
        read->set_table(table);
        // notice: this key represent the value of the given attribute
        read->set_key(valueOfKey);
        readKeys.insert(valueOfKey);
    }
    return row[row.column_number(attr)].as<std::string>();
}
