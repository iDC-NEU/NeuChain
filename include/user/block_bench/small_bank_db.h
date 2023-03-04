//
// Created by peng on 2021/4/29.
//

#ifndef NEUBLOCKCHAIN_SMALL_BANK_DB_H
#define NEUBLOCKCHAIN_SMALL_BANK_DB_H

#include "db_user_base.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace BlockBench{
    class SmallBankDB: public DBUserBase {
    public:
        static std::unique_ptr<BlockBench::SmallBankDB> CreateDB(const std::string& type, const std::vector<std::string>& args);
        static void ClientThread(std::unique_ptr<SmallBankDB> db, uint32_t benchmarkTime, uint32_t txRate);

    public:
        virtual void amalgamate(uint32_t acc1, uint32_t acc2) = 0;
        virtual void getBalance(uint32_t acc) = 0;
        virtual void updateBalance(uint32_t acc, uint32_t amount) = 0;
        virtual void updateSaving(uint32_t acc, uint32_t amount) = 0;
        virtual void sendPayment(uint32_t acc1, uint32_t acc2, unsigned amount) = 0;
        virtual void writeCheck(uint32_t acc, uint32_t amount) = 0;
    };
}

#endif //NEUBLOCKCHAIN_SMALL_BANK_DB_H
