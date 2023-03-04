//
// Created by peng on 2021/7/1.
//

#ifndef NEUBLOCKCHAIN_BLOCKING_MPMC_QUEUE_H
#define NEUBLOCKCHAIN_BLOCKING_MPMC_QUEUE_H

#include "common/concurrent_queue/mpmc_queue.h"
#include "common/concurrent_queue/light_weight_semaphore.hpp"

template <typename T>
class BlockingMPMCQueue {
public:
    explicit BlockingMPMCQueue(size_t size = 100000): queue(size) { }

    template <typename P>
    inline bool try_pop(P &&v) noexcept {
        if (sema.tryWait()) {
            while (!queue.try_pop(std::forward<P>(v)));
            return true;
        }
        return false;
    }

    template <typename P>
    inline void pop(P &&v) noexcept {
        while (!sema.wait());
        queue.pop(std::forward<P>(v));
    }

    template <typename P, typename = typename std::enable_if<std::is_nothrow_constructible<T, P &&>::value>::type>
    inline void push(P &&v) noexcept {
        queue.push(std::forward<P>(v));
        sema.signal();
    }

    [[nodiscard]] size_t availableApprox() const {
        return sema.availableApprox();
    }

private:
    rigtorp::MPMCQueue<T> queue;
    moodycamel::LightweightSemaphore sema;
};

#endif //NEUBLOCKCHAIN_BLOCKING_MPMC_QUEUE_H
