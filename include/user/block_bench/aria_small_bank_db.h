//
// Created by peng on 2021/4/29.
//

#ifndef NEUBLOCKCHAIN_ARIA_SMALL_BANK_DB_H
#define NEUBLOCKCHAIN_ARIA_SMALL_BANK_DB_H

#include <memory>
#include "small_bank_db.h"

namespace BlockBench {
    class AriaSmallBankDB : public SmallBankDB {
    public:
        AriaSmallBankDB();
        void amalgamate(uint32_t acc1, uint32_t acc2) override;
        void getBalance(uint32_t acc) override;
        void updateBalance(uint32_t acc, uint32_t amount) override;
        void updateSaving(uint32_t acc, uint32_t amount) override;
        void sendPayment(uint32_t acc1, uint32_t acc2, unsigned amount) override;
        void writeCheck(uint32_t acc, uint32_t amount) override;
    };

}

#endif //NEUBLOCKCHAIN_ARIA_SMALL_BANK_DB_H
