//
// Created by peng on 2/17/21.
//

#ifndef NEUBLOCKCHAIN_BLOCK_GENERATOR_H
#define NEUBLOCKCHAIN_BLOCK_GENERATOR_H

#include <queue>
#include <memory>
#include <functional>
#include "block_info_helper.h"

class Transaction;
class BlockStorage;

class BlockGenerator: public BlockInfoHelper {
public:
    virtual ~BlockGenerator();
    // add an executed tx to block chain, not thread safe.
    inline void addCommittedTransaction(Transaction* transaction) { pendingTransactionQueue->push(transaction); }
    // async generate a block and broadcast to other server later, not thread safe.
    virtual bool generateBlock() = 0;

public:
    // override function impl here
    bool loadBlock(epoch_size_t blockNum, std::string& serializedBlock) const override;
    bool loadBlock(epoch_size_t blockNum, Block::Block& block) const override;

    [[nodiscard]] epoch_size_t getLatestSavedEpoch() const override;

protected:
    // accept a storage, automatic deleted it when destroyed
    BlockGenerator();
    std::unique_ptr<BlockStorage> storage;

    using TxPriorityQueue = std::priority_queue<Transaction*, std::vector<Transaction*>, std::function<bool(Transaction*, Transaction*)>>;
    std::unique_ptr<TxPriorityQueue> getPendingTransactionQueue();

private:
    std::unique_ptr<TxPriorityQueue> pendingTransactionQueue;
};


#endif //NEUBLOCKCHAIN_BLOCK_GENERATOR_H
