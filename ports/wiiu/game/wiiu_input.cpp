// The pads of the Wii U for the game's host (src/host/orig_main.cpp pad_packet): the GamePad (VPAD) and the Pro / Classic
// controllers (KPAD), as one gamepad in GLFW's layout (host_input.h HostState: the buttons by GLFW_GAMEPAD_BUTTON_*, the
// axes by GLFW_GAMEPAD_AXIS_*), so the bindings and the pad's packet are the PC's. The buttons by their place, as a PC
// pad's: the bottom one (Nintendo's B) is GLFW's A (the DualShock's cross), the right one (A) GLFW's B (circle), the left
// (Y) X (square), the top (X) Y (triangle). And ProcUI each frame: the HOME menu, the closing of the application.
#include "host/host_input.h"

#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <coreinit/time.h>
#include <vpad/input.h>
#include <whb/proc.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace hi = rac1::host_input;

namespace {
// GLFW's gamepad indices (GLFW/glfw3.h)
enum : int { kA = 0, kB, kX, kY, kLB, kRB, kBack, kStart, kGuide, kLThumb, kRThumb, kUp, kRight, kDown, kLeft };
enum : int { kLX = 0, kLY, kRX, kRY, kLT, kRT };

struct Map {
    uint32_t bit;
    int button;
};
constexpr Map kVpad[] = {{VPAD_BUTTON_B, kA},       {VPAD_BUTTON_A, kB},       {VPAD_BUTTON_Y, kX},          {VPAD_BUTTON_X, kY},
                         {VPAD_BUTTON_L, kLB},      {VPAD_BUTTON_R, kRB},      {VPAD_BUTTON_MINUS, kBack},   {VPAD_BUTTON_PLUS, kStart},
                         {VPAD_BUTTON_STICK_L, kLThumb}, {VPAD_BUTTON_STICK_R, kRThumb}, {VPAD_BUTTON_UP, kUp}, {VPAD_BUTTON_RIGHT, kRight},
                         {VPAD_BUTTON_DOWN, kDown}, {VPAD_BUTTON_LEFT, kLeft}};
// (the Pro Controller's and the Classic Controller's buttons have the same bits)
constexpr Map kPro[] = {{WPAD_PRO_BUTTON_B, kA},       {WPAD_PRO_BUTTON_A, kB},       {WPAD_PRO_BUTTON_Y, kX},          {WPAD_PRO_BUTTON_X, kY},
                        {WPAD_PRO_TRIGGER_L, kLB},     {WPAD_PRO_TRIGGER_R, kRB},     {WPAD_PRO_BUTTON_MINUS, kBack},   {WPAD_PRO_BUTTON_PLUS, kStart},
                        {WPAD_PRO_BUTTON_STICK_L, kLThumb}, {WPAD_PRO_BUTTON_STICK_R, kRThumb}, {WPAD_PRO_BUTTON_UP, kUp}, {WPAD_PRO_BUTTON_RIGHT, kRight},
                        {WPAD_PRO_BUTTON_DOWN, kDown}, {WPAD_PRO_BUTTON_LEFT, kLeft}};

// (the Minus button held, the menu's Exit: rac1_wiiu_frame says the game is to end. Atomic: the pads are read by the game thread
// while the main thread draws - orig_main.cpp's Wii U branch, runtime/ee/game_thread.h)
std::atomic<bool> g_quit_asked{false};
// ((2026-10-28) the GamePad's last sample read, for the debug menu while the game thread reads the pads: VPAD read by one thread at a
// time, the menu's touch from the game's own read - rac1_wiiu_last_vpad)
std::mutex g_last_m;
VPADStatus g_last_v{};
bool g_last_ok = false;
bool g_kpad = false;

// one pad's state merged into the host's (hi::merge_pad: the buttons OR'ed, the stick deflected furthest)
template <size_t N>
void merge(hi::HostState& st, const Map (&map)[N], uint32_t hold, float lx, float ly, float rx, float ry, bool zl, bool zr) {
    bool buttons[15] = {};
    for (const Map& m : map) buttons[m.button] = (hold & m.bit) != 0;
    // (the sticks up positive, GLFW's down positive; ZL / ZR digital: the triggers -1 or 1)
    const float axes[6] = {lx, -ly, rx, -ry, zl ? 1.0f : -1.0f, zr ? 1.0f : -1.0f};
    hi::merge_pad(st, buttons, axes);
    st.pad_present = true;
}
}  // namespace

