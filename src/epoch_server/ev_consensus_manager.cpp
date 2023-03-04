//
// Created by peng on 2021/9/10.
//

#include "epoch_server/ev_consensus_manager.h"
#include "common/aria_types.h"
#include "common/msp/crypto_helper.h"
#include "common/zmq/zmq_server.h"
#include "comm.pb.h"

EVConsensusManager::EVConsensusManager(const consensus_param &param, brpc::Server* server)
        : ConsensusManager(param.initial_conf, param.raft_ip, param.raft_port, "epoch_server_group", server, param.group_id, param.batch_size) {
    server_count = 3;
    duration = param.duration;
    request_receiver_port = param.request_receiver_port;
    chain->transferLeader(YAMLConfig::getInstance()->getEpochServerIPs()[0], param.raft_port, param.group_id);
}

void EVConsensusManager::receiver_from_raft() {
    std::map<size_t, size_t> epochApproveMap;
    while (!brpc::IsAskedToQuit()) {
        std::string epoch_request_raw;
        get_message_from_raft(epoch_request_raw);
        raft::IncreaseEpochRequest epoch_request;
        epoch_request.ParseFromString(epoch_request_raw);
        auto expect_epoch = epoch_request.expect_epoch();
        auto& approve_count_ref = epochApproveMap[expect_epoch];
        approve_count_ref += 1;
        DLOG(INFO) << "get a new message from raft, epoch: " << expect_epoch;
        if (// approve_count_ref > server_count/2 &&
            expect_epoch > current_epoch.load(std::memory_order_acquire)) {
            // most server approve, new epoch
            DLOG(INFO) << "recv response, set epoch to " << expect_epoch;
            current_epoch.store(expect_epoch, std::memory_order_release);
        }
    }
}

void EVConsensusManager::receiver_from_user() {
    LOG(INFO) << "set epoch duration to " << duration << ".";
    std::map<epoch_size_t, size_t> epochSizeMap;
    ZMQServer server("0.0.0.0", std::to_string(request_receiver_port));
    auto* epochServerSigner = CryptoHelper::getEpochServerSigner();
    size_t totalSize = 0;
    BlockBench::Timer timer;
    bool sent_increase_message = false;
    while (!brpc::IsAskedToQuit()) {
        // server receive from user-proxy client, block
        std::string requestRaw;
        server.getRequest(requestRaw);
        // after get a request, we first obtain the epoch number.
        auto epochNum = current_epoch.load(std::memory_order_acquire);
        // first, we check if current epoch size == 0, to reset timer
        if(!epochSizeMap.count(epochNum)) {
            if (epochNum != 1)
                LOG(INFO) << "Epoch: "<< epochNum - 1 << ", transaction size: " << epochSizeMap[epochNum - 1];
            timer.start();
            sent_increase_message = false;
        }
        if(timer.end() > duration && !sent_increase_message) {
            // else check if timer is out
            // we only send message once in an epoch
            raft::IncreaseEpochRequest epoch_request;
            epoch_request.set_expect_epoch(epochNum+1);
            // send epoch increase signal
            DLOG(INFO) << "send to raft, epoch: " << epochNum+1;
            push_message_to_raft(epoch_request.SerializeAsString());
            sent_increase_message = true;
            // we do not reset epoch start time in here
        } // NOTE: we still use current epoch, because we have not get enough sign from epoch+1
        // second, process request and update env value
        comm::EpochResponse response;
        comm::EpochRequest* request = response.mutable_request();
        request->ParseFromString(requestRaw);
        size_t currentRequestSize = request->request_batch_size();

        totalSize += currentRequestSize;
        epochSizeMap[epochNum] += currentRequestSize;

        // send response
        response.set_epoch(epochNum);
        response.set_nonce(totalSize);
        // calculate signature
        std::string signature;
        std::string message = std::to_string(response.epoch()) +
                              request->batch_hash() +
                              std::to_string(response.nonce());
        epochServerSigner->rsaEncrypt(message, &signature);
        // append signature
        response.set_signature(signature);
        server.sendReply(response.SerializeAsString());
        // print transaction info
        DLOG(INFO) << "-----------------------";
        DLOG(INFO) << "transaction size: " << currentRequestSize;
        DLOG(INFO) << "processed transactions size: " << totalSize;
        DLOG(INFO) << "current transactions size: " << epochSizeMap[epochNum];
        DLOG(INFO) << "current epoch: " << epochNum;
        DLOG(INFO) << "current duration: " << timer.end() << ", max: " << duration;
    }
}
