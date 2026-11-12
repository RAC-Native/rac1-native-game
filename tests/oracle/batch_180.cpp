// Oracle batch 180 (shared/weapon-classes, release 0.2.0; hand-ported for 1.0, issue #148): the shared (Veldin-address) moby classes
// no level ports on its own, at Veldin's addresses (src/game/ovl/weapon_classes.cpp, drone.cpp, morph_chicken.cpp, morph.cpp,
// morph_hud.cpp; until 1.0 generated from tools/codegen/gen/mkall/veldin_weapon_classes.json):
//   class 1143 (the Vendor Orb, spawned by the Vendor's update 0x2a57a8 on its first frame)       vendor_orb_update     0x2df730
//   class 479  (the drones of the Drone Device, item 0x18: 0x2cdcc0 / 0x2ce1f0)                   drone_update          0x2ce358
//   class 270  (the Morph-o-Ray's chicken: 0x2c9348, from the Morph-o-Ray's hand update 0x2bc7e8) chicken_update        0x2c97e0
//   class 428  (spawned by the chicken's update through 0x2cbf68)                                 class428_update       0x2cc210
//   class 804  (a piece on the Morph-o-Ray's hand moby, class 0xb9)                                class804_update       0x2d7f88
//   class 166  (four linked mobys waited for in state 0x50; no spawner in Veldin's code, placed nowhere we ran)
//                                                                                                   class166_update       0x2b3458
// Also the callbacks the Morph-o-Ray and the drones reach (no port before): the Morph-o-Ray's HUD element (controller slot 4, key 0x7533,
// set by its hand update 0x2bc7e8: start 0x236b10 / update 0x236d00 / draw 0x236e50) and the particle types 5 (0x269c70) and 0x37
// (0x2726d8, the drones' particles, spawned through 0x2725d8).
// On RAM dumps of headless runs of the port (rac1-native-game, branch shared/weapon-classes, from its root), in
// assets/dumps/oracle/weapons_dumps/ (skipped without them); every run with RAC1_HEADLESS=1 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2
// RAC1_RAMDUMP=<f>,<file>:
//   drone_L0_330 / drone_L0_450   RAC1_GOTO=0@60 RAC1_GIVE_FRAME=240 RAC1_GIVE_ITEMS=0x18 RAC1_AMMO=24:20 RAC1_EQUIP=0x18
//                                 RAC1_POKE8=300:0x141345=1 RAC1_SIM_INPUT='seq:280-600:ly=-0.3'   (the six drones around the hero;
//                                 0x141345 = 0x13f350 + 0x1ff5, the request the quick select sets for item 0x18: use_agents 0x2cdcc0)
//   drone_L0_enemy (f = 420)      the same with RAC1_HERO_AT=749@270,-6,0,1,0 (the hero next to an enemy 749)
//   morph_L0_330 / _400 / _520    RAC1_GOTO=0@60 RAC1_GIVE_FRAME=240 RAC1_GIVE_ITEMS=0x15 RAC1_AMMO=21:20 RAC1_EQUIP=0x15
//                                 RAC1_HERO_AT=749@270,-4,0,1,0 RAC1_SIM_INPUT='seq:300-480:o'   (the Morph-o-Ray fired at two
//                                 enemies 749: its HUD element live, the chickens (270) made and walking)
//   morph_L0_312 / _318           the same, the frames the ray turns the first enemy (a type-5 particle live)
//   orb_L3_1350 / orb_L3_1450     RAC1_LEVEL=3 RAC1_HERO_AT=11@1300,0,1.5,0 RAC1_SIM_INPUT='seq:1400+4:t'   (Kerwan's Vendor and its
//                                 orb; at 1450 the Vendor's screen open, game mode 5)
// Each case takes one live moby of the class in the dump (a particle of the type, the HUD slot) and varies what its update reads (its
// state byte, the game mode 0x15f5c4, its position against the hero's, its timers and pvars, the random seed). Made-up subjects, as no run
// met them: class 428 is spawned in the case's set-up through 0x2cbf68 (the chicken's spawner, the port: checked itself inside the chicken's
// update) at a chicken of the dump; classes 804 and 166 (no spawner in any overlay's code: an immediate scan of all 19 overlays) are a
// live moby of the dump made one (class 0x324 / 0xa6, pvars in the test area: 804's +0 the Morph-o-Ray's hand moby (class 0xb9) of the
// dump or 0; 166's four moby indices of the dump). The Vendor Orb's update is never called in the game (the Vendor sets its no-update
// flag 0x34 bit 1 at the spawn and never clears it, 0x2a57a8): it is checked called directly on the orbs of the dumps.
// Levels: the dumps' checks on 0 (Veldin) and 3 (Kerwan, the Vendor Orb); the level-built checks at the end (the drones and
// their trails, the chicken and its debris, classes 804 and 166 on the level's own overlay, no dumps) on any level: run on 0 .. 18.
// The chicken's cases vary the gold Morph-o-Ray flag (0x13e535: its joint scale, its debris twice the size).
#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapon_classes.h>
#include <runtime/ee/ee_interpreter.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& wc_dump(const std::string& path) {
    static std::map<std::string, std::vector<uint8_t>> cache;
    std::vector<uint8_t>& d = cache[path];
    if (d.empty()) {
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            d.resize(0x02000000 + 0x4000);
            if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
            std::fclose(f);
        }
    }
    return d;
}
void wc_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
uint32_t wc_first() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t wc_count() {
    return static_cast<uint32_t>(ee_read<int32_t>(OV(0x0015ffe0)) - static_cast<int32_t>(wc_first())) >> 8;
}
std::vector<uint32_t> wc_live(int16_t cls) {
    std::vector<uint32_t> out;
    const uint32_t first = wc_first();
    const uint32_t n = wc_count() + 1;
    for (uint32_t k = 0; k < n && k < 3000; ++k) {
        const uint32_t m = first + k * 0x100;
        if (ee_read<int16_t>(m + 0xa6) == cls && ee_read<uint8_t>(m + 0x20) < 0xfd) out.push_back(m);
    }
    return out;
}
uint32_t pick(const std::vector<uint32_t>& v) { return v.empty() ? 0u : v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }
constexpr uint32_t HERO = 0x0013f350;  // (the hero struct: +0x80 its position, +0x2084 the mode word)

