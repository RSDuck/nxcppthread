#pragma once

#include <iostream>
#include <functional>
#include <tuple>
#include <chrono>
#include <cassert>

extern "C" {
#include <switch/kernel/thread.h>
}

namespace std {

class thread {
public:
    typedef Thread* native_handle_type;

    class id {
    public:
        id() noexcept : handle_(nullptr) {}
        explicit id(native_handle_type handle) noexcept : handle_(handle) {}
        
        native_handle_type handle_;
    private:

        friend class thread;
        friend class hash<thread::id>;

        friend bool operator==(thread::id lhs, thread::id rhs) noexcept;
        friend bool operator<(thread::id lhs, thread::id rhs) noexcept;

        template<class _CharT, class _Traits>
        friend basic_ostream<_CharT, _Traits>& operator<<(basic_ostream<_CharT, _Traits>& __out, thread::id __id);
    };

    thread() noexcept = default;
    thread(thread&& other) noexcept
    { swap(other); }
    template<class Function, class... Args>
    explicit thread(Function&& f, Args&&... args) {
        auto func = new std::function<void()>(
            std::bind(std::forward<Function>(f), 
                std::forward<Args>(args)...));

        _create_thread(func);
    }

    thread(thread&) = delete;
    thread(const thread&) = delete;
    thread(const thread&&) = delete;

    ~thread() {
        if (joinable())
            std::terminate();
    }

    thread& operator=(const thread&) = delete;
    thread& operator=(thread&& other) noexcept
    {
        if (joinable())
            std::terminate();
        swap(other);
        return *this;
    }

    bool joinable() const noexcept
    { return !(id_ == id()); }

    id get_id() const noexcept
    { return id_; }
    native_handle_type native_handle()
    { return id_.handle_; }

    void join();
    void detach();

    void swap(thread& other) noexcept
    { std::swap(id_, other.id_); }

    static unsigned int hardware_concurrency() noexcept
    { return 4; }

private:
    void _create_thread(std::function<void()>* func);

    id id_;
};

inline void swap(thread &lhs, thread &rhs) noexcept
{ lhs.swap(rhs); }

inline bool operator==(thread::id lhs, thread::id rhs) noexcept 
{ return lhs.handle_ == rhs.handle_; }
inline bool operator<(thread::id lhs, thread::id rhs) noexcept 
{ return lhs.handle_ < rhs.handle_; }
inline bool operator!=(thread::id lhs, thread::id rhs) noexcept 
{ return !(lhs == rhs); }
inline bool operator>(thread::id lhs, thread::id rhs) noexcept 
{ return rhs < lhs; }
inline bool operator>=(thread::id lhs, thread::id rhs) noexcept 
{ return !(lhs < rhs); }
inline bool operator<=(thread::id lhs, thread::id rhs) noexcept 
{ return !(rhs > lhs); }

template<>
struct hash<thread::id> : public __hash_base<size_t, thread::id>
{
    size_t operator()(const thread::id& __id) const noexcept
    { return std::_Hash_impl::hash(__id.handle_); }
};
template<class _CharT, class _Traits>
inline basic_ostream<_CharT, _Traits>& operator<<(basic_ostream<_CharT, _Traits>& __out, thread::id __id) {
    if (__id == thread::id())
	    return __out << "thread::id of a non-executing thread";
    else
	    return __out << __id.handle_;
}

namespace this_thread
{

thread::id get_id() noexcept;
void yield() noexcept;

void _sleep_for(u64 nanosecs) noexcept;
template<class Rep, class Period>
void sleep_for(const std::chrono::duration<Rep, Period>& sleep_duration) {
    auto span = chrono::duration_cast<chrono::nanoseconds>(sleep_duration);
    _sleep_for(span.count());
}

template<typename _Clock, typename _Duration>
inline void sleep_until(const chrono::time_point<_Clock, _Duration>& atime) {
    auto now = _Clock::now();
	if (_Clock::is_steady) {
        if (now < atime)
            sleep_for(atime - now);
        return;
	}
	while (now < atime) {
	    sleep_for(atime - now);
	    now = _Clock::now();
	}
}

}

}