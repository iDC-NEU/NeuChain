//
// Created by peng on 5/10/21.
//

#ifndef NEUBLOCKCHAIN_BLOCK_BROADCASTER_H
#define NEUBLOCKCHAIN_BLOCK_BROADCASTER_H

#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/aria_types.h"
#include <atomic>
#include <functional>
#include <mutex>

class ZMQClient;
class ServerListener;

namespace Block {
    class Block;
}

class BlockBroadcaster {
public:
    BlockBroadcaster();
    virtual ~BlockBroadcaster();

    // sequence is needed, thread safe.
    inline void addBlockToVerify(std::unique_ptr<Block::Block> block) { verifyBuffer.enqueue(std::move(block)); }

    std::function<bool(epoch_size_t, const std::string&, const std::string&)> signatureVerifyCallback;

protected:
    void getSignatureFromOtherServer(const std::string& ip, ZMQClient *client);
    void blockVerifier();

private:
    std::atomic<bool> finishSignal;
    std::unique_ptr<ServerListener> listener;
    std::unique_ptr<std::thread> blockVerifierThread;
    std::vector<bool> validatedBlockNumber;
    std::mutex validatedBlockNumberMutex;

    moodycamel::BlockingConcurrentQueue<std::unique_ptr<Block::Block>> verifyBuffer;
};

#endif //NEUBLOCKCHAIN_BLOCK_BROADCASTER_H
