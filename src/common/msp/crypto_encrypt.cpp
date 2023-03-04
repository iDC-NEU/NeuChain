#include <cstring>
#include <string>
#include <cstdio>
#include "common/msp/crypto_encrypt.h"

#define RSA_KEYLEN 2048
#define AES_ROUNDS 6

#define PSEUDO_CLIENT

#define USE_PBKDF

#define SUCCESS 0
#define FAILURE -1

EVP_PKEY* CryptoEncrypt::localKeypair;

CryptoEncrypt::CryptoEncrypt() {
    localKeypair = nullptr;
    remotePublicKey = nullptr;

#ifdef PSEUDO_CLIENT
    generateRsaKeypair(&remotePublicKey);
#endif

    init();
    encryptRsaTest();
    encryptAesTest();
}

CryptoEncrypt::CryptoEncrypt(unsigned char *remotePublicKey, size_t remotePublicKeyLength) {
    localKeypair = nullptr;
    this->remotePublicKey = nullptr;

    setRemotePublicKey(remotePublicKey, remotePublicKeyLength);
    init();
}

CryptoEncrypt::~CryptoEncrypt() {
    EVP_PKEY_free(remotePublicKey);

    EVP_CIPHER_CTX_free(rsaEncryptContext);
    EVP_CIPHER_CTX_free(aesEncryptContext);

    EVP_CIPHER_CTX_free(rsaDecryptContext);
    EVP_CIPHER_CTX_free(aesDecryptContext);

    free(aesKey);
    free(aesIv);
}

int CryptoEncrypt::init() {
    // Initalize contexts
    rsaEncryptContext = EVP_CIPHER_CTX_new();
    aesEncryptContext = EVP_CIPHER_CTX_new();

    rsaDecryptContext = EVP_CIPHER_CTX_new();
    aesDecryptContext = EVP_CIPHER_CTX_new();

    // Check if any of the contexts initializations failed
    if(rsaEncryptContext == nullptr || aesEncryptContext == nullptr || rsaDecryptContext == nullptr || aesDecryptContext == nullptr) {
        return FAILURE;
    }

    /* Don't set key or IV right away; we want to set lengths */
    EVP_CIPHER_CTX_init(aesEncryptContext);
    EVP_CIPHER_CTX_init(aesDecryptContext);

    EVP_CipherInit_ex(aesEncryptContext, EVP_aes_256_cbc(), nullptr, nullptr, nullptr, 1);

    /* Now we can set key and IV lengths */
    aesKeyLength = EVP_CIPHER_CTX_key_length(aesEncryptContext);
    aesIvLength = EVP_CIPHER_CTX_iv_length(aesEncryptContext);

    // Generate RSA and AES keys
    generateRsaKeypair(&localKeypair);
    generateAesKey(&aesKey, &aesIv);

    return SUCCESS;
}

int CryptoEncrypt::generateRsaKeypair(EVP_PKEY **keypair) {
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

    if(EVP_PKEY_keygen_init(context) <= 0) {
        return FAILURE;
    }

    if(EVP_PKEY_CTX_set_rsa_keygen_bits(context, RSA_KEYLEN) <= 0) {
        return FAILURE;
    }

    if(EVP_PKEY_keygen(context, keypair) <= 0) {
        return FAILURE;
    }

    EVP_PKEY_CTX_free(context);

    return SUCCESS;
}

int CryptoEncrypt::generateAesKey(unsigned char **aesKey, unsigned char **aesIv) {
    *aesKey = (unsigned char*)malloc(aesKeyLength);
    *aesIv = (unsigned char*)malloc(aesIvLength);

    // For the AES key we have the option of using a PBKDF or just using straight random
    // data for the key and IV. Depending on your use case, you will want to pick one or another.
#ifdef USE_PBKDF
    auto *aesPass = (unsigned char*)malloc(aesKeyLength);
    auto *aesSalt = (unsigned char*)malloc(8);

    if(aesPass == nullptr || aesSalt == nullptr) {
      return FAILURE;
    }

    // Get some random data to use as the AES pass and salt
    if(RAND_bytes(aesPass, aesKeyLength) == 0) {
      return FAILURE;
    }

    if(RAND_bytes(aesSalt, 8) == 0) {
      return FAILURE;
    }

    if(EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha256(), aesSalt, aesPass, aesKeyLength, AES_ROUNDS, *aesKey, *aesIv) == 0) {
      return FAILURE;
    }

    free(aesPass);
    free(aesSalt);
