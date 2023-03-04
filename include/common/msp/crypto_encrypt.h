#ifndef CRYPTO_H
#define CRYPTO_H

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

#define KEY_SERVER_PRI 0
#define KEY_SERVER_PUB 1
#define KEY_CLIENT_PUB 2
#define KEY_AES        3
#define KEY_AES_IV     4

class CryptoEncrypt {
public:
    CryptoEncrypt();
    CryptoEncrypt(unsigned char *remotePubKey, size_t remotePubKeyLen);
    ~CryptoEncrypt();

    bool rsaEncrypt(const std::string& message, std::string* encryptedMessage, std::string* encryptedKey, std::string* iv);
    bool rsaDecrypt(const std::string& encryptedMessage, const std::string& encryptedKey, const std::string& iv, std::string* decryptedMessage);

    bool aesEncrypt(const std::string& message, std::string* encryptedMessage);
    bool aesDecrypt(const std::string& encryptedMessage, std::string* decryptedMessage);

    int getRemotePublicKey(unsigned char **publicKey);
    int setRemotePublicKey(unsigned char *publicKey, size_t publicKeyLength);

    int getLocalPublicKey(unsigned char **publicKey);
    int getLocalPrivateKey(unsigned char **privateKey);

    int getAesKey(unsigned char **aesKey);
    int setAesKey(unsigned char *aesKey, size_t aesKeyLen);

    int getAesIv(unsigned char **aesIv);
    int setAesIv(unsigned char *aesIv, size_t aesIvLen);

    int writeKeyToFile(void *file, int key);
    void printKeys();

protected:
    static void printBytesAsHex(unsigned char *bytes, size_t length, const char *message);
    int init();
    int generateRsaKeypair(EVP_PKEY **keypair);
    int generateAesKey(unsigned char **aesKey, unsigned char **aesIv);
    static int bioToString(BIO *bio, unsigned char **string);

    bool encryptRsaTest();
    bool encryptAesTest();
private:
    static EVP_PKEY *localKeypair;
    EVP_PKEY *remotePublicKey;

    EVP_CIPHER_CTX *rsaEncryptContext{};
    EVP_CIPHER_CTX *aesEncryptContext{};

    EVP_CIPHER_CTX *rsaDecryptContext{};
    EVP_CIPHER_CTX *aesDecryptContext{};

    unsigned char *aesKey{};
    unsigned char *aesIv{};

    size_t aesKeyLength{};
    size_t aesIvLength{};
};

#endif
