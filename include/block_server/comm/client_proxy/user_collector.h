//
// Created by peng on 2021/3/18.
//

#ifndef NEUBLOCKCHAIN_USER_COLLECTOR_H
#define NEUBLOCKCHAIN_USER_COLLECTOR_H

#include "common/zmq/zmq_server.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/aria_types.h"
#include <functional>

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
    std::atomic<bool> finishSignal;
    std::unique_ptr<std::thread> collector;
    std::unique_ptr<std::thread> processor;
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<std::string>> requestBuffer;
    std::unique_ptr<ZMQClient> epochServerClient;
};


#endif //NEUBLOCKCHAIN_USER_COLLECTOR_H
