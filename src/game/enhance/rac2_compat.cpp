// See rac2_compat.h: the engine services RAC2's arena behaviours ask for, said with RAC1's engine.
#include <host/rac2_compat.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1::rac2 {

namespace {

constexpr uint32_t PAD = 0x0013c940;   // pad 1: +0x1a0 held, +0x1a4 pressed, +0x1a8 released, +0x100..+0x10c the sticks
constexpr uint32_t HERO = 0x0013f350;  // the player: +0x2080 the hero's moby, +0x10b8 the item in hand, +0x20a4 Clank
constexpr uint32_t NANOTECH = 0x001415f8, NANOTECH_MAX = 0x0015eda0, BOLTS = 0x0015ed98;

constexpr uint32_t kPool = 0x000c0000, kBlock = 0x280, kBlocks = 64;  // (the stand-ins' pvars: rac2_compat.h)
bool g_taken[kBlocks] = {};

uint32_t g_rng = 0x9e3779b9u;

struct Turn {
    int32_t rac2_cls;
    uint32_t m, pvars;
    int32_t cls;  // (its RAC1 class: gone when the slot holds another)
};
std::vector<Turn> g_turns;

// orig: (none) — the turns of the dead (or of slots gone to another moby) taken back
void turns_prune() {
    g_turns.erase(std::remove_if(g_turns.begin(), g_turns.end(), [](const Turn& t) { return moby_gone(t.m, t.cls, t.pvars); }), g_turns.end());
}

}  // namespace

// orig: (none)
uint32_t level_frame() { return lwu(OV(0x0015f5cc)); }

// orig: (none)
bool in_play() { return lw(OV(0x0015f5c4)) == 0; }

// orig: (none)
Vec3 moby_pos(uint32_t m) { return Vec3{lwc1(m + moby::kPos), lwc1(m + moby::kPos + 4), lwc1(m + moby::kPos + 8)}; }

// orig: (none) — the list from *(0x15ffd8) to its end marker (state 0xff)
uint32_t moby_by_uid(int32_t uid) {
    const uint32_t first = lwu(OV(0x0015ffd8));
    if (first == 0) return 0;
    for (uint32_t m = first; lbu(m + moby::kState) != 0xff; m += 0x100)
        if (static_cast<int8_t>(lbu(m + moby::kState)) >= 0 && lh(m + moby::kUid) == uid) return m;
    return 0;
}

// orig: (none)
void moby_show(uint32_t m, bool shown) {
    const uint32_t f = lhu(m + moby::kFlags);
    sh(m + moby::kFlags, shown ? f & ~moby::kHidden : f | moby::kHidden);
}

// orig: (none)
bool moby_gone(uint32_t m, int32_t cls, uint32_t pvars) {
    return static_cast<int8_t>(lbu(m + moby::kState)) < 0 || lh(m + moby::kClassId) != cls || lwu(m + moby::kPvars) != pvars;
}

// orig: (none)
void moby_kill(uint32_t m) { sb(m + moby::kState, 0xfe); }

// orig: (none) — RAC2's controller hands its recycled enemies over to their class's activate function, which starts them
// from their first state; ours copies a template that was never updated, so the copy starts as a freshly loaded one
uint32_t standin_spawn(uint32_t tmpl, uint32_t pv_size, Vec3 at, float yaw) {
    if (tmpl == 0 || pv_size > kBlock) return 0;
    uint32_t k = 0;
    while (k < kBlocks && g_taken[k]) ++k;
    if (k == kBlocks) return 0;
    const uint32_t m = moby_spawn(static_cast<uint32_t>(lh(tmpl + moby::kClassId)));
    if (m == 0) return 0;
    g_taken[k] = true;
    const uint32_t pv = kPool + k * kBlock;
    // the template's record, but the new slot's own index words and pvars
    const uint32_t slot = lwu(m + moby::kSlot), index = lwu(m + moby::kSlotIndex);
    for (uint32_t o = 0; o < 0x100; o += 4) sw(m + o, lwu(tmpl + o));
    sw(m + moby::kSlot, slot);
    sw(m + moby::kSlotIndex, index);
    const uint32_t tpv = lwu(tmpl + moby::kPvars);
    for (uint32_t o = 0; o < kBlock; o += 4) sw(pv + o, o < pv_size && tpv != 0 ? lwu(tpv + o) : 0u);
    sw(m + moby::kPvars, pv);
    swc1(m + moby::kPos, at.x);
    swc1(m + moby::kPos + 4, at.y);
    swc1(m + moby::kPos + 8, at.z);
    swc1(m + moby::kRot + 8, yaw);
    moby_show(m, true);
    if (lw(m + moby::kClass) != 0) moby_update_transform(m);
    return m;
}