// what every class's update may read of the frame: the game mode, the random seed, the moby moved around the hero
void common_vary(uint32_t m, float reach) {
    seed_rand();
    const int gm = ri(0, 5);
    if (gm == 1) w32(OV(0x0015f5c4), 0); else if (gm == 2) w32(OV(0x0015f5c4), 2); else if (gm == 3) w32(OV(0x0015f5c4), u(ri(3, 7)));
    if (ri(0, 2) == 0) {
        for (uint32_t k = 0; k < 2; ++k) wf(m + 0x10 + k * 4, ee_read<float>(HERO + 0x80 + k * 4) + rf(-reach, reach));
        wf(m + 0x18, ee_read<float>(HERO + 0x88) + rf(-reach * 0.5f, reach * 0.5f));
    }
    if (coin()) wf(m + 0x48, rf(-7.0f, 7.0f));
}

struct Subject {
    const char* name;
    int16_t cls;
    uint32_t fn;
    void (*port)(uint32_t);
    std::vector<std::string> dumps;
    int level;
    int states;  // the state byte's range varied (0 .. states-1), 0 = left as is
    void (*vary)(uint32_t m);
};

void vary_orb(uint32_t m) {
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    if (coin()) wf(pv + 0x40, rf(-7.0f, 7.0f));
}
void vary_drone(uint32_t m) {
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    if (ri(0, 3) == 0) w32(HERO + 0x2084, coin() ? 0x32u : u(ri(0, 0x40)));
    if (coin()) w8(HERO + 0x1ff4, u(ri(0, 8)));
    if (coin()) wf(HERO + 0x1ff8, rf(-7.0f, 7.0f));
    if (coin()) wfrand(pv + 0x20, 3, -3.0f, 3.0f);
    if (coin()) w16(pv + 0x56, u(ri(0, 120)));
    if (coin()) wfrand(pv, 3, -0.4f, 0.4f);
    if (coin()) w8(m + 0x23, u(ri(0, 12)));  // (the fade of state 4)
}
void vary_chicken(uint32_t m) {
    if (coin()) w8(m + 0x70, R() & 0xff);
    if (coin()) w8(m + 0x22, R() & 0xff);
    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
    if (coin()) w32(pv + 0x10, u(ri(0, 4)));  // (+0x14 is the carry data's pointer: left as the dump has it)
    if (ri(0, 4) == 0) w8(m + 0x31, 0);  // (not drawn: the end under load)
    // the gold Morph-o-Ray (0x13e520 + item 0x15: its chickens' joint scale, their debris twice the size), the health +0x78
    // below 1 (the burst: feathers, or with puffs +0x74 an explosion, then the debris, fewer under load), the timers +0x64 /
    // +0x66 / +0x70 / +0x7c about to run out, the frame's load factors 0x15f5d0 / 0x15f5d4 (the clucks and the debris cut)
    w8(0x0013e535, coin() ? 1u : 0u);
    if (ri(0, 3) == 0) wf(pv + 0x78, rf(-0.5f, 1.5f));
    if (ri(0, 3) == 0) w32(pv + 0x74, u(ri(0, 3)));
    if (coin()) w32(pv + 0x70, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 3)));
    if (coin()) w16(pv + 0x64, u(ri(0, 2)));
    if (coin()) w16(pv + 0x66, u(ri(0, 2)));
    if (coin()) w32(pv + 0x7c, u(ri(0, 2)));
    if (ri(0, 3) == 0) {
        wf(OV(0x0015f5d0), rf(0.5f, 1.2f));
        wf(OV(0x0015f5d4), rf(0.5f, 1.2f));
    }
}

