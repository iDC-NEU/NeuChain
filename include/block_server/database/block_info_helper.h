//
// Created by peng on 2021/7/4.
//

#ifndef NEUBLOCKCHAIN_BLOCK_INFO_HELPER_H
#define NEUBLOCKCHAIN_BLOCK_INFO_HELPER_H

#include <string>
#include "common/aria_types.h"

namespace Block {
    class Block;
}

/*
 * by calling this interface, you can get a serious of block chain info.
 * such as block data, tx data in block chain.
 * ps: ALL FUNC ARE READ-ONLY
 */
class BlockInfoHelper {
public:
    // return true when serialized block, thread safe.
    virtual bool loadBlock(epoch_size_t blockNum, std::string& serializedBlock) const = 0;
    virtual bool loadBlock(epoch_size_t blockNum, Block::Block& block) const = 0;
    // return max number of block chain, thread safe
    // return 0 if no block has saved, ps: epoch start from 1
    [[nodiscard]] virtual epoch_size_t getLatestSavedEpoch() const = 0;
    virtual bool getPartialConstructedBlock(epoch_size_t blockNum, std::string& serializedBlock) = 0;
    virtual epoch_size_t getPartialConstructedLatestEpoch() = 0;
};

#endif //NEUBLOCKCHAIN_BLOCK_INFO_HELPER_H
