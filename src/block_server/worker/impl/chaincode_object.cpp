//
// Created by peng on 2021/3/7.
//

#include "block_server/worker/impl/chaincode_object.h"
#include "block_server/transaction/transaction.h"
#include "block_server/database/orm/cc_orm_helper.h"
#include "transaction.pb.h"

ChaincodeObject::ChaincodeObject(Transaction *transaction) {
    transactionID = transaction->getTransactionID();
    helper = std::make_unique<AriaORM::CCORMHelper>(transactionID, transaction->getRWSet(), transaction->readOnly());
}

int ChaincodeObject::InvokeChaincode(const TransactionPayload &payload) {
    return this->InvokeChaincode(payload.header(), { payload.payload() });
}

ChaincodeObject::~ChaincodeObject() = default;
