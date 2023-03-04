//
// Created by peng on 4/25/21.
//


#include "common/msp/crypto_helper.h"
#include "common/yaml_config.h"

#include "glog/logging.h"
#include <memory>
#include <map>

namespace CryptoHelper {
    CryptoHelper::ServerType type;

    CryptoSign* localCryptoSigner;
    CryptoSign* localUserCryptoSigner;
    std::unique_ptr<CryptoSign> epochCryptoSigner;

    std::map<std::string, std::unique_ptr<CryptoSign>> cryptoSignerMap;
    std::map<std::string, std::unique_ptr<CryptoSign>> cryptoUserSignerMap;

    const YAMLConfig* yamlConfig;
}

void CryptoHelper::initCryptoSign(ServerType _type) {
    yamlConfig = YAMLConfig::getInstance();
    type = _type;

    if(yamlConfig->initCrypto()) {
        generateCrypto();
    }

    auto localServerConfig = yamlConfig->getBlockServerConfig(yamlConfig->getLocalBlockServerIP());
    auto epochServerConfig = yamlConfig->getEpochServerConfig();

    // 1. init local msp
    switch (type) {
        case ServerType::blockServer:
        {
            // 1.1 epoch server: pub
            epochCryptoSigner = std::make_unique<CryptoSign>(epochServerConfig->pubFilePath, localServerConfig->priFilePath);
            // 1.2 other server: pub
            for(const auto& ip: yamlConfig->getBlockServerIPs()) {
                auto remoteServerConfig = yamlConfig->getBlockServerConfig(ip);
                cryptoSignerMap[ip] = std::make_unique<CryptoSign>(remoteServerConfig->pubFilePath, localServerConfig->priFilePath);
                cryptoUserSignerMap[ip] = std::make_unique<CryptoSign>(remoteServerConfig->usrPubFilePath, localServerConfig->priFilePath);
            }
            // 1.3 local server: pub-pri
            localCryptoSigner = cryptoSignerMap[yamlConfig->getLocalBlockServerIP()].get();
            localUserCryptoSigner = cryptoUserSignerMap[yamlConfig->getLocalBlockServerIP()].get();
            break;
        }
        case ServerType::user:
        {
            // 1.1 block server: pub
            for(const auto& ip: yamlConfig->getBlockServerIPs()) {
                auto remoteServerConfig = yamlConfig->getBlockServerConfig(ip);
                cryptoSignerMap[ip] = std::make_unique<CryptoSign>(remoteServerConfig->pubFilePath, localServerConfig->usrPriFilePath);
            }
            // 1.2 local server: pub
            localCryptoSigner = cryptoSignerMap[yamlConfig->getLocalBlockServerIP()].get();
            // 1.3 local user: pub-pri
            if(yamlConfig->sendToAllClientProxy()) {
                // init all users in server_ips
                for(const auto& ip: yamlConfig->getBlockServerIPs()) {
                    auto remoteServerConfig = yamlConfig->getBlockServerConfig(ip);
                    cryptoUserSignerMap[ip] = std::make_unique<CryptoSign>(remoteServerConfig->usrPubFilePath, remoteServerConfig->usrPriFilePath);
                }
            } else {
                // just init local user pri-pub key pair
                cryptoUserSignerMap[yamlConfig->getLocalBlockServerIP()] = std::make_unique<CryptoSign>(localServerConfig->usrPubFilePath, localServerConfig->usrPriFilePath);
            }
            localUserCryptoSigner = cryptoUserSignerMap[yamlConfig->getLocalBlockServerIP()].get();
            break;
        }
        case ServerType::epochServer:
        {
            // 1.1 local server: pub-pri
            epochCryptoSigner = std::make_unique<CryptoSign>(epochServerConfig->pubFilePath, epochServerConfig->priFilePath);
            // 1.2 other server: pub
            for(const auto& ip: yamlConfig->getBlockServerIPs()) {
                auto remoteServerConfig = yamlConfig->getBlockServerConfig(ip);
                cryptoSignerMap[ip] = std::make_unique<CryptoSign>(remoteServerConfig->pubFilePath, epochServerConfig->priFilePath);
            }
            break;
        }
        default:
            CHECK(false);
    }
    // 2. init remote pub key

}

CryptoHelper::ServerType CryptoHelper::getServerType() {
    return CryptoHelper::type;
}

void CryptoHelper::generateCrypto() {
    std::string password;
    //1. generate block servers and user crypto.
    for(const auto& ip: yamlConfig->getBlockServerIPs()) {
        auto serverConfig = yamlConfig->getBlockServerConfig(ip);
        CryptoSign::generateKeyFiles(serverConfig->pubFilePath.data(), serverConfig->priFilePath.data(), reinterpret_cast<const unsigned char *>(password.data()), password.size());
        CryptoSign::generateKeyFiles(serverConfig->usrPubFilePath.data(), serverConfig->usrPriFilePath.data(), reinterpret_cast<const unsigned char *>(password.data()), password.size());
    }
    //2. generate epoch server crypto.
    auto serverConfig = yamlConfig->getEpochServerConfig();
    CryptoSign::generateKeyFiles(serverConfig->pubFilePath.data(), serverConfig->priFilePath.data(), reinterpret_cast<const unsigned char *>(password.data()), password.size());
}

const CryptoSign* CryptoHelper::getLocalBlockServerSigner() {
    DCHECK(type != ServerType::epochServer);
    return localCryptoSigner;
}

const CryptoSign* CryptoHelper::getLocalUserSigner() {
    DCHECK(type != ServerType::epochServer);
    return localUserCryptoSigner;
}

const CryptoSign* CryptoHelper::getEpochServerSigner() {
    DCHECK(type != ServerType::user);
    return epochCryptoSigner.get();
}

const CryptoSign* CryptoHelper::getBlockServerSigner(const std::string& ip) {
    return cryptoSignerMap[ip].get();
}

const CryptoSign* CryptoHelper::getUserSigner(const std::string& ip) {
    DCHECK(type != ServerType::epochServer);
    return cryptoUserSignerMap[ip].get();
}