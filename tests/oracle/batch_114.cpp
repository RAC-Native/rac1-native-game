// Oracle batch 114 (task 0340): Rilgar's own moby classes (level 5) on its real level data. Run with
// ORACLE_LEVEL=5 build/tests/ovl_oracle --batch 114 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Rilgar's own function at its own address, the port the one registered for it (the
// transliterations rilgar_classes_tr_N.cpp, the shared hand ports shared_*.cpp / novalis_*.cpp).

#include "ovl_oracle.h"

#include <game/ovl/enemy_749.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/rilgar_own.h>
#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_class1511.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_race_camera.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <string>
#include <map>
#include <memory>
#include <unordered_set>

namespace rac1::oracle {

namespace {
// A check whose original is Rilgar's own function at `l5addr` (not a Veldin address to translate).
void l5check(const char* name, uint32_t l5addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B114_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 5) {
        std::printf("%-26s L5:0x%08x  skipped (Rilgar only: run with ORACLE_LEVEL=5)\n", name, l5addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L5:0x%08x\n", name, l5addr);
    if (!check_fn(name, l5addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}

// ---- P5.1: Rilgar's own classes 35, 79, 133 and 623 ported by hand (rilgar_water_monster / _air_traffic / _race_bots /
// _spitter.cpp): their own state for the class checks, and checks of their helpers at Rilgar's own addresses
// the spawns' w lanes ignored (with_lanes below): the spawned mobys' (debris) and the particles'; for these four classes only
// the lanes their checks need (class 79's and 623's debris; 35 and 133 compare everything)
constexpr uint32_t LANES_DEBRIS = 1, LANES_POOL = 2;
// (P5.3b, the two talkers' checks: the talk's end that changes the level runs the engine's level change, the host's
// level_change_request in the port, which reads the clock into 0x15ee98 and saves the level's talkers' places
// (talkers_save); the original's run here does neither (its clock read is a no-op): those words are not compared)
constexpr uint32_t LANES_TALKSAVE = 4;
constexpr uint32_t LANES35 = 0, LANES79 = LANES_DEBRIS, LANES133 = 0, LANES623 = LANES_DEBRIS;

using GiveHit = std::function<void(uint32_t, uint32_t)>;
using Extras = std::map<std::string, std::function<void(uint32_t, int)>>;
constexpr uint32_t kH = 0x0013f350;        // the hero's block
constexpr uint32_t kHeroMoby = 0x001413d0;  // the word holding the hero's moby

// the frame time at its 50 Hz values in a third of the cases (the snapshot's are 60 Hz)
void p51_fifty() {
    if (ri(0, 2) != 0) return;
    wf(0x0015ed68, 1.2f);
    wf(0x0015ed6c, 0.02f);
    wf(0x0015ed70, 0.0004f);
}
// the animation ended (+0x70 bit 2: the class update reads it; the check's port does not run the animation step) or not
void p51_anim_end(uint32_t m) { w8(m + 0x70, coin() ? (ee_read<uint8_t>(m + 0x70) | 2u) : (ee_read<uint8_t>(m + 0x70) & ~2u)); }
// the animation at `frame` (moby_anim_time: the current frame record's +4 in 16ths, no blend)
void p51_anim_at(uint32_t m, float frame) {
    w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
    wf(m + 0x54, 0.0f);
    const uint32_t rec = ee_read<uint32_t>(m + 0x68);
    if (rec != 0 && rec < 0x02000000u) w16(rec + 4, u(static_cast<int32_t>(frame * 16.0f)));
}
uint32_t p51_path(int32_t i) { return ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4); }
// a valid path index for the moby's path word, or -1 now and then
void p51_path_word(uint32_t a, int odds_none) {
    if (ri(0, odds_none) == 0) { w32(a, 0xffffffffu); return; }
    const int32_t i = ee_read<int32_t>(a);
    if (i < 0 || i >= ee_read<int32_t>(OV(0x00160104)) || p51_path(i) == 0) w32(a, 0);
}
int32_t p51_points(uint32_t path_word) {
    const int32_t i = ee_read<int32_t>(path_word);
    if (i < 0) return 0;
    const uint32_t p = p51_path(i);
    return p != 0 ? ee_read<int32_t>(p) : 0;
}

Extras p51_extras(const GiveHit& give_hit) {
    Extras x;
    // the water monster: the hero swimming (his states +0x2084 / +0x208c 0x12) or in its mouth (0x72), at its surface or
    // near it, the waits and timers run out or not, rising / falling, the path or none
    x["class35"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        static const uint32_t hs[] = {0x12, 0x12, 0x72, 0x11, 1};
        w32(kH + 0x2084, hs[ri(0, 4)]);
        w32(kH + 0x208c, coin() ? 0x12u : 1u);
        if (coin()) {  // the hero over it, within 8 or not
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x += rf(-10, 10); q.y += rf(-10, 10); q.z += rf(0, 4);
            ee_ref<Vec4>(kH + 0x80) = q;
        }
        wf(kH + 0x2f0, coin() ? ee_read<float>(pv + 0x74) + rf(-0.45f, 0.45f) : ee_read<float>(m + 0x18) + rf(-3, 3));
        w32(pv + 0x7c, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
        w32(pv + 0x6c, u(ri(0, 3)));
        w32(pv + 0x84, u(ri(0, 2)));
        wf(pv + 0x78, rf(-1, 1) * (coin() ? 0.05f : 1.0f));
        if (coin()) wf(pv + 0x74, ee_read<float>(m + 0x18) + rf(-2, 12));
        w8(m + 0xbc, u(ri(0, 1)));
        if (st == 0) w32(pv + 0x64, coin() ? 1u : 0u);
        p51_path_word(pv + 0x60, st == 0 ? 4 : 1000);
        if (const int32_t n = p51_points(pv + 0x60); n > 0) {
            w32(pv + 0x68, u(ri(0, n - 1)));
            if (st == 1 && coin()) {  // at the point it heads for (within 3) or near it
                Vec4 q = ee_ref<Vec4>(p51_path(ee_read<int32_t>(pv + 0x60)) + 0x10 + ee_read<uint32_t>(pv + 0x68) * 16);
                q.x += rf(-2.5f, 2.5f); q.y += rf(-2.5f, 2.5f); q.z += rf(-1, 1);
                ee_ref<Vec4>(m + 0x10) = q;
            }
        }
        p51_anim_end(m);
        p51_fifty();
    };
    // the air traffic: the camera above or below 55, the lights' switch, its group, drawn or not, the frame's stamps
    // met or not, the path's turns to work out again, its place along the path, near the corner, hit
    x["class79"] = [give_hit](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        wf(0x001671c8, ri(0, 5) == 0 ? rf(0, 54.9f) : rf(55, 160));
        w32(0x001613e4, coin() ? 1u : 0u);
        w8(m + 0x21, coin() ? 0xffu : u(ri(0, 20)));
        w8(m + 0x31, u(ri(0, 1)));
        if (ri(0, 5) == 0) w32(OV(0x0015f5cc), 0);
        const uint32_t frame = ee_read<uint32_t>(OV(0x0015f5cc));
        for (const uint32_t s : {pv + 0x324, pv + 0x364}) {
            const uint32_t p = ee_read<uint32_t>(s);
            if (p != 0 && p < 0x02000000u) w32(p, coin() ? frame : frame + 1);
        }
        p51_path_word(pv, 1000);
        if (const int32_t n = p51_points(pv); n > 0) {
            if (st == 0 && coin()) wf(p51_path(ee_read<int32_t>(pv)) + 0x1c, -1.0f);
            wf(pv + 8, rf(0, static_cast<float>(n) + 0.5f));
            wf(pv + 4, rf(0, 1));
        }
        wf(pv + 0xc, rf(0.5f, 30));
        if (st == 1 && ri(0, 5) == 0 && ee_read<int32_t>(pv) >= 0) {  // its path moved near the level's corner (x or y below 8)
            const uint32_t p = p51_path(ee_read<int32_t>(pv));
            const uint32_t lane = coin() ? 0u : 4u;
            for (int32_t k = 0; p != 0 && k < ee_read<int32_t>(p); ++k) wf(p + 0x10 + u(k) * 16 + lane, rf(1, 7.5f));
        }
        if (ri(0, 4) == 0) wf(m + 0x10, rf(0, 8));
        if (ri(0, 4) == 0) wf(m + 0x14, rf(0, 8));
        w16(pv + 0x320, u(ri(0, 6)));
        w16(pv + 0x322, u(ri(0, 15)));
        w32(pv + 0x328, coin() ? 0xffffffffu : (R() & 0xff));
        if (coin()) give_hit(m, 0x10000u);
        p51_fifty();
    };
    // the race bots: the path or none, the point and its timer, drawn or not, hit (by the hero's moby or not), the hero
    // far for state 2
    x["class133"] = [give_hit](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        p51_path_word(pv + 0xb0, st == 0 ? 3 : 1000);
        if (const int32_t n = p51_points(pv + 0xb0); n > 0) {
            w32(pv + 0xb4, u(ri(0, n - 1)));
            if (st == 1 && coin()) {  // at the point it heads for (within 1) or near it
                Vec4 q = ee_ref<Vec4>(p51_path(ee_read<int32_t>(pv + 0xb0)) + 0x10 + ee_read<uint32_t>(pv + 0xb4) * 16);
                q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f);
                ee_ref<Vec4>(m + 0x10) = q;
            }
        }
        if (st == 0) w8(m + 0x53, u(ri(0, 1)));
        w32(pv + 0xb8, u(ri(0, 2)));
        w8(m + 0x31, u(ri(0, 1)));
        if (st == 2 && coin()) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f), r = rf(30, 70);
            q.x += r * std::cos(a); q.y += r * std::sin(a);
            ee_ref<Vec4>(kH + 0x80) = q;
        }
        if (coin()) {
            give_hit(m, 0x330000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                static const int who[] = {0, 1, 1, 2};
                const int k = who[ri(0, 3)];
                w32(e + 0x20, k == 0 ? 0u : k == 1 ? ee_read<uint32_t>(kHeroMoby) : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100u * u(ri(1, 60)));
            }
        }
        w32(kH + 0x8c4, u(ri(0, 30)));
        p51_fifty();
    };
    // the spitter: the game mode (a scene: 2), near the camera, its target (the hero's moby, a class mate, none) and
    // where it is, lost / alerted, the animation ended / its frame, the trigger moby and box of state 15, the wind-up and
    // wander counters, the hero in a vehicle, its health, its side
    x["class623"] = [give_hit](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        static const uint32_t modes[] = {0, 0, 0, 2, 1};
        w32(0x0015f5c4, modes[ri(0, 4)]);
        w32(pv + 0x2dc, u(ri(0, 1)));
        w16(m + 0x34, coin() ? (ee_read<uint16_t>(m + 0x34) | 1u) : (ee_read<uint16_t>(m + 0x34) & 0xfffeu));
        w8(m + 0x31, u(ri(0, 1)));
        {  // the animation set it is in (the class's: 0, 1, 2, 3, 4, 7)
            static const uint32_t sets[] = {0, 1, 2, 3, 4, 7, 0, 1};
            const uint32_t set = sets[ri(0, 7)];
            w8(m + 0x53, set);
            w8(m + 0x52, set);
        }
        if (coin()) {  // its home away from it (the way home blocked now and then: far)
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f), r = ri(0, 3) == 0 ? rf(30, 80) : rf(1.2f, 10);
            q.x += r * std::cos(a); q.y += r * std::sin(a);
            ee_ref<Vec4>(pv + 0x270) = q;
        }
        if (coin()) {  // the hero near, above it (more than 3) or level
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x += rf(-8, 8); q.y += rf(-8, 8); q.z += coin() ? rf(3.5f, 6) : rf(-1, 1);
            ee_ref<Vec4>(kH + 0x80) = q;
        }
        if (coin()) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x += rf(-30, 30); q.y += rf(-30, 30);
            ee_ref<Vec4>(0x001671c0) = q;
        }
        const uint32_t hero_moby = ee_read<uint32_t>(kHeroMoby);
        const int32_t nm = static_cast<int32_t>((ee_read<uint32_t>(OV(0x0015ffe0)) - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8);
        const int who = ri(0, 5);
        const uint32_t target = who <= 2 ? hero_moby : who == 3 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100u * u(ri(0, nm - 1));
        w32(pv + 0x160, target);
        if (target != 0 && coin()) {  // the target near it (within 3 half the time), in front of it or not
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = coin() ? ee_read<float>(m + 0x48) + rf(-0.3f, 0.3f) : rf(-3.1f, 3.1f), r = coin() ? rf(0, 3) : rf(3, 12);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-2.5f, 2.5f);
            ee_ref<Vec4>(target + 0x10) = q;
            if (target == hero_moby) ee_ref<Vec4>(kH + 0x80) = q;
        }
        w32(pv + 0x164, u(ri(0, 2)));
        w32(pv + 0x2b0, ri(0, 2) == 0 ? 0u : u(ri(1, 200)));
        w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
        p51_anim_end(m);
        if (coin()) p51_anim_at(m, rf(0, 3));
        if (st == 15 || st == 0) {
            const int32_t trig = ri(0, 2) == 0 ? -1 : ri(0, nm - 1);
            w32(pv + 0x2b8, u(trig));
            if (trig >= 0) w8(ee_read<uint32_t>(OV(0x0015ffd8)) + (u(trig) << 8) + 0xbc, u(ri(0, 1)));
            const int32_t nb = ee_read<int32_t>(OV(0x001600f0));
            const int32_t box = ri(0, 2) == 0 || nb <= 0 ? -1 : ri(0, nb - 1);
            w32(pv + 0x2bc, u(box));
            if (box >= 0 && coin()) ee_ref<Vec4>(kH + 0x80) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box) * 0x80 + 0x30);
            if (st == 0) w32(pv + 0x2b4, ri(0, 3) == 0 ? 1u : 0u);
        }
        w16(pv + 0x2d0, u(ri(0, 3)));
        static const int counts[] = {0, 30, 58, 62, 100, 178, 182, 250};
        w16(pv + 0x2d2, u(counts[ri(0, 7)]));
        if (st == 2 && coin()) {  // (P5.2) at its wander point (within 1) or just off it, its frames about the 1-second mark
            Vec4 q = ee_ref<Vec4>(pv + 0x280);
            q.x += rf(-0.8f, 0.8f); q.y += rf(-0.8f, 0.8f); q.z = ee_ref<Vec4>(m + 0x10).z;
            ee_ref<Vec4>(m + 0x10) = q;
            static const int near_counts[] = {40, 58, 59, 62, 66, 178, 182};
            w16(pv + 0x2d2, u(near_counts[ri(0, 6)]));
        }
        w32(pv + 0x2d4, coin() ? 0u : u(ri(1, 4)));
        w8(m + 0xbc, coin() ? 1u : 15u);
        w32(kH + 0x1158, ri(0, 3) == 0 ? 6u : 0u);
        wf(pv + 0x20, rf(0.2f, 3));
        wf(pv + 0x298, rf(-60, 60));
        wf(pv + 0x294, coin() ? 0.0f : rf(-1, 1));
        if (coin()) {
            give_hit(m, 0x330000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                const int kind = ri(0, 5);
                uint32_t src = kind == 0 ? 0u : kind == 1 ? hero_moby : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100u * u(ri(0, nm - 1));
                if (src == m) src = hero_moby;
                if (kind == 2) w16(src + 0xa6, 0x47);
                if (kind == 3 && ee_read<uint32_t>(src + 0x24) != 0) w16(ee_read<uint32_t>(src + 0x24) + 0x46, 5);
                if (kind == 4) w16(src + 0xa6, 0xba);
                w32(e + 0x20, src);
                wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
            }
        }
        p51_fifty();
    };
    return x;
}

