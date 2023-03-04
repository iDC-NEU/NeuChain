//
// Created by peng on 4/9/21.
//

#ifndef NEUBLOCKCHAIN_YAML_CONFIG_H
#define NEUBLOCKCHAIN_YAML_CONFIG_H

#include <yaml-cpp/yaml.h>
#include <memory>

class YAMLConfig {
public:
    struct ServerConfigStruct {
        std::string ip;
        std::string priFilePath;
        std::string pubFilePath;
        std::string usrPriFilePath;
        std::string usrPubFilePath;
    };

    struct EpochConfigStruct {
        std::string priFilePath;
        std::string pubFilePath;
    };

    static YAMLConfig* getInstance();
    virtual ~YAMLConfig() = default;
    bool initCrypto() const;
    bool resetEpoch() const;
    size_t getBlockServerCount() const;
    size_t getLocalAggregationBlockServerCount() const;
    std::vector<std::string> getBlockServerIPs() const;
    std::vector<std::string> getEpochServerIPs() const;
    std::string getLocalBlockServerIP() const;
    std::unique_ptr<ServerConfigStruct> getBlockServerConfig(const std::string& ip) const;
    std::unique_ptr<EpochConfigStruct> getEpochServerConfig() const;

    std::string getCCType() const;
    std::string getCustomCCName() const;
    std::unique_ptr<YAML::Node> getCCConfig() const;

    std::unique_ptr<YAML::Node> getHeartBeatConfig() const;
    bool sendToAllClientProxy() const;

    bool useAggregation() const;
    std::vector<std::string> getAggregationClusterIPs() const;

    bool trManagerShowDetail() const;
    bool trManagerReuseAbortTransaction() const;

    bool enableEarlyExecution() const;
    bool enableAsyncBlockGen() const;

    bool enableEarlyReturn() const;
protected:
    explicit YAMLConfig(const std::string &fileName);

private:
    const YAML::Node data;
};

#endif //NEUBLOCKCHAIN_YAML_CONFIG_H
