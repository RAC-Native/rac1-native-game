// Oracle batch 124 (task 0357): level 11's (Pokitaru) own moby classes on its real level data. Run with
// ORACLE_LEVEL=11 build/tests/ovl_oracle --batch 124 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 11's own function at its own address, the port the one registered for it (the
// transliterations pokitaru_classes_tr_N.cpp, or the hand-written pokitaru_*.cpp; the "p111_" checks drive the hand-written
// classes of stage 4 P11.1 through their states).

#include "ovl_oracle.h"
#include "trial_guard.h"

#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/pokitaru_own.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_class1111.h>
#include <game/ovl/shared_class1179.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class295.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/empty_class_updates.h>
#include <game/lib/libcdvd.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <optional>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <string>
#include <map>
#include <unordered_set>

namespace rac1::oracle {

namespace {
// a hand-written class hook (moby, position, target, float) in the hook table's uniform form (as batch 119's): Pokitaru's
// copies of the Amoeboid's hook +0 and of the hooks that keep the carry state (carry hooks +4 / +8) run these
template <int32_t (*H)(uint32_t, uint32_t, uint32_t, float)>
TrRet hook_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                   float, float, float, float) {
    return TrRet{static_cast<uint32_t>(H(a0, a1, a2, f12)), 0.0f};
}
int32_t keep_state_hook4(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_keep_state_hook(m, a1, a2, f12, 4); }  // L11:0x316210
int32_t keep_state_hook8(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_keep_state_hook(m, a1, a2, f12, 8); }  // L11:0x316270

// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.

constexpr uint32_t MOBY_SPAWN_L11 = 0x00274668;

// (stage 5, task C2a) The checks named here reach mobys_in_sphere (the level's copy), whose cell list and vertices the port keeps on
// the stack (the original: 0x70003800, 0x70002000): A2's collision scratch ranges are left out of these checks, and only of
// them. They also hide what moby_pose_matrices (called for a posed moby) writes inside them: the joint positions at
// 0x70000000 .., read back by the primitive tests, so a wrong one still shows in the hit list.
// (Task C2b: the checks added to the list reach sphere_collide or capsule_collide, whose chunk buffers,
// vertices and cell walk the port keeps on the host stack too.)
bool c2a_spr_check(const char* name) {
    static const char* const kNames[] = {"class1242", "class1246", "mobys_in_sphere", "class114", "class1231", "class1319",
                                         "class1818", "class572", "hero_move_mode", "class1134", "class318", "class805",
                                         "p111_class114", "p111_class1156", "p111_class298", "p111_class318",
                                         "p112_class1157", "p112_class1218", "p112_class1219", "p112_class1231", "p112_class1242",
                                         "p112_f30f660", "p112_f30fe50", "p112_f310698", "p112_f3125a8", "p112_f3126f8", "p112_f312838",
                                         "p113_class1246", "p113_class1297", "p113_class1319"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is level 11's own function at `l9addr` (not a Veldin address to translate).
void l11check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B124_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 11) {
        std::printf("%-26s L11:0x%08x  skipped (level 11 only: run with ORACLE_LEVEL=11)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L11:0x%08x\n", name, l9addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2a_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_124() {
    if (g_only_batch != 124) return;
    g_batch = 124;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00174610) + k);
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
    // the level's mobys whose update function is `fn` (level 11's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 11) { l11check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L11:0x%08x  no moby on this level\n", name, fn); return; }
        l11check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
                ok = run_trial(port, m);
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
    if (g_oracle_level == 11) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l11.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, see docs/reversing/pokitaru_classes.md)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class23", 0x002cb668, pok_class23_update_l11, 0x4, false},
        {"class37", 0x002cb790, raritanium_pickup_update, 0x0, false},
        {"class65", 0x002cb810, pok_class65_update_l11, 0x0, false},
        {"class90", 0x002d0710, pok_class90_update_l11, 0x0, false},
        {"class114", 0x002d0fa8, pok_class114_update_l11, 0x9000, false},
        {"class298", 0x002f0e40, pok_class298_update_l11, 0x0, false},
        {"class318", 0x002f2518, pok_class318_update_l11, 0x0, false},
        {"class341", 0x002f30b0, class341_update, 0x0, false},
        {"class361", 0x002f3350, pok_class361_update_l11, 0x0, false},
        {"class572", 0x002f9998, amoeboid_update_l1, 0x0, false},
        {"class604", 0x002fc630, class604_update, 0x0, false},
        {"class605", 0x002fc980, zone_marker_update, 0x0, false},
        {"class615", 0x002fcda8, class615_update, 0x0, false},
        {"class758", 0x00300010, swingshot_target_update, 0x0, false},
        {"class805", 0x00300578, spawn_point_update_l1, 0x0, false},
        {"class819", 0x00302510, class819_update, 0x0, false},
        {"class830", 0x00302ae0, class830_update, 0x0, false},
        {"class832", 0x00303070, class832_update, 0x0, false},
        {"class1009", 0x003089d0, class1009_update, 0x0, false},
        {"class1017", 0x00309098, class1017_update, 0x0, false},
        {"class1034", 0x003094f8, class1034_update, 0x0, false},
        {"class1075", 0x00309ac0, pok_class1075_update_l11, 0x0, false},
        {"class1111", 0x0030b358, class1111_update, 0x0, false},
        {"class1134", 0x0030b730, gold_bolt_update, 0x4, false},
        {"class1156", 0x0030c788, pok_class1156_update_l11, 0x0, false},
        {"class1157", 0x0030d800, pok_class1157_update_l11, 0x0, false},
        {"class1158", 0x0030e088, pok_class1158_update_l11, 0x0, false},
        {"class1159", 0x0030e978, pok_class1159_update_l11, 0x0, false},
        {"class1178", 0x0030eb90, pok_class1178_update_l11, 0x0, false},
        {"class1179", 0x0030ee00, class1179_update, 0x0, false},
        {"class1180", 0x0030f2a8, pok_class1180_update_l11, 0x0, false},
        {"class1218", 0x0030f728, pok_class1218_update_l11, 0x0, false},
        {"class1219", 0x00310028, pok_class1219_update_l11, 0x0, false},
        {"class1231", 0x00310180, pok_class1231_update_l11, 0x0, false},
        {"class1242", 0x00313290, pok_class1242_update_l11, 0x0, false},
        {"class1246", 0x00314318, pok_class1246_update_l11, 0x2000, false},
        {"class1248", 0x00316320, pok_class1248_update_l11, 0x0, false},
        {"class1264", 0x003172c0, pok_class1264_update_l11, 0x0, false},
        {"class1265", 0x003181f0, pok_class1265_update_l11, 0x0, false},
        {"class1297", 0x00318b30, pok_class1297_update_l11, 0x0, false},
        {"class1319", 0x00319838, pok_class1319_update_l11, 0x6, false},
        {"class1350", 0x0031aa80, pok_class1350_update_l11, 0x0, false},
        {"class1524", 0x0031ab08, pok_class1524_update_l11, 0x0, false},
        {"class1633", 0x0031b758, class1633_update, 0x0, false},
        {"class1818", 0x0031c800, sandmouse_update, 0x0, false},
        {"class1859", 0x0031d7d8, class1844_update, 0x0, false},
        {"class1860", 0x0031d8d0, empty_update, 0x0, false},
        {"class1903", 0x0031e2f0, pok_class1903_update_l11, 0x0, false},
        {"class1919", 0x0031f150, pok_class1919_update_l11, 0x0, false},
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
    if (g_oracle_level == 11 && std::getenv("B124_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B124_CLS") != nullptr && std::strcmp(std::getenv("B124_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 11 && std::getenv("B124_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B124_ST") == nullptr || std::atoi(std::getenv("B124_ST")) == st) && (std::getenv("B124_PASS") == nullptr || std::atoi(std::getenv("B124_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 11's own moby callbacks (draw callbacks installed by `jal 0x1ecc00 / 0x1eca48`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 11) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        static const Cb cbs[] = {
        {"cb2f2d58", 0x002f2d58, pok_cb_2f2d58_l11, 0x002f2518, 318},
        {"cb2fd6c0", 0x002fd6c0, class615_puzzle_draw, 0x002fcda8, 615},
        {"cb302e60", 0x00302e60, class832_draw, 0x00303070, 832},
        {"cb30a908", 0x0030a908, class1111_draw, 0x0030b358, 1111},
        {"cb30e708", 0x0030e708, nov_water_751_draw_l1, 0x0030e088, 1158},
        {"cb311d50", 0x00311d50, pok_cb_311d50_l11, 0x00000000, 0},
        {"cb3192c8", 0x003192c8, class1319_trails_draw, 0x00000000, 0},
        {"cb31c728", 0x0031c728, sandmouse_glow, 0x00000000, 0},
        {"cb31db50", 0x0031db50, pok_cb_31db50_l11, 0x0031e2f0, 1903},
        {"cb31e930", 0x0031e930, pok_cb_31e930_l11, 0x0031f150, 1919},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B124_CLS") != nullptr && std::strcmp(std::getenv("B124_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L11:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            // cb2fd6c0 / cb311d50: their GIF packet words carry a callee-saved register (s7 / s?) the callback reads on a path that never set it (the original's interpreter holds the
            // harness' leftover there, the port's local 0): the packets (the cursor's buffer) are not compared for these two, everything else is
            const size_t ign_pk = g_ignored_words.size();
            if (std::strcmp(cb.name, "cb2fd6c0") == 0 || std::strcmp(cb.name, "cb311d50") == 0)
                for (uint32_t a = 0x00800000u; a < 0x00810000u; a += 4) g_ignored_words.push_back(a);
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : pok_class23_update_l11, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 11's sound emitter type 3 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 11) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x00328b50, emitter_fade_box_update}, {"emitter5", 0x00328ef0, emitter_camera_bed_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L11:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B124_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l11check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 11's class hooks (class 827's table at 0x1dde58): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 11) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2fb780", 0x2fb780, 0x0, hook_uniform<amoeboid_hook0_l1>},
            {"hook_2fb7f0", 0x2fb7f0, 0x4, pok_hook_2fb7f0_u},
            {"hook_2fb840", 0x2fb840, 0x8, pok_hook_2fb840_u},
            {"hook_2fb890", 0x2fb890, 0xc, pok_hook_2fb890_u},
            {"hook_2fb8c0", 0x2fb8c0, 0x10, pok_hook_2fb8c0_u},
            {"hook_316160", 0x316160, 0x0, pok_hook_316160_u},
            {"hook_316210", 0x316210, 0x4, hook_uniform<keep_state_hook4>},
            {"hook_316270", 0x316270, 0x8, hook_uniform<keep_state_hook8>},
            {"hook_3162d0", 0x3162d0, 0xc, pok_hook_3162d0_u},
            {"hook_3162f0", 0x3162f0, 0x10, pok_hook_3162f0_u},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L11:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B124_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l11check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- level 11's movement-mode hero update (L11:0x00237a58, the Novalis family's 0x3a8-byte build): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 11) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        l11check("hero_move_mode", 0x00237a58, 300, Ret::None, [level] {
            level();
            const uint32_t P = 0x0013f350;
            const int mode = ri(1, 3);
            w8(P + 0x20a4, u(mode));
            static const uint32_t st1[] = {0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
            static const uint32_t st2[] = {0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62};
            static const uint32_t st3[] = {0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x52};
            w32(P + 0x2090, mode == 1 ? st1[ri(0, 12)] : mode == 2 ? st2[ri(0, 8)] : st3[ri(0, 7)]);
            if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(P + 0x80); q.x += rf(-3, 3); q.y += rf(-3, 3); q.z += rf(-2, 2); ee_ref<Vec4>(P + 0x80) = q; }
            if (ri(0, 2) == 0) wfrand(P + 0x90, 3, -2, 2);
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
            return Args{};
        }, [](A) { hero_move_mode(); return uint64_t{0}; });
        g_ignored_words.pop_back();
        g_ignored_words.pop_back();
        g_extra_noop_hooks.clear();
    }

    // ---- moby_spawn (the Veldin port) against level 11's own copy (L11:0x...): the classes' spawns all go through it
    if (g_oracle_level == 11) {
        l11check("moby_spawn", MOBY_SPAWN_L11, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x17, 0x25, 0x41, 0x72, 0x12a, 0x25c, 0x4a1};  // (classes of level 11's own table: 23, 37, 65, 114, 298, 604, 1185)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 11's camera hook (0x327a90, the pointer its setter 0x327a58 puts at 0x167a10) and the callback it queues (0x2f4b78)
    if (g_oracle_level == 11) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00327a90u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l11check("cam0_hook", 0x00327a90, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l11check("cam_callback", 0x00327af0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 11's copy L11:0x002240d0) on the level's own mobys: class 794 (the Gadgebot's helper) calls it and one of its random states differed
    if (g_oracle_level == 11) {
        l11check("mobys_in_sphere", 0x002240d0, 600, Ret::Int32, [level] {
            level();
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe) ms.push_back(m);
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            const uint32_t OUT = 0x00180000u;
            const float r = rf(0.3f, 3);
            wf(OUT, ee_read<float>(m + 0x10) + rf(-3, 3)); wf(OUT + 4, ee_read<float>(m + 0x14) + rf(-3, 3));
            wf(OUT + 8, ee_read<float>(m + 0x18) + rf(-3, 3)); wf(OUT + 12, 0.0f);
            const uint32_t fl = (ri(0, 1) ? 4u : 0u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (ri(0, 2) ? 0u : 0x80u) | (u(ri(0, 0x1f)) << 8);
            return Args{{OUT, fl, ri(0, 3) ? 0u : m, 0u}, {r}};
        }, [](A a) { return r32(mobys_in_sphere(a.i[0], a.f[0], a.i[1], a.i[2], a.i[3])); });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 11) {
        static const uint32_t empties[] = {0x0031d8d0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l11check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- stage 4 P11.1: the level's own classes ported by hand (pokitaru_props.cpp, _talkers.cpp, _resort_owner.cpp,
    // _teleporter.cpp, _raft.cpp, _gate.cpp), each driven through the states and branches its code has, from the level's own
    // mobys and data: the hero placed at what each state tests, the level's words at their edges, the frame scale at its 50 Hz
    // value in a quarter of the cases, the dialogue stream idle in half (a talk / help request starts only then). The level
    // change the talkers ask for runs for real on both sides (its clock and disc reads the host's, as batch 514's); only the
    // kernel's calls stay no-ops. Without the class loop's lists: the debris / particle w lanes are compared here.
    // B124_INFO=1 prints the subjects; the cases the generator dealt again because the port threw are counted ("redealt").
    if (g_oracle_level == 11) {
        g_ignored_words = saved_ignored;
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
        const std::time_t saved_clock = g_host_clock_fixed;
        g_host_clock_fixed = 1759658400;
        const auto saved_hooks = g_extra_hooks;
        g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                         {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                         {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
        const uint32_t P = 0x0013f350, HERO = 0x0013f3d0;
        auto moby_base = [] { return ee_read<uint32_t>(OV(0x0015ffd8)); };
        auto mobys_of_class = [moby_base](uint32_t cls) {
            std::vector<uint32_t> v;
            for (uint32_t m = moby_base(); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint16_t>(m + 0xa6) == cls) v.push_back(m);
            return v;
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto index_of = [moby_base](uint32_t m) { return (m - moby_base()) >> 8; };
        auto box_at = [](int32_t i) { return ee_read<uint32_t>(OV(0x001600ec)) + static_cast<uint32_t>(i) * 0x80; };
        auto put_hero = [HERO](uint32_t at, float r, float dz) {  // the hero `r` away (in the plane) from the point `at`
            Vec4 q = ee_ref<Vec4>(at);
            const float a = rf(-3.1f, 3.1f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += dz;
            ee_ref<Vec4>(HERO) = q;
        };
        auto common = [P](uint32_t) {
            if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }  // (50 Hz)
            if (coin()) { w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu); }  // (the dialogue stream idle)
            if (ri(0, 2) == 0) wfrand(P + 0x100, 3, -0.3f, 0.3f);  // (the hero's velocity 0x13f450: moving or not)
            else wfrand(P + 0x100, 3, -0.001f, 0.001f);
            w8(0x0015edb0, ri(0, 3) == 0 ? 1u : 0u);
            w32(OV(0x0015f5c4), ri(0, 2) == 0 ? 2u : u(ri(0, 1)));  // (the scene's mode: 2 while a scene plays)
            w32(OV(0x0015f5cc), R() & 0xffff);
            w32(0x0013cae4, ri(0, 2) == 0 ? 0x10u : (coin() ? 0u : R() & 0xffff));  // (the pad's buttons)
            w16(P + 0x30e, coin() ? 0u : 1u);  // (0x13f65e: the hero busy)
            w32(0x001413d4, ri(0, 4) == 0 ? 0x32u : u(ri(0, 3)));
            seed_rand();
        };
        auto collectible = [](uint32_t index, bool taken) {  // the level's checkpoint `index` (0x14c050's row of the level; the word as the pvar has it)
            w8(0x0014c050 + index + (ee_read<uint32_t>(0x0015ed84) << 4), taken ? 0xffu : 0u);
        };
        // talk_update's way to its end without a scene or movie (the oracle's interpreter cannot start those): the talk on a record
        // without one (+4 -1; a script of four such records in the test area, our own data, when the talker's has none), the
        // button's wait over (+8), the hero at the talker
        auto talk_ready = [P](uint32_t m, uint32_t t) {
            const uint32_t script = ee_read<uint32_t>(t + 0x3c);
            int found = -1;
            if (script != 0)
                for (int k = 0; k < 24 && found < 0; ++k)
                    if (ee_read<int16_t>(script + static_cast<uint32_t>(k) * 0x1c + 4) == -1) found = k;
            if (found < 0) {
                for (uint32_t k = 0; k < 4; ++k) {
                    const uint32_t r = T3 + k * 0x1c;
                    for (uint32_t o = 0; o < 0x1c; o += 4) w32(r + o, 0);
                    w16(r + 4, 0xffffu); w16(r + 6, 0xffffu); w16(r + 0xc, 0xffffu); w16(r + 0xe, 0xffffu);
                }
                w32(t + 0x3c, T3);
                found = 0;
            }
            w16(t + 0x36, u(found));
            w8(t + 9, 1);
            w8(t + 8, 1);
            if (ee_read<int32_t>(P + 0x2084) == 0x1d) w32(P + 0x2084, 0);
            w32(P + 0x22a8, 1);
            w32(OV(0x0015f5c4), 0);
            w32(OV(0x00179110), 0);
            Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += 0.5f; ee_ref<Vec4>(P + 0x80) = q;
        };
        auto head = [P](uint32_t m, uint32_t pv, uint32_t watch, uint32_t wander) {  // the talkers' head: the timers at their edges
            static const uint32_t tv[] = {0u, 1u, 2u, 30u};
            w32(pv + watch, tv[ri(0, 3)]);
            w32(pv + wander, tv[ri(0, 3)]);
            if (coin()) wf(m + 0x48, std::atan2(ee_read<float>(P + 0xd0) - ee_read<float>(m + 0x10), ee_read<float>(P + 0xd4) - ee_read<float>(m + 0x14)) + rf(-1.0f, 1.0f));
            else wf(m + 0x48, rf(-3.1f, 3.1f));
        };
        // a class check on `subjects` with the state set up by `prep(m, st)` for st in `states`, the cases dealt again when the
        // port throws counted
        auto p111 = [level](const char* name, uint32_t fn, void (*port)(uint32_t), const std::vector<uint32_t>& subjects,
                            std::function<void(uint32_t, int)> prep, int states, int n) {
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
            if (subjects.empty()) { std::printf("%-26s L11:0x%08x  no moby on this level\n", name, fn); return; }
            if (std::getenv("B124_INFO") != nullptr) std::printf("INFO %s subjects %zu\n", name, subjects.size());
            static int redealt = 0;
            redealt = 0;
            for (int st = 0; st < states; ++st) {
                if (std::getenv("B124_ST") != nullptr && std::atoi(std::getenv("B124_ST")) != st) continue;
                l11check(name, fn, n, Ret::None, [level, subjects, prep, port, st] {
                    static std::vector<uint8_t> saved;
                    uint32_t m = 0;
                    for (int attempt = 0; attempt < 40; ++attempt) {
                        level();
                        m = subjects[static_cast<size_t>(ri(0, static_cast<int>(subjects.size()) - 1))];
                        prep(m, st);
                        uint8_t* ram = ee_get_ram_base();
                        saved.assign(ram, ram + 0x02000000);
                        const uint32_t stack_top = ee_stack_ptr();
                        const bool ok = run_trial(port, m);
                        ee_stack_ptr() = stack_top;
                        std::memcpy(ram, saved.data(), saved.size());
                        if (ok) break;
                        ++redealt;
                    }
                    return Args{{m}, {}};
                }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
            }
            std::printf("%-26s redealt %d\n", name, redealt);
        };
        auto first_run = [](void (*port)(uint32_t), uint32_t m) {  // the class's state 0 run once (its set-up)
            w8(m + 0x20, 0);
            const uint32_t sp0 = ee_stack_ptr();
            try { port(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        };
        level();
        const std::vector<uint32_t> c23 = mobys_of_class(23), c65 = mobys_of_class(65), c90 = mobys_of_class(90), c114 = mobys_of_class(114),
                                    c298 = mobys_of_class(298), c318 = mobys_of_class(318), c361 = mobys_of_class(361),
                                    c1075 = mobys_of_class(1075), c1156 = mobys_of_class(1156), c1246 = mobys_of_class(1246),
                                    c1179 = mobys_of_class(1179);
        if (std::getenv("B124_INFO") != nullptr) {
            for (const auto& [nm, v] : std::initializer_list<std::pair<const char*, const std::vector<uint32_t>*>>{{"23", &c23}, {"65", &c65}, {"90", &c90}, {"114", &c114}, {"298", &c298}, {"318", &c318}, {"361", &c361}, {"1075", &c1075}, {"1156", &c1156}, {"1246", &c1246}, {"1179", &c1179}})
                for (uint32_t m : *v) {
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    std::printf("INFO class %s moby %u uid %u at (%.1f, %.1f, %.1f) state %u\n", nm, (m - moby_base()) >> 8, ee_read<uint16_t>(m + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(m + 0x20));
                }
        }

        p111("p111_class23", 0x002cb668, pok_class23_update_l11, c23, [common, put_hero](uint32_t m, int st) {
            w8(m + 0x20, u(st));
            w8(0x0013d4c6, ri(0, 3) == 0 ? 1u : 0u);
            put_hero(m + 0x10, ri(0, 1) ? rf(0.0f, 1.9f) : rf(2.1f, 5.0f), rf(-1.0f, 1.0f));
            common(m);
        }, 4, 24);
        p111("p111_class65", 0x002cb810, pok_class65_update_l11, c65, [common](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 0x8000u);
            wf(pv + 4, rf(-3.1f, 3.1f));
            const float swing = coin() ? 0.2076941728591919f : -0.2076941728591919f;
            wf(m + 0x48, ri(0, 2) == 0 ? ee_read<float>(pv + 4) + swing : rf(-3.1f, 3.1f));
            wf(pv, ri(0, 2) == 0 ? 0.0f : rf(-0.05f, 0.05f));
            common(m);
        }, 4, 30);
        p111("p111_class361", 0x002f3350, pok_class361_update_l11, c361, [common, P](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            if (ri(0, 3) == 0) w32(pv, 0xffffffffu);
            common(m);
            wf(P + 0x88, rf(190.0f, 198.0f));
        }, 3, 24);
        auto talker90 = [common, put_hero, head, box_at, talk_ready](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            w8(0x0013d4c3, coin() ? 1u : 0u);
            w16(pv + 4, coin() ? 2u : u(ri(0, 4)));
            w8(m + 0x53, coin() ? 0u : 1u);
            if (st == 1 && coin()) put_hero(box_at(ee_read<int32_t>(pv + 0x40)) + 0x30, rf(0.0f, 0.5f), 0.0f);
            else put_hero(m + 0x10, ri(0, 1) ? rf(0.0f, 7.9f) : rf(8.1f, 14.0f), rf(-1.0f, 1.0f));
            common(m);
            head(m, pv, 0x184, 0x188);
            if (st == 1 && coin()) talk_ready(m, pv);
        };
        p111("p111_class90", 0x002d0710, pok_class90_update_l11, c90, talker90, 4, 40);
        // the segment record's word 0x173e9c (Veldin's address; written by segment_collide's port under the floor band, the hero's
        // placing and the floor probe) is -0 or +0 by the VU0 register the earlier checks left (a known difference of the engine port,
        // nothing reads it; as hero_move_mode's check below): out of the four checks whose classes reach those calls only (298, 318,
        // 1156, 114), each passing alone with it compared
        g_ignored_words.push_back(OV(0x00173e9c));
        p111("p111_class298", 0x002f0e40, pok_class298_update_l11, c298, [common, put_hero, head, collectible, talk_ready, P](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, u(st));
            w8(0x0013d50b, ri(0, 3) == 0 ? 1u : 0u);
            w8(0x0013d389, ri(0, 5) == 0 ? 1u : 0u);
            w8(0x0013d4e3, ri(0, 5) == 0 ? 1u : 0u);
            w16(pv + 4, coin() ? 2u : u(ri(0, 4)));
            w8(m + 0x53, coin() ? 1u : 0u);
            put_hero(m + 0x10, ri(0, 1) ? rf(0.0f, 3.9f) : rf(4.1f, 12.0f), ri(0, 3) == 0 ? rf(-6.0f, 6.0f) : rf(-1.0f, 1.0f));
            collectible(ee_read<uint8_t>(m + 0xb0), coin());
            static const uint16_t kept[] = {0, 1, 5, 40, 2000};
            w16(0x00141d9a, kept[ri(0, 4)]);
            const uint32_t now = ee_read<uint16_t>(0x00141d9a) * 600u + u(ri(-1500, 1500));  // (before the time kept, too: the time not kept again)
            w32(0x0015eea4, ri(0, 3) == 0 ? R() & 0xfffff : now);
            common(m);
            head(m, pv, 0x160, 0x164);
            if (st == 1 && ri(0, 2) == 0) talk_ready(m, pv);
            else if (st == 1 && coin()) {  // the skill point's clock and the checkpoint: the hero by it, the flags clear, the time kept about now
                w8(0x0013d389, 0); w8(0x0013d4e3, 0);
                put_hero(m + 0x10, rf(0.0f, 1.5f), rf(-3.8f, 3.8f));
                static const uint16_t kept2[] = {0, 0, 1, 2, 5, 40};
                w16(0x00141d9a, kept2[ri(0, 5)]);
                w32(0x0015eea4, ee_read<uint16_t>(0x00141d9a) * 600u + u(ri(-1500, 1500)));
            }
            if (ri(0, 2) == 0) { w8(m + 0x31, 1); Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-36.0f, 36.0f); ee_ref<Vec4>(L11_ADDR(0x001677c0)) = q; }
            (void)P;
        }, 4, 40);
        p111("p111_class318", 0x002f2518, pok_class318_update_l11, c318, [common, put_hero, collectible, pick, index_of, c318, P](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (st != 0 && ri(0, 4) != 0) { w8(m + 0x20, 0); const uint32_t sp0 = ee_stack_ptr(); try { pok_class318_update_l11(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; } }
            w8(m + 0x20, u(st));
            if (ri(0, 3) == 0) w32(pv + 8, 0xffffffffu);
            if (ee_read<int32_t>(pv + 8) >= 0) collectible(ee_read<uint32_t>(pv + 8), coin());
            if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 1u);
            w32(pv, ri(0, 5) == 0 ? 0xffffffffu : index_of(pick(c318)));
            w32(pv + 4, coin() ? 0xffffffffu : index_of(pick(c318)));
            collectible(ee_read<uint32_t>(pv + 0xc), coin());
            w32(pv + 0x30, coin() ? 0xffffffffu : u(ri(0, 3)));
            w32(pv + 0x34, coin() ? 0xffffffffu : u(ri(0, 3)));
            static const float fv[] = {0.0f, 0.2f, 0.5f, 1.0f, 1.0f};
            wf(pv + 0x20, fv[ri(0, 4)]);
            wf(pv + 0x24, fv[ri(0, 4)]);
            static const float gv[] = {0.0f, 0.0f, 1.0f, 1.95f, 2.0f};
            wf(pv + 0x3c, gv[ri(0, 4)]);
            put_hero(m + 0x10, rf(0.0f, 2.0f), ri(0, 2) == 0 ? -10.0f : 0.5f);
            if (st == 4) {  // (the fade's end: both values out, the glow up)
                if (coin()) wf(pv + 0x20, 0.0f);
                if (coin()) wf(pv + 0x24, 0.0f);
                if (coin()) wf(pv + 0x3c, 1.95f);
            }
            if (st == 1 || ri(0, 2) == 0) { static const uint32_t kinds[] = {0, 1, 3, 3, 5, 6}; w32(pv + 0x10, kinds[ri(0, 5)]); }  // (the prompt's kind)
            common(m);
            w32(P + 0x2fc, ri(0, 3) != 0 ? m : 0u);  // (0x13f64c: the moby the hero stands on)
            w32(OV(0x0015f594), ri(0, 2) != 0 ? 0xbu : u(ri(0, 10)));  // (the prompt shown)
            if (st == 2) { static const uint32_t hs[] = {0x64, 0x65, 0x66, 0x67, 0x67}; w32(0x001413d4, hs[ri(0, 4)]); }  // (the teleport's answer, 0x233f38)
        }, 5, 40);
        g_ignored_words.pop_back();
        auto sound_word = [](uint32_t m, uint32_t word) {  // a looping sound's slot word: none, or a slot playing for `m` or not
            const int k = ri(0, 4);
            if (k == 0) { w32(word, 0xffffffffu); return; }
            const uint32_t slot = u(ri(0, 15)), ch = 0x0013e550 + slot * 0x70;
            w32(word, slot);
            w32(ch + 0x88, k != 4 ? m : R());
            static const uint8_t cs[] = {0, 1, 2, 7};
            w8(ch + 0x74, cs[ri(0, 3)]);
        };
        auto zone_group = [c1246, pick](uint32_t m, uint32_t word, bool half) {  // the moby group a pvar half names: its class-1246 mobys near or not, in their states
            const int32_t g = half ? ee_read<int16_t>(word) : ee_read<int32_t>(word);
            if (g < 0 || g > ee_read<int32_t>(OV(0x0015fff4))) return;
            uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + static_cast<uint32_t>(g) * 4);
            if (e == 0) return;
            const bool calm = ri(0, 3) == 0;
            for (int k = 0; k < 64; ++k, e += 2) {
                const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                static const uint8_t sts[] = {0, 1, 0xb, 0xc, 0x12, 0x14, 0x80, 3};
                if (calm) w8(o + 0x20, sts[2 + ri(0, 4)]);  // (all harmless or gone)
                else if (ri(0, 2) == 0) w8(o + 0x20, sts[ri(0, 7)]);
                if (ri(0, 3) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-9.0f, 9.0f); q.y += rf(-9.0f, 9.0f); ee_ref<Vec4>(o + 0x10) = q; }
                if (ee_read<int16_t>(e) < 0) break;
            }
            (void)c1246; (void)pick;
        };
        p111("p111_class1075", 0x00309ac0, pok_class1075_update_l11, c1075, [common, put_hero, first_run, sound_word, zone_group, P](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (st != 0) first_run(pok_class1075_update_l11, m);
            w8(m + 0x20, u(st));
            sound_word(m, pv + 0xc8);
            sound_word(m, pv + 0xcc);
            const uint32_t path = ee_read<uint32_t>(pv + 0xc4);
            if (path != 0 && st >= 2) {
                const int32_t n = ee_read<int32_t>(path);
                const int32_t i = n > 1 ? (coin() ? n - 1 : ri(1, n - 1)) : 1;
                w32(pv + 0xd4, u(i));
                if (coin()) { Vec4 q = ee_ref<Vec4>(path + 0x10 + static_cast<uint32_t>(i) * 16); q.x += rf(-0.05f, 0.05f); q.y += rf(-0.05f, 0.05f); if (coin()) q = ee_ref<Vec4>(path + 0x10 + static_cast<uint32_t>(i) * 16); ee_ref<Vec4>(m + 0x10) = q; }
            }
            wf(pv + 0xd0, ri(0, 2) == 0 ? 0.0f : rf(0.0f, 1.0f));
            if (path != 0 && st == 2 && coin()) {  // the next point one the camera pulls back on (its w 37), when the path has one
                const int32_t n = ee_read<int32_t>(path);
                for (int32_t j = 2; j < n && j < 256; ++j)
                    if (ee_read<float>(path + 0x10 + static_cast<uint32_t>(j) * 16 + 0xc) == 37.0f) { w32(pv + 0xd4, u(j - 1)); break; }
            }
            if (path != 0 && st == 2 && ri(0, 2) == 0) {  // about the point's reach (the level's speed a frame), not moving
                const Vec4 t = ee_ref<Vec4>(path + 0x10 + ee_read<uint32_t>(pv + 0xd4) * 16);
                const float thr = ee_read<float>(L11_ADDR(0x00161dd4)) * ee_read<float>(0x0015ed6c);
                Vec4 q = t; q.x += thr * rf(0.3f, 2.5f); ee_ref<Vec4>(m + 0x10) = q;
                wf(pv + 0xd0, 0.0f);
            }
            if (path != 0 && st == 4 && coin()) {  // about the rise's end
                const Vec4 t = ee_ref<Vec4>(path + 0x10 + ee_read<uint32_t>(pv + 0xd4) * 16);
                wf(pv + 0xe4, t.z + rf(5.6f, 6.8f));  // (its height, put back from +0xe4 first)
                wf(pv + 0xd0, 0.0f);
            }
            w32(pv + 0xc0, coin() ? 1u : 0u);
            w32(pv + 0xb0, coin() ? 1u : 0u);
            w32(pv + 0xb8, coin() ? 1u : 0u);
            zone_group(m, pv + 0xb4, true);
            put_hero(m + 0x10, rf(0.0f, 3.0f), coin() ? -3.0f : 3.0f);
            wf(m + 0x18, ri(0, 3) == 0 ? 230.0f : ee_read<float>(m + 0x18));
            if (ri(0, 2) != 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-36.0f, 36.0f); ee_ref<Vec4>(L11_ADDR(0x001677c0)) = q; }  // (the camera near: the spray)
            common(m);
            w32(P + 0x2fc, coin() ? m : 0u);
        }, 6, 30);
        g_ignored_words.push_back(OV(0x00173e9c));  // (the floor probe and the hero's placing, as class 298's)
        p111("p111_class1156", 0x0030c788, pok_class1156_update_l11, c1156, [common, first_run, index_of, pick, c1179](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (st != 0) first_run(pok_class1156_update_l11, m);
            w8(m + 0x20, u(st));
            if (!c1179.empty() && coin()) w32(pv, index_of(pick(c1179)));
            if (ee_read<int32_t>(pv) >= 0) w8(ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv) << 8) + 0x20, coin() ? 2u : u(ri(0, 3)));
            static const float pg[] = {0.0f, 0.5f, 0.95f, 1.0f, 1.0f};
            wf(pv + 0xc, pg[ri(0, 4)]);
            static const float ov[] = {0.0f, 0.0f, 0.45f, 0.6f, 0.95f, 1.0f};
            for (uint32_t a = 0x00161ec4; a <= 0x00161ed8; a += 4) wf(L11_ADDR(0x00161ec4) + (a - 0x00161ec4), ov[ri(0, 5)]);
            wf(L11_ADDR(0x00161edc), rf(0.0f, 1.0f));
            if (st == 3 && ri(0, 2) == 0) { wf(L11_ADDR(0x00161ec4), coin() ? 0.95f : 0.85f); wf(L11_ADDR(0x00161ec8), 0.0f); }
            if (st == 3 && ri(0, 2) == 0) { wf(L11_ADDR(0x00161ec8), 0.6f); wf(L11_ADDR(0x00161ecc), 0.0f); }
            if (st == 3 && ri(0, 2) == 0) { wf(L11_ADDR(0x00161ec8), 1.0f); wf(L11_ADDR(0x00161ed0), 0.5f); wf(L11_ADDR(0x00161ecc), 1.0f); wf(L11_ADDR(0x00161ed4), 0.5f); }
            w8(0x0013d3de, coin() ? 1u : 0u);
            w8(m + 0xbc, coin() ? 1u : 0u);
            common(m);
        }, 5, 30);
        p111("p111_class114", 0x002d0fa8, pok_class114_update_l11, c114, [common, put_hero, collectible, first_run, zone_group, box_at, talk_ready, P](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (st != 0 && ri(0, 3) != 0) first_run(pok_class114_update_l11, m);
            w8(m + 0x20, u(st));
            const uint32_t zone = u(ri(0, 3));
            w32(pv + 0x158, zone);
            static const uint32_t kinds[] = {3, 4, 8, 7};
            w32(pv + 0x15c, kinds[ri(0, 3)]);
            w8(0x0013d4ee, ri(0, 3) == 0 ? 1u : 0u);
            collectible(ee_read<uint32_t>(pv + 0x108), coin());
            collectible(ee_read<uint32_t>(pv + 0x10c), coin());
            collectible(ee_read<uint32_t>(pv + 0x118), coin());
            w16(pv + 0x56, coin() ? 2u : 0u);
            if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) ^ 2u);
            for (int k = 0; k < 2; ++k) {  // the switches it carries: placed (state 5) or not, held by the hero or not
                const uint32_t sw = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0xf0 + 4u * u(k)) << 8);
                w8(sw + 0x20, ri(0, 2) == 0 ? 5u : u(ri(0, 2)));
            }
            const uint32_t door = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0x100) << 8);
            w8(door + 0x20, coin() ? 5u : 0u);
            const uint32_t scene = OV(0x0016c860);
            if (coin()) { w32(scene + 0x30, 1); w32(scene + 0x34, coin() ? 2000u : 1999u); }
            if (ri(0, 2) == 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f); ee_ref<Vec4>(pv + 0x120) = q; }
            if ((st >= 4 && st <= 6) && coin()) {  // in one of its boxes (pvar +0xf8 / +0xfc), facing a random way
                const Vec4 b = ee_ref<Vec4>(box_at(ee_read<int32_t>(pv + 0xf8 + 4u * u(ri(0, 1)))) + 0x30);
                ee_ref<Vec4>(m + 0x10) = b;
            }
            zone_group(m, pv + 0x90 + zone * 16, true);
            const int where = ri(0, 4);
            if (where == 0) put_hero(box_at(ee_read<int32_t>(pv + 0xe4)) + 0x30, rf(0.0f, 14.0f), 0.0f);
            else put_hero(m + 0x10, where == 1 ? rf(0.0f, 2.9f) : where == 2 ? rf(3.1f, 5.9f) : where == 3 ? rf(6.1f, 7.9f) : rf(8.1f, 20.0f), rf(-1.0f, 1.0f));
            if (st == 9 && coin()) {  // the hero holding the switch of its zone
                const uint32_t sw = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0xf0 + (zone != 0 ? 4u : 0u)) << 8);
                w32(P + 0x2fc, sw);
                w8(sw + 0x20, 1);
            }
            wf(m + 0x54, rf(0.0f, 1.0f));
            if (ri(0, 3) == 0) w16(pv + 0x90 + zone * 16, 0xffffu);  // (no group in its zone: no enemy left)
            if (coin()) w8(m + 0x53, u(ri(0, 5)));
            for (int k = 0; k < 2; ++k) {
                const uint32_t sw = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0xf0 + 4u * u(k)) << 8);
                if (coin()) w8(sw + 0x53, u(ri(0, 2)));
            }
            const uint32_t gate = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0x104) << 8);
            if (coin()) {  // the gate set up (its twin spawned), then closed and waiting
                if (ri(0, 3) != 0) { w8(gate + 0x20, 0); const uint32_t sp0 = ee_stack_ptr(); try { pok_class65_update_l11(gate); } catch (const std::exception&) { ee_stack_ptr() = sp0; } }
                w8(gate + 0x20, 1);
                const uint32_t twin = ee_read<uint32_t>(ee_read<uint32_t>(gate + 0x78) + 8);
                if (twin != 0 && coin()) w8(twin + 0x20, 1);
            }
            if (ri(0, 3) == 0) w32(ee_read<uint32_t>(gate + 0x78) + 8, 0);  // (a gate without its twin)
            if (ri(0, 3) == 0) { w8(m + 0x31, 1); Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-42.0f, 42.0f); ee_ref<Vec4>(L11_ADDR(0x001677c0)) = q; }
            wf(pv + 0x148, rf(-0.01f, 0.02f));
            if (coin()) wfrand(pv + 0x130, 3, -0.1f, 0.1f);
            if (st == 7) {  // its jump: the animation's time about the frames 9 and 29, its step
                w8(m + 0x50, 1); w8(m + 0x51, 0);
                wf(m + 0x54, rf(5.0f, 35.0f));
                wf(m + 0x58, rf(0.5f, 3.0f)); wf(m + 0x5c, rf(1.0f, 20.0f));
            }
            if (st >= 4 && st <= 6 && coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + (coin() ? rf(0.4f, 0.8f) : rf(0.6f, 2.0f)));  // (above the floor in front: about the 0.5 a drop needs)
            if (st == 6 && coin()) ee_ref<Vec4>(m + 0x10) = ee_ref<Vec4>(box_at(ee_read<int32_t>(pv + 0xe0 + zone * 4)) + 0x30);
            if (st >= 4 && st <= 7 && zone >= 2 && coin()) {  // next to one of the doors' walls (paths pvar +0x80 ..): pushed out of it
                const int32_t wall = ee_read<int32_t>(pv + 0x80 + 4u * u(ri(0, zone == 2 ? 2 : 1)));
                if (wall >= 0 && wall < ee_read<int32_t>(OV(0x00160104))) {
                    const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + static_cast<uint32_t>(wall) * 4);
                    const int32_t n = ee_read<int32_t>(path);
                    if (n > 0) { Vec4 q = ee_ref<Vec4>(path + 0x10 + static_cast<uint32_t>(ri(0, n - 1)) * 16); q.x += rf(-0.25f, 0.25f); q.y += rf(-0.25f, 0.25f); q.z = ee_read<float>(m + 0x18); ee_ref<Vec4>(m + 0x10) = q; }
                }
                if (st == 7) w32(pv + 0x15c, 3);
            }
            common(m);
            if (st == 9 && ri(0, 2) != 0) w16(P + 0x30e, 0);
            if ((st == 1 || st == 12 || st == 14) && coin()) { if (st == 1) w16(pv + 0x56, 0); talk_ready(m, pv + 0x20); }
            if (st >= 3 && st <= 5 && coin()) {  // its zone's enemies (the row's groups) about 10 from it, alive
                for (uint32_t k = 0; k < 4; ++k) {
                    const int32_t g = ee_read<int16_t>(pv + 0x90 + zone * 16 + k * 4);
                    if (g < 0 || g > ee_read<int32_t>(OV(0x0015fff4))) break;
                    uint32_t e = ee_read<uint32_t>(OV(0x001ab840) + static_cast<uint32_t>(g) * 4);
                    for (int i = 0; e != 0 && i < 64; ++i, e += 2) {
                        const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                        const float r = rf(8.5f, 13.0f), an = rf(-3.1f, 3.1f);
                        Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += r * std::cos(an); q.y += r * std::sin(an); ee_ref<Vec4>(o + 0x10) = q;
                        w8(o + 0x20, 1);
                        if (ee_read<int16_t>(e) < 0) break;
                    }
                }
            }
            if (st >= 3 && st <= 5 && zone == 2 && coin()) {  // a door's wall between it and the hero: the way round it
                const int32_t wall = ee_read<int32_t>(pv + 0x80 + 4u * u(ri(0, 2)));
                if (wall >= 0 && wall < ee_read<int32_t>(OV(0x00160104))) {
                    const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + static_cast<uint32_t>(wall) * 4);
                    const int32_t n = ee_read<int32_t>(path);
                    if (n > 0) {
                        const Vec4 w = ee_ref<Vec4>(path + 0x10 + static_cast<uint32_t>(ri(0, n - 1)) * 16);
                        const float dx = rf(-2.0f, 2.0f), dy = rf(-2.0f, 2.0f), k = rf(1.0f, 4.0f);
                        Vec4 q = w; q.x += dx; q.y += dy; q.z = ee_read<float>(m + 0x18); ee_ref<Vec4>(m + 0x10) = q;
                        Vec4 h = w; h.x -= dx * k; h.y -= dy * k; h.z = q.z; ee_ref<Vec4>(P + 0x80) = h;
                    }
                }
            }
        }, 16, 40);
        g_ignored_words.pop_back();

        // ---- stage 4 P11.2: Pokitaru's sewer set pieces (pokitaru_sewer.cpp: 1157, 1158, 1159, 1178, 1180 + 0x30dd18),
        // parachute drops and energy-ball thrower (pokitaru_crates.cpp: 1218, 1219, 1231 + 0x30fd98, 0x30f660, 0x30fe50,
        // 0x318e78, 0x318ed8, 0x318d10, 0x318e98, 0x310698), and fighter jet (pokitaru_jet.cpp: 1242 + 0x3125a8, 0x309378,
        // 0x3126f8, 0x313f60, 0x312838, 0x31a758, 0x31a7d8), plus the 1.02 forks (Gameplay 10, Fixes 35 & 36).
        {
            const std::vector<uint32_t> c1157 = mobys_of_class(1157), c1158 = mobys_of_class(1158), c1159 = mobys_of_class(1159),
                                        c1178 = mobys_of_class(1178), c1180 = mobys_of_class(1180), c1231 = mobys_of_class(1231),
                                        c1242 = mobys_of_class(1242), c1248 = mobys_of_class(1248), c1319 = mobys_of_class(1319),
                                        c615 = mobys_of_class(615), c830 = mobys_of_class(830);
            auto push_spawn_w_ignores = [&] {
                const size_t mark = g_ignored_words.size();
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
                const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
                for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
                return mark;
            };
            auto p112_spawned = [level](const char* name, uint32_t fn, void (*port)(uint32_t),
                                        std::function<uint32_t(int)> make_and_prep, int states, int n) {
                if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
                static int redealt = 0;
                redealt = 0;
                for (int st = 0; st < states; ++st) {
                    if (std::getenv("B124_ST") != nullptr && std::atoi(std::getenv("B124_ST")) != st) continue;
                    l11check(name, fn, n, Ret::None, [level, make_and_prep, port, st] {
                        static std::vector<uint8_t> saved;
                        uint32_t m = 0;
                        for (int attempt = 0; attempt < 40; ++attempt) {
                            level();
                            m = make_and_prep(st);
                            uint8_t* ram = ee_get_ram_base();
                            saved.assign(ram, ram + 0x02000000);
                            const uint32_t stack_top = ee_stack_ptr();
                            const bool ok = run_trial(port, m);
                            ee_stack_ptr() = stack_top;
                            std::memcpy(ram, saved.data(), saved.size());
                            if (ok) break;
                            ++redealt;
                        }
                        return Args{{m}, {}};
                    }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
                }
                std::printf("%-26s redealt %d\n", name, redealt);
            };
            auto set_anim_at = [](uint32_t m, uint32_t set, float target_frame) {
                moby_anim_set(m, set, 0, 0);
                const uint32_t key = ee_read<uint32_t>(m + 0x68);
                const float kframe = key != 0 ? static_cast<float>(ee_read<int16_t>(key + 4)) * 0.0625f : 0.0f;
                w8(m + 0x52, u(set));
                w8(m + 0x53, u(set));
                w8(m + 0x50, 1);
                w8(m + 0x51, 0);
                wf(m + 0x58, 1.0f);
                wf(m + 0x5c, 1.0f);
                wf(m + 0x54, target_frame - kframe);
            };
            auto sound_half = [moby_base](uint32_t m, uint32_t half_addr) {
                const int k = ri(0, 3);
                if (k == 0) { w16(half_addr, 0xffffu); return; }
                const uint32_t slot = u(ri(0, 15)), ch = 0x0013e550u + slot * 0x70u;
                w16(half_addr, slot);
                w32(ch + 0x88, k != 3 ? m : moby_base());
                w8(ch + 0x74, coin() ? 1u : 0u);
            };

            // class 1157 (sewer iris gate) & pok_30dd18_l11 (its 24 blades)
            g_ignored_words.push_back(OV(0x00173e9c));
            p111("p112_class1157", 0x0030d800, pok_class1157_update_l11, c1157, [common, first_run, moby_base](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (st != 0) first_run(pok_class1157_update_l11, m);
                w8(m + 0x20, u(st));
                const uint32_t trig = moby_base() + (ee_read<uint32_t>(pv + 0x60) << 8);
                static const uint8_t bcs[] = {0, 1, 2};
                w8(trig + 0xbc, bcs[ri(0, 2)]);
                static const float fades[] = {-1.0f, -0.02f, -0.01f, 0.4f, 0.98f, 1.0f};
                wf(pv + 0x78, fades[ri(0, 5)]);
                static const float opens[] = {-0.2f, 0.0f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f, 1.2f};
                wf(pv + 0x6c, opens[ri(0, 7)]);
                wf(pv + 0x70, rf(0.0f, 0.08f));
                wf(m + 0x48, rf(-3.1f, 3.1f));
                common(m);
            }, 6, 24);
            g_ignored_words.pop_back();

            if (!c1157.empty()) {
                l11check("p112_f30dd18", 0x0030dd18, 48, Ret::None, [level, c1157, first_run] {
                    level();
                    const uint32_t m = c1157[0];
                    first_run(pok_class1157_update_l11, m);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    static const float opens[] = {-0.3f, 0.0f, 0.2f, 0.5f, 0.8f, 1.0f, 1.3f};
                    wf(pv + 0x6c, opens[ri(0, 6)]);
                    wf(m + 0x48, rf(-3.1f, 3.1f));
                    return Args{{m}, {}};
                }, [](A a) { pok_30dd18_l11(a.i[0]); return uint64_t{0}; });
            }

            // class 1158 (sewer rising water tiles)
            p111("p112_class1158", 0x0030e088, pok_class1158_update_l11, c1158, [common, first_run, c1158, HERO](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (st != 0) {
                    w32(L11_ADDR(0x00161f64u), 0u);
                    first_run(pok_class1158_update_l11, c1158[0]);
                    if (m != c1158[0]) first_run(pok_class1158_update_l11, m);
                }
                w8(m + 0x20, u(st));
                if (st == 0) {
                    w32(L11_ADDR(0x00161f64u), coin() ? 0u : u(ri(1, 13)));
                } else if (ri(0, 2) == 0) {
                    w32(pv + 4, 0u);
                }
                w8(m + 0xbc, (coin() ? 8u : 0u) | (coin() ? 4u : 0u));
                w32(pv + 8, coin() ? 1u : 0u);
                wf(pv, coin() ? 0.0f : rf(-0.1f, 0.1f));
                if (st == 2) wf(m + 0x18, coin() ? 136.98f : rf(132.0f, 137.2f));
                else wf(m + 0x18, coin() ? 132.38f : rf(132.0f, 137.2f));
                wf(L11_ADDR(0x001677c8u), coin() ? 150.0f : 232.0f);
                if (coin()) {  // on-screen vs off-screen relative to the current camera frustum
                    Vec4 q = ee_ref<Vec4>(HERO);
                    q.z += 0.5f;
                    ee_ref<Vec4>(m + 0x10) = q;
                    moby_update_transform(m);
                }
                common(m);
                if (ri(0, 2) == 0) {  // seed libc PRNG to hit rand_mod(200) == 0 splash in water_move_and_splash
                    for (uint32_t s = 1; s < 1024; ++s) {
                        libc_srand(s);
                        if (rand_mod(0xc8) == 0) { libc_srand(s); break; }
                    }
                }
            }, 4, 30);

            // class 1159 (sliding sewer plug & twin)
            p111("p112_class1159", 0x0030e978, pok_class1159_update_l11, c1159, [common, first_run, moby_base, c615, c830, index_of, pick](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (st != 0) first_run(pok_class1159_update_l11, m);
                w8(m + 0x20, u(st));
                if (st == 1) {
                    if (coin() && !c615.empty()) w32(pv + 8, index_of(pick(c615)));
                    else if (!c830.empty()) w32(pv + 8, index_of(pick(c830)));
                    const uint32_t link = moby_base() + (ee_read<uint32_t>(pv + 8) << 8);
                    w8(link + 0x20, coin() ? 4u : 1u);
                    w8(link + 0xbc, coin() ? 1u : 0u);
                }
                wf(pv, rf(0.0f, ee_read<float>(pv + 0x10)));
                wf(pv + 4, rf(-0.05f, 0.1f));
                common(m);
            }, 3, 30);

            // class 1178 (tilting sewer platform)
            p111("p112_class1178", 0x0030eb90, pok_class1178_update_l11, c1178, [common, put_hero, P](uint32_t m, int) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wf(m + 0x44, ri(0, 2) == 0 ? (coin() ? 0.6f : -0.6f) : rf(-0.25f, 0.25f));
                wf(pv + 0x60, rf(-0.08f, 0.08f));
                put_hero(m + 0x10, rf(0.2f, 2.5f), 0.2f);
                common(m);
                w32(P + 0x2fc, ri(0, 2) != 0 ? m : 0u);
                w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
                w32(0x001413dcu, coin() ? 1u : 2u);
            }, 1, 80);

            // class 1180 (swinging sewer valve)
            p111("p112_class1180", 0x0030f2a8, pok_class1180_update_l11, c1180, [common, first_run, moby_base, c1179, index_of, pick](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (st != 0) first_run(pok_class1180_update_l11, m);
                w8(m + 0x20, u(st));
                if (!c1179.empty() && ri(0, 4) != 0) w32(pv, index_of(pick(c1179)));
                const uint32_t link = moby_base() + (ee_read<uint32_t>(pv) << 8);
                w8(link + 0x20, coin() ? 2u : 1u);
                if (ri(0, 5) == 0) w16(link + 0xa6, 0x17u);
                if (st == 2) {
                    const float tgt = ee_read<float>(pv + 8) + 1.0471975803375244f;
                    wf(m + 0x48, coin() ? tgt : rf(-3.1f, 3.1f));
                    wf(pv + 4, coin() ? 0.0f : rf(-0.05f, 0.05f));
                }
                common(m);
                if (st == 1) {
                    static const uint32_t fcs[] = {0u, 10u, 11u, 12u, 100u};
                    w32(L11_ADDR(0x0015f5ccu), fcs[ri(0, 4)]);
                }
            }, 4, 25);

            // class 1218 (spawned missile & health crates) + pok_30fd98_l11 + pok_30f660_l11
            {
                const size_t sp_mark = push_spawn_w_ignores();
                const uint32_t jet = c1242.empty() ? 0u : c1242[0];
                p112_spawned("p112_class1218", 0x0030f728, pok_class1218_update_l11, [common, first_run, jet, HERO](int st) {
                    const uint32_t m = moby_spawn(coin() ? 0x4c2u : 0x4c4u);
                    Vec4 air{500.0f + rf(-20.0f, 20.0f), 500.0f + rf(-20.0f, 20.0f), 320.0f + rf(-20.0f, 20.0f), 0.0f};
                    ee_ref<Vec4>(m + 0x10) = air;
                    w8(m + 0x31, coin() ? 1u : 0u);
                    moby_update_transform(m);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (st != 0) first_run(pok_class1218_update_l11, m);
                    w8(m + 0x20, u(st));
                    wfrand(pv + 0x10, 3, -0.5f, 0.5f);
                    wf(pv + 0x30, rf(-3.1f, 3.1f));
                    const uint32_t chute = ee_read<uint32_t>(pv + 0x34);
                    if (chute != 0) {
                        w8(chute + 0x52, coin() ? 1u : 0u);
                        static const uint8_t csts[] = {1, 2, 0xfe};
                        w8(chute + 0x20, csts[ri(0, 2)]);
                    }
                    if (jet != 0) {
                        w32(0x00140940u, ri(0, 5) == 0 ? 0u : jet);
                        w32(0x001413d4u, ri(0, 3) == 0 ? 0u : 0x32u);
                        w8(jet + 0x20, ri(0, 6) == 0 ? 0xfeu : 4u);
                        w8(0x00140946u, u(ri(0, 20)));
                        w8(0x00140947u, 20u);
                        wf(0x0014094cu, rf(10.0f, 95.0f));
                        wf(0x00140950u, 100.0f);
                        const int mode = ri(0, 4);
                        if (mode == 0) {  // right on the jet:sphere_collide hits jet or dist3 < 4
                            ee_ref<Vec4>(jet + 0x10) = air;
                            moby_update_transform(jet);
                        } else if (mode == 1) {  // within 4
                            Vec4 jq = air; jq.x += rf(-2.0f, 2.0f); jq.y += rf(-2.0f, 2.0f);
                            ee_ref<Vec4>(jet + 0x10) = jq;
                            moby_update_transform(jet);
                        } else if (mode == 2) {  // within 40: magnetic pull in state 2
                            Vec4 jq = air; jq.x += rf(6.0f, 30.0f);
                            ee_ref<Vec4>(jet + 0x10) = jq;
                            w8(m + 0x31, 1u);
                            moby_update_transform(jet);
                        } else if (mode == 3) {  // world collision: place crate at ground near hero start
                            Vec4 g = ee_ref<Vec4>(HERO); g.z -= 0.2f;
                            ee_ref<Vec4>(m + 0x10) = g;
                            moby_update_transform(m);
                        }
                    }
                    if (st == 3 && ri(0, 2) == 0) {  // axis clamps (< 20 retires if z < 20, or clamps x/y < 20 and > 1003)
                        if (coin()) wf(m + 0x18, 15.0f);
                        else { wf(m + 0x10, coin() ? 10.0f : 1010.0f); wf(m + 0x14, coin() ? 12.0f : 1009.0f); wf(m + 0x18, 1010.0f); }
                    }
                    common(m);
                    if (jet != 0 && ri(0, 3) != 0) w32(0x001413d4u, 0x32u);
                    return m;
                }, 6, 30);
                g_ignored_words.resize(sp_mark);
            }

            l11check("p112_f30fd98", 0x0030fd98, 30, Ret::Int32, [level] {
                level();
                const uint32_t crate = moby_spawn(0x4c2u);
                Vec4 air{500.0f, 500.0f, 320.0f, 0.0f};
                ee_ref<Vec4>(crate + 0x10) = air;
                wf(crate + 0x48, rf(-3.1f, 3.1f));
                moby_update_transform(crate);
                if (ri(0, 5) == 0) w32(OV(0x0015ffe8), ee_read<uint32_t>(OV(0x0015ffd4)));  // (dynamic pool exhausted -> spawn 0)
                return Args{{crate}, {}};
            }, [](A a) { return r32(pok_30fd98_l11(a.i[0])); });

            l11check("p112_f30f660", 0x0030f660, 40, Ret::Int32, [level] {
                level();
                const uint32_t crate = moby_spawn(0x4c2u);
                Vec4 air{500.0f, 500.0f, 320.0f, 0.0f};
                ee_ref<Vec4>(crate + 0x10) = air;
                moby_update_transform(crate);
                const uint32_t pv = ee_read<uint32_t>(crate + 0x78);
                const int k = ri(0, 3);
                if (k == 0) {
                    w32(pv + 0x34, 0);
                    if (coin()) w32(OV(0x0015ffe8), ee_read<uint32_t>(OV(0x0015ffd4)));
                } else {
                    const uint32_t c = pok_30fd98_l11(crate);
                    w32(pv + 0x34, c);
                    if (k == 1) w8(c + 0x20, coin() ? 0xfeu : 0xfdu);
                }
                return Args{{crate, pv}, {}};
            }, [](A a) { return r32(pok_30f660_l11(a.i[0], a.i[1]) ? 1u : 0u); });

            // class 1219 (spawned parachute) + pok_30fe50_l11
            p112_spawned("p112_class1219", 0x00310028, pok_class1219_update_l11, [common, give_hit, set_anim_at](int st) {
                const uint32_t crate = moby_spawn(0x4c2u);
                Vec4 air{500.0f, 500.0f, 320.0f, 0.0f};
                ee_ref<Vec4>(crate + 0x10) = air;
                w8(crate + 0x20, 2u);
                moby_update_transform(crate);
                const uint32_t m = pok_30fd98_l11(crate);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wf(pv + 0x20, rf(0.5f, 4.0f));
                w8(m + 0x20, u(st));
                if (st < 3 && ri(0, 4) == 0) {
                    if (coin()) w32(pv + 0x70, 0);
                    else w8(crate + 0x20, coin() ? 0xfeu : 0xfdu);
                }
                if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                w8(m + 0x53, u(ri(0, 4)));
                if (st == 2) {
                    give_hit(m, 0x330000u);
                    if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                        const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                        w32(e + 0x34, m);
                        w32(e + 0x24, 0x210000u);
                        w8(e + 0x28, u(ri(0, 10)));
                        wf(e + 0x2c, coin() ? 10.0f : 0.2f);
                    }
                }
                if (st == 3) {
                    w8(m + 0x23, coin() ? 0u : u(ri(1, 200)));
                    set_anim_at(m, 2, coin() ? 14.0f : 8.0f);
                }
                common(m);
                return m;
            }, 5, 30);

            l11check("p112_f30fe50", 0x0030fe50, 48, Ret::None, [level, give_hit] {
                level();
                const uint32_t crate = moby_spawn(0x4c2u);
                Vec4 air{500.0f, 500.0f, 320.0f, 0.0f};
                ee_ref<Vec4>(crate + 0x10) = air;
                w8(crate + 0x20, ri(0, 4) == 0 ? 0xfeu : 2u);
                moby_update_transform(crate);
                const uint32_t m = pok_30fd98_l11(crate);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                wf(pv + 0x20, coin() ? 1.0f : 3.0f);
                if (coin()) { w8(pv + 0x28, 1u); w8(pv + 0x2a, 6u); }
                w8(m + 0x20, ri(0, 5) == 0 ? 4u : 2u);
                give_hit(m, 0x330000u);
                if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                    const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                    w32(e + 0x34, m);
                    w32(e + 0x24, coin() ? 0x210000u : 0x100000u);
                    w8(e + 0x28, coin() ? 9u : u(ri(0, 10)));
                    wf(e + 0x2c, coin() ? 5.0f : 0.3f);
                }
                return Args{{m, pv, pv + 0x20}, {}};
            }, [](A a) { pok_30fe50_l11(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });

            // class 1231 (energy-ball thrower) + helpers 0x318e78, 0x318ed8, 0x318d10, 0x318e98, 0x310698
            {
                const size_t sp_mark = push_spawn_w_ignores();
                g_ignored_words.push_back(OV(0x00173e9c));
                p111("p112_class1231", 0x00310180, pok_class1231_update_l11, c1231,
                     [common, first_run, put_hero, give_hit, set_anim_at, HERO, P](uint32_t m, int st) {
                         const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                         if (st != 0) first_run(pok_class1231_update_l11, m);
                         w8(m + 0x20, u(st));
                         w8(0x0015edb7u, coin() ? 1u : 0u);
                         w32(pv + 0x158, u(ri(0, 3)));
                         if (st == 0) {
                             const bool on_path = coin();
                             w32(pv + 0x130, on_path ? 1u : 0u);
                             if (on_path) w32(pv + 0x134, ee_read<uint32_t>(pv + 0x13c));
                         }
                         put_hero(m + 0x10, coin() ? rf(3.0f, 18.0f) : rf(25.0f, 35.0f), coin() ? rf(-1.0f, 1.0f) : 5.0f);
                         ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0u) + 0x10) = ee_ref<Vec4>(HERO);
                         if (ri(0, 2) == 0) {
                             w8(m + 0x31, 1u);
                             Vec4 cam = ee_ref<Vec4>(m + 0x10);
                             cam.x += rf(-15.0f, 15.0f);
                             ee_ref<Vec4>(L11_ADDR(0x001677c0u)) = cam;
                         }
                         if (st == 4) {
                             if (coin()) set_anim_at(m, 2, 15.1f);
                             else { moby_anim_set(m, 2, 0, 0); if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u); }
                         } else if (st == 5) {
                             if (ri(0, 3) != 0) w32(pv + 0x160, pok_318d10_l11(m, m + 0x10, 5));
                             if (coin()) set_anim_at(m, 3, 33.1f);
                             if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                         } else if (st == 6) {
                             if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                         } else if (st == 7 || st == 8) {
                             move_state_start(m, pv + 0x70, st == 7 ? 5 : 6, 1, 0, 0.0f);
                             if (coin()) w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                             if (st == 7 && coin()) { Vec4 out = ee_ref<Vec4>(m + 0x10); out.x += 200.0f; ee_ref<Vec4>(m + 0x10) = out; }
                         }
                         if (st >= 3 && st <= 6 && ri(0, 2) == 0) {
                             if (coin() && ee_read<uint32_t>(pv + 0x160) == 0) w32(pv + 0x160, pok_318d10_l11(m, m + 0x10, 5));
                             give_hit(m, 0x330000u);
                             if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                                 const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                                 w32(e + 0x34, m);
                                 w32(e + 0x24, 0x210000u);
                                 w32(e + 0x20, ee_read<uint32_t>(0x001413d0u));
                                 static const uint8_t hkinds[] = {0, 1, 4, 9};
                                 w8(e + 0x28, hkinds[ri(0, 3)]);
                                 static const float hdmg[] = {0.2f, 1.2f, 5.0f};
                                 wf(e + 0x2c, hdmg[ri(0, 2)]);
                             }
                         }
                         common(m);
                         (void)P;
                     }, 9, 25);
                g_ignored_words.pop_back();

                if (!c1231.empty()) {
                    l11check("p112_f318e78", 0x00318e78, 20, Ret::None, [level, c1231] {
                        level();
                        const uint32_t m = c1231[0];
                        const uint32_t shot = pok_318d10_l11(m, m + 0x10, 5);
                        wfrand(T3, 4, 100.0f, 600.0f);
                        return Args{{shot, T3}, {}};
                    }, [](A a) { pok_318e78_l11(a.i[0], a.i[1]); return uint64_t{0}; });

                    l11check("p112_f318ed8", 0x00318ed8, 30, Ret::Int32, [level, c1231, common] {
                        level();
                        const uint32_t m = c1231[0];
                        const uint32_t shot = pok_318d10_l11(m, m + 0x10, 5);
                        common(m);
                        return Args{{shot}, {}};
                    }, [](A a) { return r32(pok_318ed8_l11(a.i[0])); });

                    l11check("p112_f318d10", 0x00318d10, 24, Ret::Int32, [level, c1231, common] {
                        level();
                        const uint32_t m = c1231[0];
                        wfrand(T3, 3, 200.0f, 500.0f); wf(T3 + 0xc, 0.0f);
                        common(m);
                        if (ri(0, 5) == 0) w32(OV(0x0015ffe8), ee_read<uint32_t>(OV(0x0015ffd4)));
                        return Args{{m, T3, u(ri(1, 15))}, {}};
                    }, [](A a) { return r32(pok_318d10_l11(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });

                    l11check("p112_f318e98", 0x00318e98, 20, Ret::None, [level, c1231, common] {
                        level();
                        const uint32_t m = c1231[0];
                        const uint32_t shot = pok_318d10_l11(m, m + 0x10, 5);
                        wfrand(T3, 3, -5.0f, 5.0f); wf(T3 + 0xc, 0.0f);
                        common(m);
                        return Args{{shot, T3}, {}};
                    }, [](A a) { pok_318e98_l11(a.i[0], a.i[1]); return uint64_t{0}; });

                    l11check("p112_f310698", 0x00310698, 48, Ret::None, [level, c1231, first_run, put_hero, give_hit, common, HERO] {
                        level();
                        const uint32_t m = c1231[static_cast<size_t>(ri(0, static_cast<int>(c1231.size()) - 1))];
                        first_run(pok_class1231_update_l11, m);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, ri(0, 5) == 0 ? 8u : 3u);
                        if (coin()) w32(pv + 0x160, pok_318d10_l11(m, m + 0x10, 5));
                        put_hero(m + 0x10, coin() ? rf(2.0f, 18.0f) : rf(26.0f, 40.0f), coin() ? 0.0f : 6.0f);
                        ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0u) + 0x10) = ee_ref<Vec4>(HERO);
                        give_hit(m, 0x330000u);
                        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                            w32(e + 0x34, m);
                            w32(e + 0x24, 0x210000u);
                            w32(e + 0x20, ee_read<uint32_t>(0x001413d0u));
                            static const uint8_t hkinds[] = {0, 1, 4, 9};
                            w8(e + 0x28, hkinds[ri(0, 3)]);
                            static const float hdmg[] = {0.2f, 1.2f, 5.0f};
                            wf(e + 0x2c, hdmg[ri(0, 2)]);
                        }
                        common(m);
                        return Args{{m}, {}};
                    }, [](A a) { pok_310698_l11(a.i[0]); return uint64_t{0}; });
                }
                g_ignored_words.resize(sp_mark);
            }

            // class 1242 (fighter jet) + helpers 0x3125a8, 0x309378, 0x3126f8, 0x313f60, 0x312838, 0x31a758, 0x31a7d8
            if (!c1242.empty()) {
                const size_t sp_mark = push_spawn_w_ignores();
                g_ignored_words.push_back(OV(0x00173e9c));
                auto jet_flight_init = [first_run](uint32_t m) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84u) << 4), 0u);
                    first_run(pok_class1242_update_l11, m);
                    w8(m + 0x20, 2u);
                    wf(pv + 0x130, 1.0f);
                    const uint32_t sp0 = ee_stack_ptr();
                    try { pok_class1242_update_l11(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                };

                p111("p112_class1242", 0x00313290, pok_class1242_update_l11, c1242,
                     [common, put_hero, collectible, box_at, sound_half, jet_flight_init, c1248, c1319, HERO](uint32_t m, int st) {
                         const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                         w32(OV(0x001611c0), T3 + 0x1000u);
                         if (st == 0) {
                             w8(m + 0x20, 0u);
                             w32(L11_ADDR(0x00162170u), coin() ? 0u : 1u);
                             collectible(ee_read<uint8_t>(m + 0xb0), coin());
                             common(m);
                             return;
                         }
                         jet_flight_init(m);
                         w8(m + 0x20, u(st));
                         sound_half(m, pv + 0x12c);
                         sound_half(m, pv + 0x12e);
                         wf(pv + 0x130, coin() ? 0.0f : 0.5f);
                         if (st == 1) {
                             ee_ref<Vec4>(m + 0x10) = ee_ref<Vec4>(pv + 0x40);
                             ee_ref<Vec4>(m + 0x40) = ee_ref<Vec4>(pv + 0x50);
                             moby_update_transform(m);
                             put_hero(m + 0x10, coin() ? rf(0.5f, 2.5f) : rf(10.0f, 20.0f), 0.0f);
                             const uint32_t hm = ee_read<uint32_t>(0x001413d0u);
                             ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(HERO);
                             moby_update_transform(hm);
                             w8(0x0013d4c7u, coin() ? 1u : 0u);
                             static const uint16_t kept[] = {0, 2, 10, 40};
                             w16(0x00141b4au, kept[ri(0, 3)]);
                             w32(0x0015eea4u, ee_read<uint16_t>(0x00141b4au) * 600u + u(ri(0, 2000)));
                             common(m);
                             w32(0x001413d4u, 0u);
                             w8(0x001413f4u, 0u);
                             w32(0x0013cae4u, coin() ? 0x10u : 0u);
                             return;
                         }
                         if (st == 2) {
                             wf(pv + 0x130, coin() ? 0.99f : 0.4f);
                             w16(0x00141c10u, coin() ? 0u : 1u);
                             common(m);
                             return;
                         }
                         if (st == 4 || st == 8) {
                             const uint32_t fbox = box_at(ee_read<int32_t>(pv + 0x104));
                             Vec4 center = ee_ref<Vec4>(fbox + 0x30);
                             center.z = 310.0f;
                             const int pos_mode = ri(0, 5);
                             if (pos_mode == 1) {  // near outer xy boundary
                                 const float rad = std::hypot(ee_read<float>(fbox), ee_read<float>(fbox + 4));
                                 center.x += rad - rf(-2.0f, 8.0f);
                             } else if (pos_mode == 2) {  // near ceiling
                                 center.z = ee_read<float>(fbox + 0x28) + ee_read<float>(fbox + 0x38) - rf(-2.0f, 8.0f);
                             } else if (pos_mode == 3) {  // near floor 250
                                 center.z = 250.0f + rf(-2.0f, 8.0f);
                             } else if (pos_mode == 4) {  // world crash into landing pad
                                 center = ee_ref<Vec4>(pv + 0x40);
                                 center.z -= 1.0f;
                             } else if (pos_mode == 5 && !c1248.empty()) {  // moby collision with a bomber
                                 const uint32_t b = c1248[0];
                                 w8(b + 0x20, 1u);
                                 ee_ref<Vec4>(b + 0x10) = center;
                                 moby_update_transform(b);
                             }
                             ee_ref<Vec4>(m + 0x10) = center;
                             moby_update_transform(m);
                             static const uint16_t t68[] = {0, 15, 40, 200};
                             w16(pv + 0x68, t68[ri(0, 3)]);
                             wf(pv + 0x64, pos_mode == 4 ? 0.01f : rf(0.2f, 2.5f));
                             wf(pv + 0x34, rf(-1.5f, 1.5f));
                             wf(pv + 0x38, rf(-3.1f, 3.1f));
                             w8(pv + 0x60, coin() ? 1u : 0u);
                             wf(0x00141070u, rf(-1.0f, 1.0f));
                             wf(0x00141074u, rf(-1.0f, 1.0f));
                             w32(pv + 0x80, coin() ? 1u : 5u);
                             w32(pv + 0x84, coin() ? 1u : 5u);
                             w8(0x00140946u, coin() ? 5u : 0u);
                             w32(pv + 0x88, !c1319.empty() && coin() ? c1319[0] : 0u);
                             w32(pv + 0x8c, coin() ? 100u : 600u);
                             w32(pv + 0xdc, coin() ? 0u : 4u);
                             wf(0x0014094cu, coin() ? 5.0f : 80.0f);
                             w16(0x00141c10u, coin() ? 0u : 1u);
                             w16(0x00141928u, coin() ? 0xffffu : 10u);
                             w16(0x0014192au, u(ri(0, 20)));
                             w16(pv + 0xea, coin() ? 1u : 0u);
                             static const int16_t e8s[] = {0, 1, 3};
                             w16(pv + 0xe8, static_cast<uint16_t>(e8s[ri(0, 2)]));
                             w32(pv + 0x114, coin() ? 1u : 10u);
                             for (size_t i = 0; i < c1319.size() && i < 8; ++i)
                                 w8(c1319[i] + 0x20, (i & 1u) != 0 ? 4u : 5u);
                             w8(0x0014095fu, ri(0, 4) == 0 ? 1u : 0u);
                             if (st == 4 && ri(0, 2) == 0) {  // incoming shot hit (0x30001)
                                 const uint32_t idx = 0u, e = OV(0x00178100) + (idx << 6);
                                 w8(m + 0xa4, idx);
                                 w32(e + 0x34, m);
                                 w32(e + 0x24, 0x30001u);
                                 wf(e + 0x2c, coin() ? 10.0f : 200.0f);
                                 w32(e + 0x30, coin() ? 1u : 0u);
                                 wf(e + 0x10, 0.6f); wf(e + 0x14, 0.8f); wf(e + 0x18, 0.0f); wf(e + 0x1c, 0.0f);
                             }
                             if (st == 8) {
                                 w32(pv + 0xf8, coin() ? 1u : 5u);
                                 wf(pv + 0x130, coin() ? 0.99f : 0.4f);
                                 if (ri(0, 4) == 0) w32(pv + 0x100, 0xffffffffu);
                                 if (ri(0, 4) == 0) w32(pv + 0xfc, 0xffffffffu);
                                 if (ri(0, 4) == 0) w8(m + 0xb0, 0xffu);
                             }
                         }
                         if (st == 7) w32(pv + 0xf8, coin() ? 1u : 5u);
                         if (st == 9 && coin()) { wf(m + 0x10, 10.0f); wf(m + 0x14, 1020.0f); wf(m + 0x18, 12.0f); }
                         common(m);
                         w32(0x0013caf4u, coin() ? 0x8000u : 0u);
                         w32(0x0013caf0u, (coin() ? 0x40u : 0u) | (coin() ? 0x84u : 0u) | (coin() ? 0x28u : 0u));
                         w32(0x0013cae0u, coin() ? 0x200u : 0u);
                     }, 10, 28);
                g_ignored_words.pop_back();

                l11check("p112_f3125a8", 0x003125a8, 36, Ret::None, [level, c1242, jet_flight_init, common] {
                    level();
                    const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78);
                    jet_flight_init(m);
                    w32(pv + 0x80, coin() ? 1u : 4u);
                    w8(pv + 0x61, coin() ? 1u : 0u);
                    common(m);
                    w32(0x0013caf0u, coin() ? 0x84u : 0u);
                    return Args{{m, pv}, {rf(-3.1f, 3.1f), rf(-1.0f, 1.0f)}};
                }, [](A a) { pok_3125a8_l11(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });

                l11check("p112_f309378", 0x00309378, 36, Ret::Int32, [level, c1242, c1319, jet_flight_init, common] {
                    level();
                    const uint32_t m = c1242[0];
                    jet_flight_init(m);
                    wfrand(T3, 3, 300.0f, 500.0f); wf(T3 + 0xc, 0.0f);
                    wfrand(T3 + 0x10, 3, -3.1f, 3.1f); wf(T3 + 0x1c, 0.0f);
                    const uint32_t tgt = !c1319.empty() && coin() ? c1319[0] : 0u;
                    common(m);
                    if (ri(0, 5) == 0) w32(OV(0x0015ffe8), ee_read<uint32_t>(OV(0x0015ffd4)));
                    return Args{{m, T3, tgt, T3 + 0x10, u(ri(30, 300))}, {rf(0.5f, 3.0f)}};
                }, [](A a) { return r32(pok_309378_l11(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.f[0])); });

                l11check("p112_f3126f8", 0x003126f8, 36, Ret::None, [level, c1242, c1319, jet_flight_init, common] {
                    level();
                    const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78);
                    jet_flight_init(m);
                    w32(pv + 0x84, coin() ? 1u : 4u);
                    w16(pv + 0x6a, u(ri(0, 3)));
                    w8(0x00140946u, coin() ? 5u : 0u);
                    w32(pv + 0x88, !c1319.empty() && coin() ? c1319[0] : 0u);
                    w32(pv + 0x8c, coin() ? 200u : 600u);
                    common(m);
                    w32(0x0013caf0u, coin() ? 0x28u : 0u);
                    return Args{{m, pv}, {}};
                }, [](A a) { pok_3126f8_l11(a.i[0], a.i[1]); return uint64_t{0}; });

                l11check("p112_f313f60", 0x00313f60, 48, Ret::None, [level, c1242, box_at, jet_flight_init, common] {
                    level();
                    const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78);
                    jet_flight_init(m);
                    const uint32_t fbox = box_at(ee_read<int32_t>(pv + 0x104));
                    Vec4 center = ee_ref<Vec4>(fbox + 0x30);
                    const float rad = std::hypot(ee_read<float>(fbox), ee_read<float>(fbox + 4));
                    const int k = ri(0, 3);
                    if (k == 1) center.x += rad - rf(-5.0f, 9.0f);
                    if (k == 2) center.z = ee_read<float>(fbox + 0x28) + ee_read<float>(fbox + 0x38) - rf(-5.0f, 9.0f);
                    else if (k == 3) center.z = 250.0f + rf(-5.0f, 9.0f);
                    else center.z = 310.0f;
                    ee_ref<Vec4>(m + 0x10) = center;
                    wfrand(pv + 0x10, 3, -1.0f, 1.0f);
                    wf(pv + 0x34, rf(-0.8f, 0.8f));
                    wf(pv + 0x38, rf(-3.1f, 3.1f));
                    wf(T3, rf(-1.0f, 1.0f));
                    wf(T3 + 4, rf(-1.0f, 1.0f));
                    w8(0x0014095fu, 0u);
                    common(m);
                    return Args{{m, T3, T3 + 4}, {}};
                }, [](A a) { pok_313f60_l11(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });

                l11check("p112_f312838", 0x00312838, 60, Ret::None, [level, c1242, c1248, c1319, box_at, jet_flight_init, common] {
                    level();
                    const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78);
                    jet_flight_init(m);
                    const uint32_t fbox = box_at(ee_read<int32_t>(pv + 0x104));
                    Vec4 center = ee_ref<Vec4>(fbox + 0x30);
                    center.z = 310.0f;
                    const int mode = ri(0, 3);
                    if (mode == 1) { center = ee_ref<Vec4>(pv + 0x40); center.z -= 1.0f; }
                    else if (mode == 2 && !c1248.empty()) {
                        const uint32_t b = c1248[0];
                        w8(b + 0x20, 1u);
                        ee_ref<Vec4>(b + 0x10) = center;
                        moby_update_transform(b);
                    }
                    ee_ref<Vec4>(m + 0x10) = center;
                    moby_update_transform(m);
                    static const uint16_t t68[] = {0, 15, 40, 200};
                    w16(pv + 0x68, t68[ri(0, 3)]);
                    wf(pv + 0x64, (mode == 1 || mode == 2) ? 0.01f : rf(0.2f, 2.5f));
                    wf(pv + 0x34, mode == 1 ? 0.2f : rf(-1.5f, 1.5f));
                    wf(pv + 0x38, rf(-3.1f, 3.1f));
                    w8(pv + 0x60, coin() ? 1u : 0u);
                    wf(0x00141070u, rf(-1.0f, 1.0f));
                    wf(0x00141074u, rf(-1.0f, 1.0f));
                    w32(pv + 0x80, coin() ? 1u : 5u);
                    w32(pv + 0x84, coin() ? 1u : 5u);
                    w8(0x00140946u, coin() ? 5u : 0u);
                    w32(pv + 0x88, !c1319.empty() && coin() ? c1319[0] : 0u);
                    w32(pv + 0x8c, coin() ? 100u : 600u);
                    w32(pv + 0xdc, (mode == 2 && coin()) ? 0u : (coin() ? 0u : 4u));
                    wf(0x0014094cu, coin() ? 0.01f : 80.0f);
                    w16(0x00141c10u, coin() ? 0u : 1u);
                    w16(0x00141928u, coin() ? 0xffffu : 10u);
                    w16(0x0014192au, coin() ? 0u : 50u);
                    w32(0x0015eea4u, 3600u);
                    common(m);
                    w32(0x0013caf4u, coin() ? 0x8000u : 0u);
                    w32(0x0013caf0u, (coin() ? 0x40u : 0u) | (coin() ? 0x84u : 0u) | (coin() ? 0x28u : 0u));
                    w32(0x0013cae0u, coin() ? 0x200u : 0u);
                    return Args{{m, pv}, {}};
                }, [](A a) { pok_312838_l11(a.i[0], a.i[1]); return uint64_t{0}; });

                l11check("p112_f31a758", 0x0031a758, 30, Ret::Int32, [level, c1242, c1319] {
                    level();
                    const int32_t grp = ee_read<int32_t>(ee_read<uint32_t>(c1242[0] + 0x78) + 0x10c);
                    for (uint32_t b : c1319) w8(b + 0x20, coin() ? 5u : 4u);
                    return Args{{u(grp)}, {}};
                }, [](A a) { return r32(static_cast<uint32_t>(pok_31a758_l11(static_cast<int32_t>(a.i[0])))); });

                l11check("p112_f31a7d8", 0x0031a7d8, 48, Ret::Int32, [level, c1242, c1319, common] {
                    level();
                    const int32_t grp = ee_read<int32_t>(ee_read<uint32_t>(c1242[0] + 0x78) + 0x10c);
                    for (uint32_t b : c1319) {
                        w8(b + 0x20, ri(0, 3) == 0 ? 5u : 4u);
                        w8(b + 0x53, coin() ? 0u : 1u);
                        w32(ee_read<uint32_t>(b + 0x78) + 0x88, u(ri(0, 3)));
                    }
                    wf(L11_ADDR(0x001677d4u), rf(-1.0f, 1.0f));
                    common(c1242[0]);
                    if (ri(0, 2) != 0) {  // guarantee rand_mod(10) == 0 on first candidate in most cases
                        for (uint32_t s = 1; s < 128; ++s) {
                            libc_srand(s);
                            if (rand_mod(0xa) == 0) { libc_srand(s); break; }
                        }
                    }
                    return Args{{u(grp)}, {}};
                }, [](A a) { return r32(pok_31a7d8_l11(static_cast<int32_t>(a.i[0]))); });

                g_ignored_words.resize(sp_mark);
            }

            // ---- 1.02 Gameplay Fix 10 (pok_class1242_update_l11): blocks prompt/boarding when hero state is 0x32 / 0x1d or
            // vehicle flag 0x1413f4 != 0, sets hero state 0x32 at Triangle press in state 1, and skips it at fade-end in state 2
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "p112_class1242_fix10") == 0) {
                if (std::getenv("RAC1_GAMEPLAY102") == nullptr || c1242.empty()) {
                    std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=10 on level 11)\n", "p112_class1242_fix10");
                    ++g_skipped;
                } else {
                    int bad = 0, n = 0, blocked = 0, boarded = 0, fade_skipped = 0;
                    static std::vector<uint8_t> start, with_fix;
                    const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                    for (int c = 0; c < 90; ++c) {
                        level();
                        const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78), hm = ee_read<uint32_t>(0x001413d0u);
                        w8(0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84u) << 4), 0u);
                        first_run(pok_class1242_update_l11, m);
                        const int kind = c % 3;
                        if (kind < 2) {
                            w8(m + 0x20, 1u);
                            put_hero(m + 0x10, 1.0f, 0.0f);
                            ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(HERO);
                            moby_update_transform(hm);
                            moby_update_transform(m);
                            w8(0x0013d4c7u, 1u);
                            wf(pv + 0x130, 0.5f);
                            common(m);
                            w32(OV(0x0015f5c4), 0u);
                            w32(0x0013cae4u, 0x10u);
                            if (kind == 0) {
                                const int blk = ri(0, 2);
                                w32(0x001413d4u, blk == 0 ? 0x32u : (blk == 1 ? 0x1du : 0u));
                                w8(0x001413f4u, blk == 2 ? 1u : 0u);
                            } else {
                                w32(0x001413d4u, 0u);
                                w8(0x001413f4u, 0u);
                            }
                        } else {
                            w8(m + 0x20, 2u);
                            wf(pv + 0x130, 0.99f);
                            common(m);
                            w32(OV(0x0015f5c4), 0u);
                            w32(0x001413d4u, 0u);
                        }
                        uint8_t* ram = ee_get_ram_base();
                        start.assign(ram, ram + 0x02000000);
                        g_advanced_disabled = false;
                        pok_class1242_update_l11(m);
                        g_advanced_disabled = true;
                        with_fix.assign(ram, ram + 0x02000000);
                        std::memcpy(ram, start.data(), start.size());
                        if (kind == 0) {
                            const float saved_hm0 = ee_read<float>(hm);
                            wf(hm, saved_hm0 + 200000.0f);
                            pok_class1242_update_l11(m);
                            wf(hm, saved_hm0);
                            ++blocked;
                        } else if (kind == 1) {
                            pok_class1242_update_l11(m);
                            hero_set_state(0x32, 1);
                            ++boarded;
                        } else {
                            // Unfixed sets 0x1413d4 = 0x32; with 0x15f5c4 == 2 hero_set_state(0x32, 1) returns 0, matching Fix 10
                            pok_class1242_update_l11(m);
                            const bool unfixed_entered = ee_read<uint32_t>(0x001413d4u) == 0x32u;
                            std::memcpy(ram, start.data(), start.size());
                            w32(OV(0x0015f5c4), 2u);
                            pok_class1242_update_l11(m);
                            w32(OV(0x0015f5c4), 0u);
                            if (unfixed_entered && with_fix[m + 0x20] == 4u) ++fade_skipped;
                            else ++bad;
                        }
                        ++n;
                        for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                            if (ignored.count(a) != 0) continue;
                            if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                            uint32_t xw, yw;
                            std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                            if (xw != yw) { if (bad < 8) std::printf("  p112_class1242_fix10 case %d: %08x fix=%08x expected=%08x\n", c, a, xw, yw); ++bad; break; }
                        }
                    }
                    std::printf("%-26s %s (%d cases, %d blocked, %d boarded, %d fade_skipped)\n", "p112_class1242_fix10",
                                bad == 0 && blocked > 0 && boarded > 0 && fade_skipped > 0 ? "ok" : "FAIL", n, blocked, boarded, fade_skipped);
                    if (bad != 0 || blocked == 0 || boarded == 0 || fade_skipped == 0) ++g_fail;
                }
            }

            // ---- 1.02 Fixes 35 (pok_class1158_update_l11): restores sewer underwater tint/fog 0x161200..0x161206 when camera z <= 180
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "p112_class1158_fix35") == 0) {
                if (std::getenv("RAC1_FIXES102") == nullptr || c1158.empty()) {
                    std::printf("%-26s skipped (run with RAC1_FIXES102=35 on level 11)\n", "p112_class1158_fix35");
                    ++g_skipped;
                } else {
                    int bad = 0, n = 0, tinted = 0, kept = 0;
                    static std::vector<uint8_t> start, with_fix;
                    const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                    for (int c = 0; c < 80; ++c) {
                        level();
                        const uint32_t m = c1158[static_cast<size_t>(ri(0, static_cast<int>(c1158.size()) - 1))];
                        w8(m + 0x20, u(ri(1, 3)));
                        for (uint32_t k = 0; k < 8; ++k) w8(L11_ADDR(0x00161200u) + k, static_cast<uint8_t>(0x90u + k));
                        const float cam_z = coin() ? (coin() ? 180.0f : rf(120.0f, 179.9f)) : rf(180.1f, 250.0f);
                        wf(L11_ADDR(0x001677c8u), cam_z);
                        common(m);
                        uint8_t* ram = ee_get_ram_base();
                        start.assign(ram, ram + 0x02000000);
                        g_advanced_disabled = false;
                        pok_class1158_update_l11(m);
                        g_advanced_disabled = true;
                        with_fix.assign(ram, ram + 0x02000000);
                        std::memcpy(ram, start.data(), start.size());
                        if (cam_z <= 180.0f) {
                            w8(L11_ADDR(0x00161200u), 0);
                            w8(L11_ADDR(0x00161201u), 0x28);
                            w8(L11_ADDR(0x00161202u), 0x30);
                            w8(L11_ADDR(0x00161203u), 0x40);
                            w8(L11_ADDR(0x00161204u), 0);
                            w8(L11_ADDR(0x00161205u), 0x28);
                            w8(L11_ADDR(0x00161206u), 0x30);
                            ++tinted;
                        } else {
                            ++kept;
                        }
                        pok_class1158_update_l11(m);
                        ++n;
                        for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                            if (ignored.count(a) != 0) continue;
                            if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                            uint32_t xw, yw;
                            std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                            if (xw != yw) { if (bad < 8) std::printf("  p112_class1158_fix35 case %d: %08x fix=%08x expected=%08x\n", c, a, xw, yw); ++bad; break; }
                        }
                    }
                    std::printf("%-26s %s (%d cases, %d tinted, %d kept)\n", "p112_class1158_fix35",
                                bad == 0 && tinted > 0 && kept > 0 ? "ok" : "FAIL", n, tinted, kept);
                    if (bad != 0 || tinted == 0 || kept == 0) ++g_fail;
                }
            }

            // ---- 1.02 Fixes 36 (pok_312838_l11): removes the Square-button toggle on pv + 0x60 and keeps chase camera slot 0
            if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "p112_f312838_fix36") == 0) {
                if (std::getenv("RAC1_FIXES102") == nullptr || c1242.empty()) {
                    std::printf("%-26s skipped (run with RAC1_FIXES102=36 on level 11)\n", "p112_f312838_fix36");
                    ++g_skipped;
                } else {
                    int bad = 0, n = 0, untoggled = 0, slot0_forced = 0;
                    static std::vector<uint8_t> start, with_fix;
                    const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                    for (int c = 0; c < 80; ++c) {
                        level();
                        const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78);
                        w8(0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84u) << 4), 0u);
                        first_run(pok_class1242_update_l11, m);
                        w8(m + 0x20, 2u);
                        wf(pv + 0x130, 1.0f);
                        pok_class1242_update_l11(m);
                        const uint32_t fbox = box_at(ee_read<int32_t>(pv + 0x104));
                        Vec4 center = ee_ref<Vec4>(fbox + 0x30);
                        center.z = 310.0f;
                        ee_ref<Vec4>(m + 0x10) = center;
                        moby_update_transform(m);
                        w16(pv + 0x68, 60u);
                        const uint8_t pv60_init = (c & 1) != 0 ? 1u : 0u;
                        const uint32_t caf4 = (c & 2) != 0 ? 0x8000u : 0u;
                        w8(pv + 0x60, pv60_init);
                        common(m);
                        w32(0x0013caf4u, caf4);
                        uint8_t* ram = ee_get_ram_base();
                        start.assign(ram, ram + 0x02000000);
                        g_advanced_disabled = false;
                        pok_312838_l11(m, pv);
                        g_advanced_disabled = true;
                        with_fix.assign(ram, ram + 0x02000000);
                        std::memcpy(ram, start.data(), start.size());
                        w32(0x0013caf4u, caf4 & ~0x8000u);
                        w8(pv + 0x60, 0u);
                        pok_312838_l11(m, pv);
                        w32(0x0013caf4u, caf4);
                        w8(pv + 0x60, pv60_init);
                        if ((caf4 & 0x8000u) != 0) ++untoggled;
                        if (pv60_init == 1u) ++slot0_forced;
                        ++n;
                        for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                            if (ignored.count(a) != 0) continue;
                            if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                            uint32_t xw, yw;
                            std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                            if (xw != yw) { if (bad < 8) std::printf("  p112_f312838_fix36 case %d: %08x fix=%08x expected=%08x\n", c, a, xw, yw); ++bad; break; }
                        }
                    }
                    std::printf("%-26s %s (%d cases, %d untoggled, %d slot0_forced)\n", "p112_f312838_fix36",
                                bad == 0 && untoggled > 0 && slot0_forced > 0 ? "ok" : "FAIL", n, untoggled, slot0_forced);
                    if (bad != 0 || untoggled == 0 || slot0_forced == 0) ++g_fail;
                }
            }
            // ---- P11.3: thorough targeted checks for Pokitaru's remaining 15 entries and 24 helpers
            {
                const std::vector<uint32_t> c1246 = mobys_with(0x00314318), c1264 = mobys_with(0x003172c0),
                                            c1350 = mobys_with(0x0031aa80), c1903 = mobys_with(0x0031e2f0),
                                            c1919 = mobys_with(0x0031f150), c318  = mobys_with(0x002f2518);
                auto setup_view_looking_at = [](float tx, float ty, float tz, float dist, float dz) {
                    const uint32_t cam = L11_ADDR(0x001677c0u);
                    wf(cam + 0x0, tx - dist);
                    wf(cam + 0x4, ty);
                    wf(cam + 0x8, tz + dz);
                    wf(cam + 0xc, 1.0f);
                    wf(cam + 0x10, 0.0f);
                    wf(cam + 0x14, std::atan2(-dz, dist));
                    wf(cam + 0x18, 0.0f);
                    wf(cam + 0x1c, 0.0f);
                    render_view_setup();
                };
                auto setup_pkt_buf = []() {
                    w32(OV(0x001611c0), 0x00800000u);
                    w32(OV(0x0015f5f8), 0x00860000u);
                };

                // class 1246 (pufferfish enemy) + pok_315968_l11 + pok_315f58_l11 + pok_316090_l11
                {
                    const std::vector<uint32_t> c1075 = mobys_with(0x00309ac0);
                    const size_t sp_mark = push_spawn_w_ignores();
                    p111("p113_class1246", 0x00314318, pok_class1246_update_l11, c1246, [common, first_run, give_hit, put_hero, c1075, index_of](uint32_t m, int st) {
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        if (st != 0) first_run(pok_class1246_update_l11, m);
                        w8(m + 0x20, u(st));
                        if (st == 0) {
                            w32(pv + 0x238, u(ri(0, 2)));
                        }
                        if (st >= 12 && st <= 18 && !c1075.empty() && ee_read<int32_t>(pv + 0x254) < 0) {
                            w32(pv + 0x254, index_of(c1075[0]));
                        }
                        if (st == 12 && !c1075.empty()) {
                            const uint32_t raft = c1075[0];
                            Vec4 rp = ee_ref<Vec4>(raft + 0x10);
                            rp.y += 1.0f;
                            ee_ref<Vec4>(m + 0x10) = rp;
                            wf(pv + 0x258, 2.5f);
                        }
                        w8(m + 0x31, coin() ? 1u : 0u);
                        w8(m + 0x70, coin() ? 2u : 0u);
                        w8(m + 0x52, coin() ? ee_read<uint8_t>(m + 0x53) : u(ri(0, 7)));
                        wf(m + 0x54, coin() ? 0.0f : rf(1.0f, 10.0f));
                        w32(pv + 0x214, coin() ? 1u : 2u);
                        w32(pv + 0x38, ri(0, 3) == 0 ? 1u : 0u);
                        w32(pv + 0x244, coin() ? u(ri(0, 20)) : 0u);
                        wf(pv + 0x17c, coin() ? 1.0f : 0.0f);
                        wf(pv + 0x48, coin() ? -0.5f : 0.5f);
                        w16(pv + 0xc8, coin() ? 1u : 0u);
                        w8(m + 0xbc, 2u);
                        if (st == 19) w8(m + 0x53, u(ri(4, 7)));
                        put_hero(m + 0x10, rf(0.5f, 12.0f), rf(-1.0f, 2.0f));
                        give_hit(m, 0x00330000u);
                        common(m);
                    }, 21, 14);
                    g_ignored_words.resize(sp_mark);
                }

                // pok_hook_316160_u (class 1246 carry hook +0)
                if (!c1246.empty()) {
                    l11check("p113_hook_316160", 0x00316160, 48, Ret::Int32, [level, c1246, pick] {
                        level();
                        const uint32_t m = pick(c1246);
                        static const uint8_t sts[] = {0, 2, 4, 5, 6, 9, 0xe, 0xf, 0x10, 0x14};
                        w8(m + 0x20, sts[ri(0, 9)]);
                        w8(m + 0xbc, 2u);
                        const uint32_t T = 0x01e00000u;
                        ee_ref<Vec4>(T) = ee_ref<Vec4>(m + 0x10);
                        w32(T + 0x20, m);
                        return Args{{m, T, T + 0x20, 0, 0, 0, 0, 0}, {rf(0.0f, 5.0f), 0, 0, 0, 0, 0, 0, 0}};
                    }, [](A a) {
                        const TrRet r = pok_hook_316160_u(a.i[0], a.i[1], a.i[2], 0, 0, 0, 0, 0, a.f[0], 0, 0, 0, 0, 0, 0, 0);
                        return r32(r.v0);
                    });
                }

                // class 1248 (rising double-panel bridge)
                p111("p113_class1248", 0x00316320, pok_class1248_update_l11, c1248, [common, first_run, put_hero](uint32_t m, int st) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (st != 0) first_run(pok_class1248_update_l11, m);
                    w8(m + 0x20, u(st));
                    if (st == 1) {
                        put_hero(m + 0x10, coin() ? rf(0.0f, 4.0f) : rf(14.0f, 20.0f), 0.0f);
                    } else if (st >= 2 && st <= 4) {
                        wf(pv + 0x8, coin() ? 0.99f : rf(0.0f, 0.8f));
                        wf(pv + 0xc, rf(0.0f, 0.2f));
                    }
                    common(m);
                }, 7, 16);

                // class 1264 (sky cargo train) + pok_317678_l11 + pok_317820_l11 + pok_317c98_l11 + pok_3180a0_l11 + pok_318488_l11
                {
                    const size_t sp_mark = push_spawn_w_ignores();
                    p111("p113_class1264", 0x003172c0, pok_class1264_update_l11, c1264, [common, first_run, give_hit](uint32_t m, int st) {
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84u) << 4), (st == 0 && ri(0, 4) == 0) ? 0xffu : 0u);
                        if (st != 0) first_run(pok_class1264_update_l11, m);
                        w8(m + 0x20, u(st));
                        if (st == 1) w32(0x001413d4u, coin() ? 0x32u : 0u);
                        if (st == 2) {
                            wf(pv + 0x6c, coin() ? 0.99f : rf(0.0f, 0.8f));
                            wf(pv + 0xc0, coin() ? 0.01f : rf(0.5f, 2.0f));
                            const int32_t cnt = coin() ? 1 : ri(2, 8);
                            w32(pv + 0x64, u(cnt));
                            wf(pv + 0xe0, coin() ? 0.5f : rf(5.0f, 70.0f));
                            const uint32_t car = ee_read<uint32_t>(pv + 0x70 + (static_cast<uint32_t>(ri(0, cnt - 1)) << 2));
                            if (car != 0) give_hit(car, 0xffffffffu);
                        }
                        common(m);
                        if (st == 1) w32(0x001413d4u, coin() ? 0x32u : 0u);
                    }, 3, 24);

                    // class 1265 (cargo train carriage) + pok_31ada0_l11
                    p112_spawned("p113_class1265", 0x003181f0, pok_class1265_update_l11, [common](int st) {
                        const uint32_t m = moby_spawn(0x4f1u);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        w8(m + 0x31, 1u);
                        w32(pv + 0x28, coin() ? 0u : 5u);
                        w32(L11_ADDR(0x0015ffbcu), coin() ? 100u : 50u);
                        ee_ref<Vec4>(m + 0x10) = Vec4{400.0f, 400.0f, 300.0f, 0.0f};
                        ee_ref<Vec4>(pv) = Vec4{399.0f, 400.0f, 300.0f, 0.0f};
                        moby_update_transform(m);
                        common(m);
                        if (st == 0 && coin()) {
                            for (uint32_t s = 1; s < 128; ++s) {
                                libc_srand(s);
                                if (rand_mod(ee_read<int32_t>(L11_ADDR(0x00162340u))) == 0) { libc_srand(s); break; }
                            }
                        }
                        return m;
                    }, 2, 24);

                    // class 1524 (falling sludge/bomb drop) + pok_31ae68_l11 + pok_31b098_l11
                    p112_spawned("p113_class1524", 0x0031ab08, pok_class1524_update_l11, [common](int st) {
                        const uint32_t T = 0x01e00000u;
                        const float wz = ee_read<float>(L11_ADDR(0x00161e18u));
                        ee_ref<Vec4>(T) = Vec4{400.0f, 400.0f, (st == 1 && coin()) ? (wz + 10.0f) : (wz - 1.0f), 0.0f};
                        ee_ref<Vec4>(T + 0x10) = Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), -0.5f, 0.0f};
                        const uint32_t m = pok_31ada0_l11(T, T + 0x10, L11_ADDR(0x00161e18u), 1.0f);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        w8(m + 0x31, (ri(0, 4) == 0) ? 0u : 1u);
                        w16(pv + 0x22, coin() ? 20u : 0u);
                        if (st == 2) {
                            wf(m + 0x2c, coin() ? 0.001f : 0.5f);
                            w32(L11_ADDR(0x0015f5ccu), coin() ? 4u : 5u);
                        }
                        common(m);
                        if (st == 1 && coin()) {
                            for (uint32_t s = 1; s < 128; ++s) {
                                libc_srand(s);
                                if (rand_mod(ee_read<int32_t>(L11_ADDR(0x00162410u))) == 0) { libc_srand(s); break; }
                            }
                        }
                        return m;
                    }, 3, 24);

                    // class 1297 (energy ball) + pok_319028_l11 + pok_3191c0_l11
                    p112_spawned("p113_class1297", 0x00318b30, pok_class1297_update_l11, [common, HERO](int st) {
                        const uint32_t T = 0x01e00000u;
                        ee_ref<Vec4>(T) = ee_ref<Vec4>(HERO);
                        const uint32_t m = pok_318d10_l11(ee_read<uint32_t>(0x001413d0u), T, 10);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        w8(m + 0x20, u(st));
                        w32(pv + 0x20, coin() ? 1u : 10u);
                        w32(pv + 0x28, coin() ? 1u : 10u);
                        ee_ref<Vec4>(pv) = Vec4{rf(-1.0f, 1.0f), rf(-1.0f, 1.0f), rf(-0.5f, 0.5f), 0.0f};
                        common(m);
                        return m;
                    }, 3, 24);

                    // class 1319 (Blarg bomber) + pok_30f5a8_l11
                    p111("p113_class1319", 0x00319838, pok_class1319_update_l11, c1319, [common, first_run, put_hero, setup_view_looking_at, c1264](uint32_t m, int st) {
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        if (st != 0) first_run(pok_class1319_update_l11, m);
                        if ((st == 1 || st == 2) && !c1264.empty()) {
                            const uint32_t tpv = ee_read<uint32_t>(c1264[0] + 0x78);
                            w32(pv + 0x60, ee_read<uint32_t>(tpv + 0x60));
                            w32(pv + 0x64, 0u);
                        }
                        w8(m + 0x20, u(st));
                        w32(pv + 0x88, (st == 0) ? (coin() ? 0u : 1u) : u(ri(0, 3)));
                        w8(m + 0x31, coin() ? 1u : 0u);
                        w8(m + 0xbc, coin() ? 1u : 0u);
                        w8(0x0014095fu, coin() ? 2u : 0u);
                        w32(pv + 0x84, coin() ? 100u : 0u);
                        w32(pv + 0x98, coin() ? 200u : 0u);
                        wf(pv + 0x68, coin() ? 0.99f : (coin() ? -0.1f : 0.5f));
                        wf(pv + 0x6c, coin() ? 0.4f : -0.4f);
                        if (st == 3) {
                            const Vec4 p = ee_ref<Vec4>(m + 0x10);
                            if (coin()) setup_view_looking_at(p.x, p.y, p.z, 10.0f, 0.0f);
                            put_hero(m + 0x10, coin() ? 10.0f : 50.0f, 0.0f);
                        } else if (st == 5) {
                            static const uint32_t fcs[] = {0u, 10u, 20u, 25u};
                            w32(L11_ADDR(0x0015f5ccu), fcs[ri(0, 3)]);
                        }
                        common(m);
                    }, 7, 16);

                    g_ignored_words.resize(sp_mark);
                }

                // class 1350 (air-combat skill-point monitor)
                p111("p113_class1350", 0x0031aa80, pok_class1350_update_l11, c1350, [common, moby_base](uint32_t m, int st) {
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w32(pv + 0x0, (st == 0) ? 0xffffffffu : 0u);
                    if (st == 2) {
                        const int32_t grp = ee_read<int32_t>(pv + 4);
                        if (grp >= 0) {
                            uint32_t e = ee_read<uint32_t>(L11_ADDR(0x001ac240u) + (static_cast<uint32_t>(grp) << 2));
                            if (e != 0) {
                                for (;; e += 2) {
                                    const uint16_t raw = ee_read<uint16_t>(e);
                                    w8(moby_base() + (static_cast<uint32_t>(raw & 0x7fffu) << 8) + 0x20, 0xfeu);
                                    if (static_cast<int16_t>(raw) < 0) break;
                                }
                            }
                        }
                    }
                    w8(0x0013d41bu, coin() ? 0u : 1u);
                    common(m);
                }, 3, 16);

                // class 1903 & 1919 (water-surface scroll classes) + pok_31e048_l11 + pok_31eea8_l11
                p111("p113_class1903", 0x0031e2f0, pok_class1903_update_l11, c1903, [common, setup_view_looking_at](uint32_t m, int st) {
                    w8(m + 0x20, u(st));
                    if (st == 0) w32(L11_ADDR(0x0016252cu), coin() ? 0u : 1u);
                    else {
                        const float dist = (ri(0, 2) == 0) ? rf(5.0f, 14.0f) : ((ri(0, 1) == 0) ? rf(16.0f, 34.0f) : rf(40.0f, 60.0f));
                        const float tx = ee_read<float>(L11_ADDR(0x001624e8u));
                        const float ty = ee_read<float>(L11_ADDR(0x001624ecu));
                        const float tz = ee_read<float>(L11_ADDR(0x001624f0u));
                        setup_view_looking_at(tx, ty, tz, dist, std::max(5.0f, 190.0f - tz));
                    }
                    common(m);
                }, 2, 24);

                p111("p113_class1919", 0x0031f150, pok_class1919_update_l11, c1919, [common, setup_view_looking_at](uint32_t m, int st) {
                    w8(m + 0x20, u(st));
                    if (st == 0) w32(L11_ADDR(0x001625fcu), coin() ? 0u : 1u);
                    else {
                        const float dist = (ri(0, 2) == 0) ? rf(10.0f, 75.0f) : ((ri(0, 1) == 0) ? rf(82.0f, 98.0f) : rf(105.0f, 130.0f));
                        const float tx = ee_read<float>(L11_ADDR(0x001625b8u));
                        const float ty = ee_read<float>(L11_ADDR(0x001625bcu));
                        const float tz = ee_read<float>(L11_ADDR(0x001625c0u));
                        setup_view_looking_at(tx, ty, tz, dist, std::max(5.0f, 190.0f - tz));
                    }
                    common(m);
                }, 2, 24);

                // Draw callback checks with live packet buffer & render_view_setup():
                // pok_cb_2f2d58_l11 (teleporter glow ring)
                if (!c318.empty()) {
                    l11check("p113_cb2f2d58", 0x002f2d58, 30, Ret::None, [level, c318, pick, setup_pkt_buf, setup_view_looking_at] {
                        level();
                        const uint32_t m = pick(c318), pv = ee_read<uint32_t>(m + 0x78);
                        const Vec4 p = ee_ref<Vec4>(m + 0x10);
                        setup_view_looking_at(p.x, p.y, p.z, 8.0f, 2.0f);
                        setup_pkt_buf();
                        const int band = ri(0, 2);
                        wf(pv + 0x3c, band == 0 ? rf(0.1f, 0.6f) : (band == 1 ? rf(0.7f, 1.1f) : rf(1.2f, 1.95f)));
                        return Args{{m}, {}};
                    }, [](A a) { pok_cb_2f2d58_l11(a.i[0]); return uint64_t{0}; });
                }

                // pok_cb_31db50_l11 (class 1903 water draw) + pok_31d8d8_l11
                if (!c1903.empty()) {
                    l11check("p113_cb31db50", 0x0031db50, 24, Ret::None, [level, c1903, setup_pkt_buf] {
                        level();
                        setup_pkt_buf();
                        w8(L11_ADDR(0x00162590u), coin() ? 0u : u(ri(1, 255)));
                        return Args{{c1903[0]}, {}};
                    }, [](A a) { pok_cb_31db50_l11(a.i[0]); return uint64_t{0}; });
                }

                // pok_cb_31e930_l11 (class 1919 water draw) + pok_31e6a8_l11
                if (!c1919.empty()) {
                    l11check("p113_cb31e930", 0x0031e930, 24, Ret::None, [level, c1919, setup_pkt_buf] {
                        level();
                        wf(L11_ADDR(0x001677c8u), coin() ? 260.0f : 200.0f);
                        setup_pkt_buf();
                        w8(L11_ADDR(0x00162610u), coin() ? 0u : u(ri(1, 255)));
                        return Args{{c1919[0]}, {}};
                    }, [](A a) { pok_cb_31e930_l11(a.i[0]); return uint64_t{0}; });
                }

                // pok_cb_311d50_l11 (fighter-jet HUD) + all 6 helpers (0x311710, 0x311a88, 0x311be8, 0x318050, 0x311948, 0x311c80)
                if (!c1242.empty()) {
                    // its 10 HUD-width hooks (the hooks.glue lines P11.3 carried into the port, 5f80a3bb^; each a jal and its return address
                    // on the disc's code): the radar's two glyph_sprite 0x22baf8, a blip 0x311710, a missile 0x310ad0 (loaded 0x311f58, not 0x311f98)
                    const HudHooksExpected hud_hooks({{0x00311df0, 2}, {0x00311df8, 0}, {0x00311e28, 2}, {0x00311e30, 0}, {0x00311ecc, 2},
                                                      {0x00311ed4, 0}, {0x00311f58, 1}, {0x00311f60, 0}, {0x00311f98, 1}, {0x00311fa0, 0}});
                    l11check("p113_cb311d50", 0x00311d50, 48, Ret::None, [level, c1242, c1319, first_run, setup_pkt_buf, setup_view_looking_at, common, moby_base] {
                        level();
                        const uint32_t m = c1242[0], pv = ee_read<uint32_t>(m + 0x78);
                        w8(0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84u) << 4), 0u);
                        first_run(pok_class1242_update_l11, m);
                        setup_view_looking_at(500.0f, 500.0f, 300.0f, 20.0f, 0.0f);
                        setup_pkt_buf();
                        w8(m + 0x20, (ri(0, 3) == 0) ? 8u : 4u);
                        w32(pv + 0xe0, 256u);
                        w32(pv + 0xe4, 208u);
                        w8(0x00140947u, u(ri(0, 10)));
                        w8(0x00140946u, u(ri(0, 10)));
                        wf(0x0014094cu, coin() ? 10.0f : 80.0f);
                        wf(0x00140950u, 100.0f);
                        wf(pv + 0xf0, rf(0.0f, 100.0f));
                        w32(pv + 0xf4, u(ri(2, 253)));
                        w32(pv + 0x11c, coin() ? 2000u : 0u);
                        w32(pv + 0x120, coin() ? 0u : 10u);
                        w32(pv + 0x128, coin() ? 4000u : 0u);
                        // Clear help busy words so help prompts (0x2afd/0x2afe/0x2aff) are accepted
                        w32(0x00151720u, 0u);
                        w32(0x001516d0u + 0x50, 0u);
                        w32(0x001516d0u + 0x1c, 0u);
                        w16(0x00141968u + 0x2c0, u(ri(0, 2)));
                        w16(0x00141968u + 0x2b8, u(ri(0, 2)));
                        w16(0x00141968u + 0x2b0, coin() ? 0u : 1u);
                        w8(0x0015edb4u, coin() ? 1u : 0u);
                        if (!c1319.empty()) {
                            w8(c1319[0] + 0x20, 5u);
                            w8(c1319[0] + 0x31, 1u);
                            ee_ref<Vec4>(c1319[0] + 0x10) = Vec4{500.0f, 500.0f, 300.0f, 0.0f};
                            w32(pv + 0x88, coin() ? c1319[0] : 0u);
                            w32(pv + 0x8c, coin() ? 100u : 5u);
                            // Ensure the first moby in the jet's target group (pv+0x108) is active and lists c1319[0] as a turret so pok_311948_l11/pok_311a88_l11 lock on
                            const int32_t grp = ee_read<int16_t>(pv + 0x108);
                            const uint32_t ge = ee_read<uint32_t>(L11_ADDR(0x001ac240u) + (static_cast<uint32_t>(grp) << 2));
                            if (ge != 0) {
                                const uint32_t bm = moby_base() + (static_cast<uint32_t>(ee_read<uint16_t>(ge) & 0x7fffu) << 8);
                                w8(bm + 0x20, 1u);
                                const uint32_t bpv = ee_read<uint32_t>(bm + 0x78);
                                w32(bpv + 0x64, 1u);
                                w32(bpv + 0x70, c1319[0]);
                            }
                        }
                        common(m);
                        return Args{{m}, {}};
                    }, [](A a) { pok_cb_311d50_l11(a.i[0]); return uint64_t{0}; });
                }
            }
        }

        g_extra_hooks = saved_hooks;
        g_host_clock_fixed = saved_clock;
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
