// A worker thread pinned to one core (the Wii U), std::thread elsewhere.
//
// (2026-10-29) On the Wii U a std::thread is wut's: created on any core, and OSSetThreadAffinity called on itself afterwards did not
// keep it there - the console's sampling profile (Novalis) had the chain reader and the world's preparation worker, both "on core 0",
// running 48% on core 0, ~33% on core 2 (the game thread's, at a lower priority: they took its time) and ~18% on core 1. An OSThread
// created with its affinity stays on its core (the game thread's, the IOP mirror's).
#pragma once

#include <functional>
#include <thread>
#include <utility>

#if defined(__WIIU__)
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>

#include <cstdint>
#include <cstdio>
#endif

namespace rac1 {

#if defined(__WIIU__)
class PinnedThread {
public:
    PinnedThread() = default;
    PinnedThread(const PinnedThread&) = delete;
    PinnedThread& operator=(const PinnedThread&) = delete;
    ~PinnedThread() {
        if (joinable()) join();
    }
    // `fn` started on an OS thread on `core` (0..2; anything else: any core), at OS priority `prio` (0 highest .. 31), with `stack`
    // bytes of stack; false: not started (no memory, or already running)
    bool start(int core, int prio, const char* name, std::function<void()> fn, uint32_t stack = 256u * 1024u) {
        if (st_ != nullptr) return false;
        st_ = static_cast<State*>(MEMAllocFromDefaultHeapEx(sizeof(State), 16));
        if (st_ == nullptr) return false;
        new (st_) State();
        st_->fn = std::move(fn);
        st_->stack = MEMAllocFromDefaultHeapEx(stack, 16);
        if (st_->stack == nullptr) {
            free_state();
            return false;
        }
        const auto aff = static_cast<OSThreadAttributes>(core >= 0 && core <= 2 ? OS_THREAD_ATTRIB_AFFINITY_CPU0 << core : OS_THREAD_ATTRIB_AFFINITY_ANY);
        if (!OSCreateThread(&st_->th, entry, 0, reinterpret_cast<char*>(st_), static_cast<uint8_t*>(st_->stack) + stack, stack, prio, aff)) {
            free_state();
            return false;
        }
        OSSetThreadName(&st_->th, name);
        OSResumeThread(&st_->th);
        return true;
    }
    bool joinable() const { return st_ != nullptr; }
    void join() {
        if (st_ == nullptr) return;
        int rc = 0;
        OSJoinThread(&st_->th, &rc);
        free_state();
    }

private:
    struct State {
        OSThread th __attribute__((aligned(16)));
        void* stack = nullptr;
        std::function<void()> fn;
    };
    static int entry(int, const char** argv) {
        reinterpret_cast<State*>(argv)->fn();
        return 0;
    }
    void free_state() {
        if (st_->stack != nullptr) MEMFreeToDefaultHeap(st_->stack);
        st_->~State();
        MEMFreeToDefaultHeap(st_);
        st_ = nullptr;
    }
    State* st_ = nullptr;
};
#else
class PinnedThread {
public:
    bool start(int, int, const char*, std::function<void()> fn, unsigned = 0) {
        if (th_.joinable()) return false;
        th_ = std::thread(std::move(fn));
        return true;
    }
    bool joinable() const { return th_.joinable(); }
    void join() { th_.join(); }
    ~PinnedThread() {
        if (th_.joinable()) th_.join();
    }

private:
    std::thread th_;
};
#endif

}  // namespace rac1
