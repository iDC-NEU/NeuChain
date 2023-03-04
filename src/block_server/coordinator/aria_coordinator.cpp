//
// Created by peng on 2021/1/15.
//

#include "block_server/coordinator/aria_coordinator.h"
#include "block_server/coordinator/workload_size_adapter.h"
#include "block_server/transaction/transaction.h"

#include "block_server/worker/aria_worker.h"
#include "block_server/worker/worker_instance.h"
#include "glog/logging.h"

AriaCoordinator::AriaCoordinator(epoch_size_t currentEpoch) : Coordinator(currentEpoch), workerIDNonce(1) {
    workloadSizeAdapter = std::make_unique<WorkloadSizeAdapter>();
}

AriaCoordinator::~AriaCoordinator() {
    finishSignal = true;
    broadcastToAllWorkers(AriaGlobalState::EXIT, false);
    syncAllWorker({ WorkerState::EXITED }, false);
    LOG(INFO) << destroyUnusedWorkers() << " worker has stopped, exit coordinator." << std::endl;
}

void AriaCoordinator::run() {
    LOG(INFO) << "AriaCoordinator started.";
    while(!finishSignal) {
        assignWorkloadPhase();
        // printWorkerCondition();
        // current epoch transaction have been assigned to workers
        DLOG(INFO) << "start aria protocol, epoch = " << this->currentEpoch;
        readPhase();
        // to be more observable, we add a sleep func for testing
        //std::this_thread::sleep_for(std::chrono::seconds(5));
        commitPhase();

        DLOG(INFO) << "all worker, num: " << this->workers.size() << " finished epoch = " << this->currentEpoch << ", write back data.";
        // announce the manager
        epochTransactionFinishSignal(currentEpoch);
        //update the epoch finally
        currentEpoch += 1;
    }
    LOG(INFO) << "AriaCoordinator exited.";
}

void AriaCoordinator::assignWorkloadPhase() {
    std::unique_ptr<Workload> workload = nullptr;
    // if transactions in Current Epoch is not all assigned
    while (workload = getWorkloadFromBuffer(), workload){
        //we have get a workload for current epoch
        auto workerID = deployWorkload(std::move(workload));
        if (workerID == 0){
            LOG(ERROR) << "deploy workload failed! worker status:";
            printWorkerCondition();
        }    //workload is null after this
        // eager read optimization, we have deploy the workload and run the worker with phase one
        CHECK(workers.count(workerID));
        workers[workerID]->setCoordinatorState(AriaGlobalState::Aria_READ);
    }
}

void AriaCoordinator::readPhase() {
    for (auto worker: workers) {
        if (worker.second->getWorkerState() == WorkerState::READY)
            continue;
        worker.second->setCoordinatorState(AriaGlobalState::Aria_READ);
        worker.second->coordinatorWait({ WorkerState::FINISH_READ });
    }
}

void AriaCoordinator::commitPhase() {
    for (auto worker: workers) {
        if (worker.second->getWorkerState() == WorkerState::READY)
            continue;
        worker.second->setCoordinatorState(AriaGlobalState::Aria_COMMIT);
        worker.second->coordinatorWait({ WorkerState::READY });
    }
}

size_t AriaCoordinator::deployWorkloadWithNewWorker(std::unique_ptr<Workload> workload) {
    //init a worker
    auto* workerInstance = new WorkerInstance(workerIDNonce++, AriaGlobalState::START);
    workerInstance->workload = std::move(workload);   //coordinator should not know workload
    workers[workerInstance->workerID] = workerInstance;
    workerInstance->instance = createWorker(workerInstance);
    //run the worker
    workerInstance->thread = std::make_unique<std::thread>(&Worker::run, workerInstance->instance.get());
    workers[workerInstance->workerID]->coordinatorWait({ WorkerState::ASSIGNED });   // move on after the worker assigned
    return workerInstance->workerID;
}

void AriaCoordinator::printWorkerCondition() const {
    size_t ready = 0;
    size_t assigned = 0;
    size_t finishRead = 0;
    for (auto worker : workers){
        auto state = worker.second->getWorkerState();
        if (state == WorkerState::READY)
            ready++;
        if (state == WorkerState::ASSIGNED)
            assigned++;
        if (state == WorkerState::FINISH_READ)
            finishRead++;
    }
    LOG(INFO) << "Total:";
    LOG(INFO) << "    epoch number: " << this->currentEpoch;
    LOG(INFO) << "    worker number: "<< this->workers.size();
    LOG(INFO) << "    ready worker: "<< ready;
    LOG(INFO) << "    assigned worker: "<< assigned;
    LOG(INFO) << "    finish read phase: "<< finishRead;
}

