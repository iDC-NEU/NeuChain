//
// Created by peng on 2021/7/26.
//

#ifndef NEUBLOCKCHAIN_TX_EXECUTION_RECEIVER_H
#define NEUBLOCKCHAIN_TX_EXECUTION_RECEIVER_H


#include <atomic>
#include <thread>
#include "common/thread_pool.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"

class TransactionExecutor;
class ZMQServer;
class BlockInfoHelper;

/*
 * get execution request from client, port 8999
 * put them into a queue
 * a worker pool get tx from queue and executed them
 * the worker put the result in another queue after executed the transaction
 * the client get the results from port 9001
 */

class TxExecutionReceiver {
public:
    // the block storage is only a handle.
    explicit TxExecutionReceiver(BlockInfoHelper* infoHelper);
    ~TxExecutionReceiver();

protected:
    void runRequestServer();
    void runResultServer();

private:
    BlockInfoHelper* handle;
    std::atomic<bool> finishSignal = false;
    ThreadPool threadPool;
    std::unique_ptr<ZMQServer> requestServer;
    std::unique_ptr<ZMQServer> resultServer;
    std::unique_ptr<TransactionExecutor> executor;
    std::unique_ptr<std::thread> requestServerThread;
    std::unique_ptr<std::thread> resultServerThread;
    moodycamel::BlockingConcurrentQueue<std::string> resultQueue;

    const std::string localIp;
};


#endif //NEUBLOCKCHAIN_TX_EXECUTION_RECEIVER_H
