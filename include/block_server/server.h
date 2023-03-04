//
// Created by peng on 2021/6/29.
//

#ifndef NEUBLOCKCHAIN_SERVER_H
#define NEUBLOCKCHAIN_SERVER_H

#include "block_server/coordinator/aggregation_coordinator.h"
#include "block_server/database/database.h"
#include "block_server/transaction/impl/transaction_manager_impl.h"
#include "block_server/worker/chaincode_manager.h"
#include "block_server/comm/query/query_controller.h"
#include "block_server/comm/query/tx_execution_receiver.h"
#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"

#include "block_server/comm/comm.h"
#include "block_server/database/impl/block_generator_impl.h"

#include <thread>
#include <functional>
#include <utility>

class DDChainServer {
public:
    explicit DDChainServer(std::function<std::unique_ptr<Comm>(epoch_size_t)> commHandle)
            :createCommInstanceHandle(std::move(commHandle)) {}
    virtual ~DDChainServer() = default;
    void initServer() {
        // 1. ---- init light weight component
        // init config
        yamlConfig = YAMLConfig::getInstance();
        // init crypto
        CryptoHelper::initCryptoSign(CryptoHelper::ServerType::blockServer);
        // init all chaincode
        ChaincodeManager::InitChaincodeManager();
        // 2. ---- init other component
        // 2.1 init db and db storage
        VersionedDB::getDBInstance();
        // 2.2 init tx mgr, which will init rpc and blk storage.
        transactionManager = std::make_unique<TransactionManagerImpl>(yamlConfig->trManagerShowDetail(), yamlConfig->trManagerReuseAbortTransaction());
        // init comm module
        // move block storage into block generator
        auto blockGenerator = std::make_unique<BlockGeneratorImpl>();
        // 2.3 since the blk storage is inited, we init query interface.
        // because the query interface uses different logic, we dont init it in tx mgr.
        queryInterface = std::make_unique<QueryController>(blockGenerator.get());
        eovInterface = std::make_unique<TxExecutionReceiver>(blockGenerator.get());
        transactionManager->setCommunicateModule(createCommInstanceHandle(blockGenerator->getLatestSavedEpoch() + 1));
        // init block generator
        transactionManager->setBlockGenerator(std::move(blockGenerator));
        // connect coordinator and tx manager with epoch signal and tx handler.
        epoch_size_t startWithEpoch = transactionManager->getProcessingEpoch();
        // create ariaCoordinator
        coordinator = yamlConfig->useAggregation() ? std::make_unique<AggregationCoordinator>(startWithEpoch) : std::make_unique<AriaCoordinator>(startWithEpoch);
        // setup workload
        transactionManager->sendTransactionHandle = [this](std::unique_ptr<std::vector<Transaction*>> trWrapper) {
            //send transactions to coordinator to process it
            coordinator->addTransaction(std::move(trWrapper));
        };
        coordinator->epochTransactionFinishSignal = [this](epoch_size_t epoch) {
            //send back processed transactions from coordinator
            transactionManager->epochFinishedCallback(epoch);
        };
    }

    void startServer() {
        // 3. run the system
        coordinatorThread = std::make_unique<std::thread>(&AriaCoordinator::run, coordinator.get());
        txMgrThread = std::make_unique<std::thread>(&TransactionManagerImpl::run, transactionManager.get());
    }

    void joinServer() {
        coordinatorThread->join();
        txMgrThread->join();
    }

    void stopServer() {
        // TODO: stop the block generator first.
        // send stop to coordinator first.
        coordinator->stop();
        coordinatorThread->join();
        // after we stop it, we must stop the sender.
        transactionManager->stop();
        txMgrThread->join();
        transactionManager = nullptr;
        coordinator = nullptr;
    }

protected:
    const std::function<std::unique_ptr<Comm>(epoch_size_t)> createCommInstanceHandle;

private:
    YAMLConfig* yamlConfig{};
    std::unique_ptr<TransactionManagerImpl> transactionManager;
    std::unique_ptr<std::thread> txMgrThread;
    std::unique_ptr<AriaCoordinator> coordinator;
    std::unique_ptr<std::thread> coordinatorThread;
    std::unique_ptr<QueryController> queryInterface;
    std::unique_ptr<TxExecutionReceiver> eovInterface;
};

#endif //NEUBLOCKCHAIN_SERVER_H