// The helpers of the four classes, each against its original at Rilgar's own address, on the level's own mobys of the
// class with the class's own state (the extras above)
void p51_helper_checks(const std::function<void()>& level, const std::function<std::vector<uint32_t>(uint32_t)>& mobys_with,
                       const GiveHit& give_hit, const Extras& x) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    std::vector<uint32_t> m35, m79, m133, m623;
    if (g_oracle_level == 5) {
        level();
        m35 = mobys_with(0x002d1688);
        m79 = mobys_with(0x002d7140);
        m133 = mobys_with(0x002dac80);
        m623 = mobys_with(0x00301f48);
        if (m35.empty() || m79.empty() || m133.empty() || m623.empty()) {
            std::printf("p51 helpers                no moby of one of the classes on this level\n");
            return;
        }
    }
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
    const auto x35 = x.at("class35"), x79 = x.at("class79"), x133 = x.at("class133"), x623 = x.at("class623");
    // ---- the water monster's wake and splash
    l5check("ril_2d1e30", 0x002d1e30, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m35);
        x35(m, 1);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_2d1e30_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_2d1fd8", 0x002d1fd8, 60, Ret::None, [=] {
        level();
        const uint32_t m = pick(m35);
        p51_fifty();
        seed_rand();
        return Args{{m, coin() ? m + 0x10 : kH + 0x80}, {}};
    }, [](A a) { ril_2d1fd8_l5(a.i[1]); return uint64_t{0}; });
    // ---- the air traffic's lights and move
    l5check("ril_2d7020", 0x002d7020, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m79);
        x79(m, 1);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_2d7020_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_2d7890", 0x002d7890, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m79);
        x79(m, 1);
        const int32_t i = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78));
        return Args{{m, p51_path(i < 0 ? 0 : i)}, {}};
    }, [](A a) { ril_2d7890_l5(a.i[0], a.i[1]); return uint64_t{0}; });
    // ---- the race bots' hit, rings and sparks
    l5check("ril_2daf58", 0x002daf58, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m133);
        x133(m, 1);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_2daf58_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_2db018", 0x002db018, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m133);
        w8(m + 0x31, u(ri(0, 3) == 0 ? 0 : 1));
        p51_fifty();
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_2db018_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_2db238", 0x002db238, 30, Ret::None, [=] {
        level();
        const uint32_t m = pick(m133);
        if (coin()) wfrand(kH + 0x100, 3, -12, 12);
        p51_fifty();
        seed_rand();
        return Args{{m, coin() ? kH + 0x100 : m + 0x40}, {}};
    }, [](A a) { ril_2db238_l5(a.i[0], a.i[1]); return uint64_t{0}; });
    // ---- the spitter's frame before its state, its way round the walls, its class mates, the walls' push
    l5check("ril_303390", 0x00303390, 240, Ret::None, [=] {
        level();
        const uint32_t m = pick(m623);
        static const int sts[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15};
        const int st = sts[ri(0, 11)];
        w8(m + 0x20, u(st));
        x623(m, st);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_303390_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_303b08", 0x00303b08, 160, Ret::None, [=] {
        level();
        const uint32_t m = pick(m623);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        x623(m, 4);
        if (coin()) w32(OV(0x0015f5cc), ((m >> 8) & 3u) | (R() & 0xfffcu));  // (its frame of the four)
        const uint32_t target = ee_read<uint32_t>(pv + 0x160);
        const uint32_t to = coin() && target != 0 ? target + 0x10 : pv + 0x270;
        seed_rand();
        return Args{{m, to}, {coin() ? 0.0f : rf(-1, 1)}};
    }, [](A a) { ril_303b08_l5(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    l5check("ril_303e50", 0x00303e50, 160, Ret::Int32, [=] {
        level();
        const uint32_t m = pick(m623);
        x623(m, 4);
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        const float ang = rf(-3.1f, 3.1f), r = rf(1, 15);
        q.x += r * std::cos(ang); q.y += r * std::sin(ang);
        ee_ref<Vec4>(kH + 0x80) = q;
        return Args{{m, kH + 0x80}, {rf(0.2f, 2.0f)}};
    }, [](A a) { return r32(u(ril_303e50_l5(a.i[0], a.i[1], a.f[0]))); });
    l5check("ril_303f80", 0x00303f80, 120, Ret::Int32, [=] {
        level();
        const uint32_t m = pick(m623);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const int32_t own = ee_read<int32_t>(pv + 0x2c8);
        const int32_t group = own >= 0 && coin() ? own : ri(0, ee_read<int32_t>(OV(0x0015fff4)) > 0 ? ee_read<int32_t>(OV(0x0015fff4)) - 1 : 0);
        for (uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + u(group) * 4); e != 0; e += 2) {  // its members shown / hidden, alive or not, near or far
            const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
            if (ri(0, 3) == 0) w16(o + 0x34, ee_read<uint16_t>(o + 0x34) ^ 0x1000u);
            if (ri(0, 5) == 0) w8(o + 0x20, coin() ? 0x7fu : 1u);
            if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-130, 130); q.y += rf(-130, 130); ee_ref<Vec4>(o + 0x10) = q; }
            if (ee_read<int16_t>(e) < 0) break;
        }
        return Args{{m, u(group)}, {}};
    }, [](A a) { return r32(ril_303f80_l5(a.i[0], static_cast<int32_t>(a.i[1]))); });
    l5check("ril_304058", 0x00304058, 200, Ret::Float, [=] {
        level();
        const uint32_t m = pick(m623);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        int32_t path = ee_read<int32_t>(pv + 0x2c0);
        if (path < 0 || path >= ee_read<int32_t>(OV(0x00160104)) || p51_path(path) == 0 || coin()) path = ri(0, ee_read<int32_t>(OV(0x00160104)) - 1);
        const uint32_t p = p51_path(path);
        const int32_t n = p != 0 ? ee_read<int32_t>(p) : 0;
        if (n >= 2) {  // a point near a segment: along it (and past its ends), off it
            const int32_t i = ri(0, n - 2);
            const Vec4 a = ee_ref<Vec4>(p + 0x10 + u(i) * 16), b = ee_ref<Vec4>(p + 0x20 + u(i) * 16);
            const float t = rf(-0.3f, 1.3f);
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x = a.x + (b.x - a.x) * t + rf(-2.5f, 2.5f);
            q.y = a.y + (b.y - a.y) * t + rf(-2.5f, 2.5f);
            ee_ref<Vec4>(m + 0x10) = q;
            if (ri(0, 3) == 0) wf(p + 0x1c + u(i) * 16, 0.0f);  // (an end not a wall's)
        }
        return Args{{u(path), m + 0x10, coin() ? m + 0x10 : pv + 0x280}, {rf(0.2f, 2.5f)}};
    }, [](A a) { return rf32(ril_304058_l5(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2], a.f[0])); });
    (void)give_hit;
}

