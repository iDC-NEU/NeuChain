//
// Created by peng on 2021/1/15.
//

#ifndef NEUBLOCKCHAIN_ARIAWORKER_H
#define NEUBLOCKCHAIN_ARIAWORKER_H

#include "block_server/worker/worker.h"
#include <memory>

class AriaWorker : public Worker {
public:
    explicit AriaWorker(WorkerInstance *self);
    ~AriaWorker() override;

    void run() override;

private:
    std::unique_ptr<TransactionExecutor> transactionExecutor;
};

#endif //NEUBLOCKCHAIN_ARIAWORKER_H
