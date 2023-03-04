//
// Created by peng on 2021/1/22.
//

#ifndef NEUBLOCKCHAIN_COMMSTUB_H
#define NEUBLOCKCHAIN_COMMSTUB_H

#include "block_server/comm/comm.h"
#include <map>

class CommStub: public Comm{
public:
    explicit CommStub(epoch_size_t startWithEpoch);
    std::unique_ptr<std::vector<Transaction*>> getTransaction(epoch_size_t epoch, uint32_t maxSize, uint32_t minSize) override;
    void clearTransactionForEpoch(epoch_size_t epoch) override;

private:
    epoch_size_t epoch;
    // for restore test, this need to be changed as well.
    // because the impl of this dont need epoch, the creation of trs, which is the usage of epoch, belongs to client proxy
    size_t epochTrSentNum = 0;
    std::map<epoch_size_t ,std::vector<Transaction*>> transactionMap;

private:
    static const size_t EPOCH_MAX_TR_NUM = 19000;
};

#endif //NEUBLOCKCHAIN_COMMSTUB_H
