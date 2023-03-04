//
// Created by peng on 2021/1/19.
//

#ifndef NEUBLOCKCHAIN_DATABASE_H
#define NEUBLOCKCHAIN_DATABASE_H

#include <cstdint>
#include "common/aria_types.h"

#define ARIA_SYS_TABLE "aria_sys_table"

class KVRWSet;
class DBStorage;

// VersionedDB lists methods that a db is supposed to implement
class VersionedDB {
public:
    static VersionedDB* getDBInstance();

    virtual ~VersionedDB() = default;

    virtual DBStorage* getStorage() = 0;

    virtual bool commitUpdate(tid_size_t tid) = 0;

    virtual void abortUpdate(tid_size_t tid) = 0;
};

#endif //NEUBLOCKCHAIN_DATABASE_H
