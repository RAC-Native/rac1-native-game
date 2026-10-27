// src/jobs/job_pool.cpp — the host's job pool (openly invented tooling, ADR 0003): see job_pool.h.
#include "jobs/job_pool.h"

#include "profiler/host_profiler.h"

#include <algorithm>
#include <atomic>
#include <cfenv>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#define RAC1_JOBS_MXCSR 1
#endif

namespace rac1::jobs {

namespace {

// The calling thread's float environment, carried to the workers: the rounding mode, and on x86 the whole MXCSR (its
// flush-to-zero and denormals-are-zero bits are not in every C library's fenv_t)
// (on 32-bit PowerPC, the whole FPSCR: its NI bit, the non-IEEE mode, is not in newlib's fenv)
#if (defined(__PPC__) || defined(__powerpc__)) && !defined(__powerpc64__)
struct FloatEnv {
    double fpscr = 0.0;
    void capture() { __asm__ volatile("mffs %0" : "=f"(fpscr)); }
    void apply() const { __asm__ volatile("mtfsf 0xff, %0" : : "f"(fpscr)); }
};
#else
struct FloatEnv {
    std::fenv_t env{};
#ifdef RAC1_JOBS_MXCSR
    unsigned csr = 0;
#endif
    void capture() {
        std::fegetenv(&env);
#ifdef RAC1_JOBS_MXCSR
        csr = _mm_getcsr();
#endif
    }
    void apply() const {
        std::fesetenv(&env);
#ifdef RAC1_JOBS_MXCSR
        _mm_setcsr(csr);
#endif
    }
};
#endif

// A job's chunks are claimed through one 64-bit ticket: the job's number in the high half, the next chunk in the low. A
// worker claims only chunks of the job it took (a compare-and-swap on the whole ticket), so a worker waking late, after its
// job is over, never runs a chunk of the next one with the old job's data.
struct Job {
    const std::function<void(size_t, size_t)>* fn = nullptr;
    size_t n = 0, grain = 1, nchunks = 0;
    uint32_t gen = 0;
    FloatEnv fenv;
};

struct Pool {
    std::mutex m;
    std::condition_variable cv_work, cv_done;
    std::vector<std::thread> threads;
    bool quit = false;
    Job job;                          // (written by the caller under m; copied by each worker under m when it takes the job)
    std::atomic<uint64_t> ticket{0};  // (job.gen << 32) | the next chunk
    std::atomic<size_t> done{0};      // the job's chunks finished
    int active = 0;                   // workers that took the current job and have not left it (under m)
    std::exception_ptr error;         // (under m)

    // claims and runs the chunks of job `j` until none is left; false when it ran none
    void run_chunks(const Job& j) {
        uint64_t t = ticket.load(std::memory_order_acquire);
        for (;;) {
            if (static_cast<uint32_t>(t >> 32) != j.gen) return;
            const size_t c = static_cast<size_t>(t & 0xffffffffu);
            if (c >= j.nchunks) return;
            if (!ticket.compare_exchange_weak(t, t + 1, std::memory_order_acq_rel, std::memory_order_acquire)) continue;
            const size_t b = c * j.grain, e = std::min(j.n, b + j.grain);
            try {
                (*j.fn)(b, e);
            } catch (...) {
                std::lock_guard<std::mutex> lk(m);
                if (!error) error = std::current_exception();
            }
            done.fetch_add(1, std::memory_order_acq_rel);
            t = ticket.load(std::memory_order_acquire);
        }
    }

    void worker(int lane) {
        char name[32];
        std::snprintf(name, sizeof name, "job worker %d", lane + 1);
        prof::thread_name(name);
        uint32_t seen = 0;
        std::unique_lock<std::mutex> lk(m);
        for (;;) {
            cv_work.wait(lk, [&] { return quit || job.gen != seen; });
            if (quit) return;
            seen = job.gen;
            const Job j = job;
            ++active;
            lk.unlock();
            j.fenv.apply();
            {
                const prof::Job pj(prof::kWorkerJobs, lane);  // (the debug menu's profiler: the workers' time, summed)
                run_chunks(j);
            }
            lk.lock();
            if (--active == 0) cv_done.notify_all();
        }
    }

    ~Pool() {
        {
            std::lock_guard<std::mutex> lk(m);
            quit = true;
        }
        cv_work.notify_all();
        for (std::thread& t : threads)
            if (t.joinable()) t.join();
    }
};

int env_threads() {
    static const int n = [] {
        if (const char* e = std::getenv("RAC1_JOBS"); e != nullptr && *e != 0) return std::clamp(std::atoi(e), 1, 64);
        const int hw = static_cast<int>(std::thread::hardware_concurrency());
        // about the physical cores (two logical CPUs each), the renderer's two workers left their own, at most 8
        return std::clamp(hw / 2 - 2, 1, 8);
    }();
    return n;
}

Pool& pool() {
    static Pool p;
    static std::once_flag started;
    std::call_once(started, [] {
        const int workers = env_threads() - 1;
        for (int i = 0; i < workers; ++i) p.threads.emplace_back([i] { p.worker(i); });
    });
    return p;
}

}  // namespace

int threads() { return env_threads(); }

void parallel_for(size_t n, size_t grain, const std::function<void(size_t, size_t)>& fn) {
    if (n == 0) return;
    if (grain == 0) grain = 1;
    const size_t nchunks = chunks(n, grain);
    if (nchunks <= 1 || env_threads() <= 1) {  // (the same chunks, in order, on the caller)
        for (size_t b = 0; b < n; b += grain) fn(b, std::min(n, b + grain));
        return;
    }
    // (one job at a time: the pool has one job slot. The game thread's scene decoding and the main thread's moby draw lists
    // both use the pool, and with the game's update beside the drawing they could ask at once; the second waits)
    static std::mutex one_caller;
    std::lock_guard<std::mutex> caller(one_caller);
    Pool& P = pool();
    Job j;
    {
        std::lock_guard<std::mutex> lk(P.m);
        P.job.fn = &fn;
        P.job.n = n;
        P.job.grain = grain;
        P.job.nchunks = nchunks;
        P.job.gen = P.job.gen + 1 == 0 ? 1 : P.job.gen + 1;  // (0: no job yet)
        P.job.fenv.capture();
        P.done.store(0, std::memory_order_relaxed);
        P.error = nullptr;
        P.ticket.store(static_cast<uint64_t>(P.job.gen) << 32, std::memory_order_release);
        j = P.job;
    }
    P.cv_work.notify_all();
    P.run_chunks(j);
    std::exception_ptr error;
    {
        std::unique_lock<std::mutex> lk(P.m);
        P.cv_done.wait(lk, [&] { return P.done.load(std::memory_order_acquire) == nchunks && P.active == 0; });
        error = P.error;
        P.error = nullptr;
    }
    if (error) std::rethrow_exception(error);
}

}  // namespace rac1::jobs
