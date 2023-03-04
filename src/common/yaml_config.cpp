//
// Created by peng on 4/9/21.
//

#include "common/yaml_config.h"
#include <mutex>
#include "glog/logging.h"

#define YAML_CONFIG_FILE "config.yaml"
#define BK_YAML_CONFIG_FILE "/tmp/config.yaml"

#define YAML_INIT_CRYPTO "init_crypto"
#define YAML_RESET_EPOCH "reset_epoch"

#define YAML_BLOCK_SERVER_IPS "server_ips"
#define YAML_LOCAL_BLOCK_SERVER_IP "local_server_ip"

#define YAML_BLOCK_SERVER_DATA "server_data"
#define YAML_BLOCK_SERVER_PUBLIC_PATH "public_crt"
#define YAML_BLOCK_SERVER_PRIVATE_PATH "private_crt"
#define YAML_USER_PUBLIC_PATH "user_public_crt"
#define YAML_USER_PRIVATE_PATH "user_private_crt"

#define YAML_EPOCH_SERVER_IPS "epoch_ips"

#define YAML_EPOCH_SERVER_DATA "epoch_data"
#define YAML_EPOCH_SERVER_PUBLIC_PATH "public_crt"
#define YAML_EPOCH_SERVER_PRIVATE_PATH "private_crt"

#define YAML_CC_TYPE "cc_type"
#define YAML_CC_CONFIG "cc_config"
#define YAML_CC_CUSTOM_FUNC_NAME "cc_test_func_name"
#define YAML_HB_CONFIG "heart_beat_config"

#define YAML_SEND_TO_ALL_CLIENT_PROXY "send_to_all_client_proxy"

#define YAML_AGGREGATION "aggregation"

#define YAML_TX_MGR_SHOW_DETAIL "tr_manager_show_categorize_detail"
#define YAML_TX_MGR_REUSE_TX "tr_manager_reuse_abort_transaction"

#define YAML_EARLY_EXECUTION "early_execution"
#define YAML_ASYNC_BLOCK_GEN "async_block_gen"
#define YAML_EARLY_RETURN "early_return"

YAMLConfig::YAMLConfig(const std::string &fileName)
        :data(YAML::LoadFile(fileName)) {}

std::vector<std::string> YAMLConfig::getBlockServerIPs() const {
    std::vector<std::string> serverIPs;
    if(auto blockServers = data[YAML_BLOCK_SERVER_IPS]) {
        for(auto blockServer: blockServers) {
            serverIPs.push_back(blockServer.as<std::string>());
        }
    }
    return serverIPs;
}

size_t YAMLConfig::getBlockServerCount() const {
    if(auto blockServers = data[YAML_BLOCK_SERVER_IPS]) {
        return blockServers.size();
    }
    LOG(ERROR) << "Can not get server count!";
    return 0;
}

std::unique_ptr<YAMLConfig::ServerConfigStruct> YAMLConfig::getBlockServerConfig(const std::string &ip) const {
    if(auto serverData = data[YAML_BLOCK_SERVER_DATA][ip]) {
        auto serverConfig = std::make_unique<YAMLConfig::ServerConfigStruct>();
        serverConfig->ip = ip;
        serverConfig->pubFilePath = serverData[YAML_BLOCK_SERVER_PUBLIC_PATH].as<std::string>();
        serverConfig->priFilePath = serverData[YAML_BLOCK_SERVER_PRIVATE_PATH].as<std::string>();
        serverConfig->usrPubFilePath = serverData[YAML_USER_PUBLIC_PATH].as<std::string>();
        serverConfig->usrPriFilePath = serverData[YAML_USER_PRIVATE_PATH].as<std::string>();
        return serverConfig;
    }
    LOG(ERROR) << "Can not get server detail!" << ip;
    return nullptr;
}

bool YAMLConfig::enableEarlyExecution() const {
    return data[YAML_EARLY_EXECUTION].as<bool>();
}