#else
    if(RAND_bytes(*aesKey, aesKeyLength) == 0) {
        return FAILURE;
    }

    if(RAND_bytes(*aesIv, aesIvLength) == 0) {
        return FAILURE;
    }
#endif

    return SUCCESS;
}

bool CryptoEncrypt::rsaEncrypt(const std::string& message, std::string* encryptedMessage, std::string* encryptedKey, std::string* iv) {
    size_t messageLength = message.size();
    size_t encryptedMessageLength = 0;
    size_t blockLength = 0;
    // Allocate memory for everything
    encryptedMessage->resize(messageLength + EVP_MAX_IV_LENGTH);
    encryptedKey->resize(EVP_PKEY_size(remotePublicKey));
    iv->resize(EVP_MAX_IV_LENGTH);

    auto* encryptedKeyData = reinterpret_cast<unsigned char *>(encryptedKey->data());
    auto* ivData = reinterpret_cast<unsigned char *>(iv->data());
    int encryptedKeyLength;
    // Encrypt it!
    if(!EVP_SealInit(rsaEncryptContext, EVP_aes_256_cbc(), &encryptedKeyData, &encryptedKeyLength, ivData, &remotePublicKey, 1)) {
        return false;
    }
    encryptedKey->resize(encryptedKeyLength);

    if(!EVP_SealUpdate(rsaEncryptContext, (unsigned char *) (encryptedMessage->data() + encryptedMessageLength), (int*)&blockLength,
                       reinterpret_cast<const unsigned char *>(message.data()), messageLength)) {
        return false;
    }
    encryptedMessageLength += blockLength;

    if(!EVP_SealFinal(rsaEncryptContext, (unsigned char *) (encryptedMessage->data() + encryptedMessageLength), (int*)&blockLength)) {
        return false;
    }
    encryptedMessageLength += blockLength;
    encryptedMessage->resize(encryptedMessageLength);

    return true;
}

bool CryptoEncrypt::rsaDecrypt(const std::string& encryptedMessage, const std::string& encryptedKey, const std::string& iv, std::string* decryptedMessage) {
    size_t decryptedMessageLength = 0;
    size_t blockLength = 0;
    decryptedMessage->resize(encryptedMessage.size() + iv.size());

#ifdef PSEUDO_CLIENT
    EVP_PKEY *key = remotePublicKey;
#else
    EVP_PKEY *key = localKeypair;
#endif

    // Decrypt it!
    if(!EVP_OpenInit(rsaDecryptContext, EVP_aes_256_cbc(),
                     reinterpret_cast<const unsigned char *>(encryptedKey.data()), encryptedKey.size(),
                     reinterpret_cast<const unsigned char *>(iv.data()), key)) {
        return false;
    }

    if(!EVP_OpenUpdate(rsaDecryptContext,
                       reinterpret_cast<unsigned char *>(decryptedMessage->data() + decryptedMessageLength), (int*)&blockLength,
                       reinterpret_cast<const unsigned char *>(encryptedMessage.data()), encryptedMessage.size())) {
        return false;
    }
    decryptedMessageLength += blockLength;

    if(!EVP_OpenFinal(rsaDecryptContext, reinterpret_cast<unsigned char *>(decryptedMessage->data() + decryptedMessageLength), (int*)&blockLength)) {
        return false;
    }
    decryptedMessageLength += blockLength;
    decryptedMessage->resize(decryptedMessageLength);
    return true;
}

