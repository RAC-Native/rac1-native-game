// 1.02 Fixes and 1.02 Gameplay (see fixes102.h): the switches, and the pieces shared by more than one site or called from
// the generated code. Each fix's own change is at its site, in the North American port, behind fixes102_on(n) /
// gameplay102_on(n).
#include <game/enhance/fixes102.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>

#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
constexpr int FIXES[] = {1, 2, 3, 5, 6, 7, 11, 15, 16, 17, 18, 19, 20, 22, 26, 27, 28, 29, 30, 32, 33, 34, 35, 36, 37, 38, 45, 46, 49};  // the fixes of the 1.02 Fixes row (fixes102.h)
constexpr int GAMEPLAY[] = {4, 8, 9, 10, 12, 13, 14, 21, 23, 24, 25, 31};  // the fixes of the 1.02 Gameplay row (fixes102.h)

// orig: (none) — enhancement: a list variable's fixes (RAC1_FIXES102 / RAC1_GAMEPLAY102) as bits (bit n: fix n), `set`
// false when the variable is unset; `fixes` the row's
struct FixBits {
    bool set = false;
    std::bitset<128> bits;
    bool on(int fix) const { return fix >= 0 && fix < 128 && bits.test(static_cast<size_t>(fix)); }
};
template <size_t N>
FixBits env_list(const char* name, const int (&fixes)[N]) {
    FixBits out;
    const char* e = std::getenv(name);
    if (e == nullptr) return out;
    out.set = true;
    std::bitset<128>& bits = out.bits;
    const std::string list = e;
    size_t at = 0;
    while (at <= list.size()) {
        size_t end = list.find(',', at);
        if (end == std::string::npos) end = list.size();
        const std::string item = list.substr(at, end - at);
        at = end + 1;
        if (item.empty() || item == "none") continue;
        if (item == "all") {
            for (const int f : fixes) bits.set(static_cast<size_t>(f));
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
        bits.set(static_cast<size_t>(n));
    }
    return out;
}

// orig: (none) — enhancement: RAC1_FIXES102's fixes as bits
const FixBits& env_fixes() {
    static const FixBits v = env_list("RAC1_FIXES102", FIXES);
    return v;
}

// orig: (none) — enhancement: RAC1_GAMEPLAY102's fixes as bits
const FixBits& env_gameplay() {
    static const FixBits v = env_list("RAC1_GAMEPLAY102", GAMEPLAY);
    return v;
}
}  // namespace

// orig: (none) — enhancement (fixes102.h)
bool fixes102_on(int fix) {
    if (g_advanced_disabled) return false;
    const FixBits& env = env_fixes();
    if (env.set) return env.on(fix);
    if (!advanced_fixes102()) return false;
    for (const int f : FIXES)
        if (f == fix) return true;
    return false;
}

