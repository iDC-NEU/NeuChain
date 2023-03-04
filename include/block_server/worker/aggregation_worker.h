//
// Created by peng on 4/26/21.
//

#ifndef NEUBLOCKCHAIN_AGGREGATION_WORKER_H
#define NEUBLOCKCHAIN_AGGREGATION_WORKER_H

#include "block_server/worker/worker.h"

#include <memory>

class AggregationWorker : public Worker {
public:
    explicit AggregationWorker(WorkerInstance *self);
    ~AggregationWorker() override;

    void run() override;

private:
    bool isAggregate;
    TransactionExecutor* transactionExecutor;
    std::unique_ptr<TransactionExecutor> transactionExecutorAggregation;
    std::unique_ptr<TransactionExecutor> transactionExecutorNormal;
};
#endif //NEUBLOCKCHAIN_AGGREGATION_WORKER_H
