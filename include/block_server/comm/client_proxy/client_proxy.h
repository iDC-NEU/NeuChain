//
// Created by peng on 2021/6/20.
//

#ifndef NEUBLOCKCHAIN_CLIENT_PROXY_H
#define NEUBLOCKCHAIN_CLIENT_PROXY_H

#include "consensus_control_block.h"
#include "common/aria_types.h"
#include <string>
#include <vector>
#include <thread>
#include <functional>

class CryptoSign;
class ServerListener;
class Transaction;
class UserCollector;
class ZMQClient;
class ZMQServer;
class EpochTxnQueueManager;

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
    const epoch_size_t startWithEpoch;
    std::atomic<bool> finishSignal;
    ZMQServer* localServer;
    EpochTxnQueueManager* queueManager;
    std::unique_ptr<UserCollector> userCollector;
    std::unique_ptr<std::thread> userCollectorThread;
    std::unique_ptr<std::thread> localReceiverThread;
    std::unique_ptr<ServerListener> serverListener;
    ConsensusControlBlock* ccb;

    // Transactions that received from local server but not yet to be broadcast.
    BlockingMPMCQueue<std::pair<std::string, std::vector<Transaction*>>> pendingTransactions;
    // for verify batch hash
    const CryptoSign* epochServerSignHelper;
    // bits to invalid the remote client proxies
    // the bits must be invalid FIRST before modify data structure;
    std::unordered_map<std::string, std::pair<epoch_size_t, std::atomic<bool>>> invalidBits;
};


#endif //NEUBLOCKCHAIN_CLIENT_PROXY_H
