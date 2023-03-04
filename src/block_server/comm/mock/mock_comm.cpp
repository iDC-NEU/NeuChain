//
// Created by peng on 2021/6/18.
//

#include "block_server/comm/mock/mock_comm.h"
#include "common/payload/ycsb_payload.h"
#include "common/payload/mock_utils.h"
#include "block_server/transaction/transaction.h"
#include "transaction.pb.h"

std::unique_ptr<std::vector<Transaction *>>
MockComm::getTransaction(epoch_size_t epoch, uint32_t maxSize, uint32_t minSize) {
    // epoch start from 1
    if (firstInEpoch == 0) {
        firstInEpoch = epoch;
    }
    // make tx func
    auto makeTransaction = [] (epoch_size_t epoch, const Utils::Request& request) -> Transaction* {
        static uint64_t tid = 0; tid++;
        auto* transaction = Utils::makeTransaction(epoch, tid, false);
        *transaction->getTransactionPayload() = Utils::getTransactionPayload(tid, request);
        return transaction;
    };
    auto txList = std::make_unique<std::vector<Transaction *>>();
    if(epoch == firstInEpoch) {
        if(!firstInEpoch1)
            return nullptr;
        firstInEpoch1 = false;
        // -------------test case for epoch=0 starts here------------------
        // blurring the lines between bc & dbs: p112 test case
        // abort: 4, 5
        // commit: 1, 2, 3, 6
        Utils::Request rwRequest;
        rwRequest.funcName = "mixed";
        rwRequest.reads = {"0", "1"};
        rwRequest.writes = {"2"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {"3", "4", "5"};
        rwRequest.writes = {"0"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {"6", "7"};
        rwRequest.writes = {"3", "9"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {"2", "8"};
        rwRequest.writes = {"1", "4"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {"9"};
        rwRequest.writes = {"5", "6", "8"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {};
        rwRequest.writes = {"7"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        // -------------test case for epoch=1 ends here------------------
    }
    if(epoch == firstInEpoch + 1) {
        if(!firstInEpoch2)
            return nullptr;
        firstInEpoch2 = false;
        // -------------test case for epoch=1 starts here------------------
        // blurring the lines between bc & dbs: p112 test case
        // abort: 7, 8, 9
        // commit: 4, 5, 10
        Utils::Request rwRequest;
        rwRequest.funcName = "mixed";
        rwRequest.reads = {"0", "1"};
        rwRequest.writes = {"2"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {"3", "4", "5"};
        rwRequest.writes = {"0"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {"6", "7"};
        rwRequest.writes = {"3", "9"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        rwRequest.reads = {};
        rwRequest.writes = {"7"};
        txList->push_back(makeTransaction(epoch, rwRequest));
        // -------------test case for epoch=2 ends here------------------
    }
    if(epoch == firstInEpoch + 2) {
        while(!firstInEpoch3);
        firstInEpoch3 = false;
        txList->push_back(makeTransaction(epoch, Utils::Request{}));
    }
    for(const auto& tx: *txList) {
        txMap[tx->getTransactionID()] = tx;
    }
    return txList;
}
