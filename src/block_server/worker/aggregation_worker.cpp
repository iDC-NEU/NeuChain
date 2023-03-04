//
// Created by peng on 4/26/21.
//

#include "block_server/worker/aggregation_worker.h"
#include "block_server/worker/worker_instance.h"
#include "block_server/worker/transaction_executor.h"

#include "block_server/transaction/transaction.h"

#include "glog/logging.h"

AggregationWorker::AggregationWorker(WorkerInstance *self) : Worker{self}, transactionExecutor{nullptr} {
    self->setWorkerState(WorkerState::READY);
    isAggregate = self->workload->aggregationWorkload;
    transactionExecutorAggregation = TransactionExecutor::transactionExecutorFactory(TransactionExecutor::ExecutorType::aggregate);
    transactionExecutorNormal = TransactionExecutor::transactionExecutorFactory(TransactionExecutor::ExecutorType::normal);
    DLOG(INFO) << "AggregationWorker started";
}

AggregationWorker::~AggregationWorker() {
    LOG(INFO) << "AggregationWorker destroyed id:"<< self->workerID << std::endl;
}

void AggregationWorker::run() {
    pthread_setname_np(pthread_self(), "agg_worker");
    // init condition variable
    DLOG(INFO) << "run, worker id = " << self->workerID << std::endl;
    //step 0: if first enter, work must be assigned
    self->setWorkerState(WorkerState::ASSIGNED);
    while(true) {
        //step 1: wait for transaction payload ready
        // note: aggregate or not, we must wait signal first.
        if(!self->workerWait(AriaGlobalState::Aria_READ)) {
            DLOG(INFO) << "worker id = " << self->workerID  << " recv exit signal";
            self->setWorkerState(WorkerState::EXITED);
            return;
        }
        isAggregate = self->workload->aggregationWorkload;
        if(!isAggregate){
            transactionExecutor = transactionExecutorNormal.get();
            //generate rw set, fill in real value later.
        } else {
            transactionExecutor = transactionExecutorAggregation.get();
            // no need to read
            self->setWorkerState(WorkerState::FINISH_READ);
            //step 2: aggregate worker
            if(!self->workerWait(AriaGlobalState::Aggregate)) {
                DLOG(INFO) << "worker id = " << self->workerID  << " recv exit signal";
                self->setWorkerState(WorkerState::EXITED);
                return;
            }
        }
        // if(!isAggregate) generate rw set, fill in real value
        // if(isAggregate) no need to read, just reserve rw set
        transactionExecutor->executeList(self->workload->transactionList);  // get rw set for each transaction
        DLOG(INFO) << "worker id = " << self->workerID  << " has finished read / aggregate, epoch: " << self->workload->epoch << std::endl;
        self->setWorkerState(WorkerState::FINISH_AGGREGATE);

        //step 3: wait for commit signal to commit transaction
        if(!self->workerWait(AriaGlobalState::Aria_COMMIT)) {
            self->setWorkerState(WorkerState::EXITED);
            return;
        }
        //select commit operation
        transactionExecutor->commitList(self->workload->transactionList);  // get rw set for each transaction
        DLOG(INFO) << "worker id = " << self->workerID  << " has finished commit, epoch: " << self->workload->epoch << std::endl;

        //step4: reset worker state
        self->setWorkerState(WorkerState::READY);
    }
}
