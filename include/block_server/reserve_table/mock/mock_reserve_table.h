//
// Created by peng on 2021/1/19.
//

#ifndef NEUBLOCKCHAIN_MOCKRESERVETABLE_H
#define NEUBLOCKCHAIN_MOCKRESERVETABLE_H

#include "block_server/reserve_table/reserve_table.h"
#include "common/hash_map.h"

#include <atomic>
#include <string>

class MockReserveTable: public ReserveTable {
public:
    explicit MockReserveTable(epoch_size_t _epoch):ReserveTable(_epoch) {}
    bool reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) override;

    TransactionDependency dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) override;

private:
    HashMap<std::string, HashMap<std::string, std::atomic<tid_size_t>>> readTableList;
    HashMap<std::string, HashMap<std::string, std::atomic<tid_size_t>>> writeTableList;
};


#endif //NEUBLOCKCHAIN_MOCKRESERVETABLE_H
