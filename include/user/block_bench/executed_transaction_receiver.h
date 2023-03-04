//
// Created by peng on 2021/7/27.
//

#ifndef NEUBLOCKCHAIN_EXECUTED_TRANSACTION_RECEIVER_H
#define NEUBLOCKCHAIN_EXECUTED_TRANSACTION_RECEIVER_H

#include <atomic>
#include <thread>
#include "common/zmq/zmq_client.h"
#include "common/concurrent_queue/blocking_mpmc_queue.h"
#include "glog/logging.h"

class ExecutedTransactionReceiver {
public:
    explicit ExecutedTransactionReceiver(const std::vector<std::string>& ips, const std::vector<std::string>& orderIPs)
            :finishSignal(false), queue(1000) {
        for(const auto& ip: ips) {
            threads.emplace_back(&ExecutedTransactionReceiver::run_receiver, this, ip);
            threads.emplace_back(&ExecutedTransactionReceiver::run_sender, this, orderIPs);
        }
    }
    virtual ~ExecutedTransactionReceiver() {
        finishSignal = true;
        for(auto& t: threads)
            t.join();
    }

protected:
    void run_receiver(const std::string& blockServerIP) {
        ZMQClient receiver(blockServerIP, "9001", zmq::socket_type::sub);
        while(!finishSignal) {
            // just storage-forward
            std::string message;
            receiver.getReply(message);
            queue.push(std::move(message));
        }
    }

    void run_sender(const std::vector<std::string>& orderIPs) {
        std::vector<std::unique_ptr<ZMQClient>> sender_group;
        sender_group.reserve(orderIPs.size());
        for (const auto& ip: orderIPs) {
            sender_group.emplace_back(new ZMQClient(ip, "9002", zmq::socket_type::pub));
        }
        std::string message;
        while(!finishSignal) {
            for (auto& sender :sender_group) {
                if (queue.availableApprox() > 1000) {
                    LOG(INFO) << "too many message!";
                }
                queue.pop(message);
                sender->sendRequest(message);
            }
        }
    }
private:
    std::atomic<bool> finishSignal;
    std::vector<std::thread> threads;
    BlockingMPMCQueue<std::string> queue;
};


#endif //NEUBLOCKCHAIN_EXECUTED_TRANSACTION_RECEIVER_H
