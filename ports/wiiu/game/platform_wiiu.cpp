// The operating system's calls on the Wii U (Cafe OS through wut and devkitPPC's newlib), behind
// src/platform/platform.h: the coroutine's hand-written switch (coroutine_ppc.S), files through newlib, and stand-ins
// for what the console has no use for (the setup's helper programs, the updater, the GPU's accounts). Host code.
#include "platform/platform.h"
#include "platform/ctx_switch.h"

#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

extern char** environ;

namespace rac1::platform {

// ---- The coroutine ----
namespace {
struct Coroutine {
    void* stack = nullptr;
    std::size_t bytes = 0;
    void* host_sp = nullptr;
    void* game_sp = nullptr;
    void (*entry)() = nullptr;
};
Coroutine g_co;

[[noreturn]] void co_main() {
    g_co.entry();
    void* dead = nullptr;
    rac1_ctx_switch(&dead, g_co.host_sp);
    __builtin_trap();
}
}  // namespace

bool coroutine_create(std::size_t bytes) {
    if (g_co.stack != nullptr) return true;
    g_co.stack = MEMAllocFromDefaultHeapEx(static_cast<uint32_t>(bytes), 64);
    if (g_co.stack == nullptr) return false;
    g_co.bytes = bytes;
    return true;
}

void coroutine_start(void (*entry)()) {
    g_co.entry = entry;
    const auto top = (reinterpret_cast<std::uintptr_t>(g_co.stack) + g_co.bytes) & ~static_cast<std::uintptr_t>(15);
    g_co.game_sp = ctx_new_frame(top, &co_main);
}

void coroutine_resume() { rac1_ctx_switch(&g_co.host_sp, g_co.game_sp); }
void coroutine_yield() { rac1_ctx_switch(&g_co.game_sp, g_co.host_sp); }

// ---- Files ----
// A file read at far apart places (the disc image: the level's data, two music streams) is read through up to four handles
// of its own: each read takes the handle that is already just before it. The SD card's file system walks a file's cluster
// chain from its start to seek back: a 2 KB read of a stream behind another, 3 GB into the image, took ~96 ms; ahead, ~10.
namespace {
struct Handle {
    std::FILE* f = nullptr;
    std::uint64_t pos = ~0ull;  // (where its next read starts; ~0: unknown)
    std::uint64_t used = 0;
};
struct Handles {
    std::string path;
    Handle h[4];
    int n = 0;
};
std::mutex g_files_m;
std::map<std::FILE*, Handles> g_files;  // (the files opened for reading, by their first handle)
std::uint64_t g_reads = 0;
}  // namespace

std::FILE* open_file(const std::filesystem::path& path, const char* mode) {
    std::FILE* f = std::fopen(path.c_str(), mode);
    if (f != nullptr && std::strcmp(mode, "rb") == 0) {
        std::scoped_lock lock(g_files_m);
        Handles& hs = g_files[f];  // (a handle of a file closed since: its entry and other handles replaced)
        for (int i = 1; i < hs.n; ++i) std::fclose(hs.h[i].f);
        hs = Handles{};
        hs.path = path.string();
        hs.h[0].f = f;
        hs.n = 1;
    }
    return f;
}

// (no pread here: a seek and a read under one lock, the game's thread and the audio's may read at once)
std::int64_t read_at(std::FILE* f, std::uint64_t offset, void* dst, std::size_t n) {
    std::scoped_lock lock(g_files_m);
    static const double slow_ms = std::getenv("RAC1_WIIU_SLOW_READ") != nullptr ? std::atof(std::getenv("RAC1_WIIU_SLOW_READ")) : -1.0;
    const OSTime t0 = OSGetTime();
    Handle* h = nullptr;
    Handle one{f};
    if (auto it = g_files.find(f); it != g_files.end()) {
        Handles& hs = it->second;
        // the handle just before the read (up to 4 MB: ahead within a cluster chain is cheap), else a new one, else the least used lately
        for (int i = 0; i < hs.n; ++i)
            if (hs.h[i].pos != ~0ull && hs.h[i].pos <= offset && offset - hs.h[i].pos <= (4u << 20) && (h == nullptr || hs.h[i].pos > h->pos)) h = &hs.h[i];
        if (h == nullptr && hs.n < 4) {
            if (std::FILE* g = std::fopen(hs.path.c_str(), "rb"); g != nullptr) {
                h = &hs.h[hs.n++];
                h->f = g;
            }
        }
        if (h == nullptr) {
            h = &hs.h[0];
            for (int i = 1; i < hs.n; ++i)
                if (hs.h[i].used < h->used) h = &hs.h[i];
        }
    } else {
        h = &one;
    }
    h->used = ++g_reads;
    if (h->pos != offset && fseeko(h->f, static_cast<off_t>(offset), SEEK_SET) != 0) {
        h->pos = ~0ull;
        return -1;
    }
    const auto r = static_cast<std::int64_t>(std::fread(dst, 1, n, h->f));
    h->pos = r >= 0 ? offset + static_cast<std::uint64_t>(r) : ~0ull;
    if (slow_ms >= 0.0) {  // (testing, RAC1_WIIU_SLOW_READ=<ms>: the reads slower than that)
        const double ms = static_cast<double>(OSTicksToMicroseconds(OSGetTime() - t0)) / 1000.0;
        if (ms > slow_ms) std::printf("[wiiu] read %llu bytes at %llu: %.1f ms\n", static_cast<unsigned long long>(n), static_cast<unsigned long long>(offset), ms);
    }
    return r;
}

void will_need(std::FILE*, std::uint64_t, std::uint64_t) {}

// (no memory mapping: the file read whole into memory)
bool map_file(const std::filesystem::path& path, const std::byte** data, std::size_t* size) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    auto* buf = static_cast<std::byte*>(std::malloc(n > 0 ? static_cast<std::size_t>(n) : 1));
    const bool ok = buf != nullptr && std::fread(buf, 1, static_cast<std::size_t>(n), f) == static_cast<std::size_t>(n);
    std::fclose(f);
    if (!ok) {
        std::free(buf);
        return false;
    }
    *data = buf;
    *size = static_cast<std::size_t>(n);
    return true;
}

