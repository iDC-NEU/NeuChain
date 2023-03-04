//
// Created by peng on 2021/1/15.
//

#include "block_server/worker/aria_worker.h"
#include "block_server/worker/worker_instance.h"
#include "block_server/worker/transaction_executor.h"
#include "glog/logging.h"

AriaWorker::AriaWorker(WorkerInstance *self) : Worker(self){
    self->setWorkerState(WorkerState::READY);
    transactionExecutor = TransactionExecutor::transactionExecutorFactory(TransactionExecutor::ExecutorType::normal);
}

AriaWorker::~AriaWorker() {
    LOG(INFO) << "worker destroy id:"<< self->workerID;
}

void AriaWorker::run() {
    pthread_setname_np(pthread_self(), "aria_worker");
    // init condition variable
    DLOG(INFO) << "run, worker id = " << self->workerID;
    //step 0: if first enter, work must be assigned
    self->setWorkerState(WorkerState::ASSIGNED);
    while(true) {
        //step 1: wait for work assigned
        if(!self->workerWait(AriaGlobalState::Aria_READ)) {
            DLOG(INFO) << "worker id = " << self->workerID  << " recv exit signal";
            self->setWorkerState(WorkerState::EXITED);
            return;
        }

        //step 2: generate rw set, fill in real value
        transactionExecutor->executeList(self->workload->transactionList);  // get rw set for each transaction

        DLOG(INFO) << "worker id = " << self->workerID  << " has finished read, epoch: " << self->workload->epoch;
        self->setWorkerState(WorkerState::FINISH_READ);
        // wait for commit signal to commit transaction
        if(!self->workerWait(AriaGlobalState::Aria_COMMIT)) {
            self->setWorkerState(WorkerState::EXITED);
            return;
        }

        //step 3: select commit operation
        DLOG(INFO) << "worker id = " << self->workerID  << " start commit, workload size: " << self->workload->transactionList.size();
        transactionExecutor->commitList(self->workload->transactionList);  // get rw set for each transaction
        DLOG(INFO) << "worker id = " << self->workerID  << " has finished commit, epoch: " << self->workload->epoch;

        //step4: reset worker state
        self->setWorkerState(WorkerState::READY);
    }
}