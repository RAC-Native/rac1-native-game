// Oracle batch 117 (task 0350): level 6's (Blarg's Nebula G34) own moby classes on its real level data. Run with
// ORACLE_LEVEL=6 build/tests/ovl_oracle --batch 117 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 6's own function at its own address, the port the one registered for it (the
// transliterations blarg_classes_tr_N.cpp).

#include "ovl_oracle.h"

#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

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
        {"class156", 0x002bead0, blg_class156_update_l6, 0x0, false},
        {"class302", 0x002d85a0, blg_class302_update_l6, 0x0, false},
        {"class303", 0x002d8c20, blg_class303_update_l6, 0x0, false},
        {"class367", 0x002d9d10, blg_class367_update_l6, 0x0, false},
        {"class604", 0x002e11c8, blg_class604_update_l6, 0x0, false},
        {"class605", 0x002e1518, blg_class605_update_l6, 0x0, false},
        {"class615", 0x002e1940, blg_class615_update_l6, 0x0, false},
        {"class803", 0x002e6270, blg_class803_update_l6, 0x0, false},
        {"class805", 0x002e66e0, blg_class805_update_l6, 0x0, false},
        {"class827", 0x002e8678, blg_class827_update_l6, 0x2, false},
        {"class832", 0x002eaa40, blg_class832_update_l6, 0x0, false},
        {"class857", 0x002f0040, blg_class857_update_l6, 0x0, false},
        {"class911", 0x002f3ad8, blg_class911_update_l6, 0x0, false},
        {"class1016", 0x002f4638, blg_class1016_update_l6, 0x28, false},
        {"class1021", 0x002f4f00, blg_class1021_update_l6, 0x0, false},
        {"class1028", 0x002f55a0, blg_class1028_update_l6, 0x0, false},
        {"class1035", 0x002f6470, blg_class1035_update_l6, 0x0, false},
        {"class1038", 0x002f7288, blg_class1038_update_l6, 0x0, false},
        {"class1039", 0x002f7930, blg_class1039_update_l6, 0x0, false},
        {"class1040", 0x002f7ab8, blg_class1040_update_l6, 0x0, false},
        {"class1048", 0x002f7d78, blg_class1048_update_l6, 0x0, false},
        {"class1051", 0x002f9a28, blg_class1051_update_l6, 0x0, false},
        {"class1054", 0x002fbfb0, blg_class1054_update_l6, 0x0, false},
        {"class1061", 0x002fc640, blg_class1061_update_l6, 0x0, false},
        {"class1062", 0x002fd088, blg_class1062_update_l6, 0x0, false},
        {"class1066", 0x002fda30, blg_class1066_update_l6, 0x0, false},
        {"class1068", 0x002fdbd0, blg_class1068_update_l6, 0x0, false},
        {"class1083", 0x002ffcb0, blg_class1083_update_l6, 0x0, false},
        {"class1085", 0x00300b90, blg_class1085_update_l6, 0x0, false},
        {"class1091", 0x00300df0, blg_class1091_update_l6, 0x0, false},
        {"class1105", 0x00301070, blg_class1105_update_l6, 0x0, false},
        {"class1108", 0x00301b90, blg_class1108_update_l6, 0x0, false},
        {"class1109", 0x00302578, blg_class1109_update_l6, 0x280, false},
        {"class1118", 0x00304098, blg_class1118_update_l6, 0x10, false},
        {"class1123", 0x003049f8, blg_class1123_update_l6, 0x0, false},
        {"class1134", 0x003053a8, blg_class1134_update_l6, 0x6, false},
        {"class1302", 0x00307d30, blg_class1302_update_l6, 0x0, false},
        {"class1348", 0x003083b0, blg_class1348_update_l6, 0x0, false},
        {"class1504", 0x00308868, blg_class1504_update_l6, 0x0, false},
        {"class1512", 0x00308c68, blg_class1512_update_l6, 0x0, false},
        {"class1551", 0x00309348, blg_class1551_update_l6, 0x0, false},
        {"class1633", 0x003094f0, blg_class1633_update_l6, 0x0, false},
        {"class1655", 0x00309860, blg_class1655_update_l6, 0x0, false},
        {"class1660", 0x00309e08, blg_class1660_update_l6, 0x0, false},
        {"class1664", 0x00309fa8, blg_class1664_update_l6, 0x0, false},
        {"class1672", 0x0030a148, empty_update, 0x0, false},
        {"class1818", 0x0030ae88, blg_class1818_update_l6, 0x0, false},
        {"class1898", 0x0030be60, blg_class1898_update_l6, 0x0, false},
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
                    [type](A a) { return r32(a.i[0] != 0 ? blg_cam_type18_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
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
        l6check("cam0_hook", 0x003147b0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { blg_cam0_hook_l6(0); return uint64_t{0}; });
        l6check("cam_callback", 0x00314810, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { blg_cam_callback_l6(0); return uint64_t{0}; });
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
            }, [](A a) { return r32(blg_hook_2ea6a8_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
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
            {"hook_0_2ea5e0", 0x002ea5e0, 0x0, blg_hook_2ea5e0_u},
            {"hook_8_2ea738", 0x002ea738, 0x8, blg_hook_2ea738_u},
            {"hook_c_2ea7c8", 0x002ea7c8, 0xc, blg_hook_2ea7c8_u},
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

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
