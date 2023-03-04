//
// Created by peng on 2021/9/10.
//

#include "epoch_server/oe_consensus_manager.h"

#include <utility>
#include "common/aria_types.h"
#include "common/msp/crypto_helper.h"
#include "common/sha256_hepler.h"
#include "common/zmq/zmq_server.h"
#include "oe.pb.h"
#include "comm.pb.h"
#include <cassert>
#include  <queue>
#include "common/concurrent_queue/light_weight_semaphore.hpp"

template<class T>
class BlockGenerator {
public:
    std::queue<std::string> buffer;
    bool process_request(std::string &&request, const std::function<bool(uint32_t, std::string &&)> &failure_callback = nullptr) {
        // we have to check user signature in order phase as well as execution phase
        // because we form a block after ordering, and we have to make sure the block is valid
        // 1.1 deserialize user request
        char type = request.back();
        comm::UserRequest userRequest;
        if (type != 0x01) {
            request.pop_back();
            // unknown type, pass to handler
            return failure_callback(type, std::move(request));
        }
        // modify for eopv, send directly to all block server
        blockBroadcaster.sendReply(request);
        request.pop_back();
        if (!userRequest.ParseFromString(request)) {
            CHECK(false);
        }
        // check digest
        const CryptoSign *userSigner = CryptoHelper::getUserSigner(userRequest.ip());
        if (!userSigner->rsaDecrypt(userRequest.payload(), userRequest.digest())) {
            CHECK(false);
        }
        while (block.user_request_size() < 3000 && !buffer.empty()) {
            block.add_user_request(std::move(buffer.front()));
            buffer.pop();
        }
        if (block.user_request_size() < 3000) {     // we omit the overflow user request because it will cause the process rate swift
            block.add_user_request(std::move(request));
        } else {
            buffer.push(std::move(request));
        }
        return true;
    }

    void increaseEpoch() { ++current_epoch; }

private:
    friend T;

    explicit BlockGenerator(int block_broadcaster_port)
            : blockBroadcaster("0.0.0.0", std::to_string(block_broadcaster_port), zmq::socket_type::pub),
              current_epoch(1) {
        epochServerSigner = CryptoHelper::getEpochServerSigner();
        reset();
        recvThread = std::make_unique<std::thread>(&BlockGenerator::recv_barrier, this);
    }

    void reset() {
        timer.start();
        block.Clear();
    }

    void broadcast_block() {
        block.set_epoch(current_epoch);
        std::string signature;
        epochServerSigner->rsaEncrypt(block.SerializeAsString(), &signature);
        block.set_signature(signature);
        std::string rawBlock = block.SerializeAsString();
        rawBlock.push_back('\02');
        // WE CURRENTLY REMOVE THE SYNC FOR EACH BLOCK
        // the async process delay can be omit
        blockBroadcaster.sendReply(rawBlock);
        LOG(INFO) << "block " << current_epoch << " cost " << timer.end() << " seconds.";
        sema.wait();
    }

    void recv_barrier() {
        ZMQServer txReceiver("0.0.0.0", "9989", zmq::socket_type::sub);
        std::vector<int> epochFinishCount;
        epochFinishCount.resize(10000);
        for (int i=1;i < (int)epochFinishCount.size(); i++) {
            while(epochFinishCount[i] < 1) {    // 1 user is enough (in an az)
                std::string epoch;
                txReceiver.getRequest(epoch);
                // add state change hash to the next block
                DLOG(INFO) <<"epoch " << epoch << "recv exec complete signal from peer, increase epoch.";
                sema.signal();
                int actual_epoch = (int) std::stol(epoch);
                if (!actual_epoch)
                    continue;
                epochFinishCount[actual_epoch]++;
            }
        }
    }

private:
    eov::EOVBlock block;
    moodycamel::LightweightSemaphore sema;
    ZMQServer blockBroadcaster;
    epoch_size_t current_epoch;
    BlockBench::Timer timer;
    const CryptoSign *epochServerSigner;
    std::unique_ptr<std::thread> recvThread;
};

