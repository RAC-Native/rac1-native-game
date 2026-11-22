// Oracle batch 160 (Batalia part 2): what a play-through of Batalia (level 8) still reached unported. Run with ORACLE_LEVEL=8
// build/tests/ovl_oracle --batch 160 (Batalia's own original against the relocated port; needs the ISO); the shared particle types
// also with ORACLE_LEVEL=0 (Veldin's own original).
//  - Batalia's draw-list callbacks (hand-written; 327, 600 and 1649 the shared ports since Y3, run through the registration): class 252's two (L8:0x2d3878 on list d880, 0x2d4108 on da80), 327
//    (0x2da0d0), 440 (0x2deee0), 452 (0x2e5188), 600 (0x2f0dc8), 671 (0x2f5e58), 832 (0x2fb948), 1135 (0x304bc0), 1649 (0x309030), the
//    Sandmouse 1818's glow (0x30ab50);
//  - particle types 0 / 1 (Veldin's 0x268d48 / 0x268fe8, L8:0x272eb8 / 0x273158: the rain, emitted by class 1629), 0x4d (0x275860),
//    0x2b (0x2710a0) and 0x4a (0x275510, the Sandmouse's) (particle_types2_tr.cpp);
//  - HUD slot 7's start / update (Veldin's 0x235dc0 / 0x23a218, hud_slot7_tr.cpp) and Batalia's own draw for it (L8:0x244ab0): the
//    element the hero code requests once the Metal Detector is owned;
//  - on RAM dumps of headless Batalia runs (the port's, `RAC1_RAMDUMP`, written by `tools/oracle/batalia_tour.sh dumps`,
//    in assets/dumps/oracle/batalia_part2/*.bin, skipped without them): every Batalia draw callback (on the mobys queued, else the class's live
//    ones), the live particles of the five types, HUD slot 7, and the class updates the level-state cases of batch 120 do not reach
//    (1023 the paratroopers, 1629 the rain emitter) or the story's (440 the turret, 1283, 1130, 1144, 253, 455, 1818).
// Batalia's Trespasser socket: batch 146 with ORACLE_LEVEL=8 (its pad_L8 / puzzle_L8 dumps in assets/dumps/oracle/trespasser_0411).

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/batalia_part2.h>
#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types2_tr.h>
#include <game/ovl/shared_class1023.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1629.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/variants_l8.h>

#include <runtime/vu/vu0.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"class1023_c1818", "class1023_para2760", "class1023_para2800",
                                         "class1023_para2900", "class1023_para2960", "class1023_para3035",
                                         "class1023_para3050", "class1023_c452", "class1023_c671", "class1130_arrival1500",
                                         "class1144_c1135", "class1818_c1135", "class1818_c1629n", "class1818_c1818",
                                         "class1818_c252", "class1818_c327", "class1818_c452", "class1818_para2760",
                                         "class1818_para2900", "class1818_para3035", "class1818_rain2800",
                                         "class1818_slot7_5003", "class1818_slot7_5040", "class1818_vb832",
                                         "class253_arrival1500", "class253_c1135", "class253_c1629n", "class253_c1649",
                                         "class253_c1818", "class253_c252", "class253_c327", "class253_c440",
                                         "class253_c452", "class253_c600", "class253_c671", "class253_para2760",
                                         "class253_para2800", "class253_para2900", "class253_para2960",
                                         "class253_para3035", "class253_para3050", "class253_rain2800",
                                         "class253_slot7_5003", "class253_slot7_5040", "class253_slot7_5150",
                                         "class253_vb832"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is Batalia's own function at `l8addr`.
