//
// Created by peng on 2021/3/18.
//

#include "block_server/comm/client_proxy/user_collector.h"
#include "block_server/transaction/transaction.h"
#include "common/sha256_hepler.h"
#include "common/yaml_config.h"
#include "common/compile_config.h"
#include "common/zmq/zmq_client.h"
#include "common/msp/crypto_helper.h"
#include "common/payload/mock_utils.h"

#include "comm.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"

UserCollector::UserCollector() : ZMQServer("0.0.0.0", "5001"), finishSignal(false) {
    epochServerClient = std::make_unique<ZMQClient>(YAMLConfig::getInstance()->getEpochServerIPs()[0], "6002");
}

UserCollector::~UserCollector() {
    finishSignal = true;
    collector->join();
    processor->join();
}

void UserCollector::startService() {
    // bring up collector
    collector = std::make_unique<std::thread>(&UserCollector::collectRequest, this);
    // bring up processor
    processor = std::make_unique<std::thread>(&UserCollector::processRequest, this);
}

void UserCollector::collectRequest() {
    // this func is run in a single thread
    while(!finishSignal) {
        auto message = std::make_unique<std::string>();
        getRequest(*message);
        sendReply();
        requestBuffer.enqueue(std::move(message));
    }
    LOG(INFO) << "User collector exited.";
}

void UserCollector::processRequest() {
    const CryptoSign* userSigner = CryptoHelper::getUserSigner(YAMLConfig::getInstance()->getLocalBlockServerIP());
    SHA256Helper sha256Helper;
    std::vector<std::unique_ptr<std::string>> items;
    items.resize(EPOCH_SIZE_APPROX);
    while(!finishSignal) {
        // TODO: time out is not accurate
        if (std::size_t itemSize = requestBuffer.wait_dequeue_bulk_timed(items.data(), EPOCH_SIZE_APPROX, DEQUEUE_TIMEOUT_US)) {
            // 1. iterator user payload raw: deserialize value and phrase from items[]
            std::vector<Transaction*> transactionList;
            for (int i = 0; i < itemSize; i++) {
                // 1.1 deserialize user request
                comm::UserRequest userRequest;
                userRequest.ParseFromString(*items[i]);
                // 1.2.1 check digest
                if(!userSigner->rsaDecrypt(userRequest.payload(), userRequest.digest())) {
                    CHECK(false);
                }
                // 1.2.2 add the signature to hash helper, for generate batch signature
                if (!sha256Helper.append(userRequest.digest())) {
                    CHECK(false);
                }
                // 1.3 construct transaction
                auto* transaction = Utils::makeTransaction();
                transaction->readOnly(false);
                // note: we dont set transaction id here!
                TransactionPayload* transactionPayload = transaction->getTransactionPayload();
                transactionPayload->ParseFromString(userRequest.payload());
                transactionPayload->set_digest(userRequest.digest());
                // 1.4 add to transaction list
                transactionList.push_back(transaction);
            }
            // 2. send request to epoch server
            comm::EpochRequest request;
            // 2.1---1.2.2 since we have iterated the tr list, now we can generate a batch digest.
            std::string digest;
            sha256Helper.execute(&digest);
            // 2.2 construct request and send it
            request.set_batch_hash(digest);
            request.set_request_batch_size(itemSize);
            // 2.3 send it, and get response from epoch server
            epochServerClient->sendRequest(request.SerializeAsString());
            // debug: check if current save epoch >= the epoch received from block server, error if true.
            // this could cause bug, because reading is NOT thread safe!
            std::string responseRaw;
            epochServerClient->getReply(responseRaw);
            comm::EpochResponse response;
            response.ParseFromString(responseRaw);
            // 2.4----1.3 append epoch
            for (Transaction* transaction: transactionList) {
                transaction->setEpoch(response.epoch());
            }
            // 3. call handle, send the list, the proof and the batch size
            newTransactionHandle(transactionList, responseRaw);
        }
    }
    LOG(INFO) << "User processor exited.";
}
