//
// Created by peng on 2021/6/29.
//

#include "block_server_test.h"
#include "block_server/database/orm/query.h"
#include "block_server/server.h"

void smallBankInit() {
    auto* storage = VersionedDB::getDBInstance()->getStorage();
    const std::string savingTab = "saving";
    const std::string checkingTab = "checking";
    const std::string BALANCE = "1000";
    for(int account=1; account<=TransactionGenerator::account_count; account++) {
        storage->updateWriteSet(savingTab + "_" + std::to_string(account), BALANCE, "small_bank");
        storage->updateWriteSet(checkingTab + "_" + std::to_string(account), BALANCE, "small_bank");
    }
    LOG(INFO) << "db init complete.";
}

void smallBankCheck() {
    auto* storage = VersionedDB::getDBInstance()->getStorage();
    const std::string savingTab = "saving";
    const std::string checkingTab = "checking";
    long totalBal = 0;
    for(int account=1; account<=TransactionGenerator::account_count; account++) {
        for(auto iterator = storage->selectDB(savingTab + "_" + std::to_string(account), "small_bank"); iterator->hasNext(); iterator->next()) {
            auto bal_str = iterator->getString("value");
            if(bal_str.empty())
                CHECK(false);
            else
                std::cout << bal_str << std::endl;
            totalBal += static_cast<int>(strtol(bal_str.data(), nullptr, 10));
        }
        for(auto iterator = storage->selectDB(checkingTab + "_" + std::to_string(account), "small_bank"); iterator->hasNext(); iterator->next()) {
            auto bal_str = iterator->getString("value");
            if(bal_str.empty())
                CHECK(false);
            else
                std::cout << bal_str << std::endl;
            totalBal += static_cast<int>(strtol(bal_str.data(), nullptr, 10));
        }
    }
    EXPECT_EQ(totalBal, TransactionGenerator::init_balance*TransactionGenerator::account_count*2);
    //3. join
    LOG(INFO) << "if failed, please ensure cleaned the db.";
}

TEST_F(BlockServerTest, SmallBankTest) { /* NOLINT */
    // 1. init first
    smallBankInit();
    // 2. test start
    auto* server = new DDChainServer([](epoch_size_t startWithEpoch) {
        return std::make_unique<MockTransactionComm>(startWithEpoch);
    });
    // test case need to load specific chaincode
    ChaincodeManager::reloadChaincodeManager("small_bank");
    server->initServer();
    server->startServer();
    // bench for 10 seconds
    sleep(1);
    server->stopServer();
    delete server;
    // 3. check it
    smallBankCheck();
}

#include "block_server/comm/mock/comm_stub.h"

TEST_F(BlockServerTest, LoadTest) { /* NOLINT */
    auto* server = new DDChainServer([](epoch_size_t startWithEpoch) {
        return std::make_unique<CommStub>(startWithEpoch);
    });
    // test case need to load specific chaincode
    ChaincodeManager::reloadChaincodeManager("test");
    server->initServer();
    server->startServer();
    // bench for 10 seconds
    sleep(1);
    server->stopServer();
    delete server;
}

#include "block_server/comm/mock/mock_comm.h"

TEST_F(BlockServerTest, correctTest) { /* NOLINT */
    auto comm = std::make_unique<MockComm>();
    auto* commPtr = comm.get();
    auto* server = new DDChainServer([&](epoch_size_t startWithEpoch) {
        return std::move(comm);
    });
    // test case need to load specific chaincode
    ChaincodeManager::reloadChaincodeManager("test");
    server->initServer();
    server->startServer();
    // bench for 1 seconds
    sleep(1);
    LOG(INFO) << "Reuse Tx must set off!";
    auto& txMap = commPtr->txMap;
    EXPECT_EQ(txMap[1]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[2]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[3]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[6]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[4]->getTransactionResult(), TransactionResult::ABORT);
    EXPECT_EQ(txMap[5]->getTransactionResult(), TransactionResult::ABORT);
    EXPECT_EQ(txMap[7]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[8]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[9]->getTransactionResult(), TransactionResult::COMMIT);
    EXPECT_EQ(txMap[10]->getTransactionResult(), TransactionResult::COMMIT);
}
