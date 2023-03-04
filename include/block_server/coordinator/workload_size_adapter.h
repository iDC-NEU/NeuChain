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
        cacheCountList.resize(cacheSize);
        for (auto& cnt : cacheCountList) {
            cnt = 3000;
        }

        // half is needed to generate block
        coreCount = sysconf(_SC_NPROCESSORS_ONLN) / 2 + 1;
        minTxCount = recommendMinTx;
        maxTxCount = recommendMaxTx;
    }

    inline void setAggregateServerCount(size_t _aggregateServerCount) {
        aggregateServerCount = _aggregateServerCount;
    }

    // this func must be called by only one producer.
    inline void setLastEpochTxCount(epoch_size_t epoch, size_t count) {
        if(cacheEpoch == epoch) {
            cacheCountList[epoch % cacheSize] += count;
        } else {
            // last epoch is empty
            CHECK(cacheEpoch+1 == epoch);
            DLOG(INFO) << "lastEpoch: " << cacheEpoch << ", txn size: " << cacheCountList[epoch % cacheSize];
            calculateBestWorkloadSize();
            cacheEpoch = epoch;
            cacheCountList[epoch % cacheSize] = count;
        }
    }

    [[nodiscard]] inline size_t getMinTxPerWorkload() const {
        return minTxCount;
    }

    [[nodiscard]] inline size_t getMaxTxPerWorkload() const {
        return maxTxCount;
    }

    [[nodiscard]] inline size_t getMaxBufferSize() const {
        return recommendMaxBuffer;
    }

protected:
    inline void calculateBestWorkloadSize() {
        double averageCnt = 0;
        int zeros = 0;
        for (auto& cnt : cacheCountList) {
            if (cnt == 0) {
                zeros++;
            }
            averageCnt += double(cnt);
        }
        averageCnt /= cacheSize-zeros;

        // cpu count * factor = worker count
        auto minFactor = averageCnt / (static_cast<double>(aggregateServerCount * coreCount * recommendMaxTx)) + 1;
        auto maxFactor = averageCnt / (static_cast<double>(aggregateServerCount * coreCount * recommendMinTx)) + 1;
        auto minTxTmpCount = averageCnt / (coreCount * maxFactor);
        auto maxTxTmpCount = averageCnt / (coreCount * minFactor);
        minTxTmpCount = minTxTmpCount < minTxThreadHold ? minTxThreadHold : minTxTmpCount;
        minTxCount = minTxTmpCount;
        maxTxCount = maxTxTmpCount < minTxTmpCount ? minTxTmpCount : maxTxTmpCount;
    }

private:
    // for cache value from setLastEpochTxCount
    epoch_size_t cacheEpoch;
    const int cacheSize = 5;
    std::vector<size_t> cacheCountList;

private:
    size_t aggregateServerCount;
    size_t coreCount;
    time_t lastEpochDuration{};
    volatile size_t maxTxCount{};
    volatile size_t minTxCount{};

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