void run_subject(const Subject& s) {
    for (const std::string& dn : s.dumps) {
        const std::string name = std::string(s.name) + " " + dn;
        if (g_oracle_level != s.level) {
            skip_check(name.c_str(), s.fn, s.level == 0 ? "the dump is Veldin's: ORACLE_LEVEL=0" : "the dump is Kerwan's: ORACLE_LEVEL=3");
            continue;
        }
        const std::vector<uint8_t>& d = wc_dump(dump_path("weapons_dumps/") + dn + ".bin");
        if (d.empty()) {
            skip_check(name.c_str(), s.fn, "no RAM dump (assets/dumps/oracle/weapons_dumps)");
            continue;
        }
        wc_load(d);
        std::printf("%s: %zu live class-%d mobys\n", dn.c_str(), wc_live(s.cls).size(), s.cls);
        if (wc_live(s.cls).empty()) {
            skip_check(name.c_str(), s.fn, "no live moby of the class in the dump");
            continue;
        }
        auto gen = [&d, &s] {
            wc_load(d);
            const uint32_t m = pick(wc_live(s.cls));
            if (m == 0) return Args{{0}, {}};
            if (s.states > 0 && coin()) w8(m + 0x20, u(ri(0, s.states - 1)));
            common_vary(m, 6.0f);
            if (s.vary != nullptr) s.vary(m);
            return Args{{m}, {}};
        };
        void (*port)(uint32_t) = s.port;
        check(name.c_str(), s.fn, 80, Ret::None, gen, [port](A a) { if (a.i[0] != 0) port(a.i[0]); return uint64_t{0}; });
    }
}

const std::vector<uint8_t>* wc_need(const std::string& name, uint32_t fn, const std::string& dn, int level) {
    if (g_oracle_level != level) {
        skip_check(name.c_str(), fn, level == 0 ? "the dump is Veldin's: ORACLE_LEVEL=0" : "the dump is Kerwan's: ORACLE_LEVEL=3");
        return nullptr;
    }
    const std::vector<uint8_t>& d = wc_dump(dump_path("weapons_dumps/") + dn + ".bin");
    if (d.empty()) {
        skip_check(name.c_str(), fn, "no RAM dump (assets/dumps/oracle/weapons_dumps)");
        return nullptr;
    }
    return &d;
}

// a live moby of the dump (not the hero, not one of the excluded classes) made a moby of class `cls` with pvars in the test area
uint32_t made_up(int16_t cls, std::initializer_list<int16_t> keep) {
    const uint32_t n = wc_count(), first = wc_first();
    for (int tries = 0; tries < 80; ++tries) {
        const uint32_t c = first + u(ri(1, static_cast<int>(n))) * 0x100;
        if (ee_read<uint8_t>(c + 0x20) >= 0xfd) continue;
        bool skip = false;
        for (int16_t k : keep) skip = skip || ee_read<int16_t>(c + 0xa6) == k;
        if (skip) continue;
        for (uint32_t k = 0; k < 0x80; k += 4) w32(M3 + k, 0);
        w16(c + 0xa6, u(cls));
        w32(c + 0x78, M3);
        // (out of the spatial grid, rect +0xa0 negative: a moby of the dump that never had its transform updated (e.g. the hand's
        // pieces) has rect 0 with its id in no cell, which moby_retire's grid move 0x250b20 traps on (`teq`) in the original)
        if (static_cast<int32_t>(ee_read<uint32_t>(c + 0xa0)) >= 0 && coin()) moby_grid_move(c, 0x80807f7fu);
        else w32(c + 0xa0, 0x80807f7fu);
        return c;
    }
    return 0;
}

