//
// Created by peng on 2021/3/13.
//

#ifndef NEUBLOCKCHAIN_BLOCK_STORAGE_H
#define NEUBLOCKCHAIN_BLOCK_STORAGE_H

#include <string>
#include "common/aria_types.h"

namespace Block {
    class Block;
}

class Transaction;

class BlockStorage {
public:
    virtual ~BlockStorage() = default;
    // append a block to the block chain and update last saved epoch
    virtual bool appendBlock(const Block::Block& block) = 0;
    // update in sequence, not thread safe!
    virtual bool updateLatestSavedEpoch() = 0;
    // spec epoch number, fill in rest of the block, must implement thread safe!
    virtual bool loadBlock(Block::Block& block) const = 0;
    // return max number of block chain, must implement thread safe!
    [[nodiscard]] virtual epoch_size_t getLatestSavedEpoch() const = 0;
};

#endif //NEUBLOCKCHAIN_BLOCK_STORAGE_H
