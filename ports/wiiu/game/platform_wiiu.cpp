// The operating system's calls on the Wii U (Cafe OS through wut and devkitPPC's newlib), behind
// src/platform/platform.h: the coroutine's hand-written switch (coroutine_ppc.S), files through newlib, and stand-ins
// for what the console has no use for (the setup's helper programs, the updater, the GPU's accounts). Host code.
#include "platform/platform.h"
#include "platform/ctx_switch.h"

#include <coreinit/memdefaultheap.h>
#include <coreinit/memory.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <memory>
#include <fstream>
#include <map>
#include <sstream>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

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
// The calls into the card's file system one at a time (g_fs_m), as they always were, but a long read in pieces of 512 KB: the lock
// held for the whole read had a stream's 64 KB wait behind the level's data.wad, 13-19 MB read whole (0.6-0.85 s); between two pieces
// another read goes. A file's handles under a lock of their own.
// The files kept in memory (keep_in_memory): read whole at once, on the caller's thread (in a level's load), then their reads served
// from memory. Not in the background: with a thread of its own reading them (on core 0 or 2, at priority 24 or 30, with the card's calls
// one at a time or not) the console froze 3 times in 7 runs, at a level's start, the log cut short (not found). RAC1_WIIU_PRELOAD_MB
// (default 160; 0: none) the most kept; a file not read in when the app's memory left (MEM2 past the heap's end, and the heap's own free
// space) would fall under 192 MB.
namespace {
struct Handle {
    std::FILE* f = nullptr;
    std::uint64_t pos = ~0ull;  // (where its next read starts; ~0: unknown)
    std::uint64_t used = 0;
};
// a file's bytes, not zeroed when made (a vector's 55 MB of zeros, written over at once: ~0.3 s of a load)
struct Blob {
    std::unique_ptr<std::uint8_t[]> p;
    std::size_t n = 0;
    explicit Blob(std::size_t size) : p(new std::uint8_t[size]), n(size) {}
    std::uint8_t* data() { return p.get(); }
    const std::uint8_t* data() const { return p.get(); }
    std::size_t size() const { return n; }
};
using Bytes = std::shared_ptr<const Blob>;
struct Handles {
    std::mutex m;  // (its handles and their positions)
    std::string path;
    Handle h[4];
    int n = 0;
    Bytes ram;  // (the file kept in memory: its reads from here)
};
std::mutex g_files_m;  // (the map, and the files kept in memory)
std::mutex g_fs_m;     // (the card: one call into its file system at a time, as before - with four threads in it at once the console hung)
std::map<std::FILE*, Handles> g_files;  // (the files opened for reading, by their first handle)
std::atomic<std::uint64_t> g_reads{0};
std::map<std::string, Bytes> g_ram;  // (the files kept in memory, by their path)
std::map<std::string, int> g_ram_group;
std::uint64_t g_ram_bytes = 0;
std::uint64_t preload_cap() {
    static const std::uint64_t mb = std::getenv("RAC1_WIIU_PRELOAD_MB") != nullptr ? std::strtoull(std::getenv("RAC1_WIIU_PRELOAD_MB"), nullptr, 10) : 160;
    return mb << 20;
}
// the app's memory left: MEM2 past the heap's end and the heap's free space (wut's sbrk does not refuse past the end: see wiiu_gfx.cpp)
std::uint64_t memory_left() {
    std::uint32_t m2 = 0, m2_size = 0;
    OSGetMemBound(OS_MEM2, &m2, &m2_size);
    const auto brk = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(sbrk(0)));
    const std::uint64_t past = brk >= m2 && brk <= m2 + m2_size ? m2 + m2_size - brk : 0;
    return past + static_cast<std::uint32_t>(mallinfo().fordblks);
}
}  // namespace