// orig: (none)
void standin_release(uint32_t pvars) {
    if (pvars < kPool || pvars >= kPool + kBlocks * kBlock) return;
    g_taken[(pvars - kPool) / kBlock] = false;
}

// orig: (none)
void standin_reset() {
    std::memset(g_taken, 0, sizeof g_taken);
    g_turns.clear();
}

// orig: (none)
void seq_play(uint32_t m, uint32_t seq, int32_t blend, int32_t from) {
    if (lbu(m + moby::kNextSeq) == seq) return;
    moby_anim_set(m, seq, from, blend > 0 ? blend : 1);  // (RAC1's takes at least one frame)
}

// orig: (none) — RAC2's 0x3146d0: each frame record's +4 halfword is its time (sixteenths); the frame's and the next one's,
// mixed by how far the step is between them (RAC1's frame records are RAC2's: the converter keeps them)
float seq_time(uint32_t m) {
    const uint32_t seq = lwu(lwu(m + moby::kClass) + lbu(m + moby::kNextSeq) * 4u + 0x48);
    if (seq == 0) return 0.0f;
    const uint32_t n = lbu(seq + 0x10);
    auto time_of = [&](uint32_t k) { return static_cast<float>(lh(lwu(seq + 0x1c + 4u * std::min(k, n - 1)) + 4)) * 0.0625f; };
    if (lbu(m + 0x52) != lbu(m + moby::kNextSeq)) return time_of(lbu(m + 0x51));
    const float t = lwc1(m + 0x54);
    return time_of(lbu(m + 0x50)) * (1.0f - t) + time_of(lbu(m + 0x51)) * t;
}

// orig: (none) — RAC1's +0x50 the frame being left, +0x51 the next, +0x54 the way between (while a blend runs: the next)
float seq_frame(uint32_t m) {
    if (lbu(m + 0x52) != lbu(m + moby::kNextSeq)) return static_cast<float>(lbu(m + 0x51));
    return static_cast<float>(lbu(m + 0x50)) + lwc1(m + 0x54);
}

// orig: (none)
uint32_t seq_now(uint32_t m) { return lbu(m + moby::kNextSeq); }

// orig: (none) — moby_anim_advance's flags +0x70, bit 1
bool seq_ended(uint32_t m) { return (lbu(m + moby::kAnimFlags) & 2u) != 0; }

// orig: (none)
void joints_world(uint32_t m, uint32_t first, uint32_t n, Vec3* out) {
    n = std::min<uint32_t>(n, 8);
    if (n == 0 || lwu(lwu(m + moby::kClass) + 0x1c) == 0) {  // (no attachment points: its position)
        for (uint32_t k = 0; k < n; ++k) out[k] = moby_pos(m);
        return;
    }
    const uint32_t list = ee_stack_alloc(0x20), pos = ee_stack_alloc(0x80);
    for (uint32_t k = 0; k < n; ++k) sw(list + 4 * k, first + k);  // (words: the class's attachment points, +0x1c)
    moby_joint_positions(m, static_cast<int32_t>(n), list, pos);
    for (uint32_t k = 0; k < n; ++k) out[k] = Vec3{lwc1(pos + 0x10 * k), lwc1(pos + 0x10 * k + 4), lwc1(pos + 0x10 * k + 8)};
    ee_stack_free(0x80);
    ee_stack_free(0x20);
}

// orig: (none) — the collision's segment test (flags 2: the ground), its hit point's height 0x173e68
float ground_under(uint32_t m, float lowest) {
    const Vec3 at = moby_pos(m);
    const Vec4 a{at.x, at.y, at.z + 1.0f, 1.0f}, b{at.x, at.y, lowest, 1.0f};
    if (segment_collide_v(a, b, 2, m, nullptr) != 0) return std::max(lowest, lwc1(OV(0x00173e68)));
    return lowest;
}

