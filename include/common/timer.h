#ifndef SMARTCONTRACT_DRIVER_UTILS_TIMER_H_
#define SMARTCONTRACT_DRIVER_UTILS_TIMER_H_

#include <chrono>
#include <ctime>

namespace BlockBench {
    class Timer {
    public:
        Timer() :startTime{Clock::now()}, lastTime{startTime} {}

        void start() { startTime = Clock::now(); }

        double end() {
            Duration span;
            Clock::time_point t = Clock::now();
            span = std::chrono::duration_cast<Duration>(t - startTime);
            return span.count();
        }

        void tic() { lastTime = Clock::now(); }

        double toc() {
            Duration span;
            Clock::time_point t = Clock::now();
            span = std::chrono::duration_cast<Duration>(t - lastTime);
            lastTime = t;
            return span.count();
        }

        static inline void sleep(double t) {
            if (t < 0) {
                return;
            }
            timespec req{};
            req.tv_sec = static_cast<int>(t);
            req.tv_nsec = static_cast<int64_t>(1e9 * (t - (int64_t) t));
            nanosleep(&req, nullptr);
        }

        static inline time_t timeNow() {
            timespec ts{};
            clock_gettime(CLOCK_REALTIME, &ts);
            return (ts.tv_sec*1000000000 + ts.tv_nsec);
        }

    private:
        typedef std::chrono::high_resolution_clock Clock;
        typedef std::chrono::duration<double> Duration;

        Clock::time_point startTime;
        Clock::time_point lastTime;
    };
}
#endif