// class 166 (pvars: +0x10..+0x1c four moby indices, which state 0 turns into pointers at +0x20..+0x2c; +4 / +0xc timers, +8 a count)
void run_166(const std::string& dn, int level) {
    const std::string name = "class166_update " + dn;
    const std::vector<uint8_t>* dp = wc_need(name, 0x002b3458, dn, level);
    if (dp == nullptr) return;
    const std::vector<uint8_t>& d = *dp;
    auto gen = [&d] {
        wc_load(d);
        const uint32_t m = made_up(0xa6, {479});
        if (m == 0) return Args{{0}, {}};
        const uint32_t pv = M3, n = wc_count(), first = wc_first();
        const int st = ri(0, 2);
        w8(m + 0x20, u(st == 2 ? ri(2, 5) : st));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t idx = u(ri(1, static_cast<int>(n)));
            w32(pv + 0x10 + k * 4, idx);
            w32(pv + 0x20 + k * 4, first + idx * 0x100);
            if (ri(0, 3) == 0) w8(first + idx * 0x100 + 0x20, 0x50);
        }
        w32(pv + 4, u(ri(0, 3)));
        w32(pv + 8, u(ri(0, 4)));
        w32(pv + 0xc, u(ri(0, 3)));
        common_vary(m, 6.0f);
        return Args{{m}, {}};
    };
    check(name.c_str(), 0x002b3458, 80, Ret::None, gen, [](A a) { if (a.i[0] != 0) class166_update(a.i[0]); return uint64_t{0}; });
}

// class 804 on the Morph-o-Ray's dumps: pvars +0 the hand moby (class 0xb9) or 0, +4 / +0xc floats, +8
void run_804(const std::string& dn) {
    const std::string name = "class804_update " + dn;
    const std::vector<uint8_t>* dp = wc_need(name, 0x002d7f88, dn, 0);
    if (dp == nullptr) return;
    const std::vector<uint8_t>& d = *dp;
    auto gen = [&d] {
        wc_load(d);
        const uint32_t hand = pick(wc_live(0xb9));
        const uint32_t m = made_up(0x324, {0xb9, 270});
        if (m == 0) return Args{{0}, {}};
        const uint32_t pv = M3;
        w32(pv, ri(0, 7) == 0 ? 0u : hand);
        if (hand != 0 && ri(0, 4) == 0) w8(hand + 0x20, coin() ? 0xfeu : 0xfdu);
        wf(pv + 4, coin() ? rf(0.0f, 2.0f) : rf(0.0f, 30.0f));
        w32(pv + 8, u(ri(0, 3)));
        wf(pv + 0xc, rf(0.0f, 3.0f));
        common_vary(m, 6.0f);
        return Args{{m}, {}};
    };
    check(name.c_str(), 0x002d7f88, 80, Ret::None, gen, [](A a) { if (a.i[0] != 0) class804_update(a.i[0]); return uint64_t{0}; });
}

// class 270's hook +0xc (0x2caf10, called by 0x2dc9f0 when the carried chicken's countdown runs out) on a chicken of the dump
void run_hook_c(const std::string& dn) {
    const std::string name = "chicken_hook_c " + dn;
    const std::vector<uint8_t>* dp = wc_need(name, 0x002caf10, dn, 0);
    if (dp == nullptr) return;
    const std::vector<uint8_t>& d = *dp;
    auto gen = [&d] {
        wc_load(d);
        const uint32_t m = pick(wc_live(270));
        if (m == 0) return Args{{0}, {}};
        if (coin()) w8(m + 0x20, u(ri(0, 6)));
        common_vary(m, 6.0f);
        return Args{{m}, {}};
    };
    check(name.c_str(), 0x002caf10, 60, Ret::None, gen, [](A a) { if (a.i[0] != 0) chicken_hook_c(a.i[0]); return uint64_t{0}; });
}