YAMLConfig *YAMLConfig::getInstance() {
    static YAMLConfig* config = nullptr;
    static std::mutex mutex;
    if(config == nullptr) {  // performance
        std::unique_lock<std::mutex> lock(mutex);
        if(config == nullptr) {  // prevent for reentry
            try {
                config = new YAMLConfig(YAML_CONFIG_FILE);
            }
            catch(const YAML::Exception& e) {
                LOG(ERROR) << YAML_CONFIG_FILE << " not exist, switch to bk file!";
                delete config;
                try {
                    config = new YAMLConfig(BK_YAML_CONFIG_FILE);
                }
                catch(const YAML::Exception& e) {
                    LOG(ERROR) << BK_YAML_CONFIG_FILE << " not exist!";
                    CHECK(false);
                }
            }
        }
    }
    return config;
}

bool YAMLConfig::initCrypto() const {
    return data[YAML_INIT_CRYPTO].as<bool>();
}

std::string YAMLConfig::getLocalBlockServerIP() const {
    return data[YAML_LOCAL_BLOCK_SERVER_IP].as<std::string>();
}

std::vector<std::string> YAMLConfig::getEpochServerIPs() const {
    std::vector<std::string> serverIPs;
    if(auto blockServers = data[YAML_EPOCH_SERVER_IPS]) {
        for(auto blockServer: blockServers) {
            serverIPs.push_back(blockServer.as<std::string>());
        }
    }
    return serverIPs;
}

std::unique_ptr<YAMLConfig::EpochConfigStruct> YAMLConfig::getEpochServerConfig() const {
    if(auto serverData = data[YAML_EPOCH_SERVER_DATA]) {
        auto serverConfig = std::make_unique<YAMLConfig::EpochConfigStruct>();
        serverConfig->pubFilePath = serverData[YAML_EPOCH_SERVER_PUBLIC_PATH].as<std::string>();
        serverConfig->priFilePath = serverData[YAML_EPOCH_SERVER_PRIVATE_PATH].as<std::string>();
        return serverConfig;
    }
    LOG(ERROR) << "Can not get epoch server detail!";
    return nullptr;
}

bool YAMLConfig::resetEpoch() const {
    return data[YAML_RESET_EPOCH].as<bool>();
}

std::unique_ptr<YAML::Node> YAMLConfig::getCCConfig() const {
    return std::make_unique<YAML::Node>(data[YAML_CC_CONFIG]);
}

std::unique_ptr<YAML::Node> YAMLConfig::getHeartBeatConfig() const {
    return std::make_unique<YAML::Node>(data[YAML_HB_CONFIG]);
}

std::string YAMLConfig::getCCType() const {
    return data[YAML_CC_TYPE].as<std::string>();
}

bool YAMLConfig::useAggregation() const {
    return data[YAML_AGGREGATION]["enable"].as<bool>();
}

std::vector<std::string> YAMLConfig::getAggregationClusterIPs() const {
    std::vector<std::string> aggregationClusterIPs;
    if(auto blockServers = data[YAML_AGGREGATION]["cluster_ips"]) {
        for(auto blockServer: blockServers) {
            aggregationClusterIPs.push_back(blockServer.as<std::string>());
        }
    }
    return aggregationClusterIPs;
}

bool YAMLConfig::sendToAllClientProxy() const {
    return data[YAML_SEND_TO_ALL_CLIENT_PROXY].as<bool>();
}

bool YAMLConfig::trManagerShowDetail() const {
    return data[YAML_TX_MGR_SHOW_DETAIL].as<bool>();
}

bool YAMLConfig::trManagerReuseAbortTransaction() const {
    return data[YAML_TX_MGR_REUSE_TX].as<bool>();
}

size_t YAMLConfig::getLocalAggregationBlockServerCount() const {
    if(auto blockServers = data[YAML_AGGREGATION]["cluster_ips"]) {
        return blockServers.size();
    }
    LOG(ERROR) << "Can not get server count!";
    return 0;
}

bool YAMLConfig::enableAsyncBlockGen() const {
    return data[YAML_ASYNC_BLOCK_GEN].as<bool>();
}

std::string YAMLConfig::getCustomCCName() const {
    return data[YAML_CC_CUSTOM_FUNC_NAME].as<std::string>();
}

bool YAMLConfig::enableEarlyReturn() const {
    return data[YAML_EARLY_RETURN].as<bool>();
}
