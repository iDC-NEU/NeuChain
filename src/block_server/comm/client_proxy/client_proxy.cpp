//
// Created by peng on 2021/6/20.
//

#include "block_server/comm/client_proxy/client_proxy.h"
#include "block_server/comm/client_proxy/epoch_txn_queue_manager.h"
#include "block_server/comm/client_proxy/epoch_transaction_buffer.h"
#include "block_server/comm/client_proxy/user_collector.h"
#include "common/concurrent_queue/light_weight_semaphore.hpp"
#include "common/consume_time_calculator.h"
#include "common/thread_pool.h"
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

ClientProxy::ClientProxy(epoch_size_t _startWithEpoch):
        startWithEpoch(_startWithEpoch), finishSignal(false),
        localServer(new ZMQServer("0.0.0.0", "7001", zmq::socket_type::pub)),
        pendingTransactions(10),
        epochServerSignHelper(CryptoHelper::getEpochServerSigner()) {
    auto* configPtr = YAMLConfig::getInstance();
    // init ccb
    ccb = ConsensusControlBlock::getInstance();
    ccb->setConsensusCount(static_cast<int>(configPtr->getBlockServerCount()));
    queueManager = new EpochTxnQueueManager(startWithEpoch, configPtr->getBlockServerCount());

    // include local block server
    for (const auto& allServerIp: configPtr->getBlockServerIPs()) {
        invalidBits[allServerIp] = std::make_pair(0, false);
    }
    ccb->setMaliciousCallbackHandle([this](const std::string& maliciousId, epoch_size_t maliciousEpoch) {
        // disable the current receiver
        // we dont need lock for invalidBits since it is static
        auto& pair = invalidBits[maliciousId];
        if (pair.first != 0) {
            CHECK(false);   // no re-enter
        }
        LOG(WARNING) << "Receive view change signal for " << maliciousId << " at epoch = " << maliciousEpoch;
        pair.first = maliciousEpoch;
        pair.second.store(true, std::memory_order_release);
    });
}

ClientProxy::~ClientProxy() {
    finishSignal = true;
    userCollectorThread->join();
    localReceiverThread->join();
    delete queueManager;
}

void ClientProxy::startService() {
    //0. init
    userCollector = std::make_unique<UserCollector>();
    userCollector->newTransactionHandle = [this](const std::vector<Transaction*>& transaction, const std::string& proof) {
        this->pendingTransactions.push(std::make_pair(proof, transaction));
    };
    // start collector
    userCollector->startService();
    // 2. bring up local receiver (remote sender)
    localReceiverThread = std::make_unique<std::thread>(&ClientProxy::receiveFromLocalServer, this);
    // 3.1. Establish server to server connection
    serverListener = std::make_unique<ServerListener>(&ClientProxy::receiveFromOtherServer, this, YAMLConfig::getInstance()->getBlockServerIPs(), "7001");
}

