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

namespace rac1::game_thread {

namespace {
// orig: (host) the float environment a job runs with: the caller's (the EE's rounding toward zero; MXCSR's flush-to-zero and
// denormals-are-zero bits, which not every C library's fenv_t carries)
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

constexpr double kSpinMs = 4.0;  // (the game thread's poll for its next job, when the jobs come that close)
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
            cv.wait(lk, [this] { return has_job; });
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
thread_local bool t_game = false;
}  // namespace

// orig: (host) see game_thread.h
void run_async(std::function<void()> job) {
    Runner& r = g_run;
    std::unique_lock<std::mutex> lk(r.m);
    if (!r.started) {
        r.started = true;
        std::thread([&r] {
            t_game = true;
            r.loop();
        }).detach();  // (the process ends with std::_Exit: the thread is never joined)
    }
    {
        const Runner::Clock::time_point now = Runner::Clock::now();
        r.gap_ms = r.jobs != 0 ? std::chrono::duration<double, std::milli>(now - r.t_last_end).count() : 1e9;
    }
    r.fenv.take();
    r.job = std::move(job);
    r.error = nullptr;
    r.drawn = false;
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
    // (polled: the job's end or a request; the wait below then returns at once)
    while (r.a_busy.load(std::memory_order_acquire) && !r.a_request.load(std::memory_order_acquire)) spin_pause();
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
bool on_game_thread() { return t_game; }

// orig: (host) see game_thread.h
void wait_draw() {
    if (!t_game) return;
    Runner& r = g_run;
    std::unique_lock<std::mutex> lk(r.m);
    if (r.drawn) return;
    const int z = rac1::prof::begin(rac1::prof::kGameDrawWait);
    r.cv.wait(lk, [&r] { return r.drawn; });
    rac1::prof::end(z);
}

// orig: (host) see game_thread.h
void on_main(const std::function<void()>& fn) {
    if (!t_game) {
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

}  // namespace rac1::game_thread
