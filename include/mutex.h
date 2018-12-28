#pragma once

#include <utility>
#include <system_error>

extern "C" {
#include <switch/kernel/mutex.h>
}

#include "except.h"

namespace std
{
    class mutex {
    public:
        constexpr mutex() noexcept : handle_(0)
        { mutexInit(&handle_); /* safety first */ }
        ~mutex() = default;
        mutex(const mutex&) = delete;
        mutex& operator=(const mutex&) = delete;

        void lock()
        { mutexLock(&handle_); }
        bool try_lock()
        { return mutexTryLock(&handle_); }
        void unlock()
        { mutexUnlock(&handle_); }

        typedef Mutex* native_handle_type;
        native_handle_type native_handle()
        { return &handle_; }
    private:
        Mutex handle_;
    };

    class recursive_mutex {
    public:
        recursive_mutex()
        { rmutexInit(&handle_); }
        ~recursive_mutex() = default;
        recursive_mutex(const recursive_mutex&) = delete;
        recursive_mutex& operator=(const recursive_mutex&) = delete;

        void lock()
        { rmutexLock(&handle_); }
        bool try_lock() noexcept
        { return rmutexTryLock(&handle_); }
        void unlock()
        { return rmutexUnlock(&handle_); }

        typedef RMutex* native_handle_type;
        native_handle_type native_handle()
        { return &handle_; }
    private:
        RMutex handle_;
    };

    struct defer_lock_t { explicit defer_lock_t() = default; };
    constexpr defer_lock_t defer_lock { };
 
    struct try_to_lock_t { explicit try_to_lock_t() = default; };
    constexpr try_to_lock_t try_to_lock { };
 
    struct adopt_lock_t { explicit adopt_lock_t() = default; };
    constexpr adopt_lock_t adopt_lock { };

    template <class Mutex>
    class lock_guard {
    public:
        typedef Mutex mutex_type;
        explicit lock_guard(mutex_type& m) : pm(m)
        { m.lock(); }
        lock_guard(mutex_type& m, adopt_lock_t) : pm(m)
        {}
        ~lock_guard()
        { pm.unlock(); }
        lock_guard(lock_guard const&) = delete;
        lock_guard& operator=(lock_guard const&) = delete;
    private:
        mutex_type& pm;
    };

    template <class Mutex>
    class unique_lock {
    public:
        typedef Mutex mutex_type;

        unique_lock() noexcept : pm(nullptr), owns(false)
        {}
        explicit unique_lock(mutex_type& m) : pm(&m), owns(true)
        { pm->lock(); }
        unique_lock(mutex_type& m, defer_lock_t) noexcept : pm(&m), owns(false)
        {}
        unique_lock(mutex_type& m, try_to_lock_t) noexcept : pm(&m), owns(pm->try_lock())
        {}
        unique_lock(mutex_type& m, adopt_lock_t) noexcept : pm(&m), owns(true)
        {}
        ~unique_lock() {
            if (owns)
                pm->unlock();
        }
        unique_lock(unique_lock const&) = delete;
        unique_lock& operator=(unique_lock const&) = delete;
        unique_lock(unique_lock&& u) noexcept : pm(u.pm), owns(u.owns) {
            u.pm = nullptr;
            u.owns = false;
        }
        unique_lock& operator=(unique_lock&& u) noexcept { 
            if (owns)
                unlock();
            
            unique_lock(std::move(u)).swap(*this);
            u.pm = nullptr;
            u.owns = false;

            return *this;
        }

        // locking:
        void lock() {
            if (!pm)
                THROW(errc::operation_not_permitted);
            else if (owns)
                THROW(errc::resource_deadlock_would_occur);
            else {
                pm->lock();
                owns = true;
            }
        }
        bool try_lock() {
            if (!pm)
                THROW(errc::operation_not_permitted);
            else if (owns)
                THROW(errc::resource_deadlock_would_occur);
            else
                return (owns = pm->try_lock());
        }
        void unlock() {
            if (!owns)
                THROW(errc::operation_not_permitted);
            else if (pm) {
                pm->unlock();
                owns = false;
            }
        }

        void swap(unique_lock& u) noexcept {
            swap(pm, u.pm);
            swap(owns, u.owns);
        }
        mutex_type* release() noexcept {
            owns = false;
            auto mutex = pm;
            pm = nullptr;
            return mutex;
        }

        bool owns_lock() const noexcept
        { return owns; }
        explicit operator bool () const noexcept
        { return owns_lock(); }
        mutex_type* mutex() const noexcept
        { return pm; }

    private:
        mutex_type *pm;
        bool owns;
    };

    template<class Mutex>
    void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) noexcept
    { lhs.swap(rhs); }

    template <class MutexA>
    int _try_lock_impl(int idx, MutexA& first) 
    { return first.try_lock() ? -1 : idx; }
    // this implementation of try_lock never throws, so we'll can safely do this
    template <class MutexA, class... Mutexes>
    int _try_lock_impl(int idx, MutexA& first, Mutexes&... cde)
    { return first.try_lock() ? _try_lock_impl(idx, cde...) : idx; }

    template <class Mutex1, class Mutex2, class... Mutexes>
    int try_lock (Mutex1& a, Mutex2& b, Mutexes&... cde)
    { return _try_lock_impl(0, a, b, cde...); }

    template <class MutexA, class... Mutexes>
    int _lock_impl_helper(int idx, MutexA& first)
    { return _try_lock_impl(idx, first); }
    
    template <class MutexA, class... Mutexes>
    int _lock_impl_helper(int idx, MutexA& first, Mutexes&... cde) {
        if (first.try_lock()) {
            int res = _lock_impl_helper(idx + 1, cde...);
            if (res != -1)
                first.unlock();
            return res;
        } else
            return idx;
    }

    template <class Mutex1, class Mutex2, class... Mutexes>
    void lock(Mutex1& a, Mutex2& b, Mutexes&... cde) {
        // try and back off, not the most efficient algorithm, but it should do it for now
        while (true) {
            lock_guard<Mutex1> first(a);
            if (_lock_impl_helper(0, b, cde...) == -1)
                return;
        }
    }
}
