// The GPU's clocks as the system holds them, in the log: see host_gpu_watch.h. Host code.
#include "host_gpu_watch.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace rac1 {

namespace {

// NVML's types and calls, as its header (nvml.h) declares them: loaded at run time, no SDK needed
using nvmlDevice_t = struct nvmlDevice_st*;
struct nvmlUtilization_t {
    unsigned int gpu, memory;
};
constexpr int kClockGraphics = 0, kClockMem = 2;  // (NVML_CLOCK_GRAPHICS, NVML_CLOCK_MEM)
// the reasons' bits (nvmlClocksEventReasons, formerly ClocksThrottleReasons)
constexpr unsigned long long kSwPowerCap = 0x4, kHwSlowdown = 0x8, kSwThermal = 0x20, kHwThermal = 0x40, kHwPowerBrake = 0x80;

struct Nvml {
    int (*init)() = nullptr;
    int (*shutdown)() = nullptr;
    int (*handle)(unsigned int, nvmlDevice_t*) = nullptr;
    int (*name)(nvmlDevice_t, char*, unsigned int) = nullptr;
    int (*pstate)(nvmlDevice_t, int*) = nullptr;
    int (*clock)(nvmlDevice_t, int, unsigned int*) = nullptr;
    int (*max_clock)(nvmlDevice_t, int, unsigned int*) = nullptr;
    int (*power)(nvmlDevice_t, unsigned int*) = nullptr;
    int (*limit)(nvmlDevice_t, unsigned int*) = nullptr;
    int (*default_limit)(nvmlDevice_t, unsigned int*) = nullptr;
    int (*reasons)(nvmlDevice_t, unsigned long long*) = nullptr;
    int (*util)(nvmlDevice_t, nvmlUtilization_t*) = nullptr;
};

std::thread g_thread;
std::mutex g_mutex;
std::condition_variable g_cv;
bool g_stop = false;

template <typename F>
void get(void* lib, F& f, const char* n) {
#ifdef _WIN32
    f = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), n)));
#else
    f = reinterpret_cast<F>(dlsym(lib, n));
#endif
}

std::string reasons_text(unsigned long long r) {
    std::string s;
    auto add = [&](const char* t) {
        if (!s.empty()) s += ", ";
        s += t;
    };
    if (r & kSwPowerCap) add("power cap");
    if (r & kSwThermal) add("thermal (software)");
    if (r & (kHwThermal | kHwSlowdown)) add("thermal (hardware)");
    if (r & kHwPowerBrake) add("power brake (the charger)");
    return s.empty() ? "none" : s;
}

