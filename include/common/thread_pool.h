//
// Created by peng on 5/10/21.
//

#ifndef NEUBLOCKCHAIN_THREAD_POOL_H
#define NEUBLOCKCHAIN_THREAD_POOL_H

#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include <functional>
#include <thread>

class ThreadPool {
public:
    explicit ThreadPool(int size = 10) {
        for(int i=0; i<size; i++) {
            threads.emplace_back(&ThreadPool::run, this);
        }
    }
    virtual ~ThreadPool() {
        for(size_t i=0; i<threads.size(); i++) {
            execute(nullptr);
        }
        for(auto& t: threads)
            t.join();
    }

    bool execute(const std::function<void()>& func){ return workload.enqueue(func); }

protected:
    void run() {
        pthread_setname_np(pthread_self(), "thread_pool");
        std::function<void()> task;
        while(true) {
            workload.wait_dequeue(task);
            if (task == nullptr)
                return;
            task();
        }
    }

private:
    moodycamel::BlockingConcurrentQueue<std::function<void()>> workload;
    std::vector<std::thread> threads;
};


#endif //NEUBLOCKCHAIN_THREAD_POOL_H
