// 1.02 Fixes (see fixes102.h): the switch, and the pieces shared by more than one site. Each fix's own change is at its
// site, in the North American port, behind fixes102_on(n).
#include <game/enhance/fixes102.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
constexpr int FIXES[] = {1, 2, 3, 5, 6, 7, 11, 16};  // the fixes of this row (fixes102.h)

// orig: (none) — enhancement: RAC1_FIXES102's fixes as bits (bit n: fix n); -1 unset
int64_t env_fixes() {
    static const int64_t v = [] {
        const char* e = std::getenv("RAC1_FIXES102");
        if (e == nullptr) return int64_t{-1};
        int64_t bits = 0;
        const std::string list = e;
        size_t at = 0;
        while (at <= list.size()) {
            size_t end = list.find(',', at);
            if (end == std::string::npos) end = list.size();
            const std::string item = list.substr(at, end - at);
            at = end + 1;
            if (item.empty() || item == "none") continue;
            if (item == "all") {
                for (const int f : FIXES) bits |= int64_t{1} << f;
                continue;
            }
            char* stop = nullptr;
            const long n = std::strtol(item.c_str() + (item[0] == 'f' || item[0] == 'F' ? 1 : 0), &stop, 10);
            bool known = false;
            for (const int f : FIXES) known = known || f == n;
            if (*stop != 0 || !known) {
                std::fprintf(stderr, "RAC1_FIXES102: %s is not one of the fixes (1, 2, 3, 5, 6, 7, 11, 16, all, none)\n", item.c_str());
                continue;
            }
            bits |= int64_t{1} << n;
        }
        return bits;
    }();
    return v;
}
}  // namespace

// orig: (none) — enhancement (fixes102.h)
bool fixes102_on(int fix) {
    if (g_advanced_disabled) return false;
    const int64_t env = env_fixes();
    if (env >= 0) return ((env >> fix) & 1) != 0;
    if (!advanced_fixes102()) return false;
    for (const int f : FIXES)
        if (f == fix) return true;
    return false;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's gameplay_load clears its ring (20 words) and the index
void fixes102_level_load() {
    if (!fixes102_on(3)) return;
    for (uint32_t i = 0; i < 20; ++i) sw(FIX_RING + i * 4, 0);
    sw(FIX_RING_NEXT, 0);
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's lines in hero_vertical_velocity (a jump, state 0xe, with
// 0x13d4c2 set) and hero_horizontal_velocity (states 0xb / 0xc): not on Oltanis (level 14), the item on the back
// (slot 3) the Thruster-Pack (kind 3), the state's frame (+0x198) 5
void fixes102_thruster_jump_sound() {
    if (!fixes102_on(5)) return;
    if (lw(0x0015ed84) == 0xe) return;
    if (sub_0020d498(3) != 3) return;
    if (lw(g::PLAYER + 0x198) != frame_scale_i(5)) return;
    hero_sound_start(1, 0);
}

}  // namespace rac1