// orig: (host) the watch's thread: a sample every 0.5 s, a line for every 10 s that the GPU was held
void run(Nvml n, nvmlDevice_t dev, bool always) {
    unsigned int max_gr = 0, max_mem = 0, def = 0, lim = 0;
    n.max_clock(dev, kClockGraphics, &max_gr);
    n.max_clock(dev, kClockMem, &max_mem);
    if (n.default_limit != nullptr) n.default_limit(dev, &def);
    if (n.limit != nullptr) n.limit(dev, &lim);
    char name[96] = "?";
    if (n.name != nullptr) n.name(dev, name, sizeof name);
    std::printf("[gpu] %s: clocks up to %u MHz (graphics), %u MHz (memory); power limit %.0f W (the driver's default %.0f W)\n", name, max_gr, max_mem,
                lim / 1000.0, def / 1000.0);
    std::fflush(stdout);
    int samples = 0, busy = 0, held = 0, worst_p = 0;
    double gr = 0, mem = 0, watts = 0;
    unsigned long long why = 0;
    unsigned int lim_min = lim;
    auto t0 = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lk(g_mutex);
    while (!g_cv.wait_for(lk, std::chrono::milliseconds(500), [] { return g_stop; })) {
        int p = 0;
        unsigned int c_gr = 0, c_mem = 0, mw = 0, l = 0;
        unsigned long long r = 0;
        nvmlUtilization_t u{};
        n.pstate(dev, &p);
        n.clock(dev, kClockGraphics, &c_gr);
        n.clock(dev, kClockMem, &c_mem);
        if (n.power != nullptr) n.power(dev, &mw);
        if (n.limit != nullptr && n.limit(dev, &l) == 0 && l != 0) lim_min = lim_min == 0 ? l : std::min(lim_min, l);
        if (n.reasons != nullptr) n.reasons(dev, &r);
        n.util(dev, &u);
        ++samples;
        if (u.gpu >= 80) {  // (the game keeping it busy: a GPU idle between frames is lowered by the driver, rightly)
            ++busy;
            gr += c_gr, mem += c_mem, watts += mw / 1000.0;
            // held: a performance state under P2 (P0 / P2: the full clocks under load), the memory at under half its clock,
            // or the power limit under the driver's default
            if (p > 2 || (max_mem != 0 && c_mem * 2 < max_mem) || (def != 0 && l != 0 && l < def)) {
                ++held;
                worst_p = std::max(worst_p, p);
                why |= r;
            }
        }
        if (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(10)) continue;
        if (always || (busy > 0 && held * 4 >= samples))  // (held a quarter of the time or more; RAC1_GPU_WATCH=2: every 10 s)
            std::printf("[gpu] last 10 s: held by the system %d %% of the time the game kept it busy (%d %% of the time): P%d, graphics %.0f of %u MHz, memory %.0f of "
                        "%u MHz, %.1f W drawn, the power limit %.0f W (the driver's default %.0f W); the driver's reasons: %s. A setting of the PC (its power "
                        "mode, the charger, the temperature), not of the game\n",
                        busy != 0 ? held * 100 / busy : 0, busy * 100 / samples, worst_p, gr / std::max(busy, 1), max_gr, mem / std::max(busy, 1), max_mem, watts / std::max(busy, 1), lim_min / 1000.0, def / 1000.0,
                        reasons_text(why).c_str());
        std::fflush(stdout);
        samples = busy = held = worst_p = 0;
        gr = mem = watts = 0;
        why = 0;
        lim_min = lim;
        t0 = std::chrono::steady_clock::now();
    }
    lk.unlock();
    n.shutdown();
}

}  // namespace

// orig: (host) see host_gpu_watch.h
void host_gpu_watch_start() {
    if (g_thread.joinable()) return;
    const char* e = std::getenv("RAC1_GPU_WATCH");
    if (e != nullptr && std::atoi(e) == 0) return;
    const bool always = e != nullptr && std::atoi(e) == 2;
#ifdef _WIN32
    void* lib = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
    void* lib = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
    if (lib == nullptr) return;  // (not an NVIDIA GPU, or its driver without the library)
    Nvml n;
    get(lib, n.init, "nvmlInit_v2");
    get(lib, n.shutdown, "nvmlShutdown");
    get(lib, n.handle, "nvmlDeviceGetHandleByIndex_v2");
    get(lib, n.name, "nvmlDeviceGetName");
    get(lib, n.pstate, "nvmlDeviceGetPerformanceState");
    get(lib, n.clock, "nvmlDeviceGetClockInfo");
    get(lib, n.max_clock, "nvmlDeviceGetMaxClockInfo");
    get(lib, n.power, "nvmlDeviceGetPowerUsage");
    get(lib, n.limit, "nvmlDeviceGetEnforcedPowerLimit");
    get(lib, n.default_limit, "nvmlDeviceGetPowerManagementDefaultLimit");
    get(lib, n.reasons, "nvmlDeviceGetCurrentClocksEventReasons");
    if (n.reasons == nullptr) get(lib, n.reasons, "nvmlDeviceGetCurrentClocksThrottleReasons");
    get(lib, n.util, "nvmlDeviceGetUtilizationRates");
    if (n.init == nullptr || n.shutdown == nullptr || n.handle == nullptr || n.pstate == nullptr || n.clock == nullptr || n.max_clock == nullptr ||
        n.util == nullptr)
        return;
    if (n.init() != 0) return;
    nvmlDevice_t dev = nullptr;
    if (n.handle(0, &dev) != 0 || dev == nullptr) {  // (the first NVIDIA GPU: a laptop has one, the game's own)
        n.shutdown();
        return;
    }
    g_stop = false;
    g_thread = std::thread(run, n, dev, always);
}

// orig: (host) see host_gpu_watch.h
void host_gpu_watch_stop() {
    if (!g_thread.joinable()) return;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_stop = true;
    }
    g_cv.notify_all();
    g_thread.join();
}

}  // namespace rac1