void ClientProxy::receiveFromLocalServer() {
    pthread_setname_np(pthread_self(), "proxy_local");
    // get block server id for opening port
    auto* configPtr = YAMLConfig::getInstance();
    // send to remote server and receive from local server
    // ---- tmp variable ----
    comm::TransactionsWithProof proofSet;
    epoch_size_t epoch = startWithEpoch;
    ConsumeTimeCalculator consumeTimeCalculator;
#ifndef NO_TIME_BENCHMARK
    std::function<void(Transaction *)> localEpochBroadcastTransactionsHandle = [&consumeTimeCalculator, this](Transaction* txn){
        auto epoch = consumeTimeCalculator.getCurrentEpoch();
        if (epoch < (int)txn->getEpoch()) {
            consumeTimeCalculator.endCalculate(epoch);
            consumeTimeCalculator.getAverageResult(epoch, "client_proxy@local");
            consumeTimeCalculator.setEpoch(txn->getEpoch());
        }
        epochBroadcastTransactionsHandle(txn);
    };
    EpochTransactionBuffer localTxBuffer(localEpochBroadcastTransactionsHandle, queueManager);
#else
    EpochTransactionBuffer localTxBuffer(this->epochBroadcastTransactionsHandle, queueManager);
#endif
    while (!finishSignal) {
        // 1. get a batch of trs and related proof.
        std::pair<std::string, std::vector<Transaction*>> pair;
        pendingTransactions.pop(pair);
        // 1.1 get response data
        auto& responseRaw = pair.first;
        auto& transactionList = pair.second;
        comm::EpochResponse response;
        response.ParseFromString(responseRaw);
        // 2. check if current local epoch is over, broadcast to other server.
        // change if to while, when current epoch tx==0, move on.
        while (response.epoch() > epoch) {
            // send via consensus protocol
            // send proof list with consensus
            proofSet.set_epoch(epoch);
            proofSet.set_ip(configPtr->getLocalBlockServerIP());
            proofSet.set_type(comm::TransactionsWithProof_Type_CON);   // type is useless
            // broadcast to all block servers
            const auto rawProofSet = proofSet.SerializeAsString();
            localServer->sendReply(rawProofSet);
            // TODO: we currently use only a raft protocol.
            ccb->sendEpochFinishSignal(rawProofSet);
            proofSet.Clear();
            // wait until all trs in this epoch collected. --local
            // we must also calculate the BARRIER waiting time
            localTxBuffer.asyncEmptyBuffer(epoch++);
            // after this epoch += 1, can safely enqueue
        }
        // we only calculate the txn msg
        // start AFTER pendingTransactions.wait_dequeue, the dequeue overhead is calculated by UserCollector
        consumeTimeCalculator.startCalculate((int)epoch);
        // a signature is enough to make consensus
        proofSet.add_data(response.signature());
        // 3. generate a transaction set.
        CHECK(epoch == response.epoch());
        SHA256Helper sha256Helper;
        comm::TransactionsWithProof transactionSet;
        transactionSet.set_epoch(epoch);
        transactionSet.set_type(comm::TransactionsWithProof_Type_TX);   // type is useless
        transactionSet.set_proof(responseRaw);
        for(const auto& transaction: transactionList) {
            CHECK(epoch == transaction->getEpoch());
            // size always >=1, so ok
            std::string *transactionRaw = transactionSet.add_data();
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
            localTxBuffer.pushTransactionToBuffer(transaction);
        }
        // local server generate this information, no need to verify
//        if(!verifyBatchHash(transactionList, response)) {
//            LOG(ERROR) << "local server recv thread: signature error!";
//            CHECK(false);
//        }
        // ---- eager send transactionList
        // local current transactions has finished
        // broadcast to all block servers
        localServer->sendReply(transactionSet.SerializeAsString());
    }
}

