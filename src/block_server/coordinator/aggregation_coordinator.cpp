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

AggregationCoordinator::AggregationCoordinator(epoch_size_t startupEpoch) : AriaCoordinator(startupEpoch) {
    broadcaster = std::make_unique<AggregationBroadcaster>();
    workloadSizeAdapter->setAggregateServerCount(broadcaster->getAggregationServerCount());
}

AggregationCoordinator::~AggregationCoordinator() = default;

void AggregationCoordinator::run() {
    pthread_setname_np(pthread_self(), "agg_coord");
    LOG(INFO) << "AggregationCoordinator started.";
    while(!finishSignal) {
        assignWorkloadPhase();
        // printWorkerCondition();
        // current epoch transaction have been assigned to workers
        DLOG(INFO) << "start aria protocol, epoch = " << this->currentEpoch;
        readPhase();
        waitUntilCurrentEpochFinish();
        // finish read phase
        DLOG(INFO) << "aggregate phase...";
        aggregatePhase();
        DLOG(INFO) << "commit phase...";
        commitPhase();

        DLOG(INFO) << "all worker, num: "<< this->workers.size() <<" finished epoch = " << this->currentEpoch << ", write back data.";
        // announce the manager
        epochTransactionFinishSignal(currentEpoch);
        //update the epoch finally
        currentEpoch += 1;
    }
}

void AggregationCoordinator::readPhase() {
    broadcastToAllWorkers(AriaGlobalState::Aria_READ);
    syncAllWorker({ WorkerState::FINISH_READ, WorkerState::FINISH_AGGREGATE });
}

void AggregationCoordinator::aggregatePhase() {
    broadcaster->aggregateTransaction(currentEpoch);
    broadcastToAllWorkers(AriaGlobalState::Aggregate);
    syncAllWorker({ WorkerState::FINISH_AGGREGATE });
}

std::unique_ptr<Workload> AggregationCoordinator::createWorkload() {
    // if the buffer only contains next epoch transactions
    Transaction* transaction = queueWrapper.front();
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
        queueWrapper.pop();
        transaction = queueWrapper.front();
    } while((workload->transactionList.size() <= workloadSizeAdapter->getMinTxPerWorkload() || !queueWrapper.empty()) &&
            // if we have collect the min threshold of tx or the buffer is not empty
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
    for (size_t i=0; i<workloadSizeAdapter->getMaxTxPerWorkload(); i++) {
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
