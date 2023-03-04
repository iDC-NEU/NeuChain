//
// Created by peng on 3/31/22.
//

#ifndef NEU_BLOCKCHAIN_EPOCH_TXN_QUEUE_MANAGER_H
#define NEU_BLOCKCHAIN_EPOCH_TXN_QUEUE_MANAGER_H

#include "common/cyclic_barrier.h"
#include <memory>

class EpochTxnQueueManager {
public:
    explicit EpochTxnQueueManager(epoch_size_t startWithEpoch, size_t blkSrvCnt)
            : _blkSrvCnt(blkSrvCnt), globalProcessingEpoch(startWithEpoch), currentEpochPushedToQueueCnt(_blkSrvCnt),
              barrier(_blkSrvCnt, [this]() {
                  epoch_size_t epoch = this->globalProcessingEpoch.load(std::memory_order_relaxed);
                  LOG(INFO) << "All threads reached barrier, epoch: " << epoch << ", count: " << this->barrier.get_current_waiting();
                  this->currentEpochPushedToQueueCnt.store(0, std::memory_order_release);
                  this->globalProcessingEpoch.fetch_add(1, std::memory_order_release);
              }) {

    }

    void waitForAllCPRecvTxn(epoch_size_t waitingEpoch) {
        // waitingEpoch is useless, because all proxies sync after an epoch
        barrier.await();
    }

    void signalFinishedPushToQueue(epoch_size_t finishedEpoch) {
        // called by worker
        currentEpochPushedToQueueCnt.fetch_add(1, std::memory_order_release);
    }

    bool isAllCPPushedToQueue(epoch_size_t waitingEpoch) {
        epoch_size_t finishedLatestEpoch = this->globalProcessingEpoch.load(std::memory_order_acquire);
        // 1. waitingEpoch-1 is not fully collected, must not push waitingEpoch to queue
        if (waitingEpoch > finishedLatestEpoch)
            return false;
        CHECK(waitingEpoch == finishedLatestEpoch);
        // 2. not all client proxies push waitingEpoch-1 in the queue, must wait
        if (currentEpochPushedToQueueCnt.load(std::memory_order_acquire) < _blkSrvCnt)
            return false;
        return true;
    }

private:
    // for multi blk server
    const size_t _blkSrvCnt;
    std::atomic<epoch_size_t> globalProcessingEpoch;
    std::atomic<epoch_size_t> currentEpochPushedToQueueCnt;
    cbar::CyclicBarrier barrier;
};

#endif //NEU_BLOCKCHAIN_EPOCH_TXN_QUEUE_MANAGER_H
