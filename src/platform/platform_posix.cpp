// The host's operating system on Linux (POSIX and glibc): see platform.h. Host code.
#include "platform.h"

#include <fcntl.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif

#include <dirent.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

extern char** environ;

namespace rac1::platform {

// ---- The coroutine: ucontext ----
namespace {
constexpr std::size_t kGuard = 1u << 16;  // the guard page(s) below the stack

struct Coroutine {
    ucontext_t host{};
    ucontext_t game{};
    void* stack = nullptr;
    std::size_t bytes = 0;
};
Coroutine g_co;
}  // namespace

bool coroutine_create(std::size_t bytes) {
    if (g_co.stack != nullptr) return true;
    void* p = mmap(nullptr, bytes + kGuard, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_STACK, -1, 0);
    if (p == MAP_FAILED) return false;
    mprotect(p, kGuard, PROT_NONE);
    g_co.stack = p;
    g_co.bytes = bytes;
    return true;
}

void coroutine_start(void (*entry)()) {
    getcontext(&g_co.game);  // (the host thread's FPU modes, the rounding of decisions 11, carried into the coroutine)
    g_co.game.uc_stack.ss_sp = static_cast<char*>(g_co.stack) + kGuard;
    g_co.game.uc_stack.ss_size = g_co.bytes;
    g_co.game.uc_link = &g_co.host;  // (entry returning resumes the host)
    makecontext(&g_co.game, entry, 0);
}

void coroutine_resume() { swapcontext(&g_co.host, &g_co.game); }

void coroutine_yield() { swapcontext(&g_co.game, &g_co.host); }

// ---- Files ----

std::FILE* open_file(const std::filesystem::path& path, const char* mode) { return std::fopen(path.c_str(), mode); }

std::int64_t read_at(std::FILE* f, std::uint64_t offset, void* dst, std::size_t n) {
    return static_cast<std::int64_t>(pread(fileno(f), dst, n, static_cast<off_t>(offset)));
}

void will_need(std::FILE* f, std::uint64_t offset, std::uint64_t bytes) {
    posix_fadvise(fileno(f), static_cast<off_t>(offset), static_cast<off_t>(bytes), POSIX_FADV_WILLNEED);
}

bool map_file(const std::filesystem::path& path, const std::byte** data, std::size_t* size) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st {};
    if (::fstat(fd, &st) != 0 || st.st_size <= 0) {
        ::close(fd);
        return false;
    }
    void* p = ::mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if (p == MAP_FAILED) return false;
    *data = static_cast<const std::byte*>(p);
    *size = static_cast<std::size_t>(st.st_size);
    return true;
}

void unmap_file(const std::byte* data, std::size_t size) {
    if (data != nullptr) ::munmap(const_cast<std::byte*>(data), size);
}

// ---- The player's folders ----

std::filesystem::path config_dir() {
    if (const char* x = std::getenv("XDG_CONFIG_HOME"); x != nullptr && *x != 0) return std::filesystem::path(x) / "rac1-native";
    if (const char* h = std::getenv("HOME"); h != nullptr && *h != 0) return std::filesystem::path(h) / ".config" / "rac1-native";
    return {};
}

std::filesystem::path data_dir() {
    if (const char* x = std::getenv("XDG_DATA_HOME"); x != nullptr && *x != 0) return std::string(x) + "/rac1-native";
    if (const char* h = std::getenv("HOME"); h != nullptr && *h != 0) return std::string(h) + "/.local/share/rac1-native";
    return {};
}

std::filesystem::path executable_path() {
    std::error_code ec;
    std::filesystem::path p = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path() : p;
}

std::string system_language() {
    for (const char* v : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* s = std::getenv(v);
        if (s == nullptr || *s == 0) continue;
        std::string l;
        for (const char* c = s; *c >= 'a' && *c <= 'z' && l.size() < 3; ++c) l += *c;  // ("it_IT.UTF-8" -> "it")
        return l.size() == 2 ? l : std::string();
    }
    return {};
}

// ---- Processes and the environment ----

