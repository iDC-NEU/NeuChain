//
// Created by peng on 2021/1/16.
//

#ifndef NEUBLOCKCHAIN_WORKER_INSTANCE_H
#define NEUBLOCKCHAIN_WORKER_INSTANCE_H

#include <vector>
#include <set>
#include <functional>
#include <thread>
#include "common/aria_types.h"
#include "common/cv_wrapper.h"


class Worker;
class Transaction;
enum class WorkerState;

enum class AriaGlobalState {
    START,  // the init state of aria coordinator
    Aria_READ,      // start the read state of the protocol
    Aria_COMMIT,    // start the commit state of the protocol
    EXIT,            // the coordinator is about to exit
    Aggregate,
};

// READY means no task
// ASSIGNED means processing data
// EXITED means can be free from memory
// WORKER STATE CAN BE SET BY COORDINATOR OR WORKER BOTH!
enum class WorkerState {
    READY = 0,          //worker is initializing or is free
    ASSIGNED = 1,       // worker has init and is ready to process data
    FINISH_READ = 2,    // worker finish read phase
    FINISH_AGGREGATE = 3,
    EXITED = 4,          // worker is about to exit, use join func to safe delete it.
};

class Workload {
public:
    epoch_size_t epoch; //this epoch is not always == transaction.epoch
    std::vector<Transaction*> transactionList;
    // for aggregation
    bool aggregationWorkload = false;
};

class WorkerInstance {
public:
    explicit WorkerInstance(const size_t workerID, const AriaGlobalState& ariaGlobalState)
            : workerID(workerID), workerState{}, ariaGlobalState(ariaGlobalState) { }
    const size_t workerID;
    // the worker thread reference
    std::unique_ptr<std::thread> thread;
    // the worker object reference
    std::unique_ptr<Worker> instance;
    // workload changes every epoch
    std::unique_ptr<Workload> workload;

    // coordinator wait for state
    inline void coordinatorWait(const std::set<WorkerState>& state) {
        producer.wait([&]()->bool {
            return state.count(workerState);
        });
    }

    // worker wait for state
    inline bool workerWait(const AriaGlobalState& state) {
        AriaGlobalState signal;
        consumer.wait([&]()->bool{
            signal = this->ariaGlobalState;
            return signal == state || signal == AriaGlobalState::EXIT;
        });
        if(signal == AriaGlobalState::EXIT) {
            return false;
        }
        return true;
    }

    // worker change state and notice coordinator
    inline void setWorkerState(const WorkerState& state) {
        producer.notify_one([&]() {
            this->workerState = state;
        });
    }

    // coordinator change specific state and notice worker
    inline void setCoordinatorState(const AriaGlobalState& state) {
        consumer.notify_one([&]() {
            ariaGlobalState = state;
        });
    }

    [[nodiscard]] inline WorkerState getWorkerState() const { return workerState; }
    [[nodiscard]] inline AriaGlobalState getAriaGlobalState() const { return ariaGlobalState; }

private:
    WorkerState workerState;
    AriaGlobalState ariaGlobalState;
    CVWrapper producer, consumer;
};

#endif //NEUBLOCKCHAIN_WORKER_INSTANCE_H
