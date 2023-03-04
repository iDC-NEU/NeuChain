//
// Created by peng on 2021/3/26.
//

#include "block_server/database/orm/impl/level_db_results_iterator.h"
#include "kv_rwset.pb.h"

bool AriaORM::LevelDBResultsIterator::next() {
    if(hasNext())
        return ++index;
    return false;
}

bool AriaORM::LevelDBResultsIterator::hasNext() {
    return 1 - index;
}

int AriaORM::LevelDBResultsIterator::getInt(const std::string& attr) {
    return 0;
}

std::string AriaORM::LevelDBResultsIterator::getString(const std::string& attr) {
    if(readKeys.count(key) == 0 && kvRWSet) {
        // prevent for get the same key
        KVRead* read = kvRWSet->add_reads();
        read->set_table(table);
        // notice: this key represent the value of the given attribute
        read->set_key(key);
        read->set_value(value);
        readKeys.insert(key);
    }
    if(attr == "key") {
        return key;
    }
    return value;
}
