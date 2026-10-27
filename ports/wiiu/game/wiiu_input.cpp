// The pads of the Wii U for the game's host (src/host/orig_main.cpp pad_packet): the GamePad (VPAD) and the Pro / Classic
// controllers (KPAD), as one gamepad in GLFW's layout (host_input.h HostState: the buttons by GLFW_GAMEPAD_BUTTON_*, the
// axes by GLFW_GAMEPAD_AXIS_*), so the bindings and the pad's packet are the PC's. The buttons by their place, as a PC
// pad's: the bottom one (Nintendo's B) is GLFW's A (the DualShock's cross), the right one (A) GLFW's B (circle), the left
// (Y) X (square), the top (X) Y (triangle). And ProcUI each frame: the HOME menu, the closing of the application.
#include "host/host_input.h"

#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>
#include <whb/proc.h>

#include <algorithm>
#include <cstdint>

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
    VPADStatus v{};
    VPADReadError ve = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &v, 1, &ve) == 1 && ve == VPAD_READ_SUCCESS)
        merge(st, kVpad, v.hold, v.leftStick.x, v.leftStick.y, v.rightStick.x, v.rightStick.y, (v.hold & VPAD_BUTTON_ZL) != 0, (v.hold & VPAD_BUTTON_ZR) != 0);
    if (!g_kpad) {
        KPADInit();
        WPADEnableURCC(TRUE);  // (the Pro Controller)
        g_kpad = true;
    }
    for (int c = 0; c < 4; ++c) {
        KPADStatus k{};
        KPADError ke = KPAD_ERROR_OK;
        if (KPADReadEx(static_cast<KPADChan>(c), &k, 1, &ke) < 1 || ke != KPAD_ERROR_OK) continue;
        if (k.extensionType == WPAD_EXT_PRO_CONTROLLER)
            merge(st, kPro, k.pro.hold, k.pro.leftStick.x, k.pro.leftStick.y, k.pro.rightStick.x, k.pro.rightStick.y, (k.pro.hold & WPAD_PRO_TRIGGER_ZL) != 0,
                  (k.pro.hold & WPAD_PRO_TRIGGER_ZR) != 0);
        else if (k.extensionType == WPAD_EXT_CLASSIC || k.extensionType == WPAD_EXT_MPLUS_CLASSIC)
            merge(st, kPro, k.classic.hold, k.classic.leftStick.x, k.classic.leftStick.y, k.classic.rightStick.x, k.classic.rightStick.y,
                  (k.classic.hold & WPAD_CLASSIC_BUTTON_ZL) != 0, (k.classic.hold & WPAD_CLASSIC_BUTTON_ZR) != 0);
    }
    return st;
}

// a host frame's picture (wiiu_gfx.cpp) and ProcUI (the HOME menu handled meanwhile): false when the application is to close
void wiiu_gfx_frame();  // wiiu_gfx.cpp (nothing without the picture)
bool rac1_wiiu_frame() {
    wiiu_gfx_frame();
    return WHBProcIsRunning();
}