int run_quiet(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = 0;
    const int err = posix_spawnp(&pid, argv[0], &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (err != 0) return -1;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void launch_updater(const std::vector<std::string>& args, const std::filesystem::path& cwd) {
    if (args.empty()) return;
    if (!cwd.empty()) {
        if (::chdir(cwd.c_str()) != 0) return;
    }
    std::vector<char*> argv;
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    ::execvp(argv[0], argv.data());
}

std::string python_program() { return "python3"; }

std::vector<std::string> environment_names(const char* prefix) {
    std::vector<std::string> out;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        const std::string kv = *e;
        if (kv.rfind(prefix, 0) != 0) continue;
        out.push_back(kv.substr(0, kv.find('=')));
    }
    return out;
}

void unset_environment(const char* name) { unsetenv(name); }

// ---- Threads, time ----

std::uint64_t thread_id() { return static_cast<std::uint64_t>(syscall(SYS_gettid)); }

double thread_cpu_ms() {
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) * 1e-6;
}

bool local_time(std::time_t t, std::tm* out) { return localtime_r(&t, out) != nullptr; }

// ---- The process's resources ----

namespace {
// the whole of a small /proc or /sys file (empty when it cannot be read)
std::string read_small(const char* path) {
    std::string s;
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return s;
    char buf[4096];
    for (;;) {
        const ssize_t n = read(fd, buf, sizeof buf);
        if (n <= 0) break;
        s.append(buf, static_cast<std::size_t>(n));
        if (s.size() > (1u << 20)) break;
    }
    close(fd);
    return s;
}
// the number after `key` ("\nVmRSS:" -> "VmRSS:   1234 kB")
std::uint64_t field_u64(const std::string& s, const char* key) {
    const std::size_t at = s.find(key);
    if (at == std::string::npos) return 0;
    return std::strtoull(s.c_str() + at + std::strlen(key), nullptr, 10);
}
// the rest of the line after `key`, its leading blanks skipped
std::string field_str(const std::string& s, const char* key) {
    std::string v;
    const std::size_t at = s.find(key);
    if (at == std::string::npos) return v;
    const char* p = s.c_str() + at + std::strlen(key);
    while (*p == ' ' || *p == '\t') ++p;
    while (*p != 0 && *p != '\n') v.push_back(*p++);
    return v;
}
}  // namespace

bool process_memory(ProcessMemory* out) {
    const std::string s = read_small("/proc/self/status");
    if (s.empty()) return false;
    out->resident = field_u64(s, "\nVmRSS:") * 1024;
    out->resident_peak = field_u64(s, "\nVmHWM:") * 1024;
    out->virtual_size = field_u64(s, "\nVmSize:") * 1024;
    return true;
}

double process_cpu_ms() {
    timespec ts{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) * 1e-6;
}

int cpu_count() {
    const long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? static_cast<int>(n) : 1;
}

bool process_threads(std::vector<ThreadTime>* out) {
    out->clear();
    DIR* d = opendir("/proc/self/task");
    if (d == nullptr) return false;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        ThreadTime t;
        t.tid = std::strtoull(e->d_name, nullptr, 10);
        char path[320];
        std::snprintf(path, sizeof path, "/proc/self/task/%s/comm", e->d_name);
        t.name = read_small(path);
        while (!t.name.empty() && (t.name.back() == '\n' || t.name.back() == ' ')) t.name.pop_back();
        // (schedstat's first field: the time on a CPU in ns; stat's utime + stime are clock ticks only)
        std::snprintf(path, sizeof path, "/proc/self/task/%s/schedstat", e->d_name);
        const std::string ss = read_small(path);
        if (!ss.empty()) {
            t.cpu_ms = static_cast<double>(std::strtoull(ss.c_str(), nullptr, 10)) * 1e-6;
        } else {
            std::snprintf(path, sizeof path, "/proc/self/task/%s/stat", e->d_name);
            const std::string st = read_small(path);
            const std::size_t rp = st.rfind(')');
            if (rp != std::string::npos && rp + 2 < st.size()) {  // (the fields after the name: the state is 3, utime 14, stime 15)
                const char* p = st.c_str() + rp + 2;
                unsigned long long ut = 0, sti = 0;
                for (int field = 3; field <= 15 && *p != 0; ++field) {
                    if (field == 14) ut = std::strtoull(p, nullptr, 10);
                    if (field == 15) sti = std::strtoull(p, nullptr, 10);
                    while (*p != 0 && *p != ' ') ++p;
                    while (*p == ' ') ++p;
                }
                t.cpu_ms = static_cast<double>(ut + sti) * 1000.0 / static_cast<double>(sysconf(_SC_CLK_TCK));
            }
        }
        out->push_back(std::move(t));
    }
    closedir(d);
    return true;
}