void AriaCoordinator::broadcastToAllWorkers(AriaGlobalState globalState, bool onlyAssigned) {
    for (auto worker: workers) {
        if (onlyAssigned && worker.second->getWorkerState() == WorkerState::READY)
            continue;
        worker.second->setCoordinatorState(globalState);
    }
}

void AriaCoordinator::syncAllWorker(const std::set<WorkerState>& state, bool onlyAssigned) {
    for (auto worker: workers){
        if (onlyAssigned && worker.second->getWorkerState() == WorkerState::READY)
            continue;
        worker.second->coordinatorWait(state);
    }
}

size_t AriaCoordinator::destroyUnusedWorkers() {
    // destroy the worker after signaled exit
    size_t count = 0;
    auto worker = workers.begin();
    while (worker != workers.end()) {
        if (worker->second->getWorkerState() == WorkerState::EXITED) {
            worker->second->thread->join();
            delete worker->second;
            workers.erase(worker++);
            count++;
        } else {
            worker++;
        }
    }
    return count;
}

size_t AriaCoordinator::addTransaction(std::unique_ptr<std::vector<Transaction*>> trWrapper) {
    // early return to avoid wakeup of consumer
    if(trWrapper->empty())
        return 0;
    // send this tx wrapper size, so we can adapt workload size in the next epoch.
    auto epoch = trWrapper->front()->getEpoch();
    workloadSizeAdapter->setLastEpochTxCount(epoch, trWrapper->size());
    // after we push the tx, we send a signal to wakeup the consumer.
    txBufferCV.notify_one([&](){
        for (auto tr: *trWrapper) {
            // the epoch in the transaction must be equal
            DCHECK(tr != nullptr);
            transactionBuffer.push(tr);
        }
    });
    return trWrapper->size();
}

std::unique_ptr<Workload> AriaCoordinator::getWorkloadFromBuffer() {
    std::unique_ptr<Workload> workload;
    // transaction executor control the MIN_TRANSACTION_PER_WORKLOAD
    txBufferCV.wait([&] {
        // buffer is empty, wait
        if (transactionBuffer.empty())
            return false;
        // current epoch transaction has all saved to queue, process
        if (transactionBuffer.back()->getEpoch() > currentEpoch) {
            // if cond meets, we start to create workload.
            workload = createWorkload();
            return true;
        }
        // current epoch and transaction is not enough, wait
        if (transactionBuffer.size() < workloadSizeAdapter->getMinTxPerWorkload())
            return false;
        // current epoch and transaction is enough, process
        // if cond meets, we start to create workload.
        workload = createWorkload();
        return true;
    });
    return workload;
}

// deploy a workload to a worker, return the worker's id, if return 0, deploy worker failed.
size_t AriaCoordinator::deployWorkload(std::unique_ptr<Workload> workload) {
    // use ready worker or create new worker to run task
    for(auto worker : workers){
        if(worker.second->getWorkerState() == WorkerState::READY){
            worker.second->setWorkerState(WorkerState::ASSIGNED);
            DLOG(INFO) << "deploying workload to a worker: " << worker.second->workerID << ", epoch:" << workload->epoch;
            worker.second->workload = std::move(workload);
            // use existed ready worker
            return worker.second->workerID;
        }
    }
    // create new worker
    return deployWorkloadWithNewWorker(std::move(workload));
}

std::unique_ptr<Workload> AriaCoordinator::createWorkload() {
    // if the buffer only contains next epoch transactions
    Transaction* transaction = transactionBuffer.front();
    if (transaction->getEpoch() > currentEpoch) {
        return nullptr;
    }
    // the tr is current epoch tr
    auto workload = std::make_unique<Workload>();
    workload->epoch = transaction->getEpoch();

    //create workload
    do {
        DCHECK(transaction->getEpoch() == currentEpoch);
        DCHECK(transaction->getEpoch() == workload->epoch);
        if(transaction->getTransactionID() != UINT64_MAX)
            workload->transactionList.push_back(transaction);
        transactionBuffer.pop();
        transaction = transactionBuffer.front();
    } while(!transactionBuffer.empty() &&               // if the buffer is not empty
            transaction->getEpoch() <= currentEpoch &&  // if the buffer contains current epoch transaction
            workload->transactionList.size() < workloadSizeAdapter->getMaxTxPerWorkload());   // not overloaded
    DLOG(INFO) << "created a workload, size: " << workload->transactionList.size() << ", epoch:" << workload->epoch;

    // announce to addListenTransaction(), so we can add trs to buffer
    return workload;
}

std::unique_ptr<Worker> AriaCoordinator::createWorker(WorkerInstance* workerInstance) {
    return std::make_unique<AriaWorker>(workerInstance);
}
