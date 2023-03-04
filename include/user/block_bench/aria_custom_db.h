//
// Created by peng on 2021/4/29.
//

#pragma once

#include <memory>
#include "custom_db.h"

namespace BlockBench {
    class AriaCustomDB : public CustomDB {
    public:
        AriaCustomDB();
        void timeConsuming() override;
        void calculateConsuming() override;
        void normalTransaction() override;
    };
}