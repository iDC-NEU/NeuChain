//
// Created by peng on 2021/9/10.
//

#include "epoch_server/eov_consensus_manager.h"

#include <utility>
#include "common/aria_types.h"
#include "common/msp/crypto_helper.h"
#include "common/sha256_hepler.h"
#include "common/zmq/zmq_server.h"
#include "eov.pb.h"
#include "comm.pb.h"

template<class T>
class BlockGenerator {
public:
    bool process_request(std::string &&request, const std::function<bool(uint32_t, std::string &&)> &failure_callback = nullptr) {
        // request = transaction raw + type
        char type = request.back();
        request.pop_back();
        if (type!=0x01) {
            // unknown type, pass to handler
            return failure_callback(type, std::move(request));
        }
        block.add_transaction(std::move(request));
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
        blockBroadcaster.sendReply(block.SerializeAsString());
        DLOG(INFO) << "block " << current_epoch << " cost " << timer.end() << " seconds.";
    }

private:
    eov::EOVBlock block;
    ZMQServer blockBroadcaster;
    epoch_size_t current_epoch;
    BlockBench::Timer timer;
    const CryptoSign *epochServerSigner;
};

class BlockGeneratorWithBlockSize : private BlockGenerator<BlockGeneratorWithBlockSize> {
public:
    explicit BlockGeneratorWithBlockSize(int block_broadcaster_port, int block_size)
            : BlockGenerator(block_broadcaster_port), max_block_size(block_size) {}

    void push_request(std::string &&request) {
        // get a user request and push it into buffer
        CHECK(process_request(std::move(request)));
        if (block.transaction_size() == max_block_size) {
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
            DLOG(INFO) << "increase epoch: " << signal.epoch();
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

EOVConsensusManager::EOVConsensusManager(const consensus_param &param)
        : ConsensusManager(param.initial_conf, param.raft_ip, param.raft_port, param.batch_size) {
    block_broadcaster_port = param.block_broadcaster_port;
    request_receiver_port = param.request_receiver_port;
    block_size = param.block_size;
    duration = param.duration;
}

void EOVConsensusManager::receiver_from_raft() {
    // double duration = static_cast<double>(self->block_size) / 1000;
    // LOG(INFO) << "set epoch duration to " << duration << "sec.";
    LOG(INFO) << "set epoch size to " << block_size << ".";
    // note: in geo distribution, we use block size as cutter,
    // because the epoch increment duration is too long.
    // in local test, we use BlockGeneratorWithTime,
    // because the latency and throughput is not fair when send rate increase
    BlockGeneratorWithBlockSize generator(block_broadcaster_port, block_size);
//    BlockGeneratorWithTime generator(block_broadcaster_port, duration,
//                                     [this](epoch_size_t timeout_epoch, std::string &&message) {
//                                         push_message_to_raft(std::move(message));
//                                     });
    while (!brpc::IsAskedToQuit()) {
        std::string response;
        get_message_from_raft(response);
        generator.push_request(std::move(response));
    }
}

void EOVConsensusManager::receiver_from_user() {
    ZMQServer txReceiver("0.0.0.0", std::to_string(request_receiver_port), zmq::socket_type::sub);
    while (!brpc::IsAskedToQuit()) {
        // we get a request first
        std::string requestRaw;
        txReceiver.getRequest(requestRaw);
        // append type
        requestRaw.push_back(0x01);
        push_message_to_raft(std::move(requestRaw));
    }
}