// ---- P5.2: Rilgar's own classes 625, 717, 810, 812, 831, 838, 841 and 843 ported by hand (rilgar_flame_tank / _race_rivals /
// _water / _props.cpp): their own state for the class checks, checks of their helpers, and fix 21's fork in class 831
constexpr uint32_t LANES625 = LANES_DEBRIS | LANES_POOL, LANES717 = 0, LANES810 = 0, LANES812 = 0, LANES831 = 0, LANES838 = 0, LANES841 = 0, LANES843 = 0;

// a moby of the level by its index, and how many there are
uint32_t p52_moby(int32_t i) { return ee_read<uint32_t>(OV(0x0015ffd8)) + (u(i) << 8); }
int32_t p52_mobys() { return static_cast<int32_t>((ee_read<uint32_t>(OV(0x0015ffe0)) - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8); }
// the hero at `q` (his block's place and his moby's)
void p52_hero_at(const Vec4& q) {
    ee_ref<Vec4>(kH + 0x80) = q;
    const uint32_t hm = ee_read<uint32_t>(kHeroMoby);
    if (hm != 0) ee_ref<Vec4>(hm + 0x10) = q;
}
// `q` moved by up to `r` in the plane and `z` in height
Vec4 p52_near(Vec4 q, float r, float z) {
    const float a = rf(-3.1f, 3.1f), d = rf(0, r);
    q.x += d * std::cos(a); q.y += d * std::sin(a); q.z += rf(-z, z);
    return q;
}

void p52_extras(Extras& x, const GiveHit& give_hit) {
    // the flame tank: the hero near (within its range and 4 in height or not), its area, the alert and its timer, hit (on
    // it or its treads; health low), the fire's drops kept, the hero in a vehicle, the frame of the eight, the patrol
    x["class625"] = [give_hit](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (coin()) p52_hero_at(p52_near(ee_ref<Vec4>(m + 0x10), coin() ? 14.0f : 30.0f, coin() ? 3.5f : 8.0f));
        if (coin()) { w8(m + 0x31, 1); ee_ref<Vec4>(0x001671c0) = p52_near(ee_ref<Vec4>(m + 0x10), 35.0f, 3.0f); }  // (drawn, the camera near or not)
        if (ri(0, 3) == 0) w32(pv + 0x190, 0xffffffffu);
        wf(pv + 0x174, coin() ? 14.0f : 20.0f);
        w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
        w32(pv + 0x178, coin() ? 0u : u(ri(1, 3)));
        wf(pv + 0x20, rf(0.2f, 6));
        if (ri(0, 2) == 0) w8(pv + 0x2e, 2);
        w32(kH + 0x1158, ri(0, 3) == 0 ? 6u : 0u);
        if (coin()) w32(OV(0x0015f5cc), R() & 0xfff8u);
        w8(pv + 0xc4, coin() ? 1u : 0xffu);
        if (const uint32_t path = ee_read<uint32_t>(pv + 0xd0); path != 0 && path < 0x02000000u && ee_read<int32_t>(path) > 0) {
            const int32_t n = ee_read<int32_t>(path);
            const int32_t i = ri(0, n - 1);
            w32(pv + 0xc0, u(i));
            if (coin()) {  // at the next point (within 1) or near it
                const int32_t dir = static_cast<int8_t>(ee_read<uint8_t>(pv + 0xc4));
                const int32_t next = ((i + n + dir) % n + n) % n;
                ee_ref<Vec4>(m + 0x10) = p52_near(ee_ref<Vec4>(path + 0x10 + u(next) * 16), 1.3f, 0.0f);
            }
        }
        for (uint32_t k = 0; k < 8; ++k) {  // the fire's drops: none, a live particle 0x02 or a spent one
            const int c = ri(0, 3);
            const uint32_t p = ee_read<uint32_t>(OV(0x0016016c)) + u(ri(0, 0x7ff)) * 0x40;
            if (c == 0) { w32(pv + 0x1a0 + k * 4, 0); continue; }
            w32(pv + 0x1a0 + k * 4, p);
            w8(p, c == 3 ? 1u : 2u);
            w16(p + 0x32, c == 2 ? 0u : u(ri(1, 30)));
            ee_ref<Vec4>(p + 0x10) = p52_near(ee_ref<Vec4>(kH + 0x80), 1.0f, 0.5f);
        }
        if (coin()) {
            const uint32_t tread = ee_read<uint32_t>(pv + (coin() ? 0x184 : 0x188));
            give_hit(tread != 0 && coin() ? tread : m, 0x210000u);
        }
        if (st == 2 || coin()) { w8(m + 0x53, u(ri(0, 1))); }
        p51_fifty();
    };
    // the race rivals: the race on (the hero's state 0x16) or not, its board, the race's attempts and the win flag, in the
    // air or not and its frames, its height against the floor, the start's timer, the places, the trick and the sets
    x["class717"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(kH + 0x208c, ri(0, 4) == 0 ? 1u : 0x16u);
        if (st == 1 && ri(0, 2) == 0) w32(pv + 0x268, 0);
        if (coin()) { w8(m + 0x31, 1); ee_ref<Vec4>(0x001671c0) = p52_near(ee_ref<Vec4>(m + 0x10), 35.0f, 3.0f); }  // (drawn, the camera near or not)
        w8(0x0013d388, u(ri(0, 1)));
        w32(0x0015ee38, u(ri(0, 15)));
        static const int air[] = {0, 0, 1, 3, 6, 9, 12, 20};
        w32(pv + 0x26c, u(air[ri(0, 7)]));
        wf(pv + 0x24c, rf(-0.3f, 0.3f));
        wf(pv + 0x208, ee_read<float>(m + 0x18) + rf(-1.0f, 2.0f));
        wf(pv + 0x218, ee_read<float>(m + 0x18) + rf(-3.0f, 1.0f));
        w16(pv + 0x25a, u(ri(0, 3) == 0 ? ri(1, 3) : 0));
        w32(kH + 0x894, u(ri(0, 1400)));
        w32(kH + 0x8c0, u(ri(0, 4)));
        for (uint32_t k = 0; k < 4; ++k) w16(kH + 0x7c0 + k * 2, u(ri(0, 4)));
        static const int tricks[] = {-1, -1, 1, 2, 3};
        w16(pv + 0x272, u(tricks[ri(0, 4)]));
        static const uint32_t sets[] = {0, 5, 5, 1, 2, 3};
        const uint32_t set = sets[ri(0, 5)];
        w8(m + 0x53, set);
        w8(m + 0x52, coin() ? set : 0u);
        p51_anim_end(m);
        if (coin()) p51_anim_at(m, rf(0, 26));
        wf(pv + 0x240, rf(0, 0.5f));
        if (st == 2 && ee_read<uint32_t>(pv + 0x268) == 0 && coin()) w32(pv + 0x268, ril_2f8718_l5(m));  // (its board, as its state 1 spawns it)
        if (const uint32_t path = ee_read<uint32_t>(pv + 0x244); path != 0 && path < 0x02000000u && ee_read<int32_t>(path) > 0)
        {
            const int32_t n = ee_read<int32_t>(path);
            w16(pv + 0x258, u(ri(0, n - 1)));
            w32(pv + 0x27c, u(std::min(n - 1, static_cast<int32_t>(ee_read<int16_t>(pv + 0x258)) + ri(0, 8))));  // (the landing a few points on)
        }
        p51_fifty();
    };
    // the rocking floats: the creak's timer run out or not
    x["class810"] = [](uint32_t m, int) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x78, u(ri(0, 2)));
        p51_fifty();
    };
    // the props on a carrier: none (-1) now and then
    x["class812"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (st == 0 && ri(0, 3) == 0) w32(pv + 0x80, 0xffffffffu);
        p51_fifty();
    };
    // the water: switched (bits 4 / 8) or not, moving, its switch kind, the level's flag, the hero swimming near it
    // (below its surface) or on the valve (a moby of class 0x33e under him), the tries, the stage and its timer, the hero
    // at the valve's point, its cell the level's special one, the scene's game mode
    x["class831"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0xbc, coin() ? u(ri(0, 15)) : 1u);
        w32(pv + 0x24, u(ri(0, 1)));
        if (coin()) w32(pv + 0x10, coin() ? 0xffffffffu : 0u);
        if (st == 0) { w32(pv + 0xc, u(ri(0, 1))); w32(pv + 8, u(ri(0, 1))); }
        w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0), coin() ? 0xffu : 1u);
        w32(kH + 0x208c, coin() ? 0x11u : 1u);
        w32(kH + 0x2084, ri(0, 2) == 0 ? 0x6au : 1u);
        w32(kH + 0x198, u(ri(1, 2)));
        if (coin()) {
            Vec4 q = p52_near(ee_ref<Vec4>(m + 0x10), coin() ? 9.0f : 20.0f, 0.0f);
            q.z = ee_read<float>(m + 0x18) - rf(-0.5f, 2.0f);
            p52_hero_at(q);
            wf(kH + 0x2f0, q.z + rf(-1, 1));
        }
        if (coin()) {  // a valve under him
            const uint32_t o = p52_moby(ri(0, p52_mobys() - 1));
            w16(o + 0xa6, 0x33e);
            w32(kH + 0x2fc, o);
            w16(kH + 0x30e, u(ri(0, 3) == 0 ? 1 : 0));
        } else {
            w32(kH + 0x2fc, 0);
        }
        w32((0x00161d1cu), u(ri(0, 12)));
        w32((0x00161d20u), u(ri(0, 5)));
        w32((0x00161d2cu), u(ri(0, 1)));
        wf((0x00161d24u), ee_read<float>(m + 0x18) + rf(-3, 3));
        w32(pv + 0x18, u(ri(0, 3)));
        if (st == 8 && coin()) { Vec4 q{211.0f, 338.0f, 25.0f, 0.0f}; p52_hero_at(p52_near(q, 7.0f, 1.0f)); }
        if (coin()) w32((0x00161d10u), ee_read<uint32_t>(pv + 0x14));
        if (coin()) { const float lo = ee_read<float>(pv + 4); wf(m + 0x18, lo + rf(-1.0f, 6.0f)); }
        if (st == 7 && coin()) wf(m + 0x18, ee_read<float>(pv) + rf(-1, 1));
        if (st == 8 && coin()) wf(m + 0x18, rf(40, 48));
        p51_fifty();
    };
    // the shared hum: the camera near or far, the frame's stamp met or not, the shared slot none / its own / another of the
    // class / another class, the switch moby set or not
    x["class838"] = [](uint32_t m, int) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (coin()) ee_ref<Vec4>((0x001671c0u)) = p52_near(ee_ref<Vec4>(m + 0x10), coin() ? 60.0f : 120.0f, 5.0f);
        if (coin()) w32((0x00161d38u), ee_read<uint32_t>(OV(0x0015f5cc)));
        const uint32_t slotw = ee_read<uint32_t>(pv + 4);
        if (slotw != 0 && slotw < 0x02000000u) {
            const int c = ri(0, 4);
            const int32_t slot = c == 0 ? -1 : ri(0, 15);
            w32(slotw, u(slot));
            if (slot >= 0) {
                const uint32_t e = 0x0013e550 + u(slot) * 0x70;
                const uint32_t o = c == 1 ? 0u : c == 2 ? m : p52_moby(ri(0, p52_mobys() - 1));
                w32(e + 0x88, o);
                if (c == 3 && o != 0) w16(o + 0xa6, 0x346);
                if (c >= 3 && o != 0) { w8(o + 0x20, coin() ? 1u : 2u); ee_ref<Vec4>(o + 0x10) = p52_near(ee_ref<Vec4>(m + 0x10), 40.0f, 5.0f); }
            }
        }
        const int32_t sw_moby = ee_read<int32_t>(pv);
        if (sw_moby >= 0 && sw_moby < p52_mobys()) w8(p52_moby(sw_moby) + 0xbc, coin() ? 0u : 1u);
        p51_fifty();
    };
    // the fog zones: the hero in each of its boxes or not
    x["class841"] = [](uint32_t m, int) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        static const uint32_t boxes[] = {0x28, 0x2c, 0x38};
        const int32_t nb = ee_read<int32_t>(OV(0x001600f0));
        const int32_t b = ee_read<int32_t>(pv + boxes[ri(0, 2)]);
        if (coin() && b >= 0 && b < nb) p52_hero_at(p52_near(ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(b) * 0x80 + 0x30), 1.5f, 0.5f));
        p51_fifty();
    };
    // the gates: the switch moby set or not, near the end of the slide or of the sinking
    x["class843"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const int32_t s = ee_read<int32_t>(pv + 0x14);
        if (s >= 0 && s < p52_mobys()) w8(p52_moby(s) + 0xbc, coin() ? 0u : 1u);
        if (coin()) w32(pv + 0x1c, u(ri(0, 1)));
        wfrand(pv, 3, -0.3f, 0.3f);
        const int32_t b = ee_read<int32_t>(pv + 0x10);
        if (st == 3 && coin() && b >= 0 && b < ee_read<int32_t>(OV(0x001600f0)))
            wf(m + 0x18, ee_read<float>(ee_read<uint32_t>(OV(0x001600ec)) + u(b) * 0x80 + 0x38) - rf(5.0f, 7.0f));
        wf(pv + 8, rf(-0.5f, 0.5f));
        p51_fifty();
    };
}

