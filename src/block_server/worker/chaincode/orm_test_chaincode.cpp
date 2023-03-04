//
// Created by peng on 3/2/21.
//

#include "block_server/worker/chaincode/orm_test_chaincode.h"
#include "glog/logging.h"

#include "block_server/database/orm/cc_orm_helper.h"
#include "block_server/database/orm/table_definition.h"
#include "block_server/database/orm/query.h"
#include "block_server/database/orm/insert.h"
#include "block_server/database/orm/fields/char_field.hpp"
#include "block_server/database/orm/fields/int_field.hpp"
#include "tpc-c.pb.h"
#include "common/timer.h"

#define YCSB_FIELD_SIZE 10
#define KEY_PER_PA 200000

int ORMTestChaincode::InvokeChaincode(const std::string& chaincodeName, const std::vector<std::string> &args) {
    if(chaincodeName == "correct_test") {
        return correctTest(args[0]);
    }

    if(chaincodeName == "create_table") {
        return InitFunc(args);
    }

    if(chaincodeName == "create_data") {
        return createData(args);
    }

    if(chaincodeName == "mixed") {
        return mixed(args[0]);
    }

    if(chaincodeName == "time") {
        return timeConsume(args[0]);
    }

    if(chaincodeName == "calculate") {
        return calculateConsume(args[0]);
    }
    return false;
}

int ORMTestChaincode::InitFunc(const std::vector<std::string> &args) {
    AriaORM::ORMTableDefinition* table = helper->createTable("test_table");
    // first, init primary key attr, this field is int field,
    // (attribute name: key, can be nil: false).
    std::shared_ptr<AriaORM::IntField> intField =
            std::make_shared<AriaORM::IntField>("key", false);
    intField->setPrimary(true);
    table->addField(intField);
    // then, init value attr, this field is char(string) field,
    // (attribute name: value, max_length is 200, can be nil: false).
    std::shared_ptr<AriaORM::CharField> charField =
            std::make_shared<AriaORM::CharField>("value", 200, true);
    table->addField(charField);
    // return true to commit this transaction.
    return true;
}

int ORMTestChaincode::createData(const std::vector<std::string> &args) {
    for(int i = 0; i < KEY_PER_PA; i++) {
        AriaORM::ORMInsert* insert = helper->newInsert("test_table");
        insert->set("key", std::to_string(i));
        insert->set("value", random.a_string(YCSB_FIELD_SIZE, YCSB_FIELD_SIZE));
    }

    return true;
}

int ORMTestChaincode::correctTest(const std::string &key) {
    AriaORM::ORMQuery *query = helper->newQuery("test_table");
    query->filter("key", AriaORM::StrFilter::EQU, key);
    for (AriaORM::ResultsIterator *r = query->executeQuery(); r->hasNext(); r->next()) {
        //auto add read set in here
        LOG(INFO) << this->transactionID <<" READ: (" << r->getString("key") <<", " <<  r->getString("value") << ")";
    }
    AriaORM::ORMInsert* insert = helper->newInsert("test_table");
    std::string value = std::to_string(transactionID);
    insert->set("key", key);
    insert->set("value", value);
    LOG(INFO) << this->transactionID <<" INSERT: (" << key <<", " <<  value << ")";
    return true;
}

int ORMTestChaincode::mixed(const std::string &raw) {
    YCSB_PAYLOAD ycsbPayload;
    ycsbPayload.ParseFromString(raw);

    for (auto& write: ycsbPayload.update()) {
        AriaORM::ORMInsert* insert = helper->newInsert("test_table");
        insert->set("key", write);
        insert->set("value", random.a_string(YCSB_FIELD_SIZE, YCSB_FIELD_SIZE) + std::to_string(transactionID));
    }

    for (auto& read: ycsbPayload.reads()) {
        //table: test_table, pk column: test
        AriaORM::ORMQuery *query = helper->newQuery("test_table");
        query->filter("key", AriaORM::StrFilter::EQU, read);

        for (AriaORM::ResultsIterator *r = query->executeQuery(); r->hasNext(); r->next()) {
            //auto add read set in here
            r->getString("key");
            r->getString("value");
        }
    }
    return true;
}

int ORMTestChaincode::timeConsume(const std::string& delay) {
    BlockBench::Timer::sleep(static_cast<double>(random.next()%100) / 50000);   // 0 - 0.002s
    return true;
}

#include "common/sha256_hepler.h"

int ORMTestChaincode::calculateConsume(const std::string &delay) {
    SHA256Helper sha256Helper;
    std::string digest;
    for (int i=0; i<10; i++) {
        sha256Helper.hash(random.a_string(1000, 1000), &digest);
    }
    return true;
}
