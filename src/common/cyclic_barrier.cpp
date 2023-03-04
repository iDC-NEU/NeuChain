#include <chrono>
#include "common/cyclic_barrier.h"

cbar::CyclicBarrier::CyclicBarrier(uint32_t _parties, std::function<void()>  _callback)
    : parties(_parties), current_waits(0), callback(std::move(_callback)) { }

void cbar::CyclicBarrier::await(uint64_t nano_secs) {
    std::unique_lock<std::mutex> lock(mutex);
    if (current_waits < parties) {
        current_waits++;
    }
    if (current_waits >= parties) {
        if (callback != nullptr) {
            callback();
        }
        current_waits = 0;
        cv.notify_all();
        return;
    } else {
        if (nano_secs > 0) {
            cv.wait_for(lock, std::chrono::nanoseconds(nano_secs));
        } else {
            cv.wait(lock);
        }
    }
}

void cbar::CyclicBarrier::reset() {
    std::unique_lock<std::mutex> lock(mutex);
    if (callback != nullptr) {
        callback();
    }
    current_waits = 0;
    cv.notify_all();
}

uint32_t cbar::CyclicBarrier::get_barrier_size() const {
    return parties;
}

uint32_t cbar::CyclicBarrier::get_current_waiting() const {
    return current_waits;
}
