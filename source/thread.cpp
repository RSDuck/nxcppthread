#include "thread.h"

#include <switch.h>
#include <condition_variable>

#include <vector>
#include <map>

#include "except.h"

#define THREADVARS_MAGIC 0x21545624 // !TV$

// This structure is exactly 0x20 bytes, if more is needed modify getThreadVars() below
typedef struct {
    // Magic value used to check if the struct is initialized
    u32 magic;

    // Thread handle, for mutexes
    Handle handle;

    // Pointer to the current thread (if exists)
    Thread* thread_ptr;

    // Pointer to this thread's newlib state
    struct _reent* reent;

    // Pointer to this thread's thread-local segment
    void* tls_tp; // !! Offset needs to be TLS+0x1F8 for __aarch64_read_tp !!
} ThreadVars;

static inline ThreadVars* getThreadVars(void) {
    return (ThreadVars*)((u8*)armGetTls() + 0x1E0);
}

static bool at_exit_registered = false;
static std::vector<Thread*> detached_threads;
static int next_core = 0;

static std::map<std::thread::id, 
    std::vector<std::pair<std::condition_variable&, std::unique_lock<std::mutex>>>> condvars_to_notify;

static void thread_entry(void* params) { 
    auto func = static_cast<std::function<void()>*>(params);
    (*func)();
    delete func;
    for (auto& pair : condvars_to_notify[std::this_thread::get_id()]) {
        pair.second.unlock();
    }
}

static void free_detached_threads()
{
    for (auto thread : detached_threads) {
        threadPause(thread);
        threadClose(thread);
    }
}

namespace std
{

void thread::_create_thread(std::function<void()>* func) {
    u32 prio;
    if (R_SUCCEEDED(svcGetThreadPriority(&prio, CUR_THREAD_HANDLE)))
    {
        auto t = new Thread;
        if (R_SUCCEEDED(threadCreate(t, thread_entry, (void*)func, 256*1024, prio-1, next_core++ % 3))) {
            if (R_SUCCEEDED(threadStart(t))) {
                id_ = id(t);
                return;
            }

            threadClose(t);
        }
        delete t;
    }

    THROW(errc::resource_unavailable_try_again);
}

void thread::join() {
    if (joinable()) {
        if (R_SUCCEEDED(threadWaitForExit(id_.handle_))) {
            threadClose(id_.handle_);

            delete id_.handle_;
            id_ = id();
            
            return;
        }

        THROW(errc::resource_unavailable_try_again);
    }
}

void thread::detach() {
    detached_threads.push_back(id_.handle_);
    id_ = id();

    if (!at_exit_registered)
    {
        atexit(free_detached_threads);
        at_exit_registered = true;
    }
}

namespace this_thread {

thread::id get_id() noexcept
{
    auto threadVars = getThreadVars();
    //assert(threadVars->magic == THREADVARS_MAGIC); 
    return thread::id(threadVars->thread_ptr); 
}

void yield() noexcept
{ svcSleepThread(0); }

void _sleep_for(u64 nanosecs) noexcept
{ svcSleepThread(nanosecs); }

}

void notify_all_at_thread_exit(condition_variable& cond, unique_lock<std::mutex> lk) {
    condvars_to_notify[this_thread::get_id()].push_back(
            make_pair(std::ref(cond), std::move(lk)));
}

}