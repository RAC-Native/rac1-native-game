// Oracle batch 370 (fix/gamebreaking, issues #51 and #53): particle types 0x1f and 0x50, whose updates were never registered
// although ported level code spawns them. particles.cpp dispatches every particle's update through ovl_call_moby_fn: an unregistered
// one logged TODO_PORT and the particle neither moved nor expired, so it stayed in the level until it was reloaded:
//  - type 0x1f (Veldin 0x26eb88; L15:0x25e3c8, L18:0x270bf8, the same code: variants_l15 / variants_l18 rows, as the overlay
//    matcher placed none of Veldin's 0x26e200..0x26ed28 on those levels): spawned by L15:0x25e2c0 (Giant Clank's triangle attack, the
//    streaks of issue #51, from class 1523 L15:0x29edb0) and L18:0x270af0; it follows a moby (+0x30) and is freed when that moby
//    is gone (state 0xfe / 0xfd) or its timer +0xa runs out;
//  - type 0x50 (Veldin 0x275eb0, L17:0x26ff60): spawned by L17:0x26fdd0 (the laser beams of class 1368, issue #53).
// The port is Veldin's code transliterated (particle_types_tr.cpp, regen group veldin_particles), registered in veldin_extras.cpp;
// the original is each level's own copy (ORACLE_LEVEL=N). The records: made by the level's own spawner (its transliteration, on
// the level's own data), or alloc_particle(type) with their fields varied; then advanced by 0..40 frames of the port's update (the
// original's states later in the particle's life), the last frames (+0xa) too; the followed moby's state varied (alive / dying).
// Also, on RAM dumps (skipped without them), the live records as the game left them; from the repository's root, each with
// RAC1_HEADLESS=1 RAC1_RAMHASH=/dev/null RAC1_FRAMES=<f>+2 RAC1_RAMDUMP=<f>,assets/dumps/oracle/gamebreaking/<name>.bin:
//   gclank_streaks_L15.bin, f = 4530: RAC1_LEVEL=15 RAC1_HERO_AT='1446@740/1451@2300,0,0,0.5' and RAC1_SIM_INPUT=seq: the script of
//     batch 242's gclank dumps up to frame 3600 (circle 760, 800, .. 2200, cross 20 frames after each, triangle 2950 / 3000, cross
//     3300 / 3360), then 3600-4400 the stick forward with square every 25 frames, and triangle at 4500 (Giant Clank's attack);
//   jet_lasers_L17.bin, f = 980: RAC1_LEVEL=17 RAC1_HERO_AT='1379@800,2,0,0.5' (next to the Fighter Jet) and RAC1_SIM_INPUT=seq: circle
//     every 20 frames from 820, triangle every 20 frames from 830 (in the jet, the fleet's turrets firing).
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 370, N in 15 17 18.

#include "ovl_oracle.h"

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/quartu_mobys.h>
#include <game/ovl/shared_class1523.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_orbit_mobys.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& b370_dump(const std::string& path) {
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
void b370_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<uint32_t> live_of(uint8_t type) {
    std::vector<uint32_t> out;
    const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
    const int32_t hi = ee_read<int32_t>(OV(0x00160174));
    for (int32_t i = 0; i <= hi && i < 4096; ++i) {
        const uint32_t p = base + u(i) * 64;
        if (ee_read<uint8_t>(p) == type && ee_read<int8_t>(p + 1) >= 0) out.push_back(p);
    }
    return out;
}
}  // namespace