void ClientProxy::receiveFromOtherServer(const std::string &ip, ZMQClient *client) {
    pthread_setname_np(pthread_self(), "proxy_remote");
    // get block server id for opening port
    const CryptoSign* userSigner = CryptoHelper::getUserSigner(ip);
    // code reentry, connect to other server
    epoch_size_t epoch = startWithEpoch;
    std::set<std::string> currentEpochProofSet;
    const int workerSize = 2;
    ThreadPool workers(workerSize, "p_worker_"+ip);
    moodycamel::LightweightSemaphore sema;
    moodycamel::BlockingConcurrentQueue<std::string> remoteQueue;
    std::thread asyncGetFromRemoteThread([client, &remoteQueue, this]{
        while (!finishSignal) {
            std::string messageRaw;
            client->getReply(messageRaw);
            remoteQueue.enqueue(std::move(messageRaw));
        }
    });
    ConsumeTimeCalculator consumeTimeCalculator;
#ifndef NO_TIME_BENCHMARK
    std::function<void(Transaction *)> remoteEpochBroadcastTransactionsHandle = [&consumeTimeCalculator, this, &ip](Transaction* txn){
        auto epoch = consumeTimeCalculator.getCurrentEpoch();
        if (epoch < (int)txn->getEpoch()) {
            consumeTimeCalculator.endCalculate(epoch);
            consumeTimeCalculator.getAverageResult(epoch, "client_proxy@" + ip);
            consumeTimeCalculator.setEpoch(txn->getEpoch());
        }
        epochBroadcastTransactionsHandle(txn);
    };
    EpochTransactionBuffer remoteTxBuffer(remoteEpochBroadcastTransactionsHandle, queueManager);
#else
    EpochTransactionBuffer remoteTxBuffer(this->epochBroadcastTransactionsHandle, queueManager);
#endif
    try {
        while (!finishSignal) {
            std::string messageRaw;
            while (!remoteQueue.wait_dequeue_timed(messageRaw, RAFT_TIMEOUT_MS * 1000 / 10)) {
                // 100ms
                // cuz the blocking impl, we need to check if the bit is still valid;
                if (invalidBits[ip].second.load(std::memory_order_acquire) && invalidBits[ip].first <= epoch) {
                    LOG(ERROR) <<"InvalidClientProxyException";
                    throw InvalidClientProxyException();
                }
            }
            comm::TransactionsWithProof transactionSet;
            CHECK(transactionSet.ParseFromString(messageRaw));
            // we first determine ctl message
            // TODO: remove  transactionSet.type() == comm::TransactionsWithProof_Type_CON
            if (transactionSet.type() == comm::TransactionsWithProof_Type_CON) {
                //if (response.epoch() > epoch) {
                // we get proof from consensus protocol
                // wait until we have all transactions in a epoch
                comm::TransactionsWithProof &proofSet = transactionSet;
                // TODO: get from consensus server
                CHECK(epoch == proofSet.epoch());
                for (const auto &proof: proofSet.data()) {
                    CHECK(currentEpochProofSet.count(proof));
                }
                CHECK(static_cast<size_t>(proofSet.data_size()) == currentEpochProofSet.size());
                // calculate the barrier time
                currentEpochProofSet.clear();
                remoteTxBuffer.asyncEmptyBuffer(epoch++);
                continue;
            }
            // we only calculate the txn msg
            consumeTimeCalculator.startCalculate((int)epoch);
            // verify batch hash
            comm::EpochResponse response;
            response.ParseFromString(transactionSet.proof());
            if(epoch != response.epoch()) {
                LOG(ERROR) << "remote server recv thread: epoch error!";
                throw MaliciousClientProxyException();
            }
            currentEpochProofSet.insert(response.signature());
            std::vector<Transaction *> transactionList;
            transactionList.resize(transactionSet.data_size());

            int payloadSignatureValidFlag = true;
            for (int id=0; id<workerSize; id++) {
                workers.execute([&, workerId=id] {
                    for (int i = workerId; i < transactionSet.data_size(); i+=workerSize) {
                        // deserialize transaction here
                        auto *transaction = Utils::makeTransaction(transactionSet.data(i));
                        transaction->setInvalidBit(&invalidBits[ip]);
                        // check user digest that from other servers
                        TransactionPayload payload = *transaction->getTransactionPayload();
                        payload.clear_digest();
                        if (!userSigner->rsaDecrypt(payload.SerializeAsString(),
                                                    transaction->getTransactionPayload()->digest())) {
                            LOG(ERROR) << "tx payload signature error!";
                            payloadSignatureValidFlag = false;
                            break;
                        }
                        transactionList[i] = transaction;
                    }
                    sema.signal();  // deserialize and push into list
                });
            }

            // wait for all transaction is deserialized
            for (long i=workerSize; i>0; i-=sema.waitMany(i));
            if (!payloadSignatureValidFlag || !verifyBatchHash(transactionList, response)) {
                LOG(ERROR) << "remote server recv thread: signature error!";
                throw MaliciousClientProxyException();
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
                transaction->setTransactionID(*reinterpret_cast<tid_size_t *>(digest.data()));
                // enqueue transaction
                remoteTxBuffer.pushTransactionToBuffer(transaction);
            }
        }
    } catch (InvalidClientProxyException& e) {
        // the remote server is invalided by another process
    } catch (MaliciousClientProxyException& e) {
        ccb->broadcastMaliciousViewChange(ip, epoch);
    }
    while (!finishSignal) {
        remoteTxBuffer.asyncEmptyBuffer(epoch++);
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