void b8check(const char* name, uint32_t l8addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 8) {
        std::printf("%-26s L8:0x%08x  skipped (Batalia only: run with ORACLE_LEVEL=8)\n", name, l8addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L8:0x%08x\n", name, l8addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l8addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// the point at `p` put about the camera, until the port's screen test (0x2002e0) sees it with radius r (as batch 142's); w = 1
void into_view(uint32_t p, float r, float spread) {
    const Vec4 c = VV(OV(0x00166dc0));
    for (int k = 0; k < 200; ++k) {
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
        if (point_on_screen(p, r) != -1) return;
    }
}
const std::vector<uint8_t>& dump(const std::string& path) {
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
void load_dump(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
std::vector<std::string> dump_files() {
    const std::string d = dump_path("batalia_part2");
    std::vector<std::string> v;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(d, ec)) {
        const std::string n = e.path().filename().string();
        if (n.size() > 4 && n.substr(n.size() - 4) == ".bin") v.push_back(d + "/" + n);
    }
    std::sort(v.begin(), v.end());
    return v;
}
}  // namespace

void batch_160() {
    if (g_only_batch != 160) return;
    g_batch = 160;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);  // (as batch 142's: the pose function's spills)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
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
    // a random state that sends the port through a pointer outside RAM is not a game state: dealt again (as batch 142's)
    auto retry = [](const std::function<uint32_t()>& setup, void (*fn)(uint32_t)) {
        static std::vector<uint8_t> saved;
        uint32_t q = 0;
        for (int attempt = 0; attempt < 40; ++attempt) {
            q = setup();
            saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
            bool ok = true;
            const uint32_t stack_top = ee_stack_ptr();
            try { fn(q); } catch (const std::exception&) { ok = false; }
            ee_stack_ptr() = stack_top;
            std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
            if (ok) break;
        }
        return q;
    };

    // ---- Batalia's own draw callbacks: a moby of the class moved about the camera, its pvars, state and alpha varied
    struct Cb {
        const char* name;
        uint32_t addr;
        void (*fn)(uint32_t);
        int cls;
    };
    const Cb cbs[] = {{"bat_class252_draw_a", 0x002d3878, class252_draw_beam, 252}, {"bat_class252_draw_b", 0x002d4108, class252_draw_glow, 252},
                      {"bat_class327_draw", 0x002da0d0, [](uint32_t m) { ovl_call_moby_fn(0x002da0d0, m); }, 327},     {"bat_class440_draw", 0x002deee0, bat_class440_draw_l8, 440},
                      {"bat_class452_draw", 0x002e5188, bat_class452_draw_l8, 452},     {"bat_class600_draw", 0x002f0dc8, [](uint32_t m) { ovl_call_moby_fn(0x002f0dc8, m); }, 600},
                      {"bat_class671_draw", 0x002f5e58, bat_class671_draw_l8, 671},     {"bat_class832_draw", 0x002fb948, class832_draw, 832},
                      {"bat_class1135_draw", 0x00304bc0, class1135_draw, 1135},  {"bat_class1649_draw", 0x00309030, [](uint32_t m) { ovl_call_moby_fn(0x00309030, m); }, 1649},
                      {"bat_sandmouse_glow", 0x0030ab50, sandmouse_glow, 1818}};
    for (const Cb& cb : cbs) {
        if (g_oracle_level != 8) {
            b8check(cb.name, cb.addr, 0, Ret::None, nullptr, nullptr);
            continue;
        }
        level();
        const std::vector<uint32_t> ms = live(cb.cls);
        if (ms.empty()) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, cb.name) == 0)
                std::printf("%-26s L8:0x%08x  no moby of class %d on the level's state (see the dumps)\n", cb.name, cb.addr, cb.cls);
            continue;
        }
        b8check(cb.name, cb.addr, 100, Ret::None, [level, ms, pick, vary_pvars, retry, cb] {
            const uint32_t m = retry([level, ms, pick, vary_pvars] {
                level();
                // the frame's packet cursor (0x1611c0): 0 in the level's state after an update alone (the drawing then writes over the
                // null page and reads it back); the render frame's own buffer, as on the dumps (assets/dumps/oracle/batalia_part2/c440.bin: 0x35a710)
                w32(L8_ADDR(0x001611c0u), 0x0035a710u);
                const uint32_t m = pick(ms);
                if (coin()) into_view(m + 0x10, coin() ? 0.0f : 8.0f, coin() ? 20.0f : 150.0f);
                vary_pvars(m);
                if (coin()) w8(m + 0x20, u(ri(0, 15)));
                if (coin()) w8(m + 0x30, u(ri(0, 0xff)));
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return m;
            }, cb.fn);
            return Args{{m}, {}};
        }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- task Y3: the scrolling layers' draws of classes 600 / 1649 (the shared class600_draw / class1649_draw, shared_scroll.cpp)
    // through the level's registration, on their packets and on the VU0 lanes the original's own code writes last. The screen and
    // projection set up, the camera at the moby (or anywhere), the packet cursor on the test area's buffer (0x800000 .. 0x8f0000),
    // the layers' alpha words, the fade byte and the scroll offsets dealt in part (batch 520's), the VU0 the interpreter starts
    // from given to the port. Compared: vf1.xyz, vf2, vf3 and vf11 (written last by the last mesh's offsets, 0x258090 /
    // uv_offset_pairs: vf1.xyz cleared, vf11 the offset, vf2 / vf3 the last pairs). Not compared: vf1.w, whose last writer is the
    // set-up's vec3_scale (Veldin's 0x1ff300, VU0 code: vf1 = the camera 0x166dc0 scaled, its w the camera's w), and vf4 .. vf9,
    // rewritten by its mat4_mul (0x1ffdd8): host ports that keep no VU0 state, the generated copies glued to them as well. The
    // camera keeps the level's own w: with another w the original's odd vertices' t (vf11.w = vf1.w + v) differ by that w from the
    // ports' (the generated code's as well as the shared port's; NOTES of task Y3), which this check does not test.
    if (g_oracle_level == 8) {
        const std::mt19937 rng_before = rng;  // (the batch's later checks dealt the cases they had before these were added)
        static EeInterpreter* s_ee = nullptr;
        static VUState s_vu0_in;
        g_pre_call = [](EeInterpreter& ee) { s_ee = &ee; s_vu0_in = ee.vu0(); };
        auto vu0_given = [] {
            VUState& v = vu0::global().st;
            v.vf = s_vu0_in.vf; v.vi = s_vu0_in.vi; v.acc = s_vu0_in.acc; v.q = s_vu0_in.q; v.i = s_vu0_in.i; v.r = s_vu0_in.r;
            v.p = s_vu0_in.p; v.macflag = s_vu0_in.macflag; v.statusflag = s_vu0_in.statusflag; v.clipflag = s_vu0_in.clipflag;
            std::memcpy(v.mem.data(), s_vu0_in.mem.data(), 0x1000);
        };
        auto vu0_compare = [] {
            if (s_ee == nullptr) return;
            const VUState& o = s_ee->vu0();
            const VUState& p = vu0::global().st;
            const int regs[][2] = {{1, 7}, {2, 15}, {3, 15}, {11, 15}};
            int bad = 0;
            for (const auto& r : regs)
                for (int i = 0; i < 4; ++i)
                    if ((r[1] >> i & 1) != 0 && o.vf[static_cast<size_t>(r[0])].u[i] != p.vf[static_cast<size_t>(r[0])].u[i] && bad++ < 4)
                        std::printf("  VU0 vf%d lane %d: orig=%08x port=%08x\n", r[0], i, o.vf[static_cast<size_t>(r[0])].u[i], p.vf[static_cast<size_t>(r[0])].u[i]);
            if (bad != 0) w32(0x008ff000u, 0x0bad0000u + u(bad));
        };
        struct Lw {
            const char* name;
            uint32_t addr;
            int cls;
            uint32_t words, fade, offsets[3];
        };
        static const Lw lws[] = {{"bat_class600_draw_vu", 0x002f0dc8, 600, 0x00161dd0, 0x00161e08, {0x001e8348, 0x00161e10, 0x00161e20}},
                                 {"bat_class1649_draw_vu", 0x00309030, 1649, 0x00162390, 0x001623f0, {0x001f8548, 0x001623f8, 0x00162408}}};
        for (const Lw& x : lws) {
            level();
            const std::vector<uint32_t> ms = live(x.cls);
            if (ms.empty()) {
                std::printf("%-26s no moby of class %d\n", x.name, x.cls);
                continue;
            }
            {
                const Vec4 q = ee_ref<Vec4>(ms[0] + 0x10);
                std::printf("INFO Y3 class %d moby %u uid %d at (%.1f, %.1f, %.1f) state %u; the camera's w %g\n", x.cls,
                            (ms[0] - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8, ee_read<int16_t>(ms[0] + 0xb2), q.x, q.y, q.z,
                            ee_read<uint8_t>(ms[0] + 0x20), ee_ref<Vec4>(OV(0x00166dc0)).w);
            }
            const Lw d = x;
            b8check(d.name, d.addr, 80, Ret::None, [level, ms, pick, d] {
                level();
                if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
                screen_setup();
                projection_setup();
                const uint32_t m = pick(ms);
                if (coin()) {
                    const Vec4 t = ee_ref<Vec4>(m + 0x10);
                    const float a = rf(-3.1f, 3.1f), r = rf(5.0f, 60.0f);
                    Vec4& cam = ee_ref<Vec4>(OV(0x00166dc0));
                    cam = Vec4{t.x - r * std::cos(a), t.y - r * std::sin(a), t.z + rf(0.5f, 10.0f), cam.w};  // (w: the level's)
                }
                render_view_setup();
                w32(OV(0x001611c0), 0x00800000u + u(ri(0, 8)) * 0x10u);
                w32(OV(0x0015f5f8), 0x008f0000u);
                if (coin()) for (uint32_t k = 0; k < 0x34; k += 4) w32(d.words + k, coin() ? u(ri(0, 0x80)) : R());  // (the alphas)
                w8(d.fade, coin() ? 0x80u : (R() & 0xffu));
                for (const uint32_t o : d.offsets) if (coin()) wfrand(o, 6, -1.0f, 1.0f);
                w32(OV(0x0015f5cc), R());
                return Args{{m}, {}};
            }, [vu0_given, vu0_compare, d](A a) {
                vu0_given();
                ovl_call_moby_fn(d.addr, a.i[0]);
                vu0_compare();
                return uint64_t{0};
            });
        }
        g_pre_call = nullptr;
        rng = rng_before;
    }

    // ---- the particle types: a particle of the pool of the type, its frames, bytes, floats, colours varied
    struct Pt {
        const char* name;
        uint32_t addr;
        uint32_t kind;
        void (*fn)(uint32_t);
    };
    const Pt pts[] = {{"particle_00_update", 0x00268d48, 0x00, particle_00_update},
                      {"particle_01_update", 0x00268fe8, 0x01, particle_01_update},
                      {"particle_4d_update", 0x00275860, 0x4d, particle_4d_update},
                      {"particle_2b_update", 0x002710a0, 0x2b, particle_2b_update},
                      {"particle_4a_update", 0x00275510, 0x4a, particle_4a_update}};
    for (const Pt& p : pts) {
        level();
        const std::vector<uint32_t> ms = live(-1);
        const uint32_t kind = p.kind;
        auto setup = [level, ms, pick, kind] {
            level();
            const uint32_t q = alloc_particle(kind);
            w16(q + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
            w8(q + 2, u(ri(0, 3)));
            w32(q + 4, R());
            w8(q + 8, u(ri(0, 0xff)));
            w32(q + 0xc, coin() ? R() : u(ri(0, 8)));
            const Vec4 c = VV(OV(0x00166dc0));  // (about the camera, as the rain's spawner puts them, or anywhere in the level's box)
            if (coin()) VV(q + 0x10) = Vec4{c.x + rf(-30, 30), c.y + rf(-30, 30), c.z + rf(-10, 30), 1.0f};
            else wfrand(q + 0x10, 3, -20, 540);
            wfrand(q + 0x20, 4, -3, 3);
            if (coin()) VV(q + 0x20) = VV(q + 0x10);
            wfrand(q + 0x30, 4, -2, 2);
            if (kind == 0x01) w32(q + 0x20, u(coin() ? ri(1, 60) : ri(1, 0x7fff)));  // (type 1's frame total, its divisor)
            if (kind == 0x00 && coin()) wf(q + 0x3c, rf(-1, 3));
            if (kind == 0x4a) {  // (the Sandmouse's: +0x2c the moby it follows (a live one or none), +0x3c its descriptor (+8 / +0xa halfwords, +0xc
                // the frame total, a divisor): a second record of the pool holds one)
                const uint32_t dsc = alloc_particle(0x4a);
                w8(dsc + 1, 0xffu);  // (freed: the walk skips it)
                w16(dsc + 8, u(ri(-200, 200)));
                w16(dsc + 0xa, u(ri(-200, 200)));
                w32(dsc + 0xc, u(ri(1, 120)));
                w32(q + 0x3c, dsc);
                w32(q + 0x2c, ri(0, 3) == 0 ? 0u : pick(ms));
            }
            wf(0x0015ed64, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
            wf(0x0015ed6c, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);
            seed_rand();
            return q;
        };
        auto gen = [setup, retry, p] { return Args{{retry(setup, p.fn)}, {}}; };
        check(p.name, p.addr, 200, Ret::None, gen, [p](A a) { p.fn(a.i[0]); return uint64_t{0}; });
    }

    // ---- on RAM dumps of headless Batalia runs: the draw lists' callbacks with their queued argument, the live particles of the types
    for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);  // (as batch 142's)
    for (uint32_t a = L8_ADDR(0x00173ee0u); a < L8_ADDR(0x00173f40u); a += 4) g_ignored_words.push_back(a);
    for (uint32_t i = 0; i < 0x20; ++i) g_ignored_words.push_back(OV(0x0016e080) + i * 0x20 + 0x1c);
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80};
    const std::vector<std::string> dumps = dump_files();
    if (g_oracle_level == 8 && dumps.empty())
        skip_check("batalia_dumps", 0x00000000, ("no RAM dumps in " + dump_path("batalia_part2") + ": tools/oracle/batalia_tour.sh dumps").c_str());
    for (const std::string& path : dumps) {
        if (g_oracle_level != 8) break;
        const std::vector<uint8_t>& d = dump(path);
        if (d.empty()) continue;
        const std::string base = path.substr(path.find_last_of('/') + 1);
        const std::string tag = "_" + base.substr(0, base.size() - 4);
        std::printf("  (on %s)\n", path.c_str());
        // the draw lists d880 / da80 (fn array, arg array 0x100 after it: 64 entries each, counts 0x15f424 / 0x15f428): each Batalia callback queued, on its own argument
        load_dump(d);
        struct Q {
            uint32_t fn, arg;
        };
        std::vector<Q> queued;
        const uint32_t lists[2][2] = {{OV(0x0016d880), OV(0x0015f424)}, {OV(0x0016da80), OV(0x0015f428)}};
        for (const auto& l : lists) {
            const int n = static_cast<int>(ee_read<uint32_t>(l[1]));
            for (int i = 0; i < n && i < 64; ++i) queued.push_back({ee_read<uint32_t>(l[0] + 4 * i), ee_read<uint32_t>(l[0] + 0x100 + 4 * i)});
        }
        for (const Cb& cb : cbs) {
            std::vector<uint32_t> args;
            for (const Q& q : queued)
                if (q.fn == ORACLE_LADDR(8, cb.addr)) args.push_back(q.arg);
            if (args.empty()) args = live(cb.cls);  // (the lists are emptied by the frame's drawing: the class's live mobys, the argument they queue)
            if (args.empty()) continue;
            const std::string nm = std::string(cb.name) + tag;
            b8check(nm.c_str(), cb.addr, 60, Ret::None, [&d, args, pick, vary_pvars] {
                load_dump(d);
                const uint32_t m = pick(args);
                if (ri(0, 2) == 0 && m >= ee_read<uint32_t>(OV(0x0015ffd8)) && m < ee_read<uint32_t>(OV(0x0015ffe0))) vary_pvars(m);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [cb](A a) { cb.fn(a.i[0]); return uint64_t{0}; });
        }
        // the classes the level-state random cases of batch 120 do not exercise (1023 the paratroopers' early exit; 1629, the rain
        // emitter, works only with the camera L8:0x1675c0 within its radius pvars +4: the moby put about the camera) and the story's,
        // on their live mobys of the dump, state / pvars varied
        {
            static const struct { int cls; uint32_t fn; void (*port)(uint32_t); } classes[] = {
                {1023, 0x00301158, class1023_update}, {1629, 0x003085f0, class1629_update}, {440, 0x002e0328, bat_class440_update_l8},
                {1283, 0x003065d8, bat_class1283_update_l8}, {1130, 0x00302ce8, bat_class1130_update_l8}, {1144, 0x00305270, bat_class1144_update_l8},
                {253, 0x002d42d8, bat_class253_update_l8},   {455, 0x002e6530, class455_update},   {1818, 0x0030ac28, sandmouse_update}};
            for (const auto& c : classes) {
                load_dump(d);
                std::vector<uint32_t> ms;
                for (uint32_t m : live(-1))
                    if (ee_read<uint32_t>(m + 0x74) == c.fn) ms.push_back(m);
                if (ms.empty()) continue;
                const std::string nm = "class" + std::to_string(c.cls) + tag;
                auto port = c.port;
                const int cls = c.cls;
                // (253 / 455 spawn mobys whose pvars vectors get the w lane of a stack vector the spawner never writes: debris_spawn
                // L8:0x26de68's sq of sp+0 (0x26e05c), L8:0x2d5d08's sp+0x40 (see batch 120): a stale stack word on both sides, not the
                // same memory; the w lanes of the pvars blocks (+0xc mod 0x10) are not compared for these two)
                const size_t n_ign = g_ignored_words.size();
                if (cls == 253 || cls == 455)
                    for (uint32_t k = 0; k < 0x400; ++k)
                        for (uint32_t o = 0xc; o < 0x80; o += 0x10) g_ignored_words.push_back(ee_read<uint32_t>(OV(0x0015ffe8)) + (k << 7) + o);
                b8check(nm.c_str(), c.fn, 60, Ret::None, [&d, ms, pick, vary_pvars, retry, port, cls] {
                    const uint32_t m = retry([&d, ms, pick, vary_pvars, cls] {
                        load_dump(d);
                        const uint32_t m = pick(ms);
                        if (ri(0, 2) == 0) vary_pvars(m);
                        if (cls != 1629 && ri(0, 3) == 0) w8(m + 0x20, u(ri(0, 15)));
                        if (cls == 1629 && ri(0, 4) != 0) {  // (inside its radius: about the camera)
                            const Vec4 cam = VV(L8_ADDR(0x001675c0u));
                            VV(m + 0x10) = Vec4{cam.x + rf(-10, 10), cam.y + rf(-10, 10), cam.z + rf(-5, 5), 1.0f};
                        }
                        w32(OV(0x0015f5cc), R());
                        seed_rand();
                        return m;
                    }, port);
                    return Args{{m}, {}};
                }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
                g_ignored_words.resize(n_ign);
            }
        }
        // HUD slot 7 (the element the hero code requests with the Metal Detector owned): its start / update (Veldin's code) and
        // draw (Batalia's own L8:0x244ab0) on the dump's element(s) whose functions they are, the element's fields varied
        {
            load_dump(d);
            std::vector<uint32_t> es;
            for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
                if (ee_read<uint32_t>(e + 0x30) == ovl_code_lookup(0x00235dc0)) es.push_back(e);
            struct H {
                const char* name;
                uint32_t addr;
                void (*fn)(uint32_t);
                bool own;
            };
            const H hs[] = {{"hud_slot7_start_tr", 0x00235dc0, hud_slot7_start_tr, false},
                            {"hud_slot7_update_tr", 0x0023a218, hud_slot7_update_tr, false},
                            {"bat_hud_slot7_draw", 0x00244ab0, hud_slot7_draw_tr, true}};
            if (!es.empty())
                for (const H& h : hs) {
                    const std::string nm = std::string(h.name) + tag;
                    auto gen = [&d, es, pick] {
                        load_dump(d);
                        const uint32_t e = pick(es);
                        if (coin()) w32(e + 0x7c, u(ri(-2, 300)));
                        if (coin()) w32(e + 0x40, u(ri(0, 8)));
                        if (coin()) w8(e + 0x70, u(ri(0, 0xff)));
                        if (coin()) w8(e + 0x71, u(ri(0, 0xff)));
                        if (coin()) w32(e + 0x74, u(ri(0, 0x200)));
                        if (coin()) w32(e + 0x78, u(ri(0, 0x200)));
                        if (coin()) w32(0x0013e504, u(ri(0, 99999)));
                        const uint32_t a = ee_read<uint32_t>(e + 0x2c);
                        if (a != 0 && coin()) w32(a, u(ri(0, 2)));
                        w32(OV(0x0015f5cc), R());
                        seed_rand();
                        return Args{{e}, {}};
                    };
                    auto port = [h](A a) { h.fn(a.i[0]); return uint64_t{0}; };
                    if (h.own) b8check(nm.c_str(), h.addr, 100, Ret::None, gen, port);
                    else check(nm.c_str(), h.addr, 100, Ret::None, gen, port);
                }
        }
        // the live particles of the types
        for (const Pt& p : pts) {
            load_dump(d);
            std::vector<uint32_t> qs;
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            const int hi = static_cast<int>(ee_read<uint32_t>(OV(0x00160174)));
            for (int i = 0; i <= hi && i < 4096; ++i) {
                const uint32_t q = pool + 0x40u * static_cast<uint32_t>(i);
                if (static_cast<int8_t>(ee_read<uint8_t>(q + 1)) >= 0 && ee_read<uint8_t>(q) == p.kind) qs.push_back(q);
            }
            if (qs.empty()) continue;
            const std::string nm = std::string(p.name) + tag;
            std::printf("  %s: %zu live\n", nm.c_str(), qs.size());
            check(nm.c_str(), p.addr, 100, Ret::None, [&d, qs, pick] {
                load_dump(d);
                const uint32_t q = pick(qs);
                if (ri(0, 3) == 0) w16(q + 0xa, u(ri(-1, 3)));
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{q}, {}};
            }, [p](A a) { p.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
