// Oracle batch 117 (task 0350): level 6's (Blarg's Nebula G34) own moby classes on its real level data. Run with
// ORACLE_LEVEL=6 build/tests/ovl_oracle --batch 117 (needs the ISO). The subjects are the level's own mobys of
// each class (placed by its gameplay data) after the first gameplay frame, perturbed; the original is level 6's
// own function at its own address, the port the one registered for it (the hand ports that replaced
// blarg_classes_tr_N.cpp).

#include "ovl_oracle.h"

#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_breakables.h>
#include <game/ovl/shared_class1021.h>
#include <game/ovl/shared_class1038.h>
#include <game/ovl/shared_class1068.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class857.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_lights.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_zones.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/blarg_part2.h>
#include <game/lib/libcdvd.h>
#include <runtime/vu/vu0.h>
#include <game/enhance/advanced.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <optional>
#include <algorithm>
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
    static const char* const kNames[] = {"class1048", "class1048_hunt", "class1051_fight", "class1068", "class827",
                                         "class857", "arc_hit_test_l6", "class1016", "class1028_scene", "class1035",
                                         "class1035_near", "class1061_lift", "class1083", "class1083_tank",
                                         "class1105_scientist", "class1108_countdown", "class1109_choice",
                                         "class1109_ship", "class1118_wheel", "class1123", "class1123_arc", "class1818",
                                         "class805", "class911", "class911_jet"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// a hand-written class hook (moby, position, target, float) in the hook table's uniform form: the levels' copies of
// Veldin's hooks and the levels' hooks written by hand run these (T00)
template <int32_t (*H)(uint32_t, uint32_t, uint32_t, float)>
TrRet hook_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                   float, float, float, float) {
    return TrRet{static_cast<uint32_t>(H(a0, a1, a2, f12)), 0.0f};
}
// class 827's carry hook +8 (L6:0x2ea738): the shared port with its `which`
int32_t class827_hook_carry8(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return class827_hook_carry(m, a1, a2, f12, 8); }


