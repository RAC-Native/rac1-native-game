// src/host/rumble.cpp — the gamepad's vibration motors (see rumble.h). Host code, not a port.
#include "rumble.h"

#include "dualsense.h"

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
#endif

namespace rac1::rumble {

namespace {
using Clock = std::chrono::steady_clock;

struct State {
    std::string guid;           // the pad driven ("" none)
    int low = 0, high = 0;      // the strengths last set, in 64ths
    Clock::time_point last{};   // when they were last set
    bool tried = false;         // the device looked for (once per pad)
    bool hid = false;           // a DualSense driven by its output report (dualsense.h), else the motors below
    bool triggers_held = false; // its triggers' effects set (given back when the setting goes off or the pad changes)
    bool light_held = false;    // its light bar set
#if defined(__linux__)
    int fd = -1;
    int effect = -1;
#endif
} g_state;

constexpr int kSteps = 64;
constexpr auto kRefresh = std::chrono::milliseconds(500);
constexpr uint16_t kLengthMs = 1000;

#if defined(__linux__)
// orig: (host) bit `n` of a bit array
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

// orig: (host) the event device's effect removed, the device closed
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

// orig: (host) the motors set (in 64ths): the effect uploaded and played, and stopped at 0
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
    // played even at 0, then stopped: ff-memless (most pad drivers) restarts a playing effect after an upload at its
    // next tick, and a stop written before that tick is taken as the stop of an effect not playing, which sends
    // nothing to the pad (it kept its last strength). Played, the 0 is sent at once; the stop then ends the effect.
    input_event ev{};
    ev.type = EV_FF;
    ev.code = static_cast<uint16_t>(g_state.effect);
    ev.value = 1;
    bool ok = write(g_state.fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev);
    if (low == 0 && high == 0) {
        ev.value = 0;
        ok = write(g_state.fd, &ev, sizeof ev) == static_cast<ssize_t>(sizeof ev) && ok;
    }
    return ok;
}
#elif defined(_WIN32)
struct XInputVibration {
    WORD left, right;
};
using SetStateFn = DWORD(WINAPI*)(DWORD, XInputVibration*);
// orig: (host) XInputSetState, loaded when first needed
SetStateFn xinput_set() {
    static SetStateFn fn = [] {
        HMODULE m = LoadLibraryA("xinput1_4.dll");
        if (m == nullptr) m = LoadLibraryA("xinput9_1_0.dll");
        return m != nullptr ? reinterpret_cast<SetStateFn>(GetProcAddress(m, "XInputSetState")) : nullptr;
    }();
    return fn;
}
// orig: (host) (XInput: no device of its own)
void close_device() {}
// orig: (host) (XInput: no device of its own)
void open_device(const std::string&) {}
// orig: (host) the motors set (in 64ths) on every XInput pad
bool write_motors(int low, int high) {
    SetStateFn fn = xinput_set();
    if (fn == nullptr) return false;
    XInputVibration v{static_cast<WORD>(low * 0xffff / kSteps), static_cast<WORD>(high * 0xffff / kSteps)};
    bool any = false;
    for (DWORD i = 0; i < 4; ++i) any |= fn(i, &v) == ERROR_SUCCESS;  // (ERROR_DEVICE_NOT_CONNECTED for the empty slots)
    return any;
}
#else
// orig: (host) (none elsewhere)
void close_device() {}
// orig: (host) (none elsewhere)
void open_device(const std::string&) {}
// orig: (host) (none elsewhere)
bool write_motors(int, int) { return false; }
#endif

// orig: (host) a strength in 64ths
int steps(float v) { return static_cast<int>(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * kSteps)); }
// orig: (host) a strength in 255ths
uint8_t byte(float v) { return static_cast<uint8_t>(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255.0f)); }

// orig: (host) the DualSense's report for `out` written: the triggers' effects and the light bar while the setting is
// on, given back to neutral (effects off, the pad's own colour) once when it goes off
void hid_write(const Output& out) {
    State& s = g_state;
    dualsense::State d;
    d.left = byte(out.left);
    d.right = byte(out.right);
    if (out.dualsense || s.triggers_held) {
        d.triggers = true;
        if (out.dualsense) {
            d.r2 = dualsense::trigger(out.r2.kind, out.r2.a, out.r2.b, out.r2.c);
            d.l2 = dualsense::trigger(out.l2.kind, out.l2.a, out.l2.b, out.l2.c);
        }
    }
    const bool light = out.dualsense && out.health >= 0.0f;
    if (light) {  // the health's colour: green when full, through yellow to red, a little dimmed
        const float h = std::fmin(std::fmax(out.health, 0.0f), 1.0f);
        d.lightbar = true;
        d.red = byte(0.75f * std::fmin(1.0f, 2.0f * (1.0f - h)));
        d.green = byte(0.75f * std::fmin(1.0f, 2.0f * h));
        d.blue = 0;
    } else if (s.light_held) {
        d.lightbar = true;
        dualsense::lightbar_neutral(d.red, d.green, d.blue);
    }
    if (!dualsense::write(d)) return;
    s.triggers_held = out.dualsense;
    s.light_held = light;
}

// orig: (host) the pad driven let go: motors still, triggers and light bar given back, the device closed
void release() {
    State& s = g_state;
    if (s.hid) {
        hid_write(Output{});
        dualsense::close();
    } else {
        if (s.low != 0 || s.high != 0) write_motors(0, 0);
        close_device();
    }
}
}  // namespace

// orig: (host) (rumble.h)
void set(const std::string& guid, const Output& out) {
    State& s = g_state;
    if (guid != s.guid) {  // another pad (or none): the old one stilled, the new one looked for when it first vibrates
        release();
        s = State{};
        s.guid = guid;
    }
    if (guid.empty()) return;
    const int l = steps(out.low), h = steps(out.high);
    const bool running = l != 0 || h != 0 || out.left > 0.0f || out.right > 0.0f;
    if (!s.tried && (running || out.dualsense)) {
        s.tried = true;
        s.hid = dualsense::is_dualsense(guid) && dualsense::open(guid);
        if (!s.hid) open_device(guid);
    }
    if (s.hid) {
        hid_write(out);
        return;
    }
    const Clock::time_point now = Clock::now();
    if (l == s.low && h == s.high && (!(l != 0 || h != 0) || now - s.last < kRefresh)) return;
    if (!s.tried) return;  // (never vibrated: nothing to stop)
    write_motors(l, h);
    s.low = l;
    s.high = h;
    s.last = now;
}

// orig: (host) (rumble.h)
void shutdown() {
    release();
    g_state = State{};
}

}  // namespace rac1::rumble