void unmap_file(const std::byte* data, std::size_t) { std::free(const_cast<std::byte*>(data)); }

// ---- Folders: everything beside the game's data on the SD card (the working directory) ----
std::filesystem::path config_dir() { return "config"; }
std::filesystem::path data_dir() { return "data"; }
std::filesystem::path executable_path() { return "rac1-native.rpx"; }
std::string system_language() { return "en"; }

// ---- Programs: none on the console ----
int run_quiet(const std::vector<std::string>&) { return -1; }
void launch_updater(const std::vector<std::string>&, const std::filesystem::path&) {}
std::string python_program() { return ""; }

// ---- The environment ----
std::vector<std::string> environment_names(const char* prefix) {
    std::vector<std::string> out;
    const std::size_t n = std::strlen(prefix);
    for (char** e = environ; e != nullptr && *e != nullptr; ++e)
        if (std::strncmp(*e, prefix, n) == 0) {
            const char* eq = std::strchr(*e, '=');
            out.emplace_back(*e, eq != nullptr ? static_cast<std::size_t>(eq - *e) : std::strlen(*e));
        }
    return out;
}
void unset_environment(const char* name) { unsetenv(name); }

// ---- Threads and time ----
std::uint64_t thread_id() { return reinterpret_cast<std::uintptr_t>(OSGetCurrentThread()); }
double thread_cpu_ms() { return static_cast<double>(OSTicksToMicroseconds(OSGetSystemTime())) / 1000.0; }
bool local_time(std::time_t t, std::tm* out) { return localtime_r(&t, out) != nullptr; }

// ---- The process: no accounts here ----
bool process_memory(ProcessMemory*) { return false; }
double process_cpu_ms() { return thread_cpu_ms(); }
int cpu_count() { return 3; }
bool process_threads(std::vector<ThreadTime>*) { return false; }
bool gpu_client_info(GpuClientInfo*) { return false; }
void process_begin() {}
void process_end() {}
void print_backtrace(int) {}

}  // namespace rac1::platform
