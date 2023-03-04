//
// Created by peng on 5/10/21.
//

#include "block_server/database/block_generator.h"
#include "block_server/transaction/transaction.h"
#include "block_server/database/impl/block_storage_to_file.h"
#include "block_server/database/impl/block_storage_to_db.h"
#include "common/compile_config.h"
#include "block.pb.h"
#include "glog/logging.h"

std::unique_ptr<BlockStorage> getStorage() {
#ifdef USING_MEMORY_BLOCK
    return std::make_unique<BlockStorageToDB>();
#else
    return std::make_unique<BlockStorageToFile>();
#endif
}


BlockGenerator::BlockGenerator()
        : storage(getStorage()){
    // init priority queue
    getPendingTransactionQueue();
}

BlockGenerator::~BlockGenerator() = default;

std::unique_ptr<BlockGenerator::TxPriorityQueue> BlockGenerator::getPendingTransactionQueue() {
    auto pendingTransactionQueueRef = std::move(pendingTransactionQueue);
    pendingTransactionQueue = std::make_unique<TxPriorityQueue>([](Transaction* a, Transaction* b) {
        return a->getTransactionID() < b->getTransactionID();
    });
    return pendingTransactionQueueRef;
}

epoch_size_t BlockGenerator::getLatestSavedEpoch() const {
    return storage->getLatestSavedEpoch();
}

bool BlockGenerator::loadBlock(epoch_size_t blockNum, std::string &serializedBlock) const {
    Block::Block block;
    if (loadBlock(blockNum, block)) {
        serializedBlock = block.SerializeAsString();
        return true;
    }
    return false;
}

bool BlockGenerator::loadBlock(epoch_size_t blockNum, Block::Block &block) const {
    block.mutable_header()->set_number(blockNum);
    if (!storage->loadBlock(block)) {
        LOG(WARNING) << "Block " << blockNum << " load error.";
        return false;
    }
    return true;
}
