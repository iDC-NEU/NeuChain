//
// Created by peng on 2021/3/13.
//

#include "block_server/database/impl/block_storage_to_db.h"
#include "common/yaml_config.h"
#include "block.pb.h"
#include "glog/logging.h"

BlockStorageToDB::BlockStorageToDB()
        : bufferedEpochFromFile {0} {
    dbEpoch = 0;
}

bool BlockStorageToDB::insertBlock(const Block::Block& block) {
    try{
        int64_t epoch = block.header().number();
        std::unique_lock<std::mutex> lock(saveFileMutex);
        db[epoch] = block.SerializeAsString();
        LOG(INFO) << "block store in file successfully, epoch=" << epoch;
        bufferedEpochFromFile = epoch;
    } catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

bool BlockStorageToDB::loadBlock(Block::Block& block) const {
    try{
        auto epoch = block.header().number();
        // current block does not exist.
        if (epoch > bufferedEpochFromFile)
            return false;
        // add read lock to load the block (does not always necessary)
        std::unique_lock<std::mutex> lock(saveFileMutex);
        block.ParseFromString(db.find(epoch)->second);
    } catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

epoch_size_t BlockStorageToDB::getLatestSavedEpoch() const {
    return bufferedEpochFromFile;
}

bool BlockStorageToDB::appendBlock(const Block::Block &block) {
    if(!insertBlock(block)){
        return false;
    }
    if(!updateLatestSavedEpoch()) {
        return false;
    }
    return true;
}

bool BlockStorageToDB::updateLatestSavedEpoch() {
    dbEpoch = bufferedEpochFromFile;
    return true;
}
