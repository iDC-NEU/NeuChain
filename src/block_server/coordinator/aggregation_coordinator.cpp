//
// Created by peng on 4/26/21.
//

#include "block_server/coordinator/aggregation_coordinator.h"
#include "block_server/coordinator/aggregation_broadcaster.h"
#include "block_server/coordinator/workload_size_adapter.h"
#include "block_server/transaction/transaction.h"
#include "block_server/worker/worker_instance.h"
#include "block_server/worker/aggregation_worker.h"
#include "glog/logging.h"

AggregationCoordinator::AggregationCoordinator(epoch_size_t currentEpoch) : AriaCoordinator(currentEpoch) {
    broadcaster = std::make_unique<AggregationBroadcaster>();
    workloadSizeAdapter->setAggregateServerCount(broadcaster->getAggregationServerCount());
}

AggregationCoordinator::~AggregationCoordinator() = default;

void AggregationCoordinator::run() {
    LOG(INFO) << "AggregationCoordinator started.";
    while(!finishSignal) {
        assignWorkloadPhase();
        // printWorkerCondition();
        // current epoch transaction have been assigned to workers
        DLOG(INFO) << "start aggregation protocol, epoch = " << this->currentEpoch;
        readPhase();
        // finish read phase
        DLOG(INFO) << "aggregate phase...";
        aggregatePhase();
        DLOG(INFO) << "commit phase...";
        commitPhase();

        DLOG(INFO) << "all worker, num: "<< this->workers.size() <<
                   " finished epoch = " << this->currentEpoch <<
                   ", write back data.";
        // announce the manager
        epochTransactionFinishSignal(currentEpoch);
        //update the epoch finally
        currentEpoch += 1;
    }
}

void AggregationCoordinator::readPhase() {
    for (auto worker: workers) {
        if (worker.second->getWorkerState() == WorkerState::READY)
            continue;
        worker.second->setCoordinatorState(AriaGlobalState::Aria_READ);
        worker.second->coordinatorWait({ WorkerState::FINISH_READ, WorkerState::FINISH_AGGREGATE });
    }
}

void AggregationCoordinator::aggregatePhase() {
    broadcaster->aggregateTransaction(currentEpoch);
    for (auto worker: workers) {
        if (worker.second->getWorkerState() == WorkerState::READY)
            continue;
        worker.second->setCoordinatorState(AriaGlobalState::Aggregate);
        worker.second->coordinatorWait({ WorkerState::FINISH_AGGREGATE });
    }
}

std::unique_ptr<Workload> AggregationCoordinator::createWorkload() {
    // if the buffer only contains next epoch transactions
    Transaction* transaction = transactionBuffer.front();
    if (transaction->getEpoch() > currentEpoch) {
        return createAggregationWorkload();
    }
    // the tr is current epoch tr
    auto workload = std::make_unique<Workload>();
    workload->epoch = transaction->getEpoch();

    do {
        DCHECK(transaction->getEpoch() == currentEpoch);
        DCHECK(transaction->getEpoch() == workload->epoch);
        if (broadcaster->isLocalTransaction(transaction->getTransactionID())) {
            broadcaster->addBroadcastTransaction(transaction);
            workload->transactionList.push_back(transaction);
        } else {
            broadcaster->addListenTransaction(transaction);
            aggregationQueue.push(transaction);
        }
        transactionBuffer.pop();
        transaction = transactionBuffer.front();
    } while(!transactionBuffer.empty() &&               // if the buffer is not empty
            transaction->getEpoch() <= currentEpoch &&  // if the buffer contains current epoch transaction
            workload->transactionList.size() < workloadSizeAdapter->getMaxTxPerWorkload());   // not overloaded
    // if workload empty (because workload only contains a transaction and it is not my duty to process it), we return false
    if(workload->transactionList.empty()) {
        return createAggregationWorkload();
    }
    workload->aggregationWorkload = false;
    DLOG(INFO) << "created a workload, size: " << workload->transactionList.size() << ", epoch:" << workload->epoch;
    return workload;
}

std::unique_ptr<Workload> AggregationCoordinator::createAggregationWorkload() {
    auto workload = std::make_unique<Workload>();
    for (int i=0; i<workloadSizeAdapter->getMaxTxPerWorkload(); i++) {
        if (aggregationQueue.empty()) {
            break;
        }
        Transaction* transaction = aggregationQueue.front();
        workload->transactionList.push_back(transaction);
        DCHECK(transaction->getEpoch() == currentEpoch);
        DCHECK(transaction->getEpoch() == workload->transactionList.front()->getEpoch());
        aggregationQueue.pop();
    }
    if (workload->transactionList.empty()) {
        return nullptr;
    }
    workload->epoch = workload->transactionList.front()->getEpoch();
    workload->aggregationWorkload = true;
    DLOG(INFO) << "created a aggregation workload, size: " << workload->transactionList.size() << ", epoch:" << workload->epoch;
    return workload;
}

std::unique_ptr<Worker> AggregationCoordinator::createWorker(WorkerInstance *workerInstance) {
    return std::make_unique<AggregationWorker>(workerInstance);
}