// orig: (none)
void test_hit(uint32_t m) {
    static const std::vector<uint32_t> at = [] {
        std::vector<uint32_t> v;
        const char* e = std::getenv("RAC1_ARENA_ENEMY_HIT");
        for (const char* q = e; q != nullptr && *q != 0;) {
            char* end = nullptr;
            v.push_back(static_cast<uint32_t>(std::strtoul(q, &end, 10)));
            q = *end == ',' ? end + 1 : nullptr;
        }
        return v;
    }();
    if (std::find(at.begin(), at.end(), level_frame()) == at.end()) return;
    uint32_t rec[9] = {};
    Vec4 push = V(m + moby::kPos);
    push.x -= lwc1(HERO + 0x80);  // (away from the hero, as the blaster's)
    push.y -= lwc1(HERO + 0x84);
    push.z = 0.0f;
    vec2_set_length(push, push, 1.0f);
    std::memcpy(rec, &push, 16);
    rec[5] = 0x00010001u;  // (+0x14: the hit's kinds)
    const float damage = 1.0f;
    std::memcpy(&rec[7], &damage, 4);
    rec[8] = 1;
    const int32_t n = mobys_in_sphere_v(V(m + moby::kPos), 1.0f, 0x10, 0, rec);
    std::printf("[rac2] frame %u: test blast on %#x (class %d): %d moby(s) touched\n", level_frame(), m, lh(m + moby::kClassId), n);
}

// orig: (none)
float hit_receive(uint32_t m, uint32_t hs, Vec3* push) {
    const uint32_t hit = sub_0025a420(m, 0x00330000u, 0);
    float dmg = 0.0f;
    int32_t state = 0;
    moby_hit_react_r(m, hit, hs, 0, state, &dmg, nullptr, 4);
    sb(m + 0xa4, 0xff);
    if (hit == 0 || state == 1 || !(dmg > 0.0f)) return 0.0f;
    if (push != nullptr) *push = Vec3{lwc1(hit + 0x10), lwc1(hit + 0x14), lwc1(hit + 0x18)};
    return dmg;
}

namespace part {
// a part's own pvars (moby_spawn's block, zeroed)
constexpr uint32_t kOwner = 0x00, kOwnerPvars = 0x04, kOwnerClass = 0x08, kThrown = 0x0c, kVel = 0x10, kLie = 0x1c, kFloor = 0x20,
                   kSpin = 0x24, kBounced = 0x28;
}  // namespace part

// orig: (none)
uint32_t part_spawn(uint32_t m, int32_t cls, float scale) {
    const uint32_t w = moby_spawn(static_cast<uint32_t>(cls));
    if (w == 0) return 0;
    swc1(w + 0x2c, lwc1(w + 0x2c) * scale);
    // (its rows set by us: RAC1's "keep the rows"; updated: a class without sequences is spawned not updated; its draw
    // distance and +0x30 as its carrier's: moby_spawn leaves them 0, not drawn)
    sh(w + moby::kFlags, (lhu(w + moby::kFlags) | 0x100u) & ~2u);
    sh(w + 0x32, lhu(m + 0x32));
    sb(w + 0x30, lbu(m + 0x30));
    const uint32_t pv = lwu(w + moby::kPvars);
    if (pv != 0) {
        sw(pv + part::kOwner, m);
        sw(pv + part::kOwnerPvars, lwu(m + moby::kPvars));
        sw(pv + part::kOwnerClass, static_cast<uint32_t>(lh(m + moby::kClassId)));
    }
    return w;
}

// orig: (none)
bool part_of(uint32_t w, uint32_t m) {
    if (w == 0 || static_cast<int8_t>(lbu(w + moby::kState)) < 0) return false;
    const uint32_t pv = lwu(w + moby::kPvars);
    return pv != 0 && lwu(pv + part::kOwner) == m && lw(pv + part::kThrown) == 0;
}

// orig: (none)
void part_place(uint32_t w, uint32_t m, uint32_t point, float spin) {
    Mat4 at;
    moby_chain_world_one_v(m, point, at);
    Vec4 r0 = at.r[0], r1 = at.r[1], r2 = at.r[2];
    vec3_set_length(r0, r0, 1.0f);
    vec3_set_length(r1, r1, 1.0f);
    vec3_set_length(r2, r2, 1.0f);
    const float c = std::cos(spin), s = std::sin(spin);
    V(w + 0xc0) = Vec4{c * r0.x + s * r1.x, c * r0.y + s * r1.y, c * r0.z + s * r1.z, 0.0f};
    V(w + 0xd0) = Vec4{c * r1.x - s * r0.x, c * r1.y - s * r0.y, c * r1.z - s * r0.z, 0.0f};
    V(w + 0xe0) = Vec4{r2.x, r2.y, r2.z, 0.0f};
    swc1(w + moby::kPos, at.r[3].x);
    swc1(w + moby::kPos + 4, at.r[3].y);
    swc1(w + moby::kPos + 8, at.r[3].z);
    moby_update_transform(w);
}

