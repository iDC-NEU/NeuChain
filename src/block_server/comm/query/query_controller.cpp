//
// Created by peng on 2021/4/22.
//

#include "block_server/comm/query/query_controller.h"
#include "block_server/worker/transaction_executor.h"
#include "block_server/transaction/transaction.h"
#include "block_server/database/block_info_helper.h"
#include "common/payload/mock_utils.h"
#include "common/zmq/zmq_server.h"
#include "common/zmq/zmq_client.h"
#include "common/yaml_config.h"

#include "comm.pb.h"
#include "block.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"

QueryController::QueryController(BlockInfoHelper* infoHelper)
        : handle(infoHelper), zmqServer{new ZMQServer("0.0.0.0", "7003")} {
    LOG(INFO) << "Query listener start at 0.0.0.0:7003";
    executor = TransactionExecutor::transactionExecutorFactory(TransactionExecutor::ExecutorType::query);
    collector = std::make_unique<std::thread>(&QueryController::run, this);
}

QueryController::~QueryController() {
    ZMQClient client("127.0.0.1", "7003");
    comm::UserQueryRequest userRequest;
    userRequest.set_type("exit");
    client.sendRequest(userRequest.SerializeAsString());
    client.getReply();
    collector->join();
}

void QueryController::run() {
    pthread_setname_np(pthread_self(), "query_ctl");
    const bool earlyReturn = YAMLConfig::getInstance()->enableEarlyReturn();
    while(true) {
        // 1.1 deserialize user request
        comm::UserQueryRequest userRequest;
        std::string buffer;
        zmqServer->getRequest(buffer);
        if(!userRequest.ParseFromString(buffer)) {
            zmqServer->sendReply();
            continue;
        }
        // exit signal, send an empty response and quit.
        const auto& command = userRequest.type();
        if(command == "exit") {
            zmqServer->sendReply();
            break;
        } else if (command == "key_query") {
            // 1.2 construct transaction
            auto* transaction = Utils::makeTransaction();
            TransactionPayload* transactionPayload = transaction->getTransactionPayload();
            transactionPayload->ParseFromString(userRequest.payload());
            transactionPayload->set_digest(userRequest.digest());
            // get previous tip
            auto previous = handle->getLatestSavedEpoch();
            executor->executeList({ transaction });
            if (previous != handle->getLatestSavedEpoch()) {
                LOG(WARNING) << "Read-only transaction dirty read!";
            }
            // 1.3 generate results
            comm::QueryResult result;
            for(const auto& read: transaction->getRWSet()->reads()) {
                *result.add_reads() = read;
            }
            delete transaction;
            // 1.4 send it
            zmqServer->sendReply(result.SerializeAsString());
        } else if (command == "block_query") {
            std::string serializedBlock;
            if (earlyReturn) {
                if (!handle->getPartialConstructedBlock(strtol(userRequest.payload().data(), nullptr, 10), serializedBlock)) {
                    LOG(WARNING) << "Block " << userRequest.payload().data() << " is not ready.";
                }
            } else {
                if (!handle->loadBlock(strtol(userRequest.payload().data(), nullptr, 10), serializedBlock)) {
                    LOG(WARNING) << "Block " << userRequest.payload().data() << " is not ready.";
                }
            }
            zmqServer->sendReply(serializedBlock);
        } else if (command == "tip_query") {
            if (earlyReturn) {
                zmqServer->sendReply(std::to_string(handle->getPartialConstructedLatestEpoch()));
            } else {
                zmqServer->sendReply(std::to_string(handle->getLatestSavedEpoch()));
            }
        } else {
            // unknown state
            zmqServer->sendReply();
        }
    }
}