void keep_in_memory(int group, const std::vector<std::filesystem::path>& paths) {
    if (preload_cap() == 0 || group < 0 || group > 3) return;
    static const bool log = std::getenv("RAC1_WIIU_PRELOAD_LOG") != nullptr;
    std::vector<Bytes> dropped;  // (freed after the lock)
    {
        std::scoped_lock lock(g_files_m);  // (the group's earlier files dropped; a reader holding one keeps it alive)
        for (auto it = g_ram_group.begin(); it != g_ram_group.end();) {
            if (it->second != group) {
                ++it;
                continue;
            }
            if (auto r = g_ram.find(it->first); r != g_ram.end()) {
                g_ram_bytes -= r->second->size();
                dropped.push_back(std::move(r->second));
                g_ram.erase(r);
            }
            for (auto& [fp, hs] : g_files)
                if (hs.path == it->first) dropped.push_back(std::move(hs.ram));
            it = g_ram_group.erase(it);
        }
    }
    dropped.clear();
    const OSTime t0 = OSGetTime();
    std::uint64_t read = 0;
    std::size_t files = 0;
    for (const auto& p : paths) {
        const std::string path = p.string();
        std::uint64_t kept;
        {
            std::scoped_lock lock(g_files_m);
            if (g_ram.count(path) != 0) continue;
            kept = g_ram_bytes;
        }
        std::shared_ptr<Blob> v;
        std::FILE* f = nullptr;
        long size = 0;
        {
            std::scoped_lock fs(g_fs_m);
            f = std::fopen(path.c_str(), "rb");
            if (f == nullptr) continue;
            std::fseek(f, 0, SEEK_END);
            size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
        }
        if (size > 0 && kept + static_cast<std::uint64_t>(size) <= preload_cap() && memory_left() > (192ull << 20) + static_cast<std::uint64_t>(size)) {
            v = std::make_shared<Blob>(static_cast<std::size_t>(size));
            // in pieces of 512 KB, as read_at: read whole under the card's lock (25-55 MB, 1.3-3 s) it held the music's and the
            // sounds' streams that long - the sound stalled right before a landing
            for (std::size_t at = 0; at < v->size();) {
                const std::size_t k = std::min<std::size_t>(512 * 1024, v->size() - at);
                std::size_t got;
                {
                    std::scoped_lock fs(g_fs_m);
                    got = std::fread(v->data() + at, 1, k, f);
                }
                at += got;
                if (got < k) {
                    v.reset();
                    break;
                }
            }
        }
        {
            std::scoped_lock fs(g_fs_m);
            std::fclose(f);
        }
        if (v == nullptr) continue;
        read += v->size();
        ++files;
        std::scoped_lock lock(g_files_m);
        Bytes b = v;
        g_ram[path] = b;
        g_ram_group[path] = group;
        g_ram_bytes += v->size();
        for (auto& [fp, hs] : g_files)
            if (hs.path == path) hs.ram = b;  // (its handles: their reads from here on)
    }
    if (log)
        std::printf("[preload] group %d: %zu files, %llu KB in %.1f ms (%llu MB kept, %llu MB of memory left)\n", group, files,
                    static_cast<unsigned long long>(read >> 10), static_cast<double>(OSTicksToMicroseconds(OSGetTime() - t0)) / 1000.0,
                    static_cast<unsigned long long>(g_ram_bytes >> 20), static_cast<unsigned long long>(memory_left() >> 20));
}

bool read_kept(const std::filesystem::path& path, std::uint64_t offset, void* dst, std::size_t n) {
    std::scoped_lock lock(g_files_m);
    if (g_ram.empty()) return false;
    const auto it = g_ram.find(path.string());
    if (it == g_ram.end() || offset + n > it->second->size()) return false;
    std::memcpy(dst, it->second->data() + offset, n);
    return true;
}

std::FILE* open_file(const std::filesystem::path& path, const char* mode) {
    std::FILE* f = std::fopen(path.c_str(), mode);  // (outside g_fs_m, as before)
    if (f != nullptr && std::strcmp(mode, "rb") == 0) {
        std::unique_lock lock(g_files_m);
        Handles& hs = g_files[f];  // (a handle of a file closed since: its entry and other handles replaced)
        Bytes ram;
        if (auto it = g_ram.find(path.string()); it != g_ram.end()) ram = it->second;
        std::scoped_lock hl(hs.m, g_fs_m);
        for (int i = 1; i < hs.n; ++i) std::fclose(hs.h[i].f);
        for (Handle& h : hs.h) h = Handle{};
        hs.path = path.string();
        hs.h[0].f = f;
        hs.n = 1;
        hs.ram = ram;
    }
    return f;
}

