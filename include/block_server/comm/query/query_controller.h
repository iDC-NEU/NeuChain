//
// Created by peng on 2021/4/22.
//

#ifndef NEUBLOCKCHAIN_QUERY_CONTROLLER_H
#define NEUBLOCKCHAIN_QUERY_CONTROLLER_H

#include <atomic>
#include <thread>

class TransactionExecutor;
class ZMQServer;
class BlockInfoHelper;

class QueryController {
public:
    // the block storage is only a handle.
    explicit QueryController(BlockInfoHelper* infoHelper);
    ~QueryController();

protected:
    void run();

private:
    BlockInfoHelper* handle;
    std::unique_ptr<ZMQServer> zmqServer;
    std::unique_ptr<std::thread> collector;
    std::unique_ptr<TransactionExecutor> executor;
};


#endif //NEUBLOCKCHAIN_QUERY_CONTROLLER_H
