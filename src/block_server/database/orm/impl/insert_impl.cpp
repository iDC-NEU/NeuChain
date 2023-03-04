//
// Created by peng on 2021/3/10.
//

#include <glog/logging.h>
#include "block_server/database/orm/impl/insert_impl.h"
#include "block_server/database/db_storage.h"
#include "kv_rwset.pb.h"

AriaORM::InsertImpl::InsertImpl(std::string _table, DBStorage *_storage, KVRWSet *_kvRWSet)
        :table(std::move(_table)), storage(_storage), kvRWSet(_kvRWSet) {
    // std::map<std::string, uint> tableStructure;
    // storage->getTableStructure("test_table", tableStructure);

}

bool AriaORM::InsertImpl::set(const std::string &attr, const std::string &value) {
    if(attr == "key") {
        // buffer the key
        key = value;
        return true;
    }
    if(attr == "value") {
        KVWrite *write = nullptr;
        // TODO:fixed duplicate write keys
        for (int i=0; i < kvRWSet->writes_size(); i++) {
            if(kvRWSet->writes(i).key() == key){
                CHECK(false);
                write = kvRWSet->mutable_writes(i);
                break;
            }
        }
        // if we didnt add writes here before
        if(write == nullptr)
            write = kvRWSet->add_writes();
        write->set_table(table);
        write->set_key(key);
        write->set_value(value);
        //todo: is valid
        return true;
    }
    return false;
}

bool AriaORM::InsertImpl::set(const std::string &attr, int value) {
    return false;
}

void AriaORM::InsertImpl::execute() {

}

AriaORM::InsertImpl::~InsertImpl() {
    execute();
}