// class 428: spawned in the set-up through 0x2cbf68 (a1 its position, a2 its velocity) at a chicken of the dump, then updated
void run_428(const std::string& dn) {
    const std::string name = "class428_update " + dn;
    const std::vector<uint8_t>* dp = wc_need(name, 0x002cc210, dn, 0);
    if (dp == nullptr) return;
    const std::vector<uint8_t>& d = *dp;
    auto gen = [&d] {
        wc_load(d);
        const uint32_t c = pick(wc_live(270));
        if (c == 0) return Args{{0}, {}};
        seed_rand();
        for (uint32_t k = 0; k < 3; ++k) wf(T + k * 4, ee_read<float>(c + 0x10 + k * 4) + rf(-0.5f, 0.5f));
        wf(T + 12, 1.0f);
        wfrand(T + 0x10, 3, -0.3f, 0.3f);
        wf(T + 0x1c, 0.0f);
        const uint32_t m = chicken_debris_spawn(c, T, T + 0x10);
        if (m == 0) return Args{{0}, {}};
        const int ticks = ri(0, 40);  // (a while into its flight)
        for (int k = 0; k < ticks && ee_read<uint8_t>(m + 0x20) < 0xfd; ++k) class428_update(m);
        if (ee_read<uint8_t>(m + 0x20) >= 0xfd) return Args{{0}, {}};
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (coin()) w32(pv + 0x1c, u(ri(0, 80)));
        if (coin()) w32(pv + 0x24, u(ri(-3, 3)));
        if (coin()) w8(m + 0x31, u(ri(0, 1)));
        common_vary(m, 3.0f);
        return Args{{m}, {}};
    };
    check(name.c_str(), 0x002cc210, 80, Ret::None, gen, [](A a) { if (a.i[0] != 0) class428_update(a.i[0]); return uint64_t{0}; });
}

std::vector<uint32_t> live_particles(uint8_t type) {
    std::vector<uint32_t> out;
    const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
    const int32_t hi = ee_read<int32_t>(OV(0x00160174));
    for (int32_t i = 0; i <= hi && i < 4096; ++i) {
        const uint32_t p = base + u(i) * 64;
        if (ee_read<uint8_t>(p) == type && ee_read<int8_t>(p + 1) >= 0) out.push_back(p);
    }
    return out;
}

void run_particle(const char* fname, uint8_t type, uint32_t fn, void (*port)(uint32_t), const std::vector<std::string>& dumps) {
    for (const std::string& dn : dumps) {
        const std::string name = std::string(fname) + " " + dn;
        const std::vector<uint8_t>* dp = wc_need(name, fn, dn, 0);
        if (dp == nullptr) continue;
        const std::vector<uint8_t>& d = *dp;
        wc_load(d);
        std::printf("%s: %zu live type-0x%x particles\n", dn.c_str(), live_particles(type).size(), type);
        if (live_particles(type).empty()) {
            skip_check(name.c_str(), fn, "no live particle of the type in the dump");
            continue;
        }
        auto gen = [&d, type] {
            wc_load(d);
            const uint32_t p = pick(live_particles(type));
            if (p == 0) return Args{{0}, {}};
            seed_rand();
            { const int k = ri(0, 3); if (k == 0) w32(p + 0x20, 0); else if (k == 1) w32(p + 0x20, u(ri(1, 2))); else if (k == 2) w32(p + 0x20, u(ri(3, 60))); }
            if (coin()) wf(p + 0xc, rf(0.05f, 3.0f));
            if (coin()) wfrand(p + 0x24, 3, -0.5f, 0.5f);
            if (coin()) w8(p + 0x38, R() & 0xff);
            return Args{{p}, {}};
        };
        check(name.c_str(), fn, 60, Ret::None, gen, [port](A a) { if (a.i[0] != 0) port(a.i[0]); return uint64_t{0}; });
    }
}

