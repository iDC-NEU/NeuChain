//
// Created by peng on 2021/1/15.
//

#pragma once

#include <atomic>
#include <memory>
#include "block_server/comm/client_proxy/consensus_control_block.h"
#include "common/aria_types.h"
#include "common/concurrent_queue/light_weight_semaphore.hpp"

class Workload;
class Transaction;

enum class AriaGlobalState;

class Coordinator {
public:
    explicit Coordinator(epoch_size_t startupEpoch) : finishSignal(false), currentEpoch(startupEpoch) {
        ccb = ConsensusControlBlock::getInstance();
        ccb->startService();
    };

    virtual ~Coordinator() = default;

    virtual void run() = 0; //start a coordinator sync or async

    inline void stop() { finishSignal = true; }

    //sync call to add workload, async process it.
    virtual size_t addTransaction(std::unique_ptr<std::vector<Transaction *>> trWrapper) = 0;

    std::function<void(epoch_size_t)> epochTransactionFinishSignal;  //async callback to trManager

protected:
    void waitUntilCurrentEpochFinish() {
        ccb->waitForEpochFinish(currentEpoch);
    }

    bool needReReserve() {
        return false;
        return ccb->needReReserve(currentEpoch);
    }

protected:
    volatile bool finishSignal;
    epoch_size_t currentEpoch;   //the epoch message for coordinator and worker, not for transactionManager

private:
    ConsensusControlBlock* ccb;

};

