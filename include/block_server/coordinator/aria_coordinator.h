//
// Created by peng on 2021/1/15.
//

#ifndef NEUBLOCKCHAIN_ARIA_COORDINATOR_H
#define NEUBLOCKCHAIN_ARIA_COORDINATOR_H

#include <map>
#include <set>
#include <queue>
#include <functional>
#include "coordinator.h"
#include "common/cv_wrapper.h"

class Worker;
class WorkerInstance;
enum class WorkerState;
enum class AriaGlobalState;

class WorkloadSizeAdapter;

class Transaction;

class AriaCoordinator: public Coordinator {
public:
    explicit AriaCoordinator(epoch_size_t startWithEpoch);
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

    // this func is not thread safe, and must be called by coordinator thread
    void printWorkerCondition() const;

protected:
    std::map<size_t, WorkerInstance*> workers;
    // the biggest worker instance, if = 0, no worker is inited
    size_t workerIDNonce;

    // dynamic change workload size base on tx count per epoch
    std::unique_ptr<WorkloadSizeAdapter> workloadSizeAdapter;

    std::queue<Transaction*> transactionBuffer;
    CVWrapper txBufferCV;
};



#endif //NEUBLOCKCHAIN_ARIA_COORDINATOR_H
