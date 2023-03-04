//
// Created by peng on 5/18/21.
//

#ifndef NEUBLOCKCHAIN_RESERVE_TABLE_PROVIDER_TEST_H
#define NEUBLOCKCHAIN_RESERVE_TABLE_PROVIDER_TEST_H

#include <gtest/gtest.h>
#include "block_server/reserve_table/reserve_table.h"

class ReserveTableProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        reserveTableProvider = ReserveTableProvider::getHandle();
        reserveTable = reserveTableProvider->getReserveTable(initEpoch);
    }
    ReserveTableProvider* reserveTableProvider;
    ReserveTable* reserveTable;
    epoch_size_t initEpoch = 10;
};


#endif //NEUBLOCKCHAIN_RESERVE_TABLE_PROVIDER_TEST_H
