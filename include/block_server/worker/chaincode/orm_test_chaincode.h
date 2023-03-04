//
// Created by peng on 3/2/21.
//

#ifndef NEUBLOCKCHAIN_ORM_TEST_CHAINCODE_H
#define NEUBLOCKCHAIN_ORM_TEST_CHAINCODE_H

#include "block_server/worker/impl/chaincode_object.h"
#include "common/random.h"

class ORMTestChaincode: public ChaincodeObject {
public:
    explicit ORMTestChaincode(Transaction *transaction) : ChaincodeObject(transaction), random(transactionID) {}
    int InvokeChaincode(const std::string& chaincodeName, const std::vector<std::string>& args) override;

protected:
    int InitFunc(const std::vector<std::string>& args) override;

    int correctTest(const std::string& key);
    int mixed(const std::string& raw);

    int timeConsume(const std::string& delay);
    int calculateConsume(const std::string& delay);

protected:
    int createData(const std::vector<std::string>& args);

private:
    Random random;
};


#endif //NEUBLOCKCHAIN_ORM_TEST_CHAINCODE_H
