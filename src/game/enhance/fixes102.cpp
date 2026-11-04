// 1.02 Fixes and 1.02 Gameplay (see fixes102.h): the switches, and the pieces shared by more than one site or called from
// the generated code. Each fix's own change is at its site, in the North American port, behind fixes102_on(n) /
// gameplay102_on(n).
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
constexpr int FIXES[] = {1, 2, 3, 5, 6, 7, 11, 16};  // the fixes of the 1.02 Fixes row (fixes102.h)
constexpr int GAMEPLAY[] = {4, 8, 9, 10, 12, 13};    // the fixes of the 1.02 Gameplay row (fixes102.h)

// orig: (none) — enhancement: a list variable's fixes (RAC1_FIXES102 / RAC1_GAMEPLAY102) as bits (bit n: fix n); -1
// unset; `fixes` the row's
template <size_t N>
int64_t env_list(const char* name, const int (&fixes)[N]) {
    const char* e = std::getenv(name);
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
            for (const int f : fixes) bits |= int64_t{1} << f;
            continue;
        }
        char* stop = nullptr;
        const long n = std::strtol(item.c_str() + (item[0] == 'f' || item[0] == 'F' ? 1 : 0), &stop, 10);
        bool known = false;
        for (const int f : fixes) known = known || f == n;
        if (*stop != 0 || !known) {
            std::string list_text;
            for (const int f : fixes) list_text += std::to_string(f) + ", ";
            std::fprintf(stderr, "%s: %s is not one of the fixes (%sall, none)\n", name, item.c_str(), list_text.c_str());
            continue;
        }
        bits |= int64_t{1} << n;
    }
    return bits;
}

// orig: (none) — enhancement: RAC1_FIXES102's fixes as bits; -1 unset
int64_t env_fixes() {
    static const int64_t v = env_list("RAC1_FIXES102", FIXES);
    return v;
}

