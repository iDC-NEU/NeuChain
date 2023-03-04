//
// Created by peng on 2021/3/18.
//

#include "block_server/comm/client_proxy/user_collector.h"
#include "block_server/transaction/transaction.h"
#include "common/sha256_hepler.h"
#include "common/yaml_config.h"
#include "common/zmq/zmq_client.h"
#include "common/msp/crypto_helper.h"
#include "common/payload/mock_utils.h"
#include "common/thread_pool.h"

#include "comm.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#include "common/consume_time_calculator.h"

// the max throughput of a single node is 20k, 100 requests per second
DEFINE_uint64(USER_REQUEST_BATCH_SIZE, 300, "the batch size for each epoch request");  /* NOLINT */
// the experiment span is 10ms
DEFINE_double(USER_REQUEST_BATCH_TIMEOUT, 0.01, "the timeout for each batch, default 10ms"); /* NOLINT */

UserCollector::UserCollector() : ZMQServer("0.0.0.0", "5001", zmq::socket_type::sub), finishSignal(false) {
    auto ips = YAMLConfig::getInstance()->getEpochServerIPs();
    if (minEpochServerCount > int(ips.size())) {
        LOG(FATAL) << "The threshold exceeds the total number of epoch servers";
        CHECK(false);
    }
    epochClientList.resize(minEpochServerCount);
    for(int i=0; i<clientSize; i++) {
        for (int j=0; j<minEpochServerCount; j++) {
            epochClientList[j].enqueue(new ZMQClient(ips[j], "6002"));
        }
    }
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
    pthread_setname_np(pthread_self(), "usr_req_coll");
    // this func is run in a single thread
    while(!finishSignal) {
        std::string message;
        getRequest(message);
        requestBuffer.enqueue(std::move(message));
    }
    LOG(INFO) << "User collector exited.";
}

void UserCollector::processRequest() {
    pthread_setname_np(pthread_self(), "usr_req_proc");
    const CryptoSign* userSigner = CryptoHelper::getUserSigner(YAMLConfig::getInstance()->getLocalBlockServerIP());
    const int workerSize = 2;
    ThreadPool workers(workerSize, "user_worker");
    ThreadPool receiver(clientSize, "async_recv");
    moodycamel::LightweightSemaphore sema;
    BlockBench::Timer userBatchTimer;
    userBatchTimer.start();
    // for lambda
    epoch_size_t latestEpoch = 1;
    std::vector<time_t> batchTimerList;
    while(!finishSignal) {
        std::vector<std::string> rawUserRequestList;
        rawUserRequestList.resize(FLAGS_USER_REQUEST_BATCH_SIZE);
        auto batchStartTime = BlockBench::Timer::timeNow();
        size_t requestSize;
        BlockBench::Timer::sleep(FLAGS_USER_REQUEST_BATCH_TIMEOUT - userBatchTimer.end());
        while (requestSize = requestBuffer.try_dequeue_bulk(rawUserRequestList.begin(), FLAGS_USER_REQUEST_BATCH_SIZE), !requestSize) {
            // we have not collect any transactions in a FLAGS_USER_REQUEST_BATCH_TIMEOUT duration
            BlockBench::Timer::sleep(FLAGS_USER_REQUEST_BATCH_TIMEOUT);
        }
        userBatchTimer.start();
        DLOG(INFO) << "Received a batch of user request, size: " << requestSize;
        rawUserRequestList.resize(requestSize);
        // 1. iterator user payload raw: deserialize value and phrase from items[]
        std::vector<Transaction*> transactionList;
        transactionList.resize(requestSize);
        for (int id=0; id<workerSize; id++) {
            workers.execute([&, workerId=id] {
                for (size_t i = workerId; i < requestSize; i+=workerSize) {
                    // 1.1 deserialize user request
                    comm::UserRequest userRequest;
                    userRequest.ParseFromString(rawUserRequestList[i]);
                    // 1.2.1 check digest
                    if (!userSigner->rsaDecrypt(userRequest.payload(), userRequest.digest())) {
                        CHECK(false);
                    }
                    // 1.3 construct transaction
                    auto *transaction = Utils::makeTransaction();
                    transaction->readOnly(false);
                    // note: we dont set transaction id here!
                    TransactionPayload *transactionPayload = transaction->getTransactionPayload();
                    transactionPayload->ParseFromString(userRequest.payload());
                    transactionPayload->set_digest(userRequest.digest());
                    // 1.4 add to transaction list
                    transactionList[i] = transaction;
                }
                sema.signal();
            });
        }
        // wait for all transaction is deserialized
        for (long i=workerSize; i>0; i-=sema.waitMany(i));
        // 2. send request to epoch server
        // 2.1---1.2.2 since we have iterated the transactionList, now we can generate a batch digest.
        std::string digest;
        SHA256Helper sha256Helper;
        for (const auto& transaction: transactionList) {
            // add the signature to hash helper, for generate batch signature
            if (!sha256Helper.append(transaction->getTransactionPayload()->digest())) {
                CHECK(false);
            }
        }
        sha256Helper.execute(&digest);
        // 2.2 construct request and send it
        comm::EpochRequest request;
        request.set_batch_hash(digest);
        request.set_request_batch_size(requestSize);
        // 2.3 send it, and get response from epoch server
        receiver.execute([this, transactionList=std::move(transactionList), request=std::move(request), &latestEpoch, batchTime=batchStartTime, &batchTimerList](){
            std::vector<ZMQClient*> clientList;
            clientList.resize(minEpochServerCount);
            for (size_t i=0; i<clientList.size(); i++) {
                epochClientList[i].wait_dequeue(clientList[i]);
            }
            std::string responseRaw;
            comm::EpochResponse response;
            while(true) {
                for (auto & client : clientList) {
                    client->sendRequest(request.SerializeAsString());
                }
                // debug: check if current save epoch >= the epoch received from block server, error if true.
                // this could cause bug, because reading is NOT thread safe!
                for (auto & client : clientList) {
                    client->getReply(responseRaw);
                }
                response.ParseFromString(responseRaw);
                std::lock_guard guard(pipelineLock);
                if (response.epoch() < latestEpoch)
                    continue;
                if (response.epoch() > latestEpoch) {
                    latestEpoch = response.epoch();
                    double totalSpan = 0;
                    for (const auto& time: batchTimerList) {
                        totalSpan += BlockBench::Timer::span(time);
                    }
                    DLOG(INFO) << "local epoch increase, epoch:" << response.epoch() << ", span=" << totalSpan / (int)batchTimerList.size() / 2;
                    batchTimerList.clear();
                }
                // for new epoch start calculate
                batchTimerList.push_back(batchTime);
                for (size_t i=0; i<clientList.size(); i++) {
                    epochClientList[i].enqueue(clientList[i]);
                }
                // 2.4----1.3 append epoch
                for (auto* transaction: transactionList) {
                    transaction->setEpoch(response.epoch());
                }
                // 3. call handle, send the list, the proof and the batch size
                newTransactionHandle(transactionList, responseRaw);
                return;
            }
        });
    }
    LOG(INFO) << "User processor exited.";
}
