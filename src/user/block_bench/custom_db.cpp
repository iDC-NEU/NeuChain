//
// Created by peng on 2021/6/14.
//

#include "user/block_bench/custom_db.h"
#include "user/block_bench/aria_custom_db.h"
#include "common/random_generator.h"
#include "common/yaml_config.h"
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
    UniformGenerator op_gen(1, 100, random());
    const auto funcName = YAMLConfig::getInstance()->getCustomCCName();
    DBUserBase::RateGenerator<std::function<void()>>(benchmarkTime, txRate, [&]() {
        if (op_gen.next() == 1) {   // 1% long-tail transaction
            if (funcName == "time") {
                db->timeConsuming();
            }
            else if (funcName == "calculate") {
                db->calculateConsuming();
            } else {
                CHECK(false);
            }
        } else {
            db->normalTransaction();
        }
    });
}
