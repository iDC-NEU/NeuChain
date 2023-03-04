//
// Created by peng on 4/26/21.
//

#include "block_server/coordinator/aggregation_broadcaster.h"

#include <memory>
#include "block_server/transaction/transaction.h"
#include "block_server/database/database.h"
#include "block_server/database/db_storage.h"
#include "common/zmq/zmq_server.h"
#include "common/zmq/zmq_server_listener.h"
#include "common/yaml_config.h"
#include "common/concurrent_queue/light_weight_semaphore.hpp"

#include "comm.pb.h"
#include "glog/logging.h"

AggregationBroadcaster::AggregationBroadcaster()
        : finishSignal(false), config(YAMLConfig::getInstance()),
          remoteAggregationBlockServerCount(static_cast<int>(config->getLocalAggregationBlockServerCount()) - 1),
          broadcastEpoch(0), remoteRecvBuffer(remoteAggregationBlockServerCount*2), // duplicate size just in case
          barrier(remoteAggregationBlockServerCount), workers(remoteAggregationBlockServerCount, "agg_cst_worker") {
    CHECK(config->getLocalAggregationBlockServerCount() > 1);
    const auto&& aggregationClusterIPs = config->getAggregationClusterIPs();
    localBroadcast = std::make_unique<ZMQServer>("0.0.0.0", "7004", zmq::socket_type::pub);
    remoteRecv = std::make_unique<ServerListener>(&AggregationBroadcaster::localClientRecv, this, aggregationClusterIPs, "7004");
    totalServerCount = aggregationClusterIPs.size();
    localID = 0;
    // assert block server ip is unique
    for(const auto& ip: aggregationClusterIPs) {
        // compare with the ips in cluster to determine the sequence of local ip.
        if(strcmp(ip.data(), config->getLocalBlockServerIP().data()) < 0) {
            // get the determinant order of current block server
            localID++;
        }
    }
}

bool AggregationBroadcaster::addBroadcastTransaction(Transaction *transaction) {
    DCHECK(transaction->getEpoch() > static_cast<size_t>(broadcastEpoch));
    DCHECK(!broadcastTransaction[transaction->getEpoch()].count(transaction->getTransactionID()));
    broadcastTransaction[transaction->getEpoch()][transaction->getTransactionID()] = transaction;
    return true;
}

bool AggregationBroadcaster::addListenTransaction(Transaction *transaction) {
    DCHECK(transaction->getEpoch() > static_cast<size_t>(broadcastEpoch));
    DCHECK(!listenTransaction[transaction->getEpoch()].count(transaction->getTransactionID()));
    listenTransaction[transaction->getEpoch()][transaction->getTransactionID()] = transaction;
    return true;
}

bool AggregationBroadcaster::aggregateTransaction(epoch_size_t epoch) {
    DCHECK(static_cast<size_t>(broadcastEpoch) <= epoch);
    // has already broadcast
    if(static_cast<size_t>(broadcastEpoch) == epoch)
        return false;
    // for the first time
    broadcastEpoch = epoch;
    // 1. we first make the queue
    // we dont need proof here because the sender is from the same org.
    moodycamel::LightweightSemaphore sema;
    for (auto i=0; i < remoteAggregationBlockServerCount; i++) {
        workers.execute([this, &sema]() {
            auto* storage = VersionedDB::getDBInstance()->getStorage();
            std::unique_ptr<comm::AggregationExchange> aggregationExchange;
            remoteRecvBuffer.pop(aggregationExchange);
            // local arrive too late
            CHECK(static_cast<size_t>(broadcastEpoch) == aggregationExchange->epoch());
            auto& currentEpochTransaction = listenTransaction[aggregationExchange->epoch()];
            for(const auto& node: aggregationExchange->node()) {
                // the tid must in currentEpochTransaction, or there is a bug.
                CHECK(currentEpochTransaction.count(node.tid()));
                auto& transaction = currentEpochTransaction[node.tid()];
                // construct transaction
                transaction->setTransactionResult(static_cast<TransactionResult>(node.result()));
                transaction->getRWSet()->ParseFromString(node.rwset());
                storage->bufferUpdates(transaction->getTransactionID(), transaction->getRWSet(), {});
            }
            sema.signal();
        });
    }
    // 2. send local transaction list
    comm::AggregationExchange aggregationExchange;
    aggregationExchange.set_epoch(epoch);
    for(const auto& transaction: broadcastTransaction[epoch]) {
        auto* node = aggregationExchange.add_node();
        node->set_tid(transaction.first);
        node->set_result(static_cast<uint32_t>(transaction.second->getTransactionResult()));
        node->set_rwset(transaction.second->getRWSet()->SerializeAsString());
    }
    broadcastTransaction.erase(epoch);
    localBroadcast->sendReply(aggregationExchange.SerializeAsString());

    // 3. wait for all transaction broadcast
    for (int i=remoteAggregationBlockServerCount; i>0; i-=sema.waitMany(i));
    // after this, worker thread pool is empty
    listenTransaction.erase(broadcastEpoch);
    return true;
}

void AggregationBroadcaster::localClientRecv(const std::string &ip, ZMQClient *client) {
    pthread_setname_np(pthread_self(), "agg_recv");
    while(!finishSignal) {
        auto aggregationExchange = std::make_unique<comm::AggregationExchange>();
        std::string messageRaw;
        do {
            client->getReply(messageRaw);
        } while(!aggregationExchange->ParseFromString(messageRaw));
        DLOG(INFO) << "Receive a set of transactions, ip: "<< ip
                   << ", size: " << aggregationExchange->node_size()
                   << ", epoch: " << aggregationExchange->epoch();
        remoteRecvBuffer.push(std::move(aggregationExchange));
        barrier.await();
    }
}

AggregationBroadcaster::~AggregationBroadcaster() {
    finishSignal = true;
    // when return, serverListener will automatic call join() func,
    // so we need to do nothing here.
}
