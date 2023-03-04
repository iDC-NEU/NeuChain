//
// Created by peng on 2021/1/19.
//

#ifndef NEUBLOCKCHAIN_RESERVE_TABLE_H
#define NEUBLOCKCHAIN_RESERVE_TABLE_H

#include <memory>
#include <vector>
#include "common/aria_types.h"

class KVRWSet;
class TransactionDependency;

class ReserveTable {
public:
    explicit ReserveTable(epoch_size_t _epoch) :epoch(_epoch) {}
    virtual ~ReserveTable() = default;

    [[nodiscard]] epoch_size_t getEpoch() const { return epoch; }
    virtual bool reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) = 0;
    virtual TransactionDependency dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) = 0;

private:
    epoch_size_t epoch;
};

class ReserveTableProvider {
public:
    static ReserveTableProvider* getHandle();
    ReserveTable* getReserveTable(epoch_size_t epoch);

protected:
    ReserveTable* getHashMapReserveTable(epoch_size_t epoch);
    ReserveTable* getMVCCReserveTable(epoch_size_t epoch);

private:
    std::unique_ptr<ReserveTable> reserveTable;
    std::unique_ptr<ReserveTable> mvccReserveTable;
};

#endif //NEUBLOCKCHAIN_RESERVE_TABLE_H
