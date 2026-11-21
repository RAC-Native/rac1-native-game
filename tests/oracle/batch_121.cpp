// Oracle batch 121 (task 0356): level 9's (Gaspar) own moby classes on its real level data. Run with
// ORACLE_LEVEL=9 build/tests/ovl_oracle --batch 121 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 9's own function at its own address, the port the one registered for it (the
// transliterations batalia_classes_tr_N.cpp).

#include "ovl_oracle.h"

#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_class1023.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class1885.h>
#include <game/ovl/shared_class1886.h>
#include <game/ovl/shared_class193.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class52.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_tracer.h>
#include <game/ovl/enemy_749.h>
#include <game/level/level_data.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
// The checks named here reach sphere_collide or capsule_collide, whose chunk buffers, unpacked vertices and cell
// walk the port keeps on the host stack (the original: 0x70000000 .. 0x70002000, 0x70002000, 0x70003800): A2's
// collision scratch ranges are left out of these checks, and only of them (stage 5, task C2b).
bool c2b_spr_check(const char* name) {
    static const char* const kNames[] = {"class1023", "class1206", "class193", "class52", "p92_class1206", "class1134",
                                         "class1885", "class263", "class805", "p91_2ea778", "p91_class313", "p91_class320",
                                         "p91_class321", "p91_cloudctl", "p91_meteor_0", "p91_meteor_1", "p91_meteor_2"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}


// A check whose original is level 9's own function at `l9addr` (not a Veldin address to translate).
void l9check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B121_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 9) {
        std::printf("%-26s L9:0x%08x  skipped (level 9 only: run with ORACLE_LEVEL=9)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L9:0x%08x\n", name, l9addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_121() {
    if (g_only_batch != 121) return;
    g_batch = 121;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(0x00174610u + k);
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
    // the level's mobys whose update function is `fn` (level 9's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 9) { l9check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L9:0x%08x  no moby on this level\n", name, fn); return; }
        l9check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
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

    // the debris / spawns leave w lanes of a VU0 sin / cos in the original (the port has none)
    // (the transliterated classes' checks below take both lists; the hand-ported ones of P9.1 neither)
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 9) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }
    const std::vector<uint32_t> batch_ignored = g_ignored_words;

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l9.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class1182", 0x002c26c8, gas_class1182_update_l9, 0x0, false},
        {"class52", 0x002c5990, class52_update, 0x0, false},
        {"class156", 0x002d0d88, class156_update, 0x0, false},
        {"class193", 0x002e27d8, class193_update, 0x0, false},
        {"class258", 0x002ea338, class258_update, 0x0, false},
        {"class263", 0x002eb970, gas_class263_update_l9, 0x0, false},
        {"class276", 0x002edc30, gas_class276_update_l9, 0x0, false},
        {"class279", 0x002ee110, gas_class279_update_l9, 0x0, false},
        {"class284", 0x002ee428, empty_update, 0x0, false},
        {"class313", 0x002ef430, gas_class313_update_l9, 0x0, false},
        {"class317", 0x002ef7f8, sea_update, 0x0, false},
        {"class320", 0x002ef9b0, gas_class320_update_l9, 0x0, false},
        {"class324", 0x002efe40, empty_update, 0x0, false},
        {"class417", 0x002f0040, gas_class417_update_l9, 0x0, false},
        {"class567", 0x002f6678, class567_update, 0x0, false},
        {"class605", 0x002f7658, zone_marker_update, 0x0, false},
        {"class664", 0x002f86a0, gas_class664_update_l9, 0x0, false},
        {"class803", 0x002f8c70, swingshot_target_update, 0x0, false},
        {"class805", 0x002f90e0, spawn_point_update_l1, 0x0, false},
        {"class832", 0x002fb288, class832_update, 0x0, false},
        {"class1000", 0x00300888, gas_class1000_update_l9, 0x0, false},
        {"class1023", 0x003009d0, class1023_update, 0x0, false},
        {"class1134", 0x003022f0, gold_bolt_update, 0x4, false},
        {"class1150", 0x003033a0, gas_class1150_update_l9, 0x0, false},
        {"class1172", 0x00303d10, gas_class1172_update_l9, 0x0, false},
        {"class1181", 0x00304360, gas_class1181_update_l9, 0x0, false},
        {"class1201", 0x00304c80, gas_class1201_update_l9, 0x0, false},
        {"class1206", 0x00305a28, gas_class1206_update_l9, 0x0, false},
        {"class1212", 0x00305dc0, bg_ship1_update, 0x0, false},
        {"class1213", 0x00306720, bg_ship2_update, 0x0, false},
        {"class1258", 0x00307d68, shell_update, 0x0, false},
        {"class1285", 0x003082d8, gas_class1285_update_l9, 0x0, false},
        {"class1292", 0x00308f08, tracer_update, 0x0, false},
        {"class1293", 0x003091b0, gas_class1293_update_l9, 0x0, false},
        {"class1300", 0x00309828, empty_update, 0x0, false},
        {"class1766", 0x00309c08, gas_class1766_update_l9, 0x0, false},
        {"class1849", 0x0030a678, class1844_update, 0x0, false},
        {"class1850", 0x0030a770, empty_update, 0x0, false},
        {"class1885", 0x0030ab80, class1885_update, 0x0, false},
        {"class1886", 0x0030b3b0, class1886_update, 0x0, false},
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
    if (g_oracle_level == 9 && std::getenv("B121_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
    for (const TrCls& c : trcls) {
        if (std::getenv("B121_CLS") != nullptr && std::strcmp(std::getenv("B121_CLS"), c.name) != 0) continue;
        // the hand ports (P9.1, P9.2): none of the two lists
        const bool hand = c.fn == gas_class1182_update_l9 || c.fn == gas_class263_update_l9 || c.fn == gas_class276_update_l9 || c.fn == gas_class279_update_l9 ||
                          c.fn == gas_class313_update_l9 || c.fn == gas_class320_update_l9 || c.fn == gas_class417_update_l9 || c.fn == gas_class664_update_l9 ||
                          c.fn == gas_class1000_update_l9 || c.fn == gas_class1150_update_l9 || c.fn == gas_class1172_update_l9 || c.fn == gas_class1181_update_l9 ||
                          c.fn == gas_class1201_update_l9 || c.fn == gas_class1206_update_l9 || c.fn == gas_class1285_update_l9 || c.fn == gas_class1293_update_l9 ||
                          c.fn == gas_class1766_update_l9;
        g_ignored_words = batch_ignored;
        if (hand) g_ignored_words = saved_ignored;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 9 && std::getenv("B121_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B121_ST") == nullptr || std::atoi(std::getenv("B121_ST")) == st) && (std::getenv("B121_PASS") == nullptr || std::atoi(std::getenv("B121_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_ignored_words = batch_ignored;
    g_extra_noop_hooks.clear();

    // ---- level 9's sound emitter types (3, 6: section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 9) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x003158e8, emitter_fade_box_update}, {"emitter3", 0x00315c88, emitter_reverb_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L9:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B121_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l9check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 9's class hooks (the table at 0x209b00): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 9) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        // (P9.2: the hand ports: +0 / +4 / +8 the shared carry_state_hook, +0xc / +0x10 gaspar_carry.cpp's)
        struct Hk { const char* name; uint32_t addr; uint32_t slot; int32_t (*fn)(uint32_t, uint32_t, uint32_t, float); };
        static const Hk hks[] = {
            {"hook_2e2680", 0x2e2680, 0x0, [](uint32_t m, uint32_t a1, uint32_t a2, float f) { return carry_state_hook(m, a1, a2, f, 0, 7, 1); }},
            {"hook_2e26d0", 0x2e26d0, 0x4, [](uint32_t m, uint32_t a1, uint32_t a2, float f) { return carry_state_hook(m, a1, a2, f, 4, 7, 1); }},
            {"hook_2e2720", 0x2e2720, 0x8, [](uint32_t m, uint32_t a1, uint32_t a2, float f) { return carry_state_hook(m, a1, a2, f, 8, 7, 1); }},
            {"hook_2e2770", 0x2e2770, 0xc, gas_hook_2e2770_l9},
            {"hook_2e27a0", 0x2e27a0, 0x10, [](uint32_t m, uint32_t, uint32_t, float) { return static_cast<int32_t>(gas_hook_2e27a0_l9(m)); }},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L9:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B121_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            const bool p10 = hk.addr == 0x2e27a0;
            l9check(hk.name, hk.addr, 40, Ret::Int32, [level, own, p10] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t m = own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                else if (ri(0, 3) == 0) w8(m + 0x20, 7);  // (P9.2: carried, let go)
                if (p10 && ri(0, 7) == 0) w32(m + 0x78, 0);  // (P9.2: the hook +0x10 without pvars, or without a moby)
                const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                return Args{{p10 && ri(0, 9) == 0 ? 0u : m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
            }, [fn](A a) { return r32(static_cast<uint32_t>(fn(a.i[0], a.i[1], a.i[2], a.f[0]))); });
        }
        g_extra_noop_hooks.clear();
    }

    // ---- moby_spawn (the Veldin port) against level 9's own copy (L9:0x0026e1c0): the classes' spawns all go through it
    if (g_oracle_level == 9) {
        l9check("moby_spawn", 0x0026e1c0, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x34, 0xc1, 0x107, 0x25d, 0x298, 0x4a5, 0x739};  // (classes of level 9's own table: 52, 193, 263, 605, 664, 1181, 1849)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 9's camera hook (0x314220, the pointer its setter puts at 0x167190) and the callback it queues (0x314280)
    if (g_oracle_level == 9) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167180;
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00314220u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l9check("cam0_hook", 0x00314220, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l9check("cam_callback", 0x00314280, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- class 263's random states add vectors made of arbitrary words: the Veldin port vec3_add against the original on RAW random float words (any exponent, incl. 255: a VU0 operand with
    // exponent 255 is read as +-max, so -max + a tiny positive truncates to -(max - 1 ulp); task 0356)
    if (g_oracle_level == 9) {
        l9check("vec3_add_raw", 0x0021ef58, 600, Ret::None, [] {
            for (uint32_t k = 0; k < 12; ++k) w32(0x00180000u + 4 * k, R());
            for (uint32_t k = 0; k < 8; ++k) if (coin()) w32(0x00180000u + 4 * k, (R() & 0x807fffffu) | (u(ri(0, 255)) << 23));
            return Args{{0x00180020u, 0x00180000u, 0x00180010u}, {}};
        }, [](A a) { vec3_add(ee_ref<Vec4>(a.i[0]), ee_ref<Vec4>(a.i[1]), ee_ref<Vec4>(a.i[2])); return uint64_t{0}; });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 9) {
        static const uint32_t empties[] = {0x002ee428, 0x002efe40, 0x00309828, 0x0030a770};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l9check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- P9.1: the hand ports of Gaspar's own code (gaspar_structure / _meteors / _breakables / _platform.cpp), part 1: each
    // update on states made for its branches (its subjects spawned where the level has none), each helper called directly.
    // They need none of the class loop's ignores (the spawns' and the particle records' w lanes): only the batch's base list.
    if (g_oracle_level == 9) {
        const std::vector<uint32_t> loop_ignored = g_ignored_words;
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        auto p91 = [&](const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
            g_ignored_words = saved_ignored;
            l9check(name, addr, n, ret, gen, port);
            g_ignored_words = loop_ignored;
        };
        const uint32_t P = 0x0013f350, CAM = L9_ADDR(0x00166f40), X = 0x00180000;  // the hero, the camera, a scratch block
        // the frame rate: 60 Hz (the snapshot's), or 50 Hz one case in four
        auto rates = [] {
            if (ri(0, 3) != 0) return;
            wf(0x0015ed68, 1.2f);
            wf(0x0015ed6c, 0.02f);
            wf(0x0015ed70, 0.0004f);
        };
        bool keep_frame = false;  // (P9.2: a generator that set the frame counter itself, L9 0x15f5cc: kept)
        auto finish = [rates, &keep_frame] {
            rates();
            if (!keep_frame) w32(OV(0x0015f5cc), R() & 0xffff);
            keep_frame = false;
            seed_rand();
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto near = [](uint32_t dst, uint32_t at, float r, float dz) {
            Vec4 q = ee_ref<Vec4>(at);
            q.x += rf(-r, r); q.y += rf(-r, r); q.z += rf(-dz, dz);
            ee_ref<Vec4>(dst) = q;
        };
        // a subject of class `cls` spawned at the hero's place (on the ground), its state 0
        auto spawn_at_hero = [near, P](uint32_t cls) {
            const uint32_t m = moby_spawn(cls);
            if (m == 0) return 0u;
            near(m + 0x10, P + 0x80, 3.0f, 0.0f);
            wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-0.5f, 2.0f));
            moby_update_transform(m);
            return m;
        };
        // the camera near `at` (for the clamps at 64 / 100 and the shake within 15), else where it is
        auto camera_near = [near, CAM](uint32_t at) {
            const int k = ri(0, 3);
            if (k == 0) near(CAM, at, 5.0f, 3.0f);
            else if (k == 1) near(CAM, at, 40.0f, 10.0f);
            else if (k == 2) near(CAM, at, 120.0f, 30.0f);
        };
        auto hit_on = [give_hit](uint32_t m, uint32_t mask) {
            give_hit(m, mask);
            if (ee_read<uint8_t>(m + 0xa4) == 0xff) return;
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            wfrand(e + 0x10, 4, -1, 1);
            if (coin()) w32(e + 0x34, m);
        };
        auto upd = [](void (*fn)(uint32_t)) { return [fn](const Args& a) { fn(a.i[0]); return uint64_t{0}; }; };

        // class 1182 .. 1189 (the structure): set up (the story byte, the base's pieces, a piece far from its base), riding /
        // fused (struck, the fuse out), the burst (heavy pieces too), flying (into the lava at 25, out of the level), sinking
        {
            const std::vector<uint32_t> all = mobys_with(0x002c26c8);
            std::vector<uint32_t> bases;
            for (uint32_t m : all) if (ee_read<int16_t>(m + 0xa6) == 0x4a0) bases.push_back(m);
            if (std::getenv("B121_INFO") != nullptr) std::printf("INFO p91 1182: %zu, bases %zu\n", all.size(), bases.size());
            for (int st = 0; st < 6; ++st) {
                char nm[32];
                std::snprintf(nm, sizeof nm, "p91_class1182_s%d", st);
                p91(nm, 0x002c26c8, st == 4 ? 160 : 80, Ret::None, [&, st] {
                    level();
                    const uint32_t m = (st == 0 && coin() && !bases.empty()) ? pick(bases) : pick(all);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, u(st));
                    if (st == 0) {
                        const int k = ri(0, 3);
                        w32(pv + 0x68, k == 0 ? 0xffffffffu : u(ri(0, 15)));
                        if (k != 0) w8(0x0013d388 + ee_read<uint32_t>(pv + 0x68) + 0x39, coin() ? 1u : 0u);
                        w16(pv + 0x28, coin() ? 1u : 0u);
                        if (ri(0, 3) == 0) wf(m + 0x10, ee_read<float>(m + 0x10) + rf(-40, 40));
                    } else if (st <= 2) {
                        w8(m + 0xbc, ri(0, 2) == 0 ? 0u : 1u);
                        if (coin()) w16(pv + 0x2a, u(ri(0, 3)));
                        const uint32_t base = ee_read<uint32_t>(pv + 0x2c);
                        if (base != 0 && ri(0, 3) == 0) w8(base + 0x20, u(ri(1, 4)));
                        if (ri(0, 5) == 0) w32(pv + 0x2c, 0);
                        for (int i = 0; i < 3; ++i) if (coin()) wf(pv + 0x40 + 4 * u(ri(0, 8)), rf(-3, 3));
                    } else if (st == 3) {
                        near(P + 0x80, m + 0x10, 20.0f, 5.0f);
                        if (coin()) w32(pv + 0x68, 0xffffffffu);
                        else w32(pv + 0x68, u(ri(0, 15)));
                        wf(pv + 0x20, coin() ? 0.0f : rf(-0.2f, 0.2f));
                    } else {
                        wfrand(pv, 3, -1.5f, 1.5f);
                        wfrand(pv + 0x20, 2, -0.2f, 0.2f);
                        wf(pv + 0x64, rf(0, 0.05f));
                        const int k = ri(0, 5);
                        if (k <= 2) wf(m + 0x18, rf(23.5f, 27.0f));
                        else if (k == 3) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(5, 12.5f) : rf(1010.5f, 1020));
                    }
                    finish();
                    return Args{{m}, {}};
                }, upd(gas_class1182_update_l9));
            }
        }
        // the structure's helpers: a piece of a base, the nearest moby of a class
        {
            const std::vector<uint32_t> all = mobys_with(0x002c26c8);
            std::vector<uint32_t> bases;
            for (uint32_t m : all) if (ee_read<int16_t>(m + 0xa6) == 0x4a0) bases.push_back(m);
            p91("p91_2c2500", 0x002c2500, 60, Ret::Int32, [&] {
                level();
                const uint32_t m = bases.empty() ? pick(all) : pick(bases);
                near(X, m + 0x10, 2.0f, 1.0f);
                finish();
                return Args{{m, X, u(ri(0x49e, 0x4a5))}, {}};
            }, [](const Args& a) { return r32(gas_2c2500_l9(a.i[0], a.i[1], a.i[2])); });
            p91("p91_2c2610", 0x002c2610, 80, Ret::Int32, [&] {
                level();
                near(X, pick(all) + 0x10, coin() ? 5.0f : 200.0f, 5.0f);
                wf(X + 0x20, rf(-9, 9));
                static const uint32_t cls[] = {0x4a0, 0x49e, 0x4a3, 0x107, 0x298};
                finish();
                return Args{{X, cls[ri(0, 4)], coin() ? X + 0x20 : 0u}, {}};
            }, [](const Args& a) { return r32(gas_2c2610_l9(a.i[0], a.i[1], a.i[2])); });
        }

        // classes 263 .. 266: a meteor (low over the level, aimed at the lava, past its target, dying), a puff of smoke (fading,
        // gone), a cloud on its path (its ends), the controller (its set-up, a new cloud, its path's end)
        {
            std::vector<uint32_t> meteors, clouds, ctls;
            for (uint32_t m : mobys_with(0x002eb970)) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t kind = ee_read<uint32_t>(pv + 0x4c);
                if (kind == 1) meteors.push_back(m);
                else if (kind == 0) (ee_read<uint8_t>(m + 0xbc) == 0 ? ctls : clouds).push_back(m);
            }
            if (std::getenv("B121_INFO") != nullptr) std::printf("INFO p91 263: meteors %zu clouds %zu controllers %zu\n", meteors.size(), clouds.size(), ctls.size());
            if (!meteors.empty()) {
                for (int sc = 0; sc < 4; ++sc) {
                    char nm[32];
                    std::snprintf(nm, sizeof nm, "p91_meteor_%d", sc);
                    p91(nm, 0x002eb970, 120, Ret::None, [&, sc] {
                        level();
                        const uint32_t m = pick(meteors);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, 0);
                        wf(pv + 0x54, rf(0.5f, 3.0f));
                        w32(pv + 0x58, u(ri(-1, 4)));
                        if (sc == 0) {  // low over the level near the hero: its way through the ground (or still high)
                            near(pv + 0x20, P + 0x80, 4.0f, 0.0f);
                            near(pv, pv + 0x20, 6.0f, 0.0f);
                            wf(pv + 8, ee_read<float>(pv + 0x28) + rf(-2, 30));
                            if (ri(0, 4) == 0) wf(pv + 0x28, 25.0f);
                            if (ri(0, 5) == 0) wf(pv + 8, rf(100, 400));
                        } else if (sc == 1) {  // over the lava
                            wf(pv + 0x28, 25.0f);
                            wf(pv + 8, rf(24, 28.5f));
                            if (coin()) wf(pv, rf(0, 700));
                            if (coin()) wf(pv + 4, rf(0, 700));
                        } else if (sc == 2) {  // just past its target's height
                            near(pv + 0x20, P + 0x80, 2.0f, 0.0f);
                            if (ri(0, 3) == 0) wf(pv + 0x28, coin() ? 25.0f : rf(25.5f, 27.5f));
                            near(pv, pv + 0x20, 1.0f, 0.0f);
                            wf(pv + 8, ee_read<float>(pv + 0x28) + rf(-1, 2.5f));
                        } else {  // dying
                            w8(m + 0x20, u(ri(1, 12)));
                            if (coin()) wf(pv + 0x28, 25.0f);
                        }
                        camera_near(pv);
                        w32(0x001413d4, ri(0, 3) == 0 ? 0x32u : 0u);
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class263_update_l9));
                }
                p91("p91_puff", 0x002eb970, 80, Ret::None, [&] {
                    level();
                    const uint32_t src = pick(meteors);
                    wfrand(X, 4, -2, 2);
                    camera_near(ee_read<uint32_t>(src + 0x78));
                    const uint32_t p = gas_2ea528_l9(src, X);
                    if (p == 0) return Args{{src}, {}};
                    static const uint32_t fades[] = {0, 1, 2, 3, 4, 5, 0x7f};
                    w8(p + 0x23, ri(0, 3) == 0 ? u(ri(0, 0xff)) : fades[ri(0, 6)]);
                    camera_near(ee_read<uint32_t>(p + 0x78) + 0x20);
                    finish();
                    return Args{{p}, {}};
                }, upd(gas_class263_update_l9));
                p91("p91_2ea528", 0x002ea528, 60, Ret::Int32, [&] {
                    level();
                    const uint32_t src = pick(meteors);
                    wfrand(X, 4, -2, 2);
                    camera_near(ee_read<uint32_t>(src + 0x78));
                    finish();
                    return Args{{src, X}, {}};
                }, [](const Args& a) { return r32(gas_2ea528_l9(a.i[0], a.i[1])); });
                p91("p91_2efe48", 0x002efe48, 60, Ret::Int32, [&] {
                    level();
                    near(X, ee_read<uint32_t>(pick(meteors) + 0x78), 3.0f, 3.0f);
                    wfrand(X + 0x10, 4, -3, 3);
                    const float lo = rf(0, 4);
                    finish();
                    return Args{{R(), X, X + 0x10}, {lo, lo + rf(0, 12)}};  // (a0 unused: the original sets the class there)
                }, [](const Args& a) { return r32(gas_2efe48_l9(a.i[1], a.i[2], a.f[0], a.f[1])); });
            }
            p91("p91_2ea778", 0x002ea778, 120, Ret::Int32, [&] {
                level();
                const int k = ri(0, 3);
                if (k == 0) near(CAM, P + 0x80, 300.0f, 100.0f);
                else if (k >= 2) ee_ref<Vec4>(CAM) = Vec4{rf(1206, 1306), rf(706, 806), rf(300, 350), 1.0f};  // (amid where the meteors start)
                finish();
                return Args{{}, {}};
            }, [](const Args&) { return r32(gas_2ea778_l9()); });
            if (!clouds.empty()) {
                p91("p91_cloud", 0x002eb970, 100, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(clouds);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const int32_t pi = ee_read<int32_t>(pv + 0x10);
                    if (pi >= 0) {
                        const uint32_t path = lvd::path(pi);
                        const int32_t n = ee_read<int32_t>(path);
                        const int k = ri(0, 3);
                        const int32_t at = k == 0 ? 0 : k == 1 ? n - 2 : k == 2 ? n - 3 : ri(0, n - 2);
                        w32(pv + 0x14, u(at));
                        near(pv + 0x20, path + 0x10 + u(at) * 0x10, 6.0f, 2.0f);
                        wf(pv + 0xc, coin() ? rf(0, 0.5f) : rf(5, 60));
                        if (k == 1 && coin()) {  // about to reach the path's end (gone, its controller told)
                            near(pv + 0x20, path + u(n) * 0x10, 3.0f, 1.0f);
                            wf(pv + 0xc, rf(2, 20));
                        }
                    }
                    finish();
                    return Args{{m}, {}};
                }, upd(gas_class263_update_l9));
            }
            if (!ctls.empty()) {
                p91("p91_cloudctl", 0x002eb970, 160, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(ctls);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (ri(0, 4) == 0) w32(pv + 0x48, 0);
                    w32(pv + 0x1c, u(ri(0, 2)));
                    w32(pv + 0x18, u(ri(46, 52)));
                    const int32_t pi = ee_read<int32_t>(pv + 0x10);
                    if (pi >= 0) {
                        const uint32_t path = lvd::path(pi);
                        const int32_t n = ee_read<int32_t>(path);
                        const int k = ri(0, 2);
                        w32(pv + 0x14, u(k == 0 ? 0 : k == 1 ? n - 2 : ri(0, n - 2)));
                        wf(pv + 0xc, coin() ? rf(0, 0.5f) : rf(5, 60));
                        if (k == 1) {  // about to reach the path's end (back to its start)
                            near(pv + 0x20, path + u(n) * 0x10, 3.0f, 1.0f);
                            wf(pv + 0xc, rf(2, 20));
                        }
                    }
                    if (ri(0, 6) == 0) w32(pv + 0x10, 0xffffffffu);
                    finish();
                    return Args{{m}, {}};
                }, upd(gas_class263_update_l9));
                p91("p91_2eaa50", 0x002eaa50, 60, Ret::Int32, [&] {
                    level();
                    const uint32_t m = pick(ctls);
                    if (ri(0, 4) == 0) w32(ee_read<uint32_t>(m + 0x78) + 0x10, 0xffffffffu);
                    finish();
                    return Args{{m}, {}};
                }, [](const Args& a) { return r32(gas_2eaa50_l9(a.i[0])); });
            }
        }
        // class 417 (a splash): moving, its animation over
        p91("p91_class417", 0x002f0040, 60, Ret::None, [&] {
            level();
            const uint32_t m = spawn_at_hero(0x1a1);
            if (m != 0) {
                wfrand(ee_read<uint32_t>(m + 0x78), 4, -1, 1);
                w8(m + 0x70, R() & 0xff);
            }
            finish();
            return Args{{m}, {}};
        }, upd(gas_class417_update_l9));

        // the breakables: class 276 (its builds 1298 .. 1300 by its class number, the level's flag byte, safe / hit), its next build,
        // a chunk; class 279 (hit points, hit), its remains; the stone 313 and the chunks 320 / 321 (bouncing, on the lava, out of
        // reach, their time out)
        {
            const std::vector<uint32_t> b276 = mobys_with(0x002edc30);
            if (!b276.empty()) {
                p91("p91_class276", 0x002edc30, 120, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(b276);
                    static const uint32_t cls[] = {0x114, 0x512, 0x513, 0x514};
                    w16(m + 0xa6, cls[ri(0, 3)]);
                    w8(m + 0x20, coin() ? 0u : 1u);
                    w8(m + 0xbc, u(ri(0, 2)));
                    hit_on(m, 0x40000);
                    if (ri(0, 3) == 0) {  // its flag byte of the level (+0xb0 its index): set (gone) or not
                        const uint32_t idx = u(ri(0, 15));
                        w8(m + 0xb0, idx);
                        w8(0x0014c050 + idx + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
                    }
                    camera_near(m + 0x10);
                    finish();
                    return Args{{m}, {}};
                }, upd(gas_class276_update_l9));
                p91("p91_2edb60", 0x002edb60, 40, Ret::Int32, [&] {
                    level();
                    const uint32_t m = pick(b276);
                    static const uint32_t cls[] = {0x512, 0x513, 0x514};
                    finish();
                    return Args{{m, cls[ri(0, 2)]}, {}};
                }, [](const Args& a) { return r32(gas_2edb60_l9(a.i[0], a.i[1])); });
                p91("p91_2ef868", 0x002ef868, 60, Ret::Int32, [&] {
                    level();
                    const uint32_t m = pick(b276);
                    near(X, m + 0x10, 2.0f, 2.0f);
                    wfrand(X + 0x10, 4, -1, 1);
                    finish();
                    return Args{{m, X, X + 0x10, u(ri(0, 1))}, {rf(0.5f, 2.5f)}};
                }, [](const Args& a) { return r32(gas_chunk_l9(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0])); });
            }
            p91("p91_class279", 0x002ee110, 80, Ret::None, [&] {
                level();
                const uint32_t m = spawn_at_hero(0x117);
                if (m != 0) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    wf(pv, coin() ? rf(-1, 3) : rf(3, 12));
                    wf(m + 0x48, rf(-3.1f, 3.1f));
                    hit_on(m, 0x10000);
                }
                finish();
                return Args{{m}, {}};
            }, upd(gas_class279_update_l9));
            p91("p91_2ee380", 0x002ee380, 40, Ret::Int32, [&] {
                level();
                const uint32_t m = pick(mobys_with(0x002c26c8));
                near(X, m + 0x10, 2.0f, 2.0f);
                finish();
                return Args{{m, X}, {rf(-3.1f, 3.1f)}};
            }, [](const Args& a) { return r32(gas_2ee380_l9(a.i[0], a.i[1], a.f[0])); });
            struct Fl { const char* name; uint32_t addr; uint32_t cls; void (*fn)(uint32_t); };
            static const Fl fls[] = {{"p91_class313", 0x002ef430, 0x139, gas_class313_update_l9}, {"p91_class320", 0x002ef9b0, 0x140, gas_class320_update_l9},
                                     {"p91_class321", 0x002ef9b0, 0x141, gas_class320_update_l9}};
            for (const Fl& fl : fls) {
                p91(fl.name, fl.addr, 120, Ret::None, [&, fl] {
                    level();
                    const uint32_t m = spawn_at_hero(fl.cls);
                    if (m != 0) {
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        wfrand(pv, 3, -0.5f, 0.5f);
                        wf(pv + 8, rf(-1.0f, 0.3f));
                        wfrand(pv + 0x10, 2, -0.3f, 0.3f);
                        w32(pv + 0x1c, ri(0, 3) == 0 ? u(ri(0, 2)) : 100u);
                        const int k = ri(0, 5);
                        if (k == 0) { wf(m + 0x10, rf(400, 600)); wf(m + 0x14, rf(0, 600)); wf(m + 0x18, rf(25.0f, 26.0f)); wf(pv + 8, -1.0f); }  // over the lava
                        else if (k == 1) wf(m + 0x10 + 4 * u(ri(0, 2)), coin() ? rf(-0.5f, 2.5f) : rf(1020.5f, 1022));
                        else if (k == 2) {  // slow, just over the ground: a bounce that stops it
                            const bool tiny = coin();
                            wfrand(pv, 2, tiny ? -0.001f : -0.01f, tiny ? 0.001f : 0.01f);
                            wf(pv + 8, tiny ? rf(-0.003f, -0.0005f) : rf(-0.05f, -0.01f));
                            float ground = ee_read<float>(P + 0x88);
                            if (tiny) { const float g = floor_probe_v(1.0f, ee_ref<Vec4>(m + 0x10), 0); if (g != 0.0f) ground = g; }  // (the world's own floor there)
                            wf(m + 0x18, ground + (tiny ? rf(0.0f, 0.002f) : rf(0.0f, 0.03f)));
                        }
                        if (k == 3) {  // the camera about 64 away (x, y): the reach's edge
                            const float a = rf(-3.1f, 3.1f), r = rf(62.5f, 65.5f);
                            Vec4 q = ee_ref<Vec4>(m + 0x10);
                            q.x += r * std::cos(a); q.y += r * std::sin(a);
                            ee_ref<Vec4>(CAM) = q;
                        } else {
                            camera_near(m + 0x10);
                        }
                    }
                    finish();
                    return Args{{m}, {}};
                }, upd(fl.fn));
            }
        }

        // class 664 (the sinking platform): set up, the hero on it (in the air or not), its timer, about its quarter's height
        {
            const std::vector<uint32_t> plats = mobys_with(0x002f86a0);
            if (!plats.empty()) {
                p91("p91_class664", 0x002f86a0, 120, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(plats);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (ri(0, 5) == 0) w32(pv + 0x80, 0);
                    const int k = ri(0, 3);
                    if (k == 1 || k == 2) {
                        w32(P + 0x2fc, m);
                        w16(P + 0x30e, k == 1 ? 0u : 1u);
                        near(P + 0x80, m + 0x10, 2.0f, 1.0f);
                    }
                    w32(pv + 0x88, coin() ? 0u : u(ri(1, 40)));
                    if (coin()) wf(m + 0x18, ee_read<float>(pv + 0x84) - 0.25f + rf(-0.1f, 0.1f));
                    if (k == 3) {  // rising back, just below a quarter under its rest
                        w32(pv + 0x80, 1);
                        w32(pv + 0x88, 0);
                        if (ee_read<uint32_t>(P + 0x2fc) == m) w32(P + 0x2fc, 0);
                        wf(m + 0x18, ee_read<float>(pv + 0x84) - 0.25f - rf(0.0f, 0.002f));
                    }
                    wfrand(pv + 0x60, 2, -0.2f, 0.2f);
                    wfrand(pv + 0x70, 2, -0.05f, 0.05f);
                    finish();
                    return Args{{m}, {}};
                }, upd(gas_class664_update_l9));
            }
        }

        // ---- P9.2: the hand ports of part 2 (gaspar_controller / _lift / _fuses / _turret / _hazard / _ferry / _lava / _carry.cpp,
        // classes 1285 .. 1288 in _breakables, 1293 in _platform): each update on states made for its branches, each helper called
        // directly, the lava's draw with its packets on a buffer past the test area (0x1e20000)
        {
            const uint32_t T0 = L9_ADDR(0x0015ffd8);
            auto index_of = [T0](uint32_t o) { return u(static_cast<int32_t>(o - ee_read<uint32_t>(T0)) >> 8); };
            auto mobys_cls = [](int16_t cls) {
                std::vector<uint32_t> v;
                for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                    if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == cls) v.push_back(m);
                return v;
            };
            auto frame_slot = [&keep_frame](uint32_t m) {  // the frame counter on (or off) the moby's slot of eight
                const uint32_t f = (R() & 0xfff8u) | (coin() ? (m >> 8) & 7 : R() & 7);
                w32(L9_ADDR(0x0015f5cc), f);
                keep_frame = true;
            };
            auto sound_slot = [](uint32_t m, uint32_t at) {  // a sound slot: none, a playing one of the moby's, another's, idle
                const int k = ri(0, 3);
                if (k == 0) { w32(at, 0xffffffffu); return; }
                const uint32_t s = u(ri(0, 15)), e = 0x0013e550 + s * 0x70;
                w32(at, s);
                w32(e + 0x88, k == 3 ? R() : m);
                w8(e + 0x74, k == 2 ? 0u : 1u);
            };
            auto hero_in_box = [P](int32_t box) {  // the hero at the box's centre (when it has one)
                if (box < 0) return;
                Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, box) + 0x30);
                c.x += rf(-0.2f, 0.2f); c.y += rf(-0.2f, 0.2f);
                ee_ref<Vec4>(P + 0x80) = c;
            };
            auto view_on = [](uint32_t at) {  // the view's camera put behind `at` along its forward axis (the point on screen)
                const Vec4 r0 = ee_ref<Vec4>(OV(0x0016ccc0)), r1 = ee_ref<Vec4>(OV(0x0016ccd0)), r2 = ee_ref<Vec4>(OV(0x0016cce0));
                Vec4& cam = ee_ref<Vec4>(OV(0x0016ccf0));
                const Vec4 c = ee_ref<Vec4>(at);
                const float k = coin() ? rf(5, 60) : -rf(5, 60);  // (either way along the axis: the one in front)
                cam.x = c.x * cam.w - r0.z * k; cam.y = c.y * cam.w - r1.z * k; cam.z = c.z * cam.w - r2.z * k;
            };
            level();
            const std::vector<uint32_t> parts1172 = mobys_cls(1172), parts1181 = mobys_cls(1181), bases = mobys_cls(1184);
            std::vector<uint32_t> parts = parts1172;
            parts.insert(parts.end(), parts1181.begin(), parts1181.end());
            parts.insert(parts.end(), bases.begin(), bases.end());
            auto any_link = [&] {  // a link to a part (its index), none, or a moby being removed
                const int k = ri(0, 4);
                if (k == 0 || parts.empty()) return 0xffffffffu;
                return index_of(pick(parts));
            };
            if (std::getenv("B121_INFO") != nullptr) std::printf("INFO p92 parts 1172 %zu 1181 %zu bases %zu\n", parts1172.size(), parts1181.size(), bases.size());

            // class 1000 (the controller)
            {
                const std::vector<uint32_t> ms = mobys_with(0x00300888);
                if (!ms.empty()) {
                    p91("p92_class1000", 0x00300888, 120, Ret::None, [&] {
                        level();
                        const uint32_t m = pick(ms);
                        w32(L9_ADDR(0x0015f5cc), ri(0, 3) == 0 ? 0u : (coin() ? u(ri(1, 9)) : R() & 0x7fffffffu));
                        keep_frame = true;
                        w32(L9_ADDR(0x001613c8), coin() ? 0u : u(ri(1, 80)));
                        w8(0x0013d416, ri(0, 2) == 0 ? 1u : 0u);
                        w16(0x00141c40, ri(0, 3) == 0 ? u(ri(1, 9)) : 0u);
                        w8(0x0013d4cb, coin() ? 1u : 0u);
                        w32(0x0015eea4, R() & 0xfffff);
                        w16(0x00141c42, coin() ? R() & 0xffff : u(ri(0, 3)));
                        if (coin()) {  // (the dialogue system idle: its line can start)
                            w32(OV(0x00179410), 0);
                            w32(OV(0x00179410) + 0x24, 0xffffffffu);
                            w32(0x001516d0 + 0x50, 0);
                            w32(0x001516d0 + 0x1c, 0xffffffffu);
                        }
                        if (coin()) near(P + 0x80, m + 0x10, coin() ? 3.0f : 60.0f, 5.0f);
                        else if (coin()) hero_in_box(ee_read<int32_t>(ee_read<uint32_t>(m + 0x78)));
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1000_update_l9));
                }
            }

            // class 1150 (the lifts): off / on (its box, its bit), set up, at either end with the hero near either end, on it or off
            // it, riding (the hero under it, arriving), the hero carried
            {
                const std::vector<uint32_t> ms = mobys_with(0x003033a0);
                if (!ms.empty()) {
                    p91("p92_class1150", 0x003033a0, 200, Ret::None, [&] {
                        level();
                        const uint32_t m = pick(ms);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, ri(0, 4) == 0 ? 0u : 1u);
                        w8(pv + 0xa0, coin() ? 1u : 0u);
                        if (ri(0, 2) == 0) w32(pv + 0xc0, 0xffffffffu);
                        if (ri(0, 4) == 0) {
                            w8(L9_ADDR(0x001bb3b0) + u(ee_read<int16_t>(m + 0xb2)) + 0x454, coin() ? 1u : 0u);
                            wf(P + 0x88, ee_read<float>(P + 0x88) + 500.0f);  // (the hero far from its box)
                        }
                        if (coin()) {  // its bit of the level
                            const int32_t idx = ee_read<int16_t>(m + 0xb2);
                            const uint32_t w = 0x0014c190 + (u(idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
                            w32(w, coin() ? 0xffffffffu : 0u);
                        }
                        const int st = ri(0, 3);
                        w8(m + 0xbc, u(st));
                        const int32_t pi = ee_read<int32_t>(pv + 0xb4);
                        if (pi >= 0) {
                            const uint32_t path = lvd::path(pi);
                            const uint32_t n = ee_read<uint32_t>(path);
                            const uint32_t end = coin() ? path + 0x10 : path + (n << 4);
                            near(P + 0x80, end, 3.0f, 1.0f);
                            if (st == 2) {
                                wf(pv + 0xa4, coin() ? rf(-0.1f, 1.1f) : (coin() ? rf(-0.02f, 0.02f) : rf(0.98f, 1.02f)));
                                wf(pv + 0xa8, rf(-0.05f, 0.05f));
                                wf(pv + 0xac, coin() ? rf(-0.02f, 0.02f) : rf(-0.2f, 0.2f));
                                wf(pv + 0xbc, rf(-0.2f, 0.1f));
                                if (coin()) { near(P + 0x80, m + 0x10, 1.5f, 0.0f); wf(P + 0x88, ee_read<float>(m + 0x18) - rf(0.5f, 4.5f)); }
                            }
                        }
                        w16(pv + 0xb8, coin() ? 1u : 0u);
                        w16(pv + 0xba, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                        w32(pv + 0xc8, coin() ? 1u : 0u);
                        sound_slot(m, pv + 0xc4);
                        if (coin()) { w32(P + 0x2fc, m); w16(P + 0x30e, coin() ? 0u : 1u); }
                        if (ri(0, 4) == 0) hero_in_box(ee_read<int32_t>(pv + 0xc0));
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1150_update_l9));
                }
            }

            // classes 1172 / 1181 (the charges) and their helpers
            auto charge_links = [&](uint32_t pv, std::initializer_list<uint32_t> at) {
                for (uint32_t a : at) w32(pv + a, any_link());
                if (ri(0, 3) == 0) {  // a linked part being removed or gone
                    const uint32_t o = ee_read<uint32_t>(T0) + (ee_read<uint32_t>(pv + *at.begin()) << 8);
                    if (ee_read<int32_t>(pv + *at.begin()) != -1) w8(o + 0x20, coin() ? 0xfeu : 0xfdu);
                }
            };
            if (!parts1172.empty()) {
                for (int st = 0; st < 4; ++st) {
                    char nm[32];
                    std::snprintf(nm, sizeof nm, "p92_class1172_s%d", st);
                    p91(nm, 0x00303d10, 100, Ret::None, [&, st] {
                        level();
                        const uint32_t m = pick(parts1172);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        hit_on(m, coin() ? 0x330000u : 0x10000u);
                        if (ee_read<uint8_t>(m + 0xa4) != 0xff) wf(OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6) + 0x2c, rf(0, 12));
                        wf(pv + 0x20, coin() ? rf(0, 3) : rf(3, 20));
                        if (st == 0) {
                            w16(pv + 0x76, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 15)));
                            if (ee_read<int16_t>(pv + 0x76) != -1) w8(0x0013d388 + ee_read<uint16_t>(pv + 0x76) + 0x39, coin() ? 1u : 0u);
                        }
                        frame_slot(m);
                        sound_slot(m, pv + 0x7c);
                        w8(m + 0xbc, coin() ? 1u : 0u);
                        charge_links(pv, {0x70, 0x78});
                        if (coin()) w8(ee_read<uint32_t>(T0) + (ee_read<uint32_t>(pv + 0x78) << 8) + 0x20, 1);
                        w16(pv + 0x74, u(ri(0, 3)));
                        camera_near(m + 0x10);
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1172_update_l9));
                }
                p91("p92_303b30", 0x00303b30, 80, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(parts1172);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, ri(0, 4) == 0 ? 3u : 1u);
                    hit_on(m, coin() ? 0x330000u : 0x10000u);
                    if (ee_read<uint8_t>(m + 0xa4) != 0xff) wf(OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6) + 0x2c, coin() ? rf(0, 12) : rf(-5, 0));
                    wf(pv + 0x20, rf(0, 10));
                    charge_links(pv, {0x70});
                    finish();
                    return Args{{m, pv, pv + 0x20}, {}};
                }, [](const Args& a) { gas_303b30_l9(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
            }
            if (!parts1181.empty()) {
                for (int st = 0; st < 4; ++st) {
                    char nm[32];
                    std::snprintf(nm, sizeof nm, "p92_class1181_s%d", st);
                    p91(nm, 0x00304360, st == 1 ? 200 : 100, Ret::None, [&, st] {
                        level();
                        const uint32_t m = pick(parts1181);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        if (st == 0) {
                            w8(pv + 0x69, ri(0, 2) == 0 ? 0xffu : u(ri(0, 15)));
                            if (ee_read<int8_t>(pv + 0x69) != -1) w8(0x0013d388 + ee_read<uint8_t>(pv + 0x69) + 0x39, coin() ? 1u : 0u);
                            if (coin()) w32(pv + 0x60, 0xffffffffu);
                            if (coin()) w32(pv + 0x64, 0xffffffffu);
                        } else {
                            if (ri(0, 3) == 0) charge_links(pv, {0x60, 0x64});
                            if (ri(0, 3) == 0) charge_links(pv, {0x64, 0x60});
                            w32(pv + 0x6c, any_link());
                            if (ri(0, 5) == 0) w32(pv + 0x60, index_of(pick(mobys_cls(1201))));  // (a link to a moby of another class)
                            if (coin() && ee_read<int32_t>(pv + 0x6c) != -1) w8(ee_read<uint32_t>(T0) + (ee_read<uint32_t>(pv + 0x6c) << 8) + 0x20, 1);
                            w8(pv + 0x68, coin() ? 0u : 1u);
                            const int32_t nx = ee_read<int32_t>(pv + 0x64);
                            if (nx != -1 && coin()) w8(ee_read<uint32_t>(ee_read<uint32_t>(T0) + (u(nx) << 8) + 0x78) + 0x68, coin() ? 0u : 1u);
                            w8(m + 0x31, coin() ? 1u : 0u);
                            frame_slot(m);
                            w16(m + 0xb2, R() & 0xffff);
                            w8(m + 0xbc, ri(0, 3) == 0 ? 1u : 0u);
                            w16(pv + 0x6a, u(ri(0, 12)));
                        }
                        camera_near(m + 0x10);
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1181_update_l9));
                }
                p91("p92_3041d0", 0x003041d0, 80, Ret::Int32, [&] {
                    level();
                    const uint32_t m = pick(parts1181);
                    const uint32_t o = pick(parts);
                    near(X, o + 0x10, coin() ? 0.4f : (coin() ? 8.0f : 40.0f), 0.3f);
                    if (ri(0, 3) == 0) { ee_ref<Vec4>(X) = ee_ref<Vec4>(o + 0x10); wf(X, ee_read<float>(X) + rf(0.45f, 0.65f)); }  // (about 0.5 off)
                    if (coin()) w8(pick(parts) + 0x20, coin() ? 0xfeu : 0xfdu);
                    finish();
                    return Args{{coin() ? m : o, X, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 1))}, {}};
                }, [](const Args& a) { return r32(gas_3041d0_l9(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });
            }

            // class 1201 (the turrets): set up, the mounting (the hero near the seat, busy or not, its pause), off it, idle (the hero
            // near: sinking, turning, its light blinking), manned (the stick, the pitch's limits, firing by turns, the camera)
            {
                const std::vector<uint32_t> ms = mobys_with(0x00304c80);
                if (!ms.empty()) {
                    for (int st = 0; st < 3; ++st) {
                        char nm[32];
                        std::snprintf(nm, sizeof nm, "p92_class1201_%d", st);
                        p91(nm, 0x00304c80, st == 2 ? 240 : 160, Ret::None, [&, st] {
                            level();
                            const uint32_t m = pick(ms);
                            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                            static const uint32_t states[] = {0, 2, 4, 4, 5, 5};
                            w8(m + 0x20, st == 0 ? states[ri(0, 5)] : (st == 1 ? 4u : 5u));
                            w8(m + 0xbc, u(ri(0, 2)));
                            w16(pv + 0x42, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                            static const uint32_t hs[] = {0, 0x14, 7, 0x32, 1};
                            w32(P + 0x208c, hs[ri(0, 4)]);
                            w32(P + 0x2084, ri(0, 4) == 0 ? 0x3du : u(ri(0, 0x40)));
                            if (coin()) w32(P + 0x2fc, m);
                            const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                            Vec4 seat{};
                            moby_chain_point_v(m, 3, seat);
                            if (hm != 0) {
                                Vec4 q = seat;
                                const float r = coin() ? rf(0, 1.1f) : rf(1.1f, 3);
                                q.x += r; q.z += 0.4f;
                                ee_ref<Vec4>(hm + 0x10) = q;
                            }
                            near(P + 0x80, m + 0x10, coin() ? 3.0f : 8.0f, 1.0f);
                            w32(0x0013cae0, R() & 0x30u);
                            w32(0x0013cae4, R() & ee_read<uint32_t>(0x0013cae0));
                            static const uint32_t anims[] = {0, 3, 4, 5};  // (its animations: idle 5, the barrels 3 / 4)
                            if (coin()) { w8(m + 0x52, anims[ri(1, 3)]); w8(m + 0x53, anims[ri(0, 3)]); }
                            w8(m + 0x70, R() & 0xff);
                            wf(0x00141070, coin() ? rf(-1.5f, 1.5f) : 0.0f);
                            wf(0x00141074, coin() ? rf(-1.5f, 1.5f) : 0.0f);
                            w32(0x0015eddc, coin() ? 1u : 0u);
                            wf(pv + 0x44, rf(-0.6f, 0.6f));
                            wf(pv + 0x60, rf(-0.1f, 0.1f));
                            wf(pv + 0x64, rf(-0.1f, 0.1f));
                            w16(pv + 0x52, u(ri(0, 2)));
                            w16(pv + 0x54, coin() ? 1u : 0u);
                            w32(pv + 0x68, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 7)));
                            w32(pv + 0x6c, u(ri(0, 45)));
                            if (coin()) wf(m + 0x18, ee_read<float>(pv + 0x5c) - rf(0.0f, 2.0f));
                            if (ee_read<uint8_t>(m + 0x52) != ee_read<uint8_t>(m + 0x53) && ee_read<uint8_t>(m + 0x52) != 0xff && coin()) {  // (the animation's time about 1)
                                const uint32_t cur = ee_read<uint32_t>(m + 0x68);
                                wf(m + 0x54, 1.0f - static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f + rf(-0.3f, 0.3f));
                            }
                            finish();
                            return Args{{m}, {}};
                        }, upd(gas_class1201_update_l9));
                    }
                }
            }

            // class 1206 (the lines of heat)
            {
                const std::vector<uint32_t> ms = mobys_with(0x00305a28);
                if (!ms.empty()) {
                    p91("p92_class1206", 0x00305a28, 80, Ret::None, [&] {
                        level();
                        const uint32_t m = pick(ms);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, coin() ? 0u : 1u);
                        frame_slot(m);
                        sound_slot(m, pv + 0x14);
                        if (coin()) near(P + 0x80, m + 0x10, 3.0f, 2.0f);
                        const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                        if (hm != 0 && coin()) {  // (the hero's moby on its line)
                            Vec4 d{0.0f, 0.0f, rf(0, 1) * ee_read<float>(pv + 4), 0.0f};
                            Vec4 w{};
                            mat3_transform(w, d, ee_ref<Mat4>(m + 0xc0));
                            Vec4 q = ee_ref<Vec4>(m + 0x10);
                            q.x += w.x; q.y += w.y; q.z += w.z;
                            ee_ref<Vec4>(hm + 0x10) = q;
                            ee_ref<Vec4>(P + 0x80) = q;
                        }
                        if (ri(0, 4) == 0) w32(pv + 8, u(ri(0, 6)));
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1206_update_l9));
                }
            }

            // classes 1285 .. 1288 (the breakables): their builds, the flag byte, safe / hit, the frame's load; the next build
            {
                const std::vector<uint32_t> ms = mobys_with(0x003082d8);
                if (!ms.empty()) {
                    p91("p92_class1285", 0x003082d8, 120, Ret::None, [&] {
                        level();
                        const uint32_t m = pick(ms);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w16(m + 0xa6, u(ri(0x505, 0x508)));
                        w8(m + 0x20, coin() ? 0u : 1u);
                        w32(pv, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 15)));
                        w8(m + 0xbc, u(ri(0, 2)));
                        hit_on(m, 0x40000);
                        if (ri(0, 3) == 0) {
                            const uint32_t idx = u(ri(0, 15));
                            w8(m + 0xb0, idx);
                            w8(0x0014c050 + idx + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
                        }
                        wf(L9_ADDR(0x0015f5d0), coin() ? rf(0, 0.8f) : rf(0.8f, 1.2f));
                        wf(L9_ADDR(0x0015f5d4), coin() ? rf(0, 0.8f) : rf(0.8f, 1.2f));
                        camera_near(m + 0x10);
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1285_update_l9));
                    p91("p92_308220", 0x00308220, 40, Ret::Int32, [&] {
                        level();
                        finish();
                        return Args{{pick(ms), u(ri(0x506, 0x508))}, {}};
                    }, [](const Args& a) { return r32(gas_308220_l9(a.i[0], a.i[1])); });
                }
            }

            // class 1293 (the platforms that rise once their moby is gone): set up (with or without a moby), waiting, floating
            {
                const std::vector<uint32_t> ms = mobys_with(0x003091b0);
                if (!ms.empty()) {
                    p91("p92_class1293", 0x003091b0, 200, Ret::None, [&] {
                        level();
                        const uint32_t m = pick(ms);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        const int k = ri(0, 5);
                        w8(m + 0x20, k == 0 ? 0u : (k == 1 ? 1u : 2u));
                        if (k <= 1) {
                            w32(pv + 0x8c, ri(0, 2) == 0 ? 0xffffffffu : index_of(pick(parts)));
                            if (k == 1 && coin()) w8(ee_read<uint32_t>(T0) + (ee_read<uint32_t>(pv + 0x8c) << 8) + 0x20, coin() ? 0xfeu : 0xfdu);
                        } else {
                            const int h = ri(0, 3);
                            if (h == 1 || h == 2) {
                                w32(P + 0x2fc, m);
                                w16(P + 0x30e, h == 1 ? 0u : 1u);
                                near(P + 0x80, m + 0x10, 2.0f, 1.0f);
                            }
                            w32(pv + 0x88, coin() ? 0u : u(ri(1, 40)));
                            const float rest = ee_read<float>(pv + 0x84);
                            const int z = ri(0, 3);
                            if (z == 0) wf(m + 0x18, rest - 0.3f + rf(-0.05f, 0.05f));
                            else if (z == 1) wf(m + 0x18, rest - rf(2.0f, 5.0f));
                            else if (z == 2) { wf(m + 0x18, rest - 0.3f - rf(0.0f, 0.005f)); w32(pv + 0x88, 0); if (ee_read<uint32_t>(P + 0x2fc) == m) w32(P + 0x2fc, 0); }
                            wfrand(pv + 0x60, 2, -0.2f, 0.2f);
                            wfrand(pv + 0x70, 2, -0.05f, 0.05f);
                        }
                        finish();
                        return Args{{m}, {}};
                    }, upd(gas_class1293_update_l9));
                }
            }

            // class 1766 (the raft) and its ride
            {
                const std::vector<uint32_t> ms = mobys_with(0x00309c08);
                if (!ms.empty()) {
                    auto raft = [&](uint32_t m, int st) {
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        const uint32_t path = lvd::path(ee_read<int32_t>(pv + 0xac));
                        const int32_t n = ee_read<int32_t>(path);
                        if (st == 0) {
                            w32(pv + 0xa0, coin() ? 1u : 0u);
                            w8(m + 0xb0, u(ri(0, 15)));
                            w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), coin() ? 0xffu : 0u);
                            return;
                        }
                        const int way = ri(0, 4) == 0 ? 0 : (coin() ? 1 : -1);
                        w16(pv + 0xb8, u(way));
                        const int k = ri(0, 4);
                        const int32_t at = k == 0 ? (coin() ? 0 : n - 1) : (k == 1 ? (coin() ? n - 2 : 1) : ri(0, n - 1));
                        if (ri(0, 4) == 0) wf(path + 0x1c + u(at) * 0x10, 42.0f);  // (a point that ends its turning)
                        w16(pv + 0xba, u(at));
                        const uint32_t pt = path + 0x10 + u(at) * 0x10;
                        wf(pv + 0xa8, coin() ? rf(0, 0.3f) : rf(0.3f, 2));
                        near(m + 0x10, pt, coin() ? 0.5f : 6.0f, 0.5f);
                        if (coin()) {  // (just short of the point it heads for, lifted by its speed times 7)
                            near(m + 0x10, pt, 0.2f, 0.0f);
                            wf(m + 0x18, ee_read<float>(pt + 8) + ee_read<float>(pv + 0xa8) * 7.0f + rf(-0.2f, 0.2f));
                        }
                        wf(pv + 0xbc, coin() ? 0.0f : 3.14159274f);
                        wf(pv + 0xcc, rf(0, 1));
                        w32(pv + 0xc8, coin() ? 1u : 0u);
                        if (ri(0, 3) == 0) w32(pv + 0xb0, 0xffffffffu);
                        if (ri(0, 3) == 0) w32(pv + 0xb4, 0xffffffffu);
                        if (coin()) { w32(P + 0x2fc, m); w16(P + 0x30e, coin() ? 0u : 1u); }
                        if (coin()) near(P + 0x80, coin() ? path + 0x10 : path + u(n) * 0x10, 3.0f, 1.0f);
                        else if (coin()) hero_in_box(ee_read<int32_t>(pv + (coin() ? 0xb0 : 0xb4)));
                        frame_slot(m);
                        sound_slot(m, pv + 0xc4);
                    };
                    for (int st = 0; st < 4; ++st) {
                        char nm[32];
                        std::snprintf(nm, sizeof nm, "p92_class1766_s%d", st);
                        p91(nm, 0x00309c08, st == 3 ? 240 : 100, Ret::None, [&, st] {
                            level();
                            const uint32_t m = pick(ms);
                            raft(m, st);
                            finish();
                            return Args{{m}, {}};
                        }, upd(gas_class1766_update_l9));
                    }
                    p91("p92_30a298", 0x0030a298, 120, Ret::Int32, [&] {
                        level();
                        const uint32_t m = pick(ms);
                        raft(m, 3);
                        finish();
                        return Args{{m}, {}};
                    }, [](const Args& a) { return r32(static_cast<uint32_t>(gas_30a298_l9(a.i[0]))); });
                    p91("p92_30a238", 0x0030a238, 60, Ret::None, [&] {
                        level();
                        finish();
                        return Args{{X}, {rf(-3.2f, 3.2f), rf(-3.2f, 3.2f), rf(0, 1)}};
                    }, [](const Args& a) { gas_30a238_l9(a.i[0], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
                }
            }

            // the lava's draw (class 317's callback) and its parts, the packets on a buffer past the test area
            {
                constexpr uint32_t PKT = 0x01e20000u;
                auto streams_on_screen = [&] {  // the view moved until one of the streams is on screen (point_on_screen, as the draw tests)
                    for (int t = 0; t < 40; ++t) {
                        const uint32_t rec = L9_ADDR(0x001f3080) + u(ri(0, 0x6b)) * 0x40;
                        view_on(rec);
                        if (point_on_screen(rec, 256.0f) != -1) return;
                    }
                };
                auto draw_prep = [&] {
                    w32(L9_ADDR(0x001611c0), PKT + 0x10 * u(ri(0, 0x40)));
                    w32(L9_ADDR(0x0015f5cc), R() & 0xffff);
                    keep_frame = true;
                    w32(L9_ADDR(0x0015f458), coin() ? 1u : 0u);
                    w32(L9_ADDR(0x0015f5e0), coin() ? 7u : 0u);
                    wf(L9_ADDR(0x0015f308), coin() ? 0.0f : rf(-1, 1));  // (the streams' z offset)
                    near(CAM, P + 0x80, coin() ? 20.0f : 200.0f, 10.0f);
                };
                p91("p92_class317_draw", 0x002ef750, 30, Ret::None, [&] {
                    level();
                    draw_prep();
                    if (coin()) streams_on_screen();
                    finish();
                    return Args{{0}, {}};
                }, [](const Args& a) { gas_class317_draw_l9(a.i[0]); return uint64_t{0}; });
                p91("p92_21e8c0", 0x0021e8c0, 60, Ret::None, [&] {
                    level();
                    draw_prep();
                    if (ri(0, 3) != 0) streams_on_screen();
                    const int32_t a = ri(0x2c, 0x3b), b = coin() ? a : ri(0x2c, 0x3b);
                    finish();
                    return Args{{L9_ADDR(0x001f3080), u(ri(0, 3) == 0 ? ri(-1, 4) : 0x6c), u(a), u(b)}, {rf(0, 1)}};
                }, [](const Args& a) { gas_21e8c0_l9(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]), a.f[0]); return uint64_t{0}; });
                p91("p92_21e560", 0x0021e560, 40, Ret::None, [&] {
                    level();
                    draw_prep();
                    const int32_t a = ri(0x2c, 0x3b), b = coin() ? a : ri(0x2c, 0x3b);
                    finish();
                    return Args{{u(a), u(b)}, {rf(0, 1)}};
                }, [](const Args& a) { gas_21e560_l9(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), a.f[0]); return uint64_t{0}; });
                p91("p92_2c1978", 0x002c1978, 40, Ret::None, [&] {
                    level();
                    draw_prep();
                    w8(L9_ADDR(0x001fc740) + 0x3f, R() & 0xff);
                    finish();
                    return Args{{L9_ADDR(0x001fc740), L9_ADDR(0x001f63c0), u(ri(0, 3) == 0 ? ri(-1, 3) : 0xa)}, {}};
                }, [](const Args& a) { gas_2c1978_l9(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
                p91("p92_21baf8", 0x0021baf8, 30, Ret::None, [&] {
                    level();
                    draw_prep();
                    finish();
                    return Args{{}, {}};
                }, [](const Args&) { gas_21baf8_l9(); return uint64_t{0}; });
                p91("p92_21f6f8", 0x0021f6f8, 30, Ret::None, [&] {
                    level();
                    finish();
                    return Args{{X}, {coin() ? 1024.0f : rf(-100, 100)}};
                }, [](const Args& a) { gas_mat_scale_l9(a.i[0], a.f[0]); return uint64_t{0}; });
            }
        }
        g_extra_noop_hooks.clear();
        g_ignored_words = loop_ignored;
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
