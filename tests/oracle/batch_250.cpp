// Oracle batch 250: level 16's (Kalebo III) own code on its real level data: its moby classes, the callbacks they install, the sound emitters,
// the camera types, the class hooks, the camera mode hook and its callback, the movement-mode hero update, moby_spawn and mobys_in_sphere
// (level 16's copies). Run with ORACLE_LEVEL=16 build/tests/ovl_oracle --batch 250 (needs the ISO; no dumps). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the original is level 16's own function at its
// own address, the port the one registered for it (the transliterations kalebo_classes_tr_N.cpp / kalebo_hero_tr_N.cpp, generated from the disc by
// tools/codegen/mkall_l16.py). Made after batch 124 (Pokitaru).
#include "ovl_oracle.h"
#include "trial_guard.h"

#include <game/enhance/advanced.h>
#include <game/level/level_data.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_class1111.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1356.h>
#include <game/ovl/shared_class1443.h>
#include <game/ovl/shared_class1952.h>
#include <game/ovl/shared_class235.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class52.h>
#include <game/ovl/shared_class638.h>
#include <game/ovl/shared_class933.h>
#include <game/ovl/shared_envmap.h>
#include <game/ovl/shared_envmap_meshes.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_hoverboard.h>
#include <game/ovl/shared_race_camera.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <cstring>
#include <string>
#include <map>
#include <unordered_set>

