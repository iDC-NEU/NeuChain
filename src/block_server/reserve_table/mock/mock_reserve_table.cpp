//
// Created by peng on 2021/1/19.
//

#include "block_server/reserve_table/mock/mock_reserve_table.h"
#include "block_server/reserve_table/trasnaction_dependency.h"
#include "kv_rwset.pb.h"

bool MockReserveTable::reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) { // reentry
    for(const auto& read : kvRWSet->reads()) {
        auto& readTable = readTableList[read.table()];
        auto& reg = readTable[read.key()];
        auto old_rts = reg.load(std::memory_order_relaxed);
        do {
            if (old_rts <= transactionID && old_rts != 0) {
                break;
            }
        } while (!reg.compare_exchange_weak(old_rts, transactionID, std::memory_order_release, std::memory_order_relaxed));
    }

    for(const auto& write : kvRWSet->writes()) {
        auto& writeTable = writeTableList[write.table()];
        auto& reg = writeTable[write.key()];
        auto old_wts = reg.load(std::memory_order_relaxed);
        do {
            if (old_wts <= transactionID && old_wts != 0) {
                break;
            }
        } while (!reg.compare_exchange_weak(old_wts, transactionID, std::memory_order_release, std::memory_order_relaxed));
    }
    return true;
}

TransactionDependency MockReserveTable::dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) { // reentry
    TransactionDependency trDependency;
    for(const auto& write : kvRWSet->writes()) {
        auto& writeTable = writeTableList[write.table()];
        auto old_wts = writeTable[write.key()].load(std::memory_order_relaxed);

        if (old_wts < transactionID && old_wts != 0) {  // waw dependency
            trDependency.waw = true;
            break;
        }
    }
    for(const auto& write : kvRWSet->writes()) {
        auto& readTable = readTableList[write.table()];
        auto old_rts = readTable[write.key()].load(std::memory_order_relaxed);

        if (old_rts < transactionID && old_rts != 0) {  // war dependency
            trDependency.war = true;
            break;
        }
    }
    for(const auto& read : kvRWSet->reads()) {
        auto& writeTable = writeTableList[read.table()];
        auto old_wts = writeTable[read.key()].load(std::memory_order_relaxed);

        if (old_wts < transactionID && old_wts != 0) {  // raw dependency
            trDependency.raw = true;
            break;
        }
    }
    return trDependency;
}