bool CryptoEncrypt::aesEncrypt(const std::string& message, std::string* encryptedMessage) {
    // Allocate memory for everything
    size_t messageLength = message.size();
    size_t blockLength = 0;
    size_t encryptedMessageLength = 0;

    encryptedMessage->resize(messageLength + AES_BLOCK_SIZE);

    // Encrypt it!
    if(!EVP_EncryptInit_ex(aesEncryptContext, EVP_aes_256_cbc(), nullptr, aesKey, aesIv)) {
        return false;
    }

    if(!EVP_EncryptUpdate(aesEncryptContext, reinterpret_cast<unsigned char *>(encryptedMessage->data()),
                          (int*)&blockLength, (unsigned char*)message.data(), messageLength)) {
        return false;
    }
    encryptedMessageLength += blockLength;

    if(!EVP_EncryptFinal_ex(aesEncryptContext, reinterpret_cast<unsigned char *>(encryptedMessage->data() + encryptedMessageLength),
                            (int*)&blockLength)) {
        return false;
    }
    encryptedMessageLength += blockLength;
    encryptedMessage->resize(encryptedMessageLength);

    return true;
}

bool CryptoEncrypt::aesDecrypt(const std::string& encryptedMessage, std::string* decryptedMessage) {
    // Allocate memory for everything
    size_t encryptedMessageLength = encryptedMessage.size();
    size_t decryptedMessageLength = 0;
    size_t blockLength = 0;
    decryptedMessage->resize(encryptedMessageLength);

    // Decrypt it!
    if(!EVP_DecryptInit_ex(aesDecryptContext, EVP_aes_256_cbc(), nullptr, aesKey, aesIv)) {
        return false;
    }

    if(!EVP_DecryptUpdate(aesDecryptContext, reinterpret_cast<unsigned char *>(decryptedMessage->data()), (int*)&blockLength,
                          reinterpret_cast<const unsigned char *>(encryptedMessage.data()), (int)encryptedMessageLength)) {
        return false;
    }
    decryptedMessageLength += blockLength;

    if(!EVP_DecryptFinal_ex(aesDecryptContext, reinterpret_cast<unsigned char *>(decryptedMessage->data() + decryptedMessageLength),
                            (int*)&blockLength)) {
        return false;
    }
    decryptedMessageLength += blockLength;
    decryptedMessage->resize(decryptedMessageLength);

    return true;
}

int CryptoEncrypt::getRemotePublicKey(unsigned char **publicKey) {
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, remotePublicKey);
    return bioToString(bio, publicKey);
}

int CryptoEncrypt::setRemotePublicKey(unsigned char *publicKey, size_t publicKeyLength) {
    BIO *bio = BIO_new(BIO_s_mem());

    if(BIO_write(bio, publicKey, publicKeyLength) != (int)publicKeyLength) {
        return FAILURE;
    }

    PEM_read_bio_PUBKEY(bio, &remotePublicKey, nullptr, nullptr);
    BIO_free_all(bio);

    return SUCCESS;
}

int CryptoEncrypt::getLocalPublicKey(unsigned char **publicKey) {
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, localKeypair);
    return bioToString(bio, publicKey);
}

int CryptoEncrypt::getLocalPrivateKey(unsigned char **privateKey) {
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, localKeypair, nullptr, nullptr, 0, 0, nullptr);
    return bioToString(bio, privateKey);
}

int CryptoEncrypt::getAesKey(unsigned char **_aesKey) {
    *_aesKey = this->aesKey;
    return aesKeyLength;
}

int CryptoEncrypt::setAesKey(unsigned char *_aesKey, size_t aesKeyLengthgth) {
    // Ensure the new key is the proper size
    if(aesKeyLengthgth != aesKeyLength) {
        return FAILURE;
    }

    memcpy(this->aesKey, _aesKey, aesKeyLength);

    return SUCCESS;
}

