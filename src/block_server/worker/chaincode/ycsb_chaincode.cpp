//
// Created by peng on 2021/5/6.
//

#include "block_server/worker/chaincode/ycsb_chaincode.h"
#include "tpc-c.pb.h"
#include "glog/logging.h"

#include "block_server/database/orm/cc_orm_helper.h"
#include "block_server/database/orm/table_definition.h"
#include "block_server/database/orm/query.h"
#include "block_server/database/orm/insert.h"
#include "block_server/database/orm/fields/char_field.hpp"

YCSB_Chaincode::YCSB_Chaincode(Transaction *transaction)
        : ChaincodeObject(transaction),
          queryLambda{[&](const std::string& key) -> std::string {
              AriaORM::ORMQuery *query = helper->newQuery("ycsb");
              query->filter("key", AriaORM::StrFilter::EQU, key);
              auto* r = query->executeQuery();
              if (!r->hasNext()) {
                  return defValue;
              } else {
                  if (r->getString("value").empty())
                      return defValue;
                  return r->getString("value");
              }
          }},
          updateLambda { [&](const std::string& key, const std::string& value) {
              AriaORM::ORMInsert* insert = helper->newInsert("ycsb");
              insert->set("key", key);
              insert->set("value", value);
          }} { }

int YCSB_Chaincode::InvokeChaincode(const std::string &chaincodeName, const std::vector<std::string> &args) {
    // this is NOT real ycsb payload!
    // realArgs[0]: key
    // realArgs[1]: real ycsb payload
    YCSB_PAYLOAD ycsbPayload;
    ycsbPayload.ParseFromString(args[0]);
    const auto& realArgs = ycsbPayload.reads();
    const auto& tableName = ycsbPayload.table();
    if(chaincodeName == "write") {
        DCHECK(realArgs.size() == 2);
        return write(tableName, realArgs[0], realArgs[1]);
    }
    if(chaincodeName == "del") {
        DCHECK(realArgs.size() == 1);
        return del(tableName, realArgs[0]);
    }
    if(chaincodeName == "read") {
        DCHECK(realArgs.size() == 2);
        return read(tableName, realArgs[0], realArgs[1]);
    }
    if(chaincodeName == "create_data") {
        return InitFunc({});
    }
    return 0;
}

int YCSB_Chaincode::InitFunc(const std::vector<std::string> &args) {
    return 0;
}

int YCSB_Chaincode::write(const std::string&, const std::string &key, const std::string &val) {
    // old value
    YCSB_FOR_BLOCK_BENCH old;
    old.ParseFromString(queryLambda(key));
    // the updated value
    YCSB_FOR_BLOCK_BENCH append;
    append.ParseFromString(val);
    // merge them together
    std::unordered_map<std::string, std::string> tmp;
    for(const auto& value: old.values()) {
        tmp[value.key()] = value.value();
    }
    for(const auto& value: append.values()) {
        tmp[value.key()] = value.value();
    }
    YCSB_FOR_BLOCK_BENCH merge;
    for(const auto& value: tmp) {
        auto* valueInner = merge.add_values();
        valueInner->set_key(value.first);
        valueInner->set_value(value.second);
    }
    updateLambda(key, merge.SerializeAsString());
    return true;
}

int YCSB_Chaincode::del(const std::string&, const std::string &key) {
    // drop all value
    updateLambda(key, "");
    return true;
}

int YCSB_Chaincode::read(const std::string&, const std::string &key, const std::string &filter) {
    // all value
    YCSB_FOR_BLOCK_BENCH payload;
    payload.ParseFromString(queryLambda(key));
    // filter
    YCSB_FOR_BLOCK_BENCH payload2;
    payload2.ParseFromString(filter);
    // apply filter
    return true;
}
