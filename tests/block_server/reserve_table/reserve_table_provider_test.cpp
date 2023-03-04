//
// Created by peng on 5/18/21.
//

#include "reserve_table_provider_test.h"

namespace {
    TEST_F(ReserveTableProviderTest, GetHandle) { /* NOLINT */
        EXPECT_EQ(reserveTableProvider, ReserveTableProvider::getHandle());
    }
    TEST_F(ReserveTableProviderTest, getReserveTable) { /* NOLINT */
        reserveTable = reserveTableProvider->getReserveTable(initEpoch);
        // same sequence call should get the same reserve table.
        EXPECT_EQ(reserveTable, reserveTableProvider->getReserveTable(initEpoch));
        EXPECT_EQ(reserveTable->getEpoch(), initEpoch);
        // update epoch, get new reserve table.
        auto reserveTableNew = reserveTableProvider->getReserveTable(initEpoch + 1);
        EXPECT_EQ(reserveTableNew->getEpoch(), initEpoch + 1);
    }
}

