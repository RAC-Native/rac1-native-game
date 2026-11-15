// Oracle batch 138 (task 0385): Kerwan (level 3) part 2: the Spawner (455), the Gold Bolt (1134), class 994's draw callback and
// the level's own sound emitter types (sound_emitter_types.cpp), on the level's real data. Run with
// ORACLE_LEVEL=3 build/tests/ovl_oracle --batch 138 (needs the ISO). The subjects are the level's own mobys / emitters after the
// first gameplay frame, perturbed; the original is Kerwan's own function at its own address.

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/kerwan_part2_tr.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_class235.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/swingshot_rope_tr.h>
#include <game/ovl/variants_l3.h>
#include <runtime/vu/vu0.h>
#include <game/ovl/ovl_reloc.h>

#include <cmath>
#include <cstring>

namespace rac1::oracle {

namespace {
// A check whose original is Kerwan's own function at `l3addr`.
void k2check(const char* name, uint32_t l3addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 3) {
        std::printf("%-26s L3:0x%08x  skipped (Kerwan only: run with ORACLE_LEVEL=3)\n", name, l3addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L3:0x%08x\n", name, l3addr);
    if (!check_fn(name, l3addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}

// The VU0 the original started from (the interpreter's registers, flags and data memory), given to the port before it runs, as batch
// 128 does: the two keep separate VU0s, and after the port's own gameplay frames grew the subjects their stale lanes (the w words the
// collision's VU0 stores leave) differ
VUState g_k2_vu0;
void k2_vu0_capture(EeInterpreter& ee) {
    const VUState& o = ee.vu0();
    g_k2_vu0.vf = o.vf; g_k2_vu0.vi = o.vi; g_k2_vu0.acc = o.acc; g_k2_vu0.q = o.q; g_k2_vu0.i = o.i; g_k2_vu0.r = o.r; g_k2_vu0.p = o.p;
    g_k2_vu0.macflag = o.macflag; g_k2_vu0.statusflag = o.statusflag; g_k2_vu0.clipflag = o.clipflag;
    std::memcpy(g_k2_vu0.mem.data(), o.mem.data(), 0x1000);
}
void k2_vu0_sync() {
    VUState& v = vu0::global().st;
    v.vf = g_k2_vu0.vf; v.vi = g_k2_vu0.vi; v.acc = g_k2_vu0.acc; v.q = g_k2_vu0.q; v.i = g_k2_vu0.i; v.r = g_k2_vu0.r; v.p = g_k2_vu0.p;
    v.macflag = g_k2_vu0.macflag; v.statusflag = g_k2_vu0.statusflag; v.clipflag = g_k2_vu0.clipflag;
    std::memcpy(v.mem.data(), g_k2_vu0.mem.data(), 0x1000);
}
}  // namespace

void batch_138() {
    if (g_only_batch != 138) return;
    g_batch = 138;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the console clock and its conversion to local time (sceCdReadClock 0x121780, 0x12d6d8: IOP calls whose wait the interpreter's
    // original polls forever at 0x11ba40): the host's on both sides, as batch 75 does (level_change_request's autosave: the Gold Bolt)
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};

    // the level after its first gameplay frame (the port's)
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
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    auto mobys_of_class = [](int cls) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == cls) v.push_back(m);
        return v;
    };
    // the hero near the point or far from it
    auto hero_near = [](const Vec4& at, float near_r, float far_r) {
        Vec4 q = at;
        const float r = ri(0, 2) == 0 ? rf(0, near_r) : rf(near_r, far_r);
        const float a = rf(-3.1f, 3.1f);
        q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 3);
        ee_ref<Vec4>(0x0013f3d0) = q;
        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = q;  // (the hero moby's own position too)
    };
    // a hit on the moby: its slot in the hit table (0x178100, 64-byte entries) naming it, with a damage mask
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
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 3) { k2check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        level();
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L3:0x%08x  no moby on this level\n", name, fn); return; }
        k2check(name, fn, n, Ret::None, [level, ms, vary] {
            level();
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            vary(m);
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
    };
    // the debris' velocity / spin w lanes: a VU0 register leftover of the original's sin / cos
    if (g_oracle_level == 3) level();
    for (uint32_t a = g_oracle_level == 3 ? ee_read<uint32_t>(OV(0x0015ffe0)) : 0u; a < (g_oracle_level == 3 ? ee_read<uint32_t>(OV(0x0015ffe0)) : 0u) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }

    // ---- the Spawner (455), the Gold Bolt (1134): their state byte (0 / 1 / as placed / random small), a hit, the hero near or far
    struct Cls { uint32_t fn; const char* name; void (*port)(uint32_t); };
    static const Cls classes[] = {
        {0x002bef68u, "spawner455_update", class455_update},
        {0x002dd188u, "gold_bolt1134_update", gold_bolt_update},
    };
    for (const Cls& c : classes)
    for (int pass = 0; pass < 8; ++pass)
    classcheck(c.name, c.fn, c.port, [pass, give_hit, hero_near](uint32_t m) {
        if (pass == 1) w8(m + 0x20, 0);
        if (pass == 2) w8(m + 0x20, 1);
        if (pass >= 5) w8(m + 0x20, u(ri(0, 6)));
        give_hit(m, 0x10000 | (R() & 0x3ffffu));
        hero_near(ee_ref<Vec4>(m + 0x10), 3.0f, 30.0f);
        seed_rand();
    }, 25);
    // the Spawner's own states: the moby's state 0..9 (its jump table), the sub-state pvars +0x84 (0..6), its health +0x20, the
    // spawned count +0x86 / the most +0x87, its box +0xd0, the hero within / beyond its radii (+0x90 / +0x9c)
    for (int pass = 0; pass < 10; ++pass)
    classcheck("spawner455_states", 0x002bef68u, class455_update, [pass, give_hit, hero_near](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        w8(m + 0x20, u(pass));
        w8(pv + 0x84, u(ri(0, 6)));
        if (coin()) wf(pv + 0x20, ri(0, 2) == 0 ? rf(-2.0f, 0.5f) : rf(0.5f, 20.0f));
        if (coin()) w8(pv + 0x87, u(ri(0, 18)));
        // the spawn itself: the uids of its live mobys (+0xe8, 18 words: their count +0x86 is taken from them) cleared or kept, the
        // spawn timer +0x88 about to run out, the respawn word +0x8a, the trigger mode +0x82 (0 / 1 / 2)
        if (coin()) for (uint32_t i = 0; i < 18; ++i) if (ri(0, 3) != 0) w32(pv + 0xe8 + i * 4, 0xffffffffu);  // (a random moby index is no spawned moby: its grid cells do not hold it, the original traps)
        if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(m + 0x10);  // (the camera near: its floor band)
        if (coin()) w32(pv + 0x88, u(ri(0, 2)));
        if (coin()) w16(pv + 0x8a, u(ri(0, 2)));
        if (coin()) w8(pv + 0x82, u(ri(0, 2)));
        if (ri(0, 3) != 0) w8(pv + 0x85, u(ri(0, 6)));
        if (ri(0, 5) == 0) w8(pv + 0x82, R() & 0xff);
        if (ri(0, 3) == 0) w32(pv + 0xd0, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 20)));
        if (ri(0, 3) == 0) w8(m + 0x31, R() & 1);
        give_hit(m, 0x10000 | (R() & 0x3ffffu));
        hero_near(ee_ref<Vec4>(m + 0x10), ee_read<float>(pv + 0x90) > 0.0f && ee_read<float>(pv + 0x90) < 200.0f ? ee_read<float>(pv + 0x90) : 10.0f, 80.0f);
        seed_rand();
    }, 25);
    // the Gold Bolt's states 0 / 1 / 2, the hero right at it (the pickup: within 3 in xy, 2 in z), alive (+0x22a8) and in a state
    // that may collect (+0x20a4 0 / 3 / other), the bolt already collected or not (its byte of 0x14bec0), the bolt's kind (+0xc)
    for (int pass = 0; pass < 8; ++pass)
    classcheck("gold_bolt1134_states", 0x002dd188u, gold_bolt_update, [pass](uint32_t m) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
        w8(m + 0x20, u(pass % 3));
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        if (ri(0, 3) != 0) { q.x += rf(-2.0f, 2.0f); q.y += rf(-2.0f, 2.0f); q.z += rf(-2.5f, 2.5f); }
        else { q.x += rf(-10.0f, 10.0f); q.y += rf(-10.0f, 10.0f); }
        ee_ref<Vec4>(P + 0x80) = q;
        w32(P + 0x22a8, ri(0, 4) == 0 ? 0u : 1u);
        { static const uint8_t st[] = {0, 3, 3, 1, 7}; w8(P + 0x20a4, st[ri(0, 4)]); }
        if (coin()) w16(pv + 0xc, ri(0, 2) == 0 ? 0u : 1u);
        const int32_t idx = static_cast<int32_t>(ee_read<uint32_t>(pv));
        if (idx >= 0 && idx < 4) w8(0x0014bec0 + ee_read<uint32_t>(0x0015ed84) * 4 + static_cast<uint32_t>(idx), ri(0, 3) == 0 ? 1u : 0u);
        if (pass >= 6) w8(m + 0x53, R() & 3);
        w32(0x0015eea4, R() & 0xffff);
        seed_rand();
    }, 25);

    // ---- class 1548: state 0 / 1; in state 1 the scenes' lightning (game mode 2): the scene 0x16c860 +0x30 (3, 4, 10 or another),
    // its frame +0x34 about the effect's window (frame_scale_i(0x384) .. frame_scale_i(0x2710)), the scene's moby +0x184
    for (int pass = 0; pass < 6; ++pass)
    classcheck("class1548_update", 0x002e01d0u, kerwan_class1548_update_l3, [pass, mobys_of_class](uint32_t m) {
        const uint32_t SC = OV(0x0016c860);
        w8(m + 0x20, pass == 0 ? 0u : (pass == 5 ? u(ri(0, 3)) : 1u));
        w32(OV(0x0015f5c4), ri(0, 4) == 0 ? u(ri(0, 6)) : 2u);
        { static const uint32_t sc[] = {3, 3, 4, 10, 1, 7}; w32(SC + 0x30, sc[ri(0, 5)]); }
        w32(SC + 0x34, ri(0, 3) == 0 ? u(ri(0, 20000)) : u(ri(850, 1100)));
        w32(SC + 0x184, ri(0, 4) == 0 ? 0u : (coin() ? m : ee_read<uint32_t>(0x001413d0)));  // (the scene's moby the bolt strikes from)
        { const std::vector<uint32_t> ib = mobys_of_class(750); for (uint32_t k = 0; k < 3; ++k) w32(SC + 0x178 + 4 * k, ri(0, 2) == 0 && !ib.empty() ? ib[0] : (coin() ? m : 0u)); }  // (scenes 4 / 10: the smoking moby, +0x178 / +0x180: the Infobot, class 0x2ee)
        w32(0x0015eea4, R() & 0xffff);
        seed_rand();
    }, 25);

    // ---- the swingshot's rope (Veldin's 0x2c5dc8, Kerwan's 0x2b7830: the draw callback the swingshot's update (class 208) queues while
    // its hook is out): the swingshot moby of a level where the hero holds it (given and asked in hand, 30 frames run), its hook
    // (pvars +0x14: the hook moby, class 209, or the hero's), the wave words +0x18 / +0x1c / +0x20, the frame counter
    {
        static std::vector<uint8_t> grown;
        auto with_swingshot = [level] {
            uint8_t* ram = ee_get_ram_base();
            if (grown.empty()) {
                level();
                item_give(0xc, 0);
                w32(OV(0x0015f5c4), 0);  // (gameplay: the landing's mode 6 over)
                // the swingshot in hand: the equipment spawn (0x20ea80) with it as the item (+0x20d4) and no item moby yet
                w32(0x0013f350 + 0x20d4, 0xc);
                w32(0x0013f350 + 0x1090, 0);
                w32(OV(0x0015f3f8), 1);  // (the item budget: 0 until the landing ends)
                hero_equipment_spawn();
                for (int i = 0; i < 10; ++i) gameplay_frame();
                std::printf("  (swingshot: item in hand %d, its moby %08x)\n", ee_read<int32_t>(0x0013f350 + 0x10b8), ee_read<uint32_t>(0x0013f350 + 0x1090));
                grown.assign(ram, ram + 0x02000000);
            } else {
                std::memcpy(ram, grown.data(), grown.size());
            }
        };
        with_swingshot();
        const std::vector<uint32_t> ss = mobys_of_class(208), hooks = mobys_of_class(209);
        if (ss.empty()) {
            std::printf("%-26s no swingshot moby\n", "swingshot_rope_draw");
            ++g_fail;
        } else {
            check("swingshot_rope_draw", 0x002c5dc8, 60, Ret::None, [with_swingshot, ss, hooks] {
                with_swingshot();
                const uint32_t m = ss[0], pv = ee_read<uint32_t>(m + 0x78);
                w32(pv + 0x14, !hooks.empty() && ri(0, 3) != 0 ? hooks[0] : ee_read<uint32_t>(0x001413d0));
                if (!hooks.empty()) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x += rf(-20, 20); q.y += rf(-20, 20); q.z += rf(-3, 12);
                    ee_ref<Vec4>(hooks[0] + 0x10) = q;
                }
                wf(pv + 0x18, rf(-3.1f, 3.1f)); wf(pv + 0x1c, rf(0.05f, 2)); wf(pv + 0x20, rf(0, 10));  // (+0x1c 0 divides by zero: FLT_MAX, and angle_wrap's loop never ends)
                w32(0x0015eea4, R() & 0x3fff);
                return Args{{m}, {}};
            }, [](A a) { swingshot_rope_draw_tr(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- the classes Kerwan's own code spawns (none is placed: batch 112 found no subject): each grown from the level by its parent
    // class running in the port's own gameplay frames (and, for the parents the frame's moby pass leaves alone in the oracle, their
    // update called on each of them), the hero put next to the parent (the game's placement 0x216f90) and the camera at the parents,
    // until one is alive; then its update on those mobys, perturbed as batch 112 does (state 0 / 1 / as spawned, a hit, the hero
    // near / far)
    struct Spawned { int cls, parent; uint32_t fn; const char* name; void (*port)(uint32_t); int frames; void (*parent_port)(uint32_t); };
    static const Spawned spawned[] = {
        {235, 75, 0x002bae48u, "spawned235", class235_update, 0, nullptr},                         // (the ships': the landing's dump)
        {545, 455, 0x002c4718u, "spawned545", class545_update, 900, nullptr},                      // (the Spawner's)
        {627, 578, 0x002cbea8u, "spawned627", kerwan_class627_update_l3, 900, nullptr},                      // (the mine layer's mines)
        {833, 574, 0x002d43c8u, "spawned833", kerwan_class833_update_l3, 900, kerwan_class574_update_l3},
        {848, 631, 0x002d47c0u, "spawned848", class848_update, 900, class631_update},    // (the Blarg helicopter's)
        {898, 899, 0x002db020u, "spawned898", kerwan_class898_update_l3, 900, nullptr},
        {1633, 1818, 0x002e08c8u, "spawned1633", class1633_update, 900, sandmouse_update},  // (the Sandmouse's)
    };
    // The ships' 235s are spawned during the landing (game mode 6, its own frame loop: the oracle's gameplay frames leave the ships
    // still): their subjects come from a RAM dump of a headless run at its frame 20 (`tools/oracle/kerwan_tour.sh dumps` writes it to
    // assets/dumps/oracle/kerwan_0385/landing20.bin; the check is skipped without it)
    static std::vector<uint8_t> landing;
    if (g_oracle_level == 3 && landing.empty()) {
        if (FILE* f = std::fopen(dump_path("kerwan_0385/landing20.bin").c_str(), "rb")) {
            landing.resize(0x02000000 + 0x4000);
            if (std::fread(landing.data(), 1, landing.size(), f) != landing.size()) landing.clear();
            std::fclose(f);
        }
    }
    for (const Spawned& s : spawned) {
        if (g_oracle_level != 3) { k2check(s.name, s.fn, 0, Ret::None, nullptr, nullptr); continue; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, s.name) != 0) continue;
        static std::vector<uint8_t> grown;
        grown.clear();
        uint8_t* ram = ee_get_ram_base();
        int frames = 0;
        size_t nparents = 0;
        if (s.cls == 235) {
            if (landing.empty()) {
                std::printf("%-26s L3:0x%08x  skipped (no assets/dumps/oracle/kerwan_0385/landing20.bin: tools/oracle/kerwan_tour.sh dumps)\n", s.name, s.fn);
                ++g_skipped;
                continue;
            }
            std::memcpy(ram, landing.data(), 0x02000000);
            std::memcpy(ee_ram_ptr(0x70000000u), landing.data() + 0x02000000, 0x4000);
        } else {
            level();
            w32(OV(0x0015f5c4), 0);  // (gameplay)
            const std::vector<uint32_t> parents = mobys_of_class(s.parent);
            nparents = parents.size();
            if (!parents.empty()) {
                const uint32_t buf = ee_stack_alloc(0x20);
                ee_ref<Vec4>(buf) = ee_ref<Vec4>(parents[0] + 0x10);
                wf(buf, ee_read<float>(buf) + 6.0f);
                wf(buf + 8, ee_read<float>(buf + 8) + 1.0f);
                w32(buf + 0xc, 0);
                for (uint32_t k = 0x10; k < 0x20; k += 4) w32(buf + k, 0);
                hero_place(buf, buf + 0x10, -1, 1);
                ee_stack_free(0x20);
                for (; frames < s.frames && mobys_of_class(s.cls).empty(); ++frames) {
                    ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(parents[static_cast<size_t>(frames) % parents.size()] + 0x10);  // (the camera at a parent)
                    gameplay_frame();
                    if (s.parent_port != nullptr)
                        for (uint32_t p : parents)
                            if (ee_read<uint8_t>(p + 0x20) < 0xfd) s.parent_port(p);
                }
            }
        }
        const std::vector<uint32_t> ms = mobys_of_class(s.cls);
        if (ms.empty()) {
            std::printf("%-26s L3:0x%08x  none spawned in %d frames (parent class %d: %zu placed)\n", s.name, s.fn, frames, s.parent, nparents);
            continue;
        }
        std::printf("  (%s: %zu spawned after %d frames)\n", s.name, ms.size(), frames);
        grown.assign(ram, ram + 0x02000000);
        g_pre_call = k2_vu0_capture;
        // the collision's contact vectors' w lanes (vf25..vf29's, caller leftovers the port stores as 0: segment_collide 0x1efc70's
        // note) and their copies in the floor lights' normals (0x16e080 + 0x20 i + 0x1c, from the contact normal 0x173e80)
        const std::vector<uint32_t> ign_before = g_ignored_words;
        for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
        for (uint32_t i = 0; i < 0x20; ++i) g_ignored_words.push_back(OV(0x0016e080) + i * 0x20 + 0x1c);
        for (int pass = 0; pass < 8; ++pass)  // (0: as spawned, 1..6: the state byte 0..5, 7: a random small one)
        k2check(s.name, s.fn, 20, Ret::None, [ms, pass, give_hit, hero_near] {
            std::memcpy(ee_get_ram_base(), grown.data(), grown.size());
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            if (pass >= 1 && pass <= 6) w8(m + 0x20, u(pass - 1));
            if (pass == 7) w8(m + 0x20, u(ri(0, 9)));
            if (ri(0, 2) == 0) { const uint32_t pv = ee_read<uint32_t>(m + 0x78); if (pv != 0) for (uint32_t k = 0; k < 0x40; k += 4) if (ri(0, 7) == 0) w32(pv + k, u(ri(0, 40))); }  // (timers, counters)
            give_hit(m, 0x10000 | (R() & 0x3ffffu));
            hero_near(ee_ref<Vec4>(m + 0x10), 3.0f, 30.0f);
            seed_rand();
            return Args{{m}, {}};
        }, [s](A a) { k2_vu0_sync(); s.port(a.i[0]); return uint64_t{0}; });
        g_pre_call = nullptr;
        g_ignored_words = ign_before;
    }

    // ---- (P3.3) the trooper's bolt (833) and the mine (627) through their lives: grown as above, then the port's gameplay frames
    // run on (the parent's update, and the subject's where the frame's moby pass leaves it alone: its life word unchanged) and the
    // RAM kept every few frames while one is alive (a bolt's flight, a mine thrown, landed, armed). The checks pick a kept frame
    // and one of its subjects and set up a moment of the life: a bolt as it flies, the hero in its next step (a hit on the hero),
    // its next step down into the floor at its trooper's feet (a hit on the world), its life's end, its burst; a mine as it is,
    // the hero standing on it, its layer back within 3 after the wait (picked up), its age past 3600 / 5400 frames, its blast
    // (the camera near or far: the spray's count and the shake), its fade, its flight back to the layer, its throw
    {
        struct Life { int cls, parent; uint32_t fn; const char* name; void (*port)(uint32_t); void (*parent_port)(uint32_t); uint32_t life_word; int span, every; };
        static const Life lives[] = {
            {833, 574, 0x002d43c8u, "life833", kerwan_class833_update_l3, kerwan_class574_update_l3, 0x14, 60, 3},
            {627, 578, 0x002cbea8u, "life627", kerwan_class627_update_l3, nullptr, 0x6c, 400, 25},
            {898, 899, 0x002db020u, "life898", kerwan_class898_update_l3, nullptr, 0x64, 60, 5},
        };
        for (const Life& s : lives) {
            if (g_oracle_level != 3) { k2check(s.name, s.fn, 0, Ret::None, nullptr, nullptr); continue; }
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, s.name) != 0) continue;
            static std::vector<std::vector<uint8_t>> kept;
            static std::vector<std::vector<uint32_t>> subjects;
            kept.clear();
            subjects.clear();
            level();
            w32(OV(0x0015f5c4), 0);
            const std::vector<uint32_t> parents = mobys_of_class(s.parent);
            if (parents.empty()) { std::printf("%-26s no parent (class %d)\n", s.name, s.parent); ++g_fail; continue; }
            {
                const uint32_t buf = ee_stack_alloc(0x20);
                ee_ref<Vec4>(buf) = ee_ref<Vec4>(parents[0] + 0x10);
                wf(buf, ee_read<float>(buf) + 6.0f);
                wf(buf + 8, ee_read<float>(buf + 8) + 1.0f);
                w32(buf + 0xc, 0);
                for (uint32_t k = 0x10; k < 0x20; k += 4) w32(buf + k, 0);
                hero_place(buf, buf + 0x10, -1, 1);
                ee_stack_free(0x20);
            }
            if (s.cls == 833) {  // (the oracle's frames leave the troopers unalerted: each fires one bolt as its fire state does, at the
                                 // hero or 10 ahead of itself: the muzzle (its chain point 0), the step of length 6 dt halved in z)
                const uint32_t buf = ee_stack_alloc(0x20);
                const float step = ee_read<float>(0x0015ed6c) * 6.0f;
                for (size_t i = 0; i < parents.size() && i < 8; ++i) {
                    const uint32_t t = parents[i];
                    moby_chain_point_v(t, 0, ee_ref<Vec4>(buf));
                    Vec4 aim = ee_ref<Vec4>(0x0013f3d0);
                    if (i % 2 == 1) {
                        aim = ee_ref<Vec4>(t + 0x10);
                        aim.x += 10.0f * std::cos(ee_read<float>(t + 0x48)); aim.y += 10.0f * std::sin(ee_read<float>(t + 0x48)); aim.z += 1.0f;
                    }
                    Vec4 d = aim;
                    d.x -= ee_read<float>(buf); d.y -= ee_read<float>(buf + 4); d.z -= ee_read<float>(buf + 8);
                    const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
                    if (len < 0.01f) continue;
                    d.x *= step / len; d.y *= step / len; d.z *= 0.5f * step / len; d.w = 0.0f;
                    ee_ref<Vec4>(buf + 0x10) = d;
                    trooper_bolt_spawn_l3(buf + 0x10, buf, t, frame_scale_i(0x82), atan2_f(d.x, d.y), step, 0.75f);
                }
                ee_stack_free(0x20);
            }
            int frames = 0, since = -1;
            for (; frames < 900 + s.span && kept.size() < 12; ++frames) {
                ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(parents[static_cast<size_t>(frames) % parents.size()] + 0x10);
                std::vector<std::pair<uint32_t, uint32_t>> before;
                for (uint32_t m : mobys_of_class(s.cls)) before.push_back({m, ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + s.life_word)});
                gameplay_frame();
                if (s.parent_port != nullptr)
                    for (uint32_t p : parents)
                        if (ee_read<uint8_t>(p + 0x20) < 0xfd) s.parent_port(p);
                for (const auto& [m, w] : before)
                    if (ee_read<uint8_t>(m + 0x20) < 0xfe && static_cast<int16_t>(ee_read<uint16_t>(m + 0xa6)) == s.cls &&
                        ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + s.life_word) == w)
                        s.port(m);
                const std::vector<uint32_t> ms = mobys_of_class(s.cls);
                if (ms.empty()) continue;
                if (since < 0) since = frames;
                if ((frames - since) % s.every == 0) {
                    kept.emplace_back(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                    subjects.push_back(ms);
                }
            }
            if (kept.empty()) { std::printf("%-26s L3:0x%08x  none spawned in %d frames\n", s.name, s.fn, frames); ++g_fail; continue; }
            {
                std::string states;
                for (const auto& v : subjects) { states += std::to_string(v.size()); states += ' '; }
                std::printf("  (%s: %zu frames kept from frame %d, subjects per frame: %s)\n", s.name, kept.size(), since, states.c_str());
            }
            g_pre_call = k2_vu0_capture;
            const std::vector<uint32_t> ign_before = g_ignored_words;
            for (uint32_t a = OV(0x00173e80); a < OV(0x00173fc0); a += 0x10) g_ignored_words.push_back(a + 0xc);
            for (uint32_t i = 0; i < 0x20; ++i) g_ignored_words.push_back(OV(0x0016e080) + i * 0x20 + 0x1c);
            if (s.cls == 833) {
                // (a bolt's step into the floor: the segment collision's first triangle word (Veldin's 0x173e90, coll_hit_tri(0), stored
                // from a VU0 register by the original's collide (L3 pc 0x1ea5dc): 0 there, the port's own leftover vector; batch 124's
                // note on its w lane) and the scratchpad's 0x70003700 .. 0x7000371c (COMMON's known leftovers: saved registers' upper
                // halves); only for this check)
                for (uint32_t k = 0; k < 0x10; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
                for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
            }
            const int cls = s.cls;
            for (int pass = 0; pass < 8; ++pass)
            k2check(s.name, s.fn, 20, Ret::None, [pass, cls, parents, hero_near] {
                // (a mine touched by its layer: the first frames kept, the layer still by its mines)
                const size_t f = static_cast<size_t>(ri(0, cls == 627 && pass == 2 ? std::min(2, static_cast<int>(kept.size()) - 1) : static_cast<int>(kept.size()) - 1));
                std::memcpy(ee_get_ram_base(), kept[f].data(), kept[f].size());
                const uint32_t m = subjects[f][static_cast<size_t>(ri(0, static_cast<int>(subjects[f].size()) - 1))];
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), hero = ee_read<uint32_t>(0x001413d0);
                Vec4 at = ee_ref<Vec4>(m + 0x10);
                auto hero_at = [hero](const Vec4& q) { ee_ref<Vec4>(0x0013f3d0) = q; ee_ref<Vec4>(hero + 0x10) = q; };
                if (cls == 833) {
                    if (coin()) { Vec4 mv = ee_ref<Vec4>(0x0013f490); mv.x = rf(-0.3f, 0.3f); mv.y = rf(-0.3f, 0.3f); mv.z = rf(-0.3f, 0.3f); ee_ref<Vec4>(0x0013f490) = mv; }  // (the hero's move this frame: the bolt drifts with it, its z scaled by L3:0x161b28)
                    if (pass == 0) hero_near(at, 3.0f, 30.0f);
                    if (pass == 1 || pass == 2) {  // the hero in its next step
                        const float t = rf(0.2f, 1.2f);
                        Vec4 q = at;
                        q.x += ee_read<float>(pv) * t; q.y += ee_read<float>(pv + 4) * t; q.z += ee_read<float>(pv + 8) * t - rf(0.0f, 1.0f);
                        hero_at(q);
                    }
                    if (pass == 3) {  // into the floor at its trooper's feet
                        const uint32_t p = parents[static_cast<size_t>(ri(0, static_cast<int>(parents.size()) - 1))];
                        Vec4 q = ee_ref<Vec4>(p + 0x10);
                        q.x += rf(-1.0f, 1.0f); q.y += rf(-1.0f, 1.0f); q.z += rf(0.1f, 0.6f);
                        ee_ref<Vec4>(m + 0x10) = q;
                        wf(pv + 8, -rf(0.8f, 2.0f));
                        if (coin()) w32(pv + 0x10, p);
                    }
                    if (pass == 4) w32(pv + 0x14, u(ri(0, 2)));
                    if (pass == 5) w8(m + 0x20, 2);
                    if (pass == 6) { w8(m + 0x20, u(ri(0, 3))); hero_near(at, 2.0f, 12.0f); }
                    if (pass == 7) { wf(pv + 0x1c, rf(-3.1f, 3.1f)); wf(pv + 0x20, rf(-1.0f, 1.0f)); hero_near(at, 1.0f, 8.0f); }
                } else if (cls == 898) {  // an escalator's step: along its path, at its last leg (it retires there), the speed word
                    const uint32_t path = ee_read<uint32_t>(pv + 0x60);
                    const int32_t n = path != 0 ? static_cast<int32_t>(ee_read<uint32_t>(path)) : 0;
                    const float leg = path != 0 ? ee_read<float>(path + 0x1c) : 0.0f;
                    if (n > 2 && n < 1000 && leg > 0.0f && leg < 1000.0f) {
                        if (pass >= 4) wf(pv + 0x64, leg * static_cast<float>(n - 2) + rf(-1.5f * leg, 0.5f * leg));
                        else if (pass >= 2) wf(pv + 0x64, rf(0.0f, leg * static_cast<float>(n - 1)));
                    }
                    if (coin()) wf(L3_ADDR(0x00161bc0), rf(0.02f, 0.2f));
                } else {
                    const uint32_t layer = ee_read<uint32_t>(pv + 0x68);
                    if (pass == 0) hero_near(at, 3.0f, 30.0f);
                    if (pass == 1) {  // the hero standing on it (armed)
                        w8(m + 0x20, 3);
                        Vec4 q = at;
                        q.x += rf(-0.4f, 0.4f); q.y += rf(-0.4f, 0.4f);
                        hero_at(q);
                        wf(0x0013f3d8, at.z + rf(-0.35f, 0.35f));
                    }
                    if (pass == 2 && layer != 0) {  // its layer back after the wait
                        w8(m + 0x20, 3);
                        w32(pv + 0x64, u(ri(0, 1)));
                        Vec4 q = at;
                        q.x += rf(-3.5f, 3.5f); q.y += rf(-3.5f, 3.5f);
                        ee_ref<Vec4>(layer + 0x10) = q;
                        if (ri(0, 4) == 0) w8(layer + 0x20, coin() ? 0xfeu : 0xfdu);
                        if (ri(0, 3) != 0) {  // touching it (within 1): its layer during the wait or after, or a sibling mine (a moby
                                              // already near it: the sphere query finds mobys by their grid cells) made one that can be hit
                            Vec4 t = at;
                            t.x += rf(-0.6f, 0.6f); t.y += rf(-0.6f, 0.6f);
                            uint32_t o = layer;
                            if (coin()) for (uint32_t p : subjects[f]) if (p != m) { o = p; break; }
                            ee_ref<Vec4>(o + 0x10) = t;
                            if (ri(0, 3) != 0) w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x1000u);
                            w32(pv + 0x64, coin() ? 0u : u(ri(2, 30)));
                        }
                    }
                    if (pass == 3) {  // old
                        w8(m + 0x20, 3);
                        w32(pv + 0x6c, u(ri(3590, 5410)));
                        if (layer != 0 && coin()) w8(layer + 0x20, 0xfe);
                        if (ri(0, 4) == 0) w32(pv + 0x68, 0);
                    }
                    if (pass == 4) {  // the blast
                        w8(m + 0x20, 4);
                        Vec4 c = at;
                        const float r = rf(0.0f, 2.0f) < 1.0f ? rf(0.0f, 20.0f) : rf(20.0f, 40.0f), a = rf(-3.1f, 3.1f);
                        c.x += r * std::cos(a); c.y += r * std::sin(a);
                        ee_ref<Vec4>(OV(0x00166dc0)) = c;
                        if (coin()) hero_near(at, 2.0f, 6.0f);
                    }
                    if (pass == 5) {  // the fade
                        w8(m + 0x20, 5);
                        const float full = ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24);
                        wf(m + 0x2c, full * (coin() ? rf(0.0095f, 0.0115f) : rf(0.01f, 1.0f)));
                    }
                    if (pass == 6 && layer != 0) {  // back to the layer
                        w8(m + 0x20, 2);
                        Vec4 q = ee_ref<Vec4>(layer + 0x10);
                        const float r = coin() ? rf(0.0f, 0.3f) : rf(0.3f, 10.0f);
                        q.x += r; q.z += rf(-1.0f, 1.0f);
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                    if (pass == 7) {  // thrown
                        w8(m + 0x20, 1);
                        if (coin()) w8(m + 0x53, u(ri(0, 2)));
                        if (coin()) wf(m + 0x18, rf(3.0f, 7.0f));
                    }
                }
                seed_rand();
                return Args{{m}, {}};
            }, [s](A a) { k2_vu0_sync(); s.port(a.i[0]); return uint64_t{0}; });
            g_pre_call = nullptr;
            g_ignored_words = ign_before;
            kept.clear();
            kept.shrink_to_fit();
        }
    }

    // ---- (task 0385b) the train ride: the RAM of a headless run 855 frames into a ride (the hero boarded at frame 800; the far
    // station's camera trigger 737 near the end of its shot: `tools/oracle/kerwan_tour.sh dumps` writes assets/dumps/oracle/kerwan_0385/train1655.bin);
    // the locomotive (822), its carriages (1210) and the stations' camera triggers (737), the frame counter varied, and the 737s' shot
    // timer (+0x2c) at its end or not, their +0xbc (the train's hold on the shot) kept or cleared: the shot's end hands the hero back
    // (hero_enter_idle 0x21a190: the transliteration had it glued to hero_item_idle 0x229a98 and the hero stayed in state 0x72)
    if (g_oracle_level == 3) {
        static std::vector<uint8_t> ride;
        if (ride.empty())
            if (FILE* f = std::fopen(dump_path("kerwan_0385/train1655.bin").c_str(), "rb")) {
                ride.resize(0x02000000 + 0x4000);
                if (std::fread(ride.data(), 1, ride.size(), f) != ride.size()) ride.clear();
                std::fclose(f);
            }
        struct RideCls { int cls; uint32_t fn; const char* name; void (*port)(uint32_t); };
        static const RideCls ride_cls[] = {
            {822, 0x00292e98u, "train_ride822", kerwan_class822_update_l3},
            {1210, 0x002953f8u, "train_ride1210", train_carriage_update_l3},
            {737, 0x002cf340u, "train_ride737", scripted_camera_trigger_update_l1},
        };
        for (const RideCls& rc : ride_cls) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, rc.name) != 0) continue;
            if (ride.empty()) {
                std::printf("%-26s L3:0x%08x  skipped (no assets/dumps/oracle/kerwan_0385/train1655.bin: tools/oracle/kerwan_tour.sh dumps)\n", rc.name, rc.fn);
                ++g_skipped;
                continue;
            }
            auto load = [] {
                std::memcpy(ee_get_ram_base(), ride.data(), 0x02000000);
                std::memcpy(ee_ram_ptr(0x70000000u), ride.data() + 0x02000000, 0x4000);
            };
            load();
            const std::vector<uint32_t> ms = mobys_of_class(rc.cls);
            if (ms.empty()) { std::printf("%-26s no moby\n", rc.name); ++g_fail; continue; }
            g_pre_call = k2_vu0_capture;
            k2check(rc.name, rc.fn, rc.cls == 737 ? 60 : 30, Ret::None, [load, ms, rc] {
                load();
                if (ri(0, 2) != 0) w32(0x0015eea4, ee_read<uint32_t>(0x0015eea4) + u(ri(0, 600)));
                if (rc.cls == 737)
                    for (uint32_t m : ms) {
                        if (ee_read<uint8_t>(m + 0x20) != 2) continue;
                        if (ri(0, 2) != 0) w32(ee_read<uint32_t>(m + 0x78) + 0x2c, u(ri(0, 3)));
                        if (ri(0, 3) == 0) w8(m + 0xbc, 0);
                    }
                seed_rand();
                return Args{{ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))]}, {}};
            }, [rc](A a) { k2_vu0_sync(); rc.port(a.i[0]); return uint64_t{0}; });
            g_pre_call = nullptr;
        }
    }

    // ---- class 994's draw callback (0x2dc990, queued by the update through 0x1f93e0): the moby's state and the frame counter
    if (g_oracle_level != 3) {
        k2check("class994_draw", 0x002dc990, 0, Ret::None, nullptr, nullptr);
    } else {
        level();
        const std::vector<uint32_t> ms = mobys_of_class(994);
        if (ms.empty()) std::printf("%-26s no moby of class 994\n", "class994_draw");
        else
            k2check("class994_draw", 0x002dc990, 30, Ret::None, [level, ms] {
                level();
                const uint32_t m = ms[0];
                w32(0x0015eea4, R() & 0xffff);  // (the frame counter)
                w32(OV(0x0015f458), u(ri(0, 2)));  // (a draw mode the callback tests against 1)
                ee_ref<Vec4>(OV(0x00166dc0)) = ee_ref<Vec4>(m + 0x10);  // the camera at the moby, or somewhere else
                if (coin()) wfrand(OV(0x00166dc0), 3, -50, 400);
                return Args{{m}, {}};
            }, [](A a) { kerwan_class994_draw_l3(a.i[0]); return uint64_t{0}; });
    }

    // ---- the level's own sound emitter types (0x2ed800, 0x2ed960): the level's emitters of each type, the hero / camera near or far,
    // their timers / channel words perturbed
    struct Em { uint32_t fn; const char* name; void (*port)(uint32_t); };
    static const Em ems[] = {
        {0x002ed800u, "emitter_2ed800", emitter_reverb_box_update},
        {0x002ed960u, "emitter_2ed960", emitter_music_box_update},
    };
    for (const Em& e : ems) {
        if (g_oracle_level != 3) { k2check(e.name, e.fn, 0, Ret::None, nullptr, nullptr); continue; }
        level();
        std::vector<uint32_t> list;
        for (int32_t i = 0; i < static_cast<int32_t>(ee_read<uint32_t>(0x0013f2e0)); ++i) {
            const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
            if (ee_read<uint32_t>(em + 4) == e.fn) list.push_back(em);
        }
        if (list.empty()) { std::printf("%-26s L3:0x%08x  no emitter of this type\n", e.name, e.fn); continue; }
        for (int pass = 0; pass < 4; ++pass)
        k2check(e.name, e.fn, 40, Ret::None, [level, list, pass, box = e.fn == 0x002ed800u] {
            level();
            const uint32_t em = list[static_cast<size_t>(ri(0, static_cast<int>(list.size()) - 1))];
            const uint32_t prm = ee_read<uint32_t>(em + 8);
            // the hero inside the emitter's box (its unit box through the matrix at +0x10, about the centre +0x40) or outside it
            Vec4 l = {rf(-1.6f, 1.6f), rf(-1.6f, 1.6f), rf(-1.6f, 1.6f), 0.0f};
            if (ri(0, 2) == 0) l = {rf(-0.9f, 0.9f), rf(-0.9f, 0.9f), rf(-0.9f, 0.9f), 0.0f};
            Vec4 w;
            mat3_transform(w, l, ee_ref<Mat4>(em + 0x10));
            vec3_add(w, w, ee_ref<Vec4>(em + 0x40));
            ee_ref<Vec4>(0x0013f3d0) = w;
            if (box && pass >= 1) w8(prm + 3, R() & 1);                               // (0x2ed800: inside last frame)
            if (!box && pass >= 1) w32(prm + 8, R() & 1);                             // (0x2ed960: inside last frame)
            if (!box && pass >= 2) { w16(prm + 0xc, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 15))); w16(prm + 0xe, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 15))); }
            if (!box && pass == 3) w32(prm + 0xc, 0xffffffffu);
            seed_rand();
            return Args{{em}, {}};
        }, [e](A a) { e.port(a.i[0]); return uint64_t{0}; });
    }

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
