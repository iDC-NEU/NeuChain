#ifndef SMART_CONTRACT_DRIVER_UTILS_GENERATORS_H_
#define SMART_CONTRACT_DRIVER_UTILS_GENERATORS_H_

#include <atomic>
#include <string>
#include <random>
#include "common/fnv_hash.h"
#include "glog/logging.h"

namespace BlockBench {
    template<typename Value>
    class Generator {
    public:
        virtual ~Generator() = default;

        virtual Value next() = 0;

        virtual Value last() = 0;
    };

    class UniformGenerator : public Generator<uint64_t> {
    public:
        // Both min and max are inclusive
        UniformGenerator(uint64_t min, uint64_t max, uint64_t seed = 0)
                : lastInt(seed), generator(seed), dist(min, max) {}

        uint64_t next() override { return lastInt = dist(generator); }

        uint64_t last() override { return lastInt; }

    private:
        uint64_t lastInt;
        std::mt19937_64 generator;
        std::uniform_int_distribution<uint64_t> dist;
    };

    template <typename Value>
    class DiscreteGenerator : public Generator<Value> {
    public:
        explicit DiscreteGenerator(uint64_t seed = 0)
                :generator(seed), uniform(0.0, 1.0) {}

        void addValue(Value value, double weight) {
            if (values.empty()) {
                lastValue = value;
            }
            values.push_back(std::make_pair(value, weight));
            sum += weight;
        }

        Value next() override {
            double chooser = randomDouble();
            for (auto p : values) {
                if (chooser < p.second / sum) {
                    return lastValue = p.first;
                }
                chooser -= p.second / sum;
            }
            CHECK(false);
            return lastValue;
        }

        Value last() override { return lastValue; }

    protected:
        inline double randomDouble() {
            return uniform(generator);
        }

    private:
        std::default_random_engine generator;
        std::uniform_real_distribution<double> uniform;
        std::vector<std::pair<Value, double>> values;
        double sum{};
        Value lastValue;
    };

    class ConstGenerator : public Generator<uint64_t> {
    public:
        explicit ConstGenerator(int constant) : constant(constant) { }

        uint64_t next() override { return constant; }

        uint64_t last() override { return constant; }

    private:
        uint64_t constant;
    };

    class ZipfianGenerator : public Generator<uint64_t> {
    public:
        constexpr static const double kZipfianConst = 0.99;
        static const uint64_t kMaxNumItems = (UINT64_MAX >> 24);

        ZipfianGenerator(uint64_t min, uint64_t max, uint64_t seed = 0, double zipfian_const = kZipfianConst) :
                numItems(max - min + 1), base(min), theta(zipfian_const),
                zetaN(0), nForZeta(0), generator(seed), uniform(0.0, 1.0) {
            CHECK(numItems >= 2 && numItems < kMaxNumItems);
            zeta2 = Zeta(2, theta);
            alpha = 1.0 / (1.0 - theta);
            RaiseZeta(numItems);
            eta = Eta();
            ZipfianGenerator::next();
        }

        explicit ZipfianGenerator(uint64_t num_items) :ZipfianGenerator(0, num_items - 1, random()) {}

        uint64_t next(uint64_t num_items) {
            CHECK(num_items >= 2 && num_items < kMaxNumItems);
            if (num_items > nForZeta) { // Recompute zeta_n and eta
                RaiseZeta(num_items);
                eta = Eta();
            }
            double u = randomDouble();
            double uz = u * zetaN;

            if (uz < 1.0) {
                return lastValue = 0;
            }
            if (uz < 1.0 + std::pow(0.5, theta)) {
                return lastValue = 1;
            }
            return lastValue = base + num_items * std::pow(eta * u - eta + 1, alpha);
        }

        uint64_t next() override { return next(numItems); }

        uint64_t last() override { return lastValue; }

    private:
        ///
        /// Compute the zeta constant needed for the distribution.
        /// Remember the number of items, so if it is changed, we can recompute zeta.
        ///
        void RaiseZeta(uint64_t num) {
            CHECK(num >= nForZeta);
            zetaN = Zeta(nForZeta, num, theta, zetaN);
            nForZeta = num;
        }

        [[nodiscard]] double Eta() const {
            return (1 - std::pow(2.0 / numItems, 1 - theta)) /
                   (1 - zeta2 / zetaN);
        }

        ///
        /// Calculate the zeta constant needed for a distribution.
        /// Do this incrementally from the last_num of items to the cur_num.
        /// Use the zipfian constant as theta. Remember the new number of items
        /// so that, if it is changed, we can recompute zeta.
        ///
        static double Zeta(uint64_t last_num, uint64_t cur_num,
                           double theta, double last_zeta) {
            double zeta = last_zeta;
            for (uint64_t i = last_num + 1; i <= cur_num; ++i) {
                zeta += 1 / std::pow(i, theta);
            }
            return zeta;
        }

        static double Zeta(uint64_t num, double theta) {
            return Zeta(0, num, theta, 0);
        }

        inline double randomDouble() {
            return uniform(generator);
        }

        uint64_t numItems;
        uint64_t base; /// Min number of items to generate

        // Computed parameters for generating the distribution
        double theta, zetaN, eta, alpha, zeta2;
        uint64_t nForZeta; /// Number of items used to compute zeta_n
        uint64_t lastValue{};

        // fo generate random double
        std::default_random_engine generator;
        std::uniform_real_distribution<double> uniform;
    };

    class ScrambledZipfianGenerator : public Generator<uint64_t> {
    public:
        ScrambledZipfianGenerator(uint64_t min, uint64_t max, uint64_t seed = 0, double zipfian_const = ZipfianGenerator::kZipfianConst)
                : baseVal(min), numItems(max - min + 1),
                  zipfianGenerator(min, max, seed, zipfian_const) { }

        explicit ScrambledZipfianGenerator(uint64_t num_items) :ScrambledZipfianGenerator(0, num_items - 1, random()) {}

        uint64_t next() override {
            uint64_t value = zipfianGenerator.next();
            value = baseVal + FNVHash64(value) % numItems;
            return lastVal = value;
        }

        uint64_t last() override { return lastVal; }

    private:
        uint64_t baseVal;
        uint64_t numItems;
        ZipfianGenerator zipfianGenerator;
        uint64_t lastVal{};
    };

    class CounterGenerator : public Generator<uint64_t> {
    public:
        explicit CounterGenerator(uint64_t start = 0) : counter(start) { }

        uint64_t next() override { return counter.fetch_add(1); }

        uint64_t last() override { return counter.load() - 1; }

        void reset(uint64_t start) { counter.store(start); }

    private:
        std::atomic<uint64_t> counter;
    };

    class SkewedLatestGenerator : public Generator<uint64_t> {
    public:
        explicit SkewedLatestGenerator(CounterGenerator* counter) :
                basis(counter), zipfianGenerator(basis->last()) {
            SkewedLatestGenerator::next();
        }

        uint64_t next() override {
            uint64_t max = basis->last();
            return lastVal = max - zipfianGenerator.next(max);
        }

        uint64_t last() override { return lastVal; }

    private:
        CounterGenerator* basis;
        ZipfianGenerator zipfianGenerator;
        uint64_t lastVal{};
    };
}

#endif  // SMART_CONTRACT_DRIVERS_GENERATORS_H_
