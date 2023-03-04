//
// Created by peng on 2021/1/23.
//

#ifndef NEUBLOCKCHAIN_MOCKMVCCRESERVETABLE_H
#define NEUBLOCKCHAIN_MOCKMVCCRESERVETABLE_H

#include "block_server/reserve_table/reserve_table.h"
#include "common/mvcc_hash_map.h"

#include <atomic>

using std::string;

class MockMVCCReserveTable: public ReserveTable {
public:
    [[deprecated("MVCC reserve table currently has bug and not recommend to use.")]]
    explicit MockMVCCReserveTable(epoch_size_t _epoch): ReserveTable(_epoch) {}
    bool reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) override;

    TransactionDependency dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) override;

private:
    MVCCHashMap<997, std::string, std::atomic<tid_size_t>> readTable;
    MVCCHashMap<997, std::string, std::atomic<tid_size_t>> writeTable;
};


#endif //NEUBLOCKCHAIN_MOCKMVCCRESERVETABLE_H
