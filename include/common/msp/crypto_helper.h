//
// Created by peng on 4/25/21.
//

#ifndef NEUBLOCKCHAIN_CRYPTO_HELPER_H
#define NEUBLOCKCHAIN_CRYPTO_HELPER_H

#include <string>
#include "common/msp/crypto_sign.h"

namespace CryptoHelper {
    enum class ServerType {
        epochServer,
        blockServer,
        user,
    };
    void initCryptoSign(ServerType type);

    ServerType getServerType();

    void generateCrypto();

    const CryptoSign* getLocalBlockServerSigner();
    const CryptoSign* getLocalUserSigner();
    const CryptoSign* getEpochServerSigner();

    const CryptoSign* getBlockServerSigner(const std::string& ip);
    const CryptoSign* getUserSigner(const std::string& ip);
};


#endif //NEUBLOCKCHAIN_CRYPTO_HELPER_H
