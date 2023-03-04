//
// Created by peng on 2021/3/10.
//

#ifndef NEUBLOCKCHAIN_INSERT_IMPL_H
#define NEUBLOCKCHAIN_INSERT_IMPL_H

#include "block_server/database/orm/insert.h"

class DBStorage;
class KVRWSet;

namespace AriaORM {
    class InsertImpl : public ORMInsert {
    public:
        InsertImpl(std::string _table, DBStorage* _storage, KVRWSet* _kvRWSet);
        ~InsertImpl() override;

        bool set(const std::string& attr, const std::string& value) override;
        bool set(const std::string& attr, int value) override;

    protected:
        void execute();

    protected:
        std::string table;
        DBStorage* storage;
        KVRWSet* kvRWSet;
        std::string key;

    };
}

#endif //NEUBLOCKCHAIN_INSERT_IMPL_H
