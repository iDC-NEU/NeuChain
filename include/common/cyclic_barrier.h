#ifndef __CYCLICBARRIER_NIPUN__
#define __CYCLICBARRIER_NIPUN__

#include <mutex>
#include <functional>
#include <condition_variable>

// https://github.com/nipuntalukdar/cyclicbarrier
// not the origin version

namespace cbar
{
    class CyclicBarrier {
    public:
        /*! Constructor
          \param parties how many callers has to wait before all of them are waked up
          \param call , callable object which is fired once waiters are signaled
        */
        explicit CyclicBarrier(uint32_t parties, std::function<void()> callback = nullptr);

        /*!
          reset makes the CyclicBarrier object reusable, also wakes up any threads
          waiting on it  
        */
        void reset();

        /*!
            await is called to await for a number for required number of parties to wait 
            upon the barrier object. 
          \param nano_secs is waittime in nanoseconds, by default it is zero which specifies
           indefinite wait  
        */
        void await(uint64_t nano_secs=0);

        /*!
          get_barrier_size returns the number of parties required to wait upon the object, before
          the waiters are woke up 
        */
        [[nodiscard]] uint32_t get_barrier_size() const;

        /*!
            get_current_waiting returns how many threads are currently waiting on the object
        */
        [[nodiscard]] uint32_t get_current_waiting() const;

        CyclicBarrier() = delete;
        CyclicBarrier(const CyclicBarrier& rhs) = delete;
        CyclicBarrier& operator=(const CyclicBarrier& rhs) = delete;
    private:
        std::condition_variable cv;
        std::mutex mutex;
        uint32_t parties;
        uint32_t current_waits;
        std::function<void()> callback;
    };
}

#endif  //__CYCLICBARRIER_NIPUN__