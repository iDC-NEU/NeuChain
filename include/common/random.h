//
// Created by lhx on 1/24/21.
//

#pragma once

#include <string>

class Random {
public:
    explicit Random(uint64_t seed = 0) { init_seed(seed); }

    void init_seed(uint64_t seed) {
        seed_ = (seed ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1);
    }

    void set_seed(uint64_t seed) { seed_ = seed; }

    [[nodiscard]] uint64_t get_seed() const { return seed_; }

    uint64_t next() { return ((uint64_t)next(32) << 32) + next(32); }

    uint64_t next(unsigned int bits) {
        seed_ = (seed_ * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        return (seed_ >> (48 - bits));
    }

    /* [0.0, 1.0) */
    double next_double() {
        return static_cast<double>(((uint64_t)next(26) << 27) + next(27)) / static_cast<double>(1ULL << 53);
    }

    uint64_t uniform_dist(uint64_t a, uint64_t b) {
        if (a == b)
            return a;
        return next() % (b - a + 1) + a;
    }

    std::string rand_str(std::size_t length, const std::string &str) {
        std::string result;
        auto str_len = str.length();
        for (auto i = 0u; i < length; i++) {
            int k = uniform_dist(0, str_len - 1);
            result += str[k];
        }
        return result;
    }

    std::string a_string(std::size_t min_len, std::size_t max_len) {
        auto len = uniform_dist(min_len, max_len);
        return rand_str(len, alpha());
    }
    uint64_t non_uniform_distribution(uint64_t A, uint64_t x, uint64_t y) {
        return (uniform_dist(0, A) | uniform_dist(x, y)) % (y - x + 1) + x;
    }

private:
    static const std::string &alpha() {
        static std::string alpha_ =
                "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        return alpha_;
    };

    uint64_t seed_{};
};