bool gpu_client_info(GpuClientInfo* out) {
    *out = GpuClientInfo{};
    DIR* d = opendir("/proc/self/fdinfo");
    if (d == nullptr) return false;
    std::vector<unsigned long long> clients;  // (several descriptors may share one DRM client: counted once)
    std::string pdev;
    bool engine = false;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[320];
        std::snprintf(path, sizeof path, "/proc/self/fdinfo/%s", e->d_name);
        const std::string s = read_small(path);
        if (s.find("drm-driver:") == std::string::npos) continue;
        const std::size_t cid = s.find("drm-client-id:");
        const unsigned long long id = cid != std::string::npos ? std::strtoull(s.c_str() + cid + 14, nullptr, 10) : 0;
        if (std::find(clients.begin(), clients.end(), id) != clients.end()) continue;
        clients.push_back(id);
        out->ok = true;
        if (out->driver.empty()) out->driver = field_str(s, "drm-driver:");
        if (pdev.empty()) pdev = field_str(s, "drm-pdev:");
        // (newer kernels: drm-resident-<region>, older: drm-memory-<region>; the unit after the number)
        auto region = [&s](const char* a, const char* b) -> std::uint64_t {
            std::size_t at = s.find(a), len = std::strlen(a);
            if (at == std::string::npos) at = s.find(b), len = std::strlen(b);
            if (at == std::string::npos) return 0;
            char* end = nullptr;
            const std::uint64_t v = std::strtoull(s.c_str() + at + len, &end, 10);
            while (end != nullptr && (*end == ' ' || *end == '\t')) ++end;
            if (end != nullptr && std::strncmp(end, "KiB", 3) == 0) return v * 1024;
            if (end != nullptr && std::strncmp(end, "MiB", 3) == 0) return v * 1024 * 1024;
            return v;
        };
        out->vram += region("drm-resident-vram:", "drm-memory-vram:");
        out->gtt += region("drm-resident-gtt:", "drm-memory-gtt:");
        const std::size_t eg = s.find("drm-engine-gfx:");
        if (eg != std::string::npos) {
            out->engine_ms = (engine ? out->engine_ms : 0.0) + static_cast<double>(std::strtoull(s.c_str() + eg + 15, nullptr, 10)) * 1e-6;
            engine = true;
        }
    }
    closedir(d);
    if (!pdev.empty() && pdev.find('/') == std::string::npos) {  // the device's own totals (amdgpu's sysfs)
        const std::string base = "/sys/bus/pci/devices/" + pdev + "/";
        auto num = [&base](const char* f) { return static_cast<std::uint64_t>(std::strtoull(read_small((base + f).c_str()).c_str(), nullptr, 10)); };
        out->vram_total = num("mem_info_vram_total");
        out->vram_used = num("mem_info_vram_used");
        out->gtt_total = num("mem_info_gtt_total");
        out->gtt_used = num("mem_info_gtt_used");
    }
    return out->ok;
}

// ---- The process's host settings ----

void process_begin() {}

void process_end() {}

// ---- Debugging ----

void print_backtrace(int frames) {
#if defined(__GLIBC__)
    void* f[64];
    const int n = frames < 64 ? frames : 64;
    backtrace_symbols_fd(f, backtrace(f, n), 2);
#else
    (void)frames;
#endif
}

}  // namespace rac1::platform
