// Standalone: no disc, generated code or game runtime needed.
#include "trial_guard.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
volatile uint32_t* protected_page;
uint32_t seen = 0;
void healthy(uint32_t value) { seen = value; }
void throwing(uint32_t) { throw std::runtime_error("invalid random state"); }
void faulting(uint32_t) { *protected_page = 1; }
void spinning(uint32_t) {
    volatile uint32_t value = 0;
    for (;;) value = value + 1;
}
}

int main(int argc, char** argv) {
    using rac1::oracle::run_trial;
    if (argc == 2 && std::strcmp(argv[1], "timeout") == 0) {
        if (run_trial(spinning, 0, 1)) return 1;
        if (!run_trial(healthy, 42) || seen != 42) return 1;
        std::puts("timeout rejected; subsequent trial passed");
        return 0;
    }
#ifdef _WIN32
    protected_page = static_cast<volatile uint32_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS));
    if (!protected_page) return 2;
#else
    const auto size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void* page = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) return 2;
    protected_page = static_cast<volatile uint32_t*>(page);
#endif
    for (int i = 0; i < 16; ++i) {
        if (!run_trial(healthy, 42, 1) || seen != 42) return 1;
        if (run_trial(throwing, 0, 1)) return 1;
        if (run_trial(faulting, 0, 1)) return 1;
        if (!run_trial(healthy, 43) || seen != 43) return 1;
    }
#ifdef _WIN32
    VirtualFree(const_cast<uint32_t*>(protected_page), 0, MEM_RELEASE);
#else
    munmap(page, size);
#endif
    // A completed/faulted/throwing trial must not leave a timer armed.
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    std::puts("normal, C++ exception, repeated access fault, recovery and timer cancellation passed");
    return 0;
}