// orig: (none)
void part_throw(uint32_t w, Vec3 vel, float floor, uint32_t lie) {
    const uint32_t pv = w != 0 ? lwu(w + moby::kPvars) : 0u;
    if (pv == 0) return;
    sw(pv + part::kThrown, 1);
    swc1(pv + part::kVel, vel.x);
    swc1(pv + part::kVel + 4, vel.y);
    swc1(pv + part::kVel + 8, vel.z);
    sw(pv + part::kLie, lie);
    swc1(pv + part::kFloor, floor);
    swc1(pv + part::kSpin, 0.0f);
    sw(pv + part::kBounced, 0);
}

// orig: (none) — RAC2's 0x32b468 said simply: gravity 0.012 a frame, a bounce at half the speed, then it lies still
void part_update(uint32_t w) {
    const uint32_t pv = lwu(w + moby::kPvars);
    if (pv == 0 || lwu(pv + part::kOwner) == 0) return;  // (a placed one: none are)
    if (lw(pv + part::kThrown) == 0) {
        if (moby_gone(lwu(pv + part::kOwner), lw(pv + part::kOwnerClass), lwu(pv + part::kOwnerPvars))) moby_kill(w);
        return;
    }
    Vec3 v{lwc1(pv + part::kVel), lwc1(pv + part::kVel + 4), lwc1(pv + part::kVel + 8)};
    Vec3 p = moby_pos(w);
    const float floor = lwc1(pv + part::kFloor);
    const bool resting = lw(pv + part::kBounced) >= 2;
    if (!resting) {
        v.z -= 0.012f;
        p.x += v.x;
        p.y += v.y;
        p.z += v.z;
        if (p.z <= floor) {
            p.z = floor;
            sw(pv + part::kBounced, lw(pv + part::kBounced) + 1);
            v = Vec3{v.x * 0.5f, v.y * 0.5f, -v.z * 0.4f};
        }
        swc1(pv + part::kVel, v.x);
        swc1(pv + part::kVel + 4, v.y);
        swc1(pv + part::kVel + 8, v.z);
        swc1(w + moby::kPos, p.x);
        swc1(w + moby::kPos + 4, p.y);
        swc1(w + moby::kPos + 8, p.z);
        // (tumbling: its rows turned about x a little each frame)
        const float a = 0.15f, c = std::cos(a), s = std::sin(a);
        const Vec4 r1 = V(w + 0xd0), r2 = V(w + 0xe0);
        V(w + 0xd0) = Vec4{c * r1.x + s * r2.x, c * r1.y + s * r2.y, c * r1.z + s * r2.z, 0.0f};
        V(w + 0xe0) = Vec4{c * r2.x - s * r1.x, c * r2.y - s * r1.y, c * r2.z - s * r1.z, 0.0f};
        moby_update_transform(w);
        return;
    }
    const int32_t lie = lw(pv + part::kLie) - 1;
    sw(pv + part::kLie, static_cast<uint32_t>(lie));
    if (lie <= 0) moby_kill(w);
}

// orig: (none)
bool attack_turn_held(uint32_t m) {
    for (const Turn& t : g_turns)
        if (t.m == m) return true;
    return false;
}

// orig: (none)
bool attack_turn_take(int32_t rac2_cls, uint32_t m, int most) {
    turns_prune();
    if (attack_turn_held(m)) return true;
    int n = 0;
    for (const Turn& t : g_turns) n += t.rac2_cls == rac2_cls ? 1 : 0;
    if (n >= most) return false;
    g_turns.push_back(Turn{rac2_cls, m, lwu(m + moby::kPvars), lh(m + moby::kClassId)});
    return true;
}

// orig: (none)
void attack_turn_give(uint32_t m) {
    g_turns.erase(std::remove_if(g_turns.begin(), g_turns.end(), [m](const Turn& t) { return t.m == m; }), g_turns.end());
}

