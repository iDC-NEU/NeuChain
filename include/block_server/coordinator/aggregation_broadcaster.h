//
// Created by peng on 4/26/21.
//

#ifndef NEUBLOCKCHAIN_AGGREGATION_BROADCASTER_H
#define NEUBLOCKCHAIN_AGGREGATION_BROADCASTER_H

#include <map>
#include <atomic>
#include <functional>
#include "common/aria_types.h"
#include "common/cyclic_barrier.h"
#include "common/concurrent_queue/mpmc_queue.h"

class Transaction;
class ServerListener;
class ZMQServer;
class ZMQClient;
class YAMLConfig;

namespace comm {
    class AggregationExchange;
}

class AggregationBroadcaster {
public:
    AggregationBroadcaster();
    ~AggregationBroadcaster();

    // add a transaction to set, this transaction does not execution by local server,
    // but is merged from remote server. no reentry
    bool addListenTransaction(Transaction* transaction);

    // this transaction will be executed by local server,
    bool addBroadcastTransaction(Transaction* transaction);

    // call by coordinator, when current epoch transaction executed, broadcast to other server.
    // return when all received transaction has already storage into aggregate transaction list.
    // in other word, all transaction has finished execution, not local server only.
    bool aggregateTransaction(epoch_size_t epoch);

    [[nodiscard]] inline bool isLocalTransaction(tid_size_t transactionID) const {
        return transactionID % totalServerCount == localID;
    }

    [[nodiscard]] inline size_t getAggregationServerCount() const {
        return totalServerCount;
    }

protected:
    // receive from other server
    void localClientRecv(const std::string& ip, ZMQClient* client);

private:
    std::atomic<bool> finishSignal;
    int64_t broadcastEpoch;
    const YAMLConfig* config;
    size_t localID, totalServerCount;
    // a barrier to sync all broadcasters.
    cbar::CyclicBarrier barrier;
    // the transaction that receive from other server.
    // because of multi producer, we use MPMCQueue here to provide sequence access.
    rigtorp::MPMCQueue<std::unique_ptr<comm::AggregationExchange>> remoteRecvBuffer;
    // contains transactions that are not executed by this peer.
    std::map<epoch_size_t, std::map<tid_size_t , Transaction*>> listenTransaction;
    // contains transactions that are executed by this peer.
    std::map<epoch_size_t, std::map<tid_size_t , Transaction*>> broadcastTransaction;
    // the broadcaster server and other components.
    std::unique_ptr<ZMQServer> localBroadcast;
    std::unique_ptr<ServerListener> remoteRecv;
};

#endif //NEUBLOCKCHAIN_AGGREGATION_BROADCASTER_H
