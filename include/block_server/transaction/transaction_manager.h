//
// Created by peng on 2021/1/17.
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_MANAGER_H
#define NEUBLOCKCHAIN_TRANSACTION_MANAGER_H

#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include "common/aria_types.h"

class Transaction;

class TransactionManager{
public:
    explicit TransactionManager(bool categorize = false, bool reusing = false)
            : finishSignal(false), categorizeTransaction(categorize), reusingAbortTransaction(reusing), processingEpoch(0) { }
    virtual ~TransactionManager() = default;

    virtual void run() = 0; //start a TransactionManager sync or async
    void stop() { finishSignal = true; }
    // when collect a set of transaction, sync call it.
    // to implement it, the workload pointer may become null after the call
    // WARNING: THIS FUNC IS NOT THREAD SAFE, CONCURRENT CALL MAY CRASH THE SYSTEM
    std::function<void(std::unique_ptr<std::vector<Transaction*>>)> sendTransactionHandle;

    virtual void epochFinishedCallback(epoch_size_t finishedEpoch) = 0;

    // this function may only be called when initializing the system.
    [[nodiscard]] epoch_size_t getProcessingEpoch() const { return processingEpoch; }

protected:
    volatile bool finishSignal;
    const bool categorizeTransaction;
    const bool reusingAbortTransaction;
    // tx current executing epoch, epoch start from 1
    std::atomic<epoch_size_t> processingEpoch;
};

#endif //NEUBLOCKCHAIN_TRANSACTION_MANAGER_H
