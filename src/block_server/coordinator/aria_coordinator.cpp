//
// Created by peng on 2021/1/15.
//

#include "block_server/coordinator/aria_coordinator.h"
#include "block_server/coordinator/workload_size_adapter.h"
#include "block_server/transaction/transaction.h"

#include "block_server/worker/aria_worker.h"
#include "block_server/worker/worker_instance.h"
#include "glog/logging.h"

AriaCoordinator::AriaCoordinator(epoch_size_t startupEpoch) : Coordinator(startupEpoch), workerIDNonce(1) {
    workloadSizeAdapter = std::make_unique<WorkloadSizeAdapter>();
}

AriaCoordinator::~AriaCoordinator() {
    stop();
    broadcastToAllWorkers(AriaGlobalState::EXIT, false);
    syncAllWorker({ WorkerState::EXITED }, false);
    LOG(INFO) << destroyUnusedWorkers() << " worker has stopped, exit coordinator." << std::endl;
}

void AriaCoordinator::run() {
    pthread_setname_np(pthread_self(), "aria_coord");
    LOG(INFO) << "AriaCoordinator started.";
    while(!finishSignal) {
        assignWorkloadPhase();
        consumeTimeCalculator.startCalculate(consumeTimeCalculator.getCurrentEpoch());
        // printWorkerCondition();
        // current epoch transaction have been assigned to workers
        DLOG(INFO) << "start aria protocol, epoch = " << this->currentEpoch;
        readPhase();
        waitUntilCurrentEpochFinish();
        CHECK(needReReserve() == false);
        DLOG(INFO) << "start commit phase, epoch = " << this->currentEpoch;
        // to be more observable, we add a sleep func for testing
        //std::this_thread::sleep_for(std::chrono::seconds(5));
        commitPhase();

        DLOG(INFO) << "Worker size: " << this->workers.size() << " finished commit phase, epoch = " << this->currentEpoch;
        // announce the manager
        epochTransactionFinishSignal(currentEpoch);
        //update the epoch finally
        consumeTimeCalculator.endCalculate(consumeTimeCalculator.getCurrentEpoch());
        consumeTimeCalculator.getAverageResult(consumeTimeCalculator.getCurrentEpoch(), "aria_coordinator_async(if sync + block gen time)");
        consumeTimeCalculator.increaseEpoch();
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
    broadcastToAllWorkers(AriaGlobalState::Aria_READ);
    syncAllWorker({ WorkerState::FINISH_READ });
}

void AriaCoordinator::commitPhase() {
    broadcastToAllWorkers(AriaGlobalState::Aria_COMMIT);
    syncAllWorker({ WorkerState::READY });
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
    for (auto worker : workers) {
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
    LOG(INFO) << "    worker number: " << this->workers.size();
    LOG(INFO) << "    ready worker: " << ready;
    LOG(INFO) << "    assigned worker: " << assigned;
    LOG(INFO) << "    finish read phase: " << finishRead;
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
    queueWrapper.push_bulk(*trWrapper);
    return trWrapper->size();
}

std::unique_ptr<Workload> AriaCoordinator::getWorkloadFromBuffer() {
    auto tmp = createWorkload();
    return tmp;
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
    Transaction* transaction = queueWrapper.front();
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
        workload->transactionList.push_back(transaction);
        queueWrapper.pop();
        transaction = queueWrapper.front();
    } while((workload->transactionList.size() <= workloadSizeAdapter->getMinTxPerWorkload() || !queueWrapper.empty()) &&
            // if we have collect the min threshold of tx or the buffer is not empty
            transaction->getEpoch() <= currentEpoch &&  // if the buffer contains current epoch transaction
            workload->transactionList.size() < workloadSizeAdapter->getMaxTxPerWorkload());   // not overloaded
    DLOG(INFO) << "created a workload, size: " << workload->transactionList.size() << ", epoch:" << workload->epoch;

    // announce to addListenTransaction(), so we can add trs to buffer
    return workload;
}

std::unique_ptr<Worker> AriaCoordinator::createWorker(WorkerInstance* workerInstance) {
    return std::make_unique<AriaWorker>(workerInstance);
}
