//
// Created by peng on 2021/3/24.
//

#ifndef NEUBLOCKCHAIN_CRYPTO_SIGN_H
#define NEUBLOCKCHAIN_CRYPTO_SIGN_H

#include <string>
#include <openssl/evp.h>

class CryptoSign {
public:
    explicit CryptoSign(const std::string& pubKeyFile,
                        const std::string& pivKeyFile,
                        const std::string& password = "");
    // prepare for func reentry
    bool rsaEncrypt(const std::string& message, std::string* signature) const;
    // prepare for func reentry
    [[nodiscard]] bool rsaDecrypt(const std::string& message, const std::string& signature) const;

    static bool generateKeyFiles(const char *pub_keyfile, const char *pri_keyfile,
                          const unsigned char *passwd, int passwd_len);

protected:
    static EVP_PKEY* openPrivateKey(const char *keyfile, const unsigned char *passwd);
    static EVP_PKEY* openPublicKey(const char *keyfile);

    static void printDigest(const std::string& signature);

private:
    EVP_PKEY *pubKey;
    EVP_PKEY *pivKey;
};


#endif //NEUBLOCKCHAIN_CRYPTO_SIGN_H
