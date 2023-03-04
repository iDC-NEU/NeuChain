//
// Created by peng on 3/19/21.
//

#include "block_server/database/impl/block_storage_to_db.h"
#include "glog/logging.h"
#include <pqxx/pqxx>

bool BlockStorageToDB::loadBlock(Block::Block &block) const {
    return false;
}

epoch_size_t BlockStorageToDB::getLatestSavedEpoch() const {
    return 0;
}

void BlockStorageToDB::createBlockTable() {
    LOG(INFO)<<"create block table.";
    std::string sql = "create table Block (epoch int primary key not null,"
                      "prehash text , hash text,"
                      "metadata text , signed text)";
    executeTransaction(sql);

}

void BlockStorageToDB::createTransactionTable() {
    LOG(INFO)<<"create transaction table.";
    std::string sql = "create table Transactions (tid int not null,"
                      "epoch int not null, payload text not null,"
                      "rwSet text not null, tr_metadata text,"
                      "hash text , primary key(tid,epoch))";
    executeTransaction(sql);
}

pqxx::result BlockStorageToDB::selectFromBlockTable(const std::string &key) {
    pqxx::result result;
    std::string sql =  "select * from Block where key =\'"+key+"\'";
    if(!executeTransaction(sql, &result)) {
        LOG(WARNING)<<"selectFromBlockTable failed.";
    }
    return result;
}

pqxx::result BlockStorageToDB::selectFromTransactionTable(const std::string &key) {
    pqxx::result result;
    std::string sql = "select * from Transactions where key =\'"+key+"\'";
    if(!executeTransaction(sql, &result)) {
        LOG(WARNING)<<"selectFromBlockTable failed.";
    }
    return result;
}

void BlockStorageToDB::insertBlockTable(epoch_size_t epoch, const std::string &prehash, const std::string &hash,
                                        const std::string &metadata, const std::string &sign) {
    std::string epochs = std::to_string(epoch);
    executeTransaction("INSERT INTO Block"
                       " VALUES (\'" + epochs + "\', \'" + prehash + "\', \'" +
                       hash + "\', \'" + metadata + "\', \'" + sign + "\')");
}

void BlockStorageToDB::insertTransactionTable(tid_size_t tid, epoch_size_t epoch, const std::string &payload,
                                              const std::string &rwSet, const std::string &hash) {
    std::string tidStr = std::to_string(tid);
    std::string epochStr = std::to_string(epoch);
    executeTransaction("INSERT INTO Transactions"
             " VALUES (\'" + tidStr + "\', \'" + epochStr + "\', \'" +
             payload + "\', \'" + rwSet + "\', \'" + hash + "\')");
}

void BlockStorageToDB::initFunc() {
    createBlockTable();
    createTransactionTable();
}

bool BlockStorageToDB::appendBlock(const Block::Block &block) {
    return false;
}
