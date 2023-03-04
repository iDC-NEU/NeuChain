//
// Created by peng on 2021/3/18.
//

#ifndef NEUBLOCKCHAIN_USER_COLLECTOR_H
#define NEUBLOCKCHAIN_USER_COLLECTOR_H

#include "common/zmq/zmq_server.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/aria_types.h"
#include <functional>
#include <mutex>

class ZMQClient;
class Transaction;

class UserCollector: public ZMQServer{
public:
    UserCollector();
    ~UserCollector() override;

    void startService();

    std::function<void(const std::vector<Transaction*>& transaction, const std::string& proof)> newTransactionHandle;

protected:
    void collectRequest();
    void processRequest();

private:
    const int clientSize = 10;
    /// if 4 epoch server, min_count=2 (f=1, f+1=2)
    /// if 8 epoch server, min_count=3 (f=2, f+1=3)
    /// if 12 epoch server, min_count=4 (f=3, f+1=4)
    /// if 16 epoch server, min_count=5 (f=4, f+1=5)
    const int minEpochServerCount = 2;
    std::atomic<bool> finishSignal;
    std::unique_ptr<std::thread> collector;
    std::unique_ptr<std::thread> processor;
    moodycamel::BlockingConcurrentQueue<std::string> requestBuffer;
    std::vector<moodycamel::BlockingConcurrentQueue<ZMQClient*>> epochClientList;
    std::mutex pipelineLock;
};


#endif //NEUBLOCKCHAIN_USER_COLLECTOR_H
