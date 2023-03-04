//
// Created by peng on 2021/6/20.
//

#ifndef NEUBLOCKCHAIN_CLIENT_PROXY_H
#define NEUBLOCKCHAIN_CLIENT_PROXY_H

#include <string>
#include <vector>
#include <thread>
#include <functional>
#include "common/aria_types.h"
#include "common/cyclic_barrier.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"

class CryptoSign;
class ServerListener;
class Transaction;
class UserCollector;
class ZMQClient;
class ZMQServer;

namespace comm {
    class TransactionsWithProof;
    class EpochResponse;
}

class ClientProxy {
public:
    explicit ClientProxy(epoch_size_t startWithEpoch);
    ~ClientProxy();
    void startService();

    void receiveFromLocalServer();
    void receiveFromOtherServer(const std::string& ip, ZMQClient* client);

    std::function<void(Transaction*)> epochBroadcastTransactionsHandle;

protected:
    [[nodiscard]] bool verifyBatchHash(const std::vector<Transaction*>& transactionList,const comm::EpochResponse& response) const;

private:
    epoch_size_t epoch;
    std::atomic<bool> finishSignal;
    ZMQServer* localServer;
    std::unique_ptr<UserCollector> userCollector;
    std::unique_ptr<std::thread> userCollectorThread;
    std::unique_ptr<std::thread> localReceiverThread;
    std::unique_ptr<ServerListener> serverListener;

    // Transactions that received from local server but not yet to be broadcast.
    moodycamel::BlockingConcurrentQueue<std::pair<std::string, std::vector<Transaction*>>> pendingTransactions;
    // for multi blk server
    cbar::CyclicBarrier barrier;
    // for verify batch hash
    const CryptoSign* epochServerSignHelper;
};


#endif //NEUBLOCKCHAIN_CLIENT_PROXY_H
