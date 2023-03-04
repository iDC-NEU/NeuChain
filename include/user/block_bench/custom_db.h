//
// Created by peng on 2021/4/29.
//

#pragma once

#include "db_user_base.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace BlockBench{
    class CustomDB: public DBUserBase {
    public:
        static std::unique_ptr<BlockBench::CustomDB> CreateDB(const std::string& type, const std::vector<std::string>& args);
        static void ClientThread(std::unique_ptr<CustomDB> db, uint32_t benchmarkTime, uint32_t txRate);

    public:
        virtual void timeConsuming() = 0;
        virtual void calculateConsuming() = 0;
        virtual void normalTransaction() = 0;
    };
}
