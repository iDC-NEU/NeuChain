//
// Created by peng on 3/19/21.
//

#ifndef NEUBLOCKCHAIN_BLOCK_STORAGE_TO_DB_H
#define NEUBLOCKCHAIN_BLOCK_STORAGE_TO_DB_H

#include "block_server/database/block_storage.h"
#include "block_server/database/db_connection.h"

#include <functional>
#include <map>
#include <mutex>

class SHA256Helper;
class CryptoSign;

class BlockStorageToDB: virtual public BlockStorage {
public:
    BlockStorageToDB();

    bool appendBlock(const Block::Block& block) override;

    bool updateLatestSavedEpoch() override;

    bool loadBlock(Block::Block& block) const override;

    [[nodiscard]] epoch_size_t getLatestSavedEpoch() const override;

protected:
    bool insertBlock(const Block::Block& block);

private:
    mutable std::mutex saveFileMutex;
    volatile epoch_size_t bufferedEpochFromFile;
    std::unordered_map<epoch_size_t, std::string> db;
    epoch_size_t dbEpoch;
};


#endif //NEUBLOCKCHAIN_BLOCK_STORAGE_TO_DB_H
