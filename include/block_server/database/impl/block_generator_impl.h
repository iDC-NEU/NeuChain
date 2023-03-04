//
// Created by peng on 2/17/21.
//

#ifndef NEUBLOCKCHAIN_BLOCK_GENERATOR_IMPL_H
#define NEUBLOCKCHAIN_BLOCK_GENERATOR_IMPL_H

#include "block_server/database/block_generator.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/hash_map.h"
#include "common/lru_cache.hpp"
#include "common/cv_wrapper.h"
#include <queue>

class BlockBroadcaster;
class CryptoSign;
class ThreadPool;

class BlockGeneratorImpl: public BlockGenerator{
public:
    BlockGeneratorImpl();
    ~BlockGeneratorImpl() override;

    bool generateBlock() override;

protected:
    // append trs to block, generate tr metadata and add it to block, clear tr queue
    bool generateBlockBody(Block::Block* block, std::unique_ptr<TxPriorityQueue> transactions) const;
    bool deserializeBlock(epoch_size_t epoch, Block::Block *block, bool verify=true, bool validate=true) const;
    static bool signatureValidate(const Block::Block *block, const CryptoSign* sign, const std::string& signature, bool skipVerify=true);
    // should be called in sequence.
    bool generateBlockHeader(Block::Block *block) const;

    void run();

private:
    std::unique_ptr<BlockBroadcaster> broadcaster;
    std::unique_ptr<ThreadPool> workers;
    const CryptoSign* signHelper;

    HashMap<epoch_size_t, std::unique_ptr<TxPriorityQueue>> workloadsBuffer;
    HashMap<epoch_size_t, std::unique_ptr<Block::Block>> blockBuffer;

    std::atomic<bool> finishSignal;
    CVWrapper blockReadySignal, blockBodyGeneratedSignal;
    std::thread blockGeneratorThread;

    lru11::Cache<epoch_size_t, Block::Block, std::mutex> blockCache;
};


#endif //NEUBLOCKCHAIN_BLOCK_GENERATOR_IMPL_H
