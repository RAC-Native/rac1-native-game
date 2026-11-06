// Testing: RAC1_LOAD_PROF=1 prints the host's time of each step of a load ("[load] <what> <ms>"): a level's
// start (the host's copy of its files, the overlay, the game's level setup), a movie's open, the Vendor's stored picture.
// Host code: no game state.
#ifndef RAC1_RUNTIME_KERNEL_LOAD_PROF_H
#define RAC1_RUNTIME_KERNEL_LOAD_PROF_H

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#if defined(__WIIU__)
#include <malloc.h>
#endif

namespace rac1 {

inline bool load_prof_on() {
    static const bool on = std::getenv("RAC1_LOAD_PROF") != nullptr && std::atoi(std::getenv("RAC1_LOAD_PROF")) != 0;
    return on;
}

// orig: (host) the memory malloc holds, MB (the Wii U: its heap is the whole of the app's memory there; elsewhere 0)
inline unsigned host_mem_mb() {
#if defined(__WIIU__)
    const struct mallinfo mi = mallinfo();
    return static_cast<unsigned>(static_cast<uint32_t>(mi.uordblks) >> 20);
#else
    return 0;
#endif
}

// orig: (host) a step's wall time, printed at the end of its scope
struct LoadProf {
    const char* what;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    explicit LoadProf(const char* w) : what(w) {}
    double ms() const { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
    ~LoadProf() {
        if (!load_prof_on()) return;
#if defined(__WIIU__)
        std::printf("[load] %s %.1f ms (%u MB in use)\n", what, ms(), host_mem_mb());  // (the memory at the step's end)
#else
        std::printf("[load] %s %.1f ms\n", what, ms());
#endif
    }
};

}  // namespace rac1

#endif