bool read_packed(const std::string& path, std::vector<std::uint8_t>& out) {
    using Index = std::map<std::string, std::pair<std::uint64_t, std::uint64_t>>;  // (a file's offset and size in the pack)
    static std::mutex m;
    static std::map<std::string, Index> indexes;  // (by folder, read once; empty: no pack)
    static std::string loaded;                    // (the folder whose pack is in memory)
    static std::vector<std::uint8_t> pack;
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return false;
    const std::string dir = path.substr(0, slash);
    std::scoped_lock lock(m);
    auto ix_it = indexes.find(dir);
    if (ix_it == indexes.end()) {
        Index ix;
        std::ifstream in(dir + "/files.pack.txt");
        std::string line;
        if (in && std::getline(in, line) && line == "rac1-native files pack 1")
            while (std::getline(in, line)) {
                std::istringstream ls(line);
                std::string name;
                std::uint64_t off = 0, size = 0;
                if (ls >> name >> off >> size) ix[name] = {off, size};
            }
        ix_it = indexes.emplace(dir, std::move(ix)).first;
    }
    const auto it = ix_it->second.find(path.substr(slash + 1));
    if (it == ix_it->second.end()) return false;
    if (loaded != dir) {  // (the folder's pack read whole, in pieces of 512 KB as read_at: the sound's streams between them)
        loaded.clear();
        pack.clear();
        pack.shrink_to_fit();
        const OSTime t0 = OSGetTime();
        std::FILE* f;
        long size;
        {
            std::scoped_lock fs(g_fs_m);
            f = std::fopen((dir + "/files.pack").c_str(), "rb");
            if (f == nullptr) return false;
            std::fseek(f, 0, SEEK_END);
            size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
        }
        pack.resize(size > 0 ? static_cast<std::size_t>(size) : 0);
        bool ok = true;
        for (std::size_t at = 0; at < pack.size();) {
            const std::size_t k = std::min<std::size_t>(512 * 1024, pack.size() - at);
            std::size_t got;
            {
                std::scoped_lock fs(g_fs_m);
                got = std::fread(pack.data() + at, 1, k, f);
            }
            at += got;
            if (got < k) {
                ok = false;
                break;
            }
        }
        {
            std::scoped_lock fs(g_fs_m);
            std::fclose(f);
        }
        if (!ok) {
            pack.clear();
            return false;
        }
        loaded = dir;
        if (std::getenv("RAC1_LOAD_PROF") != nullptr)
            std::printf("[load] %s/files.pack: %zu files, %zu KB in %.0f ms\n", dir.c_str(), ix_it->second.size(), pack.size() >> 10,
                        static_cast<double>(OSTicksToMicroseconds(OSGetTime() - t0)) / 1000.0);
    }
    if (it->second.first + it->second.second > pack.size()) return false;
    out.assign(pack.begin() + static_cast<std::ptrdiff_t>(it->second.first), pack.begin() + static_cast<std::ptrdiff_t>(it->second.first + it->second.second));
    return true;
}

void close_file(std::FILE* f) {
    Handle extra[3];
    int n = 0;
    {
        std::scoped_lock lock(g_files_m);
        if (auto it = g_files.find(f); it != g_files.end()) {
            {
                std::scoped_lock hl(it->second.m);
                for (int i = 1; i < it->second.n; ++i) extra[n++] = it->second.h[i];
            }
            g_files.erase(it);
        }
    }
    std::scoped_lock fs(g_fs_m);
    for (int i = 0; i < n; ++i) std::fclose(extra[i].f);
    std::fclose(f);
}

// (no pread here: a seek and a read under the file's lock)
std::int64_t read_at(std::FILE* f, std::uint64_t offset, void* dst, std::size_t n) {
    static const double slow_ms = std::getenv("RAC1_WIIU_SLOW_READ") != nullptr ? std::atof(std::getenv("RAC1_WIIU_SLOW_READ")) : -1.0;
    const OSTime t0 = OSGetTime();
    Handles* hs = nullptr;
    {
        std::scoped_lock lock(g_files_m);
        if (auto it = g_files.find(f); it != g_files.end()) {
            hs = &it->second;
            if (const Bytes ram = hs->ram) {  // (kept in memory)
                if (offset >= ram->size()) return 0;
                const std::size_t k = std::min<std::size_t>(n, ram->size() - offset);
                std::memcpy(dst, ram->data() + offset, k);
                return static_cast<std::int64_t>(k);
            }
        }
    }
    Handle one{f};
    std::unique_lock<std::mutex> hl;
    Handle* h = nullptr;
    if (hs != nullptr) {
        hl = std::unique_lock<std::mutex>(hs->m);
        // the handle just before the read (up to 4 MB: ahead within a cluster chain is cheap), else a new one, else the least used lately
        for (int i = 0; i < hs->n; ++i)
            if (hs->h[i].pos != ~0ull && hs->h[i].pos <= offset && offset - hs->h[i].pos <= (4u << 20) && (h == nullptr || hs->h[i].pos > h->pos)) h = &hs->h[i];
        if (h == nullptr && hs->n < 4) {
            std::scoped_lock fs(g_fs_m);
            if (std::FILE* g = std::fopen(hs->path.c_str(), "rb"); g != nullptr) {
                h = &hs->h[hs->n++];
                h->f = g;
            }
        }
        if (h == nullptr) {
            h = &hs->h[0];
            for (int i = 1; i < hs->n; ++i)
                if (hs->h[i].used < h->used) h = &hs->h[i];
        }
    } else {
        h = &one;
    }
    h->used = ++g_reads;
    if (h->pos != offset) {
        std::scoped_lock fs(g_fs_m);
        if (fseeko(h->f, static_cast<off_t>(offset), SEEK_SET) != 0) {
            h->pos = ~0ull;
            return -1;
        }
    }
    std::int64_t r = 0;
    for (std::size_t at = 0; at < n;) {  // (in pieces: another file's read between them)
        const std::size_t k = std::min<std::size_t>(512 * 1024, n - at);
        std::size_t got;
        {
            std::scoped_lock fs(g_fs_m);
            got = std::fread(static_cast<std::uint8_t*>(dst) + at, 1, k, h->f);
        }
        r += static_cast<std::int64_t>(got);
        at += got;
        if (got < k) break;
    }
    h->pos = offset + static_cast<std::uint64_t>(r);
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
