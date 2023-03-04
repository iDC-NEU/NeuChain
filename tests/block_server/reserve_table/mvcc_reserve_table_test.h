//
// Created by peng on 5/17/21.
//

#ifndef NEUBLOCKCHAIN_MVCC_RESERVE_TABLE_TEST_H
#define NEUBLOCKCHAIN_MVCC_RESERVE_TABLE_TEST_H

#include <gtest/gtest.h>
#include "block_server/reserve_table/mock/mock_mvcc_reserve_table.h"
#include "block_server/reserve_table/trasnaction_dependency.h"
#include "kv_rwset.pb.h"

class MVCCReserveTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        reserveTable = new MockMVCCReserveTable(1);
        auto makeRWSet = [] (const std::vector<std::string>& reads, const std::vector<std::string>& writes) -> KVRWSet {
            KVRWSet kvRWSet;
            for(const auto& key: reads) {
                auto* read = kvRWSet.add_reads();
                read->set_key(key);
                read->set_table("test_table_0");
            }
            for(const auto& key: writes) {
                auto* write = kvRWSet.add_writes();
                write->set_key(key);
                write->set_table("test_table_0");
            }
            return kvRWSet;
        };
        // -------------test case for epoch=0 starts here------------------
        // blurring the lines between bc & dbs: p112 test case
        // abort: 4, 5
        // commit: 1, 2, 3, 6
        rwSetList.emplace_back(std::make_pair(1, makeRWSet({"0", "1"}, {"2"})));
        rwSetList.emplace_back(std::make_pair(2, makeRWSet({"3", "4", "5"}, {"0"})));
        rwSetList.emplace_back(std::make_pair(3, makeRWSet({"6", "7"}, {"3", "9"})));
        rwSetList.emplace_back(std::make_pair(4, makeRWSet({"2", "8"}, {"1", "4"})));
        rwSetList.emplace_back(std::make_pair(5, makeRWSet({"9"}, {"5", "6", "8"})));
        rwSetList.emplace_back(std::make_pair(6, makeRWSet({}, {"7"})));
        // for waw test
        rwSetList.emplace_back(std::make_pair(7, makeRWSet({"3"}, {"6"})));
    }

    void TearDown() override {
        delete reserveTable;
    }

    ReserveTable* reserveTable;
    std::vector<std::pair<tid_size_t, KVRWSet>> rwSetList;
};

#endif //NEUBLOCKCHAIN_MVCC_RESERVE_TABLE_TEST_H
