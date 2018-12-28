#pragma once

#include <chrono>
#include <mutex>
#include <system_error>

#include "except.h"

extern "C" {
#include <switch/kernel/condvar.h>
#include <switch/result.h>
}

namespace std {

enum class cv_status {
    no_timeout,
    timeout
};

class condition_variable {
public:
    condition_variable()
    { condvarInit(&handle_); }
    ~condition_variable() = default;
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    void notify_one() noexcept
    { condvarWakeOne(&handle_); }
    void notify_all() noexcept
    { condvarWakeAll(&handle_); }
    void wait(unique_lock<mutex>& lock) { 
        if (R_FAILED(condvarWait(&handle_, lock.mutex()->native_handle())))
            THROW(errc::resource_unavailable_try_again);
    }
    template <class Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template <class Clock, class Duration>
    cv_status wait_until(unique_lock<mutex>& lock, const chrono::time_point<Clock, Duration>& atime) {
        auto now = Clock::now();
        if (Clock::is_steady && now < atime)
            return wait_for(lock, atime - now);
        else
            while (now < atime) {
                if (wait_for(lock, atime - now) == cv_status::no_timeout)
                    return cv_status::no_timeout;
                now = Clock::now();
            }
        return cv_status::timeout;
    }
    template <class Clock, class Duration, class Predicate>
    bool wait_until(unique_lock<mutex>& lock, const chrono::time_point<Clock, Duration>& abs_time, Predicate pred) {
        while (!pred())
            if (wait_until(lock, abs_time) == std::cv_status::timeout)
                return pred();
        return true;
    }

    template <class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>& lock, const chrono::duration<Rep, Period>& rel_time)
    { return _handle_result(condvarWaitTimeout(&handle_, lock.mutex()->native_handle(), 
            chrono::duration_cast<chrono::nanoseconds>(rel_time).count())); }
    template <class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>& lock, const chrono::duration<Rep, Period>& rel_time, Predicate pred) 
    { return wait_until(lock, std::chrono::steady_clock::now() + rel_time, std::move(pred)); }

    typedef CondVar* native_handle_type;
    native_handle_type native_handle()
    { return &handle_; }

private:
    inline cv_status _handle_result(Result res) {
        if (R_SUCCEEDED(res))
            return cv_status::no_timeout;
        else if (res == 0xEA01)
            return cv_status::timeout;
        else
            THROW(errc::resource_unavailable_try_again);
    }

    CondVar handle_;
};

void notify_all_at_thread_exit(std::condition_variable& cond, std::unique_lock<std::mutex> lk);

}