// the pads' state this frame (the GamePad's and every KPAD controller's with a Pro / Classic layout, merged)
hi::HostState rac1_wiiu_pad_state() {
    hi::HostState st;
    // (a read with no new sample - VPAD samples every ~5 ms, KPAD less often, and a late host frame reads them once for each of
    // the game frames it runs, back to back - keeps the last state read: taken as "nothing held", a held button was released
    // and pressed again (a menu moving two places) and a short press could fall in a read that saw none)
    static VPADStatus v{};
    static bool v_ok = false;
    VPADStatus nv{};
    VPADReadError ve = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &nv, 1, &ve) == 1 && ve == VPAD_READ_SUCCESS) {
        v = nv;
        v_ok = true;
    } else if (ve != VPAD_READ_NO_SAMPLES) {
        v_ok = false;  // (the GamePad gone)
    }
    {
        std::lock_guard<std::mutex> lk(g_last_m);
        g_last_v = v;
        g_last_ok = v_ok;
    }
    // (Daino's quit: the GamePad's Minus held a second ends the game by itself - its main returns, the way back to the menu that works;
    // closing from the HOME menu hung the console, 2026-10-28)
    {
        static OSTime held_since = 0;
        const bool minus = v_ok && (v.hold & VPAD_BUTTON_MINUS) != 0;
        if (!minus) held_since = 0;
        else if (held_since == 0) held_since = OSGetTime();
        else if (!g_quit_asked.load() && OSTicksToMilliseconds(OSGetTime() - held_since) >= 1000) {
            g_quit_asked = true;
            VPADStopMotor(VPAD_CHAN_0);
            std::printf("[wiiu] Minus held: the game ends\n");
        }
    }
    if (v_ok)
        merge(st, kVpad, v.hold, v.leftStick.x, v.leftStick.y, v.rightStick.x, v.rightStick.y, (v.hold & VPAD_BUTTON_ZL) != 0, (v.hold & VPAD_BUTTON_ZR) != 0);
    if (!g_kpad) {
        KPADInit();
        WPADEnableURCC(TRUE);  // (the Pro Controller)
        g_kpad = true;
    }
    for (int c = 0; c < 4; ++c) {
        static KPADStatus last[4] = {};
        static bool last_ok[4] = {};
        KPADStatus nk{};
        KPADError ke = KPAD_ERROR_OK;
        if (KPADReadEx(static_cast<KPADChan>(c), &nk, 1, &ke) >= 1 && ke == KPAD_ERROR_OK) {
            last[c] = nk;
            last_ok[c] = true;
        } else if (ke != KPAD_ERROR_NO_SAMPLES) {
            last_ok[c] = false;  // (no controller on the channel)
        }
        if (!last_ok[c]) continue;
        const KPADStatus& k = last[c];
        if (k.extensionType == WPAD_EXT_PRO_CONTROLLER)
            merge(st, kPro, k.pro.hold, k.pro.leftStick.x, k.pro.leftStick.y, k.pro.rightStick.x, k.pro.rightStick.y, (k.pro.hold & WPAD_PRO_TRIGGER_ZL) != 0,
                  (k.pro.hold & WPAD_PRO_TRIGGER_ZR) != 0);
        else if (k.extensionType == WPAD_EXT_CLASSIC || k.extensionType == WPAD_EXT_MPLUS_CLASSIC)
            merge(st, kPro, k.classic.hold, k.classic.leftStick.x, k.classic.leftStick.y, k.classic.rightStick.x, k.classic.rightStick.y,
                  (k.classic.hold & WPAD_CLASSIC_BUTTON_ZL) != 0, (k.classic.hold & WPAD_CLASSIC_BUTTON_ZR) != 0);
    }
    return st;
}

// the game asked to end by itself (the GamePad debug menu's "Exit game"; the Minus button held does the same)
void rac1_wiiu_quit() {
    g_quit_asked = true;
    VPADStopMotor(VPAD_CHAN_0);
}

// the GamePad's last sample rac1_wiiu_pad_state read (false: none, or the GamePad gone)
bool rac1_wiiu_last_vpad(VPADStatus* out) {
    std::lock_guard<std::mutex> lk(g_last_m);
    if (g_last_ok) *out = g_last_v;
    return g_last_ok;
}

// a host frame's picture (wiiu_gfx.cpp) and ProcUI (the HOME menu handled meanwhile): false when the application is to close
void wiiu_gfx_frame();  // wiiu_gfx.cpp (nothing without the picture)
bool rac1_wiiu_frame() {
    wiiu_gfx_frame();
    return WHBProcIsRunning() && !g_quit_asked.load();
}
