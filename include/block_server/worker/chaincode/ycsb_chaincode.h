//
// Created by peng on 2021/5/6.
//

#ifndef NEUBLOCKCHAIN_YCSB_CHAINCODE_H
#define NEUBLOCKCHAIN_YCSB_CHAINCODE_H


#include "block_server/worker/impl/chaincode_object.h"
#include <functional>

class YCSB_Chaincode: public ChaincodeObject {
public:
    explicit YCSB_Chaincode(Transaction *transaction);
    int InvokeChaincode(const std::string& chaincodeName, const std::vector<std::string>& args) override;

protected:
    int InitFunc(const std::vector<std::string>& args) override;

    int write(const std::string &tableName, const std::string& key, const std::string& val);
    int del(const std::string &tableName, const std::string& key);
    int read(const std::string &tableName, const std::string& key, const std::string &filter);

private:
    std::function<std::string(const std::string&)> queryLambda;
    std::function<void(const std::string&, const std::string&)> updateLambda;

    std::string defValue = "default_value";
};



#endif //NEUBLOCKCHAIN_YCSB_CHAINCODE_H
