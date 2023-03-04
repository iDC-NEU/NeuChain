//
// Created by peng on 5/17/21.
//

#include <thread>
#include "mvcc_reserve_table_test.h"

namespace {
    TEST_F(MVCCReserveTableTest, GetEpoch) { /* NOLINT */
        EXPECT_EQ(reserveTable->getEpoch(), 1);
    }

    TEST_F(MVCCReserveTableTest, ReserveSequencely) { /* NOLINT */
        for(const auto& pair: rwSetList) {
            reserveTable->reserveRWSet(&pair.second, pair.first);
        }
        // waw, war, raw
        std::vector<TransactionDependency> estResults = {
                {false, false, false},
                {false, true, false},
                {false, true, false},
                {false, true, true},
                {false, true, true},
                {false, true, false},
                {true, true, true},
        };
        for(int i = 0; i < 7; i++) {
            auto&& results = reserveTable->dependencyAnalysis(&rwSetList[i].second, rwSetList[i].first);
            EXPECT_EQ(results.waw, estResults[i].waw);
            EXPECT_EQ(results.war, estResults[i].war);
            EXPECT_EQ(results.raw, estResults[i].raw);
        }
    }

    TEST_F(MVCCReserveTableTest, ReserveConcurrently) { /* NOLINT */
        std::vector<std::thread> threads;
        threads.reserve(10);
        for(int i=0; i < 10; i++) {
            threads.emplace_back([&]() {
                for(const auto& pair: rwSetList) {
                    reserveTable->reserveRWSet(&pair.second, pair.first);
                }
            });
        }
        for(auto& thread: threads){
            thread.join();
        }
        // waw, war, raw
        std::vector<TransactionDependency> estResults = {
                {false, false, false},
                {false, true, false},
                {false, true, false},
                {false, true, true},
                {false, true, true},
                {false, true, false},
                {true, true, true},
        };
        for(int i = 0; i < 7; i++) {
            auto&& results = reserveTable->dependencyAnalysis(&rwSetList[i].second, rwSetList[i].first);
            EXPECT_EQ(results.waw, estResults[i].waw);
            EXPECT_EQ(results.war, estResults[i].war);
            EXPECT_EQ(results.raw, estResults[i].raw);
        }
    }
}