// the Morph-o-Ray's HUD element: controller slot 4 (0x17dc50 + 4 * 0x90) when its start is 0x236b10
void run_hud(const char* fname, uint32_t fn, void (*port)(uint32_t), const std::vector<std::string>& dumps) {
    for (const std::string& dn : dumps) {
        const std::string name = std::string(fname) + " " + dn;
        const std::vector<uint8_t>* dp = wc_need(name, fn, dn, 0);
        if (dp == nullptr) continue;
        const std::vector<uint8_t>& d = *dp;
        wc_load(d);
        const uint32_t e = OV(0x0017dc50) + 4 * 0x90;
        std::printf("%s: HUD slot 4 key 0x%x start %08x update %08x draw %08x\n", dn.c_str(), ee_read<uint32_t>(e + 0x20),
                    ee_read<uint32_t>(e + 0x10), ee_read<uint32_t>(e + 0x14), ee_read<uint32_t>(e + 0x18));
        if (ee_read<uint32_t>(e + 0x10) != OVF(0x00236b10)) {
            skip_check(name.c_str(), fn, "the Morph-o-Ray's HUD element is not in slot 4 in the dump");
            continue;
        }
        auto gen = [&d, e] {
            wc_load(d);
            seed_rand();
            if (coin()) w32(e + 0x6c, u(ri(-6, 30)));
            if (coin()) w32(e + 0x7c, u(ri(0, 12)));
            if (ri(0, 3) == 0) w32(OV(0x0015f5c4), u(ri(0, 6)));
            if (coin()) w32(0x0013d428 + 0x15 * 4, u(ri(0, 40)));  // (the Morph-o-Ray's ammo)
            return Args{{e}, {}};
        };
        check(name.c_str(), fn, 60, Ret::None, gen, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    }
}

// ---- On the level's own overlay, no dumps (every level runs its own copy of these classes; the dumps above are Veldin's):
// the level laid out and a gameplay frame run, then the subjects made near the hero: 1 .. 6 drones (0x2ce1f0, as the Drone
// Device does) with their trails (0x2cde78 / 0x2725d8), a chicken (class 270 spawned) and its debris (0x2cbf68), classes
// 804 and 166 made up on live mobys of the level (804's hand: a live moby made class 0xb9). The spawns are the ports' (set-up).
const std::vector<uint8_t>& lb_level() {
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    uint8_t* ram = ee_get_ram_base();
    if (snap.empty() || snap_level != g_oracle_level) {
        veldin_boot();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        if (g_oracle_level == 14) w8(0x0013d3f0, 1);
        if (g_oracle_level == 18) w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as batches 270 / 400)  // (Oltanis: the arrival scene seen, as batches 221 / 400: else its stream is read from the disc)
        gameplay_frame();
        snap.assign(ram, ram + 0x02000000);
        snap_level = g_oracle_level;
    }
    return snap;
}
void lb_load() {
    std::memcpy(ee_get_ram_base(), lb_level().data(), 0x02000000);
    render_view_setup();
    collide_vu_reset();
    oracle_vu_contact_reset();
}
// a spawn of a class the level may not have loaded (its class data garbage: the spawn throws): 0
template <typename F>
uint32_t lb_try(F f) {
    try { return f(); } catch (const std::exception&) { return 0u; }
}
Vec4 lb_near_hero(float reach) {
    Vec4 p = V(HERO + 0x80);
    p.x = p.x + rf(-reach, reach);
    p.y = p.y + rf(-reach, reach);
    p.z = p.z + rf(0.0f, 2.0f);
    return p;
}
// 1 .. 6 drones near the hero (each one's swarm slot filed by the spawn), one of them returned
uint32_t lb_drones() {
    uint32_t prev = 0, any = 0;
    const int n = ri(1, 6);
    for (int k = 0; k < n; ++k) {
        const Vec4 pos = lb_near_hero(3.0f);
        const uint32_t d = lb_try([&] { return spawn_moby_1df_v(prev, pos); });
        if (d == 0) break;
        prev = d;
        if (any == 0 || coin()) any = d;
    }
    return any;
}
uint32_t lb_chicken() {
    const uint32_t c = lb_try([] { return moby_spawn(0x10e); });
    if (c == 0) return 0;
    V(c + 0x10) = lb_near_hero(5.0f);
    moby_update_transform(c);
    return c;
}
// a live moby of the level (not the hero) with the pvars of its class's
uint32_t lb_live() {
    std::vector<uint32_t> v;
    const uint32_t first = wc_first(), n = wc_count();
    for (uint32_t k = 1; k <= n && k < 3000; ++k) {
        const uint32_t m = first + k * 0x100;
        if (ee_read<uint8_t>(m + 0x20) < 0xfd) v.push_back(m);
    }
    return pick(v);
}

