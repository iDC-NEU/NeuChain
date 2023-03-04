//
// Created by peng on 2021/3/13.
//

#ifndef NEUBLOCKCHAIN_BLOCK_STORAGE_TO_FILE_H
#define NEUBLOCKCHAIN_BLOCK_STORAGE_TO_FILE_H

#include "block_server/database/block_storage.h"
#include "block_server/database/db_connection.h"

#include <functional>
#include <map>
#include <mutex>

class SHA256Helper;
class CryptoSign;

class BlockStorageToFile: virtual public BlockStorage {
public:
    BlockStorageToFile();

    bool appendBlock(const Block::Block& block) override;

    bool updateLatestSavedEpoch() override;

    bool loadBlock(Block::Block& block) const override;

    [[nodiscard]] epoch_size_t getLatestSavedEpoch() const override;

protected:
    bool insertBlock(const Block::Block& block);

private:
    mutable std::mutex saveFileMutex;
    volatile epoch_size_t bufferedEpochFromFile;
};


#endif //NEUBLOCKCHAIN_BLOCK_STORAGE_TO_FILE_H
