//
// Created by peng on 2021/1/15.
//

#pragma once

#include <atomic>
#include <memory>
#include "common/aria_types.h"

class Workload;
class Transaction;

enum class AriaGlobalState;

class Coordinator {
public:
    explicit Coordinator(epoch_size_t currentEpoch) : finishSignal(false), currentEpoch(currentEpoch){};
    virtual ~Coordinator() = default;
    virtual void run() = 0; //start a coordinator sync or async

    inline void stop() { finishSignal = true; }

    //sync call to add workload, async process it.
    virtual size_t addTransaction(std::unique_ptr<std::vector<Transaction*>> trWrapper) = 0;
    std::function<void(epoch_size_t)> epochTransactionFinishSignal;  //async callback to trManager

protected:
    std::atomic<bool> finishSignal;
    epoch_size_t currentEpoch;   //the epoch message for coordinator and worker, not for transactionManager
};

