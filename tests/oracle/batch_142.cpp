// Oracle batch 142 (task 0391, Umbris part 2): what a play-through of Umbris still reached unported, and the Snagglebeast's fight on its
// own state. Run with ORACLE_LEVEL=7 build/tests/ovl_oracle --batch 142 (Umbris's own original against the relocated port; needs the ISO);
// the shared particle types also on level 0.
//  - Umbris's own draw callbacks (umbris_part2_tr.cpp): 460, 823, 832, 902, 1018, 1046, 1135, the Snagglebeast 1106's three
//    (0x312420 / 0x312dc8 / 0x3131b0) and 0x317910 (queued by its helpers 0x318e30 / 0x3185xx), the Snagglebeast's HUD element 0x262078
//    (added by 1106 through 0x25d568), particle type 0x50 (Umbris's own, L7:0x29e4a0);
//  - particle types 0x33 / 0x3d / 0x41 / 0x3a (Veldin's 0x271ec8 / 0x273448 / 0x273ee0 / 0x272bc0: particle_types_tr.cpp);
//  - on RAM dumps of a headless run (tools/oracle/umbris_tour.sh dumps: assets/dumps/oracle/umbris_0391/umbris315.bin in the landing (class 882 spawned by 1041), umbris800.bin in the arrival's scene (class 631 spawned), umbris3000.bin in Qwark's scene,
//    umbris8140.bin as its first shock wave (1046) runs, umbris8655.bin as it spawns its class 1049 at a phase's end, umbris8700.bin in the Snagglebeast's fight while it takes the Blaster's hits, umbris9000.bin in its invulnerable phase, umbris8870.bin / 8970.bin its charge at the pillar 1113 the hero stands on (states 9, 5); skipped
//    without them): the Snagglebeast 1106 and the classes it spawns (the shock waves 1046), every class with a live moby, the draw
//    callbacks and the HUD element.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/umbris_part2.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_breakables.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class886.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_movers.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/variants_l7.h>

