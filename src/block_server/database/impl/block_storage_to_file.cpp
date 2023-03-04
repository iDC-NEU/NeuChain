//
// Created by peng on 2021/3/13.
//

#include "block_server/database/impl/block_storage_to_file.h"
#include "common/yaml_config.h"
#include "block.pb.h"

#include <fstream>
#include "glog/logging.h"

BlockStorageToFile::BlockStorageToFile()
        : bufferedEpochFromFile {0} {
    if(YAMLConfig::getInstance()->resetEpoch()) {
        // reset epoch, write to file.
        try{
            std::fstream ofs2("block_num.txt", std::ios::binary|std::ios::out);
            ofs2 << 0;
            ofs2.close();
        } catch (const std::exception& e) {
            LOG(ERROR) << e.what();
        }
    }
    else {
        // load epoch from file
        try{
            uint64_t epoch;
            std::fstream ifs("block_num.txt", std::ios::binary|std::ios::in);
            ifs >> epoch;
            bufferedEpochFromFile = epoch;
            ifs.close();
        } catch (const std::exception& e) {
            LOG(ERROR) << e.what();
        }
    }
}

bool BlockStorageToFile::insertBlock(const Block::Block& block) {
    try{
        int64_t epoch = block.header().number();
        std::unique_lock<std::mutex> lock(saveFileMutex);
        std::fstream ofs(std::to_string(epoch) + ".bin", std::ios::binary|std::ios::out);
        if(!ofs || !block.SerializeToOstream(&ofs))
            return false;
        ofs.close();
        LOG(INFO) << "block store in file successfully, epoch=" << epoch;
        bufferedEpochFromFile = epoch;
    } catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

bool BlockStorageToFile::loadBlock(Block::Block& block) const {
    try{
        auto epoch = block.header().number();
        // current block does not exist.
        if (epoch > bufferedEpochFromFile)
            return false;
        // add read lock to load the block (does not always necessary)
        std::unique_lock<std::mutex> lock(saveFileMutex);
        std::fstream ifs(std::to_string(epoch) + ".bin", std::ios::binary|std::ios::in);
        if(!ifs || !block.ParseFromIstream(&ifs))
            return false;
        ifs.close();
    } catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}

epoch_size_t BlockStorageToFile::getLatestSavedEpoch() const {
    return bufferedEpochFromFile;
}

bool BlockStorageToFile::appendBlock(const Block::Block &block) {
    if(!insertBlock(block)){
        return false;
    }
    if(!updateLatestSavedEpoch()) {
        return false;
    }
    return true;
}

bool BlockStorageToFile::updateLatestSavedEpoch() {
    try{
        std::fstream ofs2("block_num.txt", std::ios::binary|std::ios::out);
        ofs2 << bufferedEpochFromFile;
        ofs2.close();
    } catch (const std::exception& e) {
        LOG(ERROR) << e.what();
        return false;
    }
    return true;
}
