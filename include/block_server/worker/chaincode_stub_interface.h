//
// Created by peng on 2021/1/22.
//

#ifndef NEUBLOCKCHAIN_CHAINCODE_STUB_INTERFACE_H
#define NEUBLOCKCHAIN_CHAINCODE_STUB_INTERFACE_H

class TransactionPayload;

class ChaincodeStubInterface {
public:
    virtual ~ChaincodeStubInterface() = default;
    virtual int InvokeChaincode(const TransactionPayload& payload) = 0;
};

#endif //NEUBLOCKCHAIN_CHAINCODE_STUB_INTERFACE_H