// The helpers of the classes, each against its original at Rilgar's own address, on the level's own mobys of the class
void p52_helper_checks(const std::function<void()>& level, const std::function<std::vector<uint32_t>(uint32_t)>& mobys_with,
                       const Extras& x, const std::function<std::vector<uint32_t>(uint32_t)>& lanes) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    std::vector<uint32_t> m625, m717, m831;
    if (g_oracle_level == 5) {
        level();
        m625 = mobys_with(0x00304320);
        m717 = mobys_with(0x00307910);
        m831 = mobys_with(0x0030ca80);
        if (m625.empty() || m717.empty() || m831.empty()) {
            std::printf("p52 helpers                no moby of one of the classes on this level\n");
            return;
        }
    }
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
    const auto x625 = x.at("class625"), x717 = x.at("class717"), x831 = x.at("class831");
    // a rival with its path set up (its state 0 by the port, as generic_prep does)
    auto rival = [pick, m717] {
        const uint32_t m = pick(m717);
        if (ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + 0x244) == 0) { w8(m + 0x20, 0); ril_class717_update_l5(m); }
        return m;
    };
    // ---- the flame tank's hit (and death: its blasts and pieces leave the spawns' lanes, ignored for this check only) and the
    // fire's damage
    const std::vector<uint32_t> before_lanes = g_ignored_words;
    g_ignored_words = lanes(LANES625);
    l5check("ril_305178", 0x00305178, 160, Ret::None, [=] {
        level();
        const uint32_t m = pick(m625);
        w8(m + 0x20, u(ri(0, 3) == 0 ? 3 : ri(1, 2)));
        x625(m, 1);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_305178_l5(a.i[0]); return uint64_t{0}; });
    g_ignored_words = before_lanes;
    l5check("ril_305898", 0x00305898, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m625);
        x625(m, 2);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_305898_l5(a.i[0]); return uint64_t{0}; });
    // ---- the race rivals' path points, next point, jumps, animations
    l5check("ril_306d30", 0x00306d30, 80, Ret::None, [=] {
        level();
        const uint32_t m = rival();
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t path = ee_read<uint32_t>(pv + 0x244);
        const int32_t n = path != 0 ? ee_read<int32_t>(path) : 1;
        return Args{{m, u(ri(0, n > 0 ? n - 1 : 0)), pv + 0x210}, {}};
    }, [](A a) { ril_306d30_l5(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2]); return uint64_t{0}; });
    l5check("ril_306e10", 0x00306e10, 160, Ret::None, [=] {
        level();
        const uint32_t m = rival();
        x717(m, 2);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ri(0, 2) == 0) {  // on a side path now and then
            const int32_t k = ri(0, 3);
            const int32_t i = ee_read<int32_t>(pv + 0x224 + u(k) * 4);
            if (i >= 0 && i < ee_read<int32_t>(OV(0x00160104)) && p51_path(i) != 0 && ee_read<int32_t>(p51_path(i)) > 0) {
                w32(pv + 0x244, p51_path(i));
                w16(pv + 0x258, u(ri(0, ee_read<int32_t>(p51_path(i)) - 1)));
            }
        }
        if (ri(0, 2) == 0) {  // on the main path just before one of its forks (a point whose w lane is 1 .. 4)
            const int32_t mi = ee_read<int32_t>(pv + 0x220);
            const uint32_t mp = mi >= 0 ? p51_path(mi) : 0u;
            std::vector<int32_t> forks;
            for (int32_t j = 1; mp != 0 && j < ee_read<int32_t>(mp); ++j) if (ee_read<float>(mp + 0x1c + u(j) * 16) > 0.0f) forks.push_back(j);
            if (!forks.empty()) {
                const int32_t j = forks[static_cast<size_t>(ri(0, static_cast<int>(forks.size()) - 1))];
                w32(pv + 0x244, mp);
                w16(pv + 0x258, u(j - 1));
            }
        }
        if (coin()) {  // on its place on the path (the point within 2 / 4)
            const uint32_t path = ee_read<uint32_t>(pv + 0x244);
            ee_ref<Vec4>(pv + 0x200) = p52_near(ee_ref<Vec4>(path + 0x10 + u(ee_read<int16_t>(pv + 0x258)) * 16), 5.0f, 0.0f);
            ee_ref<Vec4>(m + 0x10) = ee_ref<Vec4>(pv + 0x200);
        }
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_306e10_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_307358", 0x00307358, 120, Ret::None, [=] {
        level();
        const uint32_t m = rival();
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const int32_t k = ri(0, 3);
        const int32_t i = ee_read<int32_t>(pv + 0x224 + u(k) * 4);
        if (coin() && i >= 0 && i < ee_read<int32_t>(OV(0x00160104)) && p51_path(i) != 0) {  // a side path, at a take-off (w 1) when it has one
            const uint32_t p = p51_path(i);
            w32(pv + 0x244, p);
            const int32_t n = ee_read<int32_t>(p);
            int32_t at = n > 0 ? ri(0, n - 1) : 0;
            bool has2 = false;
            std::vector<int32_t> offs;
            for (int32_t j = 0; j < n; ++j) {
                const float wl = ee_read<float>(p + 0x1c + u(j) * 16);
                if (wl == 2.0f) has2 = true;
                if (wl == 1.0f) offs.push_back(j);
            }
            if (has2 && !offs.empty() && ri(0, 3) != 0) at = offs[static_cast<size_t>(ri(0, static_cast<int>(offs.size()) - 1))];  // (a take-off)
            if (!has2 && ee_read<float>(p + 0x1c + u(at) * 16) == 1.0f) at = (at + 1) % n;  // (no landing: the original would loop)
            w16(pv + 0x258, u(at));
        }
        wf(pv + 0x24c, coin() ? rf(-0.3f, 0.0f) : rf(0.0f, 0.3f));
        return Args{{m}, {}};
    }, [](A a) { ril_307358_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_307480", 0x00307480, 80, Ret::None, [=] {
        level();
        const uint32_t m = rival();
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (ee_read<uint32_t>(pv + 0x268) == 0 && ri(0, 3) != 0) w32(pv + 0x268, ril_2f8718_l5(m));  // (its board, as its state 1 spawns it)
        static const int sets[] = {0, 1, 2, 3, 5};
        return Args{{m, u(sets[ri(0, 4)]), u(ri(0, 24)), u(ri(1, 10))}, {}};
    }, [](A a) { ril_307480_l5(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
    l5check("ril_307570", 0x00307570, 200, Ret::None, [=] {
        level();
        const uint32_t m = rival();
        x717(m, 2);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { ril_307570_l5(a.i[0]); return uint64_t{0}; });
    // ---- the water's group kept level and a cell's neighbours
    l5check("ril_30d5c0", 0x0030d5c0, 120, Ret::None, [=] {
        level();
        const uint32_t m = pick(m831);
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0xc, u(ri(0, 1)));
        wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-3, 3));
        if (coin()) wf(pv + 4, ee_read<float>(pv + 4) + rf(-1, 1));
        return Args{{m}, {}};
    }, [](A a) { ril_30d5c0_l5(a.i[0]); return uint64_t{0}; });
    l5check("ril_30d6a0", 0x0030d6a0, 96, Ret::None, [=] {
        level();
        const int32_t k = ri(0, 47);
        if (ri(0, 2) == 0) {  // a neighbour moved off the 16-grid (the original's corner test reads x alone)
            const uint32_t c = 0x001d6880 + u(ri(0, 47)) * 0x1190;
            wf(c + 4, ee_read<float>(c + 4) + (coin() ? 16.0f : rf(-0.05f, 0.05f)));
        }
        return Args{{u(k)}, {}};
    }, [](A a) { ril_30d6a0_l5(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });

    // ---- fix 21 (1.02 Gameplay; always on the PAL 2.00 disc): the port with the fix against the port without it plus the
    // 1.02 build's two changes by hand (L5:0x30dedc: in game mode 2 nothing; L5:0x30e4b0: in state 1, moving, more than 0.1
    // from its high height after the seek, Ratchet swimming (0x11 / 0x12) below its surface gets its height)
    if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "class831_fix21") == 0) {
        if (std::getenv("RAC1_GAMEPLAY102") == nullptr || g_oracle_level != 5) {
            std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=21 on level 5)\n", "class831_fix21");
            ++g_skipped;
        } else {
            int bad = 0, n = 0, held = 0, carried = 0;
            static std::vector<uint8_t> start, with_fix;
            const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
            for (int c = 0; c < 120; ++c) {
                level();
                const uint32_t m = pick(m831);
                const int st = ri(0, 2) == 0 ? ri(0, 8) : 1;
                w8(m + 0x20, u(st));
                x831(m, st);
                w32(OV(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
                if (st == 1) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w32(pv + 0x24, 1);
                    w32(kH + 0x208c, coin() ? 0x11u : (coin() ? 0x12u : 1u));
                    wf(kH + 0x2f0, ee_read<float>(m + 0x18) - rf(-0.5f, 1.0f));
                }
                seed_rand();
                uint8_t* ram = ee_get_ram_base();
                start.assign(ram, ram + 0x02000000);
                g_advanced_disabled = false;
                ril_class831_update_l5(m);
                g_advanced_disabled = true;
                with_fix.assign(ram, ram + 0x02000000);
                std::memcpy(ram, start.data(), start.size());
                const bool mode2 = ee_read<int32_t>(OV(0x0015f5c4)) == 2;
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const bool high = ee_read<uint8_t>(m + 0x20) == 1;
                if (mode2) {
                    ++held;  // (the 1.02 build: nothing at all)
                } else {
                    ril_class831_update_l5(m);  // (the fix off: the North American code)
                    const bool moving = high && ee_read<int32_t>(pv + 0x24) != 0;  // (state 1 stays 1; +0x24 as its update left it)
                    const float z = ee_read<float>(m + 0x18);
                    if (moving && 0.1f < std::fabs(z - ee_read<float>(pv)) && ee_read<uint32_t>(kH + 0x208c) - 0x11u < 2u &&
                        ee_read<float>(kH + 0x2f0) < z) {
                        wf(kH + 0x2f0, z);
                        ++carried;
                    }
                }
                ++n;
                for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                    if (ignored.count(a) != 0) continue;
                    if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;  // (the port's stack frames)
                    uint32_t xw, yw;
                    std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                    if (xw != yw) { if (bad < 8) std::printf("  class831_fix21 case %d: %08x fix=%08x expected=%08x\n", c, a, xw, yw); ++bad; break; }
                }
            }
            std::printf("%-26s %s (%d cases, %d held in a scene, %d carried the swimmer)\n", "class831_fix21",
                        bad == 0 && held > 0 && carried > 0 ? "ok" : "FAIL", n, held, carried);
            if (bad != 0 || held == 0 || carried == 0) ++g_fail;
        }
    }
}


