//
// Created by peng on 2021/4/29.
//

#ifndef NEUBLOCKCHAIN_SMALL_BANK_CHAINCODE_H
#define NEUBLOCKCHAIN_SMALL_BANK_CHAINCODE_H

#include "block_server/worker/impl/chaincode_object.h"
#include <functional>

class SmallBankChaincode: public ChaincodeObject {
public:
    explicit SmallBankChaincode(Transaction *transaction);
    int InvokeChaincode(const std::string& chaincodeName, const std::vector<std::string>& args) override;

protected:
    int InitFunc(const std::vector<std::string>& args) override;

    int query(const std::string& account);
    int amalgamate(const std::string& from, const std::string& to);
    int updateBalance(const std::string& from, const std::string& val);
    int updateSaving(const std::string& from, const std::string& val);
    int sendPayment(const std::string& from, const std::string& to, const std::string& amount);
    int writeCheck(const std::string& from, const std::string& amountRaw);

private:
    std::function<std::string(const std::string&)> queryLambda;
    std::function<void(const std::string&, int)> updateLambda;

    const int BALANCE = 1000;
    const std::string savingTab = "saving";
    const std::string checkingTab = "checking";
};


#endif //NEUBLOCKCHAIN_SMALL_BANK_CHAINCODE_H
