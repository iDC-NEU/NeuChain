//
// Created by peng on 4/25/21.
//

#ifndef NEUBLOCKCHAIN_MOCK_UTILS_H
#define NEUBLOCKCHAIN_MOCK_UTILS_H

#include <string>
#include <memory>
#include "common/aria_types.h"

class Transaction;

namespace Utils {
    // for chaincode test only, generate a transaction without payload
    Transaction* makeTransaction(epoch_size_t epoch, tid_size_t tid, bool readOnly=false);
    Transaction* makeTransaction(const std::string& transactionRaw);
    Transaction* makeTransaction();
}


#endif //NEUBLOCKCHAIN_MOCK_UTILS_H