// ---- P5.3 (a first part): Rilgar's own classes 879, 895, 982, 984, 1099, 1550 ported by hand (rilgar_ocean / _props.cpp)
// and class 79's lights' draw (rilgar_air_traffic.cpp; checked by batch 140): their own state for the class checks
constexpr uint32_t LANES879 = 0, LANES895 = 0, LANES982 = 0, LANES984 = 0, LANES1099 = 0, LANES1550 = LANES_POOL;

void p53_extras(Extras& x) {
    // the open sea: racing (the hero's state 0x16) or not, the frame counter anywhere
    x["class879"] = [](uint32_t, int) {
        w32(kH + 0x208c, coin() ? 0x16u : 1u);
        w32(OV(0x0015f5cc), R() & 0xffffu);
        p51_fifty();
    };
    // the turning gate: the other way (+0) or not, its switch moby set or not
    x["class895"] = [](uint32_t m, int) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv, u(ri(0, 1)));
        const int32_t s = ee_read<int32_t>(pv + 8);
        if (s >= 0 && s < p52_mobys()) w8(p52_moby(s) + 0xbc, coin() ? 0u : 1u);
        wf(m + 0x48, rf(-3.1f, 3.1f));
        p51_fifty();
    };
    // the water disc: the hero in its box (at its centre) or not
    x["class982"] = [](uint32_t m, int) {
        const int32_t b = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78));
        if (coin() && b >= 0 && b < ee_read<int32_t>(OV(0x001600f0)))
            p52_hero_at(p52_near(ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(b) * 0x80 + 0x30), 1.0f, 0.5f));
        w32(OV(0x0015f5cc), R() & 0xffffu);
    };
    // the floats and the hidden props: in a scene (game mode 2) or not
    auto mode = [](uint32_t, int) { w32(0x0015f5c4, ri(0, 2) == 0 ? 2u : 0u); p51_fifty(); };
    x["class984"] = mode;
    x["class1099"] = mode;
    // the scenes' smoke: in a scene, the scene 10 or 11 (or another), its moby 3 a moby of class 0x2ee (or none)
    x["class1550"] = [](uint32_t, int) {
        w32(0x0015f5c4, ri(0, 3) == 0 ? 0u : 2u);
        static const int scenes[] = {9, 10, 10, 11, 11, 12};
        w32(0x0016cc90, u(scenes[ri(0, 5)]));
        uint32_t smoke = 0;
        for (int32_t i = 0; i < p52_mobys() && smoke == 0; ++i)
            if (ee_read<int16_t>(p52_moby(i) + 0xa6) == 0x2ee && ee_read<uint8_t>(p52_moby(i) + 0x20) < 0xfe) smoke = p52_moby(i);
        // (Rilgar places no moby of class 0x2ee: the scenes spawn theirs; a flying car, whose class has the chain 0 the smoke
        // starts from, stands in for it, its class word set to 0x2ee)
        for (int32_t i = 0; i < p52_mobys() && smoke == 0; ++i)
            if (ee_read<int16_t>(p52_moby(i) + 0xa6) == 79 && ee_read<uint8_t>(p52_moby(i) + 0x20) < 0xfe) smoke = p52_moby(i);
        if (smoke != 0) w16(smoke + 0xa6, 0x2ee);
        w32(0x0016cc60 + 0x178 + 3 * 4, ri(0, 4) == 0 ? 0u : smoke);
        w32(0x0016cc60 + 0x178, coin() ? smoke : 0u);
        w32(OV(0x0015f5cc), R() & 0xffffu);
    };
}

// ---- P5.3b: the rest of Rilgar's own classes ported by hand (rilgar_props / _people.cpp ..): their own state
constexpr uint32_t LANES846 = LANES_DEBRIS | LANES_POOL, LANES852 = 0, LANES893 = 0, LANES919 = LANES_TALKSAVE, LANES925 = LANES_TALKSAVE, LANES919X = LANES_TALKSAVE, LANES925X = LANES_TALKSAVE, LANES855 = LANES_POOL, LANES877 = 0, LANES998 = 0;

// a timer word: run out, running out, or running
uint32_t p53b_timer() { static const uint32_t t[] = {0, 1, 2, 40}; return t[ri(0, 3)]; }
// the hero in front of moby `m` within `r` (his head point too), standing or moving
void p53b_hero_front(uint32_t m, float r) {
    const Vec4 c = ee_ref<Vec4>(m + 0x10);
    const float yaw = ee_read<float>(m + 0x48) + rf(-1.2f, 1.2f), d = rf(0.5f, r);
    Vec4 q = c; q.x += d * std::cos(yaw); q.y += d * std::sin(yaw);
    p52_hero_at(q);
    Vec4 h = q; h.z += 1.5f;
    ee_ref<Vec4>(kH + 0xd0) = h;
    Vec4 v{0, 0, 0, 0};
    if (coin()) { v.x = rf(-0.2f, 0.2f); v.y = rf(-0.2f, 0.2f); }
    ee_ref<Vec4>(kH + 0x100) = v;
}
// a talker's talk: the hero unable to talk (talk_update 0) or able (it still waits for the button). (No case runs a talk's
// record: that streams its voice, and the original's stream wait is not run here)
void p53b_talk(uint32_t, uint32_t) {
    if (coin()) { w32(kH + 0x22a8, 0); return; }
    w32(kH + 0x22a8, 1);
    w32(kH + 0x2084, 0);
}

