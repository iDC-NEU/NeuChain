//
// Created by peng on 2/20/21.
//

#ifndef NEUBLOCKCHAIN_DATABASE_IMPL_H
#define NEUBLOCKCHAIN_DATABASE_IMPL_H

#include "block_server/database/database.h"
#include "block_server/database/db_storage.h"
#include "common/hash_map.h"
#include <memory>

class ReserveTable;

class DatabaseImpl: virtual public VersionedDB {
public:
    static VersionedDB* createInstance();

    DBStorage* getStorage() override { return storage.get(); }

    bool commitUpdate(tid_size_t tid) override;

    void abortUpdate(tid_size_t tid) override;

protected:
    void clearBufferThreadSafe();

private:
    DatabaseImpl();

private:
    bool commitPhase;
    std::unique_ptr<DBStorage> storage;
    //<tid, rw set reference>
    HashMap<tid_size_t, KVRWSet*> rwSetBuffer;
    //<tid, table list>
    HashMap<tid_size_t, std::vector<std::shared_ptr<AriaORM::ORMTableDefinition>>> tableListBuffer;
};


#endif //NEUBLOCKCHAIN_DATABASE_IMPL_H