// orig: (none) — enhancement: RAC1_GAMEPLAY102's fixes as bits; -1 unset
int64_t env_gameplay() {
    static const int64_t v = env_list("RAC1_GAMEPLAY102", GAMEPLAY);
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

// orig: (none) — enhancement (fixes102.h)
bool gameplay102_on(int fix) {
    if (g_advanced_disabled) return false;
    const int64_t env = env_gameplay();
    if (env >= 0) return ((env >> fix) & 1) != 0;
    if (!advanced_gameplay102()) return false;
    for (const int f : GAMEPLAY)
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

// orig: (none) — enhancement (fixes102.h); the 1.02 build's guard before its hud_message_request(7, ...) (Pokitaru
// 0x314b34, Gemlik 0x2bc910, Drek's fleet 0x2eeff4)
bool gameplay102_talk_blocked() {
    if (!gameplay102_on(10)) return false;
    const int32_t st = lw(g::HERO_STATE);
    return st == 0x32 || st == 0x1d || lbu(g::PLAYER + 0x20a4) != 0;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's guard before talk_update (Rilgar 0x3180a8, Kalebo III
// 0x2e7dc0)
bool gameplay102_talk_held() {
    return gameplay102_on(10) && lw(g::HERO_STATE) == 0x1d;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's sb 1, +0x20ac (Rilgar 0x318150, Kalebo III 0x2e7f70)
void gameplay102_talk_lock() {
    if (gameplay102_on(10)) sb(g::PLAYER + 0x20ac, 1);
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's 0x231e3c (hero_input_update, state 0x53)
bool gameplay102_deadly_ground() {
    return gameplay102_on(9) && lbu(g::PLAYER + 0x12ea) != 0;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's 0x231f38 (state 0x59, 0.3) / 0x2320d0 (state 0x54, 0.2)
bool gameplay102_deadly_ground_near(float h) {
    return gameplay102_deadly_ground() && lwc1(g::PLAYER + 0x2dc) < h;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's hero_set_state 0x2276d8 (state 0x53) / 0x227814 (0x54)
bool gameplay102_deadly_landing(uint32_t sub) {
    if (!gameplay102_deadly_ground() || lh(g::PLAYER + 0x30c) != 0) return false;
    sw(g::PLAYER + 0x208c, sub);
    sw(g::PLAYER + 0x2284, 0);
    hero_set_state(0x58, 1);
    return true;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's 0x22c898 (hero_input_update, state 0x3f)
int gameplay102_board_ride_off() {
    if (!gameplay102_on(9)) return 0;
    const float q = u2f(0x3f490fdbu);
    const bool off = lw(g::PLAYER + 0x300) != 0 && lbu(g::PLAYER + 0x12e7) == 0 && abs_f(lwc1(g::PLAYER + 0x94)) < q &&
                     abs_f(lwc1(g::PLAYER + 0x90)) < q;
    return off ? 1 : 2;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's 0x20c514 (hero_surface_update)
bool gameplay102_quicksand_lost() {
    if (!gameplay102_on(9) || lbu(g::PLAYER + 0x20a4) - 1u >= 2u) return false;
    hero_lost();
    return true;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's 0x222d60 (hero_set_state)
void gameplay102_mode3_left(uint32_t state) {
    if (gameplay102_on(13)) sw(g::PLAYER + 0x2084, state);
}

// orig: (none) — enhancement (fixes102.h): the 1.02 build's RYNO target search (PAL 2.00 0x2cd7b0), the North American
// 0x2cc360 (ryno_asm.cpp) with its two changes: the mobys from the target list 0x1aba00 (0-terminated, the mobys of flag
// 0x1000 moby_update queues) instead of the whole moby list 0x15ffe4 (+0x28 the next), and a target whose line from
// `from` is cut by a moby of type 5 or 0xf (the segment's hit moby 0x173e40 + 0x18, its class +0x24, type +0x46) taken
// all the same. The rest is the original's: a moby drawn (+0x32), not in the 7 of `skip` (or, without `skip`, visible:
// +0x31), with a pvars word whose first float is not negative, of flag 0x1000 and type 5, nearer than `range` in xy;
// aimed at its point 0.1 over the pvars word's +0x10, within `yaw_tol` of `aim` +8 and `pitch_tol` of `aim` +4; the
// lowest score (the distance / 5 within 5, else distance x (1 + yaw diff^2 x pitch diff^2)) wins.
int32_t fixes102_ryno_targets(uint32_t from, uint32_t aim, uint32_t skip, float yaw_tol, float pitch_tol, float range) {
    uint32_t best = 0;
    float best_score = u2f(0x4e6e6b28u);
    for (uint32_t i = 0;; ++i) {
        const uint32_t m = lwu(OV(0x001aba00) + i * 4);
        if (m == 0) break;
        if (lh(m + 0x32) == 0) continue;
        if (skip != 0) {
            bool listed = false;
            for (uint32_t k = 0; k < 7 && !listed; ++k) listed = lwu(skip + k * 4) == m;
            if (listed) continue;
        } else if (lbu(m + 0x31) == 0) {
            continue;
        }
        const uint32_t pv = moby_pvar_word0(m);
        if (pv == 0 || lwc1(pv) < 0.0f) continue;
        if ((lhu(m + 0x34) & 0x1000) == 0) continue;
        const uint32_t cls = lwu(m + 0x24);
        if (cls == 0 || lh(cls + 0x46) != 5) continue;
        const float dist = vec2_distance(V(from), V(m + 0x10));
        if (!(dist < range)) continue;
        Vec4 at = V(m + 0x10);
        at.z = fpu_add(at.z, fpu_add(lwc1(pv + 0x10), u2f(0x3dcccccdu)));
        const float dy = angle_dist(lwc1(aim + 8), atan2_f(fpu_sub(at.x, lwc1(from)), fpu_sub(at.y, lwc1(from + 4))));
        float score = fpu_mul(dy, dy);
        if (!(score < fpu_mul(yaw_tol, yaw_tol))) continue;
        const float dp = angle_dist(lwc1(aim + 4), atan2_f(dist, fpu_sub(at.z, lwc1(from + 8))));
        const float dp2 = fpu_mul(dp, dp);
        if (!(dp2 < fpu_mul(pitch_tol, pitch_tol))) continue;
        if (5.0f < dist) score = fpu_add(fpu_mul(fpu_mul(score, dp2), dist), dist);
        else score = fpu_div(dist, 5.0f);
        if (!(score < best_score)) continue;
        if (segment_collide_v(V(from), at, 2, m, nullptr) != 0) {
            const uint32_t hit = lwu(OV(0x00173e40) + 0x18);
            if (hit == 0) continue;
            const uint32_t hc = lwu(hit + 0x24);
            if (hc == 0) continue;
            const int32_t type = lh(hc + 0x46);
            if (type != 5 && type != 0xf) continue;
        }
        best_score = score;
        best = m;
    }
    return static_cast<int32_t>(best);
}

}  // namespace rac1