void p53b_extras(Extras& x, const GiveHit& give_hit) {
    // the gate in its slot: its switch's word, its two delays, its class word 852 or 853, near its top or not
    x["class852"] = [](uint32_t m, int) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const int32_t s = ee_read<int32_t>(pv);
        if (s >= 0 && s < p52_mobys()) w8(p52_moby(s) + 0xbc, u(ri(0, 2)));
        w32(pv + 4, p53b_timer());
        w32(pv + 8, p53b_timer());
        w16(m + 0xa6, coin() ? 0x354u : 0x355u);
        const float top = ee_read<float>(pv + 0xc);
        if (coin()) wf(m + 0x18, top + rf(-4.0f, 0.5f));
        wf(pv + 0x10, rf(-0.5f, 0.5f));
        wf(m + 0x44, ri(0, 2) == 0 ? 0.0f : rf(-0.5f, 0.5f));
        wf(pv + 0x14, rf(-0.1f, 0.1f));
        p51_fifty();
    };
    // the pod: hit or not, burst before or not
    x["class846"] = [give_hit](uint32_t m, int) {
        if (coin()) give_hit(m, coin() ? 0x210000u : 0x800000u);
        w8(m + 0xbc, ri(0, 3) == 0 ? 1u : 0u);
        p51_fifty();
    };
    // the hatch: up or down, turned or not, its lift's word, its swing near its ends, at its place or near it
    x["class893"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w32(pv + 0x50, u(ri(0, 1)));
        w32(pv + 0x54, u(ri(0, 1)));
        const int32_t s = ee_read<int32_t>(pv + 0x58);
        if (s >= 0 && s < p52_mobys()) w8(p52_moby(s) + 0xbc, u(ri(0, 2)));
        static const float ends[] = {-0.05f, 0.0f, 0.01f, 0.3f, 0.5934119f, 0.62f, 0.7f};
        wf(pv + 0x60, ends[ri(0, 6)]);
        wf(pv + 0x64, rf(-0.3f, 0.3f));
        w8(m + 0xbc, u(ri(0, 1)));
        if ((st == 3 || st == 4) && coin()) {
            Vec4 t = ee_ref<Vec4>(pv);
            if (st == 3) { const Vec4 up = ee_ref<Vec4>(m + 0xd0); t.x -= 1.9f * up.x; t.y -= 1.9f * up.y; t.z -= 1.9f * up.z; }
            const float e = coin() ? 0.0f : rf(-0.05f, 0.05f);
            Vec4 q = ee_ref<Vec4>(m + 0x10); q.x = t.x + e; q.y = t.y; q.z = t.z + e;
            ee_ref<Vec4>(m + 0x10) = q;
            wf(pv + 0x5c, coin() ? std::fabs(e) : rf(0.0f, 0.3f));
        }
        p51_fifty();
    };
    // the waterfall: done before or not, its switch, its roar's sound slot (none, its own, another fall's, a stranger's),
    // near the point 0x1671c0 or not, above its water or below, its frames fallen
    x["class855"] = [](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (pv == 0) return;
        if (st == 0) w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
        const int32_t s = ee_read<int32_t>(pv + 8);
        if (s >= 0 && s < p52_mobys()) w8(p52_moby(s) + 0xbc, u(ri(0, 1)));
        const uint32_t h = ee_read<uint32_t>(pv + 0xc);
        if (h >= 0x100000 && h < 0x2000000) {
            const int k = ri(0, 4);
            const int32_t slot = ri(0, 7);
            w32(h, k == 0 ? 0xffffffffu : u(slot));
            uint32_t other = 0;
            for (int32_t i = 0; i < p52_mobys() && other == 0; ++i)
                if (p52_moby(i) != m && ee_read<int16_t>(p52_moby(i) + 0xa6) == 0x357) other = p52_moby(i);
            const uint32_t owner = k == 1 ? m : k == 2 ? other : k == 3 ? 0u : p52_moby(ri(0, p52_mobys() - 1));
            w32(0x0013e550 + u(slot) * 0x70 + 0x88, owner);
            if (other != 0 && coin()) w8(other + 0x20, u(ri(1, 2)));
        }
        if (coin()) { Vec4 q = p52_near(ee_ref<Vec4>(OV(0x00166dc0)), 70.0f, 3.0f); ee_ref<Vec4>(m + 0x10) = q; }
        const int32_t wb = ee_read<int32_t>(pv + 4);
        if (wb >= 0 && wb < 8 && coin()) wf(m + 0x18, ee_read<float>(ee_read<uint32_t>(OV(0x001612d0)) + u(wb) * 0x1190 + 8) + rf(-1.0f, 20.0f));
        w32(pv + 0x10, u(ri(0, 200)));
        p51_fifty();
    };
    // a moby's sound slot word `w`: none, or a slot of the table 0x13e550 its own (playing or not) or another's
    auto sound_slot = [](uint32_t m, uint32_t w, bool half) {
        const int k = ri(0, 3);
        const int32_t slot = ri(0, 7);
        if (half) w16(w, k == 0 ? 0xffffu : u(slot)); else w32(w, k == 0 ? 0xffffffffu : u(slot));
        w32(0x0013e550 + u(slot) * 0x70 + 0x88, k == 3 ? p52_moby(ri(0, p52_mobys() - 1)) : m);
        w8(0x0013e550 + u(slot) * 0x70 + 0x74, u(ri(0, 1)));
    };
    // the lift: its sound, its camera's ease, its lift near its ends, the hero on its middle, on it elsewhere, or at the other
    // end's height 5 to 32 away, its wait
    x["class877"] = [sound_slot](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        sound_slot(m, pv + 0x74, false);
        w8(m + 0xbc, u(ri(0, 3)));
        static const float eases[] = {0.0f, 0.5f, 0.95f, 1.0f};
        wf(pv + 0x78, eases[ri(0, 3)]);
        static const float lifts[] = {0.0f, 0.004f, 0.5f, 0.996f, 1.0f};
        wf(pv + 0x70, lifts[ri(0, 4)]);
        wf(pv + 0x88, coin() ? rf(-0.003f, 0.003f) : rf(-0.0005f, 0.0005f));
        w32(pv + 0x7c, p53b_timer());
        const int k = ri(0, 3);
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        if (k == 0) { q.x += rf(-0.4f, 0.4f) * 0.7f; q.y += rf(-0.3f, 0.3f); }
        else if (k == 1) { q.x += rf(0.6f, 2.0f); }
        else if (k == 2) { const float a = rf(-3.1f, 3.1f), d = rf(3.0f, 35.0f); q.x += d * std::cos(a); q.y += d * std::sin(a); }
        if (k != 3) p52_hero_at(q);
        w32(0x0013f64c, k < 2 && ri(0, 4) != 0 ? m : 0u);
        w16(0x0013f65e, ri(0, 4) == 0 ? 1u : 0u);
        wf(0x0013f628, ee_read<float>(pv + (coin() ? 0x60 : 0x64)) + rf(-2.5f, 2.5f));
        (void)st;
        p51_fifty();
    };
    // the raft: the hero on its deck (or not), the button, its stage, its progress near 1, its sound, its slots done
    x["class998"] = [sound_slot](uint32_t m, int st) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const bool on = ri(0, 3) != 0;
        w32(0x0013f64c, on ? m : 0u);
        w16(0x0013f65e, ri(0, 4) == 0 ? 1u : 0u);
        if (on) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x += rf(-1.5f, 1.5f); q.y += rf(-1.5f, 1.5f); q.z += 0.5f;
            p52_hero_at(q);
        }
        w32(0x0013cae4, coin() ? 0x10u : 0u);
        w16(pv + 0xb4, u(ri(0, 5)));
        w16(pv + 0xb6, u(ri(0, 4)));
        static const float prog[] = {0.0f, 0.5f, 0.99f, 1.0f};
        wf(pv + 0xc0, prog[ri(0, 3)]);
        wf(pv + 0xc4, rf(0.0f, 0.02f));
        sound_slot(m, pv + 0xb2, true);
        for (uint32_t w : {0xb8u, 0xbcu})
            w8(0x0014c050 + ee_read<uint32_t>(pv + w) + (ee_read<uint32_t>(0x0015ed84) << 4), ri(0, 2) == 0 ? 0xffu : 0u);
        if (st >= 3 && st <= 5 && ee_read<uint32_t>(pv + 0xac) == 0)
            w32(pv + 0xac, ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0x94) * 4));
        p51_fifty();
    };
    // the two talkers: gone or not, their talk's end, their animation, their timers, the hero in front or not
    auto talker = [](uint32_t flag, uint32_t t0, std::vector<int> ends) {
        return [flag, t0, ends](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (pv == 0) return;
            if (st == 0) w8(flag, ri(0, 3) == 0 ? 1u : 0u);
            w16(pv + 4, u(ends[static_cast<size_t>(ri(0, static_cast<int>(ends.size()) - 1))]));
            w8(m + 0x53, u(ri(0, 3) == 0 ? ri(1, 2) : 0));
            w8(m + 0x70, u(ri(0, 3)));
            for (uint32_t o = 0; o < 4; ++o) w32(pv + t0 + o * 4, p53b_timer());
            if (coin()) p53b_hero_front(m, 9.0f);
            w8(0x0015edb0, u(ri(0, 1)));
            if (st == 1) p53b_talk(m, pv);
            p51_fifty();
        };
    };
    x["class919"] = talker(0x0013dd47, 0x144, {-1, -1, 0, 0, 5, 6});
    x["class925"] = talker(0x0013d4ff, 0x154, {-1, -1, 2, 2, 3, 4, 5, 5});
    // (925: its lights by the floor near the point 0x1671c0, the moby moved there)
    x["class925"] = [f = x["class925"]](uint32_t m, int st) {
        w8(m + 0x31, u(ri(0, 1)));
        if (coin()) { Vec4 q = p52_near(ee_ref<Vec4>(OV(0x00166dc0)), 50.0f, 2.0f); ee_ref<Vec4>(m + 0x10) = q; }
        f(m, st);
    };
}

}  // namespace

