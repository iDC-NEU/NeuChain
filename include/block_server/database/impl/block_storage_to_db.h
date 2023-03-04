//
// Created by peng on 3/19/21.
//

#ifndef NEUBLOCKCHAIN_BLOCK_STORAGE_TO_DB_H
#define NEUBLOCKCHAIN_BLOCK_STORAGE_TO_DB_H

#include "block_server/database/block_storage.h"
#include "block_server/database/db_connection.h"
#include "common/aria_types.h"

namespace pqxx {
    class result;
}

class BlockStorageToDB: virtual public BlockStorage, virtual private DBConnection {
public:
    bool appendBlock(const Block::Block& block) override;
    bool loadBlock(Block::Block& block) const override;
    [[nodiscard]] epoch_size_t getLatestSavedEpoch() const override;

protected:
    void initFunc();
    void createBlockTable();
    void createTransactionTable();

    pqxx::result selectFromBlockTable(const std::string& key);
    pqxx::result selectFromTransactionTable(const std::string& key);

    void insertBlockTable(epoch_size_t epoch, const std::string& prehash, const std::string& hash,const std::string& metadata, const std::string& sign);
    void insertTransactionTable(tid_size_t tid, epoch_size_t epoch, const std::string& payload, const std::string& rwSet, const std::string& hash);

};


#endif //NEUBLOCKCHAIN_BLOCK_STORAGE_TO_DB_H
