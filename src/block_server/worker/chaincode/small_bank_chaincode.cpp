//
// Created by peng on 2021/4/29.
//

#include <common/random.h>
#include "block_server/worker/chaincode/small_bank_chaincode.h"
#include "tpc-c.pb.h"
#include "glog/logging.h"

#include "block_server/database/orm/cc_orm_helper.h"
#include "block_server/database/orm/table_definition.h"
#include "block_server/database/orm/query.h"
#include "block_server/database/orm/insert.h"
#include "block_server/database/orm/fields/char_field.hpp"

SmallBankChaincode::SmallBankChaincode(Transaction *transaction)
        : ChaincodeObject(transaction),
          queryLambda{[&](const std::string& key) -> std::string {
              AriaORM::ORMQuery *query = helper->newQuery("small_bank");
              query->filter("key", AriaORM::StrFilter::EQU, key);
              auto* r = query->executeQuery();
              if (!r->hasNext()) {
                  return std::to_string(BALANCE);
              } else {
                  if (r->getString("value").empty())
                      return std::to_string(BALANCE);
                  return r->getString("value");
              }
          }},
          updateLambda { [&](const std::string& key, int value) {
              AriaORM::ORMInsert* insert = helper->newInsert("small_bank");
              insert->set("key", key);
              insert->set("value", std::to_string(value));
          }} { }

int SmallBankChaincode::InvokeChaincode(const std::string &chaincodeName, const std::vector<std::string> &args) {
    YCSB_PAYLOAD ycsbPayload;
    ycsbPayload.ParseFromString(args[0]);
    const auto& realArgs = ycsbPayload.reads();
    if(chaincodeName == "amalgamate") {
        DCHECK(realArgs.size() == 2);
        return amalgamate(realArgs[0], realArgs[1]);
    }
    if(chaincodeName == "getBalance") {
        DCHECK(realArgs.size() == 1);
        return query(realArgs[0]);
    }
    if(chaincodeName == "updateBalance") {
        DCHECK(realArgs.size() == 2);
        return updateBalance(realArgs[0], realArgs[1]);
    }
    if(chaincodeName == "updateSaving") {
        DCHECK(realArgs.size() == 2);
        return updateSaving(realArgs[0], realArgs[1]);
    }
    if(chaincodeName == "sendPayment") {
        DCHECK(realArgs.size() == 3);
        return sendPayment(realArgs[0], realArgs[1], realArgs[2]);
    }
    if(chaincodeName == "writeCheck") {
        DCHECK(realArgs.size() == 2);
        return writeCheck(realArgs[0], realArgs[1]);
    }
    if(chaincodeName == "create_data") {
        return InitFunc({});
    }
    return 0;
}

// query a account's amount.
int SmallBankChaincode::query(const std::string &account) {
    auto bal_str1 = queryLambda(savingTab + "_" + account);
    auto bal_str2 = queryLambda(checkingTab + "_" + account);

    int bal1 = static_cast<int>(strtol(bal_str1.data(), nullptr, 10));
    int bal2 = static_cast<int>(strtol(bal_str2.data(), nullptr, 10));

    // block bench's cc ends here.
    DLOG(INFO) << "query account:" << account <<", savingTab: " << bal1  <<", checkingTab: " << bal2;
    return true;
}

// transfer all assets from a to b.
int SmallBankChaincode::amalgamate(const std::string &from, const std::string &to) {
    // TODO: we currently not support this
    if(from == to)
        return false;
    std::string src_bal_str = queryLambda(savingTab + "_" + from);
    std::string dest_bal_str = queryLambda(checkingTab + "_" + to);

    int src_bal = static_cast<int>(strtol(src_bal_str.data(), nullptr, 10));
    int dest_bal = static_cast<int>(strtol(dest_bal_str.data(), nullptr, 10));

    updateLambda(savingTab + "_" + from, 0);
    updateLambda(checkingTab + "_" + to, dest_bal + src_bal);
    return true;
}

// add some money to checkingTab of a account
int SmallBankChaincode::updateBalance(const std::string &from, const std::string &val) {
    auto bal_str = queryLambda(checkingTab + "_" + from);

    int balance = static_cast<int>(strtol(bal_str.data(), nullptr, 10));
    int amount = static_cast<int>(strtol(val.data(), nullptr, 10));
    if (amount < 0 && balance < (-amount)) {
        return false;
    }
    updateLambda(checkingTab+"_"+from, balance + amount);

    return true;
}

// add some money to savingTab of a account
int SmallBankChaincode::updateSaving(const std::string &from, const std::string &val) {
    auto bal_str = queryLambda(savingTab + "_" + from);
    int balance = static_cast<int>(strtol(bal_str.data(), nullptr, 10));
    int amount = static_cast<int>(strtol(val.data(), nullptr, 10));
    if (amount < 0 && balance < (-amount)) {
        return false;
    }
    updateLambda(savingTab + "_" + from, amount + balance);
    return true;
}

// send checkingTab a to b
int SmallBankChaincode::sendPayment(const std::string &from, const std::string &to, const std::string &amountRaw) {
    // TODO: we currently not support this
    if(from == to)
        return false;
    auto bal_str1 = queryLambda(checkingTab + "_" + from);
    auto bal_str2 = queryLambda(checkingTab + "_" + to);
    int amount = static_cast<int>(strtol(amountRaw.data(), nullptr, 10));

    int bal1 = static_cast<int>(strtol(bal_str1.data(), nullptr, 10));
    int bal2 = static_cast<int>(strtol(bal_str2.data(), nullptr, 10));

    bal1 -= amount;
    if (bal1 < 0 || amount < 0)
        return false;
    bal2 += amount;

    updateLambda(checkingTab + "_" + from, bal1);
    updateLambda(checkingTab + "_" + to, bal2);

    return true;
}

int SmallBankChaincode::writeCheck(const std::string &from, const std::string &amountRaw) {
    auto bal_str1 = queryLambda(checkingTab + "_" + from);
    int balance = static_cast<int>(strtol(bal_str1.data(), nullptr, 10));

    int amount = static_cast<int>(strtol(amountRaw.data(), nullptr, 10));
    balance -= amount;
    if(balance < 0)
        return false;
    updateLambda(checkingTab + "_" + from, balance);
    return true;
}

int SmallBankChaincode::InitFunc(const std::vector<std::string> &args) {
    Random random;
    for(int i = 0; i < 10000; i++) {
        AriaORM::ORMInsert* insert = helper->newInsert("small_bank");
        insert->set("key", checkingTab + "_" + std::to_string(i));
        insert->set("value", std::to_string(random.next()%100));
    }

    for(int i = 0; i < 10000; i++) {
        AriaORM::ORMInsert* insert = helper->newInsert("small_bank");
        insert->set("key", savingTab + "_" + std::to_string(i));
        insert->set("value", std::to_string(random.next()%100));
    }
    return true;
}