void run_level_built() {
    char name[64];
    // the drones: a state (0 .. 6), a target (a live moby of the level, its class kept or not), the swarm's counters
    std::snprintf(name, sizeof name, "drone_update L%d", g_oracle_level);
    // (the collision's scratchpad work area 0x70002000..0x70003800, the meshes' vertices, is not compared here: in one case of
    // 120 on Veldin the original leaves a word there (0x70003718: 0x400) that the shared collision port does not (a VU
    // register's leftover lane, cf. sphere_mesh_collide's note); the generated drone code (0.x) fails it the same way, and
    // nothing reads the area after the query)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    for (uint32_t a = 0x70002000u; a < 0x70003800u; a += 4) g_ignored_words.push_back(a);
    lb_load();
    if (lb_drones() == 0) {
        skip_check(name, 0x002ce358, "the drones' class (479) is not loaded on this level");
    } else {
        check(name, 0x002ce358, 120, Ret::None, [] {
            lb_load();
            const uint32_t d = lb_drones();
            if (d == 0) return Args{{0}, {}};
            seed_rand();
            const uint32_t pv = ee_read<uint32_t>(d + 0x78);
            w8(d + 0x20, u(ri(0, 6)));
            if (coin()) {
                const uint32_t t = lb_live();
                w32(pv + 0x30, t);
                w32(pv + 0x48, coin() && t != 0 ? static_cast<uint32_t>(static_cast<int32_t>(ee_read<int16_t>(t + 0xa6))) : u(ri(0, 0x400)));
                if (t != 0 && coin()) {  // (the drone on its target: the hit)
                    wf(d + 0x10, ee_read<float>(t + 0x10) + rf(-1.0f, 1.0f));
                    wf(d + 0x14, ee_read<float>(t + 0x14) + rf(-1.0f, 1.0f));
                    wf(d + 0x18, ee_read<float>(t + 0x18) + rf(-0.5f, 2.0f));
                }
            }
            if (ri(0, 2) == 0) common_vary(d, 4.0f);
            vary_drone(d);
            return Args{{d}, {}};
        }, [](A a) { if (a.i[0] != 0) drone_update(a.i[0]); return uint64_t{0}; });
    }
    g_ignored_words = saved_ignored;
    // the drones' trails
    std::snprintf(name, sizeof name, "particle_37_update L%d", g_oracle_level);
    check(name, 0x002726d8, 60, Ret::None, [] {
        lb_load();
        const uint32_t d = lb_drones();
        if (d == 0) return Args{{0}, {}};
        drone_trails_attach(d);
        const uint32_t pv = ee_read<uint32_t>(d + 0x78);
        const uint32_t p = ee_read<uint32_t>(pv + (coin() ? 0x4c : 0x50));
        if (p == 0) return Args{{0}, {}};
        seed_rand();
        if (ri(0, 5) == 0) w8(d + 0x20, coin() ? 0xfeu : 0xfdu);
        if (ri(0, 5) == 0) w32(p + 0x38, u(ri(0, 0x400)));
        common_vary(d, 4.0f);
        return Args{{p}, {}};
    }, [](A a) { if (a.i[0] != 0) particle_37_update(a.i[0]); return uint64_t{0}; });
    // the chicken: a state (0 .. 4, 6; 5 is carried: its carry data is not made here), the burst, the gold flag, the load
    std::snprintf(name, sizeof name, "chicken_update L%d", g_oracle_level);
    lb_load();
    if (lb_chicken() == 0) {
        skip_check(name, 0x002c97e0, "the chicken's class (270) is not loaded on this level");
    } else {
        check(name, 0x002c97e0, 150, Ret::None, [] {
            lb_load();
            const uint32_t c = lb_chicken();
            if (c == 0) return Args{{0}, {}};
            seed_rand();
            const int st = ri(0, 5);
            w8(c + 0x20, u(st == 5 ? 6 : st));
            const uint32_t pv = ee_read<uint32_t>(c + 0x78);
            wf(pv + 0x78, rf(0.5f, 2.0f));
            common_vary(c, 6.0f);
            vary_chicken(c);
            return Args{{c}, {}};
        }, [](A a) { if (a.i[0] != 0) chicken_update(a.i[0]); return uint64_t{0}; });
        // its debris
        std::snprintf(name, sizeof name, "class428_update L%d", g_oracle_level);
        check(name, 0x002cc210, 80, Ret::None, [] {
            lb_load();
            const uint32_t c = lb_chicken();
            if (c == 0) return Args{{0}, {}};
            seed_rand();
            w8(0x0013e535, coin() ? 1u : 0u);
            for (uint32_t k = 0; k < 3; ++k) wf(T + k * 4, ee_read<float>(c + 0x10 + k * 4) + rf(-0.5f, 0.5f));
            wf(T + 12, 1.0f);
            wfrand(T + 0x10, 3, -0.3f, 0.3f);
            wf(T + 0x1c, 0.0f);
            const uint32_t m = lb_try([c] { return chicken_debris_spawn(c, T, T + 0x10); });
            if (m == 0) return Args{{0}, {}};
            const int ticks = ri(0, 40);
            for (int k = 0; k < ticks && ee_read<uint8_t>(m + 0x20) < 0xfd; ++k) class428_update(m);
            if (ee_read<uint8_t>(m + 0x20) >= 0xfd) return Args{{0}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) w32(pv + 0x1c, u(ri(0, 80)));
            if (coin()) w8(m + 0x31, u(ri(0, 1)));
            common_vary(m, 3.0f);
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) class428_update(a.i[0]); return uint64_t{0}; });
    }
    // class 804 on a live moby made the hand (class 0xb9)
    std::snprintf(name, sizeof name, "class804_update L%d", g_oracle_level);
    check(name, 0x002d7f88, 80, Ret::None, [] {
        lb_load();
        seed_rand();
        const uint32_t hand = lb_live();
        const uint32_t m = made_up(0x324, {});
        if (m == 0 || hand == 0 || hand == m) return Args{{0}, {}};
        if (ri(0, 5) != 0) w16(hand + 0xa6, 0xb9);
        const uint32_t pv = M3;
        w32(pv, ri(0, 7) == 0 ? 0u : hand);
        wf(pv + 4, coin() ? rf(0.0f, 2.0f) : rf(0.0f, 30.0f));
        w32(pv + 8, u(ri(0, 3)));
        wf(pv + 0xc, rf(0.0f, 3.0f));
        common_vary(m, 6.0f);
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) class804_update(a.i[0]); return uint64_t{0}; });
    // class 166 made up, its four linked mobys live ones of the level
    std::snprintf(name, sizeof name, "class166_update L%d", g_oracle_level);
    check(name, 0x002b3458, 80, Ret::None, [] {
        lb_load();
        seed_rand();
        const uint32_t m = made_up(0xa6, {479});
        if (m == 0) return Args{{0}, {}};
        const uint32_t pv = M3, n = wc_count(), first = wc_first();
        const int st = ri(0, 2);
        w8(m + 0x20, u(st == 2 ? ri(2, 5) : st));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t idx = u(ri(1, static_cast<int>(n)));
            w32(pv + 0x10 + k * 4, idx);
            w32(pv + 0x20 + k * 4, first + idx * 0x100);
            if (ri(0, 3) == 0) w8(first + idx * 0x100 + 0x20, 0x50);
        }
        w32(pv + 4, u(ri(0, 3)));
        w32(pv + 8, u(ri(0, 4)));
        w32(pv + 0xc, u(ri(0, 3)));
        common_vary(m, 6.0f);
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) class166_update(a.i[0]); return uint64_t{0}; });
}
}  // namespace

