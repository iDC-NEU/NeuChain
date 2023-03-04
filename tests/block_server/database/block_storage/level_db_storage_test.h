//
// Created by peng on 5/18/21.
//

#ifndef NEUBLOCKCHAIN_LEVEL_DB_STORAGE_TEST_H
#define NEUBLOCKCHAIN_LEVEL_DB_STORAGE_TEST_H

#include <gtest/gtest.h>
#include "block_server/database/impl/level_db_storage.h"

class LevelDBStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage = new LevelDBStorage;
    }
    void TearDown() override {
        delete storage;
    }
    LevelDBStorage* storage;
};


#endif //NEUBLOCKCHAIN_LEVEL_DB_STORAGE_TEST_H