void batch_114() {
    if (g_only_batch != 114) return;
    g_batch = 114;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // not compared: the null page (a null pointer the random states give the original's code: it runs on stale registers) and the
    // scratchpad words the pose function (0x245f18) spills the caller's $t8 / $t9 into (the transliterated callers have no such registers)
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // 989snd's IOP server (as batch 103's): replies a function of the command
    {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            if (fno == 0x4d) {
                const uint32_t n = ee_read<uint32_t>(send);
                uint32_t p = send + 4;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                    ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                    if (id == 0x38) {
                        disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                        ee_write<uint32_t>(0x00137b10, 0);
                        ee_write<uint32_t>(0x00137b00, 0);
                    }
                    p += 4 + ((size + 3) & ~3u);
                }
                ee_write<uint32_t>(recv, 0xffffffffu);
                ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
                return;
            }
            ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 8, 0xffffffffu);
        });
    }
    // the level after its first gameplay frame (the port's), the hero's equipment existing
    static std::vector<uint8_t> snap;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's mobys whose update function is `fn` (Rilgar's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 5) { l5check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L5:0x%08x  no moby on this level\n", name, fn); return; }
        auto redealt = std::make_shared<int>(0);  // (P5.2: the cases dealt again, printed after the check)
        l5check(name, fn, n, Ret::None, [level, ms, vary, port, redealt] {
            // a random state that sends the port's code through a pointer outside RAM (a garbage pointer the state machine had not set yet: the
            // original's interpreter wraps it, the port refuses) is not a game state: it is dealt again (the RAM put back after the trial)
            static std::vector<uint8_t> saved;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {
                level();
                m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                vary(m);
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                try { port(m); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;  // (an exception leaves the port's frames allocated)
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
                ++*redealt;
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        if (*redealt != 0) std::printf("  %s: %d case(s) dealt again (the port threw)\n", name, *redealt);
    };
    // a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask
    // (the `kinds` bits are mixed with random ones) and a damage at +0x2c
    auto give_hit = [](uint32_t m, uint32_t kinds) {
        if (ri(0, 3) == 0) { w8(m + 0xa4, 0xff); return; }
        const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
        w8(m + 0xa4, idx);
        w32(e + 0x34, ri(0, 8) == 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) : m);
        w32(e + 0x24, ri(0, 2) == 0 ? (R() & ~kinds) : (kinds | (R() & 0xff)));
        { static const float dm[] = {-1.0f, 0.0f, 0.5f, 5.0f, 12.0f}; wf(e + 0x2c, dm[ri(0, 4)]); }
        wfrand(e, 4, -1, 1);
        w32(e + 0x28, u(ri(0, 20)));
    };
    // the hero near the moby or far from it
    auto hero_near = [](uint32_t m) {
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        const float r = ri(0, 2) == 0 ? rf(0, 5) : rf(5, 15);
        const float a = rf(-3.1f, 3.1f);
        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
        ee_ref<Vec4>(0x0013f3d0) = q;
    };

    // the debris / spawns leave w lanes of a VU0 sin / cos in the original (the port has none): ignored only in the checks
    // that need it (P5.1: TrCls::lanes, LANES_DEBRIS / LANES_POOL; none for the checks after the class loop)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    std::vector<uint32_t> debris_lanes, pool_lanes;
    if (g_oracle_level == 5) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { debris_lanes.push_back(a + 0xc); debris_lanes.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) pool_lanes.push_back(pool + k * 0x40 + 0x2c);
        }
    }
    // the ignore list with the lanes `lanes` asks for (LANES_DEBRIS: the spawned mobys' words +0xc / +0x1c, LANES_POOL: the
    // particles' +0x2c)
    auto with_lanes = [&saved_ignored, &debris_lanes, &pool_lanes](uint32_t lanes) {
        std::vector<uint32_t> v = saved_ignored;
        if ((lanes & LANES_DEBRIS) != 0) v.insert(v.end(), debris_lanes.begin(), debris_lanes.end());
        if ((lanes & LANES_POOL) != 0) v.insert(v.end(), pool_lanes.begin(), pool_lanes.end());
        if ((lanes & LANES_TALKSAVE) != 0) {  // (P5.3b: the level's talkers' places the level change saves, 0x13d5b0 + i * 16 + 0 .. 8)
            for (int32_t i = ee_read<int32_t>(OV(0x001c44b8) + 5 * 4); i < ee_read<int32_t>(OV(0x001c44b8) + 6 * 4); ++i)
                for (uint32_t o = 0; o < 0xc; o += 4) v.push_back(0x0013d5b0 + u(i) * 16 + o);
            v.push_back(0x0015ee98);
            v.push_back(0x0015ee9c);
        }
        return v;
    };

    // ---- the classes rilgar_mobys.cpp registers (the level's own transliterated by tools/codegen/generate_all.py and, since the
    // re-glue R5, the shared ports that took the place of Rilgar's copies): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t lanes = LANES_DEBRIS | LANES_POOL; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real; lanes: the spawns' w lanes above ignored)
    static const TrCls trcls[] = {
        {"class35", 0x002d1688, ril_class35_update_l5, 0x0, false, LANES35},
        {"class79", 0x002d7140, ril_class79_update_l5, 0x0, false, LANES79},
        {"class133", 0x002dac80, ril_class133_update_l5, 0x0, false, LANES133},
        {"class258", 0x002f5200, class258_update, 0x0, false},
        {"class341", 0x002f8080, class341_update, 0x0, false},
        {"class439", 0x002f87a8, class439_update, 0x0, false},
        {"class572", 0x002fecf0, amoeboid_update_l1, 0x0, false},
        {"class604", 0x00301988, class604_update, 0x0, false},
        {"class605", 0x00301cd8, zone_marker_update, 0x0, false},
        {"class623", 0x00301f48, ril_class623_update_l5, 0x0, false, LANES623},
        {"class625", 0x00304320, ril_class625_update_l5, 0x0, false, LANES625},
        {"class679", 0x003067b8, class679_update, 0x0, false},
        {"class717", 0x00307910, ril_class717_update_l5, 0x0, false, LANES717},
        {"class758", 0x003098d8, swingshot_target_update, 0x0, false},
        {"class805", 0x00309e40, spawn_point_update_l1, 0x0, false},
        {"class810", 0x0030bdd8, ril_class810_update_l5, 0x0, false, LANES810},
        {"class812", 0x0030bf98, ril_class812_update_l5, 0x0, false, LANES812},
        {"class823", 0x0030c0a8, class823_update, 0x0, false},
        {"class830", 0x0030c5b0, class830_update, 0x0, false},
        {"class831", 0x0030ca80, ril_class831_update_l5, 0x0, false, LANES831},
        {"class832", 0x0030dac8, class832_update, 0x0, false},
        {"class838", 0x0030dc68, ril_class838_update_l5, 0x0, false, LANES838},
        {"class841", 0x0030e1f8, ril_class841_update_l5, 0x0, false, LANES841},
        {"class843", 0x0030e508, ril_class843_update_l5, 0x0, false, LANES843},
        {"class844", 0x0030e7f8, ril_class844_update_l5, 0x0, false},
        {"class846", 0x0030f5c8, ril_class846_update_l5, 0x0, false, LANES846},
        {"class852", 0x00314eb0, ril_class852_update_l5, 0x0, false, LANES852},
        {"class855", 0x003150f0, ril_class855_update_l5, 0x0, false, LANES855},
        {"class877", 0x003156d0, ril_class877_update_l5, 0x0, false, LANES877},
        {"class879", 0x00316110, ril_class879_update_l5, 0x0, false, LANES879},
        {"class893", 0x00316258, ril_class893_update_l5, 0x0, false, LANES893},
        {"class895", 0x003166a0, ril_class895_update_l5, 0x0, false, LANES895},
        {"class918", 0x00316ab8, ril_class918_update_l5, 0x0, false},
        {"class919", 0x00317470, ril_class919_update_l5, 0x0, false, LANES919},
        {"class920", 0x00317aa0, class447_update, 0x0, false},
        {"class925", 0x003180a0, ril_class925_update_l5, 0x0, false, LANES925},
        {"class980", 0x003186f8, class344_update, 0x0, false},
        {"class982", 0x00318a68, ril_class982_update_l5, 0x0, false, LANES982},
        {"class984", 0x00318b98, ril_class984_update_l5, 0x0, false, LANES984},
        {"class998", 0x00318c78, ril_class998_update_l5, 0x78, false, LANES998},
        {"class1099", 0x00319c28, ril_class1099_update_l5, 0x0, false, LANES1099},
        {"class1134", 0x00319c60, gold_bolt_update, 0x6, false},
        {"class1139", 0x0031abe0, class1139_update, 0x0, false},
        {"class1347", 0x0031bf40, ril_class1347_update_l5, 0x0, false},
        {"class1511", 0x0031c8e0, class1511_update, 0x0, false},
        {"class1550", 0x0031d5c8, ril_class1550_update_l5, 0x0, false, LANES1550},
        {"class1633", 0x0031d760, class1633_update, 0x0, false},
        {"class1818", 0x0031e808, sandmouse_update, 0x0, false},
    };
    auto generic_prep = [give_hit, hero_near](uint32_t m, int st, int pass, void (*fn)(uint32_t), bool walk = false) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            for (int k = 0; k < (walk ? st : 1); ++k) {  // (walk: the state machine goes through the states before: a path / a record they make)
                w8(m + 0x20, u(k));
                const uint32_t sp0 = ee_stack_ptr();
                try { fn(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; break; }
            }
        }
        w8(m + 0x20, u(st));
        if (pass >= 1) {
            if (coin()) { w8(m + 0x53, 0); w8(m + 0x52, 0); } w8(m + 0xbc, u(ri(0, 2)));  // (the animation indices stay inside the class's animation table)
            wf(m + 0x48, rf(-3.1f, 3.1f));
            { Vec4 q = ee_ref<Vec4>(m + 0x10); const float r = ri(0, 3) == 0 ? rf(0, 6) : (coin() ? rf(6, 30) : rf(30, 80)); const float a = rf(-3.1f, 3.1f);
              q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-4, 12); ee_ref<Vec4>(P + 0x80) = q; }
            for (uint32_t o = 0x20; o < 0x200; o += 4) {  // a few of the pvars kept as they are typed: floats scaled, small counters moved
                if (ri(0, 9) != 0) continue;
                const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                else if (v >= 1 && v < 16) w32(pv + o, u(ri(1, static_cast<int>(v) + 2)));  // (a 0 stays 0: a null pointer / an unset slot)
            }
            give_hit(m, ri(0, 1) ? 0x210000u : 0x330000u);
            if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
                wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
            }
        }
        if (pass != 1) hero_near(m);
        w32(OV(0x0015f5cc), R() & 0xffff);
        seed_rand();
    };
    if (g_oracle_level == 5 && std::getenv("B114_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
        level();
        const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
        std::printf("hero start %.1f %.1f %.1f\n", h.x, h.y, h.z);
        for (const TrCls& c : trcls) {
            const std::vector<uint32_t> ms = mobys_with(c.addr);
            float best = 1e9f;
            for (uint32_t m : ms) { const Vec4 q = ee_ref<Vec4>(m + 0x10); best = std::fmin(best, std::sqrt((q.x - h.x) * (q.x - h.x) + (q.y - h.y) * (q.y - h.y) + (q.z - h.z) * (q.z - h.z))); }
            std::printf("INFO %s 0x%08x instances %zu nearest %.0f\n", c.name, c.addr, ms.size(), ms.empty() ? -1.0f : best);
        }
    }
    // P5.1: the hand-ported classes' own state on top of generic_prep (their pass 2 and their `_x` checks below)
    const std::map<std::string, std::function<void(uint32_t, int)>> extras = [&give_hit] { Extras x = p51_extras(give_hit); p52_extras(x, give_hit); p53_extras(x); p53b_extras(x, give_hit); return x; }();
    const bool no_lanes = std::getenv("B114_NO_LANES") != nullptr;  // (to find the classes that need the lanes ignored)
    for (const TrCls& c : trcls) {
        if (std::getenv("B114_CLS") != nullptr && std::strcmp(std::getenv("B114_CLS"), c.name) != 0) continue;
        g_ignored_words = with_lanes(no_lanes ? 0u : c.lanes);
        const auto ex = extras.find(c.name);
        const std::function<void(uint32_t, int)> extra = ex != extras.end() ? ex->second : nullptr;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 5 && std::getenv("B114_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B114_ST") == nullptr || std::atoi(std::getenv("B114_ST")) == st) && (std::getenv("B114_PASS") == nullptr || std::atoi(std::getenv("B114_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep, extra](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); if (pass == 2 && extra) { extra(m, st); seed_rand(); } }, 12);
        }
    }
    g_ignored_words = saved_ignored;
    // P5.1: the hand-ported classes again, with their own state in every case (more cases, the states they have)
    {
        struct X { const char* name; uint32_t addr; void (*fn)(uint32_t); std::vector<int> states; uint32_t lanes; };
        const X xs[] = {
            {"class35_x", 0x002d1688, ril_class35_update_l5, {0, 1, 1, 2, 2, 2, 3, 3, 3, 3}, LANES35},
            {"class79_x", 0x002d7140, ril_class79_update_l5, {0, 0, 1, 1, 1, 1, 2, 2, 3}, LANES79},
            {"class133_x", 0x002dac80, ril_class133_update_l5, {0, 1, 1, 1, 2, 2, 3}, LANES133},
            {"class625_x", 0x00304320, ril_class625_update_l5, {0, 1, 1, 1, 2, 2, 2, 3}, LANES625},
            {"class717_x", 0x00307910, ril_class717_update_l5, {0, 1, 1, 2, 2, 2, 2, 2}, LANES717},
            {"class810_x", 0x0030bdd8, ril_class810_update_l5, {0, 1, 1, 1}, LANES810},
            {"class812_x", 0x0030bf98, ril_class812_update_l5, {0, 0, 1, 1, 1}, LANES812},
            {"class831_x", 0x0030ca80, ril_class831_update_l5, {0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 7, 7, 8, 8, 8, 9}, LANES831},
            {"class838_x", 0x0030dc68, ril_class838_update_l5, {0, 1, 1, 1, 1, 2}, LANES838},
            {"class841_x", 0x0030e1f8, ril_class841_update_l5, {0, 1, 1, 2, 2, 3}, LANES841},
            {"class843_x", 0x0030e508, ril_class843_update_l5, {0, 1, 1, 2, 2, 3, 3, 4}, LANES843},
            {"class879_x", 0x00316110, ril_class879_update_l5, {0, 1, 1, 1}, LANES879},
            {"class895_x", 0x003166a0, ril_class895_update_l5, {0, 1, 1, 2, 2, 2}, LANES895},
            {"class982_x", 0x00318a68, ril_class982_update_l5, {0, 1, 1, 1}, LANES982},
            {"class984_x", 0x00318b98, ril_class984_update_l5, {0, 1, 1, 2}, LANES984},
            {"class1099_x", 0x00319c28, ril_class1099_update_l5, {0, 1}, LANES1099},
            {"class1550_x", 0x0031d5c8, ril_class1550_update_l5, {0, 1, 1, 1, 1, 2}, LANES1550},
            {"class846_x", 0x0030f5c8, ril_class846_update_l5, {0}, LANES846},
            {"class852_x", 0x00314eb0, ril_class852_update_l5, {0, 1, 1, 2, 2, 2, 3}, LANES852},
            {"class855_x", 0x003150f0, ril_class855_update_l5, {0, 1, 1, 2, 2, 2, 2, 3}, LANES855},
            {"class877_x", 0x003156d0, ril_class877_update_l5, {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}, LANES877},
            {"class998_x", 0x00318c78, ril_class998_update_l5, {0, 1, 1, 1, 2, 3, 3, 4, 5, 5, 6, 6}, LANES998},
            {"class893_x", 0x00316258, ril_class893_update_l5, {0, 0, 1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7}, LANES893},
            {"class919_x", 0x00317470, ril_class919_update_l5, {0, 0, 1, 1, 1, 1, 2}, LANES919X},
            {"class925_x", 0x003180a0, ril_class925_update_l5, {0, 0, 1, 1, 1, 1, 2}, LANES925X},
            {"class623_x", 0x00301f48, ril_class623_update_l5, {0, 1, 1, 2, 2, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 13, 13, 14, 14, 15, 15, 16}, LANES623},
        };
        for (const X& x : xs) {
            const auto extra = extras.at(std::string(x.name).substr(0, std::string(x.name).size() - 2));
            const std::vector<int> states = x.states;
            auto fn = x.fn;
            g_ignored_words = with_lanes(x.lanes);
            classcheck(x.name, x.addr, x.fn, [states, extra, fn, generic_prep](uint32_t m) {
                const int st = states[static_cast<size_t>(ri(0, static_cast<int>(states.size()) - 1))];
                generic_prep(m, st, ri(1, 2), fn, false);
                extra(m, st);
                seed_rand();
            }, std::strcmp(x.name, "class623_x") == 0 || std::strcmp(x.name, "class831_x") == 0 ? 480 : 240);
        }
        g_ignored_words = saved_ignored;
    }
    p51_helper_checks(level, mobys_with, give_hit, extras);
    p52_helper_checks(level, mobys_with, extras, with_lanes);
    g_extra_noop_hooks.clear();

    if (g_oracle_level == 5 && std::getenv("B114_INFO") != nullptr) {  // the classes with an empty update: how many placements
        level();
        for (const uint32_t fn : {0x0031ace8u, 0x0031d158u}) std::printf("INFO empty 0x%08x instances %zu\n", fn, mobys_with(fn).size());
    }

    // ---- the callbacks and class hooks Rilgar's classes install (a0 = the moby): checked on the mobys that own them (the class' update
    // function installs the callback; a hook is in the class' hook table at +0x2c)
    if (g_oracle_level == 5) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t owner_update; uint32_t hook_slot; };
        static const Cb cbs[] = {
            {"cb_30d678", 0x0030d678, nov_water_751_draw_l1, 0x0030ca80, 0},  // class 831's callback
            {"cb_30d8b8", 0x0030d8b8, class832_draw, 0x0030dac8, 0},  // class 832's callback
            // (the class hook +8, 0x300b98, takes (moby, a position pointer, a target moby, a float): not checked here, unproved)
            {"hook_c", 0x00300be8, [](uint32_t m) { ril_carry_hookc_l5(m, 0, 0, 0.0f); }, 0, 0xc},  // the class hook +0xc (hand-written; as its transliteration was called)
        };
        for (const Cb& cb : cbs) {
            level();
            std::vector<uint32_t> subj;
            if (cb.owner_update != 0) subj = mobys_with(cb.owner_update);
            else
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                    const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                    if (ee_read<uint8_t>(m + 0x20) >= 0xfe || cls == 0 || ee_read<uint32_t>(cls + 0x2c) == 0) continue;
                    if (ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + cb.hook_slot) == cb.addr) subj.push_back(m);
                }
            if (subj.empty()) { std::printf("%-26s L5:0x%08x  no moby owns it on this level\n", cb.name, cb.addr); continue; }
            if (std::getenv("B114_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", cb.name, cb.addr, subj.size());
            const Cb c = cb;
            for (int pass = 0; pass < 3; ++pass)
                classcheck(cb.name, cb.addr, cb.fn, [c, pass, generic_prep](uint32_t m) { generic_prep(m, 0, pass, ril_class831_update_l5); }, 20, subj);
        }
        g_extra_noop_hooks.clear();
    }

    // ---- Rilgar's sound emitter types (1, 3, 5, 6: section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 5) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {
            {"emitter1", 0x0032b478, emitter_fade_box_update}, {"emitter3", 0x0032ba68, emitter_reverb_box_update},
            {"emitter5", 0x0032bbc8, emitter_camera_bed_update}, {"emitter6", 0x0032bc78, emitter_music_box_update},
        };
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L5:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B114_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l5check(e.name, e.addr, 120, Ret::None, [level, ems] {
                level();
                const uint32_t em = ems[static_cast<size_t>(ri(0, static_cast<int>(ems.size()) - 1))];
                const uint32_t prm = ee_read<uint32_t>(em + 8);
                Vec4 q = ee_ref<Vec4>(em + 0x40);  // the hero (the camera's point) inside the emitter's volume or around it
                const float r = ri(0, 2) == 0 ? rf(0, 3) : rf(3, 40);
                const float a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-3, 3);
                ee_ref<Vec4>(0x0013f3d0) = q;
                if (ri(0, 4) == 0) w32(prm + 0xc, 0xffffffffu);
                else if (ri(0, 2) == 0) w32(prm + 0xc, R());
                w32(prm + 8, u(ri(0, 1)));
                if (ri(0, 3) == 0) w32(prm + 0x10, u(ri(0, 20)));
                return Args{{em}, {}};
            }, [fn](A a) { fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- Rilgar's camera types 3 and 8 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 5) {
        // a live camera of the type, the hero put in or near its box, its data varied
        auto cam_setup = [level](int type, bool with_other, uint32_t* other) -> uint32_t {
            level();
            uint32_t cam = 0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == type) cam = OV(0x00167150) + i * 0xa0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)  // (no live camera of the type on the level: any live camera, the functions are checked on its words)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) cam = OV(0x00167150) + i * 0xa0;
            if (cam == 0) return 0;
            const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
            const uint32_t d = ee_read<uint32_t>(e + 0x1c);
            Vec4 at = ee_ref<Vec4>(0x0013f3d0);
            if (ri(0, 3) != 0 && d != 0) {  // inside the first box, or near it
                const int32_t idx = ee_read<int32_t>(d + 0xc);
                if (idx >= 0) at = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(idx) * 0x80 + 0x30);
            }
            at.x += rf(-2, 2); at.y += rf(-2, 2); at.z += rf(-1, 1);
            ee_ref<Vec4>(0x0013f3d0) = at;
            if (ri(0, 2) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = at;
            w8(cam + 0x7c, u(ri(0, 3)));
            w32(cam + 0x74, ri(0, 3) == 0 ? u(ri(0, 6)) : 4u);
            if (with_other && other != nullptr) {
                *other = ri(0, 2) == 0 ? 0u : (coin() ? cam : OV(0x00167150u));
                if (*other != 0) { w16(*other + 0x7e, u(ri(0, 2) == 0 ? 0 : 1)); w8(*other + 0x7c, u(ri(0, 3))); }
            }
            return cam;
        };
        struct CamT { int type; uint32_t accept, init, update, exit; };
        static const CamT cams[] = {{3, 0x00326280, 0x00326288, 0x00325800, 0x003264d8}, {8, 0x0032a6d8, 0x0032a6e0, 0x0032a758, 0x0032a788}};
        for (const CamT& ct : cams) {
            level();
            bool any = false, own = false;
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) { any = true; if (ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == ct.type) own = true; }
            if (!any) { std::printf("cam_type%d                 no live camera on this level\n", ct.type); continue; }
            if (!own) std::printf("cam_type%d                 no live camera of the type on this level: checked on another live camera's words\n", ct.type);
            const int type = ct.type;
            static char nm[4][48];
            static int k = 0;
            auto name = [&](const char* what) { char* p = nm[k++ & 3]; std::snprintf(p, 48, "cam_type%d_%s", type, what); return static_cast<const char*>(p); };
            l5check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(zero_stub()) : 0u); });
            l5check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? cam_type3_init : race_cam_init)(a.i[0]); return uint64_t{0}; });
            l5check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? cam_type3_update : race_cam_update)(a.i[0]); return uint64_t{0}; });
            l5check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 3 ? cam_type3_exit : empty_update)(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- Rilgar's camera hook (0x328f80, the pointer its setter 0x328f48 puts at 0x167410) and the callback it queues (0x328fe0)
    if (g_oracle_level == 5) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167400;
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00328f80u);
            if (ri(0, 3) == 0) w32(OV(0x0015ef4c), u(ri(0, 15)));  // the queue's count (it holds up to 16)
            wf(0x001621a4, rf(0, 0.5f));  // $gp - 0x4a5c: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));
        };
        l5check("cam0_hook", 0x00328f80, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l5check("cam_callback", 0x00328fe0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- Rilgar's class hooks: +4 (0x300b48), +0x10 (0x300c18: pvars + 0xc0; the matcher pairs it with class 749's 0x2d5800), and the dispatcher on it
    if (g_oracle_level == 5) {
        l5check("hook_10", 0x00300c18, 60, Ret::Int32, [level] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t m = ri(0, 4) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
            if (m != 0 && ri(0, 3) == 0) w32(m + 0x78, 0);
            return Args{{m}, {}};
        }, [](A a) { return r32(ril_hook_10_u(a.i[0], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
    }
    if (g_oracle_level == 5) {
        level();
        std::vector<uint32_t> hm;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 0x10) != 0) hm.push_back(m);
        }
        if (hm.empty()) std::printf("class_hook_10              no moby with a +0x10 hook on this level\n");
        else
            l5check("class_hook_10", ovl_code_lookup(0x002db890), 120, Ret::Int32, [level, hm] {
                level();
                const uint32_t m = hm[static_cast<size_t>(ri(0, static_cast<int>(hm.size()) - 1))];
                if (ri(0, 6) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                if (ri(0, 6) == 0) w32(m + 0x78, 0);
                return Args{{m}, {}};
            }, [](A a) { return r32(class_hook_10(a.i[0])); });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 5) {
        static const uint32_t empties[] = {0x0032a788};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l5check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
