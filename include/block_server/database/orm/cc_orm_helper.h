//
// Created by peng on 3/2/21.
//

#ifndef NEUBLOCKCHAIN_ORM_HELPER_H
#define NEUBLOCKCHAIN_ORM_HELPER_H

#include <vector>
#include <string>
#include <memory>
#include "common/aria_types.h"

class DBStorage;
class KVRWSet;

namespace AriaORM {
    class ORMQuery;
    class ORMInsert;
    class ORMTableDefinition;

    class CCORMHelper {
    public:
        CCORMHelper(tid_size_t _tid, KVRWSet* kvRWSet, bool readOnly);

        ~CCORMHelper();

        KVRWSet* getKV_RWSet() { return kvRWSet; }

        AriaORM::ORMTableDefinition* createTable(const std::string& tableName);

        AriaORM::ORMQuery* newQuery(const std::string& tableName);

        AriaORM::ORMInsert* newInsert(const std::string& tableName);

    protected:
        bool execute();

    private:
        tid_size_t tid;
        bool readOnly;
        DBStorage* storage = nullptr;
        std::vector<std::unique_ptr<AriaORM::ORMQuery>> queryList;
        std::vector<std::unique_ptr<AriaORM::ORMInsert>> insertList;
        std::vector<std::shared_ptr<AriaORM::ORMTableDefinition>> tableList;
        KVRWSet* kvRWSet;
    };
}
#endif //NEUBLOCKCHAIN_ORM_HELPER_H
