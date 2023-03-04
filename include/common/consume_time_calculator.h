//
// Created by peng on 2021/12/21.
//

#ifndef NEU_BLOCKCHAIN_CONSUME_TIME_CALCULATOR_H
#define NEU_BLOCKCHAIN_CONSUME_TIME_CALCULATOR_H

#include "aria_types.h"
#include "compile_config.h"
#include "timer.h"
#include "glog/logging.h"
#include <vector>

class ConsumeTimeCalculator {
public:
    ConsumeTimeCalculator() {
        epochHelper = 1;
        spinList.resize(MAX_EPOCH);
        timerList.resize(MAX_EPOCH);
        showed = false;
    }

#ifdef NO_TIME_BENCHMARK

    inline void startCalculate(int epoch) {}
    inline void endCalculate(int epoch) {}
    inline double getAverageResult(int endEpoch, const std::string& header) { return -1;}
    [[nodiscard]] inline int getCurrentEpoch() const { return -1; }
    void increaseEpoch() {}
    void setEpoch(epoch_size_t epoch) {}

#else
    void startCalculate(int epoch) {    // append to spin[epoch]
        int i = getIndexFromEpoch(epoch);
        timerList[i].tic();
    }
    void endCalculate(int epoch) {
        int i = getIndexFromEpoch(epoch);
        spinList[i].first += 1;
        spinList[i].second += timerList[i].toc();
    }
    double getAverageResult(int endEpoch, const std::string& header) {
        int i = getIndexFromEpoch(endEpoch);
        if (endEpoch < TIME_BENCHMARK_MAX_EPOCH || showed) {
            return 0;
        }
        showed = true;
        double totalSpan = 0;
        size_t totalSample = 0;
        size_t epochSample = 0;
        for (int j=0; j<=i; j++) {
            if (spinList[j].first == 0) {
                continue;
            }
            epochSample++;
            totalSample += spinList[j].first;
            totalSpan += spinList[j].second;
        }
        auto eat = totalSpan / (double)epochSample;
        auto sat = totalSpan / (double)totalSample;
        // EAT: epoch average time
        // SAT: sample average time
        LOG(INFO)  << " ID: "<< header << ", Epoch: " << endEpoch
                << ", EAT: " << eat << ", SAT: " << sat;
        return eat;
    }

    [[nodiscard]] int getCurrentEpoch() const {
        return (int)epochHelper;
    }

    void increaseEpoch() {
        epochHelper++;
    }

    void setEpoch(epoch_size_t epoch) {
        epochHelper = epoch;
    }
#endif

protected:
    [[nodiscard]] int getIndexFromEpoch(int epoch) const {
        int i = epoch - 1;
        CHECK(i >= 0 && i < MAX_EPOCH);
        return i;
    }

private:
    epoch_size_t epochHelper;
    std::vector<std::pair<size_t, double>> spinList;    // sample count, total duration
    std::vector<BlockBench::Timer> timerList;
    bool showed;
};


#endif //NEU_BLOCKCHAIN_CONSUME_TIME_CALCULATOR_H