int CryptoEncrypt::getAesIv(unsigned char **_aesIv) {
    *_aesIv = this->aesIv;
    return aesIvLength;
}

int CryptoEncrypt::setAesIv(unsigned char *_aesIv, size_t aesIvLengthgth) {
    // Ensure the new IV is the proper size
    if(aesIvLengthgth != aesIvLength) {
        return FAILURE;
    }

    memcpy(this->aesIv, _aesIv, aesIvLength);

    return SUCCESS;
}

int CryptoEncrypt::writeKeyToFile(void *file, int key) {
    switch(key) {
        case KEY_SERVER_PRI:
            if(!PEM_write_PrivateKey((FILE*)file, localKeypair, nullptr, nullptr, 0, 0, nullptr)) {
                return FAILURE;
            }
            break;

        case KEY_SERVER_PUB:
            if(!PEM_write_PUBKEY((FILE*)file, localKeypair)) {
                return FAILURE;
            }
            break;

        case KEY_CLIENT_PUB:
            if(!PEM_write_PUBKEY((FILE*)file, remotePublicKey)) {
                return FAILURE;
            }
            break;

        case KEY_AES:
            fwrite(aesKey, 1, aesKeyLength * 8, (FILE*)file);
            break;

        case KEY_AES_IV:
            fwrite(aesIv, 1, aesIvLength * 8, (FILE*)file);
            break;

        default:
            return FAILURE;
    }

    return SUCCESS;
}

int CryptoEncrypt::bioToString(BIO *bio, unsigned char **string) {
    size_t bioLength = BIO_pending(bio);
    *string = (unsigned char*)malloc(bioLength + 1);

    BIO_read(bio, *string, bioLength);

    // Insert the NUL terminator
    (*string)[bioLength] = '\0';

    BIO_free_all(bio);

    return (int)bioLength;
}

bool CryptoEncrypt::encryptRsaTest() {
    std::string message ={'\x01', '\x00', '\x32'};
    std::string encryptedMessage;
    std::string encryptedKey;
    std::string iv;
    if(!this->rsaEncrypt(message, &encryptedMessage, &encryptedKey, &iv)) {
        return false;
    }
    // Decrypt the message
    std::string decryptedMessage;
    if(!this->rsaDecrypt(encryptedMessage, encryptedKey, iv, &decryptedMessage)) {
        return false;
    }

    if (message == decryptedMessage && message[message.size()-1] == decryptedMessage[decryptedMessage.size()-1]) {
        printf("RSA test ok!\n");
        return true;
    }
    return false;
}

bool CryptoEncrypt::encryptAesTest() {
    // Get the message to encrypt
    std::string message = "aes test!";
    std::string encryptedMessage;

    // Encrypt the message with AES
    if(!this->aesEncrypt(message, &encryptedMessage))
        return false;
    // Decrypt the message
    std::string decryptedMessage;
    if(!this->aesDecrypt(encryptedMessage, &decryptedMessage))
        return false;
    if (message == decryptedMessage && message[message.size()-1] == decryptedMessage[decryptedMessage.size()-1]) {
        printf("AES test ok!\n");
        return true;
    }
    return true;
}

void CryptoEncrypt::printBytesAsHex(unsigned char *bytes, size_t length, const char *message) {
    printf("%s: ", message);

    for(unsigned int i=0; i<length; i++) {
        printf("%02hhx", bytes[i]);
    }

    puts("");
}

void CryptoEncrypt::printKeys() {
    // Write the RSA keys to stdout
    this->writeKeyToFile(stdout, KEY_SERVER_PRI);
    this->writeKeyToFile(stdout, KEY_SERVER_PUB);
    this->writeKeyToFile(stdout, KEY_CLIENT_PUB);

    // Write the AES key to stdout in hex
    printBytesAsHex(aesKey, aesKeyLength, "AES Key");

    // Write the AES IV to stdout in hex
    printBytesAsHex(aesIv, aesIvLength, "AES IV");
}