void batch_180() {
    if (g_only_batch != 180) return;
    g_batch = 180;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    const std::vector<std::string> drones = {"drone_L0_330", "drone_L0_450", "drone_L0_enemy"};
    const std::vector<std::string> morphs = {"morph_L0_330", "morph_L0_400", "morph_L0_520"};
    const Subject subjects[] = {
        {"vendor_orb_update", 1143, 0x002df730, vendor_orb_update, {"orb_L3_1350", "orb_L3_1450"}, 3, 3, vary_orb},
        {"drone_update", 479, 0x002ce358, drone_update, drones, 0, 7, vary_drone},
        {"chicken_update", 270, 0x002c97e0, chicken_update, morphs, 0, 7, vary_chicken},
    };
    for (const Subject& s : subjects) run_subject(s);
    for (const std::string& dn : morphs) run_hook_c(dn);
    run_428("morph_L0_400");
    run_428("morph_L0_520");
    run_804("morph_L0_330");
    run_804("morph_L0_520");
    run_166("drone_L0_enemy", 0);
    run_166("morph_L0_400", 0);
    run_hud("morph_hud_start", 0x00236b10, morph_hud_start, morphs);
    run_hud("morph_hud_update", 0x00236d00, morph_hud_update, morphs);
    run_hud("morph_hud_draw", 0x00236e50, morph_hud_draw, morphs);
    std::vector<std::string> all = drones;
    all.insert(all.end(), morphs.begin(), morphs.end());
    run_particle("particle_05_update", 5, 0x00269c70, particle_05_update, {"morph_L0_312", "morph_L0_318"});
    run_particle("particle_37_update", 0x37, 0x002726d8, particle_37_update, all);
    const bool trap_noop = g_debug_trap_is_noop;
    g_debug_trap_is_noop = true;  // (the engine's asserts made-up subjects may reach: a no-op in the interpreter)
    const bool oob_zero = g_ee_oob_is_zero;
    g_ee_oob_is_zero = true;  // (an unmapped pointer in a made-up moby's pvars: 0 on both sides)
    run_level_built();
    g_debug_trap_is_noop = trap_noop;
    g_ee_oob_is_zero = oob_zero;
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
