// Oracle batch 117 (task 0350): level 6's (Blarg's Nebula G34) own moby classes on its real level data. Run with
// ORACLE_LEVEL=6 build/tests/ovl_oracle --batch 117 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 6's own function at its own address, the port the one registered for it (the
// transliterations blarg_classes_tr_N.cpp).

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
#include <game/enhance/advanced.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
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

    // ---- the level's own classes (transliterated by tools/codegen/generate_all.py): the level's own mobys of each class, the first
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
        }, [](A a) { return r32(blg_hook_2ea800_u(a.i[0], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
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
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