namespace rac1::oracle {

namespace {
// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.

constexpr uint32_t MOBY_SPAWN_LV = 0x00249f10;

// (stage 5, task C2a) The checks named here reach mobys_in_sphere (the level's copy), whose cell list and vertices the port keeps on
// the stack (the original: 0x70003800, 0x70002000): A2's collision scratch ranges are left out of these checks, and only of
// them. They also hide what moby_pose_matrices (called for a posed moby) writes inside them: the joint positions at
// 0x70000000 .., read back by the primitive tests, so a wrong one still shows in the hit list.
// (Task C2b: the checks added to the list reach sphere_collide or capsule_collide, whose chunk buffers,
// vertices and cell walk the port keeps on the host stack too.)
bool c2a_spr_check(const char* name) {
    static const char* const kNames[] = {"class52", "class281", "class541", "class1401", "class1445", "mobys_in_sphere",
                                         "class638", "hero_move_mode", "class1135", "class1455", "class1475", "class470",
                                         "class49", "class50", "class805", "class933",
                                         // (stage 4 P16.1's checks of the hand-written classes)
                                         "class1475_cases", "class281_cases", "class470_cases", "class654_cases", "class257_cases",
                                         // (stage 4 P16.2's: the fighter, the racers)
                                         "class541_cases", "class556_cases",
                                         // (stage 4 P16.2's: the race's organiser)
                                         "class1455_cases", "class1455_fix10"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is level 16's own function at `laddr` (not a Veldin address to translate).
void lvcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B250_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 16) {
        std::printf("%-26s L16:0x%08x  skipped (level 16 only: run with ORACLE_LEVEL=16)\n", name, laddr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L16:0x%08x\n", name, laddr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2a_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_250() {
    if (g_only_batch != 250) return;
    g_batch = 250;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00174610) + k);
    // the segment records (Veldin 0x173e90 .. 0x173ebc: 0x1efc70's sqc2 of VU0 registers the earlier checks left in the interpreter, the
    // port's VU0 state starting fresh; the classes read them only after a segment test wrote them): as batch 124's 0x173e9c
    for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
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
    // the level's mobys whose update function is `fn` (level 16's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}, uint32_t spawn_cls = 0) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 16) { lvcheck(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        // a class the level's gameplay data does not place (another class or a scene spawns it): a moby of the class spawned by the
        // game's own moby_spawn next to the hero, its first updates done by the check itself
        if (ms.empty() && spawn_cls == 0) { std::printf("%-26s L16:0x%08x  no moby on this level\n", name, fn); return; }
        lvcheck(name, fn, n, Ret::None, [level, ms, vary, port, spawn_cls] {
            // a random state that sends the port's code through a pointer outside RAM (a garbage pointer the state machine had not set yet: the
            // original's interpreter wraps it, the port refuses) is not a game state: it is dealt again (the RAM put back after the trial)
            static std::vector<uint8_t> saved;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 40; ++attempt) {
                level();
                if (ms.empty()) {
                    m = moby_spawn(spawn_cls);
                    if (m == 0) break;
                    Vec4 q = ee_ref<Vec4>(0x0013f3d0);
                    q.x += rf(-4, 4); q.y += rf(-4, 4); q.z += rf(0, 1);
                    ee_ref<Vec4>(m + 0x10) = q;
                } else {
                    m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                }
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
    if (g_oracle_level == 16) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l16.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    // skip_states: (848 / 1523 state 1: a spawned moby's unset links: the port refuses a pointer the interpreter wraps / the grid's missing-id trap;
    // the taxi 1410's states 3..5 on a placed taxi nobody rides: a pointer of -16 the port refuses, the interpreter wraps;
    // the states that request a level change (0x24d430's copy: the original would run the CD / movie path for real) or start a
    // music stream (its disc read: sceCdRead 0x121450 without the IOP's stream system, a TODO_PORT on the port's side; class 1419 state 1, 1446 states 5 / 7)
    static const TrCls trcls[] = {
        {"class49", 0x002a05e8, class49_update, 0x0, false},
        {"class50", 0x002a0a98, class50_update, 0x0, false},
        {"class52", 0x002a1088, class52_update, 0x0, false},
        {"class1475", 0x002a3f38, kal_class1475_update_l16, 0x0, false},
        {"class235", 0x002c3be8, class235_update, 0x0, false},
        {"class257", 0x002c3d38, kal_class257_update_l16, 0x0, false},
        {"class281", 0x002c5eb0, kal_class281_update_l16, 0x0, false},
        {"class439", 0x002c7788, class439_update, 0x0, false},
        {"class470", 0x002c9480, kal_class470_update_l16, 0x0, false},
        {"class471", 0x002c9878, kal_class471_update_l16, 0x0, false},
        {"class482", 0x002cb600, kal_class482_update_l16, 0x0, false},
        {"class541", 0x002cddb8, kal_class541_update_l16, 0x0, false},
        {"class546", 0x002cf198, kal_class546_update_l16, 0x0, false},
        {"class552", 0x002cf4a8, kal_class552_update_l16, 0x0, false},
        {"class556", 0x002d04a0, kal_class556_update_l16, 0x0, false},
        {"class605", 0x002d2a80, zone_marker_update, 0x0, false},
        {"class638", 0x002d2cf0, class638_update, 0x0, false},
        {"class647", 0x002d59e0, kal_class647_update_l16, 0x0, false},
        {"class650", 0x002d5cb8, kal_class650_update_l16, 0x0, false},
        {"class654", 0x002d5ef8, kal_class654_update_l16, 0x0, false},
        {"class758", 0x002d6378, swingshot_target_update, 0x0, false},
        {"class800", 0x002d6740, kal_class800_update_l16, 0x0, false},
        {"class805", 0x002d69e0, spawn_point_update_l1, 0x0, false},
        {"class876", 0x002dddd8, empty_update, 0x0, false},
        {"class933", 0x002ddde0, class933_update, 0x0, false},
        {"class1111", 0x002dee10, kal_class1111_update_l16, 0x0, false},
        {"class1134", 0x002df1e8, gold_bolt_update, 0x6, false},
        {"class1135", 0x002e0120, teleporter_update_l1, 0x0, false},
        {"class1139", 0x002e0f78, class1139_update, 0x0, false},
        {"class1140", 0x002e1080, empty_update, 0x0, false},
        {"class1356", 0x002e22d8, class1356_update, 0x0, false},
        {"class1377", 0x002e3190, kal_class1377_update_l16, 0x4, false},
        {"class1387", 0x002e36e8, kal_class1387_update_l16, 0x0, false},
        {"class1401", 0x002e37a0, kal_class1401_update_l16, 0x0, false},
        {"class1410", 0x002e43e0, kal_class1410_update_l16, 0x38, false},
        {"class1439", 0x002e5010, kal_class1439_update_l16, 0x0, false},
        {"class1441", 0x002e5258, kal_class1441_update_l16, 0x0, false},
        {"class1442", 0x002e54a0, kal_class1442_update_l16, 0x0, false},
        {"class1443", 0x002e5708, class1443_update, 0x0, false},
        {"class1445", 0x002e5e08, kal_class1445_update_l16, 0x0, false},
        {"class1455", 0x002e6808, kal_class1455_update_l16, 0x10, false},
        {"class1561", 0x002e7208, kal_class1561_update_l16, 0x0, false},
        {"class1667", 0x002e76b8, kal_class1667_update_l16, 0x0, false},
        {"class1812", 0x002e7e00, kal_class1812_update_l16, 0x0, false},
        {"class1826", 0x002e7f80, kal_class1826_update_l16, 0x0, false},
        {"class1891", 0x002e88d0, kal_class1891_update_l16, 0x0, false},
        {"class1923", 0x002e8970, kal_class1923_update_l16, 0x0, false},
        {"class1943", 0x002e8e80, kal_class1943_update_l16, 0x0, false},
        {"class1947", 0x002e93b0, kal_class1947_update_l16, 0x0, false},
        {"class1948", 0x002e98e0, envmap_1948_update, 0x0, false},
        {"class1952", 0x002e9df0, class1952_update, 0x0, false},
        {"class1953", 0x002ea128, envmap_1953_update, 0x0, false},
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
    if (g_oracle_level == 16 && std::getenv("B250_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B250_CLS") != nullptr && std::strcmp(std::getenv("B250_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 16 && std::getenv("B250_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B250_ST") == nullptr || std::atoi(std::getenv("B250_ST")) == st) && (std::getenv("B250_PASS") == nullptr || std::atoi(std::getenv("B250_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12, {}, static_cast<uint32_t>(std::atoi(c.name + 5)));
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 16's own moby callbacks (draw callbacks / update functions a class installs): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 16) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        // (the hero's callbacks, installed by its hero_set_state: batch 242, on the race's dumps)
        static const Cb cbs[] = {
        {"cb2c9cd0", 0x002c9cd0, kal_cb_2c9cd0_l16, 0x002c9878, 471},
        {"cb2cef60", 0x002cef60, kal_cb_2cef60_l16, 0x00000000, 0},
        {"cb2cf7a8", 0x002cf7a8, kal_cb_2cf7a8_l16, 0x002cf4a8, 552},
        {"cb2de3c0", 0x002de3c0, kal_cb_2de3c0_l16, 0x002dee10, 1111},
        {"cb2e0a38", 0x002e0a38, class1135_draw, 0x002e0120, 1135},
        {"cb2e58d8", 0x002e58d8, class1443_draw, 0x002e5708, 1443},
        {"cb2e84e8", 0x002e84e8, kal_cb_2e84e8_l16, 0x002e7f80, 1826},
        {"cb2e8d40", 0x002e8d40, envmap_1943_draw, 0x002e8e80, 1943},
        {"cb2e9270", 0x002e9270, kal_cb_2e9270_l16, 0x002e93b0, 1947},
        {"cb2e97a0", 0x002e97a0, envmap_1948_draw, 0x002e98e0, 1948},
        {"cb2e9ce0", 0x002e9ce0, class1952_draw, 0x002e9df0, 1952},
        {"cb2e9fe8", 0x002e9fe8, envmap_1953_draw, 0x002ea128, 1953},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B250_CLS") != nullptr && std::strcmp(std::getenv("B250_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L16:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            const size_t ign_pk = g_ignored_words.size();
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : class49_update, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 16's sound emitter types (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 16) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x002f58a0, emitter_fade_box_update}, {"emitter3", 0x002f5c40, emitter_reverb_box_update}, {"emitter6", 0x002f5da0, emitter_music_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L16:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B250_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            lvcheck(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 16's class hooks (the five-hook table of section 3): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 16) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        // (stage 4 P16.2: the hand-written hooks of class 1455's table 0x1e9050: the shared carry_state_hook with Kalebo's
        // constants (carried 7, released 1), kalebo_race_organiser.cpp's +0xc / +0x10; a third of the cases start in state 7, the
        // carried state, so that the release's branch runs. hook_dispatch_l16 below checks that the level's dispatch,
        // class_hook_call_level, runs the same as these rows.)
        struct Hk { const char* name; uint32_t addr; uint32_t slot; int32_t (*fn)(uint32_t, uint32_t, uint32_t, float); };
        static const Hk hks[] = {
            {"hook_2e65d0", 0x2e65d0, 0x0, [](uint32_t m, uint32_t a1, uint32_t a2, float f) { return carry_state_hook(m, a1, a2, f, 0, 7, 1); }},
            {"hook_2e6620", 0x2e6620, 0x4, [](uint32_t m, uint32_t a1, uint32_t a2, float f) { return carry_state_hook(m, a1, a2, f, 4, 7, 1); }},
            {"hook_2e6670", 0x2e6670, 0x8, [](uint32_t m, uint32_t a1, uint32_t a2, float f) { return carry_state_hook(m, a1, a2, f, 8, 7, 1); }},
            {"hook_2e66c0", 0x2e66c0, 0xc, kal_hook_2e66c0_l16},
            {"hook_2e66f0", 0x2e66f0, 0x10, [](uint32_t m, uint32_t, uint32_t, float) { return static_cast<int32_t>(kal_hook_2e66f0_l16(m)); }},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L16:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B250_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            const bool getter = hk.slot == 0x10;  // (the pvars getter: also asked for no moby)
            lvcheck(hk.name, hk.addr, 40, Ret::Int32, [level, own, getter] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t m = getter && ri(0, 3) == 0 ? 0u : own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                if (m == 0) return Args{{0u, 0u, 0u}, {0.0f}};
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                else if (ri(0, 2) == 0) {
                    w8(m + 0x20, 7);
                    if (coin()) w16(ee_read<uint32_t>(m + 0x78) + 0x60 + 0x68, 8);  // (the carry record's state 8: the carry hooks answer 0, the release)
                }
                const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
            }, [fn](A a) { return r32(static_cast<uint32_t>(fn(a.i[0], a.i[1], a.i[2], a.f[0]))); });
        }
        // the level's dispatch (the game's call of a class hook by its address) against the rows above, port against port, from
        // the same state: the same answer and the same RAM
        if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "hook_dispatch_l16") == 0) {
            int bad = 0, n = 0;
            static std::vector<uint8_t> start, direct;
            for (const Hk& hk : hks) {
                for (int c = 0; c < 30; ++c) {
                    level();
                    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                    std::vector<uint32_t> own;
                    for (uint32_t m = first; m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                        if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
                    }
                    if (own.empty()) break;
                    const uint32_t m = own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                    if (ri(0, 2) == 0) {
                        w8(m + 0x20, 7);
                        if (coin()) w16(ee_read<uint32_t>(m + 0x78) + 0x60 + 0x68, 8);  // (the release, as above)
                    }
                    const uint32_t a1 = coin() ? 0x0013f3d0u : m + 0x10, a2 = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                    const float f12 = rf(-1, 3);
                    seed_rand();
                    uint8_t* ram = ee_get_ram_base();
                    start.assign(ram, ram + 0x02000000);
                    const int32_t r0 = hk.fn(m, a1, a2, f12);
                    direct.assign(ram, ram + 0x02000000);
                    std::memcpy(ram, start.data(), start.size());  // (the random seed with it)
                    int32_t r1 = 0;
                    const bool found = class_hook_call_level(hk.addr, m, a1, a2, f12, r1);
                    ++n;
                    uint32_t diff = 0;  // (the first word that differs, the EE stack's frames left out)
                    for (uint32_t x = 0x00100000; x < 0x02000000 && diff == 0; x += 4) {
                        if (x >= ee_stack_ptr() - 0x10000 && x < ee_stack_ptr() + 0x10000) continue;
                        if (std::memcmp(direct.data() + x, ram + x, 4) != 0) diff = x;
                    }
                    if (!found || r0 != r1 || diff != 0) {
                        if (bad < 8) std::printf("  hook_dispatch_l16 %s case %d: found %d answer %d / %d, first word differing %08x (%08x / %08x, sp %08x)\n", hk.name, c, found, r1, r0, diff, diff ? ee_read<uint32_t>(diff) : 0u, diff ? *reinterpret_cast<const uint32_t*>(direct.data() + diff) : 0u, ee_stack_ptr());
                        ++bad;
                    }
                }
            }
            std::printf("%-26s %s (%d cases)\n", "hook_dispatch_l16", bad == 0 && n > 0 ? "ok" : "FAIL", n);
            if (bad != 0 || n == 0) ++g_fail;
        }
        g_extra_noop_hooks.clear();
    }

    // ---- level 16's movement-mode hero update (L16:0x00204958): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 16) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        lvcheck("hero_move_mode", 0x00204958, 300, Ret::None, [level] {
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

    // ---- moby_spawn (the Veldin port) against level 16's own copy (L16:0x00249f10): the classes' spawns all go through it
    if (g_oracle_level == 16) {
        lvcheck("moby_spawn", MOBY_SPAWN_LV, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x31, 0x32, 0x34, 0x5c3, 0xeb, 0x101, 0x119};  // (classes of level 16's own table: 49, 50, 52, 1475, 235, 257, 281)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 16's camera hook (0x2f3a88, the pointer its setter puts at the hook slot) and the callback it queues (0x2f3ae8)
    if (g_oracle_level == 16) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002f3a88u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        lvcheck("cam0_hook", 0x002f3a88, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        lvcheck("cam_callback", 0x002f3ae8, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 16's copy L16:0x001f10d0) on the level's own mobys
    if (g_oracle_level == 16) {
        lvcheck("mobys_in_sphere", 0x001f10d0, 600, Ret::Int32, [level] {
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

    // ---- level 16's camera types (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 16) {
        // a live camera of the type, the hero put in or near its box, its data varied
        // a camera's +0x8c is its row in the level's type table (section 4), not the type: the rows of the level's own types
        auto cam_row = [](int type) -> int { switch (type) { case 3: return 1; case 8: return 6; default: return -1; } };
        auto cam_setup = [level, cam_row](int type, bool with_other, uint32_t* other) -> uint32_t {
            level();
            uint32_t cam = 0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == cam_row(type)) cam = OV(0x00167150) + i * 0xa0;
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
        static const CamT cams[] = {{3, 0x002f0d88, 0x002f0d90, 0x002f0308, 0x002f0fe0}, {8, 0x002f51e0, 0x002f51e8, 0x002f5260, 0x00000000}};
        for (const CamT& ct : cams) {
            level();
            bool any = false, own = false;
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) { any = true; if (ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == cam_row(ct.type)) own = true; }
            if (!any) { std::printf("cam_type%d                 no live camera on this level\n", ct.type); continue; }
            if (!own) std::printf("cam_type%d                 no live camera of the type on this level: checked on another live camera's words\n", ct.type);
            const int type = ct.type;
            static char nm[4][48];
            static int k = 0;
            auto name = [&](const char* what) { char* p = nm[k++ & 3]; std::snprintf(p, 48, "cam_type%d_%s", type, what); return static_cast<const char*>(p); };
            if (type == 3) {
            lvcheck(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(zero_stub()) : 0u); });
            lvcheck(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) cam_type3_init(a.i[0]); return uint64_t{0}; });
            lvcheck(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) cam_type3_update(a.i[0]); return uint64_t{0}; });
            lvcheck(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) cam_type3_exit(a.i[0]); return uint64_t{0}; });
            }
            if (type == 8) {
            lvcheck(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(zero_stub()) : 0u); });
            lvcheck(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) race_cam_init(a.i[0]); return uint64_t{0}; });
            lvcheck(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) race_cam_update(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 16) {
        static const uint32_t empties[] = {0x002dddd8, 0x002e1080};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            lvcheck(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- stage 4 P16.1: the hand-written classes (kalebo_*.cpp) in the states the generic loop above leaves unreached (measured
    // with ORACLE_COVER_EE): built from the level's own mobys, boxes, paths and groups; the frame time at its 50 Hz value in a
    // quarter of the cases; the hero's moby (and his box point +0xd0) moved with his position. Only the batch's first ignore
    // list here (not the debris / particle w lanes of the generic loop): a check that needs one says so.
    if (g_oracle_level == 16) {
        g_ignored_words = saved_ignored;
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        const uint32_t P = 0x0013f350;
        auto hz50 = [] { if (ri(0, 3) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); } };
        auto hero_to = [P](Vec4 q) {
            ee_ref<Vec4>(P + 0x80) = q;
            ee_ref<Vec4>(P + 0xd0) = q;
            const uint32_t hm = ee_read<uint32_t>(0x001413d0);
            if (hm != 0) ee_ref<Vec4>(hm + 0x10) = q;
        };
        auto finish = [hz50] { hz50(); w32(OV(0x0015f5cc), R() & 0xffff); seed_rand(); };
        auto live = [](uint32_t cls_addr_or_0, int16_t cls) {  // the level's live mobys of class `cls` (any class: -1)
            std::vector<uint32_t> v;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && (cls < 0 || ee_read<int16_t>(m + 0xa6) == cls) && (cls_addr_or_0 == 0 || ee_read<uint32_t>(m + 0x74) == cls_addr_or_0)) v.push_back(m);
            return v;
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v.empty() ? 0u : v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto index_of = [](uint32_t m) { return (m - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8; };
        auto box_centre = [](int32_t i) { return ee_ref<Vec4>(lvd::box(lvd::BOXES, i) + 0x30); };
        {  // the play-test spots: each placed moby of the hand-written classes (and of 541 / 556), its index, uid and place
            level();
            static const std::pair<uint32_t, int> kSpots[] = {{0x002c3d38, 257}, {0x002c9480, 470}, {0x002c9878, 471}, {0x002cf198, 546},
                {0x002cf4a8, 552}, {0x002d59e0, 647}, {0x002d5cb8, 650}, {0x002d5ef8, 654}, {0x002cddb8, 541}, {0x002d04a0, 556}};
            for (const auto& [fn, cls] : kSpots)
                for (uint32_t m : live(fn, -1))
                    std::printf("INFO P16 class %d idx %u uid %d at %.1f %.1f %.1f\n", cls, index_of(m), ee_read<int16_t>(m + 0xb2),
                                ee_read<float>(m + 0x10), ee_read<float>(m + 0x14), ee_read<float>(m + 0x18));
        }

        // class 1475, the rocket (spawned): homing on a target after its first frames, near the floor heading down, far from the
        // hero, its time running out, below 2 on an axis
        classcheck("class1475_cases", 0x002a3f38, kal_class1475_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int kind = ri(0, 4);
            w8(m + 0x20, ri(0, 7) == 0 ? 1u : 0u);
            w16(pv + 6, u(ri(0, 14)));
            wf(pv + 0x10, rf(0, 0.6f));
            w32(pv + 8, kind == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x001413d0) : pick(live(0, -1))));
            w32(pv, coin() ? 0u : pick(live(0, -1)));
            w16(pv + 4, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 60)));
            wf(m + 0x44, rf(-1.6f, 1.6f)); wf(m + 0x48, rf(-3.1f, 3.1f));
            wfrand(pv + 0x14, 2, -0.5f, 0.5f);
            Vec4 h = ee_ref<Vec4>(P + 0x80);
            Vec4 q = h;
            if (kind == 1) { q.x += rf(-1, 1); q.y += rf(-1, 1); q.z += rf(0.05f, 0.6f); }      // just above the hero's floor
            else { q.x += rf(-6, 6); q.y += rf(-6, 6); q.z += rf(-1, 3); }
            if (kind == 2) { h.x += coin() ? 61.0f : rf(50, 70); hero_to(h); }                  // the hero far
            if (kind == 3) { if (coin()) q.x = rf(-1, 2.5f); else q.z = rf(-1, 2.5f); }          // near the level's corner
            ee_ref<Vec4>(m + 0x10) = q;
            finish();
        }, 60, {}, 1475);

        // class 257, the path runner (placed): waiting with the hero free or not, in its box or not; running near its path's end,
        // near the hero; at the end with the view set up (the path's start on screen or not); a runner without a box / path
        classcheck("class257_cases", 0x002c3d38, kal_class257_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 3);
            w8(m + 0x20, u(st));
            w32(0x001413dc, coin() ? 0xfu : u(ri(0, 0x20)));
            if (st == 0 && ri(0, 2) == 0) w32(pv + (coin() ? 0u : 4u), 0xffffffffu);
            const int32_t box = ee_read<int32_t>(pv), path_i = ee_read<int32_t>(pv + 4);
            if (st == 1 && box >= 0 && coin()) hero_to(box_centre(box));
            if (st == 2 && path_i >= 0) {
                const uint32_t path = lvd::path(path_i);
                const float last = static_cast<float>(ee_read<int32_t>(path) - 1);
                wf(pv + 0x10, coin() ? rf(last - 0.3f, last + 0.1f) : rf(0, last));
                wf(pv + 0xc, rf(0, 0.2f));
                w8(m + 0xbc, u(ri(0, 1)));
                w16(pv + 0x16, u(ri(0, 1)));
                w16(pv + 0x14, ri(0, 2) == 0 ? 0xffffu : u(ri(0, 20)));
                if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-8, 8); q.y += rf(-8, 8); hero_to(q); }
            }
            if (st == 3) {
                w8(m + 0x31, u(ri(0, 1)));
                if (path_i >= 0 && coin()) { Vec4 q = ee_ref<Vec4>(lvd::path(path_i) + 0x10); q.x += rf(-20, 20); q.y += rf(-20, 20); q.z += rf(0, 6); ee_ref<Vec4>(OV(0x00166dc0)) = q; }
                if (coin()) render_view_setup();
            }
            finish();
        }, 60);

        // class 281, the bomb (spawned next to the hero): in flight just above his floor (bouncing), resting, its fuse at the
        // blinks' edges, blowing up
        classcheck("class281_cases", 0x002c5eb0, kal_class281_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 7) == 0 ? ri(0, 4) : ri(1, 2);
            w8(m + 0x20, u(st));
            const uint32_t fighter = coin() ? pick(live(0x002cddb8, 0x21d)) : 0u;  // (half at a fighter's place: the level's ground)
            Vec4 q = ee_ref<Vec4>(fighter != 0 ? fighter + 0x10 : P + 0x80);
            q.x += rf(-1.5f, 1.5f); q.y += rf(-1.5f, 1.5f); q.z += rf(0.0f, 0.8f);
            ee_ref<Vec4>(m + 0x10) = q;
            wf(pv + 0x10, rf(-0.3f, 0.3f)); wf(pv + 0x14, rf(-0.3f, 0.3f)); wf(pv + 0x18, coin() ? rf(-0.4f, 0.0f) : rf(-0.01f, 0.01f));
            if (fighter != 0 && coin()) {  // thrown through the fighter (its move crosses the fighter's collision: the segment test)
                const Vec4 c = ee_ref<Vec4>(fighter + 0x10);
                const float a = rf(-3.1f, 3.1f), r = rf(0.6f, 1.2f);
                ee_ref<Vec4>(m + 0x10) = Vec4{c.x + r * std::cos(a), c.y + r * std::sin(a), c.z + rf(0.3f, 1.2f), c.w};
                const Vec4 q2 = ee_ref<Vec4>(m + 0x10);
                const float k = rf(1.5f, 2.5f);
                wf(pv + 0x10, (c.x - q2.x) * k); wf(pv + 0x14, (c.y - q2.y) * k); wf(pv + 0x18, (c.z + 0.6f - q2.z) * k);
            }
            wf(pv + 0x24, rf(0, 0.02f));
            w32(pv + 0x20, coin() ? 0u : ee_read<uint32_t>(0x001413d0));
            const int every = st == 1 ? 10 : 20;
            w32(pv + 0x28, u(ri(0, 2) == 0 ? ri(0, 3) : every * ri(0, 6) + ri(-1, 1)));
            finish();
        }, 80, {}, 281);

        // class 470, the beacon (placed, turning): the glows' frames at the wrap's edges, their timers running out
        classcheck("class470_cases", 0x002c9480, kal_class470_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, ri(0, 5) == 0 ? 0u : 1u);
            for (uint32_t k = 0; k < 4; ++k) {
                const int e = ri(0, 2);
                wf(pv + k * 4, e == 0 ? rf(252, 255) : (e == 1 ? rf(-1, 3) : rf(0, 255)));
                wf(pv + 0x10 + k * 4, rf(-3, 3));
                w32(pv + 0x20 + k * 4, u(ri(0, 2) == 0 ? ri(0, 2) : ri(3, 255)));
            }
            finish();
        }, 60);

        // class 546, the switch (placed): set up done for good or not; dim and hit; lit with its time near its end (the fast
        // blink) or out; the class 1387 moby it names
        const std::vector<uint32_t> m1387 = [&] { level(); return live(0, 0x56b); }();
        classcheck("class546_cases", 0x002cf198, kal_class546_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int st = ri(0, 2);
            w8(m + 0x20, u(st));
            if (st == 0 && coin()) w8(0x0014c050u + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0), 0xff);
            const int t = ri(0, 3);
            w32(pv + 0x64, t == 0 ? 0xffffffffu : (t == 1 ? 0u : u(ri(1, 300))));
            w32(pv + 0x60, ri(0, 2) == 0 ? 0xffffffffu : (!m1387.empty() && coin() ? index_of(pick(m1387)) : index_of(pick(live(0, -1)))));
            w32(pv + 0x6c, u(ri(0, 2) == 0 ? ri(0, 3) : ri(60, 120)));
            wf(pv + 0x68, rf(-3.1f, 3.1f));
            if (st == 1) give_hit(m, 0x330000u);
            finish();
        }, 80);

        // class 552, the post (placed): the beam up or down (a switch of its group lit), its hum's slot playing for it or not
        classcheck("class552_cases", 0x002cf4a8, kal_class552_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, ri(0, 4) == 0 ? 0u : 1u);
            if (ri(0, 5) == 0) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) ^ 2u);
            if (ri(0, 3) == 0) w32(pv, 0);
            if (ri(0, 2) == 0) {  // the group's posts at heights 0.05 .. 0.25 apart (the pairing's 0.1)
                uint32_t e = lvd::group(ee_read<uint8_t>(m + 0x21));
                for (; e != 0; e += 2) {
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ee_read<int16_t>(o + 0xa6) == 0x228 && o != m) wf(o + 0x18, ee_read<float>(m + 0x18) + rf(-0.25f, 0.25f));
                    if (ee_read<int16_t>(e) < 0) break;
                }
            }
            if (ri(0, 7) == 0) {  // a group with no list
                for (int32_t g = 0; g < lvd::group_count(); ++g) if (lvd::group(g) == 0) { w8(m + 0x21, u(g)); break; }
            }
            if (coin()) {  // a switch of its group lit
                uint32_t e = lvd::group(ee_read<uint8_t>(m + 0x21));
                for (; e != 0; e += 2) {
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ee_read<int16_t>(o + 0xa6) == 0x222 && coin()) w8(o + 0xbc, 1);
                    if (ee_read<int16_t>(e) < 0) break;
                }
            }
            w32(pv + 8, u(ri(0, 1)));
            const int slot = ri(0, 3) == 0 ? -1 : ri(0, 15);
            w32(pv + 0xc, static_cast<uint32_t>(slot));
            if (slot >= 0) {
                const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70;
                w32(e + 0x88, coin() ? m : pick(live(0, -1)));
                w8(e + 0x74, u(ri(0, 3)));
            }
            finish();
        }, 80);

        // class 650, the floor plate (placed): its timer running, running out (the clunk, or not within 4 frames of the last)
        classcheck("class650_cases", 0x002d5cb8, kal_class650_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, ri(0, 5) == 0 ? 0u : 1u);
            w32(pv + 8, u(ri(0, 3) == 0 ? ri(0, 1) : ri(2, 6)));
            wf(pv + 4, rf(-0.2f, 0.2f));
            wf(pv, ee_read<float>(m + 0x18) + rf(-0.5f, 0.5f));
            finish();
            const uint32_t now = ee_read<uint32_t>(OV(0x0015f5cc));
            w32(L16_ADDR(0x00161a88), now + u(ri(-6, 6)));
        }, 60);

        // class 654, the fight's controller (placed): waiting with the hero in its box or not; the fight with its fighters left
        // or none (their states 0xfe), its second over or not; the end without a camera; set-up without a box / camera path
        classcheck("class654_cases", 0x002d5ef8, kal_class654_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int st = coin() ? 3 : ri(0, 6);  // (half in the fight: its camera)
            w8(m + 0x20, u(st));
            w16(0x0014162au, R() & 0xffffu);  // (the hero word the controller sets: not already its value)
            if (st == 0 && ri(0, 2) == 0) w32(pv + (coin() ? 0u : 0xcu), 0xffffffffu);
            const int32_t box = ee_read<int32_t>(pv);
            if (box >= 0 && coin()) hero_to(box_centre(box));
            if (coin()) {  // its fighters gone
                uint32_t e = lvd::group(ee_read<int32_t>(pv + 8));
                for (; e != 0; e += 2) {
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    w8(o + 0x20, 0xfe);
                    if (ee_read<int16_t>(e) < 0) break;
                }
            }
            w32(pv + 0x24, u(ri(0, 2) == 0 ? ri(0, 2) : ri(3, 60)));
            w32(pv + 0x14, coin() ? 0xffffffffu : u(ri(0, 2)));
            wf(pv + 0x1c, rf(0, 1)); wf(pv + 0x20, rf(-0.05f, 0.05f));
            finish();
        }, 80);

        // class 647, a fight's gate (placed): sliding toward an offset from another, at a speed
        classcheck("class647_cases", 0x002d59e0, kal_class647_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w8(m + 0x20, ri(0, 5) == 0 ? u(ri(0, 2)) : 1u);
            ee_ref<Vec4>(pv) = ee_ref<Vec4>(m + 0x10);
            wf(pv + 0x14, coin() ? 0.0f : rf(-4, 4));
            wf(pv + 0x18, coin() ? ee_read<float>(pv + 0x14) : rf(-4, 4));
            wf(pv + 0x10, rf(-0.2f, 0.2f));
            finish();
        }, 60);

        // ---- stage 4 P16.2: class 541, the fighter, and class 556, the racers (kalebo_fighter.cpp, kalebo_racers.cpp)
        // the moby slots' w lanes (debris, bolts, the bombs spawned: a stale VU0 lane in the original) and the particle pool's
        // velocity w lanes (the flame spray's particles), left out of the checks that spawn them, and only there
        std::vector<uint32_t> spawn_lanes;
        {
            level();
            for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { spawn_lanes.push_back(a + 0xc); spawn_lanes.push_back(a + 0x1c); }
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) spawn_lanes.push_back(pool + k * 0x40 + 0x2c);
        }
        auto with_spawn_lanes = [&](auto&& run) {
            const size_t before = g_ignored_words.size();
            g_ignored_words.insert(g_ignored_words.end(), spawn_lanes.begin(), spawn_lanes.end());
            run();
            g_ignored_words.resize(before);
        };
        auto walk = [](void (*fn)(uint32_t), uint32_t m) {  // one update by the port (a state it reaches on its own)
            const uint32_t sp0 = ee_stack_ptr();
            try { fn(m); } catch (const std::exception&) { ee_stack_ptr() = sp0; }
        };
        // a hit on the moby (the generic loop's hit record, with its state / damage words)
        auto hit_on = [give_hit, P](uint32_t m, uint32_t kinds) {
            give_hit(m, kinds);
            if (ee_read<uint8_t>(m + 0xa4) == 0xff) return;
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
            wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
        };
        const std::vector<uint32_t> fighters = [&] { level(); return live(0x002cddb8, 0x21d); }();
        auto fighter_path = [](uint32_t m) { return lvd::path(ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0xc4)); };
        // the animation's time put near 15 (state 4's throw as it crosses 15: the time one frame past or not yet)
        auto anim_near_15 = [](uint32_t m) {
            const uint8_t set = ee_read<uint8_t>(m + 0x53);
            w8(m + 0x52, set == 0xfe ? 0xfdu : (set ^ 1u));
            const uint32_t cur = ee_read<uint32_t>(m + 0x68);
            const float base = cur != 0 ? static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f : 0.0f;
            wf(m + 0x54, 15.0f - base + rf(-0.6f, 1.6f));
            wf(m + 0x58, 1.0f); wf(m + 0x5c, 1.0f);
        };
        if (std::getenv("B250_INFO") != nullptr)
            for (uint32_t f : fighters) { const uint32_t pv = ee_read<uint32_t>(f + 0x78); std::printf("INFO P16 fighter idx %u kind %d path %d lives %d plates %d carried %08x\n", index_of(f), ee_read<int32_t>(pv + 0xc0), ee_read<int32_t>(pv + 0xc4), ee_read<int32_t>(pv + 0xc8), ee_read<int32_t>(pv + 0xcc), ee_read<uint32_t>(pv + 0x104)); }

        // class 541 through its states: each state's fighter kind (3 / 4 the throwers, 5 / 6 the walkers, 7 / 8 the flames), at
        // its path's point or near it (the step's arrival), the timers at their edges, a hit (reeling, its last health), out of
        // health (+0x2e = 2) with lives left or none, the group's spin due or done this frame, the hero's moby where it walks
        with_spawn_lanes([&] {
        classcheck("class541_cases", 0x002cddb8, kal_class541_update_l16, [=](uint32_t m0) {
            const uint32_t pv = ee_read<uint32_t>(m0 + 0x78);
            static const int kByKind[4][10] = {{0, 2, 3, 3, 4, 4, 4, 9, 10, 11}, {0, 2, 5, 5, 5, 6, 6, 9, 10, 11},
                                               {0, 2, 7, 7, 8, 8, 8, 9, 10, 11}, {0, 2, 9, 10, 11, 0, 2, 9, 10, 11}};
            const int32_t kind = ee_read<int32_t>(pv + 0xc0);
            const int st = kByKind[kind >= 0 && kind <= 2 ? kind : 3][ri(0, 9)];
            const uint32_t path = fighter_path(m0);
            const int32_t n = path != 0 ? ee_read<int32_t>(path) : 0;
            if (st != 0 && ri(0, 4) != 0) { w8(m0 + 0x20, 0); walk(kal_class541_update_l16, m0); }  // (its set-up: what it carries spawned)
            w8(m0 + 0x20, u(st));
            w32(pv + 0xc8, u(ri(0, 2)));
            if (ri(0, 6) == 0) w8(pv + 0x2e, 2);
            w32(pv + 0x118, coin() ? ee_read<uint32_t>(OV(0x0015f5cc)) : 0u);
            wf(pv + 0x20, coin() ? rf(0.05f, 0.5f) : rf(1, 3));
            if (coin()) hit_on(m0, 0x330000u); else w8(m0 + 0xa4, 0xff);
            const uint32_t dc = n > 0 ? (st == 5 && ri(0, 3) != 0 ? u(coin() ? 0 : n - 1) : u(ri(0, 1))) : 0u;  // (state 5: the path's ends turn it)
            w32(pv + 0xdc, dc);
            w32(pv + 0xf0, u(ri(0, 2) == 0 ? ri(0, 2) : ri(3, 70)));
            wf(pv + 0xfc, coin() ? rf(0, 0.004f) : rf(0, 0.2f));
            w32(pv + 0xb4, coin() ? 2u : u(ri(0, 1)));
            w32(pv + 0x108, coin() ? 1u : 0xffffffffu);
            w8(m0 + 0x70, u(ri(0, 3)));
            if (coin()) w8(m0 + 0x53, u(ri(0, 0x17)));
            if (st == 0) { w32(pv + 0xc0, u(ri(0, 3))); if (coin()) w32(pv + 0x104, 0); if (coin()) w32(pv + 0xc8, 0); }
            if (path != 0 && n > 0 && st >= 2 && st <= 9) {  // at its point, near it, or a few away
                Vec4 q = ee_ref<Vec4>(path + 0x10 + (st == 2 ? 0u : st == 6 ? ((dc + ee_read<uint32_t>(pv + 0x108)) & 1u) : dc) * 0x10);  // (2 goes to the first point)
                const int near = st == 5 && coin() ? 0 : ri(0, 2);  // (state 5 arrives only within 1e-4)
                if (near == 1) { q.x += rf(-0.03f, 0.03f); q.y += rf(-0.03f, 0.03f); }
                else if (near == 2) { q.x += rf(-3, 3); q.y += rf(-3, 3); }
                ee_ref<Vec4>(m0 + 0x10) = q;
            }
            if (st == 4) anim_near_15(m0);
            if (st == 5) w8(m0 + 0x53, ri(0, 3) == 0 ? 0xau : 9u);
            if (st == 9) { static const uint8_t back[] = {3, 5, 7}; w32(pv + 0x110, back[ri(0, 2)]); w32(pv + 0x114, u(ri(0, 0x17))); }
            if (st == 5 && coin()) { Vec4 q = ee_ref<Vec4>(m0 + 0x10); q.x += rf(-0.3f, 0.3f); q.y += rf(-0.3f, 0.3f); hero_to(q); w32(0x0013f510, ri(0, 3) == 0 ? 5u : 0u); }
            else hero_near(m0);
            if (st == 11) wf(pv + 0xe8, ee_read<float>(m0 + 0x18) - rf(0, 2));
            finish();
        }, 160, fighters);
        });

        // class 541's helpers, each against its own address
        lvcheck("fighter_plate_find", 0x002d5db0, 80, Ret::Int32, [=] {
            level();
            const uint32_t m = pick(fighters);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int32_t g = ri(0, 4) == 0 ? ri(0, lvd::group_count() - 1) : ee_read<int32_t>(pv + 0xcc);
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            if (coin()) { q.x += rf(-40, 40); q.y += rf(-40, 40); }
            ee_ref<Vec4>(pv + 0x70) = q;  // (the point: in the fighter's own target slot)
            const uint32_t e = lvd::group(g);  // (a group whose list holds other classes would loop in both: only plates' groups or none)
            if (e != 0 && g != ee_read<int32_t>(pv + 0xcc)) {
                const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                if (ee_read<int16_t>(o + 0xa6) != 0x28a) return Args{{u(ee_read<int32_t>(pv + 0xcc)), pv + 0x70}, {}};
            }
            return Args{{u(g), pv + 0x70}, {}};
        }, [](A a) { return r32(kal_fighter_plate_find_l16(static_cast<int32_t>(a.i[0]), a.i[1])); });
        lvcheck("fighter_plate_press", 0x002d5e80, 60, Ret::None, [=] {
            level();
            const uint32_t plate = pick(live(0x002d5cb8, 0x28a));
            w32(ee_read<uint32_t>(plate + 0x78) + 8, coin() ? 0u : u(ri(1, 6)));
            finish();
            w32(L16_ADDR(0x00161a88), ee_read<uint32_t>(OV(0x0015f5cc)) + u(ri(-6, 6)));
            return Args{{plate}, {}};
        }, [](A a) { kal_fighter_plate_press_l16(a.i[0]); return uint64_t{0}; });
        lvcheck("fighter_step", 0x002ced20, 80, Ret::Int32, [=] {
            level();
            const uint32_t m = pick(fighters);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t path = fighter_path(m);
            const uint32_t t = path + 0x10 + u(ri(0, 1)) * 0x10;
            Vec4 q = ee_ref<Vec4>(t);
            const int near = ri(0, 2);
            if (near == 0) { const float r = rf(0.04f, 0.065f), a = rf(-3.1f, 3.1f); q.x += r * std::cos(a); q.y += r * std::sin(a); }  // (the arrival's 0.05 edge)
            else if (near == 1) { q.x += rf(-4, 4); q.y += rf(-4, 4); }
            ee_ref<Vec4>(m + 0x10) = q;
            wf(pv + 0xfc, near == 0 || coin() ? rf(0, 0.004f) : rf(0, 0.3f));
            wf(pv + 0x100, rf(-0.2f, 0.2f));
            finish();
            return Args{{m, t}, {rf(-3.1f, 3.1f)}};
        }, [](A a) { return r32(kal_fighter_step_l16(a.i[0], a.i[1], a.f[0])); });
        lvcheck("fighter_bomb_throw", 0x002c6268, 60, Ret::Int32, [=] {
            level();
            const uint32_t m = pick(fighters);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            ee_ref<Vec4>(pv + 0x70) = Vec4{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), rf(-0.1f, 0.3f), rf(-1, 1)};  // (the velocity: in the fighter's target slot)
            finish();
            return Args{{m, m + 0x10, pv + 0x70, u(ri(0, 300))}, {rf(0, 0.02f)}};
        }, [](A a) { return r32(kal_fighter_bomb_throw_l16(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0])); });
        lvcheck("fighter_group_spin", 0x002cee70, 60, Ret::None, [=] {
            level();
            const uint32_t m = pick(fighters);
            if (ri(0, 4) == 0) w8(m + 0x21, 0xff);
            wf(ee_read<uint32_t>(m + 0x78) + 0x11c, rf(-3.14f, 3.14f));
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_fighter_group_spin_l16(a.i[0]); return uint64_t{0}; });
        with_spawn_lanes([&] {
        lvcheck("fighter_frame", 0x002ce9f0, 120, Ret::None, [=] {
            level();
            const uint32_t m = pick(fighters);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            static const uint8_t kStates[] = {1, 2, 3, 5, 8, 9, 9, 10, 11};
            if (ri(0, 3) != 0) { w8(m + 0x20, 0); walk(kal_class541_update_l16, m); }  // (its set-up: what it carries spawned)
            w8(m + 0x20, kStates[ri(0, 8)]);
            if (ri(0, 4) == 0) { w8(pv + 0x2e, 2); w32(pv + 0xc8, u(ri(0, 1))); if (coin()) w32(pv + 0x104, 0); }
            w32(pv + 0x118, coin() ? ee_read<uint32_t>(OV(0x0015f5cc)) : 0u);
            wf(pv + 0x20, coin() ? rf(0.05f, 0.5f) : rf(1, 3));
            if (ri(0, 4) != 0) hit_on(m, 0x330000u); else w8(m + 0xa4, 0xff);
            if (coin()) w8(m + 0x53, 8);
            hero_near(m);
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_fighter_frame_l16(a.i[0]); return uint64_t{0}; });
        });

        // class 556, the racers (placed: five), made to race by one update of the port in state 1 with the hero racing, then:
        // their point on the main path, at a fork (a side path's point: w lane > 10), on a side path near its end, at a
        // take-off (w lane 1) on the ground; in the air (the trick's frames, the landing's pose); the start's timer running or
        // out; over a boost pad with the hero's board set (hoverboard_boost_pads(m, 0x260, 0x2f6, 0x2dc)); shot (0x30000); shot
        // off and coming back with the camera looking at it or away; out of the race; set-up (won or not, the attempts)
        const std::vector<uint32_t> racers = [&] { level(); return live(0x002d04a0, 0x22c); }();
        const std::vector<uint32_t> pads = [&] { level(); return live(0, 0x473); }();
        const uint32_t board = [&] { level(); return pick(live(0, 0x1b7)); }();
        auto take_offs = [](uint32_t path) {  // the points of `path` whose w lane is 1, when one of its points has a 2
            std::vector<int32_t> v;
            if (path == 0) return v;
            const int32_t n = ee_read<int32_t>(path);
            bool landing = false;
            for (int32_t i = 0; i < n; ++i) if (ee_read<float>(path + 0x1c + u(i) * 0x10) == 2.0f) landing = true;
            if (!landing) return v;
            for (int32_t i = 0; i < n; ++i) if (ee_read<float>(path + 0x1c + u(i) * 0x10) == 1.0f) v.push_back(i);
            return v;
        };
        if (std::getenv("B250_INFO") != nullptr) std::printf("INFO P16 racers %zu pads %zu board %08x\n", racers.size(), pads.size(), board);
        auto racer_prep = [=](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            w32(0x001413dc, 0x16);
            if (st >= 2) { w8(m + 0x20, 1); walk(kal_class556_update_l16, m); }
            w8(m + 0x20, u(st));
            const int32_t mainp = ee_read<int32_t>(pv + 0x290);
            uint32_t path = lvd::path(mainp);
            const int where = ri(0, 4);
            if (where == 3) {  // a side path (its slots +0x294 ..), near its end
                const int32_t si = ee_read<int32_t>(pv + 0x294 + u(ri(0, 3)) * 4);
                const uint32_t side = si >= 0 && si < lvd::path_count() ? lvd::path(si) : 0u;
                if (side != 0 && side != path && ee_read<int32_t>(side) >= 3 && ee_read<int32_t>(side) < 1000) path = side;
            }
            w32(pv + 0x2c0, path);
            const int32_t n = ee_read<int32_t>(path);
            int32_t i = n > 0 ? ri(0, n - 1) : 0;
            if (where == 1 && n > 0) { for (int32_t k = 0; k < n; ++k) if (ee_read<float>(path + 0x1c + u(k) * 0x10) > 10.0f && coin()) { i = (k + n - 1) % n; break; } }
            if (where == 2) { const std::vector<int32_t> t = take_offs(path); if (!t.empty()) i = t[static_cast<size_t>(ri(0, static_cast<int>(t.size()) - 1))]; }
            if (where == 3 && n > 2) i = n - 1 - ri(1, 2);
            if (take_offs(path).empty())  // (a take-off with no landing on its path: both sides would search for it without end)
                for (int32_t k = 0; k < n && ee_read<float>(path + 0x1c + u(i) * 0x10) == 1.0f; ++k) i = (i + 1) % n;
            w16(pv + 0x2f4, u(i));
            Vec4 q = ee_ref<Vec4>(path + 0x10 + u(i) * 0x10);
            q.x += rf(-2.5f, 2.5f); q.y += rf(-2.5f, 2.5f); q.z += coin() ? rf(-0.3f, 0.3f) : rf(-1, 3);
            ee_ref<Vec4>(pv + 0x260) = q;
            ee_ref<Vec4>(m + 0x10) = q;
            wf(m + 0x48, rf(-3.1f, 3.1f));
            wf(pv + 0x2c8, coin() ? rf(-0.3f, 0.0f) : rf(-0.3f, 0.3f));
            wf(pv + 0x2bc, rf(0.0f, 0.6f));
            w32(pv + 0x2e4, ri(0, 2) == 0 || where == 1 ? 0u : u(ri(1, 22)));  // (not in the air at a fork: the side path taken there
            // would leave the landing on the main path, and the rise's fit would walk the side path toward it without end)
            // (the landing at its point: the rise's fit walks from the point to the landing with the path's wrap, so a landing of
            // another path (the point may move onto a side path first) would never be reached; a jump's own landing comes from
            // the take-offs, where == 2)
            if (ee_read<int32_t>(pv + 0x2e4) != 0) { w16(pv + 0x2f0, u(i)); ee_ref<Vec4>(pv + 0x270) = ee_ref<Vec4>(path + 0x10 + u(i) * 0x10); }
            w16(pv + 0x2f6, ri(0, 2) == 0 ? 0u : u(ri(1, 200)));
            w16(pv + 0x2fe, ri(0, 2) == 0 ? 0xffffu : u(ri(1, 3)));
            { static const uint8_t sets[] = {0, 1, 2, 3, 5, 5}; const uint8_t s = sets[ri(0, 5)]; w8(m + 0x53, s); w8(m + 0x52, coin() ? s : sets[ri(0, 5)]); }
            w8(m + 0x70, u(ri(0, 3)));
            wf(m + 0x54, rf(0, 1));
            w32(P + 0x894, u(ri(0, 1000)));
            w32(P + 0x8c0, u(ri(0, 5)));
            for (uint32_t k = 0; k < 6; ++k) w16(P + 0x7c0 + k * 2, u(ri(0, 5)));
            if (ri(0, 2) == 0 && !pads.empty() && board != 0) {  // over a boost pad, the hero on his board
                w32(P + 0x86c, board);
                w32(P + 0x894, u(ri(200, 2000)));
                w32(pv + 0x2dc, coin() ? 0u : u(ri(0, 3)));
                Vec4 p = ee_ref<Vec4>(pick(pads) + 0x10);
                p.x += rf(-0.3f, 0.3f); p.y += rf(-0.3f, 0.3f); p.z -= 0.8f;
                ee_ref<Vec4>(pv + 0x260) = p;
            }
            if (ri(0, 4) == 0) hit_on(m, 0x30000u); else w8(m + 0xa4, 0xff);
            if (ri(0, 9) == 0) w32(0x001413dc, u(ri(0, 0x20)));
        };
        with_spawn_lanes([&] {
        classcheck("class556_cases", 0x002d04a0, kal_class556_update_l16, [=](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            static const int kStates[] = {0, 1, 2, 2, 2, 2, 2, 2, 3, 3};
            const int st = kStates[ri(0, 9)];
            racer_prep(m, st);
            if (st == 0) { w8(0x0013d4df, u(ri(0, 1))); w32(0x0015ee3c, u(ri(0, 40))); }
            if (st == 1) w32(pv + 0x2cc, u(ri(-1, 5)));
            if (st == 3) {
                w16(pv + 0x2f2, u(frame_scale_i(0xf0) + ri(-2, 2)));
                if (ri(0, 3) != 0) {  // the camera looking away from it (its yaw opposite the camera-to-racer heading)
                    const float dx = ee_read<float>(m + 0x10) - ee_read<float>(L16_ADDR(0x001671c0)), dy = ee_read<float>(m + 0x14) - ee_read<float>(L16_ADDR(0x001671c4));
                    float a = std::atan2(dy, dx) + 3.14159265f;
                    if (a > 3.14159265f) a -= 6.2831853f;
                    wf(L16_ADDR(0x001671d8), a + rf(-0.5f, 0.5f));
                } else {
                    wf(L16_ADDR(0x001671d8), rf(-3.1f, 3.1f));
                }
            }
            if (st == 2 && coin()) wf(pv + 0x2bc, ee_read<float>(pv + 0x2b4) + rf(-0.06f, 0.06f));  // (its speed near its target)
            finish();
        }, 160, racers);
        });
        lvcheck("racer_point", 0x002cf9d0, 40, Ret::None, [=] {
            level();
            const uint32_t m = pick(racers);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const int32_t n = ee_read<int32_t>(ee_read<uint32_t>(pv + 0x2c0));
            return Args{{m, u(ri(0, n > 0 ? n - 1 : 0)), pv + 0x270}, {}};
        }, [](A a) { kal_racer_point_l16(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2]); return uint64_t{0}; });
        lvcheck("racer_next_point", 0x002cf9f8, 120, Ret::None, [=] {
            level();
            const uint32_t m = pick(racers);
            racer_prep(m, 2);
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_racer_next_point_l16(a.i[0]); return uint64_t{0}; });
        lvcheck("racer_jump", 0x002cff48, 80, Ret::None, [=] {
            level();
            const uint32_t m = pick(racers);
            racer_prep(m, 2);
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_racer_jump_l16(a.i[0]); return uint64_t{0}; });
        lvcheck("racer_anims", 0x002d0058, 120, Ret::None, [=] {
            level();
            const uint32_t m = pick(racers);
            racer_prep(m, 2);
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (coin()) wf(pv + 0x278, ee_read<float>(pv + 0x268) - rf(-2, 6));
            if (coin()) w32(pv + 0x2e4, u(frame_scale_i(ri(0, 1) ? 7 : 11) + ri(-1, 1)));
            if (coin()) {  // a trick playing, its time between 5 and 22 (the speed's fit to the air time)
                w8(m + 0x52, ee_read<uint8_t>(m + 0x53)); w8(m + 0x51, 0); w8(m + 0x50, 1);
                const uint32_t cur = ee_read<uint32_t>(m + 0x68);
                const float base = cur != 0 ? static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f : 0.0f;
                wf(m + 0x54, rf(5, 22) - base);
                if (ee_read<int16_t>(pv + 0x2fe) == -1) w16(pv + 0x2fe, u(ri(1, 3)));
            }
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_racer_anims_l16(a.i[0]); return uint64_t{0}; });

        // ---- stage 4 P16.2: class 1455, the race's organiser (kalebo_race_organiser.cpp), on the level's own organiser. Kept short of
        // the scenes the oracle cannot run: a record of his talk starting its scene (talk_update: the scene mode 0x15f5c4 set, or the
        // hero out of the talk's reach, while state 1 runs), state 2's movie (talk +4 = 4 out of the scene mode) and state 4's prize
        // and level change (the scene mode kept set there).
        const std::vector<uint32_t> organisers = [&] { level(); return live(0x002e6808, -1); }();
        for (uint32_t m : organisers)
            std::printf("INFO P16 class 1455 idx %u uid %d at %.1f %.1f %.1f\n", index_of(m), ee_read<int16_t>(m + 0xb2),
                        ee_read<float>(m + 0x10), ee_read<float>(m + 0x14), ee_read<float>(m + 0x18));
        // the organiser in state `st` with what each state reads around its edges; `talk_free`: the talk may run (the hero out of
        // its reach), else the scene mode is set while state 1 runs
        // the talk's prompt: the hero 1 .. 2.5 in front of him facing him, in his normal state, the triangle not pressed and the
        // record waiting for it (+8 = 0): talk_update runs talk_step and the prompt, never the record's scene
        auto talk_prompt = [=](uint32_t m) {
            const uint32_t t = ee_read<uint32_t>(m + 0x78) + 0x20;
            const float yaw = ee_read<float>(m + 0x48) + rf(-0.5f, 0.5f), r = rf(1, 2.5f);
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            q.x += r * std::cos(yaw); q.y += r * std::sin(yaw);
            hero_to(q);
            float face = yaw + 3.14159265f + rf(-0.3f, 0.3f);
            if (face > 3.14159265f) face -= 6.2831853f;
            wf(P + 0x98, face);
            w32(OV(0x0015f5c4), 0u);
            w32(0x001413d4, 0u);
            w32(P + 0x22a8, 1u);
            w8(t + 8, 0);
            w32(0x0013cae4, R() & ~0x10u);
        };
        auto organiser_prep = [=](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78), t = pv + 0x20;
            w8(m + 0x20, u(st));
            w8(t + 9, 1);  // (the talk set up: talk_update does not run talk_init, which would reset +0x36)
            w8(m + 0x31, u(ri(0, 1)));
            if (ri(0, 3) == 0) {  // the camera within 28 of him or just past it
                Vec4 c = ee_ref<Vec4>(m + 0x10);
                const float r = coin() ? rf(0, 27.5f) : rf(27.5f, 29), a = rf(-3.1f, 3.1f);
                c.x += r * std::cos(a); c.y += r * std::sin(a);
                ee_ref<Vec4>(L16_ADDR(0x001671c0)) = c;
            }
            {  // the hero: within 3.5 (the help), within 8 in front of him or behind (the head's look), or farther
                const int where = ri(0, 3);
                const float r = where == 0 ? rf(0, 3.5f) : (where == 1 ? rf(3.5f, 8) : (where == 2 ? rf(0, 8) : rf(8, 30)));
                const float a = ee_read<float>(m + 0x48) + (where == 2 ? rf(1.6f, 4.6f) : rf(-1.5f, 1.5f));
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-0.5f, 2);
                hero_to(q);
                if (coin()) { Vec4 v{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-0.05f, 0.05f), 0}; if (coin()) v = Vec4{0.005f, 0, 0, 0}; ee_ref<Vec4>(P + 0x100) = v; }
                else ee_ref<Vec4>(P + 0x100) = Vec4{0, 0, 0, 0};
            }
            {  // the head's timers at their edges, its look point
                static const int32_t kT[] = {0, 0, 1, 2, 3, 60, 120};
                w32(pv + 0xc8, u(kT[ri(0, 6)]));
                w32(pv + 0xcc, u(kT[ri(0, 6)]));
                if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-6, 6); q.y += rf(-6, 6); q.z += rf(0, 3); ee_ref<Vec4>(pv + 0xd0) = q; }
            }
            if (ri(0, 4) == 0) w8(m + 0x53, u(ri(1, 2)));
            else w8(m + 0x53, 0);
            w8(0x0015edb0, u(ri(0, 1)));
            if (ri(0, 3) != 0) { w32(0x00151720, 0); w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu); }  // (the help and dialogue idle)
            w32(0x001413d4, coin() ? 0u : (coin() ? 0x1du : u(ri(0, 0x40))));  // (the hero's state)
            {  // the help's clock: the game's frame count and the last time (in 600-frame units) on both sides of 18 minutes
                const int32_t now = ri(0, 3) == 0 ? ri(0, 2000) : ri(2000, 200000);
                w32(0x0015eea4, u(now));
                const int k = ri(0, 3);
                w16(0x00141da2, k == 0 ? 0u : (k == 1 ? u(ri(0, 400)) : u(std::max(0, now / 600 + ri(-3, 1)))));
            }
            w8(0x0013d4de, ri(0, 3) == 0 ? 1u : 0u);
            w32(0x001413dc, ri(0, 2) == 0 ? 0x16u : (coin() ? 0xfu : (coin() ? 0u : u(ri(1, 0x20)))));  // (the hero's sub-state: 0x16 racing)
            w32(OV(0x0015f5c4), 1u);  // (the scene mode: talk_update quiet)
            if (st == 0) w32(0x0013dc1c, u(ri(0, 1)));
            if (st == 1) {
                static const int16_t k36[] = {3, 3, 3, 0, 1, 2, 4, -1};
                w16(t + 0x36, u(k36[ri(0, 7)]));
                w16(t + 4, u(ri(-1, 5)));
                const int talk = ri(0, 3);
                if (talk == 0) {  // the talk free to run, the hero out of its reach (twice +0xc)
                    w32(OV(0x0015f5c4), 0u);
                    Vec4 q = ee_ref<Vec4>(m + 0x10); const float a = rf(-3.1f, 3.1f), r = rf(8, 20);
                    q.x += r * std::cos(a); q.y += r * std::sin(a); hero_to(q);
                } else if (talk == 1) {
                    talk_prompt(m);
                }
                if (talk != 1 && coin()) w32(0x001413d4, 0x1du);
            }
            if (st == 2) {
                const bool scene = coin();
                w32(OV(0x0015f5c4), scene ? 2u : 0u);
                static const int16_t k4[] = {-1, 0, 1, 2, 3, 5};
                w16(t + 4, u(scene && coin() ? 4 : k4[ri(0, 5)]));
            }
            if (st == 3 || st >= 5) {
                const int k = ri(0, 3);
                w32(P + 0x8a8, u(k == 0 ? 4500 + ri(-2, 2) : (k == 1 ? ri(0, 4499) : ri(4501, 20000))));
                w8(0x0013d422, u(ri(0, 1)));
                w32(0x0015ee6c, u(coin() ? 4500 + ri(-2, 2) : ri(0, 9000)));
                if (coin()) w32(0x001413dc, 0u);
                w8(m + 0xbc, u(ri(0, 3) == 0 ? 0 : ri(1, 4)));
                w8(0x0013d507, u(ri(0, 1)));
            } else if (ri(0, 2) == 0) {
                w8(m + 0xbc, u(ri(0, 3)));
            }
            if (st == 4) w32(OV(0x0015f5c4), coin() ? 1u : 2u);  // (never 0: the prize and the level change)
        };
        classcheck("class1455_cases", 0x002e6808, kal_class1455_update_l16, [=](uint32_t m) {
            static const int kStates[] = {0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3, 4, 5, 9};
            organiser_prep(m, kStates[ri(0, 16)]);
            finish();
        }, 240, organisers);
        lvcheck("sea_height_set", 0x002de3b8, 20, Ret::None, [=] {
            level();
            return Args{{}, {coin() ? rf(60, 120) : rf(-1e4f, 1e4f)}};
        }, [](A a) { kal_sea_height_set_l16(a.f[0]); return uint64_t{0}; });
        lvcheck("race_course_on", 0x002e70c0, 30, Ret::None, [=] {
            level();
            const uint32_t m = pick(organisers);
            if (coin()) wf(L16_ADDR(0x0015f638), rf(0, 200));
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_race_course_on_l16(a.i[0]); return uint64_t{0}; });
        lvcheck("race_course_off", 0x002e7168, 30, Ret::None, [=] {
            level();
            const uint32_t m = pick(organisers);
            if (coin()) kal_race_course_on_l16(m);  // (the course shown first: its groups' flags change back)
            finish();
            return Args{{m}, {}};
        }, [](A a) { kal_race_course_off_l16(a.i[0]); return uint64_t{0}; });

        // 1.02 Gameplay, fix 10 in class 1455 (run with RAC1_GAMEPLAY102=10): the port with the fix on leaves what it leaves with the
        // fix off, with the hero's input lock (+0x20ac = 1) added where the race's scene ended (state 1, talk +0x36 = 3: "locked").
        // The guard before talk_update (the hero in the state 0x1d: "held") changes nothing on this disc's code, whose talk_update
        // returns at once in 0x1d: those cases must leave the same as without the fix.
        if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "class1455_fix10") == 0) {
            if (std::getenv("RAC1_GAMEPLAY102") == nullptr || organisers.empty()) {
                std::printf("%-26s skipped (run with RAC1_GAMEPLAY102=10)\n", "class1455_fix10");
                ++g_skipped;
            } else {
                int bad = 0, n = 0, locked = 0, held = 0, talked = 0;
                static std::vector<uint8_t> start, with_fix;
                const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                CollisionScratchIgnored spr;
                for (int c = 0; c < 90; ++c) {
                    level();
                    const uint32_t m = organisers[0], pv = ee_read<uint32_t>(m + 0x78), t = pv + 0x20;
                    organiser_prep(m, 1);
                    const int kind = c % 3;
                    w16(t + 0x36, kind == 0 ? 3u : u(ri(0, 2)));
                    if (kind == 1) w32(0x001413d4, 0x1du);
                    if (kind == 2) talk_prompt(m);  // the talk run in both (its prompt: the hero within its reach, not in 0x1d)
                    finish();
                    uint8_t* ram = ee_get_ram_base();
                    start.assign(ram, ram + 0x02000000);
                    const uint32_t sp0 = ee_stack_ptr();
                    bool threw = false;
                    g_advanced_disabled = false;
                    try { kal_class1455_update_l16(m); } catch (const std::exception&) { threw = true; }
                    g_advanced_disabled = true;
                    ee_stack_ptr() = sp0;
                    with_fix.assign(ram, ram + 0x02000000);
                    std::memcpy(ram, start.data(), start.size());
                    try { kal_class1455_update_l16(m); } catch (const std::exception&) { threw = true; }
                    ee_stack_ptr() = sp0;
                    if (threw) { if (bad < 8) std::printf("  class1455_fix10 case %d: the port threw\n", c); ++bad; continue; }
                    const bool lock = kind == 0;
                    if (lock) {
                        if (ee_read<uint8_t>(m + 0x20) != 3u || with_fix[P + 0x20ac] != 1u) { if (bad < 8) std::printf("  class1455_fix10 case %d: not locked (state %u)\n", c, ee_read<uint8_t>(m + 0x20)); ++bad; }
                        w8(P + 0x20ac, 1);
                        ++locked;
                    } else if (kind == 1) {
                        ++held;
                    } else {
                        ++talked;
                    }
                    ++n;
                    for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                        if (ignored.count(a) != 0) continue;
                        if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                        uint32_t xw, yw;
                        std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                        if (xw != yw) { if (bad < 8) std::printf("  class1455_fix10 case %d (kind %d): %08x fix=%08x expected=%08x\n", c, kind, a, xw, yw); ++bad; break; }
                    }
                }
                std::printf("%-26s %s (%d cases, %d locked, %d held, %d talked)\n", "class1455_fix10",
                            bad == 0 && locked > 0 && held > 0 && talked > 0 ? "ok" : "FAIL", n, locked, held, talked);
                if (bad != 0 || locked == 0 || held == 0 || talked == 0) ++g_fail;
            }
        }
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
