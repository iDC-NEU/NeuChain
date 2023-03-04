//
// Created by peng on 2021/6/18.
//

#ifndef NEUBLOCKCHAIN_MOCK_COMM_H
#define NEUBLOCKCHAIN_MOCK_COMM_H

#include "block_server/comm/comm.h"
#include <map>

class MockComm: public Comm {
public:
    std::unique_ptr<std::vector<Transaction*>> getTransaction(epoch_size_t epoch, uint32_t maxSize, uint32_t minSize) override;
    void clearTransactionForEpoch(epoch_size_t epoch) override { }

    std::map<tid_size_t, Transaction*> txMap;
private:
    bool firstInEpoch1 = true;
    bool firstInEpoch2 = true;
    bool firstInEpoch3 = true;
    // calculate offset
    epoch_size_t firstInEpoch{0};
};


#endif //NEUBLOCKCHAIN_MOCK_COMM_H
