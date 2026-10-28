// The game's update on a thread of its own: see game_thread.h.
#include "game_thread.h"

#include <profiler/host_profiler.h>

#include <atomic>
#include <chrono>
#include <cfenv>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <mutex>
#include <thread>
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <xmmintrin.h>
#define RAC1_GT_SSE 1
#endif
#if defined(__WIIU__)
#include <coreinit/core.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>

#include <algorithm>
#include <cstdlib>
#endif

namespace rac1::game_thread {

namespace {
// orig: (host) the float environment a job runs with: the caller's (the EE's rounding toward zero; MXCSR's flush-to-zero and
// denormals-are-zero bits, which not every C library's fenv_t carries)
#if (defined(__PPC__) || defined(__powerpc__)) && !defined(__powerpc64__)
// (32-bit PowerPC, the Wii U: the whole FPSCR - its NI bit, the non-IEEE mode, is not in newlib's fenv -, as job_pool.cpp's)
struct FloatEnv {
    double fpscr = 0.0;
    void take() { __asm__ volatile("mffs %0" : "=f"(fpscr)); }
    void apply() const { __asm__ volatile("mtfsf 0xff, %0" : : "f"(fpscr)); }
};
#else
struct FloatEnv {
    std::fenv_t env{};
#ifdef RAC1_GT_SSE
    unsigned csr = 0;
#endif
    void take() {
        std::fegetenv(&env);
#ifdef RAC1_GT_SSE
        csr = _mm_getcsr();
#endif
    }
    void apply() const {
        std::fesetenv(&env);
#ifdef RAC1_GT_SSE
        _mm_setcsr(csr);
#endif
    }
};
#endif

#if defined(__WIIU__)
// (the Wii U: no polling - a core's scheduler wakes a waiting thread at once, and a poll there would take the core from the
// others it runs: the IOP's mirror on core 2)
constexpr double kSpinMs = 0.0;
#else
constexpr double kSpinMs = 4.0;  // (the game thread's poll for its next job, when the jobs come that close)
#endif
inline void spin_pause() {
#ifdef RAC1_GT_SSE
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}

struct Runner {
    std::mutex m;
    std::condition_variable cv;
    std::thread th;
    std::function<void()> job;
    FloatEnv fenv;
    bool started = false;
    bool has_job = false, busy = false, drawn = true;
    bool quit = false;  // (stop: the loop left)
    // (the hand-offs: the waiting side polls these a while before it sleeps - a sleeping thread's wake-up took milliseconds on a
    // loaded machine, the whole gain of the overlap. The main thread polls for the job's end (it would wait anyway); the game thread
    // polls for its next job only as long as the last gap between jobs was short: a host frame bound by the CPU, not paced)
    std::atomic<bool> a_job{false}, a_busy{false}, a_request{false};
    double gap_ms = 1e9;
    const std::function<void()>* request = nullptr;  // (an on_main call waiting for the main thread)
    bool request_done = false;
    std::exception_ptr request_error;  // (thrown by the request: rethrown on the game thread)
    std::exception_ptr error;
    std::atomic<bool> running{false};
    // (the report: the jobs, the time from run_async to the job's start, the job's own time, its end to finish's return)
    using Clock = std::chrono::steady_clock;
    Clock::time_point t_post{}, t_end{}, t_last_end{};
    double ms_start = 0, ms_job = 0, ms_join = 0;
    uint64_t jobs = 0;
    void loop() {
        rac1::prof::thread_name("Game thread (EE update)");
        rac1::prof::game_thread_attach();
        std::unique_lock<std::mutex> lk(m);
        for (;;) {
            if (!has_job && gap_ms < kSpinMs) {  // (the next job is likely soon: polled, then slept for)
                lk.unlock();
                const Clock::time_point t0 = Clock::now();
                while (!a_job.load(std::memory_order_acquire) && Clock::now() - t0 < std::chrono::duration<double, std::milli>(kSpinMs)) spin_pause();
                lk.lock();
            }
            cv.wait(lk, [this] { return has_job || quit; });
            if (!has_job) return;  // (stop)
            a_job.store(false, std::memory_order_relaxed);
            has_job = false;
            std::function<void()> fn = std::move(job);
            const Clock::time_point t_start = Clock::now();
            ms_start += std::chrono::duration<double, std::milli>(t_start - t_post).count();
            lk.unlock();
            fenv.apply();
            {
                const rac1::prof::Job prof_job(rac1::prof::kWorkerGame);
                try {
                    fn();
                } catch (...) {
                    error = std::current_exception();
                }
            }
            lk.lock();
            t_end = Clock::now();
            t_last_end = t_end;
            ms_job += std::chrono::duration<double, std::milli>(t_end - t_start).count();
            ++jobs;
            busy = false;
            running.store(false, std::memory_order_release);
            a_busy.store(false, std::memory_order_release);
            cv.notify_all();
        }
    }
};
Runner g_run;
#if defined(__WIIU__)
// The Wii U: the game thread an OS thread of its own (std::thread's stack is wut's 128 KB, on any core), and no thread_local there
// (platform/per_thread.h): the game thread told by its OSThread. Its core RAC1_WIIU_GAME_CORE (default 2: the IOP's mirror there is
// light, core 0 runs the chain's reader and the world's preparation worker - beside the update -, core 1 the main thread, GX2's);
// its priority RAC1_WIIU_GAME_PRIORITY (default 25: below the streams' I/O thread, 24, and the IOP's mirror, 10, on core 2 - its
// steps, most of a host frame, would keep the reads of the music waiting); a 1 MB stack (the game's frames run on the EE coroutine's
// own stack, ee_thread.cpp; this one holds the host's work around them, as the main thread's does)
OSThread g_os_thread __attribute__((aligned(16)));
void* g_os_stack = nullptr;
constexpr uint32_t kOsStack = 1u << 20;
bool on_game_os_thread() { return OSGetCurrentThread() == &g_os_thread; }  // (never a live thread's address before it is made)
int os_thread_main(int, const char**) {
    std::printf("[game-thread] the game thread on core %u, priority %d\n", OSGetCoreId(), OSGetThreadPriority(OSGetCurrentThread()));
    g_run.loop();
    return 0;
}
// orig: (host) the game thread started (false: no stack, no thread - the job then runs on the caller's thread)
bool os_thread_start() {
    const char* ce = std::getenv("RAC1_WIIU_GAME_CORE");
    const int core = ce != nullptr ? std::atoi(ce) : 2;
    const char* pe = std::getenv("RAC1_WIIU_GAME_PRIORITY");
    const int prio = pe != nullptr ? std::clamp(std::atoi(pe), 0, 31) : 25;
    g_os_stack = MEMAllocFromDefaultHeapEx(kOsStack, 16);
    if (g_os_stack == nullptr) return false;
    const auto aff = static_cast<OSThreadAttributes>(core >= 0 && core <= 2 ? OS_THREAD_ATTRIB_AFFINITY_CPU0 << core : OS_THREAD_ATTRIB_AFFINITY_ANY);
    if (!OSCreateThread(&g_os_thread, os_thread_main, 0, nullptr, static_cast<uint8_t*>(g_os_stack) + kOsStack, kOsStack, prio, aff)) {
        MEMFreeToDefaultHeap(g_os_stack);
        g_os_stack = nullptr;
        return false;
    }
    OSSetThreadName(&g_os_thread, "rac1 game thread");
    OSResumeThread(&g_os_thread);
    return true;
}
#else
thread_local bool t_game = false;
#endif
}  // namespace

// orig: (host) see game_thread.h
void run_async(std::function<void()> job, bool draw_reads_gs) {
    Runner& r = g_run;
    std::unique_lock<std::mutex> lk(r.m);
    if (!r.started) {
#if defined(__WIIU__)
        if (!os_thread_start()) {  // (no thread: the job here, at once)
            lk.unlock();
            std::printf("[game-thread] not started: the game frames on the main thread\n");
            job();
            return;
        }
        r.started = true;
#else
        r.started = true;
        std::thread([&r] {
            t_game = true;
            r.loop();
        }).detach();  // (the process ends with std::_Exit: the thread is never joined)
#endif
    }
    {
        const Runner::Clock::time_point now = Runner::Clock::now();
        r.gap_ms = r.jobs != 0 ? std::chrono::duration<double, std::milli>(now - r.t_last_end).count() : 1e9;
    }
    r.fenv.take();
    r.job = std::move(job);
    r.error = nullptr;
    r.drawn = !draw_reads_gs;
    r.busy = true;
    r.running.store(true, std::memory_order_release);
    r.has_job = true;
    r.a_busy.store(true, std::memory_order_relaxed);
    r.a_job.store(true, std::memory_order_release);
    r.t_post = Runner::Clock::now();
    r.cv.notify_all();
}

// orig: (host) see game_thread.h
void finish() {
    Runner& r = g_run;
    {
        std::lock_guard<std::mutex> lg(r.m);
        r.drawn = true;
    }
    r.cv.notify_all();
#if !defined(__WIIU__)
    // (polled: the job's end or a request; the wait below then returns at once)
    while (r.a_busy.load(std::memory_order_acquire) && !r.a_request.load(std::memory_order_acquire)) spin_pause();
#endif
    std::unique_lock<std::mutex> lk(r.m);
    for (;;) {
        r.cv.wait(lk, [&r] { return !r.busy || (r.request != nullptr && !r.request_done); });
        if (r.request != nullptr && !r.request_done) {
            const std::function<void()>* fn = r.request;
            lk.unlock();
            std::exception_ptr e;
            try {
                (*fn)();
            } catch (...) {
                e = std::current_exception();
            }
            lk.lock();
            r.request_error = e;
            r.request_done = true;
            r.a_request.store(false, std::memory_order_relaxed);
            r.cv.notify_all();
            continue;
        }
        break;
    }
    if (r.jobs != 0 && r.t_end.time_since_epoch().count() != 0) r.ms_join += std::chrono::duration<double, std::milli>(Runner::Clock::now() - r.t_end).count();
    r.t_end = {};
    if (r.error) {
        std::exception_ptr e = r.error;
        r.error = nullptr;
        std::rethrow_exception(e);
    }
}

// orig: (host) see game_thread.h
void report() {
    Runner& r = g_run;
    std::lock_guard<std::mutex> lk(r.m);
    if (r.jobs == 0) return;
    const double n = static_cast<double>(r.jobs);
    std::printf("[game-thread] %llu jobs: %.3f ms from the start asked to the job's start, %.3f ms the job, %.3f ms from its end to the join (means)\n",
                static_cast<unsigned long long>(r.jobs), r.ms_start / n, r.ms_job / n, r.ms_join / n);
}

// orig: (host) see game_thread.h
bool running() { return g_run.running.load(std::memory_order_acquire); }

// orig: (host) see game_thread.h
#if defined(__WIIU__)
bool on_game_thread() { return on_game_os_thread(); }
#else
bool on_game_thread() { return t_game; }
#endif

// orig: (host) see game_thread.h
void wait_draw() {
    if (!on_game_thread()) return;
    Runner& r = g_run;
    std::unique_lock<std::mutex> lk(r.m);
    if (r.drawn) return;
    const int z = rac1::prof::begin(rac1::prof::kGameDrawWait);
    r.cv.wait(lk, [&r] { return r.drawn; });
    rac1::prof::end(z);
}

// orig: (host) see game_thread.h
void on_main(const std::function<void()>& fn) {
    if (!on_game_thread()) {
        fn();
        return;
    }
    Runner& r = g_run;
    std::unique_lock<std::mutex> lk(r.m);
    r.request = &fn;
    r.request_done = false;
    r.a_request.store(true, std::memory_order_release);
    r.cv.notify_all();
    r.cv.wait(lk, [&r] { return r.request_done; });
    r.request = nullptr;
    r.request_done = false;
    if (r.request_error) {
        std::exception_ptr e = r.request_error;
        r.request_error = nullptr;
        lk.unlock();
        std::rethrow_exception(e);
    }
}

// orig: (host) see game_thread.h
void stop() {
#if defined(__WIIU__)
    Runner& r = g_run;
    {
        std::unique_lock<std::mutex> lk(r.m);
        if (!r.started) return;
        r.cv.wait(lk, [&r] { return !r.busy; });  // (a job not finished: waited for - its requests are not run any more)
        r.quit = true;
    }
    r.cv.notify_all();
    int rc = 0;
    OSJoinThread(&g_os_thread, &rc);
    std::lock_guard<std::mutex> lk(r.m);
    r.started = false;
    std::printf("[game-thread] the game thread ended\n");
#endif
}

}  // namespace rac1::game_thread
