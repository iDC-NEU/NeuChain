//
// Created by peng on 2021/4/29.
//

#include "user/block_bench/small_bank_db.h"
#include "user/block_bench/aria_small_bank_db.h"
#include "common/random_generator.h"
#include "common/timer.h"
#include "common/base64.h"
#include "glog/logging.h"

std::unique_ptr<BlockBench::SmallBankDB> BlockBench::SmallBankDB::CreateDB(const std::string &type, const std::vector<std::string> &args) {
    if(type == "aria") {
        return std::make_unique<AriaSmallBankDB>();
    }
    return nullptr;
}

void BlockBench::SmallBankDB::ClientThread(std::unique_ptr<SmallBankDB> db, uint32_t benchmarkTime, uint32_t txRate) {
    pthread_setname_np(pthread_self(), "small_bank");
    UniformGenerator op_gen(1, 6, random());
    UniformGenerator acc_gen(1, 100000, random());
    UniformGenerator bal_gen(50, 100, random());
    DBUserBase::RateGenerator<std::function<void()>>(benchmarkTime, txRate, [&]() {
        auto op = op_gen.next();
        switch (op) {
            case 1:
                db->amalgamate(acc_gen.next(), acc_gen.next());
                break;
            case 2:
                db->getBalance(acc_gen.next());
                break;
            case 3:
                db->updateBalance(acc_gen.next(), bal_gen.next());
                break;
            case 4:
                db->updateSaving(acc_gen.next(), bal_gen.next());
                break;
            case 5:
                db->sendPayment(acc_gen.next(), acc_gen.next(), bal_gen.next());
                break;
            case 6:
                db->writeCheck(acc_gen.next(), bal_gen.next());
                break;
            default:
                LOG(ERROR) << "wrong op number.";
        }
    });
}