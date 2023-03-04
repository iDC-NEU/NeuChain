//
// Created by peng on 5/10/21.
//

#include "block_server/database/block_broadcaster.h"
#include "common/zmq/zmq_server_listener.h"
#include "common/zmq/zmq_server.h"
#include "common/base64.h"
#include "common/yaml_config.h"
#include "block.pb.h"
#include "comm.pb.h"

#include "glog/logging.h"

BlockBroadcaster::BlockBroadcaster() :finishSignal(false) {
    validatedBlockNumber.resize(10000);
    auto* config = YAMLConfig::getInstance();
    blockVerifierThread = std::make_unique<std::thread>(&BlockBroadcaster::blockVerifier, this);
    // 1.1. Establish server to server connection
    listener = std::make_unique<ServerListener>(&BlockBroadcaster::getSignatureFromOtherServer, this, config->getBlockServerIPs(), "7002");
}

BlockBroadcaster::~BlockBroadcaster() {
    finishSignal = true;
    blockVerifierThread->join();
}

void BlockBroadcaster::getSignatureFromOtherServer(const std::string &ip, ZMQClient *client) {
    pthread_setname_np(pthread_self(), "blk_ver_cli");
    // code reentry, connect to other server
    while(!finishSignal) {
        std::string messageRaw;
        client->getReply(messageRaw);
        auto* signatureExchange = new comm::SignatureExchange;
        signatureExchange->ParseFromString(messageRaw);
        DLOG(INFO) << "Exchange epoch: " << signatureExchange->epoch() <<
                   ", ip: " << signatureExchange->ip() <<
                   ", signature: " << base64_encode(signatureExchange->signature());
        CHECK(signatureVerifyCallback(signatureExchange->epoch(), signatureExchange->ip(), signatureExchange->signature()));
        std::lock_guard guard(validatedBlockNumberMutex);
        if (!validatedBlockNumber[signatureExchange->epoch()]) {
            DLOG(INFO) << "validated epoch=" << signatureExchange->epoch();
            validatedBlockNumber[signatureExchange->epoch()] = true;
        }
    }
}

void BlockBroadcaster::blockVerifier() {
    pthread_setname_np(pthread_self(), "blk_ver_ser");
    std::unique_ptr<Block::Block> block;
    ZMQServer server("0.0.0.0","7002", zmq::socket_type::pub);
    while (!finishSignal) {
        if (!verifyBuffer.wait_dequeue_timed(block, std::chrono::seconds(5))) {
            LOG(WARNING) << "block verifier haven't receive block for 5 seconds, please check the sender health.";
            continue;
        }
        CHECK(block->metadata().metadata_size() >= 3);
        // 1. trMetadata
        // 2. merkleRoot
        // 3. block signature
        auto* signatureExchange = new comm::SignatureExchange;
        signatureExchange->set_signature(block->metadata().metadata(2));
        signatureExchange->set_ip(YAMLConfig::getInstance()->getLocalBlockServerIP());
        signatureExchange->set_epoch(block->header().number());
        // broadcast the signature
        server.sendReply(signatureExchange->SerializeAsString());
    }
    LOG(INFO) << "block verifier receive exit signal.";
}
