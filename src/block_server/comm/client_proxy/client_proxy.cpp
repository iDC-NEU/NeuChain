//
// Created by peng on 2021/6/20.
//

#include "block_server/comm/client_proxy/client_proxy.h"
#include "block_server/comm/client_proxy/user_collector.h"
#include "block_server/transaction/transaction.h"
#include "common/base64.h"
#include "common/msp/crypto_helper.h"
#include "common/payload/mock_utils.h"
#include "common/sha256_hepler.h"
#include "common/yaml_config.h"
#include "common/zmq/zmq_server.h"
#include "common/zmq/zmq_server_listener.h"
#include "common/zmq/zmq_client.h"
#include "comm.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"


ClientProxy::ClientProxy(epoch_size_t startWithEpoch):
        epoch(startWithEpoch), finishSignal(false),
        localServer(new ZMQServer("0.0.0.0", "7001", zmq::socket_type::pub)),
        barrier(YAMLConfig::getInstance()->getBlockServerCount(), [this](){
            LOG(INFO) << "All threads reached barrier, epoch: " << this->epoch << ", count: " << this->barrier.get_current_waiting();
            this->epoch++;
        }), epochServerSignHelper(CryptoHelper::getEpochServerSigner()) { }

ClientProxy::~ClientProxy() {
    finishSignal = true;
    userCollectorThread->join();
    localReceiverThread->join();
}

void ClientProxy::startService() {
    //0. init
    userCollector = std::make_unique<UserCollector>();
    userCollector->newTransactionHandle = [this](const std::vector<Transaction*>& transaction, const std::string& proof) {
        this->pendingTransactions.enqueue(std::make_pair(proof, transaction));
    };
    // start collector
    userCollector->startService();
    // 2. bring up local receiver (remote sender)
    localReceiverThread = std::make_unique<std::thread>(&ClientProxy::receiveFromLocalServer, this);
    // 3.1. Establish server to server connection
    serverListener = std::make_unique<ServerListener>(&ClientProxy::receiveFromOtherServer, this, YAMLConfig::getInstance()->getBlockServerIPs(), "7001");
}

void ClientProxy::receiveFromLocalServer() {
    // send to remote server and receive from local server
    comm::ProxyBroadcast transactionSetMulti;
    // ---- tmp variable ----
    comm::EpochResponse response;
    while (!finishSignal) {
        // 1. get a batch of trs and related proof.
        std::pair<std::string, std::vector<Transaction*>> pair;
        pendingTransactions.wait_dequeue(pair);
        // 1.1 get response data
        auto& responseRaw = pair.first;
        auto& transactionList = pair.second;
        response.ParseFromString(responseRaw);
        // 2. check if current local epoch is over, broadcast to other server.
        // change if to while, when current epoch tx==0, move on.
        while (response.epoch() > epoch) {
            // local current transactions has finished
            transactionSetMulti.set_epoch(epoch);
            // broadcast to all block servers
            localServer->sendReply(transactionSetMulti.SerializeAsString());
            transactionSetMulti.Clear();
            // wait until all trs in this epoch collected. --local
            barrier.await();
            // after this epoch += 1, can safely enqueue
        }
        // 3. generate a transaction set.
        CHECK(epoch == response.epoch());
        auto* transactionSet = transactionSetMulti.add_set();
        transactionSet->set_proof(responseRaw);
        SHA256Helper sha256Helper;
        for(const auto& transaction: transactionList) {
            CHECK(epoch == transaction->getEpoch());
            // size always >=1, so ok
            std::string *transactionRaw = transactionSet->add_transaction();
            // serialize transaction here
            CHECK(transaction->serializeToString(transactionRaw));
            // 3.1 add to epochBroadcastTransactionsHandle queue (send to local server)
            // generate transaction id
            sha256Helper.append(response.signature());
            sha256Helper.append(transaction->getTransactionPayload()->digest());
            std::string digest;
            sha256Helper.execute(&digest);
            transaction->setTransactionID(*reinterpret_cast<tid_size_t*>(digest.data()));
            // enqueue transaction
            epochBroadcastTransactionsHandle(transaction);
        }
        if(!verifyBatchHash(transactionList, response)) {
            LOG(ERROR) << "local server recv thread: signature error!";
        }
    }
}

void ClientProxy::receiveFromOtherServer(const std::string &ip, ZMQClient *client) {
    const CryptoSign* userSigner = CryptoHelper::getUserSigner(ip);
    // code reentry, connect to other server
    while(!finishSignal) {
        std::string messageRaw;
        client->getReply(messageRaw);

        comm::ProxyBroadcast transactionSetMulti;
        transactionSetMulti.ParseFromString(messageRaw);

        for(const auto& transactionSet: transactionSetMulti.set()) {
            // verify batch hash
            comm::EpochResponse response;
            response.ParseFromString(transactionSet.proof());
            std::vector<Transaction*> transactionList;
            transactionList.reserve(transactionSet.transaction().size());
            for (const auto &transactionRaw :transactionSet.transaction()) {
                // deserialize transaction here
                auto *transaction = Utils::makeTransaction(transactionRaw);
                // check user digest that from other servers
                TransactionPayload payload = *transaction->getTransactionPayload();
                payload.clear_digest();
                if(!userSigner->rsaDecrypt(payload.SerializeAsString(), transaction->getTransactionPayload()->digest())) {
                    LOG(ERROR) << "tx payload signature error!";
                    CHECK(false);
                }
                transactionList.push_back(transaction);
            }
            if(!verifyBatchHash(transactionList, response)) {
                LOG(ERROR) << "remote server recv thread: signature error!";
            }
            // calculate tid
            SHA256Helper sha256Helper;
            for (const auto &transaction :transactionList) {
                // add to broadcast queue
                CHECK(transaction->getEpoch() == epoch);
                // generate transaction id
                sha256Helper.append(response.signature());
                sha256Helper.append(transaction->getTransactionPayload()->digest());
                std::string digest;
                sha256Helper.execute(&digest);
                transaction->setTransactionID(*reinterpret_cast<tid_size_t*>(digest.data()));
                // enqueue transaction
                epochBroadcastTransactionsHandle(transaction);
            }
        }
        barrier.await();
    }
}

bool ClientProxy::verifyBatchHash(const std::vector<Transaction*>& transactionList, const comm::EpochResponse &response) const {
    if(transactionList.size() != response.request().request_batch_size()) {
        LOG(ERROR) << "pre-check: batch size error!";
        return false;
    }
    SHA256Helper sha256Helper;
    for(const auto& transaction: transactionList) {
        sha256Helper.append(transaction->getTransactionPayload()->digest());
    }
    std::string digest;
    sha256Helper.execute(&digest);
    if(response.request().batch_hash() != digest) {
        LOG(ERROR) << "pre-check: batch hash error, print detail:";
        LOG(ERROR) << "transactions digest: ";
        for(const auto& transaction: transactionList) {
            LOG(ERROR) << base64_encode(transaction->getTransactionPayload()->digest());
        }
        LOG(ERROR) << "size: " << transactionList.size();
        LOG(ERROR) << "response batch hash: " << base64_encode(response.request().batch_hash());
        LOG(ERROR) << "calculated digest: " << base64_encode(digest);
        return false;
    }

    std::string message = std::to_string(response.epoch()) +
                          response.request().batch_hash() +
                          std::to_string(response.nonce());
    // verify signature
    if(!epochServerSignHelper->rsaDecrypt(message, response.signature()))
        return false;
    return true;
}
