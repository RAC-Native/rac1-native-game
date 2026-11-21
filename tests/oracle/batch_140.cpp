// Oracle batch 140 (task 0387, Rilgar part 2): what a play-through of Rilgar still reached unported, and the race's code on the race's
// own state. Run with ORACLE_LEVEL=5 build/tests/ovl_oracle --batch 140 (Rilgar's own original against the relocated port; needs the
// ISO); the shared functions also on level 0.
//  - Rilgar's seven class draw callbacks (rilgar_part2_tr.cpp; 823, 1511 and 1818's the shared ports since the re-glue R5): 79 (two),
//    823, 838, 879, 1511, 1818;
//  - class 775's update 0x2d7f38, particle type 0x43's update 0x274518 (veldin_extras.cpp, particle_types_tr.cpp);
//  - the screens' drawing 0x2018c8 transliterated whole (screen_draw_tr.cpp) and screen_draw (screens.cpp: the hand-ported kinds and the
//    hand-over to the transliteration), every kind;
//  - on RAM dumps of the race (tools/oracle/rilgar_tour.sh dumps: assets/dumps/oracle/rilgar_0387/race1200.bin in the race, race22990.bin on its
//    result screen; skipped without them): the racers (717), the boards (439), the rings (1139), the Hoverboard Girl (918), the race HUD's
//    draw functions (the HUD elements 0x17dc50 + 0x90 i's +0x18 that are Rilgar's own callbacks), the result screen's drawing.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/rilgar_part2.h>
#include <game/ovl/screen_draw_tr.h>
#include <game/ovl/shared_class1511.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_particles.h>
#include <game/ovl/shared_race_hud.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/veldin_extras.h>

#include <optional>
#include <cstring>
#include <map>
#include <string>

