//
// Created by peng on 2021/6/14.
//

#include "user/block_bench/custom_db.h"
#include "user/block_bench/aria_custom_db.h"
#include "common/random_generator.h"
#include "common/timer.h"
#include "common/base64.h"
#include "glog/logging.h"

std::unique_ptr<BlockBench::CustomDB>
BlockBench::CustomDB::CreateDB(const std::string &type, const std::vector<std::string> &args) {
    if(type == "aria") {
        return std::make_unique<AriaCustomDB>();
    }
    return nullptr;
}

void BlockBench::CustomDB::ClientThread(std::unique_ptr<CustomDB> db, uint32_t benchmarkTime, uint32_t txRate) {
    pthread_setname_np(pthread_self(), "custom_client");
    UniformGenerator op_gen(1, 1, random());
    DBUserBase::RateGenerator<std::function<void()>>(benchmarkTime, txRate, [&]() {
        auto op = op_gen.next();
        switch (op) {
            case 1:
                db->timeConsuming();
                break;
            case 2:
                db->calculateConsuming();
                break;
            default:
                LOG(ERROR) << "wrong op number.";
        }
    });
}
