//
// Created by peng on 2/17/21.
//

#include "block_server/database/impl/block_generator_impl.h"
#include "block_server/database/block_storage.h"
#include "block_server/database/block_broadcaster.h"
#include "block_server/transaction/transaction.h"
#include "common/msp/crypto_helper.h"
#include "common/merkle_tree/merkle_tree.h"
#include "common/sha256_hepler.h"
#include "common/base64.h"
#include "common/thread_pool.h"
#include "block.pb.h"
#include "glog/logging.h"

BlockGeneratorImpl::BlockGeneratorImpl():
        broadcaster(std::make_unique<BlockBroadcaster>()),
        workers(new ThreadPool((int)sysconf(_SC_NPROCESSORS_ONLN) / 2, "blk_gen_worker")),
        signHelper(CryptoHelper::getLocalBlockServerSigner()),
        finishSignal(false),
        blockGeneratorThread(&BlockGeneratorImpl::run, this),
        blockCache(100),
        partialEpoch(0) {
    broadcaster->signatureVerifyCallback = [this] (epoch_size_t epoch, const std::string& ip, const std::string& signature) ->bool {
        // wait until block is ready
        blockReadySignal.wait([&]()->bool{
            return storage->getLatestSavedEpoch() >= epoch;
        });
        Block::Block pBlock;
        static std::mutex blockMutex;
        if (!blockCache.tryGet(epoch, pBlock)) {
            std::unique_lock <std::mutex> lck(blockMutex);
            // prevent reentry
            if (!blockCache.tryGet(epoch, pBlock)) {
                CHECK(deserializeBlock(epoch, &pBlock, false, false));
                blockCache.insert(epoch, pBlock);
            }
        }
        auto remoteSignHelper = CryptoHelper::getBlockServerSigner(ip);
        return signatureValidate(&pBlock, remoteSignHelper, signature);
    };
}

BlockGeneratorImpl::~BlockGeneratorImpl() {
    finishSignal = true;
    blockGeneratorThread.join();
}

bool BlockGeneratorImpl::generateBlock() {
    static epoch_size_t currentBlockEpoch = storage->getLatestSavedEpoch();
    currentBlockEpoch++;
    auto* transactions = getPendingTransactionQueue().release();
    workers->execute([this, epoch = currentBlockEpoch, txPtr = transactions]() {
        consumeTimeCalculator.startCalculate(epoch);
        CHECK(txPtr != nullptr);
        auto block = std::make_unique<Block::Block>();
        if(!generateBlockBody(epoch, block.get(), std::unique_ptr<TxPriorityQueue>(txPtr))) {
            LOG(ERROR) << "generate block body failed!";
            CHECK(false);
        }
        blockBuffer[epoch] = std::move(block);
        consumeTimeCalculator.endCalculate(epoch);
        blockBodyGeneratedSignal.notify_one();
    });
    return true;
}

void BlockGeneratorImpl::run() {
    pthread_setname_np(pthread_self(), "blk_header_gen");
    int64_t currentBlockEpoch = storage->getLatestSavedEpoch();
    while(!finishSignal) {
        currentBlockEpoch++;
        blockBodyGeneratedSignal.wait([&]()->bool{
            return blockBuffer[currentBlockEpoch] != nullptr;
        });
        consumeTimeCalculator.startCalculate(currentBlockEpoch);
        auto block = std::move(blockBuffer[currentBlockEpoch]);
        DLOG(INFO) << "generate block:" << currentBlockEpoch;
        CHECK(block != nullptr);

        if(!generateBlockHeader(block.get())) {
            LOG(ERROR) << "generate block header failed!";
            CHECK(false);
        }

        if(!storage->appendBlock(*block)) {
            LOG(ERROR) << "save current block failed!";
            CHECK(false);
        }
        // after we append a block, epoch+=1, we can emit a signal that block is ready.
        consumeTimeCalculator.endCalculate(currentBlockEpoch);
        // sign the block and broadcast the block async
        broadcaster->addBlockToVerify(std::move(block));
        consumeTimeCalculator.getAverageResult(currentBlockEpoch, "block generator");
        blockReadySignal.notify_all();
    }
    LOG(INFO) << "block generator quiting...";
}

