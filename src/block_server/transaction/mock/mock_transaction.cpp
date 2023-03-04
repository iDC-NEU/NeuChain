//
// Created by peng on 2021/1/18.
//

#include "block_server/transaction/mock/mock_transaction.h"
#include "common/sha256_hepler.h"
#include "transaction.pb.h"
#include "kv_rwset.pb.h"

#include <google/protobuf/io/coded_stream.h>
#include "glog/logging.h"

MockTransaction::MockTransaction() :readOnlyFlag(true), epoch(0), tid(0){
    kvRWSet = new KVRWSet();
    transactionResult = TransactionResult::PENDING;
    transactionPayload = new TransactionPayload();
}

MockTransaction::~MockTransaction() {
    delete kvRWSet;
    delete transactionPayload;
}

void MockTransaction::resetTransaction() {
    epoch = 0;
    kvRWSet->Clear();
    transactionResult = TransactionResult::PENDING;
}

bool MockTransaction::readOnly() {
    return readOnlyFlag;
}

void MockTransaction::readOnly(bool flag) {
    readOnlyFlag = flag;
}

void MockTransaction::setEpoch(epoch_size_t _epoch) {
    this->epoch = _epoch;
}

epoch_size_t MockTransaction::getEpoch() {
    return epoch;
}

tid_size_t MockTransaction::getTransactionID() {
    return tid;
}

void MockTransaction::setTransactionID(tid_size_t _tid) {
    tid = _tid;
}

void MockTransaction::resetRWSet() {
    this->kvRWSet->Clear();
}

KVRWSet* MockTransaction::getRWSet() {
    return this->kvRWSet;
}

TransactionResult MockTransaction::getTransactionResult() {
    return transactionResult;
}

void MockTransaction::setTransactionResult(TransactionResult _transactionResult) {
    this->transactionResult = _transactionResult;
}

TransactionPayload* MockTransaction::getTransactionPayload() {
    return transactionPayload;
}

void MockTransaction::resetTransactionPayload() {
    transactionPayload->Clear();
}

bool MockTransaction::serializeToString(std::string *transactionRaw) {
    // transactionRaw is empty
    transactionRaw->clear();
    google::protobuf::io::StringOutputStream stringOutputStream(transactionRaw);
    auto* codedOutputStream = new google::protobuf::io::CodedOutputStream(&stringOutputStream);
    // tid: int64
    codedOutputStream->WriteVarint64(static_cast<uint64_t>(tid));
    // epoch int64
    codedOutputStream->WriteVarint64(static_cast<uint64_t>(epoch));
    // read-only flag bool
    codedOutputStream->WriteVarint32(static_cast<uint32_t>(readOnlyFlag));
    // payload int64 + string
    std::string payload = transactionPayload->SerializeAsString();
    codedOutputStream->WriteVarint64(payload.size());
    codedOutputStream->WriteString(payload);
    // rw-set int64 + string
    std::string rwSet = kvRWSet->SerializeAsString();
    codedOutputStream->WriteVarint64(rwSet.size());
    codedOutputStream->WriteString(rwSet);
    // result uint32
    codedOutputStream->WriteVarint32(static_cast<uint32_t>(transactionResult));
    // Destroy the CodedOutputStream and position the underlying
    // ZeroCopyOutputStream immediately after the last byte written.
    delete codedOutputStream;

    SHA256Helper hashHelper;
    std::string hashResult;
    if(!hashHelper.hash(*transactionRaw, &hashResult)) {
        return false;
    }
    transactionRaw->append(hashResult);
    return true;
}

bool MockTransaction::deserializeFromString(const std::string &transactionRaw) {
    // data start at tr head
    google::protobuf::io::ArrayInputStream arrayInputStream(transactionRaw.data(), transactionRaw.size());
    google::protobuf::io::CodedInputStream codedInputStream(&arrayInputStream);
    // read tid
    {
        uint64_t _tid;
        if (!codedInputStream.ReadVarint64(&_tid)) {
            return false;
        }
        tid = static_cast<tid_size_t>(_tid);
    }
    // read epoch
    {
        uint64_t _epoch;
        if (!codedInputStream.ReadVarint64(&_epoch)) {
            return false;
        }
        epoch = static_cast<epoch_size_t>(_epoch);
    }
    // read readonly flag
    {
        uint32_t flag;
        if (!codedInputStream.ReadVarint32(&flag)) {
            return false;
        }
        readOnlyFlag = static_cast<bool>(flag);
    }
    // read size + payload
    {
        uint64_t payloadSize;
        if (!codedInputStream.ReadVarint64(&payloadSize)) {
            return false;
        }
        std::string payloadString;
        if (!codedInputStream.ReadString(&payloadString, payloadSize)) {
            return false;
        }
        CHECK(transactionPayload->ParseFromString(payloadString));
    }
    // read size + rwset
    {
        uint64_t rwSetSize;
        if (!codedInputStream.ReadVarint64(&rwSetSize)) {
            return false;
        }
        std::string rwSetString;
        if (!codedInputStream.ReadString(&rwSetString, rwSetSize)) {
            return false;
        }
        CHECK(kvRWSet->ParseFromString(rwSetString));
    }
    // result uint32
    {
        uint32_t result;
        if (!codedInputStream.ReadVarint32(&result)) {
            return false;
        }
        transactionResult = static_cast<TransactionResult>(result);
    }
    // phase hash
    {
        std::string transactionHash;
        if (!codedInputStream.ReadString(&transactionHash, SHA256HashSize)) {
            return false;
        }
    }
    return true;
}