void batch_370() {
    if (g_only_batch != 370) return;
    g_batch = 370;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    const int L = g_oracle_level;

    // the level after its first gameplay frame (the port's), as batch 192's
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as batch 270 does on level 18)
            gameplay_frame();
            render_view_setup();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    const bool has = L == 15 || L == 17 || L == 18;
    if (has) level();
    std::vector<uint32_t> ms;
    if (has)
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe) ms.push_back(m);
    auto moby = [ms]() -> uint32_t { return ms.empty() ? M : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))]; };
    auto alive = [](uint32_t q, uint32_t type) { return q != 0 && static_cast<int8_t>(ee_read<uint8_t>(q + 1)) >= 0 && ee_read<uint8_t>(q) == type; };
    auto near = [](uint32_t p, float spread) {  // a point about the camera (w = 1)
        const Vec4 c = VV(OV(0x00166dc0));
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
    };
    // the record a spawner made: the one of the type alive after it and not before (0: none)
    auto spawned = [](uint32_t type, const std::function<void()>& fn) -> uint32_t {
        const std::vector<uint32_t> before = live_of(static_cast<uint8_t>(type));
        fn();
        for (uint32_t q : live_of(static_cast<uint8_t>(type)))
            if (std::find(before.begin(), before.end(), q) == before.end()) return q;
        return 0;
    };
    // the record: the level's spawner's or a new one of the type, its fields varied; then `adv` frames of the port's update while it
    // lives (a record freed on the way: a new one, not advanced)
    auto make = [&](uint32_t type, void (*upd)(uint32_t), const std::function<uint32_t()>& spawn,
                    const std::function<void(uint32_t)>& vary) -> uint32_t {
        uint32_t q = (spawn && ri(0, 2) != 0) ? spawn() : 0;
        if (q == 0) {
            q = alloc_particle(type);
            if (q == 0) return 0;
            near(q + 0x10, 20.0f);
            wfrand(q + 0x20, 3, -0.5f, 0.5f);
            w8(q + 2, R() & 0xff);
            w8(q + 8, R() & 0xff);
            wf(q + 0xc, rf(0.0f, 60000.0f));
            w16(q + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
            vary(q);
        }
        const uint32_t s = q;
        for (int k = ri(0, 3) == 0 ? 0 : ri(1, 40); k > 0 && alive(q, type); --k) upd(q);
        if (!alive(q, type)) {  // (freed while advanced: a fresh one)
            q = alloc_particle(type);
            if (q == 0) return s;
            near(q + 0x10, 20.0f);
            w16(q + 0xa, u(ri(-1, 60)));
            vary(q);
        }
        if (ri(0, 4) == 0) w16(q + 0xa, u(ri(-1, 2)));  // (its last frames)
        wf(0x0015ed60, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);  // (the frame scale)
        seed_rand();
        return q;
    };
    const uint32_t P = TEST_AREA + 0x6000;  // (the spawners' position argument)

    // ---- type 0x1f: following the moby +0x30 (its position +0x10 eased towards it by the $gp rates 0x1601f4 / 0x1601f8), freed
    // when the moby is dying (state 0xfe / 0xfd) or gone, or the timer +0xa runs out
    if (L == 15 || L == 18) {
        auto vary_moby = [moby](uint32_t q) {
            const uint32_t m = moby();
            w32(q + 0x30, ri(0, 8) == 0 ? 0u : m);
            if (m != 0 && ri(0, 5) == 0) w8(m + 0x20, coin() ? 0xfeu : 0xfdu);
        };
        std::function<uint32_t()> sp = [P, moby, near, spawned, L] {
            const uint32_t m = moby();
            near(P, 20.0f);
            const uint32_t color = R();
            return spawned(0x1f, [&] {
                if (L == 15) class1523_streak(color, P, m);  // (Quartu: the shared port it is re-glued onto)
                else class1523_streak(color, P, m);  // (Veldin's finale: the shared port it is re-glued onto)
            });
        };
        check("particle_1f_update", 0x0026eb88, 200, Ret::None, [&] {
            level();
            if (coin()) wf(OV(0x001601f4), rf(0.0f, 1.5f));
            if (coin()) wf(OV(0x001601f8), rf(0.0f, 1.5f));
            const uint32_t q = make(0x1f, particle_1f_update, sp, [vary_moby](uint32_t q) { wfrand(q + 0x20, 4, -30.0f, 30.0f); vary_moby(q); });
            if (q != 0 && ri(0, 3) == 0) vary_moby(q);
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_1f_update(a.i[0]); return uint64_t{0}; });
    } else {
        skip_check("particle_1f_update", 0x0026eb88, "spawned on levels 15 and 18: ORACLE_LEVEL=15 / 18");
    }

    // ---- type 0x50: the laser beams' (L17:0x26fdd0: kind 0 / 1 (+0x20), the colour fade over the timer +0xa)
    if (L == 17) {
        std::function<uint32_t()> sp = [P, near, spawned] {
            near(P, 20.0f);
            const uint32_t kind = coin() ? 1u : 0u, n = u(ri(-2, 16));
            return spawned(0x50, [&] { vo_26fdd0_l17(P, kind, n, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); });
        };
        check("particle_50_update", 0x00275eb0, 200, Ret::None, [&] {
            level();
            const uint32_t q = make(0x50, beam_particle_update, sp, [](uint32_t q) {
                w32(q + 0x20, coin() ? 1u : 0u);
                w32(q + 4, R());
            });
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) beam_particle_update(a.i[0]); return uint64_t{0}; });
    } else {
        skip_check("particle_50_update", 0x00275eb0, "spawned on level 17: ORACLE_LEVEL=17");
    }

    // ---- the live streaks of a Giant Clank fight (Quartu's dump)
    {
        const char* name = "particle_1f_update gclank";
        static const std::vector<uint8_t> none;
        const std::vector<uint8_t>& d = L == 15 ? b370_dump(dump_path("gamebreaking/gclank_streaks_L15.bin")) : none;
        if (L != 15) {
            skip_check(name, 0x0026eb88, "Quartu's dump: ORACLE_LEVEL=15");
        } else if (d.empty()) {
            skip_check(name, 0x0026eb88, "no RAM dump (assets/dumps/oracle/gamebreaking/gclank_streaks_L15.bin)");
        } else {
            b370_load(d);
            const size_t n = live_of(0x1f).size();
            std::printf("gclank_streaks_L15: %zu live type-0x1f particles\n", n);
            if (n == 0) {
                skip_check(name, 0x0026eb88, "no live type-0x1f particle in the dump");
            } else {
                check(name, 0x0026eb88, 100, Ret::None, [&d] {
                    b370_load(d);
                    const std::vector<uint32_t> ps = live_of(0x1f);
                    const uint32_t p = ps[static_cast<size_t>(ri(0, static_cast<int>(ps.size()) - 1))];
                    seed_rand();
                    { const int k = ri(0, 3); if (k == 0) w16(p + 0xa, 0); else if (k == 1) w16(p + 0xa, u(ri(1, 2))); }
                    if (ri(0, 4) == 0 && ee_read<uint32_t>(p + 0x30) != 0) w8(ee_read<uint32_t>(p + 0x30) + 0x20, coin() ? 0xfeu : 0xfdu);
                    return Args{{p}, {}};
                }, [](A a) { particle_1f_update(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    // ---- the live laser beams of the jet section (Drek's fleet's dump)
    {
        const char* name = "particle_50_update jet";
        static const std::vector<uint8_t> none17;
        const std::vector<uint8_t>& d = L == 17 ? b370_dump(dump_path("gamebreaking/jet_lasers_L17.bin")) : none17;
        if (L != 17) {
            skip_check(name, 0x00275eb0, "Drek's fleet's dump: ORACLE_LEVEL=17");
        } else if (d.empty()) {
            skip_check(name, 0x00275eb0, "no RAM dump (assets/dumps/oracle/gamebreaking/jet_lasers_L17.bin)");
        } else {
            b370_load(d);
            const size_t n = live_of(0x50).size();
            std::printf("jet_lasers_L17: %zu live type-0x50 particles\n", n);
            if (n == 0) {
                skip_check(name, 0x00275eb0, "no live type-0x50 particle in the dump");
            } else {
                check(name, 0x00275eb0, 100, Ret::None, [&d] {
                    b370_load(d);
                    const std::vector<uint32_t> ps = live_of(0x50);
                    const uint32_t p = ps[static_cast<size_t>(ri(0, static_cast<int>(ps.size()) - 1))];
                    seed_rand();
                    { const int k = ri(0, 3); if (k == 0) w16(p + 0xa, 0); else if (k == 1) w16(p + 0xa, u(ri(1, 2))); }
                    if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
                    return Args{{p}, {}};
                }, [](A a) { beam_particle_update(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
