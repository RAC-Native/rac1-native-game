// A value of each thread where `thread_local` is not to be had: the Wii U's executables take no thread-local storage
// (elf2rpl), and its build defines thread_local away. PerThread<T> keeps one T per OS thread in a small table, the
// entry found by the thread's OSThread (claimed with a compare-and-swap on first use: no lock). For the few values two
// threads must not share (the 989snd's random sequence, the IOP stream instance a thread works on: the game's thread and
// the IOP's mirror's). Elsewhere code keeps its thread_local.
#pragma once
#if defined(__WIIU__)
#include <coreinit/thread.h>

#include <atomic>
#include <cstdlib>

namespace rac1::platform {

template <class T, int N = 8>
class PerThread {
public:
    T& get() {
        void* const me = OSGetCurrentThread();
        for (Entry& e : e_)
            if (e.owner.load(std::memory_order_acquire) == me) return e.v;
        for (Entry& e : e_) {
            void* none = nullptr;
            if (e.owner.compare_exchange_strong(none, me, std::memory_order_acq_rel)) return e.v;
        }
        std::abort();  // (more threads than entries)
    }

private:
    struct Entry {
        std::atomic<void*> owner{nullptr};
        T v{};
    };
    Entry e_[N];
};

}  // namespace rac1::platform
#endif