class BlockGeneratorWithBlockSize : private BlockGenerator<BlockGeneratorWithBlockSize> {
public:
    explicit BlockGeneratorWithBlockSize(int block_broadcaster_port, int block_size)
            : BlockGenerator(block_broadcaster_port), max_block_size(block_size) {}

    void push_request(std::string &&request) {
        // get a user request and push it into buffer
        CHECK(process_request(std::move(request)));
        if (block.user_request_size() > max_block_size) {
            // form a block and reset
            broadcast_block();
            reset();
            // increase epoch
            increaseEpoch();
        }
    }

private:
    const int max_block_size;
};

class BlockGeneratorWithTime : private BlockGenerator<BlockGeneratorWithTime> {
public:
    explicit BlockGeneratorWithTime(int block_broadcaster_port, double duration,
                                    std::function<void(epoch_size_t, std::string &&)> timeout)
            : BlockGenerator(block_broadcaster_port), timeout_signal_handle(std::move(timeout)), duration(duration) {}

    void push_request(std::string &&request) {
        // get a user request and push it into buffer
        const auto request_callback = [this](uint32_t type, std::string&& request) -> bool {
            CHECK(type == 0x02);
            // is a control message
            eov::TimeoutSignal signal;
            if (!signal.ParseFromString(request)) {
                // unknown message
                return false;
            }
            // form a block and reset
            if (signal.epoch() >= current_epoch) {
                LOG(INFO) << "actual increase epoch: " << signal.epoch();
                broadcast_block();
                reset();
                // increase epoch
                current_epoch = signal.epoch()+1;
                sent_signal = false;
            }
            return true;
        };
        CHECK(process_request(std::move(request), request_callback));
        // if timeout we send a signal only once
        if (timer.end() > duration && !sent_signal) {
            eov::TimeoutSignal signal;
            signal.set_epoch(current_epoch);
            std::string timeout_request = signal.SerializeAsString();
            timeout_request.push_back(0x02);
            timeout_signal_handle(current_epoch, std::move(timeout_request));
            sent_signal = true;
        }
    }

protected:
    // send a signal after epoch i is time out
    // for each epoch, only send once
    std::function<void(epoch_size_t, std::string &&)> timeout_signal_handle;

private:
    const double duration;
    bool sent_signal = false;
};

OEConsensusManager::OEConsensusManager(const consensus_param &param)
        : ConsensusManager(param.initial_conf, param.raft_ip, param.raft_port, param.batch_size) {
    block_broadcaster_port = param.block_broadcaster_port;
    request_receiver_port = param.request_receiver_port;
    block_size = param.block_size;
    duration = param.duration;
}

void OEConsensusManager::receiver_from_raft() {
    // double duration = static_cast<double>(self->block_size) / 1000;
    // LOG(INFO) << "set epoch duration to " << duration << "sec.";
    LOG(INFO) << "set block size to " << block_size << ".";
    // BlockGeneratorWithBlockSize generator(block_broadcaster_port, block_size);
    BlockGeneratorWithTime generator(block_broadcaster_port, duration,
                                     [this](epoch_size_t timeout_epoch, std::string &&message) {
                                         push_message_to_raft(std::move(message));
                                     });
    while (!brpc::IsAskedToQuit()) {
        std::string response;
        get_message_from_raft(response);
        generator.push_request(std::move(response));
    }
}

void OEConsensusManager::receiver_from_user() {
    ZMQServer txReceiver("0.0.0.0", std::to_string(request_receiver_port), zmq::socket_type::sub);
    while (!brpc::IsAskedToQuit()) {
        // we get a request first
        std::string requestRaw;
        txReceiver.getRequest(requestRaw);
        // append type
        requestRaw.push_back(0x01);
        // to simplify, we order the message first(because we use cc to execute transaction,
        // this will cause only about (process - block_timeout) additional delay in geo distribution,
        // no delay in the same az)
        // delay = max(raft+block_timeout, process)
        // ours = max(raft+block_timeout, raft+process)
        push_message_to_raft(std::move(requestRaw));
    }
}
