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
        signHelper(CryptoHelper::getLocalBlockServerSigner()),
        workers(new ThreadPool(8)),
        finishSignal(false),
        blockGeneratorThread(&BlockGeneratorImpl::run, this),
        blockCache(10) {
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
    auto transactions = getPendingTransactionQueue();
    workloadsBuffer[currentBlockEpoch] = std::move(transactions);

    workers->execute([this, epoch = currentBlockEpoch]() {
        CHECK(workloadsBuffer[epoch] != nullptr);
        auto block = std::make_unique<Block::Block>();
        if(!generateBlockBody(block.get(), std::move(workloadsBuffer[epoch]))) {
            LOG(ERROR) << "generate block body failed!";
            CHECK(false);
        }
        blockBuffer[epoch] = std::move(block);
        workloadsBuffer.remove(epoch);
        blockBodyGeneratedSignal.notify_all();
    });
    return true;
}

void BlockGeneratorImpl::run() {
    int64_t currentBlockEpoch = storage->getLatestSavedEpoch();
    while(!finishSignal) {
        currentBlockEpoch++;
        blockBodyGeneratedSignal.wait([&]()->bool{
            return blockBuffer[currentBlockEpoch] != nullptr;
        });
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
        blockReadySignal.notify_all();
        // sign the block and broadcast the block async
        broadcaster->addBlockToVerify(std::move(block));
    }
    LOG(INFO) << "block generator quiting...";
}

bool BlockGeneratorImpl::generateBlockBody(Block::Block *block, std::unique_ptr<TxPriorityQueue> transactions) const {
    // add metadata of transaction
    auto* trMetadata = block->mutable_metadata()->add_metadata();
    // add metadata of merkle root (not necessary)
    auto* merkleRoot = block->mutable_metadata()->add_metadata();
    // add metadata of block second
    auto* signature = block->mutable_metadata()->add_metadata();
    Block::BlockData* blockData = block->mutable_data();
    while (!transactions->empty()) {
        Transaction* transaction = transactions->top();
        std::string* data = blockData->add_data();
        CHECK(transaction->serializeToString(data));
        // write metadata of transaction first
        trMetadata->push_back((char)transaction->getTransactionResult());
        transactions->pop();
    }
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
        if (!(//sha256Helper.hash(block->data().SerializeAsString()) &&
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
    if(!(sha256Helper.hash(block->data().SerializeAsString())&&
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
    // TODO: CHECK BLOCK CONSISTENCY
    if (true) {
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
