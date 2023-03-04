//
// Created by peng on 2021/6/30.
//

#ifndef NEUBLOCKCHAIN_CV_WRAPPER_H
#define NEUBLOCKCHAIN_CV_WRAPPER_H

#include <mutex>
#include <condition_variable>

class CVWrapper {
public:
    template<typename Predicate>
    inline void wait(Predicate&& cond) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, std::forward<Predicate>(cond));
    }

    template<typename Predicate>
    inline void notify_one(Predicate&& func) {
        std::unique_lock<std::mutex> lock(mutex);
        func();
        cv.notify_one();
    }

    inline void notify_one() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.notify_one();
    }

    template<typename Predicate>
    inline void notify_all(Predicate&& func) {
        std::unique_lock<std::mutex> lock(mutex);
        func();
        cv.notify_all();
    }

    inline void notify_all() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.notify_all();
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
};

#endif //NEUBLOCKCHAIN_CV_WRAPPER_H
