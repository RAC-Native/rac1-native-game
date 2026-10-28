// The host's thread priorities through rtkit (see host_priority.h): Linux. Host code.
#include "host_priority.h"

#if defined(__APPLE__)
// (macOS: no rtkit; the threads keep the priorities the system gives them)
namespace rac1 {
// orig: (host) host_priority.h, on macOS: nothing to raise
bool host_priority_enabled() { return false; }
void host_priority_set_enabled(bool) {}
bool host_priority_realtime(std::uint64_t, int) { return false; }
int host_priority_raise_all(int) { return 0; }
}  // namespace rac1
#else

#include <dirent.h>
#include <dlfcn.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <type_traits>

namespace rac1 {

namespace {
// libdbus's C API, the few calls needed (dbus/dbus.h is not required to build: the library is loaded if present)
struct DBusError {
    const char* name;
    const char* message;
    unsigned int dummy;
    void* padding1;
    char reserve[32];  // (room: the real struct is smaller)
};
using Conn = void;
using Msg = void;
struct Dbus {
    void (*error_init)(DBusError*) = nullptr;
    void (*error_free)(DBusError*) = nullptr;
    unsigned (*error_is_set)(const DBusError*) = nullptr;
    Conn* (*bus_get)(int, DBusError*) = nullptr;
    void (*set_exit_on_disconnect)(Conn*, unsigned) = nullptr;
    Msg* (*new_method_call)(const char*, const char*, const char*, const char*) = nullptr;
    unsigned (*append_args)(Msg*, int, ...) = nullptr;
    Msg* (*send_block)(Conn*, Msg*, int, DBusError*) = nullptr;
    void (*unref)(Msg*) = nullptr;
    Conn* conn = nullptr;
    bool ok = false;
};
constexpr int kBusSystem = 1;
constexpr int kTypeInvalid = 0, kTypeInt32 = 'i', kTypeUint32 = 'u', kTypeUint64 = 't';

std::mutex g_lock;
int g_enabled = -1;  // -1: not decided (the default: on unless headless, decided by the host)

Dbus* dbus() {
    static Dbus d;
    static bool tried = false;
    if (tried) return d.ok ? &d : nullptr;
    tried = true;
    void* h = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    if (h == nullptr) return nullptr;
    auto sym = [h](auto& f, const char* n) { f = reinterpret_cast<std::remove_reference_t<decltype(f)>>(dlsym(h, n)); return f != nullptr; };
    if (!sym(d.error_init, "dbus_error_init") || !sym(d.error_free, "dbus_error_free") || !sym(d.error_is_set, "dbus_error_is_set") ||
        !sym(d.bus_get, "dbus_bus_get") || !sym(d.set_exit_on_disconnect, "dbus_connection_set_exit_on_disconnect") ||
        !sym(d.new_method_call, "dbus_message_new_method_call") || !sym(d.append_args, "dbus_message_append_args") ||
        !sym(d.send_block, "dbus_connection_send_with_reply_and_block") || !sym(d.unref, "dbus_message_unref"))
        return nullptr;
    DBusError e;
    d.error_init(&e);
    d.conn = d.bus_get(kBusSystem, &e);
    if (d.error_is_set(&e) != 0 || d.conn == nullptr) {
        d.error_free(&e);
        return nullptr;
    }
    d.set_exit_on_disconnect(d.conn, 0);  // (libdbus would otherwise _exit the process if the bus goes away)
    d.ok = true;
    return &d;
}

// orig: (host) one rtkit call with (u64 thread, <type> value); true when it answered without an error
bool rtkit_call(const char* method, uint64_t tid, int type, const void* value) {
    std::scoped_lock lock(g_lock);
    Dbus* d = dbus();
    if (d == nullptr) return false;
    Msg* m = d->new_method_call("org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1", "org.freedesktop.RealtimeKit1", method);
    if (m == nullptr) return false;
    if (d->append_args(m, kTypeUint64, &tid, type, value, kTypeInvalid) == 0) {
        d->unref(m);
        return false;
    }
    DBusError e;
    d->error_init(&e);
    Msg* r = d->send_block(d->conn, m, 1000, &e);
    d->unref(m);
    const bool ok = r != nullptr && d->error_is_set(&e) == 0;
    static int reported = 0;  // (rtkit refuses past its rate limit, "busy": reported a few times)
    if (!ok && d->error_is_set(&e) != 0 && e.message != nullptr && reported++ < 2)
        std::fprintf(stderr, "[priority] rtkit %s(%llu): %s\n", method, static_cast<unsigned long long>(tid), e.message);
    if (r != nullptr) d->unref(r);
    d->error_free(&e);
    return ok;
}
}  // namespace

bool host_priority_enabled() {
    if (const char* p = std::getenv("RAC1_PRIORITY")) return std::atoi(p) != 0;
    return g_enabled > 0;
}

void host_priority_set_enabled(bool on) { g_enabled = on ? 1 : 0; }

bool host_priority_realtime(std::uint64_t tid, int prio) {
    if (!host_priority_enabled()) return false;
    // rtkit requires the process's RLIMIT_RTTIME at most its RTTimeUSecMax (200 ms): a real-time thread that runs that long
    // without blocking gets SIGXCPU (the audio thread blocks every few ms)
    rlimit rl{};
    rl.rlim_cur = rl.rlim_max = 200000;
    if (setrlimit(RLIMIT_RTTIME, &rl) != 0) return false;
    const uint32_t p = static_cast<uint32_t>(prio < 1 ? 1 : prio > 20 ? 20 : prio);
    return rtkit_call("MakeThreadRealtime", static_cast<uint64_t>(tid), kTypeUint32, &p);
}

int host_priority_raise_all(int nice) {
    if (!host_priority_enabled()) return 0;
    const int32_t n = nice < -15 ? -15 : nice;
    int changed = 0;
    if (DIR* dir = opendir("/proc/self/task")) {
        while (dirent* e = readdir(dir)) {
            const long tid = std::strtol(e->d_name, nullptr, 10);
            if (tid <= 0) continue;
            const int pol = sched_getscheduler(static_cast<pid_t>(tid));
            if (pol == SCHED_FIFO || pol == SCHED_RR) continue;  // (the audio thread, already real-time)
            if (getpriority(PRIO_PROCESS, static_cast<id_t>(tid)) <= n) continue;
            if (rtkit_call("MakeThreadHighPriority", static_cast<uint64_t>(tid), kTypeInt32, &n)) ++changed;
        }
        closedir(dir);
    }
    return changed;
}

}  // namespace rac1

#endif  // !__APPLE__
