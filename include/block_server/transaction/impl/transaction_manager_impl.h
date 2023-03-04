//
// Created by peng on 2021/1/22.
//

#ifndef NEUBLOCKCHAIN_TRANSACTIONIMPL_H
#define NEUBLOCKCHAIN_TRANSACTIONIMPL_H


#include "block_server/transaction/transaction_manager.h"
#include "common/hash_map.h"
#include "common/concurrent_queue/concurrent_queue.hpp"
#include "common/cv_wrapper.h"
#include <condition_variable>
#include <common/zmq/zmq_client.h>
#include <common/yaml_config.h>

class Comm;
class BlockGenerator;

class TransactionManagerImpl : public TransactionManager {
public:
    explicit TransactionManagerImpl(bool categorize = false, bool reusing = false)
            :TransactionManager(categorize, reusing) {
        client = std::make_unique<ZMQClient>(YAMLConfig::getInstance()->getBlockServerIPs().back(), "9989", zmq::socket_type::pub);
    }
    ~TransactionManagerImpl() override;

    void run() override;

    void epochFinishedCallback(epoch_size_t finishedEpoch) override;

    void setBlockGenerator(std::unique_ptr<BlockGenerator> generator);
    void setCommunicateModule(std::unique_ptr<Comm> module) { commModule = std::move(module); }

protected:
    inline void categorizeEpoch(epoch_size_t finishedEpoch);
    inline void nonCategorizeEpoch(epoch_size_t finishedEpoch);

private:
    HashMap<epoch_size_t, moodycamel::ConcurrentQueue<Transaction*>> epochTxBuffer;
    // the epoch which block has already generated.
    epoch_size_t cleanedEpoch = 0;
    std::unique_ptr<Comm> commModule;
    std::unique_ptr<BlockGenerator> blockGenerator;
    // for alert processingEpoch changes
    CVWrapper epochFinishedSignal;
    std::unique_ptr<ZMQClient> client;
};

#endif //NEUBLOCKCHAIN_TRANSACTIONIMPL_H
