//
// Created by peng on 2021/5/7.
//

#include "block_server/worker/chaincode_manager.h"
#include "block_server/worker/chaincode/orm_test_chaincode.h"
#include "block_server/worker/chaincode/small_bank_chaincode.h"
#include "block_server/worker/chaincode/ycsb_chaincode.h"
#include "common/yaml_config.h"
#include "glog/logging.h"
#include <mutex>
#include <map>

volatile bool ChaincodeManager::isInit = false;

std::function<std::unique_ptr<ChaincodeObject>(Transaction*)> ChaincodeManager::getChaincodeInstance;

void ChaincodeManager::loadChaincodeManager(const std::string& ccType) {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if(!isInit) {
        std::map<std::string, decltype(getChaincodeInstance)> typeMap;
        typeMap.insert({"ycsb", [](Transaction* t)->std::unique_ptr<ChaincodeObject> {return std::make_unique<YCSB_Chaincode>(t);}});
        typeMap.insert({"small_bank", [](Transaction* t)->std::unique_ptr<ChaincodeObject> {return std::make_unique<SmallBankChaincode>(t);}});
        typeMap.insert({"test", [](Transaction* t)->std::unique_ptr<ChaincodeObject> {return std::make_unique<ORMTestChaincode>(t);}});

        if(!typeMap.count(ccType)) {
            LOG(ERROR) << "No chaincode provided, please check your config file (support: ycsb, small_bank, test)";
            CHECK(false);
        }
        getChaincodeInstance = typeMap[ccType];
        isInit = true;
    }
}

std::string ChaincodeManager::getCCType() {
    return YAMLConfig::getInstance()->getCCType();
}

ChaincodeManager::~ChaincodeManager() = default;
