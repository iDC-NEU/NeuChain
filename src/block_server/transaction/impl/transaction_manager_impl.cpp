//
// Created by peng on 2021/1/22.
//

#include "block_server/transaction/impl/transaction_manager_impl.h"
#include "block_server/comm/comm.h"
#include "block_server/database/block_generator.h"
#include "block_server/transaction/transaction.h"
#include "common/timer.h"
#include "common/yaml_config.h"
#include "glog/logging.h"

TransactionManagerImpl::~TransactionManagerImpl() = default;

void TransactionManagerImpl::setBlockGenerator(std::unique_ptr<BlockGenerator> generator) {
    this->blockGenerator = std::move(generator);
    cleanedEpoch = blockGenerator->getLatestSavedEpoch();
    processingEpoch = cleanedEpoch + 1;
    LOG(INFO) << "TransactionManagerImpl start with epoch = " << processingEpoch;
}

void TransactionManagerImpl::run() {
    pthread_setname_np(pthread_self(), "tx_mgr");
    // to process with epoch=i, we first must get the txs.
    epoch_size_t currentEpoch = processingEpoch;
    std::unique_ptr<std::vector<Transaction*>> trWrapper;
    // we REMOVE the ConsumeTimeCalculator here since the process does not cost much
    while(!finishSignal) {
        auto& currentEpochTxQueue = epochTxBuffer[currentEpoch];
        // get all epoch = i transaction
        while(trWrapper = commModule->getTransaction(currentEpoch, TR_MGR_GET_BATCH_MAX_SIZE, 1), trWrapper!= nullptr) {
            // merge it into currentEpochTrWrapper
            for(auto* tx: *trWrapper) {
                currentEpochTxQueue.enqueue(tx);
            }
            sendTransactionHandle(std::move(trWrapper));
        }
        currentEpoch++;
        // for reusing mode, we must wait after we transferred curr=i+1, until proc=i is finished
        if (reusingAbortTransaction) {
            epochFinishedSignal.wait([&]()->bool {
                return processingEpoch + 1 >= currentEpoch;
            });
            if (reuseTxWrapper) {
                // if reuse transaction, send the aborted transaction
                sendTransactionHandle(std::move(reuseTxWrapper));
            }
        }
    }
    LOG(INFO) << "TransactionManagerImpl exiting...";
}

void TransactionManagerImpl::epochFinishedCallback(epoch_size_t finishedEpoch) {
    // finish processed epoch=i, processingEpoch=i+1;
    (categorizeTransaction || reusingAbortTransaction) ? categorizeEpoch(finishedEpoch) : nonCategorizeEpoch(finishedEpoch);
    // generate next block
    if (!blockGenerator->generateBlock()) {
        LOG(INFO) << "Generate Block Failed! ";
        CHECK(false);
    }
    // send feed back
    LOG(INFO) << "Finish with epoch id=" << finishedEpoch;
    while(!YAMLConfig::getInstance()->enableAsyncBlockGen() &&
          finishedEpoch != blockGenerator->getLatestSavedEpoch()) {
        BlockBench::Timer::sleep(0.001);
    }
    // move on to next epoch
    while (cleanedEpoch < blockGenerator->getLatestSavedEpoch())
        this->commModule->clearTransactionForEpoch(++cleanedEpoch);
    // clear tr wrapper
    epochTxBuffer.remove(finishedEpoch);
    // notice the consumer at last.
    processingEpoch = finishedEpoch + 1;
    epochFinishedSignal.notify_one();
}

void TransactionManagerImpl::categorizeEpoch(epoch_size_t finishedEpoch) {
    DLOG(INFO) << "finishEpoch: " << finishedEpoch;
    auto& currentEpochTxQueue = epochTxBuffer[finishedEpoch];
    auto& nextEpochTxQueue = epochTxBuffer[finishedEpoch + 1];
    size_t commit = 0, abort = 0;
    auto trWrapper = std::make_unique<std::vector<Transaction*>>();
    Transaction* tr = nullptr;
    while (currentEpochTxQueue.try_dequeue(tr)) {
        DCHECK(finishedEpoch == tr->getEpoch());
        // add last epoch transaction to current epoch
        if(tr->getTransactionResult() == TransactionResult::ABORT) {
            // DLOG(INFO) << "ABORT: " << tr->getTransactionID();
            if(!reusingAbortTransaction) {
                blockGenerator->addCommittedTransaction(tr);
            } else {
                // reset the transaction
                tr->resetTransaction();   //tr->getEpoch() == 0 after reset
                tr->setEpoch(finishedEpoch + 1);
                DCHECK(tr->getEpoch() == finishedEpoch + 1);
                trWrapper->push_back(tr);
                // add the transaction to monitor list
                nextEpochTxQueue.enqueue(tr);
            }
            abort++;
            continue;
        }
        if(tr->getTransactionResult() == TransactionResult::COMMIT) {
            // DLOG(INFO) << "COMMIT: " << tr->getTransactionID();
            blockGenerator->addCommittedTransaction(tr);    // add to blk gen
            commit++;
            continue;
        }
        if(tr->getTransactionResult() == TransactionResult::ABORT_NO_RETRY) {
            // DLOG(INFO) << "ABORT_NO_RETRY: " << tr->getTransactionID();
            // aborted tr also has been execute successfully
            blockGenerator->addCommittedTransaction(tr);
            abort++;
            continue;
        }
    }
    // send the rest tr wrapper
    reuseTxWrapper = std::move(trWrapper);
    // summary
    LOG(INFO) << "Total, abort: " << abort << " commit: "<<commit;
}

// faster, non categorize, non calculate aborted number
void TransactionManagerImpl::nonCategorizeEpoch(epoch_size_t finishedEpoch) {
    auto& currentEpochTxQueue = epochTxBuffer[finishedEpoch];
    DLOG(INFO) << "finishEpoch: " << finishedEpoch;
    Transaction* tr = nullptr;
    while(currentEpochTxQueue.try_dequeue(tr)) {
        DCHECK(finishedEpoch == tr->getEpoch());
        blockGenerator->addCommittedTransaction(tr);
    }
}
