//
// Created by peng on 3/31/22.
//

#ifndef NEU_BLOCKCHAIN_EPOCH_TRANSACTION_BUFFER_H
#define NEU_BLOCKCHAIN_EPOCH_TRANSACTION_BUFFER_H

#include "block_server/transaction/transaction.h"
#include "common/aria_types.h"
#include "common/compile_config.h"
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/yaml_config.h"
#include "glog/logging.h"
#include <queue>
#include <functional>


class Transaction;
class EpochTxnQueueManager;


template<typename Handle>
class EpochTransactionBuffer {
public:
    // not thread safe, emptyBuffer is called only once in an epoch
    explicit EpochTransactionBuffer(const Handle& handle, EpochTxnQueueManager* queueMgr,
                                    bool eagerSent=YAMLConfig::getInstance()->enableEarlyExecution())
            : _handle{handle}, _eagerSent(eagerSent), _queueMgr(queueMgr) {
        buffer.resize(MAX_EPOCH);
        threads.emplace_back(&EpochTransactionBuffer::run, this);
        static_assert(std::is_invocable<Handle, Transaction*>::value, "invalid handle");
        if (_eagerSent) {
            pushTransactionToBuffer = [&](Transaction * transaction) {
                auto txEpoch = transaction->getEpoch();
                if (this->_queueMgr->isAllCPPushedToQueue(txEpoch)) {
                    // we need another sync to make sure current epoch is finished
                    syncEmptyBuffer(txEpoch);
                    _handle(transaction);
                    return;
                }
                // if we call emptyBuffer(), txEpoch must be finished.
                buffer[txEpoch].push(transaction);
            };
        } else {
            pushTransactionToBuffer = [&](Transaction * transaction) {
                auto txEpoch = transaction->getEpoch();
                buffer[txEpoch].push(transaction);
            };
        }
    }

    ~EpochTransactionBuffer() {
        workload.enqueue(nullptr);
    }

    Handle pushTransactionToBuffer;

    // after an epoch is finished, call this function
    // the worker in it will async empty the queue
    // finish epoch is local/remote finished epoch, not the global one
    void asyncEmptyBuffer(epoch_size_t finishedEpoch) {
        // the async function:
        workload.enqueue([targetEpoch = finishedEpoch, this]() {
            this->_queueMgr->waitForAllCPRecvTxn(targetEpoch);
            // TODO: eager send previously do not adapt recovery
            if (!_eagerSent) {
                DLOG(INFO) << "increase epoch, free buffer for epoch = " << targetEpoch << ", size=" << buffer[targetEpoch].size();
                syncEmptyBuffer(targetEpoch);
            }
            else {
                DLOG(INFO) << "increase epoch, free buffer for epoch = " << targetEpoch;
            }
            this->_queueMgr->signalFinishedPushToQueue(targetEpoch);
        });
    }

protected:
    void run() {
        pthread_setname_np(pthread_self(), "txn_buffer");
        std::function<void()> task;
        while(workload.wait_dequeue(task), task != nullptr) {
            task();
        }
    }

    // not thread safe, no additional check, no signal emit
    void syncEmptyBuffer(epoch_size_t finishedEpoch) {
        auto& queue = buffer[finishedEpoch];
        while (!queue.empty()) {
            auto* txn = queue.front();
            CHECK(txn->getEpoch() == finishedEpoch);
            _handle(txn);
            queue.pop();
        }
    }

private:
    const Handle& _handle;
    const bool _eagerSent;
    EpochTxnQueueManager* _queueMgr;
    std::vector<std::queue<Transaction*>> buffer;
    // one consumer, one producer
    moodycamel::BlockingConcurrentQueue<std::function<void()>> workload;
    std::vector<std::thread> threads;
};

#endif //NEU_BLOCKCHAIN_EPOCH_TRANSACTION_BUFFER_H