// orig: (none) — enhancement (fixes102.h)
bool gameplay102_on(int fix) {
    if (g_advanced_disabled) return false;
    const FixBits& env = env_gameplay();
    if (env.set) return env.on(fix);
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

// orig: (none) — enhancement (fixes102.h); the 1.02 build's cutscene_start (PAL disc 2.00 0x299b68): `lw 0x15f6a8` (the
// game mode) == 2: return, before the hero's test
bool fixes102_cutscene_refused() {
    return fixes102_on(15) && lw(OV(0x0015f5c4)) == 2;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's cutscene_start 0x299cd0: the underwater view's byte
// (0x167014) kept in the scene's +0x4b, the view cleared (here in FIX_UNDERWATER: the North American scene block's
// +0x46..+0x49 are in use)
void fixes102_cutscene_start() {
    if (!fixes102_on(15)) return;
    sw(FIX_UNDERWATER, lbu(OV(0x00167014)));
    sw(OV(0x00167014), 0);
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's cutscene_end 0x299edc: after the fade, its copy of
// draw_lists_reset (0x299108), then the underwater view put back from the scene's +0x4b
void fixes102_cutscene_end() {
    if (!fixes102_on(15)) return;
    draw_lists_reset();
    sw(OV(0x00167014), lw(FIX_UNDERWATER));
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L2:0x2d68d0 (PAL disc 2.00): the shark's place, 0.333
// higher, to enemy_death_burst(m, place, 0.333, 0)
void fixes102_sand_shark_burst(uint32_t m) {
    // (region: the PAL disc 2.00 is the 1.02 build, whose own shark has the burst (region_core_moved()): there it bursts
    // whatever the fix's row, as the disc's own code did while the shark was generated)
    if (!fixes102_on(18) && !region_core_moved()) return;
    const float k = u2f(0x3eaa7efau);
    const uint32_t at = ee_stack_alloc(0x10);
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), k));
    enemy_death_burst(m, at, k, 0.0f);
    ee_stack_free(0x10);
}

namespace {
// orig: (none) — enhancement: an EE string (the level's text) as a host string
std::string ee_string(uint32_t a) {
    std::string out;
    for (uint8_t c = lbu(a); c != 0; c = lbu(++a)) out += static_cast<char>(c);
    return out;
}
}  // namespace

// orig: (none) — enhancement (fixes102.h); the 1.02 build's Rilgar L5:0x263a64 (the poses: "x%d %s", 20855 / 20856 by
// the count 1), L5:0x263d00 (the flips: "%s + %d %s", 20647 at 360 / 20648), L5:0x266e88 (the place: "%s", 21028 + the
// place - 1, at most 21032) (PAL disc 2.00, its text ids 5 higher past 20695); the original's own texts (English: "x%d
// Poses", "%s + %d Flips", "% dst".."% dth") are written first, then this one over them
bool fixes102_race_text(uint32_t buf, int kind, uint32_t name, int32_t n) {
    // (region: a PAL build's disc, 0x15ee90 not 0, draws these texts itself, 1.00 as 2.00 (PAL 1.00 L5:0x262cb4, 0x262f38,
    // 0x2660ec, with the ids NTSC-U's): there they are written whatever the fix's row, the disc's own lines)
    if (!fixes102_on(19) && lw(0x0015ee90) == 0) return false;
    std::string t;
    if (kind == 0) {
        t = "x" + std::to_string(n) + " " + ee_string(sub_00203ad8(TXT(n != 1 ? 20856 : 20855)));
    } else if (kind == 1) {
        t = ee_string(name) + " + " + std::to_string(n) + " " + ee_string(sub_00203ad8(n != 360 ? 20648 : 20647));
    } else {
        int32_t i = n - 1;
        if (!(i < 5)) i = 4;
        t = ee_string(sub_00203ad8(TXT(21028 + i)));
    }
    for (size_t i = 0; i < t.size(); ++i) sb(buf + static_cast<uint32_t>(i), static_cast<uint8_t>(t[i]));
    sb(buf + static_cast<uint32_t>(t.size()), 0);
    return true;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L6:0x3011c4 (0x300278's new argument: `link` != -1, in the
// far break only) and L6:0x301754 (no debris loop with it 0)
bool fixes102_blarg_far_debris(int32_t link) {
    // (region: the PAL disc 2.00 is the 1.02 build, whose own break this is (region_core_moved()): there its rule applies
    // whatever the fix's row, as the disc's own code did while the class was generated)
    if (!fixes102_on(20) && !region_core_moved()) return true;
    return link != -1;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L6:0x2fb0b8 (PAL disc 2.00): the Snagglebeast's state 1 tests
// its countdown's flag (+0x170) first, the pump (+0xb4) and its target's class only while it is not set
bool fixes102_snagglebeast_start() {
    // (region: on the PAL disc 2.00, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(32) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L7:0x316e6c and L7:0x314f50 (PAL 2.00 disc): the Snagglebeast's
// walk flags tested with `& 4` and `& 2` in place of `== 4` and `== 2`
bool fixes102_umbris_snagglebeast_walk() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(33) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L8:0x2eb770 (PAL 2.00 disc): Batalia's class 467 retires its
// group (classes 0x24c..0x256 and 0x1d8) and itself once 0x13d3b6 is set while its link +0x8 is negative
bool fixes102_batalia_gate_group() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(34) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L11:0x30f4f8 (PAL 2.00 disc): Pokitaru's class 1158 sets
// 0x161200..0x161206 back to the sewer's underwater colours while 0x1677c8 <= 180
bool fixes102_pokitaru_sewer_tint() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(35) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L11:0x313d00 (PAL 2.00 disc): Pokitaru's fighter jet flight
// step drops the square-button toggle of pvar +0x60 and its second camera offset
bool fixes102_pokitaru_jet_camera() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(36) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L0:0x2a6b80 (PAL 2.00 disc): the Vendor's state 0 only sets
// the hologram moby's +0x73 to 0x20 when its class's byte +6 is non-zero
bool fixes102_vendor_holo_alpha() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(37) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L13:0x2b5b08 (PAL 2.00 disc): Gemlik Base's turret top (class
// 36, spawned by its base's L13:0x2b4958) gets 0.7 at +0x10 of its hit record
bool fixes102_gemlik_turret_aim() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(38) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L14:0x2f0050 (PAL 2.00 disc): Oltanis's gondola (class 685)
// starts its ride only while vec3_dot(its +0xe0 row, the hero's moby's +0xe0 row) > 0
bool fixes102_oltanis_gondola_upright() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(49) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L18:0x2f2d08 .. 0x2f2dac and L18:0x2f2c08 (PAL 2.00 disc):
// Veldin's finale's raft (class 1381) takes the hero for a rider after frame_scale_i(30) frames on it
bool fixes102_finale_raft_ride() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(45) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's L18:0x2f38d0 (PAL 2.00 disc): Veldin's finale's water (class
// 1402) carries a swimming Ratchet up with it
bool fixes102_finale_water_carry() {
    // (region: on the PAL 2.00 disc, the 1.02 build, whatever the fix's row: its own code)
    return fixes102_on(46) || region_core_moved();
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's decoy_object_update 0x2c5f10 (PAL disc 2.00): hero state
// 0x31 (swimming at the surface) and the decoy within 1.5 of Ratchet (xy): sub_00259bc8(m, m, 0x10000, Ratchet's
// place, the zero vector 0x15f580, 1000) before the decoy reads its events (0x30001)
void gameplay102_decoy_push(uint32_t m) {
    if (!gameplay102_on(14) || lw(g::HERO_STATE) != 0x31) return;
    if (!(vec2_distance(V(m + 0x10), V(g::PLAYER + 0x80)) < 1.5f)) return;
    sub_00259bc8(m, m, 0x10000, g::PLAYER + 0x80, OV(0x0015f580), 1000.0f);
}

// orig: (none) — enhancement (fixes102.h); fix 21 on, or the PAL 2.00 disc (its own build of class 831 does it: the
// port, rilgar_water.cpp, runs on every disc)
static bool water_fix21() { return gameplay102_on(21) || region_core_moved(); }

// orig: (none) — enhancement (fixes102.h); the 1.02 build's Rilgar L5:0x30dedc (PAL disc 2.00): game mode 2: return
bool gameplay102_water_held() {
    return water_fix21() && lw(OV(0x0015f5c4)) == 2;
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's Rilgar L5:0x30e4b0, after the water's seek toward its height
void gameplay102_water_carry(uint32_t m, float target) {
    if (!water_fix21()) return;
    if (!(u2f(0x3dcccccdu) < abs_f(fpu_sub(lwc1(m + 0x18), target)))) return;
    if (lw(g::PLAYER + 0x208c) - 0x11u >= 2u) return;
    const float z = lwc1(m + 0x18);
    if (lwc1(g::PLAYER + 0x2f0) < z) swc1(g::PLAYER + 0x2f0, z);
}

// orig: (none) — enhancement (fixes102.h); the 1.02 build's guard before its hud_message_request(7, ...) (Pokitaru
// 0x314b34, Gemlik 0x2bc910, Drek's fleet 0x2eeff4; on the PAL 2.00 disc Pokitaru's port runs on every disc)
bool gameplay102_talk_blocked() {
    if (!gameplay102_on(10) && !region_core_moved()) return false;
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