// A check whose original is level 6's own function at `l6addr` (not a Veldin address to translate).
void l6check(const char* name, uint32_t l6addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B117_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 6) {
        std::printf("%-26s L6:0x%08x  skipped (level 6 only: run with ORACLE_LEVEL=6)\n", name, l6addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L6:0x%08x\n", name, l6addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l6addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_117() {
    if (g_only_batch != 117) return;
    g_batch = 117;
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
    // the level's mobys whose update function is `fn` (level 6's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 6) { l6check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L6:0x%08x  no moby on this level\n", name, fn); return; }
        l6check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    if (g_oracle_level == 6) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (once transliterated by tools/codegen/generate_all.py; hand ports now): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class55", 0x002b4770, blg_class55_update_l6, 0x0, false},
        {"class87", 0x002b9678, empty_update, 0x0, false},
        {"class156", 0x002bead0, class156_update, 0x0, false},
        {"class302", 0x002d85a0, class302_update, 0x0, false},
        {"class303", 0x002d8c20, class303_update, 0x0, false},
        {"class367", 0x002d9d10, blg_class367_update_l6, 0x0, false},
        {"class604", 0x002e11c8, class604_update, 0x0, false},
        {"class605", 0x002e1518, zone_marker_update, 0x0, false},
        {"class615", 0x002e1940, class615_update, 0x0, false},
        {"class803", 0x002e6270, swingshot_target_update, 0x0, false},
        {"class805", 0x002e66e0, spawn_point_update_l1, 0x0, false},
        {"class827", 0x002e8678, class827_update, 0x2, false},
        {"class832", 0x002eaa40, class832_update, 0x0, false},
        {"class857", 0x002f0040, class857_update, 0x0, false},
        {"class911", 0x002f3ad8, blg_class911_update_l6, 0x0, false},
        {"class1016", 0x002f4638, gadget_pickup_update, 0x28, false},
        {"class1021", 0x002f4f00, class1021_update, 0x0, false},
        {"class1028", 0x002f55a0, blg_class1028_update_l6, 0x0, false},
        {"class1035", 0x002f6470, blg_class1035_update_l6, 0x0, false},
        {"class1038", 0x002f7288, class1038_update, 0x0, false},
        {"class1039", 0x002f7930, deadly_zones_update, 0x0, false},
        {"class1040", 0x002f7ab8, class1040_update, 0x0, false},
        {"class1048", 0x002f7d78, blg_class1048_update_l6, 0x0, false},
        {"class1051", 0x002f9a28, blg_class1051_update_l6, 0x0, false},
        {"class1054", 0x002fbfb0, blg_class1054_update_l6, 0x0, false},
        {"class1061", 0x002fc640, blg_class1061_update_l6, 0x0, false},
        {"class1062", 0x002fd088, blg_class1062_update_l6, 0x0, false},
        {"class1066", 0x002fda30, blg_class1066_update_l6, 0x0, false},
        {"class1068", 0x002fdbd0, class1068_update, 0x0, false},
        {"class1083", 0x002ffcb0, blg_class1083_update_l6, 0x0, false},
        {"class1085", 0x00300b90, beam_shard_update, 0x0, false},
        {"class1091", 0x00300df0, blg_class1091_update_l6, 0x0, false},
        {"class1105", 0x00301070, blg_class1105_update_l6, 0x0, false},
        {"class1108", 0x00301b90, blg_class1108_update_l6, 0x0, false},
        {"class1109", 0x00302578, blg_class1109_update_l6, 0x280, false},
        {"class1118", 0x00304098, blg_class1118_update_l6, 0x10, false},
        {"class1123", 0x003049f8, blg_class1123_update_l6, 0x0, false},
        {"class1134", 0x003053a8, gold_bolt_update, 0x6, false},
        {"class1302", 0x00307d30, class1302_update, 0x0, false},
        {"class1348", 0x003083b0, blg_class1348_update_l6, 0x0, false},
        {"class1504", 0x00308868, flicker_light_update, 0x0, false},
        {"class1512", 0x00308c68, blg_class1512_update_l6, 0x0, false},
        {"class1551", 0x00309348, blg_class1551_update_l6, 0x0, false},
        {"class1633", 0x003094f0, class1633_update, 0x0, false},
        {"class1655", 0x00309860, class1655_update, 0x0, false},
        {"class1660", 0x00309e08, class1660_update, 0x0, false},
        {"class1664", 0x00309fa8, class1664_update, 0x0, false},
        {"class1672", 0x0030a148, empty_update, 0x0, false},
        {"class1818", 0x0030ae88, sandmouse_update, 0x0, false},
        {"class1898", 0x0030be60, flash_update, 0x0, false},
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
    if (g_oracle_level == 6 && std::getenv("B117_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B117_CLS") != nullptr && std::strcmp(std::getenv("B117_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 6 && std::getenv("B117_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B117_ST") == nullptr || std::atoi(std::getenv("B117_ST")) == st) && (std::getenv("B117_PASS") == nullptr || std::atoi(std::getenv("B117_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // (level 6 has no sound emitter types: section 5 of its overlay holds only the terminator)

    // ---- level 6's camera type 0x12 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 6) {
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
        static const CamT cams[] = {{18, 0x00316368, 0x003163d0, 0x003163d8, 0x003163e0}};
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
            l6check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(cam_type18_accept(a.i[0], a.i[1])) : 0u); });
            l6check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            l6check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            l6check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- level 6's camera hook (0x328f80, the pointer its setter 0x328f48 puts at 0x167410) and the callback it queues (0x328fe0)
    if (g_oracle_level == 6) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167780;
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x003147b0u);
            if (ri(0, 3) == 0) w32(OV(0x0015ef4c), u(ri(0, 15)));  // the queue's count (it holds up to 16)
            wf(0x0016256c, rf(0, 0.5f));  // $gp - 0x4694: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));
        };
        l6check("cam0_hook", 0x003147b0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l6check("cam_callback", 0x00314810, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- level 6's class hooks: +4 (0x2ea6a8), +0x10 (0x2ea800: pvars + 0x60; the matcher pairs it with class 749's 0x2d5800), and the dispatchers on them
    if (g_oracle_level == 6) {
        l6check("hook_10", 0x002ea800, 60, Ret::Int32, [level] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t m = ri(0, 4) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
            if (m != 0 && ri(0, 3) == 0) w32(m + 0x78, 0);
            return Args{{m}, {}};
        }, [](A a) { return r32(class_hook_pvars60_l6(a.i[0])); });
    }
    if (g_oracle_level == 6) {
        level();
        std::vector<uint32_t> hm;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 0x10) != 0) hm.push_back(m);
        }
        if (hm.empty()) std::printf("class_hook_10              no moby with a +0x10 hook on this level\n");
        else
            l6check("class_hook_10", ovl_code_lookup(0x002db890), 120, Ret::Int32, [level, hm] {
                level();
                const uint32_t m = hm[static_cast<size_t>(ri(0, static_cast<int>(hm.size()) - 1))];
                if (ri(0, 6) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                if (ri(0, 6) == 0) w32(m + 0x78, 0);
                return Args{{m}, {}};
            }, [](A a) { return r32(class_hook_10(a.i[0])); });
    }


    // ---- level 6's class hook +4 (0x2ea6a8: a0 = the moby, a1 = a place the carried mobys' hooks read) on the mobys whose class owns it, and the dispatcher on it
    if (g_oracle_level == 6) {
        level();
        std::vector<uint32_t> h4;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 4) == 0x002ea6a8u) h4.push_back(m);
        }
        if (h4.empty()) std::printf("hook_4                     no moby owns the +4 hook 0x2ea6a8 on this level\n");
        else {
            l6check("hook_4", 0x002ea6a8, 60, Ret::Int32, [level, h4] {
                level();
                const uint32_t m = h4[static_cast<size_t>(ri(0, static_cast<int>(h4.size()) - 1))];
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                return Args{{m, ri(0, 3) == 0 ? 0u : OV(0x00173e40u)}, {}};
            }, [](A a) { return r32(static_cast<uint32_t>(class827_hook_carry(a.i[0], a.i[1], 0, 0.0f, 4))); });
            l6check("class_hook_4", ovl_code_lookup(0x002da218), 60, Ret::Int32, [level, h4] {
                level();
                const uint32_t m = h4[static_cast<size_t>(ri(0, static_cast<int>(h4.size()) - 1))];
                return Args{{m, ri(0, 3) == 0 ? 0u : OV(0x00173e40u)}, {}};
            }, [](A a) { return r32(static_cast<uint32_t>(class_hook_4(a.i[0], a.i[1]))); });
        }
    }

    // ---- level 6's class hooks +0 / +8 / +0xc (the class' hook table at +0x2c): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 6) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_0_2ea5e0", 0x002ea5e0, 0x0, hook_uniform<class827_hook_carry0>},
            {"hook_8_2ea738", 0x002ea738, 0x8, hook_uniform<class827_hook_carry8>},
            {"hook_c_2ea7c8", 0x002ea7c8, 0xc, hook_uniform<class827_hook_carryc>},
            {"hook_c_2d8118", 0x002d8118, 0xc, hook_uniform<blg_carry_hookc_l6>},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L6:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B117_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l6check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t m = own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
            }, [fn](A a) { return r32(fn(a.i[0], a.i[1], a.i[2], 0, 0, 0, 0, 0, a.f[0], 0, 0, 0, 0, 0, 0, 0).v0); });
        }
        g_extra_noop_hooks.clear();
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 6) {
        static const uint32_t empties[] = {0x002b9678, 0x0030a148, 0x003163d0, 0x003163d8, 0x003163e0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l6check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- stage 4 P6.1: the hand ports of classes 55, 367, 911, 1028, 1035 and 1048 (blarg_doors.cpp, blarg_flame_jet.cpp,
    // blarg_bridge.cpp, blarg_barrier.cpp, blarg_creature.cpp): their states driven through the branches the class loop above
    // seldom reaches (the landing scene's steps, a jet's sound slot, the bridge's scene and fades, the barrier near the camera
    // with the hero crossing it and its switches, the creature's states past 15 and its pack), each against level 6's own update
    if (g_oracle_level == 6 && std::getenv("B117_CLS") == nullptr) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (as the class loop's: the level change)
        const uint32_t P = 0x0013f350;
        // the frame scale, the frame time and its square at their 50 Hz values in a quarter of the cases (the snapshot's are
        // 1.0, 1/60 and its square)
        auto frame50 = [] { if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); } };
        // (P6.2) the batch-wide ignores narrowed for these checks: the base list (the VU0 lanes of the camera rows, the particle
        // update's saved return address, the null page, the scratchpad's spill words) without the segment record 0x174610 ..
        // 0x174618 and the moby table's / particle pool's w lanes; each check adds back only what it needs: `seg` the segment
        // record (segment_collide's hit point, built from stale stack lanes), `mobys` the w lanes debris and spawns leave in
        // the moby table (a VU0 sin / cos the port has not), `pool` the particle pool's velocity w lane
        std::vector<uint32_t> base_ignored;
        for (const uint32_t w : saved_ignored) if (w < 0x00174610u || w > 0x00174618u) base_ignored.push_back(w);
        auto ignores = [base_ignored](bool seg, bool mobys, bool pool) {
            g_ignored_words = base_ignored;
            if (seg) for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(0x00174610u + k);
            if (mobys)
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
            if (pool) {
                const uint32_t pool_at = ee_read<uint32_t>(OV(0x0016016c));
                for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool_at + k * 0x40 + 0x2c);
            }
        };
        // a sound slot word: none, a free slot, or a slot playing the moby's sound (its owner +0x88, playing +0x74)
        auto sound_slot = [](uint32_t m) -> uint32_t {
            const int r = ri(0, 3);
            if (r == 0) return 0xffffffffu;
            const uint32_t s = u(ri(0, 15)), e = 0x0013e550u + s * 0x70;
            if (r >= 2) { w32(e + 0x88, r == 2 ? m : m + 0x100); w8(e + 0x74, coin() ? 1u : 0u); }
            return s;
        };
        // the camera somewhere about the moby (within `r`)
        auto camera_near = [](uint32_t m, float r) {
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            c.x += rf(-r, r); c.y += rf(-r, r); c.z += rf(-2, r * 0.5f);
            ee_ref<Vec4>(OV(0x00166dc0)) = c;
        };
        // the hero at the moby's place, `d` away in a random direction
        auto hero_at = [](uint32_t m, float d, float dz) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f);
            q.x += d * std::cos(a); q.y += d * std::sin(a); q.z += dz;
            ee_ref<Vec4>(0x0013f3d0) = q;
        };

        level();
        ignores(false, false, false);
        // class 55: the landing scene (mode 6 at 0x15f5c4) at its steps 3 / 8 (0x13e050), its doors shut, open or between
        for (int pass = 0; pass < 6; ++pass)
            classcheck("class55_scene", 0x002b4770, blg_class55_update_l6, [pass, generic_prep, frame50](uint32_t m) {
                generic_prep(m, ri(0, 2), pass % 3, blg_class55_update_l6);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w32(L6_ADDR(0x0015f5c4), ri(0, 3) != 0 ? 6u : u(ri(0, 7)));
                { static const uint32_t steps[] = {3, 8, 0, 4, 9}; w32(0x0013e050, steps[ri(0, 4)]); }
                w32(pv + 0x10, ri(0, 2) == 0 ? 1u : 0u);
                const float open = 0.34906584f;
                const int r = ri(0, 3);
                if (r == 0) { wf(pv + 0x14, 0.0f); wf(pv + 0x18, 0.0f); }
                else if (r == 1) { wf(pv + 0x14, open); wf(pv + 0x18, rf(0.0f, open)); }
                else if (r == 2) { wf(pv + 0x14, 0.0f); wf(pv + 0x18, rf(0.0f, open)); }
                wf(pv + 0x8, rf(-0.05f, 0.05f));
                if (coin()) wf(m + 0x44, coin() ? 0.0f : 3.1415927f);
                frame50();
            }, 20);

        // class 367: sliding from its place to 11 along its axis and back, the fraction at or near its goal
        for (int pass = 0; pass < 4; ++pass)
            classcheck("class367_slide", 0x002d9d10, blg_class367_update_l6, [pass, generic_prep, frame50](uint32_t m) {
                generic_prep(m, ri(0, 3) == 0 ? 0 : 2, pass % 3, blg_class367_update_l6);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const float goal = coin() ? 1.0f : 0.0f;
                wf(pv + 0x28, goal);
                const int r = ri(0, 2);
                wf(pv + 0x20, r == 0 ? goal : (r == 1 ? rf(0, 1) : goal + rf(-0.01f, 0.01f)));
                wf(pv + 0x24, rf(-0.1f, 0.1f));
                frame50();
            }, 20);

        level();
        ignores(true, false, true);
        // class 911: a sweeping jet or a bursting one in each state, the hero near (in its box or not), its sound slot, its
        // timer at its edge, the camera near (the nozzle on screen)
        for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class911_jet", 0x002f3ad8, blg_class911_update_l6, [st, pass, generic_prep, frame50, sound_slot, camera_near, hero_at](uint32_t m) {
                    generic_prep(m, st, pass, blg_class911_update_l6);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, u(st));
                    if (st == 0) w32(pv + 0x60, coin() ? 1u : 0u);
                    w32(pv + 0x94, sound_slot(m));
                    { const int r = ri(0, 3); w32(pv + 0x88, r == 0 ? 0u : (r == 1 ? 1u : u(ri(2, 200)))); }
                    hero_at(m, coin() ? rf(0, 10) : rf(10, 30), rf(-1, 2));
                    if (ri(0, 2) == 0) w32(pv + 0x90, 0xffffffffu);
                    if (coin()) camera_near(m, 15);
                    if (coin()) wf(pv + 0x70, rf(0.5f, 3.0f));
                    if (ri(0, 2) == 0) {  // the jet placed so that its flame reaches the hero moby (where the collision has it)
                        const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(P + 0x2080) + 0x10);
                        const float yaw = ee_read<float>(m + 0x48), d = rf(0.3f, 2.5f);
                        Vec4 q = ee_ref<Vec4>(m + 0x10);
                        q.x = h.x - d * std::cos(yaw); q.y = h.y - d * std::sin(yaw); q.z = h.z + rf(0.2f, 1.0f);
                        ee_ref<Vec4>(m + 0x10) = q;
                        ee_ref<Vec4>(P + 0x80) = h;
                        if (st == 2) w32(pv + 0x88, u(ri(1, 3)));
                    }
                    frame50();
                }, 12);

        level();
        ignores(false, true, false);
        // class 1028: the bridge's scene: the trigger (the hero's mode 1, the Hydrodisplacer, no scene), the fades at their
        // crossing and their end, the bridge's values about their thresholds, its checkpoint taken or not, its sound slot
        for (int st = 0; st < 7; ++st)
            for (int pass = 0; pass < 2; ++pass)
                classcheck("class1028_scene", 0x002f55a0, blg_class1028_update_l6, [st, pass, generic_prep, frame50](uint32_t m) {
                    generic_prep(m, st, pass, blg_class1028_update_l6);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, u(st));
                    w8(P + 0x20a4, coin() ? 1u : u(ri(0, 3)));
                    w8(0x0013d4d6, coin() ? 1u : 0u);
                    w32(L6_ADDR(0x0015f5c4), ri(0, 2) != 0 ? 0u : 6u);
                    {
                        const int r = ri(0, 4);
                        wf(pv + 0x6c, r == 0 ? rf(-0.03f, 0.0f) : r == 1 ? rf(-0.01f, 0.03f) : r == 2 ? rf(0.97f, 1.0f) : r == 3 ? -1.0f : rf(-1, 1));
                        // (state 5's end requests the level change, whose disc wait the interpreter cannot answer: kept short of it)
                        if (st == 5 && ee_read<float>(pv + 0x6c) > 0.9f) wf(pv + 0x6c, rf(-0.03f, 0.03f));
                        // a fade that lands on 0 exactly (the crossing's `0 <=`): one step of 1 / frame_scale_i(20) short of it
                        if (ri(0, 4) == 0) wf(pv + 0x6c, -1.0f / static_cast<float>(ri(0, 2) == 0 ? 17 : 20));
                    }
                    {
                        const int r = ri(0, 3);
                        wf(L6_ADDR(0x00161d78), r == 0 ? rf(0, 20) : r == 1 ? rf(19.5f, 20.5f) : r == 2 ? 20.0f : rf(0, 3));
                        wf(L6_ADDR(0x00161d7c), coin() ? rf(0, 1000) : rf(990, 1010));
                        wf(L6_ADDR(0x00161d80), coin() ? rf(0, 1000) : rf(990, 1010));
                        if (coin()) { wf(L6_ADDR(0x00161dac), rf(0, 20)); wf(L6_ADDR(0x00161db0), rf(0, 1000)); }
                        wf(L6_ADDR(0x00161d9c), rf(0, 2)); wf(L6_ADDR(0x00161da0), rf(0, 60)); wf(L6_ADDR(0x00161da4), rf(0, 60));
                    }
                    {
                        const uint32_t cp = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0);
                        w8(cp, coin() ? 0xffu : 0u);
                    }
                    if (coin()) w8(m + 0xbc, u(ri(0, 15)));
                    if (ri(0, 3) != 0) {  // the scripted camera (the camera entry of uid 5) whose pvars +0x80 / +0x90 state 4 moves: in the test area
                        const uint32_t cam = OV(0x00167150) + u(ri(0, 3)) * 0xa0;
                        w16(cam + 0x86, 5);
                        w32(cam + 0x70, T);
                        wfrand(T + 0x80, 8, -50, 50);
                    }
                    if (ri(0, 2) == 0) {  // its last plate's sound playing in that slot
                        const uint32_t e = 0x0013e550u + u(ee_read<uint8_t>(m + 0xbc) & 0xf) * 0x70;
                        w32(e + 0x88, ee_read<uint32_t>(pv + 0x134)); w8(e + 0x74, coin() ? 1u : 0u);
                    }
                    frame50();
                }, 12);

        level();
        ignores(true, false, true);
        // class 1035: the barrier near the camera, the hero crossing one of its lines (the barrier moved onto him), against
        // it (+0x23c), its shocks' count and grace time, the help record, its four switches (class 1302 opened or not, other
        // classes done or not, none)
        {
            level();
            std::vector<uint32_t> switches, others;
            for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100) {
                if (ee_read<uint8_t>(o + 0x20) >= 0xfe) continue;
                (ee_read<int16_t>(o + 0xa6) == 0x516 ? switches : others).push_back(o);
            }
            for (int st = 1; st < 4; ++st)
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("class1035_near", 0x002f6470, blg_class1035_update_l6, [st, pass, generic_prep, frame50, sound_slot, camera_near, switches, others](uint32_t m) {
                        generic_prep(m, st, pass % 3, blg_class1035_update_l6);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                        w8(m + 0x20, u(st));
                        w32(pv + 0x10, sound_slot(m));
                        if (ri(0, 3) != 0) camera_near(m, 40);
                        if (coin()) {  // the hero on one of its lines: the barrier placed so that the line passes through him
                            const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(P + 0x2080) + 0x10);  // (the hero moby where the collision has it)
                            ee_ref<Vec4>(P + 0x80) = h;
                            const float yaw = ee_read<float>(m + 0x48) + (coin() ? 0.0f : 3.1415927f);
                            const float t = rf(-2.8f, 2.8f), side = yaw + 1.5707964f;
                            Vec4 q = ee_ref<Vec4>(m + 0x10);
                            q.x = h.x - 0.5f * std::cos(yaw) - t * std::cos(side);
                            q.y = h.y - 0.5f * std::sin(yaw) - t * std::sin(side);
                            q.z = h.z + ee_read<float>(L6_ADDR(0x00161e18)) + rf(0.2f, 1.2f);
                            ee_ref<Vec4>(m + 0x10) = q;
                            if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = q;
                        }
                        w32(P + 0x23c, ri(0, 2) == 0 ? m : ee_read<uint32_t>(P + 0x23c));
                        if (coin()) w32(OV(0x00173e58), ee_read<uint32_t>(P + 0x2080));  // (the last hit record's moby: the hero, kept by a line that meets nothing)
                        w16(pv + 0x16, ri(0, 2) == 0 ? u(ri(1, 30)) : 0u);
                        w16(pv + 0x14, u(ri(0, 4)));
                        wf(pv + 0x18, coin() ? 0.0f : rf(0, 1));
                        w16(0x00141ac0, ri(0, 2) == 0 ? (coin() ? 0xffffu : u(ri(1, 5))) : 0u);
                        w16(0x00141ac2, u(ri(0, 3000)));
                        w32(0x0015eea4, u(ri(0, 3000000)));
                        const int mode = ri(0, 3);  // the switches: all done, some not, none listed
                        for (uint32_t k = 0; k < 4; ++k) {
                            if (mode == 2 || (ri(0, 3) == 0)) { w32(pv + k * 4, coin() ? 0xffffffffu : ee_read<uint32_t>(pv + k * 4)); continue; }
                            const bool sw = !switches.empty() && coin();
                            const std::vector<uint32_t>& from = sw ? switches : others;
                            if (from.empty()) continue;
                            const uint32_t o = from[static_cast<size_t>(ri(0, static_cast<int>(from.size()) - 1))];
                            w32(pv + k * 4, (o - first) >> 8);
                            if (sw) w8(o + 0x20, mode == 0 || coin() ? 2u : u(ri(0, 3)));
                            else w8(o + 0xbc, mode == 0 || coin() ? 1u : 0u);
                        }
                        frame50();
                    }, 12);
        }

        level();
        ignores(true, true, true);
        // class 1048: its states 14 .. 17 (the class loop above runs 0 .. 15) and every state with its target, its pack, its
        // animation's end, its alert timer and the hero about it
        for (int st = 0; st < 18; ++st)
            for (int pass = 0; pass < 2; ++pass)
                classcheck("class1048_hunt", 0x002f7d78, blg_class1048_update_l6, [st, pass, generic_prep, frame50, hero_at, camera_near](uint32_t m) {
                    generic_prep(m, st, pass, blg_class1048_update_l6);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, u(st));
                    hero_at(m, ri(0, 2) == 0 ? rf(0, 4) : rf(4, 25), coin() ? rf(-0.5f, 0.5f) : (coin() ? rf(-2.2f, 2.2f) : rf(-4, 4)));
                    if (coin()) w32(pv + 0x160, coin() ? ee_read<uint32_t>(P + 0x2080) : 0u);
                    w32(pv + 0x164, coin() ? 2u : 0u);
                    w32(pv + 0x1ac, ri(0, 2) == 0 ? 0u : u(ri(1, 200)));
                    w32(pv + 0x1a8, ri(0, 2) == 0 ? 1u : u(ri(0, 60)));
                    if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                    w8(m + 0x53, u(ri(0, 7)));
                    if (ri(0, 3) == 0) w8(m + 0x21, 0xff);
                    if (ri(0, 3) == 0) w32(pv + 0x1cc, coin() ? 1u : 0u);
                    if (ri(0, 3) == 0) w32(pv + 0x1c0, coin() ? 0xffffffffu : u(ri(0, 20)));
                    if (ri(0, 3) == 0) w32(pv + 0x1b4, coin() ? 1u : 0u);
                    if (ri(0, 3) == 0) w32(P + 0x1c0, u(ri(0, 20)));
                    if (ri(0, 3) == 0) w32(L6_ADDR(0x00161edc), coin() ? 1u : 0u);
                    if (ri(0, 4) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) - rf(0, 400));
                    {  // the hero moby where the hero is (its target), far from the creature now and then, above or below it
                        Vec4 h = ee_ref<Vec4>(P + 0x80);
                        if (ri(0, 4) == 0) { const float a = rf(-3.1f, 3.1f), r = rf(25, 60); h.x = ee_read<float>(m + 0x10) + r * std::cos(a); h.y = ee_read<float>(m + 0x14) + r * std::sin(a); }
                        if (ri(0, 5) == 0) h.z += coin() ? 5.0f : -5.0f;
                        const int32_t wake = ee_read<int32_t>(pv + 0x1c0);
                        if (wake >= 0 && wake < 0x400 && ri(0, 2) == 0) h = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(wake << 7) + 0x30);
                        ee_ref<Vec4>(P + 0x80) = h;
                        ee_ref<Vec4>(ee_read<uint32_t>(P + 0x2080) + 0x10) = h;
                    }
                    if (coin()) { w8(m + 0x31, 1); if (coin()) camera_near(m, 20); }
                    if (st == 4 && coin()) w8(m + 0x53, coin() ? 3u : 6u);
                    if (ri(0, 3) != 0) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                    if (ri(0, 3) == 0) w32(pv + 0x38, 1);
                    if (coin()) { wf(m + 0x54, rf(-2.0f, 10.0f)); if (coin()) w8(m + 0x52, ee_read<uint8_t>(m + 0x53) ^ 1u); }
                    if (ri(0, 3) == 0) w32(P + 0x23c, m);
                    if (ee_read<uint8_t>(m + 0x21) != 0xff && coin()) {  // pack mates dozing (state 1), to be woken by an alert
                        const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(ee_read<uint8_t>(m + 0x21)) * 4);
                        for (uint32_t e = list; list != 0 && e < list + 0x40; e += 2) {
                            const uint32_t mate = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                            if (mate != m && coin()) w8(mate + 0x20, 1);
                            if (ee_read<int16_t>(e) < 0) break;
                        }
                    }
                    if (ri(0, 5) == 0) {  // a group without a list
                        for (int32_t i = 0x6f; i >= 0; --i) if (ee_read<uint32_t>(OV(0x001ab840) + u(i) * 4) == 0) { w8(m + 0x21, u(i)); break; }
                    }
                    frame50();
                }, 12);
        ignores(false, false, false);
        // Blarg's build of hero_enter_idle (class 1028's and class 1109's call, class 615's port on this level), directly: the
        // hero's movement mode 0 .. 4 (the Veldin build also tests for Clank; this one does not)
        l6check("hero_enter_idle_l6", 0x00239528, 60, Ret::None, [level] {
            level();
            w8(P + 0x20a4, u(ri(0, 4)));
            seed_rand();
            return Args{};
        }, [](A) { hero_enter_idle_l6(); return uint64_t{0}; });

        // ---- stage 4 P6.2: the hand ports of classes 1051, 1054, 1061, 1062, 1066 and 1083 (blarg_snagglebeast.cpp,
        // blarg_station_props.cpp, blarg_hydro_station.cpp, blarg_tank.cpp) and the shared helpers (blarg_groups.cpp), each
        // against level 6's own update; "class1051_fix32" / "class1083_fix20" check the 1.02 forks (1.02 Fixes fixes 32 and
        // 20) when run with RAC1_FIXES102=20,32 (skipped otherwise)
        // the uid-5 camera (the scripted camera's pvars +0x80 / +0x90 the scenes move): an entry of the camera table, in the
        // test area
        auto uid5_camera = [] {
            const uint32_t cam = OV(0x00167150) + u(ri(0, 3)) * 0xa0;
            w16(cam + 0x86, 5);
            w32(cam + 0x70, T);
            wfrand(T + 0x80, 8, -50, 50);
        };
        // the mobys of the group `group` set to `state` now and then (a list of the level's)
        auto group_states = [](int32_t group, std::initializer_list<uint32_t> states) {
            if (group < 0 || group >= 0x70) return;
            const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(group) * 4);
            for (uint32_t e = list; list != 0 && e < list + 0x80; e += 2) {
                const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                if (coin()) w8(o + 0x20, *(states.begin() + ri(0, static_cast<int>(states.size()) - 1)));
                if (ee_read<int16_t>(e) < 0) break;
            }
        };
        // class 1051, the Snagglebeast: every state, its target, the pump mode, its health about 20 and 10, the phase
        // change's countdown, its death (+0x2e's flag 2), its animation's end, its rise's camera, its packs
        level();
        ignores(true, true, true);
        auto beast_prep = [generic_prep, frame50, hero_at, uid5_camera, group_states, camera_near](uint32_t m, int st, int pass) {
            w8(0x0013d3ab, ri(0, 3) == 0 ? 1u : 0u);
            w8(0x0013d505, coin() ? 1u : 0u);
            generic_prep(m, st, pass, blg_class1051_update_l6);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            if (ri(0, 5) == 0) w32(pv + 0xb0, 0);
            else if (coin()) w32(pv + 0xb0, ee_read<uint32_t>(0x0013f350 + 0x2080));
            w32(pv + 0xb4, ri(0, 3) == 0 ? 2u : 0u);
            w32(pv + 0x170, coin() ? 1u : 0u);
            { static const int32_t counts[] = {0, 1, 2, 9, 10, 11, 30}; w32(pv + 0x12c, u(ri(0, 4) == 0 ? ri(0, 60) : counts[ri(0, 6)])); }
            { static const float hps[] = {30.0f, 20.4f, 20.0f, 19.9f, 10.3f, 10.0f, 9.9f, 0.2f}; wf(pv + 0x20, ri(0, 4) == 0 ? rf(0, 30) : hps[ri(0, 7)]); }
            wf(pv + 0x174, coin() ? 20.0f : 10.0f);
            w8(pv + 0x2e, ri(0, 5) == 0 ? 2u : 1u);
            w8(m + 0x53, u(ri(0, 10)));
            if (ri(0, 2) != 0) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
            wf(m + 0x54, coin() ? 0.0f : rf(0.0f, 1.0f));
            if (coin()) { w8(m + 0x52, ee_read<uint8_t>(m + 0x53) ^ 1u); wf(m + 0x54, rf(-2.0f, 10.0f)); }
            wf(pv + 0x118, coin() ? 1.0f : rf(0.8f, 1.0f));
            wf(pv + 0x48, rf(-1.0f, 1.0f));
            wf(m + 0x18, ee_read<float>(pv + 0xc8) + (coin() ? rf(-0.5f, 1.0f) : rf(0.0f, 20.0f)));
            w32(pv + 0x178, ri(0, 3) == 0 ? 1u : 0u);
            w8(0x0013d412, ri(0, 3) == 0 ? 1u : 0u);
            w32(0x00140408, coin() ? 8u : 0u);
            w32(0x0013cae0, coin() ? 0x20u : 0u);
            w32(pv + 0x120, ri(0, 2) == 0 ? 1u : u(ri(2, 300)));
            hero_at(m, ri(0, 2) == 0 ? rf(0, 6) : rf(6, 30), rf(-1, 2));
            ee_ref<Vec4>(ee_read<uint32_t>(0x0013f350 + 0x2080) + 0x10) = ee_ref<Vec4>(0x0013f3d0);
            if (coin()) {  // a live camera of type 0x12 (state 15), the cutscene mode now and then
                const uint32_t i = u(ri(0, 47));
                w32(OV(0x00169890) + i * 4, 1);
                w16(OV(0x00167150) + i * 0xa0 + 0x86, 0x12);
            }
            w32(L6_ADDR(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
            if (ri(0, 3) != 0) uid5_camera();
            group_states(ee_read<int32_t>(pv + 0xf0), {0xf, 0x10, 0xfe, 3});
            group_states(ee_read<int32_t>(pv + 0xf4), {0x12, 0, 0xfe});
            if (coin()) {  // its packs all down (each member gone or down): the waves' ends
                for (const uint32_t g : {ee_read<uint32_t>(pv + 0xf0), ee_read<uint32_t>(pv + 0xf4)}) {
                    if (g >= 0x70) continue;
                    const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + g * 4);
                    for (uint32_t e = list; list != 0 && e < list + 0x80; e += 2) {
                        const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                        w8(o + 0x20, coin() ? 0x10u : 0xfeu);
                        if (ee_read<int16_t>(e) < 0) break;
                    }
                }
            }
            if (ee_read<uint8_t>(m + 0xa4) != 0xff && coin()) {  // the hit from a moby of one of the classes its damage scales by
                const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                const uint32_t src = ee_read<uint32_t>(OV(0x0015ffd8)) + u(ri(1, 40)) * 0x100;
                { static const uint32_t classes[] = {0xb0, 0x79, 0x131, 0x33b, 0x57}; w16(src + 0xa6, classes[ri(0, 4)]); }
                w32(e + 0x20, src);
            }
            if (ri(0, 3) == 0) w32(pv + 0x10c, u(ri(0, 7)));  // (a health bar up)
            if (coin()) { w8(m + 0x31, 1); camera_near(m, 50); }
            if (st == 2 && coin()) {  // rising from its pit
                w8(m + 0x53, 8);
                wf(m + 0x18, ee_read<float>(pv + 0xc8) - rf(3.0f, 20.0f));
                wf(pv + 0x48, rf(0.3f, 1.0f));
            }
            if ((st == 3 || st == 5) && coin()) { w8(m + 0x53, 6); wf(m + 0x54, 0.0f); w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u); }
            if (st == 8 && coin()) {  // its target close in front of it: the bite's start
                const uint32_t t = ee_read<uint32_t>(pv + 0xb0) != 0 ? ee_read<uint32_t>(pv + 0xb0) : ee_read<uint32_t>(0x0013f350 + 0x2080);
                w32(pv + 0xb0, t);
                w32(pv + 0xb4, 0);
                w32(pv + 0x120, 30);
                const float yaw = ee_read<float>(m + 0x48) + rf(-0.3f, 0.3f), d = rf(0.5f, ee_read<float>(L6_ADDR(0x00161f00)) + 1.0f);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += d * std::cos(yaw); q.y += d * std::sin(yaw);
                ee_ref<Vec4>(t + 0x10) = q;
            }
            w8(0x0014162a, 0);
            frame50();
        };
        for (int st = 0; st < 16; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1051_fight", 0x002f9a28, blg_class1051_update_l6, [st, pass, beast_prep](uint32_t m) { beast_prep(m, st, pass); }, 12);
        // 1.02 Fixes, fix 32 (RAC1_FIXES102=32): the original with the 1.02 build's order in its state 1 (L6:0x2fb0b8 on the
        // PAL disc 2.00: the countdown's flag +0x170 tested first): an interpreter hook at L6:0x2f9c88 (its `lw v0,0xb4(s3)`)
        // sends a running countdown on to 0x2f9cc8 (the countdown's test, v0 = +0x12c; s0 2, the state the start stores, which
        // the 1.02 build writes out as `li v0,2`) and runs the instruction otherwise;
        // against the port with the fix on
        if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "class1051_fix32") == 0) {
            if (std::getenv("RAC1_FIXES102") == nullptr) {
                std::printf("%-26s skipped (run with RAC1_FIXES102=20,32)\n", "class1051_fix32");
                ++g_skipped;
            } else {
                static int forked = 0;
                forked = 0;
                g_extra_hooks = {{0x002f9c88u, [](EeCpu& c) {
                    const uint32_t pv = c.r32(19);
                    if (ee_read<uint32_t>(pv + 0x170) != 0) {
                        ++forked;
                        c.set32(2, ee_read<int32_t>(pv + 0x12c));
                        c.set32(16, 2);  // (the state the start stores: s0 2 in the North American build's path, the 1.02 build's li v0,2)
                        c.set32(31, 0x002f9cc8);
                    } else {
                        c.set32(2, ee_read<int32_t>(pv + 0xb4));
                        c.set32(31, 0x002f9c8c);
                    }
                }}};
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1051_fix32", 0x002f9a28, [](uint32_t m) {
                        g_advanced_disabled = false;
                        blg_class1051_update_l6(m);
                        g_advanced_disabled = true;
                    }, [pass, beast_prep](uint32_t m) {
                        beast_prep(m, 1, pass);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w32(pv + 0x170, ri(0, 3) != 0 ? 1u : 0u);
                        w32(pv + 0x12c, coin() ? 0u : 1u);
                        w32(pv + 0xb4, coin() ? 2u : 0u);
                        if (coin()) w32(pv + 0xb0, ee_read<uint32_t>(0x0013f350 + 0x2080));
                    }, 20);
                g_extra_hooks.clear();
                std::printf("%-26s %d runs of the original took the 1.02 order's branch\n", "class1051_fix32", forked);
                if (forked == 0) ++g_fail;
            }
        }

        // class 1083, the glass tank: its trigger (a moby's +0xbc, the moby it breaks for), Ratchet near or far, the scene's
        // frame count about its length 0x162044, the camera's move near 0.05 and 1, the hero's place mark
        level();
        ignores(false, true, false);
        auto tank_prep = [generic_prep, frame50, hero_at, uid5_camera](uint32_t m, int st, int pass) {
            generic_prep(m, st, pass, blg_class1083_update_l6);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            const uint32_t trig = u(ri(0, static_cast<int>(n) - 1));
            if (ri(0, 3) != 0) { w32(pv + 8, trig); w8(first + (trig << 8) + 0xbc, coin() ? 1u : 0u); }
            else w32(pv + 8, 0xffffffffu);
            if (coin()) w32(pv + 0x20, 0xffffffffu);
            else {
                const uint32_t o = u(ri(0, static_cast<int>(n) - 1));
                w32(pv + 0x20, o);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                if (coin()) { q.x += rf(-0.08f, 0.08f); q.y += rf(-0.08f, 0.08f); } else { q.x += rf(-3, 3); q.y += rf(-3, 3); }
                ee_ref<Vec4>(first + (o << 8) + 0x10) = q;
            }
            w32(pv + 0x4, ri(0, 3) != 0 ? 1u : 0u);
            w32(pv + 0x0, coin() ? 1u : 0u);
            const int32_t len = ee_read<int32_t>(L6_ADDR(0x00162044));
            w32(pv + 0x24, u(coin() ? ri(0, 2) : len + ri(-2, 2)));
            { const int r = ri(0, 3); wf(pv + 0x18, r == 0 ? rf(0.0f, 0.1f) : r == 1 ? rf(0.95f, 1.0f) : r == 2 ? 1.0f : rf(0.0f, 1.0f)); }
            if (ri(0, 3) == 0) w32(pv + 0xc, 0xffffffffu);
            hero_at(m, ri(0, 2) == 0 ? rf(0, 16) : (coin() ? rf(16, 32) : rf(32, 60)), rf(-1, 2));
            if (ri(0, 3) != 0) uid5_camera();
            frame50();
        };
        for (int st = 0; st < 6; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1083_tank", 0x002ffcb0, blg_class1083_update_l6, [st, pass, tank_prep](uint32_t m) { tank_prep(m, st, pass); }, 12);
        // 1.02 Fixes, fix 20 (RAC1_FIXES102=20): the original with the 1.02 build's break: an interpreter hook at the head of
        // 0x300278's shard loop (L6:0x300328, its `lui at,0x40c0`) ends the break there (0x3003c8, the epilogue's `lq ra`) when
        // it is the far break's (its return address 0x2ffd9c on the stack) and the tank has no moby to break for (+0x20 -1),
        // as the 1.02 build's argument does (L6:0x3011c4 / 0x301754 on the PAL disc 2.00); against the port with the fix on
        if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "class1083_fix20") == 0) {
            if (std::getenv("RAC1_FIXES102") == nullptr) {
                std::printf("%-26s skipped (run with RAC1_FIXES102=20,32)\n", "class1083_fix20");
                ++g_skipped;
            } else {
                static int forked = 0, kept = 0;
                forked = 0;
                kept = 0;
                g_extra_hooks = {{0x00300328u, [](EeCpu& c) {
                    const uint32_t sp = c.r32(29), m = c.r32(17);
                    const bool far = ee_read<uint32_t>(sp + 0x50) == 0x002ffd9cu;
                    if (far && ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x20) == -1) {
                        ++forked;
                        c.set32(31, 0x003003c8);
                    } else {
                        if (far) ++kept;
                        c.set32(1, 0x40c00000);
                        c.set32(31, 0x0030032c);
                    }
                }}};
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1083_fix20", 0x002ffcb0, [](uint32_t m) {
                        g_advanced_disabled = false;
                        blg_class1083_update_l6(m);
                        g_advanced_disabled = true;
                    }, [pass, tank_prep, hero_at](uint32_t m) {
                        tank_prep(m, 1, pass);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w32(pv + 8, 0xffffffffu);  // (set off by the moby +0x20 alone)
                        if (coin()) w32(pv + 0x20, 0xffffffffu);
                        else {
                            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                            const uint32_t trig = u(ri(0, 40));
                            w32(pv + 8, trig);
                            w8(first + (trig << 8) + 0xbc, 1);
                        }
                        hero_at(m, ri(0, 3) == 0 ? rf(0, 30) : rf(33, 60), rf(-1, 2));
                    }, 20);
                g_extra_hooks.clear();
                std::printf("%-26s %d far breaks without shards, %d with\n", "class1083_fix20", forked, kept);
                if (forked == 0 || kept == 0) ++g_fail;
            }
        }

        // class 1054, the double door: Ratchet in its box or not, its +4 (1 judders, 2 held), its group, its judder's steps
        // and timer, the level's word 0x161f5c
        level();
        ignores(false, true, false);
        for (int st = 0; st < 6; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1054_door", 0x002fbfb0, blg_class1054_update_l6, [st, pass, generic_prep, frame50](uint32_t m) {
                    generic_prep(m, st, pass, blg_class1054_update_l6);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, u(st));
                    if (st == 0 && ri(0, 2) == 0) w32(pv, 0xffffffffu);
                    w32(pv + 4, u(ri(0, 3)));
                    const int32_t box = ee_read<int32_t>(pv);
                    if (box >= 0 && box < 0x400 && coin()) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box << 7) + 0x30);
                    if (ri(0, 3) == 0) w8(m + 0x21, 0xff);
                    w8(m + 0xbc, u(ri(0, 3)));
                    w32(pv + 0x4c, ri(0, 2) == 0 ? 1u : u(ri(2, 40)));
                    wf(L6_ADDR(0x00161f5c), coin() ? 0.0f : 1.0f);
                    if (coin()) {  // the halves at their open distances or about them
                        const float a = ee_read<float>(L6_ADDR(0x00161f1c)) + (coin() ? 0.0f : rf(-0.02f, 0.02f));
                        const Vec4 home = ee_ref<Vec4>(pv + 0x10), ax = ee_ref<Vec4>(pv + 0x30);
                        Vec4 q = home; q.x += ax.x * a; q.y += ax.y * a; q.z += ax.z * a;
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                    frame50();
                }, 12);

        // class 1061, the Hydrodisplacer's lift: the hero's mode and the Hydrodisplacer, the console (its box, the button,
        // the help text's answer), Ratchet riding it, above or below it, its water's levels, its wait
        level();
        ignores(false, false, false);
        for (int st = 0; st < 9; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1061_lift", 0x002fc640, blg_class1061_update_l6, [st, pass, generic_prep, frame50](uint32_t m) {
                    generic_prep(m, st, pass, blg_class1061_update_l6);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                    w8(m + 0x20, u(st));
                    w8(P + 0x20a4, u(ri(0, 3) == 0 ? ri(0, 3) : (coin() ? 1 : 0)));
                    w8(0x0013d4d6, coin() ? 1u : 0u);
                    w8(0x0013d4c6, coin() ? 1u : 0u);
                    w8(0x0013d3a8, coin() ? 1u : 0u);
                    w32(L6_ADDR(0x00161f78), u(ri(-1, 1)));
                    w32(L6_ADDR(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
                    w32(pv + 0x90, ri(0, 2) == 0 ? 1u : u(ri(0, 30)));
                    w32(pv + 0x84, coin() ? 1u : 0u);
                    w32(pv + 0x94, coin() ? 1u : 0u);
                    w32(0x0013cae4, coin() ? 0x10u : 0u);
                    if (ri(0, 4) == 0) w32(P + 0x2084, 0x64);
                    w32(OV(0x0015f594), ri(0, 3) == 0 ? 9u : 0u);  // (the help line's owner: free or the lift's)
                    const int32_t console = ee_read<int32_t>(pv + 0x70);
                    const int r = (st == 3 || st == 4) && coin() ? ri(0, 1) : ri(0, 4);
                    if (st == 3 || st == 4) { if (ri(0, 3) != 0) w32(0x0013cae4, 0x10u); if (coin()) w32(pv + 0x90, 0); }
                    Vec4 h = ee_ref<Vec4>(m + 0x10);
                    if (r == 0 && console >= 0 && console < 0x400) h = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(console << 7) + 0x30);
                    else if (r == 1) { h.z += rf(0.3f, 1.0f); h.x += rf(-0.9f, 0.9f); w32(P + 0x2fc, m); w16(P + 0x30e, coin() ? 0u : 1u); }
                    else if (r == 2) { h.z += coin() ? rf(6, 12) : -rf(6, 12); h.x += rf(-10, 10); }
                    else if (r == 3) { h.x += rf(-30, 30); h.y += rf(-30, 30); }
                    ee_ref<Vec4>(P + 0x80) = h;
                    if (coin()) w8(0x0014161b, 0);
                    if (st == 1 && coin()) w16(ee_read<uint32_t>(P + 0x2080) + 0xa6, 0x57);  // (the hero moby of class 0x57)
                    if ((st == 5 || st == 6) && coin()) {  // at its water level or about it
                        const uint32_t water = ee_read<uint32_t>(OV(0x001600ec)) + u(ee_read<int32_t>(pv + 0x78) << 7);
                        const float level = ee_read<float>(water + 0x38) + (st == 6 ? ee_read<float>(water + 0x28) : 0.0f);
                        wf(m + 0x18, level + (coin() ? 0.0f : rf(-0.02f, 0.02f)));
                        wf(pv + 0x8c, 0.0f);
                    }
                    if ((st == 3 || st == 4) && coin()) {  // at its console, the button down, the help line free
                        const int32_t console = ee_read<int32_t>(pv + 0x70);
                        if (console >= 0 && console < 0x400) ee_ref<Vec4>(P + 0x80) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(console << 7) + 0x30);
                        w32(0x0013cae4, 0x10u);
                        w32(P + 0x2084, 0);
                        w32(pv + 0x90, 0);
                        w32(OV(0x0015f594), 0);
                        if (coin()) w32(L6_ADDR(0x00161f78), 0);
                    }
                    frame50();
                }, 12);

        // class 1062, the meshes' choice: the camera in or out of its boxes, the hero in the state 0xf
        level();
        ignores(false, false, false);
        for (int pass = 0; pass < 4; ++pass)
            classcheck("class1062_zones", 0x002fd088, blg_class1062_update_l6, [](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const int32_t box = ee_read<int32_t>(pv + u(ri(0, 1)) * 0x40 + u(ri(0, 3)) * 4);
                Vec4 c = ee_ref<Vec4>(m + 0x10);
                if (box >= 0 && box < 0x400 && ri(0, 3) != 0) c = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box << 7) + 0x30);
                else { c.x += rf(-40, 40); c.y += rf(-40, 40); }
                ee_ref<Vec4>(OV(0x00166dc0)) = c;
                w32(0x0013f350 + 0x208c, ri(0, 4) == 0 ? 0xfu : 0u);
                if (ri(0, 3) == 0) w32(OV(0x0015f424), u(ri(0x3d, 0x40)));  // (the draw queue nearly full or full)
            }, 20);

        // class 1066, the vent: Ratchet in its box, its animation's end, its group's creatures waiting (state 0xf) or not,
        // its timer
        level();
        ignores(false, false, false);
        for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1066_vent", 0x002fda30, blg_class1066_update_l6, [st, pass, generic_prep, frame50, group_states](uint32_t m) {
                    generic_prep(m, st, pass, blg_class1066_update_l6);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, u(st));
                    const int32_t box = ee_read<int32_t>(pv + 4);
                    if (box >= 0 && box < 0x400 && coin()) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box << 7) + 0x30);
                    if (ri(0, 2) != 0) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                    w32(pv + 8, ri(0, 2) == 0 ? 1u : u(ri(0, 30)));
                    if (ri(0, 4) == 0) w32(pv, 0xffffffffu);
                    group_states(ee_read<int32_t>(pv), {0xf, 0xf, 6, 0});
                    frame50();
                }, 12);

        // the shared helpers, directly: the panels (class 367) of a group sent out and back, a group's walkers down, a group
        // put away and brought back, a door set (the mobys the Snagglebeast and the lifts name) opened, shut and set back, a
        // creature let out
        level();
        ignores(false, false, false);
        {
            std::vector<int32_t> groups;
            for (int32_t i = 0; i < 0x70; ++i) if (ee_read<uint32_t>(OV(0x001ab840) + u(i) * 4) != 0) groups.push_back(i);
            std::vector<uint32_t> sets, creatures;
            for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100) {
                if (ee_read<uint8_t>(o + 0x20) >= 0xfe) continue;
                if (ee_read<int16_t>(o + 0xa6) == 0x418) creatures.push_back(o);
            }
            // the door sets: the mobys the Snagglebeast (+0xfc / +0x100) and the lifts (+0x60 .. +0x68) name, with their parts
            for (const uint32_t fn : {0x002f9a28u, 0x002fc640u})
                for (const uint32_t o : mobys_with(fn)) {
                    const uint32_t pv = ee_read<uint32_t>(o + 0x78);
                    for (const uint32_t off : fn == 0x002f9a28u ? std::vector<uint32_t>{0xfc, 0x100} : std::vector<uint32_t>{0x60, 0x64, 0x68}) {
                        const int32_t i = ee_read<int32_t>(pv + off);
                        if (i < 0 || i > 0x2000) continue;
                        const uint32_t d = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(i) << 8);
                        if (std::find(sets.begin(), sets.end(), d) == sets.end()) sets.push_back(d);
                    }
                }
            std::printf("INFO groups %zu door sets %zu creatures %zu\n", groups.size(), sets.size(), creatures.size());
            auto any_group = [groups, group_states]() -> int32_t {
                if (groups.empty()) return -1;
                const int32_t g = groups[static_cast<size_t>(ri(0, static_cast<int>(groups.size()) - 1))];
                group_states(g, {0x12, 0, 0xf, 0x10, 0xfe, 0xfd, 6});
                if (ri(0, 4) == 0) {  // a member made one of the classes the helpers look for
                    const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4);
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(list) & 0x7fffu) << 8);
                    w16(o + 0xa6, coin() ? 0x16fu : 0x33bu);
                }
                return g;
            };
            struct GroupFn { const char* name; uint32_t addr; void (*fn)(int32_t); };
            static const GroupFn gfns[] = {{"panels_slide_out_l6", 0x002d9e08, panels_slide_out_l6},
                                           {"panels_slide_back_l6", 0x002d9e78, panels_slide_back_l6},
                                           {"group_put_away_l6", 0x002f9948, group_put_away_l6},
                                           {"group_bring_back_l6", 0x002f99b0, group_bring_back_l6}};
            for (const GroupFn& g : gfns) {
                auto fn = g.fn;
                l6check(g.name, g.addr, 40, Ret::None, [level, any_group] { level(); const int32_t gr = any_group(); return Args{{u(gr)}, {}}; },
                        [fn](A a) { fn(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
            }
            l6check("group_walkers_down_l6", 0x002e9ea0, 60, Ret::Int32, [level, any_group] { level(); return Args{{u(any_group())}, {}}; },
                    [](A a) { return r32(group_walkers_down_l6(static_cast<int32_t>(a.i[0]))); });
            struct SetFn { const char* name; uint32_t addr; void (*fn)(uint32_t); };
            static const SetFn sfns[] = {{"door_set_open_l6", 0x002f5360, door_set_open_l6},
                                         {"door_set_shut_l6", 0x002f53e8, door_set_shut_l6},
                                         {"door_set_reset_l6", 0x002f54a0, door_set_reset_l6}};
            for (const SetFn& f : sfns) {
                if (sets.empty()) { std::printf("%-26s no door set on this level\n", f.name); continue; }
                auto fn = f.fn;
                l6check(f.name, f.addr, 40, Ret::None, [level, sets] {
                    level();
                    const uint32_t m = sets[static_cast<size_t>(ri(0, static_cast<int>(sets.size()) - 1))];
                    { static const uint32_t states[] = {2, 6, 0, 4, 8}; w8(m + 0x20, states[ri(0, 4)]); }
                    seed_rand();
                    return Args{{m}, {}};
                }, [fn](A a) { fn(a.i[0]); return uint64_t{0}; });
            }
            if (creatures.empty()) std::printf("%-26s no creature (class 0x418) on this level\n", "creature_let_out_l6");
            else
                l6check("creature_let_out_l6", 0x002e9d10, 60, Ret::Int32, [level, creatures] {
                    level();
                    const uint32_t m = creatures[static_cast<size_t>(ri(0, static_cast<int>(creatures.size()) - 1))];
                    w8(m + 0x20, coin() ? 0xfu : u(ri(0, 17)));
                    Vec4 q = ee_ref<Vec4>(m + 0x10); q.z += rf(0, 20);
                    ee_ref<Vec4>(OUT) = q;
                    seed_rand();
                    return Args{{m, OUT}, {}};
                }, [](A a) { return r32(creature_let_out_l6(a.i[0], a.i[1])); });
        }
        //@@P63_ME
        // ---- stage 4 P6.3: the hand ports of classes 1091, 1105, 1108, 1109, 1118, 1123, 1348, 1512 and 1551
        // (blarg_scene_props.cpp, blarg_scientist.cpp, blarg_countdown.cpp, blarg_ship.cpp, blarg_valve_wheel.cpp,
        // blarg_electric_arc.cpp, blarg_help_voices.cpp, blarg_smoke_sparks.cpp), their helpers, the level's own builds of two
        // engine functions (blarg_engine_l6.cpp) and the draws of classes 1035, 1062 and 1123 (blarg_draws.cpp), each against
        // level 6's own code
        // the groups of the level that have a list
        level();
        std::vector<int32_t> listed_groups;
        for (int32_t i = 0; i < 0x70; ++i) if (ee_read<uint32_t>(OV(0x001ab840) + u(i) * 4) != 0) listed_groups.push_back(i);
        auto a_group = [listed_groups, group_states]() -> int32_t {
            const int32_t g = listed_groups[static_cast<size_t>(ri(0, static_cast<int>(listed_groups.size()) - 1))];
            group_states(g, {0, 1, 4, 0xf, 0x10, 0x11, 0x12, 0x7e, 0x7f, 0xfe, 0xfd});
            return g;
        };
        // a moby of the table by its index in a pvar word (the index when it is one, else -1)
        auto table_index_moby = [](uint32_t word) -> uint32_t {
            const int32_t i = ee_read<int32_t>(word);
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), n = (ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8;
            return i >= 0 && u(i) < n ? first + (u(i) << 8) : 0u;
        };

        // class 1108, the countdown: its checkpoint taken or not, the room's creatures down or not, its alarm moby's states
        // (1 / 2 / above 3 / 7), the camera's path at its end and in its first 30 points, the clock at 30 / 15 / 5 seconds
        // and at its end, the next rumble due, the dialog stream on, Ratchet in the state 0x72, a scene on, the fade at 1, the
        // disc's language and the warnings heard
        level();
        ignores(false, true, true);
        auto countdown_prep = [generic_prep, frame50, group_states, table_index_moby, uid5_camera](uint32_t m, int st, int pass) {
            generic_prep(m, st, pass, blg_class1108_update_l6);
            frame50();
            if (ri(0, 3) != 0) uid5_camera();  // (the scripted camera the states 2 / 3 move: in the test area)
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0), ri(0, 3) == 0 ? (coin() ? 0xffu : 1u) : 0u);
            const int32_t creatures = ee_read<int32_t>(pv + 0x1c);
            if (creatures >= 0 && creatures < 0x70) {
                if (coin()) group_states(creatures, {0xfe, 0x80, 0xff});
                else group_states(creatures, {0, 0x10, 0xfe});
            }
            if (const uint32_t alarm = table_index_moby(pv + 0x2c); alarm != 0) {
                static const uint32_t states[] = {0, 1, 2, 3, 4, 7, 9};
                w8(alarm + 0x20, states[ri(0, 6)]);
            }
            const int32_t pi = ee_read<int32_t>(pv + 0x24);
            const uint32_t path = pi >= 0 && pi < 0xb0 ? ee_read<uint32_t>(OV(0x001b04b0) + u(pi) * 4) : 0u;
            if (path != 0) {
                const int32_t n = ee_read<int32_t>(path);
                const int r = ri(0, 3);
                w32(pv + 0xb0, u(r == 0 ? n - 2 : r == 1 ? ri(0, 31) : ri(0, n > 2 ? n - 2 : 0)));
            }
            {
                static const int32_t at[] = {0x708, 0x384, 0x12c, 1, 0, 2};
                const int r = ri(0, 7);
                w32(pv + 0xb4, u(r < 6 ? frame_scale_i(at[r]) : ri(3, 2000)));
            }
            w32(pv + 0xc0, ri(0, 2) == 0 ? 1u : u(ri(0, 600)));
            w16(0x0015172a, coin() ? 3u : u(ri(0, 4)));
            w32(P + 0x2084, ri(0, 3) == 0 ? 0x72u : ee_read<uint32_t>(P + 0x2084));
            w32(L6_ADDR(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
            wf(pv + 0xb8, ri(0, 2) == 0 ? 1.0f : (coin() ? rf(0.9f, 1.0f) : 0.0f));
            w32(pv + 0xbc, ri(0, 2) == 0 ? 1u : u(ri(0, 20)));
            w32(0x0015ed88, u(ri(0, 6)));
            for (uint32_t k = 0; k < 3; ++k) w32(L6_ADDR(0x001620b8) + k * 4, coin() ? 1u : 0u);
        };
        for (int st = 0; st < 7; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1108_countdown", 0x00301b90, blg_class1108_update_l6, [st, pass, countdown_prep](uint32_t m) { countdown_prep(m, st, pass); }, 12);

        // class 1108's helpers, directly: the group ones on the level's groups (their mobys' states varied), the voice line on a
        // countdown moby (each `which`, the language, the warnings heard), a moby set off, the smoke sources started (a group's
        // members made class 1512 waiting, on screen or not)
        level();
        ignores(false, false, false);
        {
            struct GroupFn { const char* name; uint32_t addr; void (*fn)(int32_t); };
            static const GroupFn gfns[] = {{"group_hold_creatures_l6", 0x002e9e30, group_hold_creatures_l6},
                                           {"group_restart_l6", 0x002f45a0, group_restart_l6},
                                           {"group_wait_l6", 0x002f45f0, group_wait_l6},
                                           {"group_hold_l6", 0x002ffc38, group_hold_l6}};
            for (const GroupFn& g : gfns) {
                auto fn = g.fn;
                l6check(g.name, g.addr, 40, Ret::None, [level, a_group] { level(); return Args{{u(a_group())}, {}}; },
                        [fn](A a) { fn(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
            }
            const std::vector<uint32_t> clocks = mobys_with(0x00301b90);
            if (clocks.empty()) std::printf("%-26s no class-1108 moby on this level\n", "countdown_voice_l6");
            else
                l6check("countdown_voice_l6", 0x00302400, 80, Ret::None, [level, clocks] {
                    level();
                    const uint32_t m = clocks[static_cast<size_t>(ri(0, static_cast<int>(clocks.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w32(0x0015ed88, u(ri(0, 6)));
                    for (uint32_t k = 0; k < 3; ++k) w32(L6_ADDR(0x001620b8) + k * 4, ri(0, 2) != 0 ? 1u : 0u);
                    if (ri(0, 4) == 0) w32(pv + 0x38 + u(ri(0, 5)) * 4, 0xffffffffu);
                    if (ri(0, 4) == 0) w32(pv + 0x50 + u(ri(0, 5)) * 4, 0xffffffffu);
                    if (ri(0, 4) == 0) w32(pv + 0x68 + u(ri(0, 17)) * 4, 0xffffffffu);
                    w32(0x001516ec, R());
                    seed_rand();
                    return Args{{m, u(ri(-1, 3))}, {}};
                }, [](A a) { countdown_voice_l6(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
            l6check("moby_set_off_l6", 0x003047b8, 40, Ret::None, [level] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t m = first + u(ri(0, 60)) * 0x100;
                w8(m + 0x20, coin() ? 1u : u(ri(0, 4)));
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { moby_set_off_l6(a.i[0]); return uint64_t{0}; });
            l6check("smoke_sources_start_l6", 0x00309240, 60, Ret::None, [level, a_group] {
                level();
                if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
                screen_setup();
                projection_setup();
                render_view_setup();
                const int32_t g = coin() ? a_group() : ri(0, 0x6f);
                const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4);
                const Vec4 cam = ee_ref<Vec4>(OV(0x00166dc0));
                for (uint32_t e = list; list != 0 && e < list + 0x80; e += 2) {
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ri(0, 3) != 0) { w16(o + 0xa6, 0x5e8); w8(o + 0x20, coin() ? 1u : 2u); }
                    if (ri(0, 3) != 0)  // (put where the view sees it: the original's test, w 0.25 and radius 64)
                        for (int k = 0; k < 200; ++k) {
                            const Vec4 q{cam.x + rf(-40, 40), cam.y + rf(-40, 40), cam.z + rf(-15, 15), 0.25f};
                            ee_ref<Vec4>(OUT + 0x100) = q;
                            if (point_on_screen(OUT + 0x100, 64.0f) >= 0) { Vec4 at = q; at.w = ee_read<float>(o + 0x1c); ee_ref<Vec4>(o + 0x10) = at; break; }
                        }
                    if (ee_read<int16_t>(e) < 0) break;
                }
                seed_rand();
                return Args{{u(g)}, {}};
            }, [](A a) { smoke_sources_start_l6(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
            // the level's build of level_sound_play: the index about the level's count, a moby or none
            l6check("level_sound_play_l6", 0x0029aec0, 60, Ret::Int32, [level] {
                level();
                const int32_t n = ee_read<int32_t>(L6_ADDR(0x0015f5f0)) - ee_read<int32_t>(L6_ADDR(0x0015f574));
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                return Args{{u(coin() ? ri(0, n > 0 ? n - 1 : 0) : ri(n - 2, n + 2)), coin() ? 0u : R() & 0xff, coin() ? first + u(ri(0, 40)) * 0x100 : 0u}, {}};
            }, [](A a) { return r32(u(level_sound_play_l6(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2]))); });
        }

        // the draws (as batch 517's): the screen, the projection and the view set up, the camera about the moby, the packet cursor
        // on a test-area buffer; the port given the VU0 the original started from, and after it the VU0 lanes whose last writer is
        // the function itself or a VU0-exact callee compared with the original's: vf1 .. vf3, vf4 (all four lanes on
        // arc_strands_draw_l6 and class1035_draw_pk; .x .y on class1123_draw_pk, where mat4_mul runs after shadows_vu1_setup and
        // writes vf4.z .w last while its host port keeps no VU0 state), vf5 (all four lanes on arc_strands_draw_l6; .x .y on the
        // two callers, where mat4_scale_diag's qmtc2 carries the caller's GPR upper half into .z .w), vf6.x, vf10 / vf11 .x .y,
        // vf14 .. vf17, vf20, vf21, ACC and Q (a difference marks the test area's word 0x1e0ff00)
        {
            static EeInterpreter* s_ee = nullptr;
            static VUState s_vu0_in;
            g_pre_call = [](EeInterpreter& ee) { s_ee = &ee; s_vu0_in = ee.vu0(); };
            auto vu0_given = [] {
                VUState& v = vu0::global().st;
                v.vf = s_vu0_in.vf; v.vi = s_vu0_in.vi; v.acc = s_vu0_in.acc; v.q = s_vu0_in.q; v.i = s_vu0_in.i; v.r = s_vu0_in.r;
                v.p = s_vu0_in.p; v.macflag = s_vu0_in.macflag; v.statusflag = s_vu0_in.statusflag; v.clipflag = s_vu0_in.clipflag;
                std::memcpy(v.mem.data(), s_vu0_in.mem.data(), 0x1000);
            };
            auto vu0_compare = [](int vf4_mask, int vf5_mask) {
                if (s_ee == nullptr) return;
                const VUState& o = s_ee->vu0();
                const VUState& p = vu0::global().st;
                const int regs[][2] = {{1, 15}, {2, 15}, {3, 15}, {4, vf4_mask}, {5, vf5_mask}, {6, 1}, {10, 3}, {11, 3},
                                       {14, 15}, {15, 15}, {16, 15}, {17, 15}, {20, 15}, {21, 15}};
                int bad = 0;
                for (const auto& r : regs)
                    for (int i = 0; i < 4; ++i)
                        if ((r[1] >> i & 1) != 0 && o.vf[static_cast<size_t>(r[0])].u[i] != p.vf[static_cast<size_t>(r[0])].u[i] && bad++ < 4)
                            std::printf("  VU0 vf%d lane %d: orig=%08x port=%08x\n", r[0], i, o.vf[static_cast<size_t>(r[0])].u[i], p.vf[static_cast<size_t>(r[0])].u[i]);
                for (int i = 0; i < 4; ++i)
                    if (o.acc.u[i] != p.acc.u[i] && bad++ < 4) std::printf("  VU0 ACC lane %d: orig=%08x port=%08x\n", i, o.acc.u[i], p.acc.u[i]);
                if (std::bit_cast<uint32_t>(o.q) != std::bit_cast<uint32_t>(p.q) && bad++ < 4) std::printf("  VU0 Q: orig=%08x port=%08x\n", std::bit_cast<uint32_t>(o.q), std::bit_cast<uint32_t>(p.q));
                if (bad != 0) w32(TEST_AREA + 0xff00, 0x0bad0000u + u(bad));
            };
            auto view = [level](uint32_t m, float r) {
                level();
                if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
                screen_setup();
                projection_setup();
                if (m != 0 && ri(0, 3) != 0) {
                    Vec4 c = ee_ref<Vec4>(m + 0x10);
                    c.x += rf(-r, r); c.y += rf(-r, r); c.z += rf(0, r * 0.5f);
                    ee_ref<Vec4>(OV(0x00166dc0)) = c;
                }
                render_view_setup();
                w32(L6_ADDR(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);
            };
            auto of_class = [](int16_t cls) {
                std::vector<uint32_t> v;
                for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                    if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<int16_t>(o + 0xa6) == cls) v.push_back(o);
                return v;
            };
            // the strands' strengths about their base and top (both blends, the base itself, a base of 0)
            auto strengths = [](uint32_t table, uint32_t top_word, uint32_t base_word) {
                const int r = ri(0, 4);
                if (r == 0) w32(base_word, 0);
                const int32_t top = ee_read<int32_t>(top_word), base = ee_read<int32_t>(base_word);
                for (uint32_t k = 0; k < 4; ++k) {
                    const int c = ri(0, 4);
                    w32(table + k * 4, u(c == 0 ? base : c == 1 ? top : c == 2 ? base + ri(-60, 0) : c == 3 ? base + ri(1, 60) : ri(-1000, 1000)));
                }
            };
            level();
            ignores(false, false, false);
            const std::vector<uint32_t> barriers = of_class(1035), arcs = of_class(1123);
            std::vector<uint32_t> anyone = barriers;
            anyone.insert(anyone.end(), arcs.begin(), arcs.end());
            if (anyone.empty()) anyone.push_back(ee_read<uint32_t>(OV(0x0015ffd8)));
            std::printf("INFO draws: class-1035 mobys %zu, class-1123 %zu\n", barriers.size(), arcs.size());
            // class 1035's bolts: the barrier's alpha (+0x18), its turn, the level's strengths
            l6check("class1035_draw_pk", 0x002f6dd0, 60, Ret::None, [view, strengths, barriers, anyone] {
                const std::vector<uint32_t>& from = barriers.empty() ? anyone : barriers;
                const uint32_t m = from[static_cast<size_t>(ri(0, static_cast<int>(from.size()) - 1))];
                view(m, 20.0f);
                wf(ee_read<uint32_t>(m + 0x78) + 0x18, coin() ? rf(0, 1) : (coin() ? 0.0f : 1.0f));
                if (coin()) wfrand(m + 0x40, 3, -3.1f, 3.1f);
                if (ri(0, 3) != 0) strengths(L6_ADDR(0x00161e40), L6_ADDR(0x00161e50), L6_ADDR(0x00161e54));
                seed_rand();
                return Args{{m}, {}};
            }, [vu0_given, vu0_compare](A a) { vu0_given(); blg_class1035_draw_l6(a.i[0]); vu0_compare(15, 3); return uint64_t{0}; });
            // class 1062's first draw: the camera's angles moving its texture
            l6check("class1062_draw_a_pk", 0x002fd1a0, 40, Ret::None, [view, anyone] {
                const uint32_t m = anyone[static_cast<size_t>(ri(0, static_cast<int>(anyone.size()) - 1))];
                view(0, 0.0f);
                wf(L6_ADDR(0x00167554), rf(-3.1f, 3.1f));
                wf(L6_ADDR(0x00167558), rf(-3.1f, 3.1f));
                seed_rand();
                return Args{{m}, {}};
            }, [vu0_given](A a) { vu0_given(); blg_class1062_draw_a_l6(a.i[0]); return uint64_t{0}; });
            // class 1123's strands: the arc's points (pvars) moved, its strengths (+0xf00), its turn
            l6check("class1123_draw_pk", 0x00305170, 60, Ret::None, [view, strengths, arcs, anyone] {
                const std::vector<uint32_t>& from = arcs.empty() ? anyone : arcs;
                const uint32_t m = from[static_cast<size_t>(ri(0, static_cast<int>(from.size()) - 1))];
                view(m, 20.0f);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (coin()) wfrand(pv, 480, -3.0f, 3.0f);
                if (coin()) wfrand(m + 0x40, 3, -3.1f, 3.1f);
                strengths(pv + 0xf00, L6_ADDR(0x00162234), L6_ADDR(0x00162238));
                seed_rand();
                return Args{{m}, {}};
            }, [vu0_given, vu0_compare](A a) { vu0_given(); blg_class1123_draw_l6(a.i[0]); vu0_compare(3, 3); return uint64_t{0}; });
            // the strands directly: a record in the test area (1 .. 30 points a strand, random strengths, colours, coordinates)
            // and a matrix
            l6check("arc_strands_draw_l6", 0x00216b88, 80, Ret::None, [view, strengths] {
                view(0, 0.0f);
                const int32_t n = ri(0, 3) == 0 ? ri(1, 3) : ri(4, 30);
                w32(T + 0x14, u(n));
                w32(T + 0x18, T2);
                wfrand(T2, u(n) * 16, -5.0f, 5.0f);
                w32(T + 8, T + 0x40);
                w32(T + 0, u(ri(-200, 400)));
                w32(T + 4, u(ri(0, 4) == 0 ? 0 : ri(-100, 300)));
                strengths(T + 0x40, T + 0, T + 4);
                w32(T + 0xc, R());
                w32(T + 0x10, R());
                wf(T + 0x1c, rf(-0.2f, 0.2f));
                wf(T + 0x20, rf(-1.0f, 1.0f));
                wfrand(T + 0x100, 16, -2.0f, 2.0f);
                seed_rand();
                return Args{{T, T + 0x100}, {}};
            }, [vu0_given, vu0_compare](A a) { vu0_given(); arc_strands_draw_l6(a.i[0], a.i[1]); vu0_compare(15, 15); return uint64_t{0}; });
            g_pre_call = nullptr;
        }
        // ---- stage 4 P6.3 (helper a): class 1109, Ratchet's ship (blarg_ship.cpp), and its helpers, each against level 6's
        // own code. The scenes it starts are kept off the disc: the hero's +0x22a8 at 0 sends cutscene_start down its early
        // way out (hero_lost), 0x141401 set makes movie_play return before the movie; the planet map's level change (state
        // 7 without 0x13d4de, state 9 outside a scene) is not driven.
        {
            level();
            const std::vector<uint32_t> ships = mobys_with(0x00302578);
            const int32_t nboxes = ee_read<int32_t>(OV(0x001600f0));
            auto box_ok = [nboxes](int32_t i) { return i >= 0 && i < nboxes; };
            auto box_at = [](int32_t i) { return ee_read<uint32_t>(OV(0x001600ec)) + (u(i) << 7); };
            auto path_at = [](int32_t i) -> uint32_t { return i >= 0 && i < 176 ? ee_read<uint32_t>(OV(0x001b04b0) + u(i) * 4) : 0u; };
            auto group_at = [](int32_t g) -> uint32_t { return g >= 0 && g < 0x70 ? ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4) : 0u; };
            auto moby_ok = [](int32_t i) {
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                return i >= 0 && u(i) < ((ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8);
            };
            // a route (0 .. 4) whose three paths (the camera's +0x60, the look +0x74, the ship's +0x88) exist
            auto route_ok = [path_at](uint32_t pv, int32_t r) {
                if (r < 0 || r > 4) return false;
                for (const uint32_t off : {0x60u, 0x74u, 0x88u})
                    if (path_at(ee_read<int32_t>(pv + off + u(r) * 4)) == 0) return false;
                return true;
            };
            if (!ships.empty()) {
                const uint32_t pv = ee_read<uint32_t>(ships[0] + 0x78);
                std::printf("INFO class1109 ships %zu routes", ships.size());
                for (int32_t r = 0; r < 5; ++r) std::printf(" %d", route_ok(pv, r) ? 1 : 0);
                std::printf(" group 0x%x (list %s) infobot %d boxes a0..ac %d %d %d %d zones %d %d %d f0 %d checkpoints %d %d\n",
                            ee_read<uint32_t>(pv + 0xcc), group_at(ee_read<int32_t>(pv + 0xcc)) != 0 ? "yes" : "no",
                            ee_read<int32_t>(pv + 0x10c), ee_read<int32_t>(pv + 0xa0), ee_read<int32_t>(pv + 0xa4),
                            ee_read<int32_t>(pv + 0xa8), ee_read<int32_t>(pv + 0xac), ee_read<int32_t>(pv + 0x100),
                            ee_read<int32_t>(pv + 0x104), ee_read<int32_t>(pv + 0x108), ee_read<int32_t>(pv + 0xf0),
                            ee_read<int32_t>(pv + 0xf8), ee_read<int32_t>(pv + 0xfc));
            }
            // the ship's pvars kept to the level's own values where they name level data: a route without its paths takes
            // a valid route's paths (the game offers only the routes the level has)
            auto fix_routes = [route_ok](uint32_t pv) {
                int32_t good = -1;
                for (int32_t r = 0; r < 5 && good < 0; ++r) if (route_ok(pv, r)) good = r;
                if (good < 0) return false;
                for (int32_t r = 0; r < 5; ++r)
                    if (!route_ok(pv, r))
                        for (const uint32_t off : {0x60u, 0x74u, 0x88u}) w32(pv + off + u(r) * 4, ee_read<uint32_t>(pv + off + u(good) * 4));
                return true;
            };
            // the scenes kept off the disc (see above), the screen not in a scene now and then
            auto no_disc = [P] {
                w32(P + 0x22a8, 0);
                w8(0x00141401, 1);
            };
            // the camera path of route `route`: its points' events (their w): most none (-1), others an explosion 0 .. 7
            // (its box valid), the blast 10 (`blast`), or 8 / 9 (neither: only cleared); `dense` more of them
            auto seed_events = [path_at, box_ok, group_at](uint32_t pv, int32_t route, bool dense, bool blast) {
                const uint32_t cam = path_at(ee_read<int32_t>(pv + 0x60 + u(route) * 4));
                if (cam == 0) return;
                const int32_t n = ee_read<int32_t>(cam);
                const bool blast_ok = blast && group_at(ee_read<int32_t>(pv + 0xcc)) != 0;
                for (int32_t k = 0; k < n && k < 0x200; ++k) {
                    int32_t ev = -1;
                    const int r = ri(0, dense ? 5 : 11);
                    if (r <= 2) { ev = ri(0, 7); if (!box_ok(ee_read<int32_t>(pv + 0xd0 + u(ev) * 4))) ev = -1; }
                    else if (r == 3 && blast_ok && ri(0, 3) == 0) ev = 10;
                    else if (r == 4) ev = ri(8, 9);
                    wf(cam + 0x1c + u(k) * 16, static_cast<float>(ev));
                }
                if (ri(0, 3) == 0) {  // (an explosion at the point 0: the original's write of the path's header word +0xc)
                    const int32_t ev = ri(0, 7);
                    if (box_ok(ee_read<int32_t>(pv + 0xd0 + u(ev) * 4))) wf(cam + 0x1c, static_cast<float>(ev));
                }
            };
            // a flight's state: its progress at its start, about the roll's 0.8, at its end (one step short of 1 or there),
            // or anywhere; its roll and rate; its sizes from the level's two
            auto flight_prep = [](uint32_t pv) {
                const int r = ri(0, 5);
                wf(pv + 0xf4, r == 0 ? rf(0.0f, 0.01f) : r == 1 ? rf(0.78f, 0.82f) : r == 2 ? 1.0f - rf(0.0f, 0.0008f)
                                    : r == 3 ? 1.0f : rf(0.0f, 1.0f));
                wf(pv + 0xb0, coin() ? 0.0f : rf(-3.1f, 3.1f));
                wf(pv + 0xb4, coin() ? 0.0f : rf(-0.2f, 0.2f));
                const float a = ee_read<float>(L6_ADDR(0x001620f4)), b = ee_read<float>(L6_ADDR(0x001620f8));
                wf(pv + 0xc4, coin() ? a : b);
                wf(pv + 0xc8, coin() ? a : b);
            };

            // class 1109, the ship: every state (7 only into its cutscene 5, 9 only in a scene). State 1: pass 0 the boxes that
            // move it (Ratchet off it in the zone of its site), pass 1 the offer (Ratchet on it, the voice line's flags and
            // record, the checkpoints, the button and the HUD owner, the choice), pass 2 everything at random (a flight
            // asked, 0x162100, busy). State 3: the flight's progress, its route's camera events, landing after the blast route
            // or not, at each site. State 6: Ratchet near the infobot or not. Each: the flash asked (0x162104)
            level();
            // (seg, and the hit point's w word 0x17461c: the segment record's hit point 0x174610 .. 0x17461c, whose w lane the
            // original's segment_collide stores from a stale VU0 lane, the port 0: the known engine-port difference of the
            // collision records, OV(0x173e40) .. 0x173ec0, which nothing reads; seen in state 1 once the draws' checks have
            // left other VU0 values in the same process)
            ignores(true, true, true);
            g_ignored_words.push_back(0x0017461cu);
            auto ship_prep = [=](uint32_t m, int st, int pass) {
                generic_prep(m, st, 0, blg_class1109_update_l6);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                fix_routes(pv);
                w8(m + 0x20, u(st));
                const uint32_t cp = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4);
                auto cp_set = [pv, cp](uint32_t off, bool taken) {
                    const int32_t i = ee_read<int32_t>(pv + off);
                    if (i >= 0 && i < 16) w8(cp + u(i), taken ? 0xffu : 0u);
                };
                // (the blast's checkpoint taken retires the group's pieces in state 0: only with the group's list)
                cp_set(0xf8, ri(0, 2) == 0 && group_at(ee_read<int32_t>(pv + 0xcc)) != 0);
                cp_set(0xfc, coin());
                w8(m + 0x53, coin() ? 2u : u(ri(0, 3)));
                w32(pv + 0xbc, u(ri(0, 2)));
                w32(pv + 0xc0, u(ri(0, 4)));
                w32(pv + 0x110, ri(0, 3) == 0 ? 1u : 0u);
                w32(pv + 0x114, coin() ? 1u : 0u);
                w32(pv + 0x9c, u(ri(-1, 4)));
                flight_prep(pv);
                w8(0x0013d3a9, coin() ? 1u : 0u);
                w8(0x0013d3ac, coin() ? 1u : 0u);
                w16(0x00141c00, coin() ? 0u : 1u);
                w8(0x0015ee1c, ri(0, 3) == 0 ? 1u : 0u);
                w8(0x0015ee1d, ri(0, 3) == 0 ? 1u : 0u);
                w32(L6_ADDR(0x00179b90), coin() ? 0u : 1u);
                w32(L6_ADDR(0x00179bb4), coin() ? 0xffffffffu : u(ri(0, 3)));
                w32(L6_ADDR(0x00162100), ri(0, 4) == 0 ? 1u : 0u);
                w32(L6_ADDR(0x00162104), ri(0, 4) == 0 ? 1u : 0u);
                w32(0x0013cae4, coin() ? 0x10u : 0u);
                w32(L6_ADDR(0x0015f594), ri(0, 3) != 0 ? 10u : (coin() ? 0u : 9u));
                if (coin()) { w32(P + 0x2fc, m); w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u); }
                {  // Ratchet in one of its boxes now and then
                    static const uint32_t zones[] = {0x100, 0x104, 0x108, 0xf0};
                    const int r = ri(0, 6);
                    if (r < 4 && box_ok(ee_read<int32_t>(pv + zones[r]))) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(box_at(ee_read<int32_t>(pv + zones[r])) + 0x30);
                }
                w32(L6_ADDR(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
                no_disc();
                if (ri(0, 3) != 0) uid5_camera();
                if (st == 1 && pass == 0) {  // off it, nothing asked, Ratchet in the zone that moves it from its site
                    w32(P + 0x2fc, 0);
                    w32(L6_ADDR(0x00162100), 0);
                    w32(pv + 0x110, ri(0, 5) == 0 ? 1u : 0u);
                    const int32_t site = ri(0, 3) == 0 ? 1 : (coin() ? 0 : 2);
                    w32(pv + 0xbc, u(site));
                    const uint32_t zone = site == 0 ? (coin() ? 0x100u : 0x108u) : site == 2 ? (coin() ? 0x104u : 0x108u) : (coin() ? 0x100u : 0x108u);
                    const int32_t b = ee_read<int32_t>(pv + zone);
                    if (box_ok(b) && ri(0, 3) != 0) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(box_at(b) + 0x30);
                    else hero_at(m, rf(0, 30), rf(-1, 2));
                }
                if (st == 1 && pass == 1) {  // on it, the offer; the button on its HUD message now and then
                    w32(P + 0x2fc, m);
                    w16(P + 0x30e, ri(0, 5) == 0 ? 1u : 0u);
                    w32(L6_ADDR(0x00162100), ri(0, 5) == 0 ? 1u : 0u);
                    w32(pv + 0xbc, u(ri(0, 4) == 0 ? 3 : ri(0, 2)));  // (3: a site the offer leaves alone)
                    if (coin()) { w32(0x0013cae4, 0x10u); w32(L6_ADDR(0x0015f594), 10); }
                    if (coin()) { w8(0x0015ee1c, 0); w8(0x0015ee1d, 0); }
                    else if (coin()) { w32(L6_ADDR(0x00179b90), 0); w32(L6_ADDR(0x00179bb4), 0xffffffffu); }  // (flags but not busy)
                    if (coin()) w16(0x00141c00, 0);
                    w32(pv + 0x110, 0);
                }
                if (st == 2 && coin()) {  // Ratchet in the box that starts the blast route
                    const int32_t b = ee_read<int32_t>(pv + 0xf0);
                    if (box_ok(b)) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(box_at(b) + 0x30);
                }
                if (st == 3) {
                    const int32_t route = ri(0, 4);
                    w32(pv + 0xc0, u(route));
                    seed_events(pv, route, pass == 2, pass != 0);
                    if (pass == 1) { wf(pv + 0xf4, coin() ? 1.0f : 1.0f - rf(0.0f, 0.0008f)); w32(pv + 0x114, ri(0, 2) != 0 ? 1u : 0u); }
                    const int32_t bot = ee_read<int32_t>(pv + 0x10c);
                    if (!moby_ok(bot)) w32(pv + 0x114, 0);
                }
                if (st == 6) {  // Ratchet about the infobot (its cutscene within 3.5)
                    const int32_t bot = ee_read<int32_t>(pv + 0x10c);
                    if (moby_ok(bot)) {
                        const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(bot) << 8);
                        hero_at(o, coin() ? rf(0.0f, 3.4f) : rf(3.6f, 12.0f), rf(-1, 1));
                    }
                }
                if (st == 7 && ee_read<uint32_t>(L6_ADDR(0x0015f5c4)) != 2) w8(0x0013d4de, 1);  // (into the cutscene 5, not the level change)
                if (st == 9) w32(L6_ADDR(0x0015f5c4), 2);  // (outside a scene it requests the level change)
                if (st == 8 && coin()) { w32(0x001415f8, 0); }  // (the movie refused: hero_lost)
                frame50();
            };
            if (ships.empty()) std::printf("%-26s no ship (class 1109) on this level\n", "class1109_ship");
            for (int st = 0; st < 10; ++st)
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1109_ship", 0x00302578, blg_class1109_update_l6, [st, pass, ship_prep](uint32_t m) { ship_prep(m, st, pass); }, 12);
            // (P6.3) the offer's choices taken (the button on its HUD message, Ratchet on the ship, no voice line busy): each site,
            // the checkpoints +0xf8 / +0xfc taken or not
            if (!ships.empty())
                classcheck("class1109_choice", 0x00302578, blg_class1109_update_l6, [ship_prep](uint32_t m) {
                    ship_prep(m, 1, 1);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w32(P + 0x2fc, m);
                    w16(P + 0x30e, 0);
                    w8(0x0015ee1c, 0);
                    w8(0x0015ee1d, 0);
                    w32(L6_ADDR(0x00162100), 0);
                    w32(0x0013cae4, 0x10u);
                    w32(L6_ADDR(0x0015f594), 10);
                    w32(pv + 0xbc, u(ri(0, 2)));
                    for (const uint32_t o : {0xf8u, 0xfcu})
                        w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint32_t>(pv + o), coin() ? 0xffu : 0u);
                }, 40);
            // (P6.3) the zones that move the ship: Ratchet off it, nothing asked, at the centre of the zone its site answers to
            if (!ships.empty())
                classcheck("class1109_zones", 0x00302578, blg_class1109_update_l6, [ship_prep](uint32_t m) {
                    ship_prep(m, 1, 0);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w32(P + 0x2fc, 0);
                    w32(L6_ADDR(0x00162100), 0);
                    w32(pv + 0x110, 0);
                    const int32_t site = ri(0, 2);
                    w32(pv + 0xbc, u(site));
                    static const uint32_t zones[] = {0x100, 0x108, 0x104, 0x108, 0x100, 0x104};
                    const int32_t b = ee_read<int32_t>(pv + zones[site * 2 + ri(0, 1)]);
                    if (b >= 0 && b < ee_read<int32_t>(OV(0x001600f0))) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(b) * 0x80 + 0x30);
                }, 40);

            if (!ships.empty()) {
                // the offer, directly: each site (3: left alone), the checkpoints +0xf8 / +0xfc taken or not, the voice line's
                // flags and its record 0x141c00, the choice before it (-1 .. 5)
                level();
                ignores(false, false, false);
                l6check("ship_offer_l6", 0x00302fd8, 40, Ret::None, [level, ships] {
                    level();
                    if (ri(0, 3) != 0) { w32(OV(0x00179410), 0); w32(OV(0x00179434), 0xffffffffu); w32(0x00151720, 0); w32(0x001516ec, 0xffffffffu); }  // (P6.3: the voice line free, sub_00203908 takes a request)
                    const uint32_t m = ships[static_cast<size_t>(ri(0, static_cast<int>(ships.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w32(pv + 0xbc, u(ri(0, 5) == 0 ? (coin() ? 3 : -1) : ri(0, 2)));
                    const uint32_t cp = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4);
                    for (const uint32_t off : {0xf8u, 0xfcu}) {
                        const int32_t i = ee_read<int32_t>(pv + off);
                        if (i >= 0 && i < 16) w8(cp + u(i), coin() ? 0xffu : 0u);
                    }
                    w32(pv + 0x9c, u(ri(-1, 5)));
                    w16(0x00141c00, coin() ? 0u : 1u);
                    w8(0x0015ee1c, coin() ? 1u : 0u);
                    w8(0x0015ee1d, ri(0, 2) == 0 ? 1u : 0u);
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { ship_offer_l6(a.i[0]); return uint64_t{0}; });

                // an explosion, directly: at a box's centre toward its first row (as the flight's events) or toward a random
                // point, 0, 1, 6 or a few flames, scales 6, 30 or any
                level();
                ignores(false, true, true);
                l6check("ship_explosion_l6", 0x00303748, 30, Ret::None, [level, ships, nboxes, box_at] {
                    level();
                    const uint32_t m = ships[static_cast<size_t>(ri(0, static_cast<int>(ships.size()) - 1))];
                    const uint32_t b = box_at(ri(0, nboxes > 0 ? nboxes - 1 : 0));
                    uint32_t dir = b;
                    if (coin()) {
                        dir = OUT + 0x10;
                        wfrand(dir, 4, -2, 2);
                    }
                    static const int32_t counts[] = {0, 1, 6, 2, 3};
                    static const float scales[] = {6.0f, 30.0f, 1.0f};
                    const int32_t count = counts[ri(0, 4)];
                    const float scale = ri(0, 3) == 0 ? rf(0.5f, 30.0f) : scales[ri(0, 2)];
                    seed_rand();
                    return Args{{m, b + 0x30, dir, u(count)}, {scale}};
                }, [](A a) { ship_explosion_l6(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.f[0]); return uint64_t{0}; });

                // a piece of the blasted group launched, directly: its velocity and spin (w lanes too) from the test area
                const int32_t group = ee_read<int32_t>(ee_read<uint32_t>(ships[0] + 0x78) + 0xcc);
                std::vector<uint32_t> pieces;
                for (uint32_t e = group_at(group); e != 0 && pieces.size() < 0x40; e += 2) {
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ee_read<uint32_t>(o + 0x78) != 0) pieces.push_back(o);
                    if (ee_read<int16_t>(e) < 0) break;
                }
                std::printf("INFO class1109 pieces %zu\n", pieces.size());
                if (pieces.empty()) {
                    std::printf("%-26s no piece in the ship's group\n", "ship_piece_launch_l6");
                } else {
                    level();
                    ignores(false, false, false);
                    l6check("ship_piece_launch_l6", 0x00300f70, 20, Ret::None, [level, pieces] {
                        level();
                        const uint32_t o = pieces[static_cast<size_t>(ri(0, static_cast<int>(pieces.size()) - 1))];
                        w8(o + 0x20, u(ri(0, 3)));
                        wfrand(OUT, 8, -3, 3);
                        return Args{{o, OUT, OUT + 0x10}, {}};
                    }, [](A a) { ship_piece_launch_l6(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
                }

                // the blast, directly: the group's pieces of other sizes (+0xc: the biggest and the others' speeds) and moved
                // about, the level's puff count 0x162190 as it is
                if (group_at(group) != 0) {
                    level();
                    ignores(false, true, true);
                    l6check("ship_blast_group_l6", 0x00303b68, 20, Ret::None, [level, ships, pieces, frame50] {
                        level();
                        const uint32_t m = ships[static_cast<size_t>(ri(0, static_cast<int>(ships.size()) - 1))];
                        for (const uint32_t o : pieces) {
                            if (coin()) wf(o + 0xc, rf(0.5f, 6.0f));
                            if (ri(0, 3) == 0) { Vec4 q = ee_ref<Vec4>(o + 0x10); q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-2, 2); ee_ref<Vec4>(o + 0x10) = q; }
                        }
                        frame50();
                        seed_rand();
                        return Args{{m}, {}};
                    }, [](A a) { ship_blast_group_l6(a.i[0]); return uint64_t{0}; });
                } else {
                    std::printf("%-26s the ship's group has no list\n", "ship_blast_group_l6");
                }

                // a flight, directly: each route, the progress at its start / the roll's 0.8 / its end, the roll, the sizes,
                // the route's camera events (explosions, the blast, at the point 0, the event before a cleared one)
                level();
                ignores(false, true, true);
                l6check("ship_flight_l6", 0x00303160, 60, Ret::Int32, [level, ships, fix_routes, flight_prep, seed_events, uid5_camera, frame50] {
                    level();
                    const uint32_t m = ships[static_cast<size_t>(ri(0, static_cast<int>(ships.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    fix_routes(pv);
                    const int32_t route = ri(0, 4);
                    flight_prep(pv);
                    seed_events(pv, route, coin(), true);
                    if (ri(0, 3) != 0) uid5_camera();
                    frame50();
                    seed_rand();
                    return Args{{m, u(route)}, {}};
                }, [](A a) { return r32(ship_flight_l6(a.i[0], static_cast<int32_t>(a.i[1])) ? 1u : 0u); });

                // the pieces retired, directly (state 0 with the blast's checkpoint taken): the pieces in any state
                if (group_at(group) != 0) {
                    level();
                    ignores(false, false, false);
                    l6check("ship_pieces_retire_l6", 0x00304028, 20, Ret::None, [level, ships, pieces] {
                        level();
                        const uint32_t m = ships[static_cast<size_t>(ri(0, static_cast<int>(ships.size()) - 1))];
                        for (const uint32_t o : pieces) if (coin()) w8(o + 0x20, u(ri(0, 3)));
                        return Args{{m}, {}};
                    }, [](A a) { ship_pieces_retire_l6(a.i[0]); return uint64_t{0}; });
                }
            }
        }
        // ---- stage 4 P6.3 (b2): the hand ports of classes 1118, 1123 and 1512 (blarg_valve_wheel.cpp, blarg_electric_arc.cpp,
        // blarg_smoke_sparks.cpp) and their helpers wheel_burst_l6 / arc_hit_test_l6, each against level 6's own code
        // (the level's box count, a box's address, a moby's index in the moby table, a random moby index of the level's)
        auto b2_box_ok = [](int32_t i) { return i >= 0 && i < ee_read<int32_t>(OV(0x001600f0)); };
        auto b2_box = [](int32_t i) { return ee_read<uint32_t>(OV(0x001600ec)) + u(i) * 0x80; };
        auto b2_index = [](uint32_t o) { return (o - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8; };
        auto b2_any_index = [] {
            const uint32_t n = (ee_read<uint32_t>(OV(0x0015ffe0)) - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8;
            return u(ri(0, static_cast<int>(n) - 1));
        };

        // class 1118, the geared wheel: its spin-up about 1 (the gears turned), Ratchet within 3.7 or not with the help line
        // free / his / another's and the button, the waits +0x40 at 0 / 1 / under 10 / over, the scene's rise across the
        // marked boxes (their +0x78 put inside the frame's rise window) and across 1.075, the camera settling at 2
        level();
        ignores(false, false, true);
        auto wheel_prep = [generic_prep, frame50, hero_at, uid5_camera, b2_box_ok, b2_box](uint32_t m, int st, int pass) {
            generic_prep(m, st, pass, blg_class1118_update_l6);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            if (st == 2) {
                const int r = ri(0, 3);
                wf(pv + 0x2c, r == 0 ? 1.0f : r == 1 ? rf(0.97f, 1.0f) : r == 2 ? rf(0.0f, 1.0f) : rf(0.995f, 1.0f));
                wf(pv + 0x30, rf(0.0f, 0.02f));
            }
            if (st == 3) {
                hero_at(m, ri(0, 2) != 0 ? rf(0.0f, 3.6f) : rf(3.8f, 10.0f), rf(-1, 1));
                { static const uint32_t owners[] = {0, 4, 0, 9}; w32(OV(0x0015f594), owners[ri(0, 3)]); }  // (the help line: free, the wheel's, another's)
                w32(0x0013cae4, ri(0, 2) != 0 ? 0x10u : (coin() ? 0u : 0x40u));
            }
            if (st == 4 || st == 5) {  // the wait: idle, ending, in its last 10 frames, before them
                const int r = ri(0, 4);
                w32(pv + 0x40, r == 0 ? 0u : r == 1 ? 1u : r == 2 ? u(ri(2, 9)) : r == 3 ? u(ri(10, 12)) : 30u);
            }
            if (st == 5) {  // the rise: below the hero's place (marked boxes put inside this frame's window), across 1.075, past it
                const int r = ri(0, 3);
                const float x = r == 0 ? rf(1.07f, 1.0749f) : r == 1 ? rf(1.08f, 1.9f) : rf(0.0f, 1.06f);
                const float v = rf(0.002f, 0.008f);
                wf(pv + 0x2c, x);
                wf(pv + 0x30, v);
                for (uint32_t k = 0; k < 9; ++k) {
                    const int32_t i = ee_read<int32_t>(pv + 0x8 + k * 4);
                    if (!b2_box_ok(i) || ri(0, 2) == 0) continue;
                    // (the burst's point in hundreds of degrees of the rise: inside (x, x + its step), or just short of it)
                    wf(b2_box(i) + 0x78, (x + (coin() ? rf(0.05f, 0.6f) : rf(-0.5f, 0.0f)) * v) * 1.7453293f);
                }
            }
            if (st == 6) {
                const int r = ri(0, 2);
                wf(pv + 0x2c, r == 0 ? 2.0f : r == 1 ? rf(1.98f, 2.0f) : rf(1.075f, 2.0f));
                wf(pv + 0x30, rf(0.0f, 0.034f));
            }
            if (st >= 4 && ri(0, 3) != 0) uid5_camera();
            frame50();
        };
        for (int st = 0; st < 8; ++st)
            for (int pass = 0; pass < 3; ++pass)
                classcheck("class1118_wheel", 0x00304098, blg_class1118_update_l6, [st, pass, wheel_prep](uint32_t m) { wheel_prep(m, st, pass); }, 12);
        // its bursts, directly: at a marked box's centre or a random point, the level's burst count 0x1621b8 at 0 / 1 / 2 or
        // its own
        l6check("wheel_burst_l6", 0x00304818, 40, Ret::None, [level, mobys_with, b2_box_ok, b2_box] {
            level();
            const std::vector<uint32_t> ws = mobys_with(0x00304098);
            const uint32_t m = ws.empty() ? 0u : ws[static_cast<size_t>(ri(0, static_cast<int>(ws.size()) - 1))];
            uint32_t at = T + 0x200;
            wfrand(at, 4, -50, 50);
            if (m != 0 && coin()) {
                const int32_t i = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x8 + u(ri(0, 8)) * 4);
                if (b2_box_ok(i)) at = b2_box(i) + 0x30;
            }
            { const int r = ri(0, 3); if (r != 3) w32(L6_ADDR(0x001621b8), u(r)); }
            seed_rand();
            return Args{{m, at}, {}};
        }, [](A a) { wheel_burst_l6(a.i[1]); return uint64_t{0}; });

        // class 1123, the electric arc: linked (+0xf10) to none, to another arc below / at its state 3, or to a moby of another
        // class; its strands' counts at 0 (drawn afresh) or running; its phase about 1; the camera word 0x167540 within 48
        // or not; its switch (+0xf14) none / set / not; its hum's slot (none, free, its own playing or not); the hero on
        // one of its hit lines; the sparks of state 3
        level();
        ignores(true, false, true);
        {
            const std::vector<uint32_t> arcs = mobys_with(0x003049f8);
            // the arc `m` moved so that one of its hit lines (+-x along its row 0, at +-0x1621ec along row 1, 0x1621f0 along
            // row 2 of its matrix +0xc0) passes through the hero moby (where the collision has it), the hero there too
            auto arc_on_hero = [](uint32_t m) {
                const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(0x0013f350 + 0x2080) + 0x10);
                ee_ref<Vec4>(0x0013f3d0) = h;
                const Vec4 r0 = ee_ref<Vec4>(m + 0xc0), r1 = ee_ref<Vec4>(m + 0xd0), r2 = ee_ref<Vec4>(m + 0xe0);
                const float t = rf(-0.9f, 0.9f) * ee_read<float>(L6_ADDR(0x001621e8)), y = (coin() ? 1.0f : -1.0f) * ee_read<float>(L6_ADDR(0x001621ec)),
                            z = ee_read<float>(L6_ADDR(0x001621f0)) - rf(0.2f, 0.8f);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x = h.x - (t * r0.x + y * r1.x + z * r2.x);
                q.y = h.y - (t * r0.y + y * r1.y + z * r2.y);
                q.z = h.z - (t * r0.z + y * r1.z + z * r2.z);
                ee_ref<Vec4>(m + 0x10) = q;
            };
            for (int st = 0; st < 5; ++st)
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1123_arc", 0x003049f8, blg_class1123_update_l6, [st, pass, generic_prep, frame50, sound_slot, arcs, arc_on_hero, b2_index, b2_any_index](uint32_t m) {
                        generic_prep(m, st, pass, blg_class1123_update_l6);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        if (st == 0 && coin()) w32(pv + 0xf10, coin() ? 0xffffffffu : b2_any_index());
                        if (st == 1) {
                            const int r = ri(0, 2);
                            if (r == 0 && arcs.size() > 1) {  // another arc, on (0 .. 2) or off (3, 4)
                                uint32_t o = m;
                                while (o == m) o = arcs[static_cast<size_t>(ri(0, static_cast<int>(arcs.size()) - 1))];
                                w32(pv + 0xf10, b2_index(o));
                                w8(o + 0x20, u(ri(0, 4)));
                            } else if (r == 1) {  // a moby of another class
                                uint32_t i = b2_any_index();
                                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                                for (int k = 0; k < 20 && ee_read<int16_t>(first + (i << 8) + 0xa6) == 0x463; ++k) i = b2_any_index();
                                w32(pv + 0xf10, i);
                            } else if (!arcs.empty()) {  // an arc (itself or another) in a random state
                                const uint32_t o = arcs[static_cast<size_t>(ri(0, static_cast<int>(arcs.size()) - 1))];
                                w32(pv + 0xf10, b2_index(o));
                                if (o != m) w8(o + 0x20, u(ri(0, 4)));
                            }
                        }
                        if (st == 2) {
                            w32(pv + 0xf18, sound_slot(m));
                            if (ri(0, 2) == 0) arc_on_hero(m);  // (first: the camera word below is placed about where it ends up)
                            for (uint32_t k = 0; k < 4; ++k) w32(pv + 0xf00 + k * 4, ri(0, 2) == 0 ? 0u : u(ri(1, 45)));
                            if (coin()) wf(L6_ADDR(0x001621fc), rf(0.005f, 0.05f));
                            {  // the phase: anywhere, or one step short of 1 (it wraps)
                                const float step = ee_read<float>(L6_ADDR(0x001621fc)) * ee_read<float>(0x0015ed6c);
                                wf(L6_ADDR(0x001621f8), coin() ? rf(0.0f, 1.0f) : 1.0f - step * rf(0.0f, 0.9f));
                            }
                            {  // the camera word: within 48 of the arc or beyond
                                Vec4 c = ee_ref<Vec4>(m + 0x10);
                                const float d = coin() ? rf(0.0f, 45.0f) : rf(50.0f, 90.0f), a = rf(-3.1f, 3.1f);
                                c.x += d * std::cos(a); c.y += d * std::sin(a);
                                if (ri(0, 3) != 0) ee_ref<Vec4>(L6_ADDR(0x00167540)) = c;
                            }
                            {  // its switch: none, or a moby with its +0xbc set or not
                                const int r = ri(0, 3);
                                if (r == 0) w32(pv + 0xf14, 0xffffffffu);
                                else {
                                    const int32_t own = ee_read<int32_t>(pv + 0xf14);
                                    const uint32_t i = own >= 0 && coin() ? u(own) : b2_any_index();
                                    w32(pv + 0xf14, i);
                                    w8(ee_read<uint32_t>(OV(0x0015ffd8)) + (i << 8) + 0xbc, coin() ? 1u : 0u);
                                }
                            }
                        }
                        frame50();
                    }, 12);
            // its hit test, directly: on the level's arcs, the hero moby on one of its lines or anywhere
            l6check("arc_hit_test_l6", 0x00305000, 40, Ret::None, [level, arcs, arc_on_hero, hero_at] {
                level();
                const uint32_t m = arcs.empty() ? ee_read<uint32_t>(OV(0x0015ffd8)) : arcs[static_cast<size_t>(ri(0, static_cast<int>(arcs.size()) - 1))];
                if (coin()) arc_on_hero(m);
                else if (coin()) hero_at(m, rf(0, 4), rf(-1, 2));
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { arc_hit_test_l6(a.i[0]); return uint64_t{0}; });
        }

        // class 1512, smoke or sparks: put where the view sees it (point_on_screen, the camera's present rows: a point
        // between the camera 0x166dc0 and the hero) or left off screen; set off first (+4) or not, sparks (+0) or smoke;
        // its timers +0x20 / +0x24 / +0x28 idle, ending or running (the bursts of 5 .. 9 sparks and of five puffs)
        level();
        ignores(false, true, true);
        {
            static int seen = 0, tried = 0;
            seen = 0;
            tried = 0;
            // the moby `m` moved to a point the view sees (true), its probe at OUT + 0x100
            auto on_screen = [](uint32_t m) {
                ++tried;
                // (the screen, the projection and the view set up first: the snapshot has none, as batch 517's level())
                if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
                screen_setup();
                projection_setup();
                render_view_setup();
                const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
                for (int k = 0; k < 200; ++k) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x = c.x + rf(-40, 40);
                    q.y = c.y + rf(-40, 40);
                    q.z = c.z + rf(-15, 15);
                    ee_ref<Vec4>(OUT + 0x100) = Vec4{q.x, q.y, q.z, 3.0f};
                    if (point_on_screen(OUT + 0x100, 64.0f) != -1) {
                        ee_ref<Vec4>(m + 0x10) = q;
                        ++seen;
                        return true;
                    }
                }
                return false;
            };
            for (int st = 0; st < 5; ++st)
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1512_smoke", 0x00308c68, blg_class1512_update_l6, [st, pass, generic_prep, frame50, on_screen](uint32_t m) {
                        if (ri(0, 4) != 0) on_screen(m);
                        generic_prep(m, st, pass, blg_class1512_update_l6);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        w32(pv + 0x0, coin() ? 1u : 0u);
                        if (st == 0) w32(pv + 0x4, coin() ? 1u : 0u);
                        for (const uint32_t o : {0x20u, 0x24u, 0x28u}) {  // idle, ending, running
                            const int r = ri(0, 3);
                            w32(pv + o, r == 0 ? 0u : r == 1 ? 1u : u(ri(2, 100)));
                        }
                        frame50();
                    }, 12);
            std::printf("%-26s on screen in %d of %d tries\n", "class1512_smoke", seen, tried);
        }
        // ---- stage 4 P6.3 (b1): the hand ports of classes 1105 (blarg_scientist.cpp), 1091 and 1551 (blarg_scene_props.cpp)
        // and 1348 (blarg_help_voices.cpp), each against level 6's own update
        {
            // the moby table's mobys of the group `g` (its list in the level's group table; none for a group without one)
            auto members = [](int32_t g) {
                std::vector<uint32_t> v;
                if (g < 0 || g >= 0x70) return v;
                const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4);
                for (uint32_t e = list; list != 0 && e < list + 0x80; e += 2) {
                    v.push_back(ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8));
                    if (ee_read<int16_t>(e) < 0) break;
                }
                return v;
            };
            // the hero at the centre of the box `box` (a valid one), else where he is; whether he was put there
            auto hero_in_box = [](int32_t box) {
                if (box < 0 || box >= 0x400) return false;
                ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(box << 7) + 0x30);
                return true;
            };
            // the level's live mobys of the class `cls`
            auto of_class = [](int16_t cls) {
                std::vector<uint32_t> v;
                for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                    if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<int16_t>(o + 0xa6) == cls) v.push_back(o);
                return v;
            };

            // (INFO) the door sets' class: the mobys the Scientist's pvars +0x160 / +0x164 and the Snagglebeast's +0xfc / +0x100 name
            level();
            for (const uint32_t fn : {0x00301070u, 0x002f9a28u})
                for (const uint32_t o : mobys_with(fn)) {
                    const uint32_t pv = ee_read<uint32_t>(o + 0x78);
                    for (const uint32_t off : fn == 0x00301070u ? std::vector<uint32_t>{0x160, 0x164} : std::vector<uint32_t>{0xfc, 0x100}) {
                        const int32_t i = ee_read<int32_t>(pv + off);
                        if (i < 0 || i > 0x2000) continue;
                        const uint32_t d = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(i) << 8);
                        std::printf("INFO door set named by L6:0x%08x's +0x%x: moby %d, class %d, state %u\n", fn, off, i,
                                    ee_read<int16_t>(d + 0xa6), ee_read<uint8_t>(d + 0x20));
                    }
                }

            // class 1105, the Scrawny Scientist: every state (6: the head alone); state 0 with 0x13d505 / 0x13d8ac, the talk
            // made to run (a record with no scene: the hero in reach facing him, the button, the group's creatures (class
            // 0x33b) down or not, the start delay at its edge), the talk's range box, Ratchet's sub-state 15, the talk's +4
            // about 2 (the Grindboots bought: the autosave's clock the host's on both sides), the scene's camera move about
            // 0.1 / 0.2 / 1 with the current camera none / of type 5, the help voice playing or not, the fades at their ends;
            // the head: Ratchet within 8 in front (moving or not), its timers at their edges, spots behind or steeply above /
            // below (the clamps), the big-head cheat, the floor band near the camera
            level();
            ignores(false, false, false);
            const std::time_t saved_clock = g_host_clock_fixed;
            g_host_clock_fixed = 1759658400;
            g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                             {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                             {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
            auto sci_prep = [generic_prep, frame50, hero_at, camera_near, uid5_camera, members, hero_in_box](uint32_t m, int st, int pass) {
                const uint32_t P = 0x0013f350;
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(0x0013d505, 0);
                if (st != 0) {  // (his state 0 run first: the talk set up, the door sets shut)
                    w8(m + 0x20, 0);
                    const uint32_t sp0 = ee_stack_ptr();
                    try { blg_class1105_update_l6(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                }
                // (the talk record and the door sets', box's and group's indices kept as the level has them: generic_prep's
                // moved counters would name other mobys)
                uint32_t keep[0x10], keep2[5];
                for (uint32_t k = 0; k < 0x10; ++k) keep[k] = ee_read<uint32_t>(pv + k * 4);
                for (uint32_t k = 0; k < 5; ++k) keep2[k] = ee_read<uint32_t>(pv + 0x160 + k * 4);
                generic_prep(m, 0, pass, blg_class1105_update_l6);
                for (uint32_t k = 0; k < 0x10; ++k) w32(pv + k * 4, keep[k]);
                for (uint32_t k = 0; k < 5; ++k) w32(pv + 0x160 + k * 4, keep2[k]);
                w8(m + 0x20, u(st));
                w8(0x0013d505, st == 0 && ri(0, 3) == 0 ? 1u : 0u);
                w32(0x0013d8ac, coin() ? 1u : 0u);
                w8(m + 0x53, ri(0, 2) == 0 ? 1u : 0u);
                w8(m + 0x31, coin() ? 1u : 0u);
                if (coin()) camera_near(m, 30);
                w32(L6_ADDR(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
                w32(P + 0x208c, ri(0, 2) == 0 ? 0xfu : ee_read<uint32_t>(P + 0x208c));
                w8(pv + 0x8, coin() ? 1u : 0u);
                w16(pv + 0x176, coin() ? 1u : 0u);
                { const int r = ri(0, 3); w16(pv + 0x174, r == 0 ? 0u : r == 1 ? 1u : u(ri(2, 45))); }
                // the hero: in the talk's range box, near him or far
                if (!(ri(0, 3) == 0 && hero_in_box(ee_read<int32_t>(pv + 0x168)))) hero_at(m, ri(0, 2) != 0 ? rf(0.3f, 8.0f) : rf(8.0f, 40.0f), rf(-0.5f, 1.5f));
                // the group's creatures: all down (gone / down), some standing, or as the level has them
                {
                    const std::vector<uint32_t> g = members(ee_read<int32_t>(pv + 0x170));
                    const int mode = ri(0, 2);
                    bool any33b = false;
                    for (const uint32_t o : g) any33b = any33b || ee_read<int16_t>(o + 0xa6) == 0x33b;
                    if (!g.empty() && !any33b && coin()) w16(g[0] + 0xa6, 0x33b);
                    for (const uint32_t o : g) {
                        if (mode == 0) { static const uint32_t down[] = {0xfe, 0xfd, 0x10}; w8(o + 0x20, down[ri(0, 2)]); }
                        else if (mode == 1 && coin()) { static const uint32_t up[] = {0, 3, 0xf, 0x10, 0xfe}; w8(o + 0x20, up[ri(0, 4)]); }
                    }
                }
                if (st == 1 && coin()) {  // the talk made to run: a record without a scene, the hero in reach facing him, the button
                    for (const uint32_t o : members(ee_read<int32_t>(pv + 0x170))) { static const uint32_t down[] = {0xfe, 0xfd, 0x10}; w8(o + 0x20, down[ri(0, 2)]); }
                    w16(pv + 0x174, coin() ? 0u : 1u);
                    w32(L6_ADDR(0x0015f5c4), 0);
                    w32(P + 0x22a8, 1);
                    if (ee_read<int32_t>(P + 0x2084) == 0x1d) w32(P + 0x2084, 0);
                    w8(pv + 0x9, 1);
                    w32(OV(0x00179110), 0);  // (the talk's start frame)
                    w32(0x0013cae4, ri(0, 3) != 0 ? 0x10u : 0u);
                    hero_at(m, rf(0.3f, 3.0f), 0.0f);
                    const Vec4 h = ee_ref<Vec4>(0x0013f3d0), q = ee_ref<Vec4>(m + 0x10);
                    wf(P + 0x98, std::atan2(q.y - h.y, q.x - h.x) + rf(-0.8f, 0.8f));
                    // the records reachable from the first (+6 / +0xc / +0xe); one with no scene (+4 -1), or none (+0x36 -1)
                    const uint32_t script = ee_read<uint32_t>(pv + 0x3c);
                    if (script != 0 && script < 0x02000000u) {
                        std::vector<int32_t> seen{0}, quiet;
                        for (size_t k = 0; k < seen.size() && seen.size() < 48; ++k) {
                            const uint32_t rec = script + u(seen[k]) * 0x1c;
                            if (ee_read<int16_t>(rec + 4) == -1) quiet.push_back(seen[k]);
                            for (const uint32_t f : {6u, 0xcu, 0xeu}) {
                                const int32_t nx = ee_read<int16_t>(rec + f);
                                if (nx >= 0 && nx < 64 && std::find(seen.begin(), seen.end(), nx) == seen.end()) seen.push_back(nx);
                            }
                        }
                        if (!quiet.empty()) {
                            const int32_t i = quiet[static_cast<size_t>(ri(0, static_cast<int>(quiet.size()) - 1))];
                            w16(pv + 0x36, u(i));
                            w8(pv + 0x8, ee_read<uint8_t>(script + u(i) * 0x1c + 0x10) & 1u);
                        } else {
                            w16(pv + 0x36, 0xffff);
                        }
                    }
                }
                if (st == 2) {
                    static const uint32_t said[] = {0xffff, 0, 1, 2, 2, 3};
                    w16(pv + 0x4, said[ri(0, 5)]);
                    const uint32_t cp = 0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0);
                    w8(cp, coin() ? 0xffu : 0u);
                    w32(0x0013d290 + 0xc0, coin() ? 0xffffffffu : ee_read<uint32_t>(0x0013d290 + 0xc0));  // (the autosave's slot)
                }
                if (st >= 3) {
                    const int r = ri(0, 5);
                    wf(pv + 0x178, r == 0 ? rf(0.0f, 0.1f) : r == 1 ? rf(0.09f, 0.11f) : r == 2 ? rf(0.19f, 0.21f) : r == 3 ? rf(0.2f, 0.99f) : r == 4 ? 1.0f : rf(0.97f, 1.0f));
                    wf(pv + 0x17c, rf(0.0f, 0.03f));
                    w32(L6_ADDR(0x00179b90), ri(0, 2) == 0 ? 1u : 0u);      // (a help voice playing)
                    w32(L6_ADDR(0x00179bb4), ri(0, 2) == 0 ? u(ri(0, 20)) : 0xffffffffu);
                    const int f = ri(0, 4);
                    wf(L6_ADDR(0x0015f3fc), f == 0 ? 1.0f : f == 1 ? rf(0.93f, 1.0f) : f == 2 ? 0.0f : f == 3 ? rf(0.0f, 0.07f) : rf(0.0f, 1.0f));
                    const int c = ri(0, 3);
                    if (c == 0) w32(L6_ADDR(0x00167580), 0);  // (no current camera)
                    else if (c == 1) {  // the current camera of type 5 (its pvars in the test area)
                        const uint32_t cam = OV(0x00167150) + u(ri(0, 3)) * 0xa0;
                        w16(cam + 0x86, 5);
                        w32(cam + 0x70, T);
                        wfrand(T + 0x80, 8, -50, 50);
                        w32(L6_ADDR(0x00167580), cam);
                    } else uid5_camera();
                }
                // the head: Ratchet's target in front of him or behind, level, high or low; his velocity; the timers; the spot
                {
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    const float yaw = ee_read<float>(m + 0x48) + (ri(0, 2) != 0 ? rf(-1.4f, 1.4f) : rf(1.7f, 4.5f));
                    const float d = rf(0.5f, 7.0f);
                    Vec4 t = q;
                    t.x += d * std::cos(yaw); t.y += d * std::sin(yaw);
                    { const int r = ri(0, 3); t.z += r == 0 ? rf(3.0f, 10.0f) : r == 1 ? -rf(3.0f, 10.0f) : rf(-0.5f, 2.0f); }
                    ee_ref<Vec4>(P + 0xd0) = t;
                    Vec4 v{0, 0, 0, 0};
                    if (coin()) { const float s = coin() ? 0.005f : 0.3f; v.x = rf(-s, s); v.y = rf(-s, s); v.z = rf(-s, s); }
                    ee_ref<Vec4>(P + 0x100) = v;
                    { const int r = ri(0, 3); w32(pv + 0x158, r == 0 ? 0u : r == 1 ? 1u : u(ri(2, 120))); }
                    { const int r = ri(0, 3); w32(pv + 0x15c, r == 0 ? 0u : r == 1 ? 1u : u(ri(2, 300))); }
                    if (coin()) {
                        Vec4 s = q;
                        const float a = rf(-3.1f, 3.1f), r = rf(0.5f, 8.0f);
                        s.x += r * std::cos(a); s.y += r * std::sin(a);
                        s.z += ri(0, 2) == 0 ? (coin() ? rf(4.0f, 12.0f) : -rf(4.0f, 12.0f)) : rf(-1.0f, 3.0f);
                        ee_ref<Vec4>(pv + 0x140) = s;
                    }
                    w8(0x0015edb0, ri(0, 3) == 0 ? 1u : 0u);
                }
                if (st == 1 && ri(0, 3) == 0) w8(m + 0x53, 0);
                frame50();
            };
            for (int st = 0; st < 7; ++st)
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1105_scientist", 0x00301070, blg_class1105_update_l6, [st, pass, sci_prep](uint32_t m) { sci_prep(m, st, pass); }, 12);
            g_extra_hooks.clear();
            g_host_clock_fixed = saved_clock;

            // class 1091 (and 1092 .. 1098, 1103), the floating piece: its states 0 .. 2 (and one past them), Ratchet held in a
            // scene (state 0x72) or not, the drift's w lane about 1, a moby without pvars
            level();
            ignores(false, false, false);
            for (int st = 0; st < 4; ++st)
                for (int pass = 0; pass < 3; ++pass)
                    classcheck("class1091_float", 0x00300df0, blg_class1091_update_l6, [st, pass, generic_prep, frame50](uint32_t m) {
                        generic_prep(m, st == 3 ? 0 : st, pass, blg_class1091_update_l6);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                        w8(m + 0x20, u(st == 3 ? ri(3, 9) : st));
                        w32(P + 0x2084, coin() ? 0x72u : ee_read<uint32_t>(P + 0x2084));
                        if (pv != 0 && st == 2) {
                            wfrand(pv, 3, -0.3f, 0.3f);
                            { const int r = ri(0, 3); wf(pv + 0xc, r == 0 ? rf(0.0f, 1.0f) : r == 1 ? 1.0f : r == 2 ? rf(1.0f, 1.05f) : rf(1.0f, 60.0f)); }
                            wfrand(pv + 0x10, 3, -0.1f, 0.1f);
                        }
                        if (pv != 0 && st == 1 && coin()) wf(pv + 0xc, ee_read<float>(m + 0x18) + rf(-1.0f, 1.0f));
                        if (ri(0, 7) == 0) w32(m + 0x78, 0);  // (no pvars: retired)
                        frame50();
                    }, 12);

            // class 1348, the help voices: its records 0x28 / 0x29 / 0x2a / 0x2e / 0x2f counted or not (0, 1, 0xffff), the record
            // 0x28's time against the game's clock about 18 s (0, under, over, and the time brought up), Ratchet in its boxes
            // +0x20 / +0x24, its group +0x18 all gone or not, its six mobys (in state 1, gone, standing, a slot -1 or another
            // moby), its moby +0x28 gone or not with or without the Taunter, the flag 0x13d3ad and its moby +0x2c's +0xbc
            level();
            ignores(false, false, false);
            for (int pass = 0; pass < 8; ++pass)
                classcheck("class1348_voices", 0x003083b0, blg_class1348_update_l6, [pass, generic_prep, frame50, members, hero_in_box](uint32_t m) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    uint32_t keep[0xc];
                    for (uint32_t k = 0; k < 0xc; ++k) keep[k] = ee_read<uint32_t>(pv + k * 4);
                    generic_prep(m, 0, pass % 3, blg_class1348_update_l6);
                    if (ri(0, 3) != 0) { w32(OV(0x00179410), 0); w32(OV(0x00179434), 0xffffffffu); w32(0x00151720, 0); w32(0x001516ec, 0xffffffffu); }  // (P6.3: the voice line free, sub_00203908 takes a request)
                    for (uint32_t k = 0; k < 0xc; ++k) w32(pv + k * 4, keep[k]);  // (its indices kept: they name mobys, a group, boxes)
                    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                    const int n = static_cast<int>((ee_read<uint32_t>(OV(0x0015ffe0)) - first) >> 8);
                    for (const uint32_t k : {0x28u, 0x29u, 0x2au, 0x2eu, 0x2fu}) {
                        const uint32_t rec = 0x00141968 + k * 8;
                        { const int r = ri(0, 3); w16(rec, r <= 1 ? 0u : r == 2 ? 1u : 0xffffu); }
                        w16(rec + 2, ri(0, 4) == 0 ? 0u : u(ri(1, 3000)));
                        w32(rec + 4, coin() ? 0u : (R() | 0x80000000u));
                    }
                    {  // the game's clock about the record 0x28's time + 18 s
                        const int32_t t = ee_read<uint16_t>(0x00141968 + 0x28 * 8 + 2);
                        const int r = ri(0, 4);
                        const int32_t d = r == 0 ? ri(0, 599) : r == 1 ? ri(600, 1079) : r == 2 ? ri(1079, 1081) : r == 3 ? ri(1081, 6000) : ri(-600, 0);
                        w32(0x0015eea4, u(std::max(0, t * 600 + d)));
                    }
                    if (coin()) hero_in_box(ee_read<int32_t>(pv + 0x20));
                    else if (coin()) hero_in_box(ee_read<int32_t>(pv + 0x24));
                    {
                        const std::vector<uint32_t> g = members(ee_read<int32_t>(pv + 0x18));
                        const bool all_gone = ri(0, 2) != 0;
                        for (const uint32_t o : g) w8(o + 0x20, all_gone ? (coin() ? 0xfeu : 0xfdu) : (coin() ? 0u : 0xfeu));
                    }
                    for (uint32_t k = 0; k < 6; ++k) {
                        const int r = ri(0, 7);
                        if (r == 0) w32(pv + k * 4, 0xffffffffu);
                        else if (r == 1 && n > 0) w32(pv + k * 4, u(ri(0, n - 1)));  // (another moby of the level)
                        const int32_t i = ee_read<int32_t>(pv + k * 4);
                        if (i < 0 || i >= n) continue;
                        static const uint32_t states[] = {1, 1, 0xfe, 0xfd, 0, 3};
                        if (ri(0, 3) != 0) w8(first + (u(i) << 8) + 0x20, states[ri(0, 5)]);
                    }
                    {
                        const int32_t i = ee_read<int32_t>(pv + 0x28);
                        if (i >= 0 && i < n) { static const uint32_t states[] = {0xfe, 0xfd, 0, 1, 3}; w8(first + (u(i) << 8) + 0x20, states[ri(0, 4)]); }
                    }
                    w8(0x0013d4c0 + 0xe, coin() ? 1u : 0u);  // (the Taunter owned)
                    w8(0x0013d3ad, ri(0, 3) == 0 ? 1u : 0u);
                    if (ri(0, 4) == 0) w32(pv + 0x2c, 0xffffffffu);
                    {
                        const int32_t i = ee_read<int32_t>(pv + 0x2c);
                        if (i >= 0 && i < n) w8(first + (u(i) << 8) + 0xbc, coin() ? 1u : 0u);
                    }
                    frame50();
                }, 20);

            // class 1551, the cutscene 5's smoke: its states 0 / 1 (and one past), the cutscene running or not, the scene's
            // number 5 or another, its frame about frame_scale_i(127), its third moby a class-750 moby, another class, or none
            level();
            ignores(false, false, true);
            {
                const std::vector<uint32_t> smokers = of_class(0x2ee);
                std::printf("INFO class-750 mobys %zu\n", smokers.size());
                for (int pass = 0; pass < 4; ++pass)
                    classcheck("class1551_smoke", 0x00309348, blg_class1551_update_l6, [pass, generic_prep, frame50, smokers](uint32_t m) {
                        generic_prep(m, 0, pass % 3, blg_class1551_update_l6);
                        w8(m + 0x20, ri(0, 5) == 0 ? 0u : (ri(0, 7) == 0 ? 2u : 1u));
                        w32(L6_ADDR(0x0015f5c4), ri(0, 4) == 0 ? 0u : 2u);
                        w32(L6_ADDR(0x0016d010), ri(0, 3) != 0 ? 5u : u(ri(0, 6)));
                        { const int r = ri(0, 3); w32(L6_ADDR(0x0016d014), u(r == 0 ? ri(0, 126) : r == 1 ? ri(126, 129) : r == 2 ? ri(151, 154) : ri(129, 400))); }
                        const int r = ri(0, 4);
                        if (r <= 2 && !smokers.empty()) w32(L6_ADDR(0x0016d160), smokers[static_cast<size_t>(ri(0, static_cast<int>(smokers.size()) - 1))]);
                        else if (r == 3) w32(L6_ADDR(0x0016d160), m);  // (another class: no smoke)
                        frame50();
                    }, 20);
            }
        }
        //@@P63_HELPERS
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
