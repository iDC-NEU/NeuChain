//
// Created by peng on 2021/1/16.
//

#ifndef NEUBLOCKCHAIN_SPINLOCK_H
#define NEUBLOCKCHAIN_SPINLOCK_H

#include <atomic>

class SpinLock {
public:
    SpinLock() = default;

    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;

    // Modifiers
    void lock() {
        while (lock_.test_and_set(std::memory_order_acquire));
    }

    void unlock() { lock_.clear(std::memory_order_release); }

private:
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};


#endif //NEUBLOCKCHAIN_SPINLOCK_H
