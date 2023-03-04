//
// Created by peng on 2021/3/24.
//

#include "common/msp/crypto_sign.h"
#include "glog/logging.h"
#include <openssl/pem.h>

#define RSA_KEY_LENGTH 1024

bool CryptoSign::rsaEncrypt(const std::string &message, std::string *signature) const {
    signature->clear();
    signature->resize(1024);
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    unsigned int signatureLength;
    EVP_MD_CTX_init(mdCtx);     //reset hash context
    if(!EVP_SignInit_ex(mdCtx, EVP_sha256(), nullptr)) {   // set hash method
        LOG(ERROR) << "phase 1 error";
        EVP_MD_CTX_free(mdCtx);
        return false;
    }
    if(!EVP_SignUpdate(mdCtx, message.data(), message.size())) {
        LOG(ERROR) << "phase 2 error";
        EVP_MD_CTX_free(mdCtx);
        return false;
    }
    if(!EVP_SignFinal(mdCtx, reinterpret_cast<unsigned char *>(signature->data()), &signatureLength, pivKey)) {
        LOG(ERROR) << "phase 3 error";
        EVP_MD_CTX_free(mdCtx);
        return false;
    }
    signature->resize(signatureLength);
    EVP_MD_CTX_free(mdCtx);
    return true;
}

bool CryptoSign::rsaDecrypt(const std::string &message, const std::string &signature) const {
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    EVP_MD_CTX_init(mdCtx);     //reset hash context
    if(!EVP_VerifyInit_ex(mdCtx, EVP_sha256(), nullptr)) {   //验证初始化，设置摘要算法，一定要和签名一致。
        LOG(ERROR) << "phase 1 error";
        EVP_MD_CTX_free(mdCtx);
        return false;
    }
    if(!EVP_VerifyUpdate(mdCtx, message.data(), message.size())) {
        LOG(ERROR) << "phase 2 error";
        EVP_MD_CTX_free(mdCtx);
        return false;
    }
    if(!EVP_VerifyFinal(mdCtx, reinterpret_cast<const unsigned char *>(signature.data()), signature.length(), pubKey)) {
        LOG(ERROR) << "phase 3 error";
        EVP_MD_CTX_free(mdCtx);
        return false;
    }
    EVP_MD_CTX_free(mdCtx);
    return true;
}

CryptoSign::CryptoSign(const std::string& pubKeyFile, const std::string& pivKeyFile, const std::string& password) {
    pubKey = CryptoSign::openPublicKey(pubKeyFile.data());
    pivKey = CryptoSign::openPrivateKey(pivKeyFile.data(), reinterpret_cast<const unsigned char *>(password.data()));
}

bool CryptoSign::generateKeyFiles(const char *pub_keyfile, const char *pri_keyfile,
                                  const unsigned char *passwd, int passwd_len) {
    BIGNUM* bne = BN_new();
    BN_set_word(bne, RSA_F4);
    RSA *rsa = RSA_new();
    RSA_generate_key_ex(rsa, RSA_KEY_LENGTH, bne, nullptr);

    // 生成公钥文件
    BIO *bp = BIO_new(BIO_s_file());
    if(nullptr == bp) {
        LOG(ERROR) << "phase 1 error";
        return false;
    }
    if(BIO_write_filename(bp, (void *)pub_keyfile) <= 0) {
        LOG(ERROR) << "phase 2 error";
        return false;
    }

    if(!PEM_write_bio_RSAPublicKey(bp, rsa)) {
        LOG(ERROR) << "phase 3 error";
        return false;
    }
    DLOG(INFO) << "Create public key ok";
    BIO_free_all(bp);

    // 生成私钥文件
    bp = BIO_new_file(pri_keyfile, "w+");
    if(nullptr == bp) {
        LOG(ERROR) << "phase 1 error";
        return false;
    }

    if(PEM_write_bio_RSAPrivateKey(bp, rsa, EVP_des_ede3_ofb(), (unsigned char *)passwd,
                                   passwd_len, nullptr, nullptr) != 1) {
        LOG(ERROR) << "phase 3 error";
        return false;
    }

    DLOG(INFO) << "Create private key ok";
    BIO_free_all(bp);
    RSA_free(rsa);
    BN_free(bne);
    return true;
}

EVP_PKEY* CryptoSign::openPublicKey(const char *keyfile) {
    OpenSSL_add_all_algorithms();
    BIO *bp = BIO_new(BIO_s_file());;
    BIO_read_filename(bp, keyfile);
    if(nullptr == bp) {
        LOG(ERROR) << "phase 1 error";
        return nullptr;
    }
    RSA *rsa = PEM_read_bio_RSAPublicKey(bp, nullptr, nullptr, nullptr);
    if(rsa == nullptr) {
        LOG(ERROR) << "phase 2 error";
        return nullptr;
    }
    EVP_PKEY* key = EVP_PKEY_new();
    if(nullptr == key) {
        LOG(ERROR) << "phase 3 error";
        return nullptr;
    }
    EVP_PKEY_assign_RSA(key, rsa);
    DLOG(INFO) << "Load public key ok";
    BIO_free_all(bp);
    return key;
}

EVP_PKEY* CryptoSign::openPrivateKey(const char *keyfile, const unsigned char *passwd) {
    OpenSSL_add_all_algorithms();
    BIO *bp = BIO_new_file(keyfile, "rb");
    if(nullptr == bp) {
        LOG(ERROR) << "phase 1 error";
        return nullptr;
    }

    RSA *rsa = PEM_read_bio_RSAPrivateKey(bp, nullptr, nullptr, (void *)passwd);
    if(rsa == nullptr) {
        LOG(ERROR) << "phase 2 error";
        return nullptr;
    }
    EVP_PKEY* key = EVP_PKEY_new();
    if(nullptr == key) {
        LOG(ERROR) << "phase 3 error";
        return nullptr;
    }
    EVP_PKEY_assign_RSA(key, rsa);
    DLOG(INFO) << "Load private key ok";
    BIO_free_all(bp);
    return key;
}

void CryptoSign::printDigest(const std::string &signature) {
    for(std::size_t i = 0; i < signature.size(); i++) {
        if(i%16==0)
            printf("\n%08lxH: ",i);
        printf("%02x ", signature[i]);
    }
    printf("\n");
}