namespace rac1 {
void screen_draw();  // (screens.cpp, orig 0x2018c8)
}

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"spawned980", "race_racer717", "race_racer717_end"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is Rilgar's own function at `l5addr`.
void r2check(const char* name, uint32_t l5addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 5) {
        std::printf("%-26s L5:0x%08x  skipped (Rilgar only: run with ORACLE_LEVEL=5)\n", name, l5addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L5:0x%08x\n", name, l5addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l5addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// the point at `p` put about the camera, until the port's screen test (0x2002e0) sees it with radius r (as batch 136's); w = 1
void into_view(uint32_t p, float r, float spread) {
    const Vec4 c = VV(OV(0x00166dc0));
    for (int k = 0; k < 200; ++k) {
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
        if (point_on_screen(p, r) != -1) return;
    }
}
// a RAM dump of a headless run (32 MB + the scratchpad), empty when the file is missing
const std::vector<uint8_t>& dump(const char* path) {
    static std::map<std::string, std::vector<uint8_t>> cache;
    std::vector<uint8_t>& d = cache[path];
    if (d.empty()) {
        if (FILE* f = std::fopen(path, "rb")) {
            d.resize(0x02000000 + 0x4000);
            if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
            std::fclose(f);
        }
    }
    return d;
}
void load_dump(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
}  // namespace

void batch_140() {
    if (g_only_batch != 140) return;
    g_batch = 140;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the console clock / the disc reads the originals poll (IOP calls): the host's on both sides, as batch 138 does
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};

    // the level after its first gameplay frame (the port's)
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            render_view_setup();  // (the view matrices of the render frame: zero after an update alone)
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto live = [](int cls) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && (cls < 0 || static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == cls)) v.push_back(m);
        return v;
    };
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
    // a moby's pvars' floats / small counters varied (as batch 136's)
    auto vary_pvars = [](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        if (pv == 0 || !coin()) return;
        for (uint32_t o = 0; o < 0x80; o += 4) {
            const uint32_t w = ee_read<uint32_t>(pv + o);
            const uint32_t e = (w >> 23) & 0xff;
            if (e > 100 && e < 150 && coin()) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
            else if (w < 0x100 && coin()) w32(pv + o, u(ri(0, 0x40)));
        }
    };

    // ---- Rilgar's own draw callbacks: a moby of the class (any when none is live) moved about the camera, its pvars varied
    struct Cb {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int cls;
    };
    const Cb cbs[] = {{"ril_class79_draw_a", 0x002d6f48, ril_class79_draw_a_l5, 79},   {"ril_class79_draw_b", 0x002d7920, ril_class79_draw_b_l5, 79},
                      {"ril_class823_draw", 0x0030c220, class823_draw, 823},    {"ril_class838_draw", 0x0030de90, ril_class838_draw_l5, 838},
                      {"ril_class879_draw", 0x00316070, ril_class879_draw_l5, 879},    {"ril_class1511_draw", 0x0031cf58, class1511_draw, 1511},
                      {"ril_class1818_draw", 0x0031e730, sandmouse_glow, 1818}};
    for (const Cb& cb : cbs) {
        if (g_oracle_level != 5) {
            r2check(cb.name, cb.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        level();
        std::vector<uint32_t> ms = live(cb.cls);
        if (ms.empty()) ms = live(-1);
        r2check(cb.name, cb.addr, 100, Ret::None, [level, ms, pick, cb, vary_pvars] {
            level();
            const uint32_t m = pick(ms);
            if (coin()) into_view(m + 0x10, coin() ? 0.0f : 8.0f, coin() ? 20.0f : 150.0f);
            vary_pvars(m);
            if (coin()) w8(m + 0x20, u(ri(0, 4)));
            if (coin()) w8(m + 0x30, u(ri(0, 0xff)));
            if (cb.addr == 0x002d7920 && coin()) w32(0x001613f0, 0);  // (class 79's draw_b returns at once while its flag gp-0x5810 is set)
            if (cb.addr == 0x002d6f48 && ee_read<uint32_t>(m + 0x78) != 0 && coin()) {  // (P5.3: 79's lights: drawn, its three light points in view)
                w8(m + 0x31, 1);
                for (uint32_t o = 0x330; o < 0x360; o += 0x10) into_view(ee_read<uint32_t>(m + 0x78) + o, 0.0f, 30.0f);
            }
            if (cb.addr == 0x0031e730 && ee_read<uint32_t>(m + 0x78) != 0)  // (1818's glows: the points at its pvars +0x70, +0x80, +0x90...)
                for (uint32_t o = 0x70; o < 0xb0; o += 0x10)
                    if (coin()) into_view(ee_read<uint32_t>(m + 0x78) + o, 0.0f, 30.0f);
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{m}, {}};
        }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- class 775's update: a moby of the class as spawn_moby_307 (0x2d7e90) puts one out at a live moby, its byte +0x23 near the end
    // (0..10) or not, its scale +0x2c
    {
        level();
        const std::vector<uint32_t> ms = live(-1);
        check("class775_update", 0x002d7f38, 60, Ret::None, [level, ms, pick] {
            level();
            const uint32_t m = spawn_moby_307(pick(ms) + 0x10, rf(0.2f, 3.0f));
            if (m == 0) return Args{{pick(ms)}, {}};
            w8(m + 0x23, u(ri(0, 2) == 0 ? ri(0, 10) : ri(0, 0xff)));
            if (coin()) wf(m + 0x2c, rf(-4, 4));
            return Args{{m}, {}};
        }, [](A a) { class775_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- particle type 0x43's update: a particle of the pool, its frames (+0xa: the last ones too), its mode (+0x3c 0 / 1 / other),
    // the moby it follows (+0x2c: a live one or none), its floats and counters varied
    {
        level();
        const std::vector<uint32_t> ms = live(-1);
        check("particle_43_update", 0x00274518, 200, Ret::None, [level, ms, pick] {
            level();
            const uint32_t p = alloc_particle(0x43);
            w16(p + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
            w8(p + 7, u(ri(0, 0xff)));
            wfrand(p + 0x10, 3, -300, 300);
            wfrand(p + 0x20, 3, -3, 3);
            w32(p + 0x2c, ri(0, 3) == 0 ? 0u : pick(ms));
            wfrand(p + 0x30, 2, -2, 2);
            w16(p + 0x38, u(ri(0, 60)));
            w16(p + 0x3a, u(ri(0, 0x20)));
            w16(p + 0x3c, u(ri(0, 3)));
            wfrand(p + 0x40, 8, -3, 3);
            wf(0x0015ed64, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
            seed_rand();
            return Args{{p}, {}};
        }, [](A a) { particle_43_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- particle type 0x30's update (the explosions': the Extermitank's, ...): its frames (+0xa: the last ones too), the bytes +8 /
    // +0x3c, its floats +0x10..+0x30, the two colours +0x34 / +0x38 (rgba_lerp)
    check("particle_30_update", 0x00271c88, 200, Ret::None, [level] {
        level();
        const uint32_t p = alloc_particle(0x30);
        w16(p + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
        w8(p + 8, u(ri(0, 0xff)));
        wfrand(p + 0x10, 3, -300, 300);
        wfrand(p + 0x20, 3, -3, 3);
        wf(p + 0x30, rf(-0.05f, 0.05f));
        w32(p + 0x34, R());
        w32(p + 0x38, R());
        w8(p + 0x3c, u(ri(0, 0xff)));
        return Args{{p}, {}};
    }, [](A a) { particle_30_update(a.i[0]); return uint64_t{0}; });

    // ---- particle type 0x32's update (class 893's): its frames (+0xa: the last ones too), the byte +2 (compared with *(*(0x1b2148))),
    // the bytes +8 / +0x34, its floats +0x10..+0x3f
    check("particle_32_update", 0x00271df8, 200, Ret::None, [level] {
        level();
        const uint32_t p = alloc_particle(0x32);
        w16(p + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
        w8(p + 2, u(ri(0, 3)));
        w8(p + 8, u(ri(0, 0xff)));
        wfrand(p + 0x10, 3, -300, 300);
        wfrand(p + 0x20, 4, -3, 3);
        wfrand(p + 0x30, 4, -1, 1);
        w8(p + 0x34, u(ri(0, 0xff)));
        wf(0x0015ed6c, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
        return Args{{p}, {}};
    }, [](A a) { particle_32_update(a.i[0]); return uint64_t{0}; });

    // ---- the galactic map's widget function planet_map_enter (0x27a710, Rilgar L5:0x2a4a18: a variants_l5 row, the matcher had merged it
    // into its neighbour, so the ship's planet menu logged it TODO_PORT on Rilgar): any widget address
    check("planet_map_enter", 0x0027a710, 20, Ret::Int32, [level] {
        level();
        return Args{{R() & 0x01fffff0u}, {}};
    }, [](A a) { return r32(static_cast<uint32_t>(planet_map_enter(a.i[0]))); });

    // ---- the screens' drawing: every kind (0..8), its countdowns / step / counters, the lines' texts (a message or none), the mode it
    // was opened over; the whole transliteration and the hand port's dispatch
    {
        auto gen = [level] {
            level();
            const uint32_t sb = OV(0x00173040);
            w32(sb, std::getenv("B140_KIND") != nullptr ? u(std::atoi(std::getenv("B140_KIND"))) : u(ri(0, 8)));
            w32(sb + 4, u(ri(0, 40)));
            static const int32_t texts[] = {0x4f6e, 0x5248, 0x5249, 0x5229, 0x4ee0, 0x524a, 0x4e2b};
            for (uint32_t o = 8; o <= 0x10; o += 4) w32(sb + o, coin() ? static_cast<uint32_t>(sub_00203ad8(texts[ri(0, 6)])) : 0u);
            w32(sb + 0x14, u(ri(0, 3)));
            w32(sb + 0x18, u(ri(0, 5)));
            w32(sb + 0x1c, u(ri(0, 4)));
            w32(sb + 0x20, u(ri(0, 30)));
            w32(sb + 0x24, u(ri(0, 30)));
            w32(sb + 0x28, u(ri(0, 8)));
            w32(OV(0x0015f3f8), R() & 0xffff);
            w32(0x0015eeb0, u(ri(0, 0x18)));
            return Args{{0}, {}};
        };
        check("screen_draw_tr", 0x002018c8, 150, Ret::None, gen, [](A) { screen_draw_tr(0); return uint64_t{0}; });
        check("screen_draw", 0x002018c8, 150, Ret::None, gen, [](A) { screen_draw(); return uint64_t{0}; });
    }

    // ---- 980 / 981, the Pipe Mounted Grating's (846) pieces: not placed, spawned by 846's update when the grating is hit (L5:0x30f620 /
    // 0x30f730); grown here from the level by 846's port given a hit (its hit table slot naming it, the damage kinds random), then their
    // own update (L5:0x3186f8) from their state as spawned, their state byte 0..9, their pvars varied
    if (g_oracle_level != 5) {
        r2check("spawned980", 0x003186f8, 0, Ret::None, nullptr, nullptr);
    } else if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "spawned980") == 0) {
        static std::vector<uint8_t> grown;
        level();
        w32(OV(0x0015f5c4), 0);
        const std::vector<uint32_t> gratings = live(846);
        int tries = 0;
        for (; tries < 200 && live(980).empty() && live(981).empty() && !gratings.empty(); ++tries) {
            const uint32_t m = gratings[static_cast<size_t>(tries) % gratings.size()];
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, m);  // (the entry names the grating: sub_0025a420's owner test)
            w32(e + 0x24, R() | 0x00b30000u);  // (one of the kinds 846 tests, lui a1,0xb3)
            wf(e + 0x2c, rf(0, 12));
            ril_class846_update_l5(m);
        }
        std::vector<uint32_t> ms = live(980);
        for (uint32_t m : live(981)) ms.push_back(m);
        if (ms.empty()) {
            std::printf("%-26s L5:0x%08x  none spawned in %d hits of 846\n", "spawned980", 0x003186f8u, tries);
        } else {
            std::printf("  (spawned980: %zu spawned after %d hits of 846)\n", ms.size(), tries);
            grown.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
            r2check("spawned980", 0x003186f8, 120, Ret::None, [ms, pick, vary_pvars] {
                // a random state that sends the port through a pointer outside RAM is not a game state: dealt again (as batch 114's)
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 40; ++attempt) {
                    std::memcpy(ee_get_ram_base(), grown.data(), grown.size());
                    m = pick(ms);
                    if (ri(0, 3) != 0) w8(m + 0x20, u(ri(0, 9)));
                    vary_pvars(m);
                    seed_rand();
                    saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                    bool ok = true;
                    const uint32_t stack_top = ee_stack_ptr();
                    try { class344_update(m); } catch (const std::exception&) { ok = false; }
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                    if (ok) break;
                }
                return Args{{m}, {}};
            }, [](A a) { class344_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- the race, on RAM dumps of the headless race (tools/oracle/rilgar_tour.sh dumps)
    struct Dumped {
        const char* name;
        uint32_t fn;  // (the class update, L5)
        void (*port)(uint32_t);
    };
    const Dumped racers[] = {{"race_racer717", 0x00307910, ril_class717_update_l5}, {"race_board439", 0x002f87a8, class439_update},
                             {"race_ring1139", 0x0031abe0, class1139_update}, {"race_girl918", 0x00316ab8, ril_class918_update_l5}};
    const Dumped huds[] = {{"race_hud_262ae8", 0x00262ae8, race_hud_start}, {"race_hud_262b58", 0x00262b58, race_meter_update},
                           {"race_hud_262f50", 0x00262f50, ril_cb_262f50_l5}, {"race_hud_2661a0", 0x002661a0, race_hud_reset},
                           {"race_hud_2661e8", 0x002661e8, race_hud_update}, {"race_hud_266320", 0x00266320, ril_cb_266320_l5},
                           {"race_hud_266710", 0x00266710, race_hud_draw}, {"race_hud_29bda0", 0x0029bda0, bullet_particle_update},
                           {"race_hud_2a00f0", 0x002a00f0, beam_particle_update}};
    // the collision's contact vectors' w lanes (vf25..vf29's, caller leftovers the port stores as 0, -0.0 in the original after the racers'
    // floor probe 0x2839d0 -> segment_collide 0x21e8d8: segment_collide 0x1efc70's note) and their copies in the floor lights' normals (as
    // batch 138's)
    for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
    for (uint32_t i = 0; i < 0x20; ++i) g_ignored_words.push_back(OV(0x0016e080) + i * 0x20 + 0x1c);
    for (const std::string& path_s : std::vector<std::string>{dump_path("rilgar_0387/race1200.bin"), dump_path("rilgar_0387/race22990.bin")}) {
        const char* path = path_s.c_str();
        const bool end = std::strstr(path, "22990") != nullptr;
        if (g_oracle_level != 5) {
            for (const Dumped& d : racers) r2check(d.name, d.fn, 0, Ret::None, nullptr, nullptr);
            break;
        }
        const std::vector<uint8_t>& d = dump(path);
        if (d.empty()) {
            std::printf("race dump %s missing: skipped (tools/oracle/rilgar_tour.sh dumps)\n", path);
            ++g_skipped;
            continue;
        }
        std::printf("  (on %s)\n", path);
        for (const Dumped& c : racers) {
            load_dump(d);
            std::vector<uint32_t> ms;
            for (uint32_t m : live(-1))
                if (ee_read<uint32_t>(m + 0x74) == c.fn) ms.push_back(m);
            if (ms.empty()) {
                std::printf("%-26s L5:0x%08x  none live in the dump\n", c.name, c.fn);
                continue;
            }
            const std::string nm = std::string(c.name) + (end ? "_end" : "");
            r2check(nm.c_str(), c.fn, 60, Ret::None, [&d, ms, pick, vary_pvars] {
                load_dump(d);
                const uint32_t m = pick(ms);
                if (ri(0, 2) == 0) vary_pvars(m);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [c](A a) { c.port(a.i[0]); return uint64_t{0}; });
        }
        // the classes' draw callbacks on the race's own camera and mobys (the city around the race track in view): a moby of the class,
        // its pvars varied, class 79's flag cleared or not
        for (const Cb& cb : cbs) {
            load_dump(d);
            const std::vector<uint32_t> ms = live(cb.cls);
            if (ms.empty()) continue;
            const std::string nm = std::string(cb.name) + (end ? "_race_end" : "_race");
            r2check(nm.c_str(), cb.addr, 60, Ret::None, [&d, ms, pick, cb, vary_pvars] {
                load_dump(d);
                const uint32_t m = pick(ms);
                if (ri(0, 2) == 0) vary_pvars(m);
                if (cb.addr == 0x002d7920 && coin()) w32(0x001613f0, 0);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
        }
        // the HUD elements whose draw function (+0x18) is one of Rilgar's callbacks
        for (const Dumped& c : huds) {
            load_dump(d);
            std::vector<uint32_t> es;
            for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
                if (ee_read<uint32_t>(e + 0x18) == c.fn) es.push_back(e);
            if (es.empty()) continue;
            const std::string nm = std::string(c.name) + (end ? "_end" : "");
            r2check(nm.c_str(), c.fn, 40, Ret::None, [&d, es, pick] {
                load_dump(d);
                const uint32_t e = pick(es);
                if (coin()) w32(e + 0x24, u(ri(0, 0x40)));
                w32(OV(0x0015f5cc), R());
                return Args{{e}, {}};
            }, [c](A a) { c.port(a.i[0]); return uint64_t{0}; });
        }
        if (end) {
            const std::string nm = "race_result_screen";
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || nm == only) {
                load_dump(d);
                std::printf("  (the result screen: game mode %d, kind %d)\n", ee_read<int32_t>(OV(0x0015f5c4)), ee_read<int32_t>(OV(0x00173040)));
            }
            check(nm.c_str(), 0x002018c8, 20, Ret::None, [&d] {
                load_dump(d);
                w32(OV(0x00173040) + 4, u(ri(0, 30)));
                w32(OV(0x00173040) + 0x20, u(ri(0, 30)));
                w32(OV(0x00173040) + 0x24, u(ri(0, 30)));
                return Args{{0}, {}};
            }, [](A) { screen_draw(); return uint64_t{0}; });
        }
    }

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