// orig: (none)
uint32_t standin_count() {
    uint32_t n = 0;
    for (bool t : g_taken) n += t ? 1 : 0;
    return n;
}

// orig: (none)
Vec3 hero_pos() {
    const uint32_t m = lwu(HERO + 0x2080);
    return m != 0 ? moby_pos(m) : Vec3{};
}

// orig: (none)
int32_t hero_health() { return lw(NANOTECH); }

// orig: (none) — as nanotech.cpp's hero_health_max: Clank's 4, else *(0x15eda0)
int32_t hero_health_max() { return lbu(HERO + 0x20a4) == 1 ? 4 : lw(NANOTECH_MAX); }

// orig: (none)
uint32_t hero_item() { return lwu(HERO + 0x10b8); }

// orig: (none) — see rac2_compat.h: the record moby_hit_record_v keeps (+0x00 the push, +0x10 the attacker, +0x14 the
// kinds, +0x1c the strength, +0x20 1: the push given)
void hero_hit(uint32_t by, Vec3 push, float damage) {
    const uint32_t hm = lwu(HERO + 0x2080);
    if (hm == 0) return;
    alignas(16) uint8_t rec[0x24] = {};
    const float w = 5627.95f;  // (0x45afdf66: RAC1's and RAC2's mark of a push given with the hit)
    std::memcpy(rec + 0x0, &push.x, 4);
    std::memcpy(rec + 0x4, &push.y, 4);
    std::memcpy(rec + 0x8, &push.z, 4);
    std::memcpy(rec + 0xc, &w, 4);
    std::memcpy(rec + 0x10, &by, 4);
    const uint32_t kinds = 0x00010001u, given = 1;
    std::memcpy(rec + 0x14, &kinds, 4);
    std::memcpy(rec + 0x1c, &damage, 4);
    std::memcpy(rec + 0x20, &given, 4);
    moby_hit_record_v(hm, rec);
}

// orig: (none)
void bolts_give(int32_t n) { sw(BOLTS, static_cast<uint32_t>(lw(BOLTS) + n)); }

// orig: (none)
uint32_t pad_pressed() { return lwu(PAD + 0x1a4); }

// orig: (none)
void pad_take(uint32_t mask) {
    for (uint32_t o : {0x1a0u, 0x1a4u, 0x1a8u}) sw(PAD + o, lwu(PAD + o) & ~mask);
}

// orig: (none)
void pad_block() {
    pad_take(0xffffffffu);
    for (uint32_t o = 0x100; o < 0x110; o += 4) swc1(PAD + o, 0.0f);
}

// orig: (none) — as hud_message_box, several lines: a box (alpha 0x50) and each line centred, with its shadow
void hud_lines(int32_t y, const std::vector<Line>& lines) {
    if (lines.empty()) return;
    constexpr int32_t kStep = 18;
    size_t widest = 0;
    for (const Line& l : lines) widest = std::max(widest, l.text.size());
    const int32_t half = std::min<int32_t>(0xf0, static_cast<int32_t>(widest) * 5 + 0x20);
    hud_box(0x100 - half, y - 8, half * 2, kStep * static_cast<int32_t>(lines.size()) + 14, 0x50);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& t = lines[i].text;
        const uint32_t len = static_cast<uint32_t>(t.size());
        const uint32_t s = ee_stack_alloc(len + 1);
        for (uint32_t k = 0; k < len; ++k) sb(s + k, static_cast<uint8_t>(t[k]));
        sb(s + len, 0);
        const int32_t ly = y + kStep * static_cast<int32_t>(i);
        const uint32_t rgb = lines[i].rgb, bgr = (rgb & 0xff00u) | (rgb >> 16 & 0xffu) | (rgb & 0xffu) << 16;  // (the GS's order)
        const uint64_t col = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((0x80u << 24) + bgr)));
        text_centered(0x101, ly + 1, col & 0xffffffffff000000ull, s, -1);
        text_centered(0x100, ly, col, s, -1);
        ee_stack_free(len + 1);
    }
}

// orig: (none)
int32_t rand_below(int32_t n) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return n <= 1 ? 0 : static_cast<int32_t>(g_rng % static_cast<uint32_t>(n));
}

// orig: (none)
float rand_unit() { return static_cast<float>(rand_below(0x1000000)) / 16777216.0f; }

}  // namespace rac1::rac2
