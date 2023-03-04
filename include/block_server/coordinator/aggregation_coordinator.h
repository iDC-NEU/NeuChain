//
// Created by peng on 4/26/21.
//

#ifndef NEUBLOCKCHAIN_AGGREGATION_COORDINATOR_H
#define NEUBLOCKCHAIN_AGGREGATION_COORDINATOR_H

#include "block_server/coordinator/aria_coordinator.h"

class AggregationBroadcaster;

class AggregationCoordinator: public AriaCoordinator {
public:
    explicit AggregationCoordinator(epoch_size_t startupEpoch);
    ~AggregationCoordinator() override;
    void run() override;

protected:
    void readPhase() override;
    void aggregatePhase();
    std::unique_ptr<Workload> createWorkload() override;
    // when we cant create a regular workload for epoch=i, eg: no trs.epoch=i
    // we start creating aggregation workload.
    // when we finish creating all aggregation workload, return nullptr to finish epoch=i.
    std::unique_ptr<Workload> createAggregationWorkload();
    std::unique_ptr<Worker> createWorker(WorkerInstance* workerInstance) override;

private:
    std::unique_ptr<AggregationBroadcaster> broadcaster;
    std::queue<Transaction*> aggregationQueue;
};


#endif //NEUBLOCKCHAIN_AGGREGATION_COORDINATOR_H