#include <optional>
#include <cstring>
#include <map>
#include <string>

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"class1049_rocks", "class1106_charge", "class882_arrival", "class882_landing"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is Umbris's own function at `l7addr`.
void u2check(const char* name, uint32_t l7addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 7) {
        std::printf("%-26s L7:0x%08x  skipped (Umbris only: run with ORACLE_LEVEL=7)\n", name, l7addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L7:0x%08x\n", name, l7addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l7addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// the point at `p` put about the camera, until the port's screen test (0x2002e0) sees it with radius r (as batch 140's); w = 1
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

void batch_142() {
    if (g_only_batch != 142) return;
    g_batch = 142;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // (the pose function 0x245f18's spills of the caller's $t8 / $t9 into the scratchpad: the transliterated callers have no such registers)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
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
    // a moby's pvars' floats / small counters varied (as batch 140's)
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

    // ---- Umbris's own draw callbacks: a moby of the class (any when none is live) moved about the camera, its pvars varied
    struct Cb {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int cls;
    };
    const Cb cbs[] = {{"umb_class460_draw", 0x002f8038, umb_class460_draw_l7, 460},       {"umb_class823_draw", 0x003048b0, class823_draw, 823},
                      {"umb_class832_draw", 0x00304c40, class832_draw, 832},       {"umb_class902_draw", 0x0030cd20, nov_water_751_draw_l1, 902},
                      {"umb_class1018_draw", 0x0030d420, umb_class1018_draw_l7, 1018},    {"umb_class1046_draw", 0x0030dcf0, umb_class1046_draw_l7, 1046},
                      {"umb_class1106_draw_a", 0x00312420, umb_class1106_draw_a_l7, 1106}, {"umb_class1106_draw_b", 0x00312dc8, umb_class1106_draw_b_l7, 1106},
                      {"umb_class1106_draw_c", 0x003131b0, umb_class1106_draw_c_l7, 1106}, {"umb_draw_317910", 0x00317910, umb_draw_317910_l7, 1106},
                      {"umb_class1135_draw", 0x0031ce40, class1135_draw, 1135}};
    for (const Cb& cb : cbs) {
        if (g_oracle_level != 7) {
            u2check(cb.name, cb.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        level();
        const std::vector<uint32_t> ms = live(cb.cls);
        if (ms.empty()) {  // (a moby of another class gives the callback garbage pvars: no game state; checked on the dumps instead)
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, cb.name) == 0)
                std::printf("%-26s L7:0x%08x  no moby of class %d on the level's state (see the dumps)\n", cb.name, cb.addr, cb.cls);
            continue;
        }
        u2check(cb.name, cb.addr, 100, Ret::None, [level, ms, pick, vary_pvars] {
            level();
            const uint32_t m = pick(ms);
            if (coin()) into_view(m + 0x10, coin() ? 0.0f : 8.0f, coin() ? 20.0f : 150.0f);
            vary_pvars(m);
            if (coin()) w8(m + 0x20, u(ri(0, 15)));
            if (coin()) w8(m + 0x30, u(ri(0, 0xff)));
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return Args{{m}, {}};
        }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- the particle types: a particle of the pool of the type, its frames (+0xa: the last ones too), its bytes / floats / colours
    // varied, the moby it may follow (+0x2c) a live one or none
    struct Pt {
        const char* name;
        uint32_t addr;
        uint32_t kind;
        void (*fn)(uint32_t);
        bool own;  // (Umbris's own: an L7 address)
    };
    const Pt pts[] = {{"particle_33_update", 0x00271ec8, 0x33, particle_33_update, false},
                      {"particle_3d_update", 0x00273448, 0x3d, particle_3d_update, false},
                      {"particle_41_update", 0x00273ee0, 0x41, particle_41_update, false},
                      {"particle_3a_update", 0x00272bc0, 0x3a, particle_3a_update, false},
                      {"umb_particle_50_update", 0x0029e4a0, 0x50, beam_particle_update, true}};
    for (const Pt& p : pts) {
        if (p.own && g_oracle_level != 7) {
            u2check(p.name, p.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        level();
        const std::vector<uint32_t> ms = live(-1);
        const uint32_t kind = p.kind;
        auto setup = [level, ms, pick, kind] {
            level();
            const uint32_t q = alloc_particle(kind);
            w16(q + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
            w8(q + 2, u(ri(0, 3)));
            w8(q + 4, u(ri(0, 0xff)));
            w8(q + 7, u(ri(0, 0xff)));
            w8(q + 8, u(ri(0, 0xff)));
            w32(q + 0xc, coin() ? R() : u(ri(0, 8)));
            wfrand(q + 0x10, 3, -300, 300);
            wfrand(q + 0x20, 3, -3, 3);
            w32(q + 0x2c, ri(0, 3) == 0 ? 0u : pick(ms));
            wfrand(q + 0x30, 2, -2, 2);
            w32(q + 0x34, R());
            w32(q + 0x38, R());
            w8(q + 0x3c, u(ri(0, 3)));
            if (kind == 0x3d) {  // (0x3d's +0x20 is the moby it follows, +0x2e its joint: 0x24f7c8)
                w32(q + 0x20, ee_read<uint32_t>(OV(0x0015ffd8)));  // (the hero, the first moby: the lava burn of the hero code spawns it)
                w16(q + 0x2e, u(ri(0, 12)));
            }
            wf(0x0015ed64, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
            wf(0x0015ed6c, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
            seed_rand();
            return q;
        };
        // a random state that sends the port through a pointer outside RAM (a joint the moby's model does not have) is not a game
        // state: dealt again (as batch 119's)
        auto gen = [setup, p] {
            static std::vector<uint8_t> saved;
            uint32_t q = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {
                q = setup();
                saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                try { p.fn(q); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;
                std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                if (ok) break;
            }
            return Args{{q}, {}};
        };
        auto port = [p](A a) { p.fn(a.i[0]); return uint64_t{0}; };
        if (p.own) u2check(p.name, p.addr, 200, Ret::None, gen, port);
        else check(p.name, p.addr, 200, Ret::None, gen, port);
    }

    // ---- on RAM dumps of the headless run (tools/oracle/umbris_tour.sh dumps)
    // the collision's contact vectors' w lanes and their copies in the floor lights' normals (as batches 138 / 140)
    for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
    // (and the vectors vf25..vf29 segment_collide (0x1efc70) stores at 0x173ee0.. scaled by I at its end: on the dumps the original stores
    // the VU0 registers' contents, which a RAM dump does not hold (the Snagglebeast's draw_c 0x3131b0 on the pillar dump: vf27 at
    // 0x173f10); the port its own values)
    for (uint32_t a = L7_ADDR(0x00173ee0u); a < L7_ADDR(0x00173f40u); a += 4) g_ignored_words.push_back(a);  // (level 7's own address: the dumps are its)
    for (uint32_t i = 0; i < 0x20; ++i) g_ignored_words.push_back(OV(0x0016e080) + i * 0x20 + 0x1c);
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (as batch 119's: vsync / kernel waits, the level change)
    for (const std::string& path_s : std::vector<std::string>{dump_path("umbris_0391/umbris315.bin"), dump_path("umbris_0391/umbris800.bin"), dump_path("umbris_0391/umbris3000.bin"), dump_path("umbris_0391/umbris8140.bin"),
                             dump_path("umbris_0391/umbris8655.bin"), dump_path("umbris_0391/umbris8700.bin"),
                             dump_path("umbris_0391/umbris9000.bin"), dump_path("umbris_0391/umbris8870.bin"), dump_path("umbris_0391/umbris8970.bin")}) {
        const char* path = path_s.c_str();
        if (g_oracle_level != 7) break;
        const std::vector<uint8_t>& d = dump(path);
        if (d.empty()) {
            std::printf("dump %s missing: skipped (tools/oracle/umbris_tour.sh dumps)\n", path);
            ++g_skipped;
            continue;
        }
        const std::string tag = std::strstr(path, "s315.") != nullptr ? "_landing" : std::strstr(path, "s800.") != nullptr ? "_arrival" : std::strstr(path, "8655") != nullptr ? "_rocks" : std::strstr(path, "3000") != nullptr ? "_scene" : std::strstr(path, "8140") != nullptr ? "_wave" : std::strstr(path, "8700") != nullptr ? "_fight" : std::strstr(path, "8870") != nullptr ? "_charge" : std::strstr(path, "8970") != nullptr ? "_pillar" : "_phase";
        std::printf("  (on %s)\n", path);
        // every class with a level-7 update function that has a live moby in the dump (the Snagglebeast, the shock waves 1046, ...)
        static const struct { int cls; uint32_t fn; void (*port)(uint32_t); } classes[] = {
            {1106, 0x00314150, umb_class1106_update_l7}, {1046, 0x0030e1f8, umb_class1046_update_l7}, {1049, 0x0030e458, umb_class1049_update_l7},
            {880, 0x0030b6a8, umb_class880_update_l7},   {882, 0x0030bbc8, umb_class882_update_l7},   {1113, 0x00319f48, umb_class1113_update_l7},
            {1128, 0x0031a9e8, umb_class1128_update_l7}, {1129, 0x0031aee0, mover_platform_update}, {447, 0x002f61c0, class447_update},
            {1133, 0x0031b3f0, umb_class1133_update_l7}, {460, 0x002f8058, sea_update},   {737, 0x00300970, scripted_camera_trigger_update_l1},
            {1069, 0x003112c8, umb_class1069_update_l7}, {1135, 0x0031c528, teleporter_update_l1}, {1474, 0x002cdb28, umb_class1474_update_l7},
            // (spawned: 696 when the Snagglebeast charges a pillar (frame ~8950 of the pillar line); 1049 near the Snagglebeast at its phase's
            // end. Not 631: the arrival's scene holds one (frames 740..2690, over the landing) whose pvars +0x238 is a float, not the uid its
            // state 0 retires: no run of the game reached that path, the port refuses the address the original would write through)
            {696, 0x00300338, rock_update}};
        for (const auto& c : classes) {
            load_dump(d);
            std::vector<uint32_t> ms;
            for (uint32_t m : live(-1))
                if (ee_read<uint32_t>(m + 0x74) == c.fn) ms.push_back(m);
            const std::string nm = "class" + std::to_string(c.cls) + tag;
            if (ms.empty()) {
                if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || nm == only)
                    std::printf("%-26s L7:0x%08x  none live in the dump\n", nm.c_str(), c.fn);
                continue;
            }
            auto port = c.port;
            const int cls = c.cls;
            u2check(nm.c_str(), c.fn, cls == 1106 || cls == 1128 ? 200 : 60, Ret::None, [&d, ms, pick, vary_pvars, port, cls, live] {
                // a random state that sends the port through a pointer outside RAM is not a game state: dealt again (as batch 119's)
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 40; ++attempt) {
                    load_dump(d);
                    m = pick(ms);
                    if (ri(0, 2) == 0) vary_pvars(m);
                    if (ri(0, 3) == 0) w8(m + 0x20, u(ri(0, 17)));
                    if (cls == 1106) {  // the Snagglebeast: its phase (+0x1c5 0 / 1 / 2, the glow timer +0x1c8), its health, the hero on one of
                        // the four pillars 1113..1116 above it (state 11 then goes to 9, the pillar's charge) or not
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        if (coin()) w8(pv + 0x1c5, u(ri(0, 2)));
                        if (coin()) wf(pv + 0x1c8, coin() ? 0.0f : rf(0, 1));
                        if (coin()) wf(pv + 0x20, rf(0, 120));
                        std::vector<uint32_t> pillars;
                        for (int k = 1113; k <= 1116; ++k)
                            for (uint32_t q : live(k)) pillars.push_back(q);
                        if (!pillars.empty() && coin()) {
                            ee_write<uint32_t>(0x0013f64c, pick(pillars));
                            wf(0x0013f3d8, ee_read<float>(m + 0x18) + rf(0.5f, 6));
                        }
                        if (coin()) w8(m + 0x20, coin() ? 11u : u(ri(5, 17)));
                    }
                    if (cls == 1128) {  // its saved flag 0x14c050[level * 16 + +0xb0] and the group flag 0x15fc88[+0xb0], set (0xff) or not
                        if (coin()) w8(m + 0xb0, coin() ? 0xffu : u(ri(0, 15)));
                        const uint32_t b = ee_read<uint8_t>(m + 0xb0);
                        if (b != 0xff) {
                            w8(0x0014c050 + ee_read<uint32_t>(0x0015ed84) * 16 + b, coin() ? 0xffu : u(ri(0, 3)));
                            w8(OV(0x0015fc88) + b, coin() ? 0xffu : u(ri(0, 3)));
                        }
                        if (coin()) w32(ee_read<uint32_t>(m + 0x78) + 0x3c, coin() ? 0u : R());
                    }
                    w32(OV(0x0015f5cc), R());
                    seed_rand();
                    saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                    bool ok = true;
                    const uint32_t stack_top = ee_stack_ptr();
                    try { port(m); } catch (const std::exception&) { ok = false; }
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                    if (ok) break;
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        }
        // the draw callbacks on the dump's own camera and mobys (not 460's / 1018's: they read the VU0 registers the frame's drawing left,
        // which a RAM dump does not hold; they are checked on the level's own state above)
        for (const Cb& cb : cbs) {
            if (cb.addr == 0x002f8038 || cb.addr == 0x0030d420) continue;
            load_dump(d);
            const std::vector<uint32_t> ms = live(cb.cls);
            if (ms.empty()) continue;
            const std::string nm = std::string(cb.name) + tag;
            u2check(nm.c_str(), cb.addr, 60, Ret::None, [&d, ms, pick, vary_pvars] {
                load_dump(d);
                const uint32_t m = pick(ms);
                if (ri(0, 2) == 0) vary_pvars(m);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
        }
        // the HUD elements (0x17dc50 + 0x90 i) whose draw function (+0x18) is the Snagglebeast's 0x262078
        {
            load_dump(d);
            std::vector<uint32_t> es;
            for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
                if (ee_read<uint32_t>(e + 0x18) == 0x00262078u) es.push_back(e);
            const std::string nm = "umb_hud_262078" + tag;
            if (!es.empty())
                u2check(nm.c_str(), 0x00262078, 60, Ret::None, [&d, es, pick] {
                    load_dump(d);
                    const uint32_t e = pick(es);
                    if (coin()) w32(e + 0x24, u(ri(0, 0x40)));
                    const uint32_t v = ee_read<uint32_t>(e + 0xc);  // (the slot's argument: the value the bar shows, the Snagglebeast's pvars +0x1b4)
                    if (v != 0 && coin()) w32(v, u(ri(0, 0x780)) | u(ri(0, 15)));
                    w32(OV(0x0015f5cc), R());
                    return Args{{e}, {}};
                }, [](A a) { boss_health_bar_draw(a.i[0]); return uint64_t{0}; });
        }
    }

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
