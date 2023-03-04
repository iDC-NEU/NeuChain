//
// Created by peng on 5/18/21.
//

#include "level_db_storage_test.h"
#include "block_server/database/orm/query.h"

namespace {
    TEST_F(LevelDBStorageTest, RemoveWriteSet) { /* NOLINT */
        storage->updateWriteSet("test_key", "test_value", "table_1");
        for(auto iterator = storage->selectDB("test_key", "table_1"); iterator->hasNext(); iterator->next()) {
            EXPECT_EQ(iterator->getString("key"), "test_key");
            EXPECT_EQ(iterator->getString("value"), "test_value");
        }
        storage->removeWriteSet("test_key", "table_1");
        for(auto iterator = storage->selectDB("test_key", "table_1"); iterator->hasNext(); iterator->next()) {
            EXPECT_EQ(iterator->getString("key"), "test_key");
            EXPECT_EQ(iterator->getString("value"), "");
        }
    }

    TEST_F(LevelDBStorageTest, UpdateWriteSet) { /* NOLINT */
        storage->updateWriteSet("test_key", "test_value", "table_1");
        for(auto iterator = storage->selectDB("test_key", "table_1"); iterator->hasNext(); iterator->next()) {
            EXPECT_EQ(iterator->getString("key"), "test_key");
            EXPECT_EQ(iterator->getString("value"), "test_value");
        }
        storage->updateWriteSet("test_key", "test_value_2", "table_1");
        for(auto iterator = storage->selectDB("test_key", "table_1"); iterator->hasNext(); iterator->next()) {
            EXPECT_EQ(iterator->getString("key"), "test_key");
            EXPECT_EQ(iterator->getString("value"), "test_value_2");
        }
    }
}