bool BlockGeneratorImpl::generateBlockBody(epoch_size_t epoch, Block::Block *block, std::unique_ptr<TxPriorityQueue> txnQueue) {
    {
        // add metadata of transaction
        auto txnQueueForEarlyResponse = *txnQueue;
        auto partialBlock = std::make_unique<Block::Block>();
        Block::BlockData* partialBlockData = partialBlock->mutable_data();
        while (!txnQueueForEarlyResponse.empty()) {
            auto* txn = txnQueueForEarlyResponse.top();
            auto* data = partialBlockData->add_data();
            CHECK(txn->serializeResultToString(data));
            txnQueueForEarlyResponse.pop();
        }

        partialBlock->mutable_header()->set_number(epoch);
        partialBlockBuffer[epoch] = std::move(partialBlock);
        if(epoch > 100 && partialBlockBuffer[epoch - 100] != nullptr) {
            partialBlockBuffer.remove(epoch - 100);
        }
        partialEpoch.store(epoch);
        DLOG(INFO) << "partialBlock generated block: " << epoch;
    }
    // for actual block
    auto* meta = block->mutable_metadata()->add_metadata();
    Block::BlockData* blockData = block->mutable_data();
    while (!txnQueue->empty()) {
        Transaction* transaction = txnQueue->top();
        std::string* data = blockData->add_data();
        CHECK(transaction->serializeToString(data));
        // write metadata of transaction first
        meta->push_back((char)transaction->getTransactionResult());
        txnQueue->pop();
    }
    // add metadata of merkle root (not necessary)
    auto* merkleRoot = block->mutable_metadata()->add_metadata();
    // add metadata of block second
    auto* signature = block->mutable_metadata()->add_metadata();
    // write metadata of block
    // metadata[0]: transaction result(abort or success)
    // metadata[1]: merkle root
    // metadata[2]: block signature
    std::string digest;
    mt_t *mt = mt_create();
    SHA256Helper sha256Helper;
    for(auto& data: blockData->data()) {
        sha256Helper.hash(data, &digest);
        CHECK(mt_add(mt, reinterpret_cast<const uint8_t *>(digest.data()), HASH_LENGTH) == MT_SUCCESS);
    }
    merkleRoot->resize(HASH_LENGTH);
    CHECK(mt_get_root(mt, reinterpret_cast<uint8_t *>(merkleRoot->data())) == MT_SUCCESS);
    DLOG(INFO) << "generate merkle root hash:" << base64_encode(*merkleRoot);
    mt_delete(mt);
    CHECK(signHelper->rsaEncrypt(*merkleRoot, signature));
    return true;
}

bool BlockGeneratorImpl::deserializeBlock(epoch_size_t epoch, Block::Block *block, bool verify, bool validate) const {
    if(!loadBlock(epoch, *block)) {
        return false;
    }
    if (verify) {
        SHA256Helper sha256Helper;
        std::string hashResult;
        if (!(sha256Helper.hash(block->data().SerializeAsString()) &&
              sha256Helper.append(block->metadata().metadata(0)) &&   // result
              sha256Helper.append(block->metadata().metadata(1)) &&   // merkle root
              sha256Helper.execute(&hashResult))) {
            return false;
        }
        if (hashResult != block->header().data_hash()) {
            return false;
        }
        if (block->metadata().metadata_size() < 3) {
            return false;
        }
    }
    if (validate) {
        auto& signature = block->metadata().metadata(2);
        return signatureValidate(block, signHelper, signature);
    }
    return true;
}

bool BlockGeneratorImpl::generateBlockHeader(Block::Block *block) const {
    SHA256Helper sha256Helper;
    std::string hashResult;
    if(!(//sha256Helper.hash(block->data().SerializeAsString())&&   // fixed, the merkle root represent the block body
         sha256Helper.append(block->metadata().metadata(0))&&   // result
         sha256Helper.append(block->metadata().metadata(1))&&   // merkle root
         sha256Helper.execute(&hashResult))){
        return false;
    }
    Block::BlockHeader* header = block->mutable_header();
    int64_t last_epoch = storage->getLatestSavedEpoch();
    header->set_data_hash(hashResult);
    header->set_number(last_epoch + 1);
    // cache block hash, for speed up.
    static std::string previousDataHash;
    if(last_epoch > 0) {
        Block::Block previous;
        CHECK(deserializeBlock(last_epoch, &previous, false, false));
        CHECK(previous.header().data_hash() == previousDataHash);
        header->set_previous_hash(previousDataHash);
    }
    previousDataHash = hashResult;
    DLOG(INFO) << "generate merkle root for block " << block->header().number() << " is: " << base64_encode(block->metadata().metadata(1))
               << ", block signature: " << base64_encode(block->metadata().metadata(2));
    return true;
}

