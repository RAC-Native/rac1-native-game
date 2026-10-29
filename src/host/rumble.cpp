// src/host/rumble.cpp — the gamepad's vibration motors (see rumble.h). Host code, not a port.
#include "rumble.h"

#include <chrono>
#include <cmath>
#include <cstdio>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__WIIU__)
#include <vpad/input.h>
#endif

namespace rac1::rumble {

namespace {
using Clock = std::chrono::steady_clock;

struct State {
    std::string guid;           // the pad driven ("" none)
    int low = 0, high = 0;      // the strengths last set, in 64ths
    Clock::time_point last{};   // when they were last set
    bool tried = false;         // the device looked for (once per pad)
#if defined(__linux__)
    int fd = -1;
    int effect = -1;
#endif
} g_state;

constexpr int kSteps = 64;
constexpr auto kRefresh = std::chrono::milliseconds(500);
constexpr uint16_t kLengthMs = 1000;

#if defined(__linux__)
bool test_bit(const unsigned long* bits, int n) {
    constexpr int kBits = 8 * sizeof(unsigned long);
    return (bits[n / kBits] >> (n % kBits)) & 1ul;
}

// orig: (host) GLFW's GUID for an evdev device (linux_joystick.c: the bus, vendor, product and version, little-endian,
// each followed by two zero bytes)
std::string evdev_guid(const input_id& id) {
    char g[33];
    std::snprintf(g, sizeof g, "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000", id.bustype & 0xff, id.bustype >> 8, id.vendor & 0xff,
                  id.vendor >> 8, id.product & 0xff, id.product >> 8, id.version & 0xff, id.version >> 8);
    return g;
}

void close_device() {
    if (g_state.fd >= 0) {
        if (g_state.effect >= 0) ioctl(g_state.fd, EVIOCRMFF, g_state.effect);
        close(g_state.fd);
    }
    g_state.fd = -1;
    g_state.effect = -1;
}

// orig: (host) the event device with FF_RUMBLE whose GUID is `guid`
void open_device(const std::string& guid) {
    for (int i = 0; i < 64; ++i) {
        char path[32];
        std::snprintf(path, sizeof path, "/dev/input/event%d", i);
        const int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        input_id id{};
        unsigned long ff[(FF_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long))] = {};
        if (ioctl(fd, EVIOCGID, &id) == 0 && evdev_guid(id) == guid && ioctl(fd, EVIOCGBIT(EV_FF, sizeof ff), ff) >= 0 &&
            test_bit(ff, FF_RUMBLE)) {
            g_state.fd = fd;
            char name[128] = {};
            ioctl(fd, EVIOCGNAME(sizeof name), name);
            std::printf("[rumble] %s (%s): force feedback\n", path, name);
            std::fflush(stdout);
            return;
        }
        close(fd);
    }
    std::printf("[rumble] no force feedback device for the pad %s (or no write access to it)\n", guid.c_str());
    std::fflush(stdout);
}

bool write_motors(int low, int high) {
    if (g_state.fd < 0) return false;
    ff_effect e{};
    e.type = FF_RUMBLE;
    e.id = static_cast<int16_t>(g_state.effect);
    e.u.rumble.strong_magnitude = static_cast<uint16_t>(low * 0xffff / kSteps);
    e.u.rumble.weak_magnitude = static_cast<uint16_t>(high * 0xffff / kSteps);
    e.replay.length = kLengthMs;
    if (ioctl(g_state.fd, EVIOCSFF, &e) < 0) {
        if (errno == ENODEV) close_device();
        return false;
    }
    g_state.effect = e.id;
    input_event ev{};
    ev.type = EV_FF;
    ev.code = static_cast<uint16_t>(g_state.effect);
    ev.value = low > 0 || high > 0 ? 1 : 0;
    return write(g_state.fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev);
}
#elif defined(_WIN32)
struct XInputVibration {
    WORD left, right;
};
using SetStateFn = DWORD(WINAPI*)(DWORD, XInputVibration*);
SetStateFn xinput_set() {
    static SetStateFn fn = [] {
        HMODULE m = LoadLibraryA("xinput1_4.dll");
        if (m == nullptr) m = LoadLibraryA("xinput9_1_0.dll");
        return m != nullptr ? reinterpret_cast<SetStateFn>(GetProcAddress(m, "XInputSetState")) : nullptr;
    }();
    return fn;
}
void close_device() {}
void open_device(const std::string&) {}
bool write_motors(int low, int high) {
    SetStateFn fn = xinput_set();
    if (fn == nullptr) return false;
    XInputVibration v{static_cast<WORD>(low * 0xffff / kSteps), static_cast<WORD>(high * 0xffff / kSteps)};
    bool any = false;
    for (DWORD i = 0; i < 4; ++i) any |= fn(i, &v) == ERROR_SUCCESS;  // (ERROR_DEVICE_NOT_CONNECTED for the empty slots)
    return any;
}
#elif defined(__WIIU__)
void close_device() { VPADStopMotor(VPAD_CHAN_0); }
void open_device(const std::string&) {}
bool write_motors(int low, int high) {
    const int m = low > high ? low : high;
    if (m <= 0) {
        VPADStopMotor(VPAD_CHAN_0);
        return true;
    }
    // 120-bit (15-byte, ~1 s at 120 Hz) PWM bitmask proportional to m / kSteps
    uint8_t pattern[15] = {};
    constexpr int kBits = 120;
    for (int i = 0; i < kBits; ++i) {
        if (((i * m) % kSteps) + m >= kSteps) pattern[i >> 3] |= static_cast<uint8_t>(0x80u >> (i & 7));
    }
    return VPADControlMotor(VPAD_CHAN_0, pattern, kBits) == 0;
}
#else
void close_device() {}
void open_device(const std::string&) {}
bool write_motors(int, int) { return false; }
#endif

int steps(float v) { return static_cast<int>(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * kSteps)); }
}  // namespace

void set(const std::string& guid, float low, float high) {
    State& s = g_state;
#if defined(__WIIU__)
    const std::string pad_guid = guid.empty() ? "vpad0" : guid;
    const float peak = std::fmax(low, high);
    const int l = peak > 0.05f ? steps(peak) : 0, h = l;
#else
    const std::string& pad_guid = guid;
    const int l = pad_guid.empty() ? 0 : steps(low), h = pad_guid.empty() ? 0 : steps(high);
#endif
    if (pad_guid != s.guid) {  // another pad (or none): the old one stilled, the new one looked for when it first vibrates
        if (s.low != 0 || s.high != 0) write_motors(0, 0);
        close_device();
        s = State{};
        s.guid = pad_guid;
    }
    if (pad_guid.empty()) return;
    const Clock::time_point now = Clock::now();
    const bool running = l != 0 || h != 0;
    if (l == s.low && h == s.high && (!running || now - s.last < kRefresh)) return;
    if (!s.tried && running) {
        s.tried = true;
        open_device(pad_guid);
    }
    write_motors(l, h);
    s.low = l;
    s.high = h;
    s.last = now;
}

void shutdown() {
    if (g_state.low != 0 || g_state.high != 0) write_motors(0, 0);
    close_device();
    g_state = State{};
}

}  // namespace rac1::rumble
