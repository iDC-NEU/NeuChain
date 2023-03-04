//
// Created by peng on 2021/7/26.
//

#include "block_server/comm/query/tx_execution_receiver.h"
#include "block_server/worker/transaction_executor.h"
#include "block_server/transaction/transaction.h"
#include "common/payload/mock_utils.h"
#include "common/zmq/zmq_server.h"
#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"
#include <common/sha256_hepler.h>
#include "comm.pb.h"
#include "transaction.pb.h"
#include "glog/logging.h"

// copy from query executor
TxExecutionReceiver::TxExecutionReceiver(BlockInfoHelper *infoHelper)
        : handle(infoHelper),
          threadPool(20),
          requestServer{new ZMQServer("0.0.0.0", "8999", zmq::socket_type::sub)},
          resultServer{new ZMQServer("0.0.0.0", "9001", zmq::socket_type::pub)},
          localIp{YAMLConfig::getInstance()->getLocalBlockServerIP()} {
    executor = TransactionExecutor::transactionExecutorFactory(TransactionExecutor::ExecutorType::eov);
    requestServerThread = std::make_unique<std::thread>(&TxExecutionReceiver::runRequestServer, this);
    resultServerThread = std::make_unique<std::thread>(&TxExecutionReceiver::runResultServer, this);
}

//todo: fix destructor
TxExecutionReceiver::~TxExecutionReceiver() = default;

void TxExecutionReceiver::runRequestServer() {
    const CryptoSign* userSigner = CryptoHelper::getUserSigner(YAMLConfig::getInstance()->getLocalBlockServerIP());
    const CryptoSign* blockServerSigner = CryptoHelper::getLocalBlockServerSigner();
    while (!finishSignal) {
        auto* requestRaw = new std::string;
        requestServer->getRequest(*requestRaw);
        // generate an operation
        auto execTxFunc = [this, requestRaw, userSigner, blockServerSigner]() {
            // --------------copy from user collector
            // 1.1 deserialize user request
            comm::UserRequest userRequest;
            CHECK(userRequest.ParseFromString(*requestRaw));
            delete requestRaw;
            // 1.2.1 check digest
            if(!userSigner->rsaDecrypt(userRequest.payload(), userRequest.digest())) {
                CHECK(false);
            }
            SHA256Helper sha256Helper;
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
            transactionPayload->clear_endorser_digest();
            //--------------------------------
            executor->executeList({ transaction });
            // 1.3 generate results
            std::string transactionRaw;
            transaction->serializeToString(&transactionRaw);
            // generate endorser signature
            std::string endorserSignature;
            blockServerSigner->rsaEncrypt(transactionRaw, &endorserSignature);
            // add endorser signature to transaction
            EndorserDigest endorserDigest;
            endorserDigest.set_ip(localIp);
            endorserDigest.set_signature(endorserSignature);
            transactionPayload->add_endorser_digest(endorserDigest.SerializeAsString());
            // re-serialize transaction and send it.
            transaction->serializeToString(&transactionRaw);
            resultQueue.enqueue(std::move(transactionRaw));
            delete transaction;
        };
        // move to thread pool
        threadPool.execute(std::move(execTxFunc));
    }
}

void TxExecutionReceiver::runResultServer() {
    while (!finishSignal) {
        std::string transactionRaw;
        resultQueue.wait_dequeue(transactionRaw);
        if (resultQueue.size_approx() > 5000) {
            LOG(INFO) << "too many request";
        }
        resultServer->sendReply(transactionRaw);
    }
}