#include "common/payload/mock_utils.h"
#include "kv_rwset.pb.h"
#include <set>
/*
 * Debug only, trace block difference.
 */
void signatureValidateDebugTrace(const Block::Block *block, const CryptoSign *sign, const std::string &signature) {
    SHA256Helper sha256Helper;
    std::string digest;
    mt_t *mt = mt_create();
    std::string merkleRoot;
    merkleRoot.resize(HASH_LENGTH);
    std::set<tid_size_t> tidSet;
    for(auto& data: block->data().data()) {
        sha256Helper.hash(data, &digest);
        LOG(INFO) << "generate merkle tree node hash:" << base64_encode(digest);
        CHECK(mt_add(mt, reinterpret_cast<const uint8_t *>(digest.data()), HASH_LENGTH) == MT_SUCCESS);
        CHECK(mt_get_root(mt, reinterpret_cast<uint8_t *>(merkleRoot.data())) == MT_SUCCESS);
        LOG(INFO) << "-------- merkle root hash:" << base64_encode(merkleRoot);
        Transaction* transaction = Utils::makeTransaction();
        transaction->deserializeFromString(data);
        LOG(INFO) << "epoch:" << transaction->getEpoch() << ", tid: " << transaction->getTransactionID() << ", result:" << static_cast<int>(transaction->getTransactionResult());
        for(const auto& read: transaction->getRWSet()->reads()) {
            LOG(INFO) << "read key:" << read.key() << ", read value:" << base64_encode(read.value()) << ", read table:" << read.table();
        }
        for(const auto& write: transaction->getRWSet()->writes()) {
            LOG(INFO) << "write key:" << write.key() << ", write value:" << write.value()<< "write table:" << write.table();
        }
        if (tidSet.count(transaction->getTransactionID())) {
            CHECK(false);
        }
        tidSet.insert(transaction->getTransactionID());
    }
    CHECK(mt_get_root(mt, reinterpret_cast<uint8_t *>(merkleRoot.data())) == MT_SUCCESS);
    mt_delete(mt);
    CHECK(merkleRoot == block->metadata().metadata(1));
    if(!sign->rsaDecrypt(merkleRoot, signature)) {
        CHECK(false);
    }
    LOG(INFO) << "block verify successfully finished.";
}

bool BlockGeneratorImpl::signatureValidate(const Block::Block *block, const CryptoSign *sign, const std::string &signature, bool skipVerify) {
    SHA256Helper sha256Helper;
    std::string digest;
    std::string root;
    if (skipVerify) {
        root = block->metadata().metadata(1);
    } else {
        mt_t *mt = mt_create();
        for(auto& data: block->data().data()) {
            sha256Helper.hash(data, &digest);
            CHECK(mt_add(mt, reinterpret_cast<const uint8_t *>(digest.data()), HASH_LENGTH) == MT_SUCCESS);
        }
        root.resize(HASH_LENGTH);
        CHECK(mt_get_root(mt, reinterpret_cast<uint8_t *>(root.data())) == MT_SUCCESS);
        mt_delete(mt);
        CHECK(root == block->metadata().metadata(1));
    }
    if(!sign->rsaDecrypt(root, signature)) {
        LOG(INFO) << "block inconsistent, please ensure you have clean the database.";
        LOG(INFO) << "check merkle root for block " << block->header().number() << " is: " << base64_encode(root)
                  << ", block signature: " << base64_encode(signature);
        signatureValidateDebugTrace(block, sign, signature);
        CHECK(false);
        return false;
    }
    DLOG(INFO) << "block verify successfully finished.";
    return true;
}

epoch_size_t BlockGeneratorImpl::getPartialConstructedLatestEpoch() {
    return partialEpoch.load();
}

bool BlockGeneratorImpl::getPartialConstructedBlock(epoch_size_t blockNum, std::string &serializedBlock) {
    if(partialBlockBuffer[blockNum] != nullptr) {
        partialBlockBuffer[blockNum]->SerializeToString(&serializedBlock);
        return true;
    }
    return false;
}
