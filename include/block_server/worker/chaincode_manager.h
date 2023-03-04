//
// Created by peng on 2021/5/7.
//

#ifndef NEUBLOCKCHAIN_CHAINCODE_MANAGER_H
#define NEUBLOCKCHAIN_CHAINCODE_MANAGER_H

#include "block_server/worker/impl/chaincode_object.h"
#include <functional>
#include <memory>

class ChaincodeManager {
public:
    static inline void InitChaincodeManager(const std::string& str = getCCType()) {
        if(!isInit) {
            loadChaincodeManager(str);
        }
    }

    static inline void reloadChaincodeManager(const std::string& str = getCCType()) {
        isInit = false;
        loadChaincodeManager(str);
    }

    ~ChaincodeManager();
    static std::function<std::unique_ptr<ChaincodeObject>(Transaction*)> getChaincodeInstance;

protected:
    static std::string getCCType();
    static void loadChaincodeManager(const std::string& ccType);

private:
    volatile static bool isInit;
};

#endif //NEUBLOCKCHAIN_CHAINCODE_MANAGER_H
