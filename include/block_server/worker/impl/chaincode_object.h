//
// Created by peng on 2021/3/7.
//

#ifndef NEUBLOCKCHAIN_CHAINCODE_OBJECT_H
#define NEUBLOCKCHAIN_CHAINCODE_OBJECT_H

#include "block_server/worker/chaincode_stub_interface.h"
#include <vector>
#include <string>
#include <memory>
#include "common/aria_types.h"

class Transaction;
namespace AriaORM {
    class CCORMHelper;
}

class ChaincodeObject: virtual public ChaincodeStubInterface {
public:
    explicit ChaincodeObject(Transaction* transaction);
    ~ChaincodeObject() override;

    int InvokeChaincode(const TransactionPayload& payload) override;

protected:
    virtual int InitFunc(const std::vector<std::string>& args) = 0;
    virtual int InvokeChaincode(const std::string& chaincodeName, const std::vector<std::string>& args) = 0;

protected:
    std::unique_ptr<AriaORM::CCORMHelper> helper;
    tid_size_t transactionID;
};


#endif //NEUBLOCKCHAIN_CHAINCODE_OBJECT_H
