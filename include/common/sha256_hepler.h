//
// Created by peng on 2021/3/23.
//
#ifndef NEUBLOCKCHAIN_SHA256_HEPLER_H
#define NEUBLOCKCHAIN_SHA256_HEPLER_H

#include "common/sha224_256.h"

/*
 * This class may cache state inside, thus not thread safe,
 * please prevent concurrent calls to one instance,
 * which may not throw exception!
 */

class SHA256Helper {
public:
    inline SHA256Helper() { SHA256Reset(&ctx); };

    // reset the state of class.
    // call this func when you get a bad hash value.
    // no need to call this func after created a new instance or finished hash a value.
    inline bool reset() {
        if (SHA256Reset(&ctx) != shaSuccess)
            return false;
        return true;
    }

    // hash a string, return the hash value immediately.
    inline bool hash(const std::string& data, std::string* digest) {
        if (!reset())
            return false;
        if(!append(data))
            return false;
        return execute(digest);
    }

    // hash a string, cache the value.
    // you can call append() func to append data,
    // or call execute() func to get the result.
    inline bool hash(const std::string& data) {
        if (!reset())
            return false;
        return append(data);
    }

    // append a string to current hash state.
    // if is called the first time, please confirm that the state has reset!
    inline bool append(const std::string& data) {
        if (SHA256Input(&ctx, reinterpret_cast<const uint8_t *>(data.data()), data.size()) != shaSuccess)
            return false;
        return true;
    }

    // get the hash results of the buffered string, reset the state.
    // so you can call hash() or append() func later.
    inline bool execute(std::string* digest) {
        digest->clear();
        digest->resize(SHA256HashSize);
        if(SHA256Result(&ctx, reinterpret_cast<uint8_t *>(digest->data())) != shaSuccess) {
            return false;
        }
        return reset();
    }

private:
    // the state of the hash value, need to be init after creation.
    SHA256Context ctx{};

};

#endif //NEUBLOCKCHAIN_SHA256_HEPLER_H
