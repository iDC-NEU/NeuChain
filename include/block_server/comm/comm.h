//
// Created by peng on 2021/1/22.
//

#ifndef NEUBLOCKCHAIN_COMM_H
#define NEUBLOCKCHAIN_COMM_H

#include <vector>
#include <memory>
#include "common/aria_types.h"

class Transaction;

class Comm {
public:
    virtual ~Comm() = default;
    // get a set of transactions form comm, not thread safe
    virtual std::unique_ptr<std::vector<Transaction*>> getTransaction(epoch_size_t epoch, uint32_t maxSize, uint32_t minSize) = 0;
    // clear transaction of specific epoch number, not thread safe
    virtual void clearTransactionForEpoch(epoch_size_t epoch) = 0;
};


#endif //NEUBLOCKCHAIN_COMM_H
