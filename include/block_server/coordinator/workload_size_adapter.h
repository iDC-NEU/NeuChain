//
// Created by peng on 2021/6/17.
//

#ifndef NEUBLOCKCHAIN_WORKLOAD_SIZE_ADAPTER_H
#define NEUBLOCKCHAIN_WORKLOAD_SIZE_ADAPTER_H

#include <atomic>
#include <string>
#include "common/aria_types.h"
#include <unistd.h>

class WorkloadSizeAdapter {
public:
    explicit WorkloadSizeAdapter(size_t _aggregateServerCount=1) {
        aggregateServerCount = _aggregateServerCount;
        // we start it from 1, not 0, to prevent inconsistency
        cacheEpoch = 1;
        cacheCount = 0;

        coreCount = sysconf(_SC_NPROCESSORS_ONLN);
        minTxCount = recommendMinTx;
        maxTxCount = recommendMaxTx;
    }

    inline void setAggregateServerCount(size_t _aggregateServerCount) {
        aggregateServerCount = _aggregateServerCount;
    }

    // this func must be called by only one producer.
    inline void setLastEpochTxCount(epoch_size_t epoch, size_t count) {
        if(cacheEpoch == epoch) {
            cacheCount += count;
        } else {
            calculateBestWorkloadSize();
            cacheEpoch = epoch;
            cacheCount = count;
        }
    }

    [[nodiscard]] inline size_t getMinTxPerWorkload() const {
        return minTxCount.load(std::memory_order_relaxed);
    }

    [[nodiscard]] inline size_t getMaxTxPerWorkload() const {
        return maxTxCount.load(std::memory_order_relaxed);
    }

    [[nodiscard]] inline size_t getMaxBufferSize() const {
        return recommendMaxBuffer;
    }

protected:
    inline void calculateBestWorkloadSize() {
        // cpu count * factor = worker count
        auto minFactor = cacheCount / (static_cast<double>(aggregateServerCount) * coreCount * recommendMaxTx) + 1;
        auto maxFactor = cacheCount / (static_cast<double>(aggregateServerCount) * coreCount * recommendMinTx) + 1;
        auto minTxTmpCount = cacheCount / (coreCount * maxFactor);
        auto maxTxTmpCount = cacheCount / (coreCount * minFactor);
        minTxTmpCount = minTxTmpCount < minTxThreadHold ? minTxThreadHold : minTxTmpCount;
        minTxCount.store(minTxTmpCount, std::memory_order_relaxed);
        maxTxCount.store(maxTxTmpCount < minTxTmpCount ? minTxTmpCount : maxTxTmpCount, std::memory_order_relaxed);
    }

private:
    // for cache value from setLastEpochTxCount
    epoch_size_t cacheEpoch;
    size_t cacheCount;

private:
    size_t aggregateServerCount;
    size_t coreCount;
    time_t lastEpochDuration{};
    std::atomic<size_t> maxTxCount{};
    std::atomic<size_t> minTxCount{};

public:
    // the max trs assigned to a worker, this must <= COORDINATOR_INPUT_MAX_BUFFER_SIZE
    const size_t recommendMaxTx = 1000;
    // the min trs assigned to a worker, this must <= COORDINATOR_MAX_TRANSACTION_PER_WORKLOAD
    const size_t recommendMinTx = 500;
    // the max buffered transaction size of coordinator
    // when current epoch is over or buffer full, coordinator assign the buffer to worker
    const size_t recommendMaxBuffer = 5000;

    // minimum tx count per workload
    const size_t minTxThreadHold = 5;
};


#endif //NEUBLOCKCHAIN_WORKLOAD_SIZE_ADAPTER_H
