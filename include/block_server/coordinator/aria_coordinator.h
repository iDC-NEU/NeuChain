//
// Created by peng on 2021/1/15.
//

#ifndef NEUBLOCKCHAIN_ARIA_COORDINATOR_H
#define NEUBLOCKCHAIN_ARIA_COORDINATOR_H

#include <map>
#include <set>
#include <queue>
#include <functional>
#include <common/consume_time_calculator.h>
#include "coordinator.h"
#include "common/cv_wrapper.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"

class Worker;
class WorkerInstance;
enum class WorkerState;
enum class AriaGlobalState;

class WorkloadSizeAdapter;

class Transaction;

// use case: one consumer, one producer
class TransactionQueueWrapper {
public:
    TransactionQueueWrapper()
            : _front(nullptr) { }

    // block wait
    [[nodiscard]] Transaction* front() {
        if(_front == nullptr) {
            _queue.wait_dequeue(_front);
        }
        return _front;
    }

    void push(Transaction* value) {
        _queue.enqueue(value);
    }

    void push_bulk(const std::vector<Transaction*>& bulk) {
        _queue.enqueue_bulk(bulk.data(), bulk.size());
    }

    // if front, pop front;
    // else, pop last elem in the queue
    Transaction* pop() {
        Transaction* value;
        if (_front) {
            value = _front;
            _front = nullptr;
        } else {
            _queue.wait_dequeue(value);
        }
        return value;
    }

    bool empty() {
        if (_front) {
            return false;
        }
        return !_queue.try_dequeue(_front);
    }


private:
    moodycamel::BlockingConcurrentQueue<Transaction*> _queue;
    Transaction* _front;
};

class AriaCoordinator: public Coordinator {
public:
    explicit AriaCoordinator(epoch_size_t startupEpoch);
    ~AriaCoordinator() override;

    void run() override;
    size_t addTransaction(std::unique_ptr<std::vector<Transaction*>> trWrapper) override;

protected:
    // make sure that all tx.epoch=i has assigned to a workload before return.
    void assignWorkloadPhase();
    // return after all workers have finished phase 1.
    virtual void readPhase();
    // return after all workers have finished phase 2.
    void commitPhase();

    // return nullptr when epochChan=i has no pending tx.
    // do not return empty workload.
    std::unique_ptr<Workload> getWorkloadFromBuffer();
    // return nullptr when epoch=i is finished, otherwise block.
    virtual std::unique_ptr<Workload> createWorkload();
    // wrapper for creating a new worker.
    virtual std::unique_ptr<Worker> createWorker(WorkerInstance* workerInstance);

    // assign a workload to a worker, return the worker id, return 0 if failed.
    size_t deployWorkload(std::unique_ptr<Workload> workload);
    // assign a workload by creating a new worker, return the worker id, return 0 if failed.
    size_t deployWorkloadWithNewWorker(std::unique_ptr<Workload> workload);

    // broadcast this.globalState signal to all workers, async return.
    void broadcastToAllWorkers(AriaGlobalState globalState, bool onlyAssigned = true);
    // wait until all worker has emit a state which in state set.
    void syncAllWorker(const std::set<WorkerState>& state, bool onlyAssigned = true);

    size_t destroyUnusedWorkers();
    void printWorkerCondition() const;

protected:
    std::map<size_t, WorkerInstance*> workers;
    // the biggest worker instance, if = 0, no worker is inited
    size_t workerIDNonce;

    // dynamic change workload size base on tx count per epoch
    std::unique_ptr<WorkloadSizeAdapter> workloadSizeAdapter;

    //std::queue<Transaction*> transactionBuffer;
    CVWrapper txBufferCV;
    TransactionQueueWrapper queueWrapper;

    ConsumeTimeCalculator consumeTimeCalculator;
};



#endif //NEUBLOCKCHAIN_ARIA_COORDINATOR_H
