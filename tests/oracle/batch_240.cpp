// Oracle batch 240: level 15's (Quartu) own code on its real level data: its moby classes, the callbacks they install, the sound emitters,
// the camera types, the class hooks, the camera mode hook and its callback, cutscene_level_extras, the movement-mode hero update, moby_spawn and mobys_in_sphere
// (level 15's copies). Run with ORACLE_LEVEL=15 build/tests/ovl_oracle --batch 240 (needs the ISO; no dumps). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the original is level 15's own function at its
// own address, the port the one registered for it (the transliterations quartu_classes_tr_N.cpp / quartu_hero_tr_N.cpp, generated from the disc by
// tools/codegen/mkall_l15.py). Made after batch 124 (Pokitaru).
#include "ovl_oracle.h"
#include "trial_guard.h"

#include <game/ovl/helmet_tr.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/level/level_data.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/quartu_mobys.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/shared_class1108.h>
#include <game/ovl/shared_class1179.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class1224.h>
#include <game/ovl/shared_class123.h>
#include <game/ovl/shared_class123_draw.h>
#include <game/ovl/shared_class1331.h>
#include <game/ovl/shared_class1371.h>
#include <game/ovl/shared_class1523.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class193.h>
#include <game/ovl/shared_class196.h>
#include <game/ovl/shared_class250.h>
#include <game/ovl/shared_class256.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class28.h>
#include <game/ovl/shared_class28_draw.h>
#include <game/ovl/shared_class325.h>
#include <game/ovl/shared_class403.h>
#include <game/ovl/shared_class408.h>
#include <game/ovl/shared_class408_tint.h>
#include <game/ovl/shared_class44.h>
#include <game/ovl/shared_class49.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class655.h>
#include <game/ovl/shared_class655_draw.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_class77.h>
#include <game/ovl/shared_class78.h>
#include <game/ovl/shared_class78_draw.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class830.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class934.h>
#include <game/ovl/shared_clank_pad.h>
#include <game/ovl/shared_floor_switch.h>
#include <game/ovl/shared_gadget_pickups.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_ripples.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_shockwave.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shared_three_glows.h>
#include <game/ovl/shared_waves.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <optional>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.

constexpr uint32_t MOBY_SPAWN_LV = 0x0023de38;

// (stage 5, task C2a) The checks named here reach mobys_in_sphere (the level's copy), whose cell list and vertices the port keeps on
// the stack (the original: 0x70003800, 0x70002000): A2's collision scratch ranges are left out of these checks, and only of
// them. They also hide what moby_pose_matrices (called for a posed moby) writes inside them: the joint positions at
// 0x70000000 .., read back by the primitive tests, so a wrong one still shows in the hit list.
// (Task C2b: the checks added to the list reach sphere_collide or capsule_collide, whose chunk buffers,
// vertices and cell walk the port keeps on the host stack too.)
bool c2a_spr_check(const char* name) {
    static const char* const kNames[] = {"class28", "class44", "class156", "class1257", "class1427", "mobys_in_sphere",
                                         "class193", "class325", "class403", "class934", "hero_move_mode", "class1134",
                                         "class123", "class1430", "class1446", "class200", "class256", "class408",
                                         "class49", "class567", "class805", "class819", "class233_full", "class491_full",
                                         "class1257_full", "class1430_full", "class1446_full"};  // (class233_full: sphere_collide in 0x247e90; class491_full: the punch's mobys_in_sphere / segment_collide, walker_step, the floor probes)
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is level 15's own function at `laddr` (not a Veldin address to translate).
void lvcheck(const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B240_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 15) {
        std::printf("%-26s L15:0x%08x  skipped (level 15 only: run with ORACLE_LEVEL=15)\n", name, laddr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L15:0x%08x\n", name, laddr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2a_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_240() {
    if (g_only_batch != 240) return;
    g_batch = 240;
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
    // the level's mobys whose update function is `fn` (level 15's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}, uint32_t spawn_cls = 0) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 15) { lvcheck(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        // a class the level's gameplay data does not place (another class or a scene spawns it): a moby of the class spawned by the
        // game's own moby_spawn next to the hero, its first updates done by the check itself
        if (ms.empty() && spawn_cls == 0) { std::printf("%-26s L15:0x%08x  no moby on this level\n", name, fn); return; }
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
    if (g_oracle_level == 15) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l15.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    // skip_states: (848 / 1523 state 1: a spawned moby's unset links: the port refuses a pointer the interpreter wraps / the grid's missing-id trap;
    // the taxi 1410's states 3..5 on a placed taxi nobody rides: a pointer of -16 the port refuses, the interpreter wraps;
    // the states that request a level change (0x24d430's copy: the original would run the CD / movie path for real) or start a
    // music stream (its disc read: sceCdRead 0x121450 without the IOP's stream system, a TODO_PORT on the port's side; class 1419 state 1, 1446 states 5 / 7)
    static const TrCls trcls[] = {
        {"class28", 0x002955c0, class28_update, 0x0, false},
        {"class44", 0x002979d8, class44_update, 0x0, false},
        {"class49", 0x0029ac30, class49_update, 0x0, false},
        {"class67", 0x0029aff0, qua_class67_update_l15, 0x0, false},
        {"class256", 0x0029d6c0, class256_update, 0x0, false},
        {"class1427", 0x0029ead0, shockwave_update, 0x0, false},
        {"class1523", 0x0029edb0, class1523_update, 0x2, false},
        {"class77", 0x002a2488, class77_update, 0x0, false},
        {"class78", 0x002a2bf0, class78_update, 0x0, false},
        {"class92", 0x002a36d0, qua_class92_update_l15, 0x0, false},
        {"class93", 0x002a3ba8, qua_class93_update_l15, 0x0, false},
        {"class123", 0x002a6e68, class123_update, 0x0, false},
        {"class148", 0x002a7880, qua_class148_update_l15, 0x0, false},
        {"class154", 0x002aa280, qua_class154_update_l15, 0x0, false},
        {"class156", 0x002aabb8, class156_update, 0x0, false},
        {"class193", 0x002bc608, class193_update, 0x0, false},
        {"class196", 0x002bddb0, class196_update, 0x0, false},
        {"class200", 0x002be298, rock_update, 0x0, false},
        {"class221", 0x002c2938, qua_class221_update_l15, 0x0, false},
        {"class233", 0x002c5630, qua_class233_update_l15, 0x0, false},
        {"class250", 0x002c6f08, class250_update, 0x0, false},
        {"class258", 0x002c73b8, class258_update, 0x0, false},
        {"class283", 0x002c9240, empty_update, 0x0, false},
        {"class325", 0x002ca240, class325_update, 0x0, false},
        {"class403", 0x002ca9f8, class403_update, 0x0, false},
        {"class408", 0x002cb4c8, class408_update, 0x0, false},
        {"class419", 0x002cbe58, empty_update, 0x0, false},
        {"class491", 0x002cf3a8, qua_class491_update_l15, 0x0, false},
        {"class531", 0x002d3bd8, empty_update, 0x0, false},
        {"class567", 0x002d4748, class567_update, 0x0, false},
        {"class605", 0x002d5728, zone_marker_update, 0x0, false},
        {"class655", 0x002d6810, class655_update, 0x0, false},
        {"class1179", 0x002d9100, class1179_update, 0x0, false},
        {"class679", 0x002d95e8, class679_update, 0x0, false},
        {"class758", 0x002db100, swingshot_target_update, 0x0, false},
        {"class805", 0x002db668, spawn_point_update_l1, 0x0, false},
        {"class819", 0x002dd730, class819_update, 0x0, false},
        {"class830", 0x002ddd00, class830_update, 0x0, false},
        {"class832", 0x002de290, class832_update, 0x0, false},
        {"class848", 0x002de430, class848_update, 0x2, false},
        {"class934", 0x002e4380, class934_update, 0x0, false},
        {"class937", 0x002e46b0, qua_class937_update_l15, 0x0, false},
        {"class1134", 0x002e4770, gold_bolt_update, 0x4, false},
        {"class1209", 0x002e5958, floor_switch_update, 0x0, false},
        {"class1221", 0x002e5e70, class1221_update, 0x0, false},
        {"class1222", 0x002e5f58, class1221_update, 0x0, false},
        {"class1224", 0x002e6038, class1224_update, 0x0, false},
        {"class1250", 0x002e73c0, qua_class1250_update_l15, 0x0, false},
        {"class1257", 0x002e7f68, qua_class1257_update_l15, 0x0, false},
        {"class1331", 0x002e8910, class1331_update, 0x0, false},
        {"class1371", 0x002ea5e0, class1371_update, 0x0, false},
        {"class1388", 0x002ea748, qua_class1388_update_l15, 0x4, false},
        {"class1394", 0x002eabd8, qua_class1394_update_l15, 0x0, false},
        {"class1408", 0x002eb0a0, class1408_update, 0x0, false},
        {"class1409", 0x002eb458, class1409_update, 0x0, false},
        {"class1419", 0x002eb4c0, qua_class1419_update_l15, 0xa, false},
        {"class1425", 0x002eb928, qua_class1425_update_l15, 0x0, false},
        {"class1428", 0x002ebd78, class1428_update, 0x0, false},
        {"class1430", 0x002ec030, qua_class1430_update_l15, 0x0, false},
        {"class1446", 0x002ec760, qua_class1446_update_l15, 0x1a0, false},
        {"class1451", 0x002ed068, clank_pad_update, 0x0, false},
        {"class1469", 0x002ed398, qua_class1469_update_l15, 0x0, false},
        {"class1560", 0x002edb20, qua_class1560_update_l15, 0x0, false},
        {"class1565", 0x002edf58, class1565_update, 0x0, false},
        {"class1567", 0x002ee338, class1567_update, 0x0, false},
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
    if (g_oracle_level == 15 && std::getenv("B240_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B240_CLS") != nullptr && std::strcmp(std::getenv("B240_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 15 && std::getenv("B240_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B240_ST") == nullptr || std::atoi(std::getenv("B240_ST")) == st) && (std::getenv("B240_PASS") == nullptr || std::atoi(std::getenv("B240_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12, {}, static_cast<uint32_t>(std::atoi(c.name + 5)));
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 15's own moby callbacks (draw callbacks / update functions a class installs): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 15) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        static const Cb cbs[] = {
        {"cb296710", 0x00296710, class28_draw_glows, 0x00000000, 0},
        {"cb297808", 0x00297808, class28_draw_spot, 0x00000000, 0},
        {"cb29aba8", 0x0029aba8, class44_glows_draw, 0x002979d8, 44},
        {"cb2a29b8", 0x002a29b8, class77_draw, 0x002a2488, 77},
        {"cb2a3138", 0x002a3138, class78_draw, 0x002a2bf0, 78},
        {"cb2a7628", 0x002a7628, class123_draw, 0x002a6e68, 123},
        {"cb2ca810", 0x002ca810, class325_draw, 0x002ca240, 325},
        {"cb2cbd88", 0x002cbd88, class408_tint, 0x00000000, 0},
        {"cb2d77c0", 0x002d77c0, class1108_draw, 0x002d6810, 655},
        {"cb2d8710", 0x002d8710, class655_draw, 0x002d6810, 655},
        {"cb2de080", 0x002de080, class832_draw, 0x002de290, 832},
        {"cb2e6a10", 0x002e6a10, class1224_draw, 0x002e6038, 1224},
        {"cb2eac90", 0x002eac90, ripple_rings_draw, 0x002eb0a0, 1408},
        {"cb2eb108", 0x002eb108, ripple_pool_draw, 0x002eb458, 1409},
        {"cb2eb6b8", 0x002eb6b8, qua_cb_2eb6b8_l15, 0x002eb4c0, 1419},
        {"cb2edbe0", 0x002edbe0, quartu_waves_draw_a, 0x002edf58, 1565},
        {"cb2edfc0", 0x002edfc0, quartu_waves_draw_b, 0x002ee338, 1567},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B240_CLS") != nullptr && std::strcmp(std::getenv("B240_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L15:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            const size_t ign_pk = g_ignored_words.size();
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : class28_update, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 15's sound emitter types (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 15) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter3", 0x002f9230, emitter_reverb_box_update}, {"emitter5", 0x002f9390, emitter_camera_bed_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L15:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B240_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
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


    // ---- level 15's class hooks (the five-hook table of section 3): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 15) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2bc4b0", 0x2bc4b0, 0x0, qua_hook_2bc4b0_u},
            {"hook_2bc500", 0x2bc500, 0x4, qua_hook_2bc500_u},
            {"hook_2bc550", 0x2bc550, 0x8, qua_hook_2bc550_u},
            {"hook_2bc5a0", 0x2bc5a0, 0xc, qua_hook_2bc5a0_u},
            {"hook_2bc5d0", 0x2bc5d0, 0x10, qua_hook_2bc5d0_u},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L15:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B240_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            const uint32_t slot = hk.slot;
            lvcheck(hk.name, hk.addr, 40, Ret::Int32, [level, own, slot] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                uint32_t m = own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                else if (coin()) w8(m + 0x20, coin() ? 7u : u(ri(0, 9)));  // (P15.3: the hooks' own states, 7 held and 1 let go)
                // (P15.3: the carry record (the pvars' +0x60, the hook +0x10's) in its state 8, where the shared hooks let go)
                if (const uint32_t pv = ee_read<uint32_t>(m + 0x78); pv != 0 && coin()) w16(pv + 0x60 + 0x68, 8);
                if (slot == 0x10 && ri(0, 3) == 0) {  // (the hook +0x10 without a moby or without pvars)
                    if (coin()) m = 0; else w32(m + 0x78, 0);
                }
                const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
            }, [fn](A a) { return r32(fn(a.i[0], a.i[1], a.i[2], 0, 0, 0, 0, 0, a.f[0], 0, 0, 0, 0, 0, 0, 0).v0); });
        }
        g_extra_noop_hooks.clear();
    }

    // ---- level 15's movement-mode hero update (L15:0x001ff938): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 15) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same class: the segment record's word (Veldin 0x1efc70's sqc2 into 0x173e90..) is -0 or +0 by the VU0 vf register the earlier checks left; passes alone, fails after the class checks)
        lvcheck("hero_move_mode", 0x001ff938, 300, Ret::None, [level] {
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

    // ---- moby_spawn (the Veldin port) against level 15's own copy (L15:0x0023de38): the classes' spawns all go through it
    if (g_oracle_level == 15) {
        lvcheck("moby_spawn", MOBY_SPAWN_LV, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x1c, 0x2c, 0x31, 0x43, 0x100, 0x593, 0x5f3};  // (classes of level 15's own table: 28, 44, 49, 67, 256, 1427, 1523)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 15's camera hook (0x2f6980, the pointer its setter puts at the hook slot) and the callback it queues (0x2f69e0)
    if (g_oracle_level == 15) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002f6980u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        lvcheck("cam0_hook", 0x002f6980, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        lvcheck("cam_callback", 0x002f69e0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 15's copy L15:0x001ebf50) on the level's own mobys
    if (g_oracle_level == 15) {
        lvcheck("mobys_in_sphere", 0x001ebf50, 600, Ret::Int32, [level] {
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

    // ---- level 15's camera types (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 15) {
        // a live camera of the type, the hero put in or near its box, its data varied
        // a camera's +0x8c is its row in the level's type table (section 4), not the type: the rows of the level's own types
        auto cam_row = [](int type) -> int { switch (type) { case 19: return 6; case 22: return 7; default: return -1; } };
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
        static const CamT cams[] = {{19, 0x002f84d8, 0x002f7a38, 0x002f7df8, 0x002f85a0}, {22, 0x002f8ba8, 0x002f8c10, 0x002f8c18, 0x002f8c20}};
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
            if (type == 19) {
            lvcheck(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(cam_type19_accept(a.i[0], a.i[1])) : 0u); });
            lvcheck(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) cam_type19_init(a.i[0]); return uint64_t{0}; });
            lvcheck(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) cam_type19_update(a.i[0]); return uint64_t{0}; });
            lvcheck(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            }
            if (type == 22) {
            lvcheck(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [](A a) { return r32(a.i[0] != 0 ? qua_cam_type22_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
            lvcheck(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            lvcheck(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            lvcheck(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    // ---- level 15's own cutscene_level_extras (L15:0x0022db50: the cutscene's mobys of the level's scenes)
    if (g_oracle_level == 15) {
        lvcheck("cutscene_level_extras", 0x0022db50, 100, Ret::None, [level] {
            level();
            if (ri(0, 2) == 0) w32(OV(0x0015ed84), ri(0, 1) ? 15u : u(ri(0, 18)));
            return Args{};
        }, [](A) { qua_cutscene_level_extras_l15(); return uint64_t{0}; });
        // Y2: the hand port (quartu_props.cpp) through the engine's dispatch (cutscene.cpp), its branches reached: the level's
        // number 15 or another, the scene record's +0x30 about 3 .. 4, its moby count +0x44 (none, negative, up to 12) and
        // list +0x178 (the level's live mobys)
        lvcheck("cutscene_level_extras_x", 0x0022db50, 240, Ret::None, [level] {
            level();
            const uint32_t sc = OV(0x0016c860);
            std::vector<uint32_t> live;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe) live.push_back(m);
            w32(0x0015ed84, ri(0, 5) == 0 ? u(ri(0, 18)) : 15u);
            w32(sc + 0x30, ri(0, 3) == 0 ? u(ri(-1, 8)) : u(ri(3, 4)));
            const int n = ri(0, 5) == 0 ? ri(-2, 0) : ri(1, 12);
            w16(sc + 0x44, static_cast<uint32_t>(n) & 0xffffu);
            for (int i = 0; i < 12 && !live.empty(); ++i)
                w32(sc + 0x178 + u(i) * 4, live[static_cast<size_t>(ri(0, static_cast<int>(live.size()) - 1))]);
            return Args{};
        }, [](A) { cutscene_level_extras(); return uint64_t{0}; });
    }

    // T00: the level's copy of Veldin's helmet_init_fx (0x2e06a8; the scan's body hash pairs them, the overlay map does not) at
    // L15:0x002ea8d0: the hand port runs in its place; a live moby's pvars +0x10 .. +0x4c set up (the random bytes, the timers by
    // the frame scale)
    if (g_oracle_level == 15) {
        lvcheck("helmet_init_fx_2ea8d0", 0x002ea8d0, 120, Ret::None, [level] {
            level();
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) ms.push_back(m);
            const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
            if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);  // (the frame scale at its 50 Hz value: frame_scale_i's counts)
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { helmet_init_fx(a.i[0]); return uint64_t{0}; });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 15) {
        static const uint32_t empties[] = {0x002c9240, 0x002cbe58, 0x002d3bd8, 0x002f85a0, 0x002f8c10, 0x002f8c18, 0x002f8c20};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            lvcheck(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // P15.1: targeted coverage checks for Quartu's hand-ported classes 67, 92, 93, 148, 154, and 221
    // (real level mobys and boxes, 50 Hz frame scale in 1/3 of cases, Giant Clank mode, persistent bits,
    // hit-table entries, group mates, and calls into shared ports rock_spawn and plant_bolts_drop_l1).
    // Scoped to saved_ignored so particle +0x2c (written by class148/154) is strictly checked.
    g_ignored_words = saved_ignored;
    if (g_oracle_level == 15) {
        auto maybe_50hz = [] {
            if (ri(0, 2) == 0) {
                wf(0x0015ed68, 1.2f);
                wf(0x0015ed6c, 0.02f);
                wf(0x0015ed70, 0.0004f);
            }
        };
        auto put_in_box = [](uint32_t pt_addr, int32_t box_idx, bool inside) {
            if (box_idx < 0 || box_idx >= lvd::box_count(lvd::BOXES)) return;
            Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, box_idx) + 0x30);
            if (!inside) { c.x += 500.0f; c.y += 500.0f; }
            ee_ref<Vec4>(pt_addr) = c;
        };
        // the play-test spots: each placed moby of the six classes (its index in the moby table, its uid, its position)
        level();
        for (const auto& [cname, fn] : std::vector<std::pair<const char*, uint32_t>>{{"class67", 0x0029aff0}, {"class92", 0x002a36d0},
                 {"class93", 0x002a3ba8}, {"class148", 0x002a7880}, {"class154", 0x002aa280}, {"class221", 0x002c2938}}) {
            const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
            for (const uint32_t m : mobys_with(fn)) {
                const Vec4 q = ee_ref<Vec4>(m + 0x10);
                std::printf("  INFO %s: moby %u uid %d at %.1f %.1f %.1f\n", cname, (m - pool) >> 8, ee_read<int16_t>(m + 0xb2), q.x, q.y, q.z);
            }
        }

        // class 67 (L15:0x0029aff0): horizontal sliding door
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x0029aff0);
            if (!ms.empty()) {
                lvcheck("class67_full", 0x0029aff0, 240, Ret::None, [level, ms, maybe_50hz, put_in_box] {
                    level();
                    maybe_50hz();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                    const int st = ri(0, 5);
                    if (st != 0) { w8(m + 0x20, 0); qua_class67_update_l15(m); }
                    w8(m + 0x20, u(st));
                    wf(m + 0x48, rf(-3.1f, 3.1f));
                    const int32_t b_open = ee_read<int32_t>(pv + 0x10);
                    const int32_t b_close = ee_read<int32_t>(pv + 0x14);
                    put_in_box(0x0013f3d0u, b_open, coin());
                    put_in_box(OV(0x00166dc0u), b_close, coin());
                    if (st == 1) {
                        const int trig_mode = ri(0, 3);
                        if (trig_mode == 0) {
                            w32(pv + 0x1c, 0xffffffffu);
                        } else {
                            w32(pv + 0x1c, 0u);
                            w16(pool + 0xa6, trig_mode == 1 ? 0x4b9u : (trig_mode == 2 ? 0x596u : 0x100u));
                            w8(pool + 0x20, coin() ? 4u : 1u);
                        }
                        w32(pv + 0x28, coin() ? 1u : 0u);
                        const int32_t uid = ee_read<int16_t>(m + 0xb2);
                        if (ri(0, 2) == 0) w8(OV(0x001bb230u) + u(uid) + 0x454, 1);
                        else if (ri(0, 1) == 0) {
                            const uint32_t wa = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84u) << 8) + u((uid >> 5) << 2);
                            w32(wa, ee_read<uint32_t>(wa) | (1u << (uid & 0x1f)));
                        }
                    } else if (st == 2 || st == 4) {
                        const float dist = std::fabs(ee_read<float>(pv + 0x24)) + 1.0f;
                        wf(pv + 0x24, dist);
                        // the end of the travel: crossed this frame, just short of it (within a step), or far from it
                        const int edge = ri(0, 2);
                        wf(pv + 0x18, st == 2 ? (edge == 0 ? dist - 0.01f : edge == 1 ? dist - rf(0.0f, 0.2f) : rf(0.0f, dist * 0.5f))
                                              : (edge == 0 ? 0.005f : edge == 1 ? rf(0.0f, 0.2f) : rf(dist * 0.3f, dist)));
                        if (coin()) {
                            const int32_t ch = ri(0, 3);
                            w32(pv + 0x20, u(ch));
                            const uint32_t slot = 0x0013e550u + u(ch * 0x70);
                            w32(slot + 0x88, coin() ? m : 0u);
                            w8(slot + 0x74, coin() ? 1u : 0u);
                        } else {
                            w32(pv + 0x20, 0xffffffffu);
                        }
                        if (st == 2) {
                            w32(pv + 0x1c, coin() ? 0xffffffffu : 0u);
                            w32(pv + 0x28, coin() ? 1u : 0u);
                        }
                    }
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class67_update_l15(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 92 (L15:0x002a36d0): vertical sliding door
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002a36d0);
            if (!ms.empty()) {
                lvcheck("class92_full", 0x002a36d0, 240, Ret::None, [level, ms, maybe_50hz, put_in_box] {
                    level();
                    maybe_50hz();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                    const int st = ri(0, 6);
                    if (st != 0) { w8(m + 0x20, 0); qua_class92_update_l15(m); }
                    w8(m + 0x20, u(st));
                    w8(0x0013dd50u, coin() ? 1u : 0u);
                    w8(0x001413f4u, ri(0, 2) == 0 ? 2u : u(ri(0, 1)));
                    if (coin()) w32(pv + 0x20, 0xffffffffu);
                    else w32(pv + 0x20, u(ri(0, 2)));
                    const int32_t b_open = ee_read<int32_t>(pv + 0x10);
                    const int32_t b_close = ee_read<int32_t>(pv + 0x14);
                    put_in_box(0x0013f3d0u, b_open, coin());
                    put_in_box(OV(0x00166dc0u), b_close, coin());
                    if (st == 1) {
                        if (coin()) {
                            w32(pv + 0x24, 0xffffffffu);
                        } else {
                            w32(pv + 0x24, 0u);
                            w8(pool + 0x20, ri(0, 2) == 0 ? 0xfeu : (coin() ? 0xfdu : 1u));
                            w32(OV(0x00161ac0u), coin() ? 0u : 1u);
                        }
                    } else if (st == 2 || st == 4) {
                        const float h = ee_read<float>(0x001614f8u);
                        const int edge = ri(0, 2);
                        wf(pv + 0x18, st == 2 ? (edge == 0 ? h - 0.01f : edge == 1 ? h - rf(0.0f, 0.2f) : rf(0.0f, h * 0.5f))
                                              : (edge == 0 ? 0.005f : edge == 1 ? rf(0.0f, 0.2f) : rf(h * 0.3f, h)));
                        if (coin()) {
                            const int32_t ch = ri(0, 3);
                            w32(pv + 0x1c, u(ch));
                            const uint32_t slot = 0x0013e550u + u(ch * 0x70);
                            w32(slot + 0x88, coin() ? m : 0u);
                            w8(slot + 0x74, coin() ? 1u : 0u);
                        } else {
                            w32(pv + 0x1c, 0xffffffffu);
                        }
                    } else if (st == 3) {
                        w32(pv + 0x28, coin() ? 0u : 1u);
                        if (ee_read<int32_t>(pv + 0x20) >= 0) put_in_box(0x0013f3d0u, ee_read<int32_t>(pv + 0x20), coin());
                    }
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class92_update_l15(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 93 (L15:0x002a3ba8): hinged swinging door
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002a3ba8);
            if (!ms.empty()) {
                lvcheck("class93_full", 0x002a3ba8, 240, Ret::None, [level, ms, maybe_50hz, put_in_box] {
                    level();
                    maybe_50hz();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                    const int st = ri(0, 5);
                    if (st != 0) { w8(m + 0x20, 0); qua_class93_update_l15(m); }
                    w8(m + 0x20, u(st));
                    w32(pv + 0x10, coin() ? 1u : 0u);
                    const int32_t b_open = ee_read<int32_t>(pv + 0x4);
                    const int32_t b_close = ee_read<int32_t>(pv + 0x8);
                    if (coin()) {
                        w32(pv + 0x14, 0xffffffffu);
                        // the hero and Clank each in the open box, the close box or neither
                        for (const uint32_t who : {0x0013f3d0u, OV(0x00166dc0u)}) {
                            const int where = ri(0, 2);
                            put_in_box(who, where == 1 ? b_close : b_open, where != 2);
                            if (where == 1 && b_close >= 0 && b_close < lvd::box_count(lvd::BOXES) && coin()) {
                                // a point of the close box outside the open box (the open box usually lies inside the close box)
                                const Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, b_close) + 0x30);
                                for (int k = 0; k < 40; ++k) {
                                    Vec4 q = c;
                                    q.x += rf(-12.0f, 12.0f); q.y += rf(-12.0f, 12.0f);
                                    ee_ref<Vec4>(who) = q;
                                    if (point_in_box(who, b_close) != 0 && point_in_box(who, b_open) == 0) break;
                                    ee_ref<Vec4>(who) = c;
                                }
                            }
                        }
                        // the list at 0x15ffe4 (linked by +0x28): only the mobys whose first pvar word is set count
                        std::vector<uint32_t> nodes;
                        for (uint32_t n = ee_read<uint32_t>(OV(0x0015ffe4u)); n != 0 && nodes.size() < 64; n = ee_read<uint32_t>(n + 0x28))
                            if ((ee_read<uint16_t>(n + 0x34) & 0x20) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(n + 0x78)) != 0) nodes.push_back(n);
                        for (const uint32_t n : nodes) {
                            const int where = ri(0, 3);  // (most of them away from both boxes)
                            if (where < 2) put_in_box(n + 0x10, where == 1 ? b_close : b_open, true);
                            else put_in_box(n + 0x10, b_open, false);
                        }
                    } else {
                        w32(pv + 0x14, 0u);
                        w16(pool + 0xa6, coin() ? 0x49bu : 0x100u);
                        w8(pool + 0x20, coin() ? 2u : 1u);
                    }
                    if (st == 2 || st == 4) {
                        const float lim = ee_read<float>(0x001614fcu) * 0.017453292f;
                        wf(pv + 0xc, st == 2 ? (coin() ? lim + 0.01f : lim * 0.5f)
                                             : (coin() ? -0.01f : lim * 0.5f));
                    }
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class93_update_l15(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 148 (L15:0x002a7880): destructible pillar / tower (exercises rock_spawn, explosion_effect, spawn_particle_10)
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002a7880);
            if (!ms.empty()) {
                lvcheck("class148_full", 0x002a7880, 120, Ret::None, [level, ms, maybe_50hz] {
                    level();
                    maybe_50hz();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    w8(0x001413f4u, ri(0, 3) == 0 ? 0u : 2u);
                    w8(m + 0x20, coin() ? 0u : 1u);
                    w16(m + 0xa6, coin() ? 0x94u : 0x95u);
                    w8(0x0013dd50u, coin() ? 1u : 0u);
                    if (coin()) {
                        const int32_t uid = ee_read<int16_t>(m + 0xb2);
                        const uint32_t wa = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84u) << 8) + u((uid >> 5) << 2);
                        w32(wa, ee_read<uint32_t>(wa) | (1u << (uid & 0x1f)));
                    }
                    if (ri(0, 2) != 0) {
                        const uint32_t slot = u(ri(0, 5));
                        const uint32_t e = OV(0x00178100u) + (slot << 6);
                        w32(e + 0x34, m);
                        w32(e + 0x24, ri(0, 3) == 0 ? 0x40000u : 0x80000u);  // (an entry of the moby without the giant's flag: not a hit)
                        if (coin()) {
                            wf(e + 0x0, 0.0f); wf(e + 0x4, 0.0f); wf(e + 0x8, 0.0f);
                        } else {
                            Vec4 q = ee_ref<Vec4>(m + 0x10);
                            q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(1, 5);
                            ee_ref<Vec4>(e) = q;
                        }
                        wf(e + 0x10, rf(-1, 1)); wf(e + 0x14, rf(-1, 1)); wf(e + 0x18, rf(0, 1));
                    }
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class148_update_l15(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 154 (L15:0x002aa280): destructible wall / barrier (exercises rock_spawn, explosion_effect, spawn_particle_10)
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002aa280);
            if (!ms.empty()) {
                lvcheck("class154_full", 0x002aa280, 120, Ret::None, [level, ms, maybe_50hz] {
                    level();
                    maybe_50hz();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    w8(0x001413f4u, ri(0, 3) == 0 ? 0u : 2u);
                    w8(m + 0x20, coin() ? 0u : 1u);
                    wf(m + 0x48, rf(-1.5f, 1.5f));
                    w8(0x0013dd50u, coin() ? 1u : 0u);
                    if (coin()) {
                        const int32_t uid = ee_read<int16_t>(m + 0xb2);
                        const uint32_t wa = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84u) << 8) + u((uid >> 5) << 2);
                        w32(wa, ee_read<uint32_t>(wa) | (1u << (uid & 0x1f)));
                    }
                    if (ri(0, 2) != 0) {
                        const uint32_t slot = u(ri(0, 5));
                        const uint32_t e = OV(0x00178100u) + (slot << 6);
                        w32(e + 0x34, m);
                        w32(e + 0x24, ri(0, 3) == 0 ? 0x40000u : 0x80000u);  // (an entry of the moby without the giant's flag: not a hit)
                        if (coin()) {
                            wf(e + 0x0, 0.0f); wf(e + 0x4, 0.0f); wf(e + 0x8, 0.0f);
                        } else {
                            Vec4 q = ee_ref<Vec4>(m + 0x10);
                            q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(1, 5);
                            ee_ref<Vec4>(e) = q;
                        }
                        wf(e + 0x10, rf(-1, 1)); wf(e + 0x14, rf(-1, 1)); wf(e + 0x18, rf(0, 1));
                    }
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class154_update_l15(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 221 (L15:0x002c2938): spring-jointed factory emitter / plant (exercises lvd::group and plant_bolts_drop_l1)
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002c2938);
            if (!ms.empty()) {
                // state 6's three debris mobys: the shared debris_spawn port leaves their w lanes +0x0c, +0x1c, +0x8c, +0x9c at 0 where
                // the original leaves a VU0 sin / cos lane (a known difference of debris_spawn, COMMON.md; measured without this
                // ignore: exactly those four words of each spawned moby differ, nothing else). Only this check.
                const size_t ign_before = g_ignored_words.size();
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
                lvcheck("class221_full", 0x002c2938, 240, Ret::None, [level, ms, maybe_50hz, give_hit] {
                    level();
                    maybe_50hz();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const int st = ri(0, 6);
                    if (st != 0) { w8(m + 0x20, 0); qua_class221_update_l15(m); }
                    w8(m + 0x20, u(st));
                    if (ri(0, 5) == 0) give_hit(m, 0x10000u);
                    if (coin()) {  // the joint node (pv +0x40.. angles, +0x50.. their speeds) in motion, the time step 0x15ed64 off 1
                        for (uint32_t k = 0; k < 3; ++k) { wf(pv + 0x40 + k * 4, rf(-0.2f, 0.2f)); wf(pv + 0x50 + k * 4, rf(-0.05f, 0.05f)); }
                    }
                    if (ri(0, 3) == 0) wf(0x0015ed64, rf(0.5f, 3.0f));
                    // Put camera near the moby so distance < 42 / < 20 checks pass in most cases, and hero within 7 so plant_bolts_drop_l1 drops bolts
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    if (coin()) {
                        Vec4 h = q; h.x += rf(-3.0f, 3.0f); h.y += rf(-3.0f, 3.0f);
                        ee_ref<Vec4>(0x0013f3d0u) = h;
                    }
                    const float dist = ri(0, 3) == 0 ? 60.0f : rf(1.0f, 15.0f);
                    q.x += dist;
                    ee_ref<Vec4>(OV(0x00166dc0u)) = q;
                    if (st == 1) {
                        w16(pv + 0x80, coin() ? 1u : 5u);
                        if (ri(0, 3) == 0) w8(m + 0x21, 0xff);
                        else {
                            // the group's mates: all idle (state 1), all idle but one busy, or as the level has them
                            const uint8_t grp = ee_read<uint8_t>(m + 0x21);
                            if (grp != 0xff) {
                                const uint32_t list = lvd::group(static_cast<int32_t>(grp));
                                if (list != 0) {
                                    std::vector<uint32_t> mates;
                                    for (uint32_t p = list;; p += 2) {
                                        const uint16_t entry = ee_read<uint16_t>(p);
                                        const uint32_t mate = ee_read<uint32_t>(OV(0x0015ffd8u)) + (u(entry & 0x7fffu) << 8);
                                        if (mate != m) mates.push_back(mate);
                                        if ((entry & 0x8000u) != 0 || mates.size() > 64) break;
                                    }
                                    const int mode = ri(0, 2);
                                    if (mode != 2) for (const uint32_t x : mates) w8(x + 0x20, 1);
                                    if (mode == 1 && !mates.empty()) {
                                        static const uint8_t busy[] = {0, 2, 3, 4, 5};
                                        w8(mates[static_cast<size_t>(ri(0, static_cast<int>(mates.size()) - 1))] + 0x20, busy[ri(0, 4)]);
                                    }
                                }
                            }
                        }
                    } else if (st == 2) {
                        wf(pv + 0x84, rf(-0.2f, 0.2f));
                        wf(pv + 0x8c, coin() ? ee_read<float>(pv + 0x84) : rf(-0.2f, 0.2f));
                    } else if (st == 3) {
                        const int16_t var = static_cast<int16_t>(ri(0, 1));
                        w16(pv + 0x82, u(var));
                        w8(m + 0x53, u((var << 1) + 1));
                        w8(m + 0x70, coin() ? 2u : 0u);
                    } else if (st == 4) {
                        w8(m + 0x31, coin() ? 1u : 0u);
                        w16(pv + 0x80, coin() ? 1u : 5u);
                        wf(pv + 0x90, rf(-3.1f, 3.1f));
                    } else if (st == 5) {
                        const int16_t var = static_cast<int16_t>(ri(0, 1));
                        w16(pv + 0x82, u(var));
                        w8(m + 0x53, u((var << 1) + 3));
                        w8(m + 0x70, coin() ? 2u : 0u);
                    }
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class221_update_l15(a.i[0]); return uint64_t{0}; });
                g_ignored_words.resize(ign_before);
            }
        }

        // P15.2: class 233 (L15:0x002c5630), the flying craft, with its helpers 0x247e90 (the move against the walls),
        // 0x2c4f88 (the flight toward a goal), 0x2c52b8 (its place near the target) and 0x2c6900 (its hits): the level's
        // own crafts, set up by their first update, then each state with the hero Giant Clank or not, its flag +0x1a4,
        // its group / box / zone / route kept or none, the hero near or out of range (and in its box), its mates of the
        // class moved near it in the states that count, a charge of class 848 in flight (a moby of the level made one:
        // its class and state), its timers at their edges, hits (kinds 0x330000, the applied mark), its hit record dead,
        // the floor band near Clank, 50 Hz in a third, the time step 0x15ed64 off 1 in a quarter
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002c5630);
            if (!ms.empty()) {
                static const int states[] = {0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 7, 64, 10};
                // state 7's bolts (moby_drop_bolts): a dropped bolt's direction (its pvars +0x20) has its w lane (+0x2c) from an
                // unset stack slot of bolts_drop in the original (the known difference, batch 65 / 112; measured: those words
                // alone differ), here for this check alone
                const size_t ign_before = g_ignored_words.size();
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) g_ignored_words.push_back(a + 0x2c);
                // and the collision record's w lanes (OV(0x173e40) .. 0x173ec0: the known difference when the sphere meets the
                // hero's moby): the craft copies the pushed point (+0x30) whole, so its position's w lane (+0x1c), its velocity's
                // (pvars +0x15c) and its piece's position's (+0x1c) follow it; those words of the case's craft are added by the
                // generator
                for (uint32_t a = OV(0x00173e40); a < OV(0x00173ec0); a += 0x10) g_ignored_words.push_back(a + 0xc);
                for (const uint32_t x : ms) {  // the play-test spots, and each craft's group, zone, route, box and flag
                    const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                    const Vec4 q = ee_ref<Vec4>(x + 0x10);
                    std::printf("  INFO class233: moby %u uid %d at %.1f %.1f %.1f group %d zone %d route %d box %d giant-only %d (paths %d)\n",
                                (x - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8, ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<int32_t>(xp + 0x190),
                                ee_read<int32_t>(xp + 0x194), ee_read<int32_t>(xp + 0x1ac), ee_read<int32_t>(xp + 0x1b0), ee_read<int32_t>(xp + 0x1a4),
                                lvd::path_count());
                }
                lvcheck("class233_full", 0x002c5630, 480, Ret::None, [level, ms, maybe_50hz, put_in_box, give_hit] {
                    level();
                    const int st = states[ri(0, 20)];
                    // (state 2 flies the route: only the crafts that have one get there)
                    std::vector<uint32_t> cand;
                    const bool lone = st == 1 && coin();  // (state 1: the crafts without a group, that wait for a target / their box)
                    for (const uint32_t x : ms) {
                        const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                        if (st == 2 && ee_read<int32_t>(xp + 0x1ac) == -1) continue;
                        if (lone && ee_read<int32_t>(xp + 0x190) != -1) continue;
                        cand.push_back(x);
                    }
                    const uint32_t m = cand[static_cast<size_t>(ri(0, static_cast<int>(cand.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                    w8(0x001413f4u, 2);  // (the first update as the level starts it: the hero Giant Clank)
                    w8(m + 0x20, 0);
                    qua_class233_update_l15(m);
                    w8(m + 0x20, u(st));
                    maybe_50hz();
                    if (ri(0, 3) == 0) wf(0x0015ed64, rf(0.5f, 3.0f));
                    w8(0x001413f4u, ri(0, 2) == 0 ? 0u : 2u);
                    w32(pv + 0x1a4, coin() ? 1u : 0u);
                    if (ri(0, 3) == 0) w32(pv + 0x190, 0xffffffffu);
                    if (ri(0, 3) == 0) w32(pv + 0x1b0, 0xffffffffu);
                    // (no route only before the route (state 2 needs one); no zone only where the craft is not in it yet: states
                    // 2 .. 5 walk the zone's polygon, a garbage path the interpreter wraps and the port refuses)
                    if (st != 2 && ri(0, 5) == 0) w32(pv + 0x1ac, 0xffffffffu);
                    if ((st <= 1 || st >= 6) && ri(0, 3) == 0) w32(pv + 0x194, 0xffffffffu);
                    if (ri(0, 7) == 0) w8(pv + 0x2e, 0);
                    w8(m + 0x31, coin() ? 1u : 0u);
                    // on its route (state 2): at one of its points, mostly near the route's end (the braking, the arrival)
                    if (st == 2 && coin()) {
                        const uint32_t route = lvd::path(ee_read<int32_t>(pv + 0x1ac));
                        const int32_t n = ee_read<int32_t>(route);
                        if (n > 0) {
                            const int mode = ri(0, 2) == 0 ? 3 : ri(0, 2);  // (3: on the route's last point, slower than a frame's braking)
                            const int32_t k = mode == 3 ? n - 1 : mode >= 1 ? ri(std::max(0, n - 10), n - 1) : ri(0, n - 1);
                            Vec4 p = ee_ref<Vec4>(route + 0x10 + u(k) * 0x10);
                            if (mode != 3) { p.x += rf(-1.5f, 1.5f); p.y += rf(-1.5f, 1.5f); }
                            p.w = ee_ref<Vec4>(m + 0x10).w;
                            ee_ref<Vec4>(m + 0x10) = p;
                            wf(pv + 0x170, mode == 3 ? rf(0.0f, 0.002f) : rf(0.0f, 0.4f));
                        }
                    }
                    // the hero: in its box, inside its zone (the craft near him or not), near it, or out of its ranges; Clank near it or not
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    // (states 2 .. 5 want the target inside the zone and in range: mostly so)
                    int where = ri(0, 4);
                    if (st >= 2 && st <= 5) { const int k = ri(0, 3); where = k == 0 ? 4 : k <= 2 ? (st == 2 ? 1 : 2) : where; }
                    if (st == 1 && coin()) where = 3;
                    if (st == 1 && ee_read<int32_t>(pv + 0x1b0) >= 0 && ri(0, 3) != 0) where = 0;
                    const int32_t zi = ee_read<int32_t>(pv + 0x194);
                    if (where == 0 && ee_read<int32_t>(pv + 0x1b0) >= 0) {
                        put_in_box(0x0013f3d0u, ee_read<int32_t>(pv + 0x1b0), true);
                    } else if (where <= 2 && zi >= 0 && zi < lvd::path_count()) {
                        const uint32_t zone = lvd::path(zi);
                        const int32_t n = ee_read<int32_t>(zone);
                        Vec4 c{};
                        for (int32_t k = 0; k < n; ++k) { const Vec4 p = ee_ref<Vec4>(zone + 0x10 + u(k) * 0x10); c.x += p.x / n; c.y += p.y / n; c.z += p.z / n; }
                        c.x += rf(-3.0f, 3.0f); c.y += rf(-3.0f, 3.0f); c.w = ee_ref<Vec4>(0x0013f3d0u).w;
                        ee_ref<Vec4>(0x0013f3d0u) = c;
                        if (where == 2 && st != 2) {  // the craft near him, inside the zone or at its edge (or on the ring 10 out)
                            const float r = coin() ? rf(9.6f, 10.4f) : rf(3.0f, 25.0f), a = rf(-3.1f, 3.1f);
                            Vec4 p = q; p.x = c.x + r * std::cos(a); p.y = c.y + r * std::sin(a);
                            ee_ref<Vec4>(m + 0x10) = p;
                            q = p;
                        }
                    } else {
                        const float r = where <= 3 ? rf(2.0f, 25.0f) : rf(70.0f, 120.0f), a = rf(-3.1f, 3.1f);
                        Vec4 h = q; h.x += r * std::cos(a); h.y += r * std::sin(a); h.z -= rf(0.0f, 10.0f);
                        ee_ref<Vec4>(0x0013f3d0u) = h;
                        if (st == 3 && where == 4 && zi >= 0 && zi < lvd::path_count() && coin()) {  // (the craft inside its zone, no target)
                            const uint32_t zone = lvd::path(zi);
                            const int32_t n = ee_read<int32_t>(zone);
                            Vec4 c = q; c.x = 0; c.y = 0;
                            for (int32_t k = 0; k < n; ++k) { const Vec4 p = ee_ref<Vec4>(zone + 0x10 + u(k) * 0x10); c.x += p.x / n; c.y += p.y / n; }
                            ee_ref<Vec4>(m + 0x10) = c;
                            q = c;
                        }
                    }
                    { Vec4 c = q; c.x += coin() ? rf(-20.0f, 20.0f) : 80.0f; ee_ref<Vec4>(OV(0x00166dc0u)) = c; }
                    {  // the hero's moby with him (target_select aims at the moby's position)
                        const uint32_t hm = ee_read<uint32_t>(0x001413d0u);
                        if (hm != 0) { Vec4 hp = ee_ref<Vec4>(0x0013f3d0u); hp.w = ee_ref<Vec4>(hm + 0x10).w; ee_ref<Vec4>(hm + 0x10) = hp; }
                    }
                    wf(pv + 0x170, coin() ? 0.0f : rf(0.0f, 0.4f));
                    for (const uint32_t o : {0x150u, 0x154u}) wf(pv + o, rf(-0.3f, 0.3f));
                    for (const uint32_t o : {0x174u, 0x178u, 0x17cu, 0x184u, 0x18cu}) wf(pv + o, rf(-0.05f, 0.05f));
                    wf(pv + 0x180, rf(-3.0f, 3.0f)); wf(pv + 0x188, rf(-0.5f, 0.5f));
                    { const float t = coin() ? 0.05f : 0.3f; wf(m + 0x40, rf(-t, t)); wf(m + 0x44, rf(-t, t)); } wf(m + 0x48, rf(-3.1f, 3.1f));
                    { static const uint16_t tv[] = {0, 1, 5}; w16(pv + 0x168, tv[ri(0, 2)]); w16(pv + 0x16a, tv[ri(0, 2)]); }
                    if (ri(0, 3) == 0 || (st == 1 && coin())) wf(pv + 0x198, rf(40.0f, 200.0f));  // (the waiting range: the level's, or wider)
                    wf(pv + 0x16c, rf(-3.1f, 3.1f));
                    if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-2.0f, 2.0f));
                    // a charge in flight: none, or a moby of the level made one (class 848), alive or going
                    if (ri(0, 2) == 0) {
                        w32(pv + 0x164, 0);
                    } else {
                        const uint32_t c = pool + (u(ri(0, 60)) << 8);
                        if (c != m) {
                            w16(c + 0xa6, coin() ? 0x350u : 0x351u);
                            static const uint8_t cst[] = {0, 1, 0xfe, 0xfd};
                            w8(c + 0x20, cst[ri(0, 3)]);
                            w32(pv + 0x164, c);
                        }
                    }
                    if (ri(0, 4) == 0) w32(pv + 0x160, 0);
                    // its mates of the class: all gone (it is the nearest to the target), or some moved near it, in a state that counts or not
                    const bool alone = ri(0, 2) == 0;
                    for (const uint32_t x : ms) {
                        if (x == m) continue;
                        if (alone) { w8(x + 0x20, coin() ? 0x40u : 1u); continue; }
                        if (!coin()) continue;
                        Vec4 p = q; p.x += rf(-12.0f, 12.0f); p.y += rf(-12.0f, 12.0f);
                        ee_ref<Vec4>(x + 0x10) = p;
                        static const uint8_t mst[] = {1, 0x40, 2, 5};
                        w8(x + 0x20, mst[ri(0, 3)]);
                    }
                    if (ri(0, 2) == 0) {
                        give_hit(m, 0x330000u);
                        if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                            if (coin()) w32(e + 0x1c, 0x45afdf66u);  // (applied: the push that throws it off)
                        }
                    }
                    if (coin()) wf(pv + 0x20, coin() ? 0.5f : 3.0f);
                    if (st == 64 && coin()) {  // (the moby's saved bits set, for state 0x40 to clear)
                        const int32_t uid = ee_read<int16_t>(m + 0xb2);
                        w8(OV(0x001ba5d0u) + u(uid) + 0x454, 1);
                        w8(OV(0x001bb230u) + u(uid) + 0x454, 1);
                        const uint32_t bit = 1u << (uid & 0x1f), word = u((uid >> 5) << 2);
                        const uint32_t sa = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84u) << 8) + word;
                        w32(sa, ee_read<uint32_t>(sa) | bit);
                        w32(OV(0x001ba4d0u) + word, ee_read<uint32_t>(OV(0x001ba4d0u) + word) | bit);
                    }
                    w32(OV(0x0015f5cc), R() & 0xffff);
                    g_ignored_words.push_back(m + 0x1c);
                    g_ignored_words.push_back(pv + 0x15c);
                    if (ee_read<uint32_t>(pv + 0x160) != 0) g_ignored_words.push_back(ee_read<uint32_t>(pv + 0x160) + 0x1c);
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class233_update_l15(a.i[0]); return uint64_t{0}; });
                g_ignored_words.resize(ign_before);
            }
        }

        // P15.3: class 491 (L15:0x002cf3a8), the melee robot, with its helpers 0x2cf110 (its place near the target) and
        // 0x2d0fa8 (its hits and its target): the level's own robots, set up by their first update (state 0, the hero Giant
        // Clank), then each state with the hero Giant Clank or not; the hero (his moby with him) at the punch's range, the
        // place's, 8 / 10 away, further or out of range, in its box / zone polygon; its group / box / zone / entry path / jump
        // paths kept or none, its "no walking" / "no jump paths" flags, the jump path in use and its point at the ends; the
        // animation each state looks for (the set, its frame, ended or not: the throw's release, the punch's frames 17 .. 24);
        // the jump's speeds, above the floor or under it; its timer and throw count at their edges; mates of the class near
        // it and near the target; hits from the Giant Clank hit table (the kinds, the hero's, a class 848 charge's, a pause
        // with +0x2a 0x100, the push's mark, a deadly damage); the jets' sound slot its own and playing or not; its saved bits
        // for state 0x40; 50 Hz in a third
        {
            level();
            const std::vector<uint32_t> ms = mobys_with(0x002cf3a8);
            if (!ms.empty()) {
                const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                std::printf("  INFO class491: ranges 0x161b24 %.2f 0x161b28 %.2f, jump speed %.2f gravity %.2f, throw speed %.2f life %d\n",
                            ee_read<float>(0x00161b24u), ee_read<float>(0x00161b28u), ee_read<float>(0x00161b14u),
                            ee_read<float>(0x00161b18u), ee_read<float>(0x00161b20u), ee_read<int32_t>(0x00161b1cu));
                {  // the moby list 0x15ffe4 (+0x28 links: the mobys 0x2cf110 looks among for its mates) and the punch's animation
                    int n = 0, n491 = 0;
                    for (uint32_t x = ee_read<uint32_t>(0x0015ffe4u); x != 0 && n < 4000; x = ee_read<uint32_t>(x + 0x28), ++n)
                        if (ee_read<uint32_t>(x + 0x74) == 0x002cf3a8u) ++n491;
                    const uint32_t m0 = ms[0];
                    std::printf("  INFO class491: list 0x15ffe4 %d mobys, %d of the class;", n, n491);
                    moby_anim_set(m0, 2, 0, 1);
                    for (int f = 0; f <= 60; ++f) {
                        if (f % 5 == 0) std::printf(" anim 2 after %d steps: time %.2f end %d", f, moby_anim_time(m0), (ee_read<uint8_t>(m0 + 0x70) & 2) != 0);
                        moby_anim_advance(m0);
                    }
                    std::printf("\n");
                    level();
                }
                for (const uint32_t x : ms) {  // the play-test spots, and each robot's group, zone, paths, box, flags and ranges
                    const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                    const Vec4 q = ee_ref<Vec4>(x + 0x10);
                    std::printf("  INFO class491: moby %u uid %d at %.1f %.1f %.1f group %d zone %d entry %d jumps %d %d box %d no-walk %d range %.1f\n",
                                (x - pool) >> 8, ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<int32_t>(xp + 0x1cc), ee_read<int32_t>(xp + 0x1d0),
                                ee_read<int32_t>(xp + 0x170), ee_read<int32_t>(xp + 0x180), ee_read<int32_t>(xp + 0x184), ee_read<int32_t>(xp + 0x1dc),
                                ee_read<int32_t>(xp + 0x1d8), ee_read<float>(xp + 0x178));
                }
                static const int states[] = {0, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 64, 64, 30};
                // the mobys it spawns (state 10's three debris, the class 819 charges it throws, the blast's and bolts' of its
                // death): the shared spawn ports leave their w lanes +0x0c, +0x1c, +0x8c, +0x9c at 0 where the original leaves a VU0
                // sin / cos lane (the known difference of debris_spawn, COMMON.md; measured without this ignore: those lanes of the
                // spawned mobys alone differ, in 53 cases of 480, nothing else). Only this check.
                const size_t ign_before = g_ignored_words.size();
                for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) {
                    g_ignored_words.push_back(a + 0xc);
                    g_ignored_words.push_back(a + 0x1c);
                }
                lvcheck("class491_full", 0x002cf3a8, 480, Ret::None, [level, ms, maybe_50hz, put_in_box] {
                    level();
                    const int st = states[ri(0, static_cast<int>(sizeof(states) / sizeof(states[0])) - 1)];
                    std::vector<uint32_t> cand;
                    const bool jumpers = (st == 5 || st == 6 || st == 8 || st == 9) && ri(0, 3) != 0;  // (those with jump paths)
                    for (const uint32_t x : ms) {
                        const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                        if (st == 2 && ee_read<int32_t>(xp + 0x170) == -1) continue;  // (state 2 walks its entry path)
                        if (jumpers && (ee_read<int32_t>(xp + 0x180) < 0 || ee_read<int32_t>(xp + 0x184) < 0)) continue;
                        cand.push_back(x);
                    }
                    if (st == 1 && coin()) {  // (the robots with a start box: the hero in it)
                        std::vector<uint32_t> boxed;
                        for (const uint32_t x : cand) if (ee_read<int32_t>(ee_read<uint32_t>(x + 0x78) + 0x1dc) >= 0) boxed.push_back(x);
                        if (!boxed.empty()) cand = boxed;
                    }
                    if (cand.empty()) cand = ms;
                    const uint32_t m = cand[static_cast<size_t>(ri(0, static_cast<int>(cand.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                    const uint32_t hm = ee_read<uint32_t>(0x001413d0u);
                    w8(0x001413f4u, 2);  // (the first update as the level starts it: the hero Giant Clank)
                    w8(m + 0x20, 0);
                    qua_class491_update_l15(m);
                    w8(m + 0x20, u(st));
                    maybe_50hz();
                    w8(0x001413f4u, ri(0, 4) == 0 ? 0u : 2u);
                    // its numbers: kept as the level gives them, or none
                    if (ri(0, 4) == 0) w32(pv + 0x1cc, 0xffffffffu);
                    if (ri(0, 4) == 0) w32(pv + 0x1dc, 0xffffffffu);
                    if (ri(0, 4) == 0) w32(pv + 0x1d0, 0xffffffffu);
                    if (st != 2 && ri(0, 5) == 0) w32(pv + 0x170, 0xffffffffu);
                    w32(pv + 0x1d8, ri(0, 4) == 0 ? 1u : 0u);
                    if (ri(0, 4) == 0) w32(pv + 0x1d4, coin() ? 1u : 0u);
                    const bool jumps = ee_read<int32_t>(pv + 0x180) >= 0 && ee_read<int32_t>(pv + 0x184) >= 0;
                    if (!jumps) w32(pv + 0x1d4, 1);  // (state 0 sets it so: the jump paths are read only without it)
                    w32(pv + 0x1b0, coin() ? 1u : 0u);
                    if (jumps) {
                        const int32_t n = ee_read<int32_t>(lvd::path(ee_read<int32_t>(pv + 0x180 + (ee_read<uint32_t>(pv + 0x1b0) << 2))));
                        const int k = ri(0, 3);
                        w32(pv + 0x1b4, u(k == 0 ? n - 1 : k == 1 ? std::max(0, n - 2) : ri(0, std::max(0, n - 1))));
                    }
                    { static const int32_t tv[] = {0, 1, 1, 2, 5, 30}; w32(pv + 0x1c0, u(tv[ri(0, 5)])); }
                    { static const int32_t cv[] = {0, 1, 4, 5, 5, 6, 6, 7}; w32(pv + 0x1c4, u(cv[ri(0, 7)])); }  // (the throws made: six ends them)
                    if (coin()) w8(pv + 0x58, 0x19);
                    wf(pv + 0x188, rf(0.2f, 3.0f));
                    wf(pv + 0x1bc, rf(-0.2f, 0.2f));
                    wf(m + 0x48, rf(-3.1f, 3.1f));
                    if (coin()) wf(pv + 0x178, rf(15.0f, 120.0f));
                    if (st == 1 && coin()) wf(pv + 0x174, rf(30.0f, 200.0f));
                    wf(pv + 0xe8, coin() ? 0.0f : rf(0.0f, 1.5f));
                    if (coin()) { Vec4 f = ee_ref<Vec4>(m + 0x10); f.x += rf(-1, 1); f.y += rf(-1, 1); f.z += rf(-1, 1); ee_ref<Vec4>(pv + 0x1a0) = f; }
                    // on its entry path (state 2): at one of its points, often near the end
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    if (st == 2) {
                        const uint32_t route = lvd::path(ee_read<int32_t>(pv + 0x170));
                        const int32_t n = ee_read<int32_t>(route);
                        if (n > 0) {
                            const int mode = ri(0, 3);  // (2: on its last point, as it arrives; 3: past it)
                            const int32_t k = mode >= 2 ? n - 1 : mode == 1 ? ri(std::max(0, n - 4), n - 1) : ri(0, n - 1);
                            Vec4 p = ee_ref<Vec4>(route + 0x10 + u(k) * 0x10);
                            if (mode <= 1) { p.x += rf(-1.0f, 1.0f); p.y += rf(-1.0f, 1.0f); p.z += rf(-0.5f, 1.0f); }
                            if (mode == 3 && n >= 2) {
                                const Vec4 b = ee_ref<Vec4>(route + 0x10 + u(n - 2) * 0x10);
                                const float t = rf(0.0f, 3.0f);
                                p.x += (p.x - b.x) * t; p.y += (p.y - b.y) * t;
                            }
                            p.w = q.w;
                            ee_ref<Vec4>(m + 0x10) = p;
                            q = p;
                        }
                    }
                    // a jump (state 3): its speeds; high above the floor, just above it or under it
                    if (st == 3) {
                        wf(pv + 0x40, rf(-0.4f, 0.4f)); wf(pv + 0x44, rf(-0.4f, 0.4f));
                        { const int v = ri(0, 2); wf(pv + 0x48, v == 0 ? rf(-0.6f, 0.0f) : v == 1 ? rf(-0.03f, 0.0f) : rf(-0.1f, 0.4f)); }  // (1: about to land)
                        const int k = ri(0, 2);
                        q.z += k == 0 ? rf(3.0f, 12.0f) : k == 1 ? rf(0.0f, 1.0f) : rf(-0.6f, 0.0f);
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                    // the animation the state looks for: its set, a frame (the release at 0, the punch's 17 .. 24), ended or not
                    {
                        int set = ri(0, 7), frame = ri(0, 30);
                        switch (st) {
                            case 2: set = coin() ? 5 : 4; break;
                            case 3: set = coin() ? 3 : 4; break;
                            case 4: set = coin() ? 0 : ri(1, 7); break;
                            case 5: set = ri(0, 2) == 0 ? 7 : 6; frame = coin() ? 0 : ri(1, 4); break;  // (7 at frame 0: the release)
                            case 8: set = ri(0, 3) != 0 ? 2 : ri(0, 7); frame = coin() ? ri(16, 25) : ri(0, 30); break;
                            default: break;
                        }
                        bool punching = false;
                        if (ri(0, 4) != 0) {
                            if (st == 8 && set == 2 && ri(0, 3) != 0) {  // (the punch's frames: the animation run from its start)
                                moby_anim_set(m, 2, 0, 1);
                                static const int edge[] = {33, 34, 35, 36, 47, 48, 49};  // (the window's edges: times 16.5 .. 18, 23.5 .. 24.5)
                                for (int k = coin() ? edge[ri(0, 6)] : ri(30, 52); k > 0; --k) moby_anim_advance(m);  // (half a frame a step: times 15 .. 26)
                                punching = ri(0, 3) != 0;
                            } else {
                                moby_anim_set(m, u(set), frame, ri(1, 8));
                                if (coin()) wf(m + 0x54, 0.0f); else wf(m + 0x54, rf(0.0f, 1.0f));
                            }
                        }
                        w8(m + 0x70, coin() && !punching ? (ee_read<uint8_t>(m + 0x70) | 2u) : (ee_read<uint8_t>(m + 0x70) & 0xfdu));
                    }
                    // the hero (his moby with him): inside its zone polygon mostly (its target is taken only there), in its box, or
                    // near the robot; the robot (off its entry path) at the punch's range from him, the place's, 8 / 10 away,
                    // further, out of range
                    {
                        const float r24 = ee_read<float>(0x00161b24u), r28 = ee_read<float>(0x00161b28u);
                        const int32_t zi = ee_read<int32_t>(pv + 0x1d0);
                        const int where = ri(0, 5);
                        Vec4 h = q;
                        if (where <= 3 && zi >= 0 && zi < lvd::path_count()) {
                            const uint32_t zone = lvd::path(zi);
                            const int32_t n = ee_read<int32_t>(zone);
                            Vec4 c{};
                            for (int32_t j = 0; j < n; ++j) { const Vec4 p = ee_ref<Vec4>(zone + 0x10 + u(j) * 0x10); c.x += p.x / n; c.y += p.y / n; }
                            h.x = c.x + rf(-4.0f, 4.0f); h.y = c.y + rf(-4.0f, 4.0f); h.z = q.z + rf(-1.0f, 2.0f);
                        } else if ((where == 4 || (st == 1 && coin())) && ee_read<int32_t>(pv + 0x1dc) >= 0) {
                            put_in_box(0x0013f3d0u, ee_read<int32_t>(pv + 0x1dc), true);
                            h = ee_ref<Vec4>(0x0013f3d0u);
                        }
                        float r;
                        switch (st == 6 && coin() ? 7 : ri(0, 6)) {
                            case 7: r = rf(7.5f, 9.5f); break;  // (state 6: the hop's 8)
                            case 0: r = rf(0.3f, r28); break;
                            case 1: r = rf(r28, r24); break;
                            case 2: r = coin() ? rf(7.5f, 8.5f) : rf(9.5f, 10.5f); break;
                            case 3: r = coin() ? rf(11.5f, 12.5f) : rf(19.5f, 20.5f); break;
                            case 4: r = rf(r24, 50.0f); break;
                            case 5: r = rf(50.0f, 100.0f); break;
                            default: r = rf(100.0f, 200.0f); break;
                        }
                        const float a = rf(-3.1f, 3.1f);
                        if (st == 2 || where == 5) {  // (the robot stays: the hero r away from it)
                            h = q; h.x += r * std::cos(a); h.y += r * std::sin(a); h.z += rf(-1.0f, 2.0f);
                        } else if (ri(0, 3) != 0) {
                            q.x = h.x + r * std::cos(a); q.y = h.y + r * std::sin(a);
                            ee_ref<Vec4>(m + 0x10) = q;
                        }
                        h.w = ee_ref<Vec4>(0x0013f3d0u).w;
                        ee_ref<Vec4>(0x0013f3d0u) = h;
                        if (hm != 0) { Vec4 hp = h; hp.w = ee_ref<Vec4>(hm + 0x10).w; ee_ref<Vec4>(hm + 0x10) = hp; }
                    }
                    // waiting (state 1): shown already, or its group's mates gone (all of them in state 0x40)
                    if (st == 1) {
                        if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 0x1000u);
                        const int32_t g = ee_read<int32_t>(pv + 0x1cc);
                        if (g >= 0 && g < lvd::group_count() && lvd::group(g) != 0 && coin()) {
                            for (uint32_t e = lvd::group(g);; e += 2) {
                                const uint16_t id = ee_read<uint16_t>(e);
                                const uint32_t x = pool + (u(id & 0x7fffu) << 8);
                                if (x != m && ee_read<int8_t>(x + 0x20) >= 0) w8(x + 0x20, 0x40);
                                if ((id & 0x8000u) != 0) break;
                            }
                        }
                    }
                    // its mates of the class: as placed, or moved near it (within 3) / near the target, linked into the moby list
                    // 0x15ffe4 when they are not in it (0x2cf110 looks for them there); one in a state of its own
                    {
                        auto listed = [](uint32_t x) {
                            int n = 0;
                            for (uint32_t y = ee_read<uint32_t>(0x0015ffe4u); y != 0 && n < 4000; y = ee_read<uint32_t>(y + 0x28), ++n)
                                if (y == x) return true;
                            return false;
                        };
                        const bool near_me = coin(), moved = ri(0, 2) != 0;
                        for (const uint32_t x : ms) {
                            if (!moved || x == m || ri(0, 2) == 0) continue;
                            Vec4 p = near_me ? q : ee_ref<Vec4>(0x0013f3d0u);
                            const float d = near_me ? rf(0.5f, 3.5f) : rf(0.5f, 6.0f), a = rf(-3.1f, 3.1f);
                            p.x += d * std::cos(a); p.y += d * std::sin(a);
                            ee_ref<Vec4>(x + 0x10) = p;
                            if (coin()) w8(x + 0x20, u(ri(1, 9)));
                            if (!listed(x)) { w32(x + 0x28, ee_read<uint32_t>(0x0015ffe4u)); w32(0x0015ffe4u, x); }
                        }
                        if (!listed(m) && coin()) { w32(m + 0x28, ee_read<uint32_t>(0x0015ffe4u)); w32(0x0015ffe4u, m); }
                    }
                    // a hit from the Giant Clank hit table (0x178700, 64 entries of 0x40 bytes)
                    if (ri(0, 2) != 0) {
                        const uint32_t e = OV(0x00178100u) + (u(ri(0, 63)) << 6);
                        w32(e + 0x34, m);
                        { static const uint32_t kv[] = {0x20000u, 0x80000u, 0xa0000u, 0x40000u, 0x20001u}; w32(e + 0x24, kv[ri(0, 4)]); }
                        uint32_t from = hm;
                        const int k = ri(0, 3);
                        if (k == 1 || k == 2) {  // another moby of the level (a class 848 charge: its hits are not taken)
                            from = pool + (u(ri(0, 60)) << 8);
                            if (from == m) from = hm;
                            else if (k == 2) w16(from + 0xa6, 0x350);
                        }
                        if (k == 3) from = 0;
                        w32(e + 0x20, from);
                        Vec4 p = q; p.x += rf(-2, 2); p.y += rf(-2, 2); p.z += rf(0, 3);
                        ee_ref<Vec4>(e) = p;
                        wf(e + 0x10, rf(-1, 1)); wf(e + 0x14, rf(-1, 1)); wf(e + 0x18, rf(0, 1));
                        w32(e + 0x1c, coin() ? 0x45afdf66u : 0u);  // (the push's mark: vec_yaw_marked scales the knock-back)
                        w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4)));
                        w16(e + 0x2a, ri(0, 2) == 0 || ((st == 5 || st == 9) && coin()) ? 0x100u : u(ri(0, 0x60)));
                        { static const float dm[] = {0.0f, 0.5f, 3.0f, 30.0f}; wf(e + 0x2c, dm[ri(0, 3)]); }
                        if (coin()) {  // (a second entry: the hero's, dropped unless a giant hit)
                            const uint32_t e2 = OV(0x00178100u) + (u(ri(0, 63)) << 6);
                            if (e2 < e) { w32(e2 + 0x34, m); w32(e2 + 0x24, 0x20000u); w32(e2 + 0x20, hm); }
                        }
                    }
                    if (coin()) wf(pv + 0x20, coin() ? 0.5f : 12.0f);
                    // the jets' sound slot: none, its own (playing or not), another moby's
                    if (coin()) {
                        w32(pv + 0x1e0, 0xffffffffu);
                    } else {
                        const uint32_t slot = u(ri(0, 7)), se = 0x0013e550u + slot * 0x70;
                        w32(pv + 0x1e0, slot);
                        w32(se + 0x88, ri(0, 3) == 0 ? pool : m);
                        w8(se + 0x74, coin() ? 1u : 0u);
                    }
                    if (st == 64 && coin()) {  // (the moby's saved bits set, for state 0x40 to clear)
                        const int32_t uid = ee_read<int16_t>(m + 0xb2);
                        w8(OV(0x001ba5d0u) + u(uid) + 0x454, 1);
                        w8(OV(0x001bb230u) + u(uid) + 0x454, 1);
                        const uint32_t bit = 1u << (uid & 0x1f), word = u((uid >> 5) << 2);
                        const uint32_t sa = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84u) << 8) + word;
                        w32(sa, ee_read<uint32_t>(sa) | bit);
                        w32(OV(0x001ba4d0u) + word, ee_read<uint32_t>(OV(0x001ba4d0u) + word) | bit);
                    }
                    // state 5's jump to its other jump path, which a random deal seldom meets: a pause hit this frame (kind 9: +0x2a
                    // 0x100, from another moby), the target inside the zone 21 .. 50 away, walking and jump paths allowed
                    if (st == 5 && jumps && ee_read<int32_t>(pv + 0x1d0) >= 0 && ri(0, 2) == 0) {
                        w32(pv + 0x1d8, 0); w32(pv + 0x1d4, 0);
                        wf(pv + 0x178, 60.0f); wf(pv + 0x20, 12.0f);
                        w32(pv + 0x1c4, u(ri(0, 5)));
                        const uint32_t zone = lvd::path(ee_read<int32_t>(pv + 0x1d0));
                        const int32_t n = ee_read<int32_t>(zone);
                        Vec4 h = ee_ref<Vec4>(0x0013f3d0u), c{};
                        for (int32_t j = 0; j < n; ++j) { const Vec4 p = ee_ref<Vec4>(zone + 0x10 + u(j) * 0x10); c.x += p.x / n; c.y += p.y / n; }
                        h.x = c.x + rf(-2.0f, 2.0f); h.y = c.y + rf(-2.0f, 2.0f); h.z = q.z + rf(0.0f, 1.0f);
                        ee_ref<Vec4>(0x0013f3d0u) = h;
                        if (hm != 0) { Vec4 hp = h; hp.w = ee_ref<Vec4>(hm + 0x10).w; ee_ref<Vec4>(hm + 0x10) = hp; }
                        const float r = rf(21.0f, 50.0f), a = rf(-3.1f, 3.1f);
                        Vec4 p = ee_ref<Vec4>(m + 0x10); p.x = h.x + r * std::cos(a); p.y = h.y + r * std::sin(a);
                        ee_ref<Vec4>(m + 0x10) = p;
                        for (uint32_t k = 0; k < 64; ++k) { const uint32_t e = OV(0x00178100u) + (k << 6); if (ee_read<uint32_t>(e + 0x34) == m) w32(e + 0x34, 0); }
                        const uint32_t e = OV(0x00178100u) + (u(ri(0, 63)) << 6);
                        uint32_t from = pool + (u(ri(0, 60)) << 8);
                        if (from == m || ee_read<int16_t>(from + 0xa6) == 0x350) from = pool;
                        w32(e + 0x34, m); w32(e + 0x24, 0x80000u); w32(e + 0x20, from); w16(e + 0x2a, 0x100u); wf(e + 0x2c, 0.0f); w32(e + 0x1c, 0);
                        moby_anim_set(m, 6, 0, 4);
                        w8(m + 0x70, ee_read<uint8_t>(m + 0x70) & 0xfdu);
                    }
                    w32(OV(0x0015f5cc), R() & 0xffff);
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { qua_class491_update_l15(a.i[0]); return uint64_t{0}; });
                g_ignored_words.resize(ign_before);
            }
        }
    }

    // P15.3 (the rest): Quartu's Giant Clank part, hand-ported (quartu_props.cpp, quartu_triggers.cpp, quartu_clank_pad.cpp,
    // quartu_class1446.cpp, quartu_class1469.cpp, quartu_cam22.cpp): the play-test spots, then targeted checks
    g_ignored_words = saved_ignored;
    if (g_oracle_level == 15) {
        const char* only = std::getenv("ORACLE_ONLY");
        if (only == nullptr || std::strcmp(only, "p15_4_info") == 0) {  // the play-test spots: each placed moby of the classes, with the words its code reads
            level();
            const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
            for (const auto& [cname, fn] : std::vector<std::pair<const char*, uint32_t>>{{"class937", 0x002e46b0}, {"class1250", 0x002e73c0},
                     {"class1560", 0x002edb20}, {"class1394", 0x002eabd8}, {"class1257", 0x002e7f68}, {"class1425", 0x002eb928}, {"class1388", 0x002ea748},
                     {"class1419", 0x002eb4c0}, {"class1430", 0x002ec030}, {"class1446", 0x002ec760}, {"class1469", 0x002ed398}}) {
                for (const uint32_t m : mobys_with(fn)) {
                    const Vec4 q = ee_ref<Vec4>(m + 0x10);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    std::printf("  INFO %s: moby %u uid %d at %.1f %.1f %.1f state %u cp %u pv", cname, (m - pool) >> 8, ee_read<int16_t>(m + 0xb2), q.x, q.y, q.z,
                                ee_read<uint8_t>(m + 0x20), ee_read<uint8_t>(m + 0xb0));
                    if (pv != 0) for (uint32_t o = 0; o < 0x60; o += 4) std::printf(" %x", ee_read<uint32_t>(pv + o));
                    std::printf("\n");
                }
            }
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0) {
                    const uint32_t cam = OV(0x00167150) + i * 0xa0;
                    std::printf("  INFO camera %u: row %d entry %d cur86 %d\n", i, ee_read<int16_t>(cam + 0x8c), ee_read<int16_t>(cam + 0x84), ee_read<int16_t>(ee_read<uint32_t>(L15_ADDR(0x00167400u)) + 0x86));
                }
            for (int32_t i = 0; i < lvd::camera_count(); ++i) {
                const uint32_t d = lvd::camera_data(i);
                std::printf("  INFO level camera %d: type %d data", i, ee_read<int32_t>(lvd::camera(i)));
                if (d != 0) for (uint32_t o = 0; o < 0x50; o += 4) std::printf(" %x", ee_read<uint32_t>(d + o));
                std::printf("\n");
            }
        }
    }
    if (g_oracle_level == 15) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        auto maybe_50hz = [] {
            if (ri(0, 2) == 0) { wf(0x0015ed68, 1.2f); wf(0x0015ed6c, 0.02f); wf(0x0015ed70, 0.0004f); }
        };
        if (std::getenv("B240_INFO") != nullptr) {
            level();
            int n750 = 0;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<int16_t>(m + 0xa6) == 0x2ee) ++n750;
            std::printf("INFO class 750 mobys live: %d\n", n750);
        }
        // the hero at `p`, his moby with him (target and distance tests read either)
        auto hero_to = [](Vec4 p) {
            ee_ref<Vec4>(0x0013f3d0u) = p;
            if (const uint32_t hm = ee_read<uint32_t>(0x001413d0u); hm != 0) { Vec4 q = p; q.w = ee_ref<Vec4>(hm + 0x10).w; ee_ref<Vec4>(hm + 0x10) = q; }
        };
        // the help system and the voice line free (the snapshot's busy word and stream refuse every request)
        auto help_free = [] { w32(OV(0x00179410), 0); w32(OV(0x00179434), 0xffffffffu); w32(0x00151720, 0); w32(0x001516ec, 0xffffffffu); };
        auto live = [] {
            std::vector<uint32_t> v;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) v.push_back(m);
            return v;
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto shape_at = [](lvd::BoxList list, int32_t i) { return ee_ref<Vec4>(lvd::box(list, i) + 0x30); };
        auto near = [](Vec4 p, float r, float dz) { const float a = rf(-3.1f, 3.1f), d = rf(0.0f, r); p.x += d * std::cos(a); p.y += d * std::sin(a); p.z += dz; return p; };
        auto index_of = [](uint32_t m) { return (m - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8; };

        // class 937 (0x2e46b0): the linked moby in its states 2 / 4 (and others), its flag +0x10 both ways, no link
        if (const std::vector<uint32_t> ms = mobys_with(0x002e46b0); !ms.empty())
            lvcheck("class937_full", 0x002e46b0, 120, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
                const int r = ri(0, 4);
                if (r == 0) w32(pv, 0xffffffffu);
                else if (r >= 2) w32(pv, index_of(pick(live())));
                if (const int32_t link = ee_read<int32_t>(pv); link != -1) {
                    const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(link) << 8);
                    static const uint8_t st[] = {2, 4, 2, 4, 0, 3, 1};
                    w8(o + 0x20, st[ri(0, 6)]);
                    if (ee_read<uint32_t>(o + 0x78) != 0) w32(ee_read<uint32_t>(o + 0x78) + 0x10, coin() ? 1u : 0u);
                }
                wf(m + 0x48, rf(-3.1f, 3.1f));
                return Args{{m}, {}};
            }, [](A a) { qua_class937_update_l15(a.i[0]); return uint64_t{0}; });

        // class 1560 (0x2edb20): in a scene or not, each shot of the scene, the scene's moby list dealt from the level's mobys
        if (const std::vector<uint32_t> ms = mobys_with(0x002edb20); !ms.empty())
            lvcheck("class1560_full", 0x002edb20, 120, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms);
                w8(m + 0x20, u(ri(0, 2)));
                w32(L15_ADDR(0x0015f5c4u), ri(0, 2) != 0 ? 2u : u(ri(0, 3)));
                w32(L15_ADDR(0x0016ce90u), u(ri(0, 7)));
                const std::vector<uint32_t> lv = live();
                // (scene_moby_smoke smokes only the scene's mobys, class 750: the level's own, when it has any, else any live moby,
                // which it leaves alone; a moby made class 750 is smoked by Veldin's port otherwise than by the level's copy when it
                // has no joint chain of the kind the scenes' mobys have: not dealt)
                std::vector<uint32_t> own;
                for (const uint32_t o : lv) if (ee_read<int16_t>(o + 0xa6) == 0x2ee) own.push_back(o);
                for (uint32_t k = 0; k < 5; ++k) w32(L15_ADDR(0x0016cfd8u) + 4 * k, !own.empty() && ri(0, 4) != 0 ? pick(own) : pick(lv));
                return Args{{m}, {}};
            }, [](A a) { qua_class1560_update_l15(a.i[0]); return uint64_t{0}; });

        // class 1257 (0x2e7f68): a ball spawned as its dropper spawns it (0x2e7ed8's fields), falling onto the floor, a moby or the hero,
        // riding a moby (gone, or one with a pvar word 2), hit by its own class or another, with its burst flag set
        // (this check only: the collision record's w lanes, OV(0x173e40) .. 0x173ec0, a known difference of the shared collision ports)
        const size_t ign_1257 = g_ignored_words.size();
        for (uint32_t k = 0; k < 8; ++k) g_ignored_words.push_back(OV(0x00173e40) + k * 0x10 + 0xc);
        if (const std::vector<uint32_t> drops = mobys_with(0x002eabd8); !drops.empty())
            lvcheck("class1257_full", 0x002e7f68, 240, Ret::None, [=] {
                level(); maybe_50hz();
                const std::vector<uint32_t> lv = live();
                std::vector<uint32_t> solid;
                for (const uint32_t o : lv) if (ee_read<uint32_t>(o + 0x94) != 0) solid.push_back(o);
                const int where = ri(0, 3);
                const Vec4 from = where == 0 ? ee_ref<Vec4>(pick(drops) + 0x10) : where == 1 ? ee_ref<Vec4>(pick(solid) + 0x10) : where == 2 ? ee_ref<Vec4>(0x0013f3d0u)
                                                                                     : ee_ref<Vec4>(pick(mobys_with(0x002e73c0)) + 0x10);
                const uint32_t b = moby_spawn(0x4e9);
                if (b == 0) return Args{{0}, {}};
                const uint32_t pv = ee_read<uint32_t>(b + 0x78);
                w16(b + 0x32, 0x7f); w8(b + 0x30, 0xff); w8(b + 0x31, 1); w8(b + 0x20, 0); w8(b + 0xbc, 0); w32(pv + 0x10, 0);
                ee_ref<Vec4>(pv) = Vec4{};
                ee_ref<Vec4>(b + 0x10) = near(from, 0.5f, where == 0 ? -rf(0.0f, 3.0f) : rf(0.0f, 1.5f));
                w32(b + 0x94, ee_read<uint32_t>(ee_read<uint32_t>(b + 0x24) + 0x10));
                w32(b + 0xa0, 0x80807f7fu);  // (not in the grid)
                moby_update_transform(b);
                const int st = ri(0, 3);
                w8(b + 0x20, u(st == 3 ? ri(2, 5) : st == 2 ? 0 : st));
                if (st == 0 || st == 2) wf(pv + 8, -rf(0.0f, 0.6f));
                if (st == 1) {
                    // (the mounts: the conveyor pieces the droppers drop onto, whose pvar word 2 is their attach record; another solid moby)
                    const uint32_t mount = ri(0, 5) == 0 ? 0u : ri(0, 3) != 0 ? pick(mobys_with(0x002e73c0)) : pick(solid);
                    w32(pv + 0x10, mount);
                    if (mount != 0) {
                        const int k = ri(0, 5);
                        if (k == 0) w8(mount + 0x20, 0xfe); else if (k == 1) w8(mount + 0x20, 0xfd);
                        Vec4 p = ee_ref<Vec4>(mount + 0x10); p.z += rf(0.0f, 1.0f); ee_ref<Vec4>(b + 0x10) = p;
                    }
                }
                if (st == 1 && coin()) wfrand(pv, 3, -0.3f, 0.3f);  // (an old velocity the ride replaces)
                if (ri(0, 5) == 0) w8(b + 0xbc, 1);
                if (ri(0, 2) == 0) {  // a hit naming it, from a moby of its class or another
                    const uint32_t idx = u(ri(0, 63)), e = OV(0x00178100) + (idx << 6);
                    w8(b + 0xa4, idx); w32(e + 0x34, b); w32(e + 0x24, coin() ? 0x830000u : (0x10000u << ri(0, 7)));
                    w32(e + 0x20, ri(0, 3) == 0 ? 0u : coin() ? b : pick(lv));
                }
                seed_rand();
                return Args{{b}, {}};
            }, [](A a) { if (a.i[0] != 0) qua_class1257_update_l15(a.i[0]); return uint64_t{0}; });
        g_ignored_words.resize(ign_1257);

        // class 1425 (0x2eb928): its puffs (one frame in 0x16219c - 1), the particle compared whole (the class loop leaves its +0x2c out)
        if (const std::vector<uint32_t> ms = mobys_with(0x002eb928); !ms.empty())
            lvcheck("class1425_full", 0x002eb928, 120, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms);
                if (coin()) wf(ee_read<uint32_t>(m + 0x78), rf(-2.0f, 6.0f));
                if (ri(0, 2) == 0) w32(L15_ADDR(0x0016219cu), u(ri(2, 6)));
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { qua_class1425_update_l15(a.i[0]); return uint64_t{0}; });

        // class 1388 (0x2ea748): set up with the item held or not, waiting (the hero 3 or more away: nearer starts a scene the
        // interpreter cannot run), its scene running, done
        if (const std::vector<uint32_t> ms = mobys_with(0x002ea748); !ms.empty())
            lvcheck("class1388_full", 0x002ea748, 120, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms);
                static const uint8_t st[] = {0, 1, 2, 3, 4};
                const uint8_t s = st[ri(0, 4)];
                w8(m + 0x20, s);
                w8(0x0013d50a, coin() ? 1u : 0u);
                if (s == 2) w32(L15_ADDR(0x0015f5c4u), 2);  // (the scene still running: the level change waits)
                Vec4 h = ee_ref<Vec4>(m + 0x10); h.x += rf(3.05f, 9.0f) * (coin() ? 1.0f : -1.0f); hero_to(h);
                wf(m + 0x48, rf(-3.1f, 3.1f));
                return Args{{m}, {}};
            }, [](A a) { qua_class1388_update_l15(a.i[0]); return uint64_t{0}; });

        // class 1419 (0x2eb4c0): waiting with its checkpoint collected or not, 0x161ac0, the hero in its box or not, the linked moby
        // in its state 3 or not (all four at once start a scene: left out); its other states inside a scene (or the movie of state 2)
        if (const std::vector<uint32_t> ms = mobys_with(0x002eb4c0); !ms.empty())
            lvcheck("class1419_full", 0x002eb4c0, 120, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78);
                const int s = ri(0, 5);
                w8(m + 0x20, u(s));
                w32(L15_ADDR(0x0015f5c4u), (s == 1 || s == 3 || ri(0, 2) == 0) ? 2u : 0u);
                const uint32_t cp = 0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84u) << 4);
                w8(cp, coin() ? 0xffu : 0u);
                w32(L15_ADDR(0x00161ac0u), ri(0, 3) == 0 ? 1u : 0u);
                const bool in = coin();
                if (in) hero_to(shape_at(lvd::BOXES, ee_read<int32_t>(pv)));
                const uint32_t link = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 8) << 8);
                w8(link + 0x20, (in && ee_read<uint8_t>(cp) != 0xff && ee_read<uint32_t>(L15_ADDR(0x00161ac0u)) == 0) ? 2u : (coin() ? 3u : 1u));
                return Args{{m}, {}};
            }, [](A a) { qua_class1419_update_l15(a.i[0]); return uint64_t{0}; });

        // the subtitle callback (0x2eb6b8): the scene's shots 3 / 4 (and others), its frame inside each text's window, at its edges, or
        // anywhere; the packet cursor on a buffer of the test area
        if (const std::vector<uint32_t> ms = mobys_with(0x002eb4c0); !ms.empty())
            lvcheck("cb2eb6b8_text", 0x002eb6b8, 240, Ret::None, [=] {
                level(); maybe_50hz();
                w32(OV(0x001611c0), 0x00800000u + u(ri(0, 8)) * 0x10u);
                static const int32_t shots[] = {3, 3, 3, 4, 4, 1, 5};
                const int32_t shot = shots[ri(0, 6)];
                static const int32_t win[4][2] = {{0xb5, 0x104}, {0x1c2, 0x280}, {0x2ee, 0x3a2}, {0x8fc, 0x960}};
                const int w = shot == 4 ? 3 : ri(0, 2);
                const int32_t lo = frame_scale_i(win[w][0]), hi = frame_scale_i(win[w][1]);
                const int e = ri(0, 5);
                const int32_t t = e == 0 ? lo : e == 1 ? hi : e == 2 ? lo + 1 : e == 3 ? hi - 1 : e == 4 ? ri(0, 0xa00) : ri(lo + 1, hi - 1);
                w32(L15_ADDR(0x0016ce90u), u(shot));
                w32(L15_ADDR(0x0016ce94u), u(t));
                return Args{{pick(ms)}, {}};
            }, [](A a) { qua_cb_2eb6b8_l15(a.i[0]); return uint64_t{0}; });

        // class 1430 (0x2ec030) and its helpers: each state, with its saved bits, its groups, the item, the hero near or not, Giant
        // Clank or not, the level camera's frame mode (0x2f78e0), the Clank spawned / missing / swinging out and back, its group's
        // mobys gone (0x2ebfb0's count to 10), the reward taken or not, the linked moby it rides along with
        if (const std::vector<uint32_t> ms = mobys_with(0x002ec030); !ms.empty())
            lvcheck("class1430_full", 0x002ec030, 480, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78), pool = ee_read<uint32_t>(OV(0x0015ffd8));
                const int s = ri(0, 5);
                w8(m + 0x20, u(s));
                if (ri(0, 2) == 0) w32(pv + 0xc, 0xffffffffu);
                if (const int32_t link = ee_read<int32_t>(pv + 0xc); link != -1) {
                    Vec4 p = ee_ref<Vec4>(pool + (u(link) << 8) + 0x10); p.x += rf(-0.5f, 0.5f); p.y += rf(-0.5f, 0.5f); ee_ref<Vec4>(pv + 0x10) = p;
                }
                const int32_t group = ee_read<int32_t>(pv + 0x20);
                for (uint32_t k = 1; k < 24; ++k) if (ri(0, 7) == 0) w32(pv + 0x20 + 4 * k, u(ri(0, 20)));
                const int32_t uid = ee_read<int16_t>(m + 0xb2);
                if (s == 0) {
                    if (ri(0, 3) == 0) w8(L15_ADDR(0x001bb830u) + u(uid) + 0x454, 1);
                    else if (ri(0, 2) == 0) { const uint32_t wa = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84u) << 8) + u((uid >> 5) << 2); w32(wa, ee_read<uint32_t>(wa) | (1u << (uid & 0x1f))); }
                }
                w8(0x0013d38a, ri(0, 3) != 0 ? 1u : 0u);
                if (ri(0, 2) != 0) hero_to(near(ee_ref<Vec4>(m + 0x10), coin() ? 2.9f : 6.0f, 0.0f));
                w8(0x001413f4, ri(0, 3) == 0 ? 2u : (ri(0, 3) == 0 ? 1u : 0u));
                {  // 0x2f78e0: the current camera's type and the level camera's frame mode
                    w16(ee_read<uint32_t>(L15_ADDR(0x00167400u)) + 0x86, coin() ? 0x13u : u(ri(0, 2)));
                    const uint32_t fr = ee_read<uint32_t>(ee_read<uint32_t>(lvd::camera_data(ee_read<int32_t>(pv)) + 4) + 0x70);
                    static const uint16_t modes[] = {1, 3, 0, 2};
                    w16(fr + 0x54, modes[ri(0, 3)]);
                    wf(L15_ADDR(0x0015f3fcu), coin() ? 0.0f : rf(0.1f, 2.0f));
                }
                if (s >= 3) {  // the Clank: spawned, another moby, or missing
                    uint32_t c = 0;
                    if (ri(0, 3) != 0) {
                        c = moby_spawn(0x594);
                        if (c != 0) { w32(c + 0xa0, 0x80807f7fu); ee_ref<Vec4>(c + 0x10) = ee_ref<Vec4>(m + 0x10); }
                    }
                    w32(pv + 4, c);
                    wf(pv + 8, coin() ? rf(-3.14f, 3.14f) : rf(2.0f, 3.14f) * (coin() ? 1.0f : -1.0f));
                }
                if (s == 4) {
                    w8(0x0013d3f5, ri(0, 3) == 0 ? 1u : 0u);
                    if (ri(0, 5) == 0) w32(pv + 0x20, u(ri(0, 111)));  // (a group that may have no list)
                    if (const uint32_t e0 = lvd::group(group); e0 != 0) {  // some of its group's mobys gone
                        const int keep = ri(0, 3) == 0 ? 0 : ri(0, 40);
                        int k = 0;
                        const int how = ri(0, 2);  // (gone as 0xfe, as 0xfd, or either)
                        for (uint32_t e = e0;; e += 2, ++k) { if (k >= keep) w8(pool + ((ee_read<uint16_t>(e) & 0x7fffu) << 8) + 0x20, how == 0 ? 0xfeu : how == 1 ? 0xfdu : coin() ? 0xfeu : 0xfdu); if (ee_read<int16_t>(e) < 0) break; }
                    }
                }
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { qua_class1430_update_l15(a.i[0]); return uint64_t{0}; });

        // class 1446 (0x2ec760): its states but the scene starts and the level change (5, 7, 8), state 6 in a scene; its talk with the
        // hero in its box and free; its groups gone; its head's look point (the hero in front within 8 or not, moving or not, its
        // timers at their edges)
        if (const std::vector<uint32_t> ms = mobys_with(0x002ec760); !ms.empty())
            lvcheck("class1446_full", 0x002ec760, 480, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78), pool = ee_read<uint32_t>(OV(0x0015ffd8));
                static const uint8_t st[] = {0, 1, 1, 2, 3, 3, 4, 6, 9, 10};
                const uint8_t s = st[ri(0, 9)];
                if (s != 0) { w8(m + 0x20, 0); qua_class1446_update_l15(m); }  // (its talk record set up)
                w8(m + 0x20, s);
                if (ri(0, 7) == 0) w32(pv + 0x14c, 0xffffffffu);
                w8(0x001413f4, coin() ? 2u : 0u);
                w32(L15_ADDR(0x0015f5c4u), ri(0, 2) == 0 ? 2u : 0u);  // (state 6 out of a scene: its movie, which the interpreter runs)
                const uint32_t cp = 0x0014c050u + ee_read<uint32_t>(pv + 0x280) + (ee_read<uint32_t>(0x0015ed84u) << 4);
                if (s == 0 || s == 1) {  // (mates of its checkpoint for the retire loop: live mobys given its +0x280 as their +0xb0)
                    w8(cp, coin() ? 0xffu : 0u); w8(0x0013dd50, coin() ? 1u : 0u);
                    std::vector<uint32_t> lv;  // (the level's moby list 0x15ffe4, +0x28 the next: the mobys the retire loop walks)
                    for (uint32_t o = ee_read<uint32_t>(L15_ADDR(0x0015ffe4u)); o != 0 && lv.size() < 400; o = ee_read<uint32_t>(o + 0x28)) lv.push_back(o);
                    if (lv.empty()) lv.push_back(m);
                    for (int k = ri(0, 3); k > 0; --k) {
                        const uint32_t o = pick(lv);
                        w8(o + 0xb0, ee_read<uint8_t>(pv + 0x280));
                        if (ri(0, 2) == 0) w8(o + 0x20, coin() ? 0xfeu : 0xfdu);
                    }
                    if (coin()) w8(m + 0xb0, ee_read<uint8_t>(pv + 0x280));  // (itself among them)
                }
                if (s == 1) {  // (the hero in its box and free to talk runs talk_update, whose voice stream (a disc read) the interpreter
                               // cannot run: in its box he is busy (+0xc 3), else anywhere)
                    const bool in = coin();
                    if (in) hero_to(shape_at(lvd::BOXES, ee_read<int32_t>(pv + 0x27c)));
                    static const uint32_t fr[] = {0, 1, 9, 3};
                    w32(0x001413dc, in ? 3u : fr[ri(0, 3)]);
                }
                if (s == 2) w16(pv + 4, coin() ? 1u : 0u);
                if (s == 3) {
                    w8(0x0013f350u + 0x20a4, coin() ? 2u : 0u);
                    if (ri(0, 2) == 0) w32(pv + 0x158, 0xffffffffu);
                    else if (coin()) w32(pv + 0x158, index_of(pick(live())));
                    const int gone = ri(0, 2);  // (0: as they are, 1: all at 0x40 (done), 2: all at 0x40 but one)
                    for (const uint32_t g : {0x144u, 0x148u})
                        if (const uint32_t e0 = lvd::group(ee_read<int32_t>(pv + g)); e0 != 0 && gone != 0)
                            for (uint32_t e = e0;; e += 2) { w8(pool + ((ee_read<uint16_t>(e) & 0x7fffu) << 8) + 0x20, (gone == 2 && ri(0, 3) == 0) ? 1u : 0x40u); if (ee_read<int16_t>(e) < 0) break; }
                    if (ee_read<int32_t>(pv + 0x158) != -1 && coin()) w8(pool + (ee_read<uint32_t>(pv + 0x158) << 8) + 0x20, coin() ? 0xfeu : 0xfdu);
                }
                if (s == 4) w32(pv + 0x15c, u(ri(2, 60)));  // (its timer running: once out, the music stream starts, which the interpreter cannot run)
                // the head
                if (s != 1) {  // (state 1: the hero left where it put him)
                    const float yaw = ee_read<float>(m + 0x48), off = coin() ? rf(-1.4f, 1.4f) : rf(1.7f, 3.1f) * (coin() ? 1.0f : -1.0f), d = coin() ? rf(0.5f, 7.9f) : rf(8.1f, 20.0f);
                    Vec4 h = ee_ref<Vec4>(m + 0x10); h.x += d * std::cos(yaw + off); h.y += d * std::sin(yaw + off); h.z += rf(-6.0f, 4.0f);
                    hero_to(h);
                    ee_ref<Vec4>(0x0013f420u) = Vec4{h.x, h.y, h.z + 1.0f, 0.0f};
                }
                ee_ref<Vec4>(0x0013f450u) = coin() ? Vec4{} : Vec4{rf(-0.1f, 0.1f), rf(-0.1f, 0.1f), 0.0f, 0.0f};
                static const uint32_t t1[] = {0, 1, 2, 60};
                w32(pv + 0x274, t1[ri(0, 3)]);
                w32(pv + 0x278, t1[ri(0, 3)]);
                w8(0x0015edb0, coin() ? 1u : 0u);
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { qua_class1446_update_l15(a.i[0]); return uint64_t{0}; });

        // class 1469 (0x2ed398): the hero in each of its boxes or none, the game's flags and counters each way, the help system free or
        // busy, the pad moby alive or not, the reminders' counters at the 18-second edge
        if (const std::vector<uint32_t> ms = mobys_with(0x002ed398); !ms.empty())
            lvcheck("class1469_full", 0x002ed398, 480, Ret::None, [=] {
                level(); maybe_50hz();
                const uint32_t m = pick(ms), pv = ee_read<uint32_t>(m + 0x78), pool = ee_read<uint32_t>(OV(0x0015ffd8));
                w8(m + 0x20, ri(0, 7) == 0 ? u(ri(0, 2)) : 1u);
                if (ri(0, 2) != 0) help_free();
                static const uint32_t boxes[] = {0x0, 0xc, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x54};
                if (ri(0, 7) != 0) hero_to(near(shape_at(lvd::BOXES, ee_read<int32_t>(pv + boxes[ri(0, 7)])), 0.5f, 0.0f));
                w8(0x0013d4df, coin() ? 1u : 0u); w8(0x0013d4c4, coin() ? 1u : 0u);
                w8(0x0013d3f7, ri(0, 3) == 0 ? 1u : 0u); w8(0x0013d3f6, ri(0, 3) == 0 ? 1u : 0u);
                const int32_t now = ri(0, 0x40000);
                w32(0x0015eea4, u(now));
                const int32_t step = frame_scale_i(now) / 600;
                for (const uint32_t a : {0x00141d42u, 0x00141d4au}) w16(a, u(std::max(0, step - ri(-1, 3))));
                w16(0x00141d48, coin() ? 1u : 0u);
                w16(0x00141db0, ri(0, 3) == 0 ? 1u : 0u); w16(0x00141da8, ri(0, 3) == 0 ? 1u : 0u);
                w32(0x00141d54, coin() ? 0u : 0xffffffffu); w32(0x00141d6c, coin() ? 0u : 0xffffffffu); w32(0x00141d7c, coin() ? 0u : R());
                w32(L15_ADDR(0x0015f5a4u), coin() ? 1u : 0u); w32(L15_ADDR(0x00161ac0u), ri(0, 3) == 0 ? 1u : 0u);
                static const uint32_t fr[] = {0, 1, 9, 4};
                w32(0x001413dc, fr[ri(0, 3)]);
                static const uint8_t modes[] = {0, 2, 3, 3};
                w8(0x001413f4, modes[ri(0, 3)]);
                w16(0x0013f350u + 0x22e0, ri(0, 3) == 0 ? 1u : 0u);
                if (ri(0, 3) == 0) w32(0x0013f350u + 0x2084, 0x35);
                w32(pv + 0x34, coin() ? u(frame_scale_i(0x4b0)) : u(ri(0, 4)));
                w32(pv + 0x3c, coin() ? u(ri(0, 8)) : u(ri(0, 0x600)));
                w32(pv + 0x38, coin() ? u(frame_scale_i(0xe10) + ri(-1, 2)) : u(ri(0, 0x1000)));
                w32(pv + 0x40, coin() ? 0xffffffffu : ee_read<uint32_t>(pv + boxes[ri(0, 7)]));
                if (const int32_t k = ee_read<int32_t>(pv + 0x10); k != -1) {
                    static const uint8_t pst[] = {0, 1, 2, 3, 0xfe, 1};
                    w8(pool + (u(k) << 8) + 0x20, pst[ri(0, 5)]);
                    if (ri(0, 7) == 0) w16(pool + (u(k) << 8) + 0xa6, 0x4f);
                }
                if (ri(0, 5) == 0) w32(pv + 0x10, 0xffffffffu);
                if (ri(0, 2) == 0) {  // the hints' conditions met: the pad alive, the hero free and in one of the boxes, the counters past their waits
                    help_free();
                    if (const int32_t k = ee_read<int32_t>(pv + 0x10); k != -1) { w8(pool + (u(k) << 8) + 0x20, 1); w16(pool + (u(k) << 8) + 0xa6, 0x4e); }
                    w32(0x001413dc, coin() ? 0u : 9u);
                    static const uint32_t hb[] = {0x0, 0xc, 0xc, 0x1c, 0x20, 0x24, 0x28};
                    const uint32_t bx = hb[ri(0, 6)];
                    hero_to(near(shape_at(lvd::BOXES, ee_read<int32_t>(pv + bx)), 0.3f, 0.0f));
                    if (ri(0, 2) == 0) w32(pv + 0x40, ee_read<uint32_t>(pv + bx));  // (the box's message given already)
                    static const uint32_t hs[] = {0x6a, 0x82, 0x76, 0x75, 0x0};  // (sub_0020d568: the hero's states that allow the minute's hint)
                    w32(0x001413d4, hs[ri(0, 4)]);
                    if (ri(0, 3) == 0) w32(0x001413dc, u(ri(0x11, 0x12)));
                    if (bx == 0x1c) { w8(0x001413f4, 2); w8(0x0013d3f7, 0); w32(0x00141d6c, 0); }
                    w32(pv + 0x3c, u(ee_read<int32_t>(pv + 0x34) + ri(0, 3)));
                    w32(pv + 0x38, u(frame_scale_i(0xe10) + ri(-1, 2)));
                    w8(0x0013d4c4, 1);
                    if (coin()) {  // the reminders' 18 seconds at their edge: the game time ~1080 frames past the counter's step
                        const int32_t hu = ri(1, 300), target = hu * 600 + frame_scale_i(0x12) * 60 + ri(-2, 2);
                        int32_t t = target;
                        for (int k = 0; k < 40000 && frame_scale_i(t) < target; ++k) ++t;
                        w32(0x0015eea4, u(t));
                        w16(0x00141d42, u(hu)); w16(0x00141d4a, u(hu)); w16(0x00141d48, coin() ? 1u : 0u);
                    }
                }
                // Giant Clank's animation 0xc (0x13d3f7)
                if (coin()) w8(ee_read<uint32_t>(0x001413d0u) + 0x52, coin() ? 0xcu : 0x3u);
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { qua_class1469_update_l15(a.i[0]); return uint64_t{0}; });

        // camera type 22 (0x2f8ba8, with 0x2f88e8 / 0x2f8698 / 0x2f85a8): a live camera of the type, the current camera's +0x86, the
        // data's counters at their edges, the hero Giant Clank or not, inside the data's volumes or out, the claim free / its own /
        // another's, the right stick, the data's rate / distance / heights set or not
        {
            std::vector<uint32_t> cams;
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 7) cams.push_back(OV(0x00167150) + i * 0xa0);
            auto deal_cam = [=](bool counters) {
                level(); maybe_50hz();
                const uint32_t cam = pick(cams), d = lvd::camera_data(ee_read<int16_t>(cam + 0x84));
                const uint32_t cur = ee_read<uint32_t>(L15_ADDR(0x00167400u));
                w16(cur + 0x86, ri(0, 3) == 0 ? 1u : 0u);
                w8(0x001413f4, ri(0, 3) != 0 ? 2u : 0u);
                const uint32_t frm = ee_read<uint32_t>(cur + 0x70);
                w32(frm + 0x220, ri(0, 2) == 0 ? 0u : coin() ? cam : pick(cams));
                // the volumes: the hero in the data's cylinder / excluded cylinder / box / sphere / polygon, or away
                if (ri(0, 2) == 0) w32(d + 0x28, 0xffffffffu);
                if (ri(0, 3) == 0) { w32(d + 0x10, 0xffffffffu); w32(d + 0xc, coin() ? u(ri(0, 40)) : 0xffffffffu); if (coin()) w32(d + 8, u(ri(0, 10))); else if (coin()) w32(d + 0x14, u(ri(0, 40))); }
                const int where = ri(0, 5);
                if (where == 0 && ee_read<int32_t>(d + 0x10) >= 0) hero_to(near(shape_at(lvd::CYLINDERS, ee_read<int32_t>(d + 0x10)), 1.0f, 0.0f));
                else if (where == 1 && ee_read<int32_t>(d + 0x28) >= 0) hero_to(near(shape_at(lvd::CYLINDERS, ee_read<int32_t>(d + 0x28)), 1.0f, 0.0f));
                else if (where == 2 && ee_read<int32_t>(d + 0xc) >= 0) hero_to(near(shape_at(lvd::BOXES, ee_read<int32_t>(d + 0xc)), 1.0f, 0.0f));
                else if (where == 3 && ee_read<int32_t>(d + 8) >= 0) hero_to(near(shape_at(lvd::SPHERES, ee_read<int32_t>(d + 8)), 1.0f, 0.0f));
                else if (where == 4 && ee_read<int32_t>(d + 0x14) >= 0) { const uint32_t p = lvd::path(ee_read<int32_t>(d + 0x14)); if (p != 0) hero_to(ee_ref<Vec4>(p + 0x10)); }
                if (counters) {
                    const int32_t edges[] = {-1, 0, 1, frame_scale_i(0xc8), frame_scale_i(0xc8) + 1, frame_scale_i(0x12c) - 1, frame_scale_i(0x12c), frame_scale_i(0x230) - 1, frame_scale_i(0x230), ri(0, 0x300)};
                    w16(d + 0x20, u(edges[ri(0, 9)]));
                    const int32_t c3[] = {0, frame_scale_i(0x190) - 2, frame_scale_i(0x190) - 1, ri(0, 0x300)};
                    w32(d + 0x3c, u(c3[ri(0, 3)]));
                    wf(0x0013ca40, ri(0, 3) == 0 ? rf(-1, 1) : 0.0f); wf(0x0013ca44, ri(0, 3) == 0 ? rf(-1, 1) : 0.0f);
                    for (const uint32_t o : {0x0u, 0x34u, 0x38u, 0x44u}) if (ri(0, 3) == 0) wf(d + o, coin() ? 0.0f : rf(-20.0f, 30.0f));
                }
                // the current camera's place and direction near its box or not (0x2f8698's angles)
                if (coin()) { Vec4 p = ee_ref<Vec4>(cur + 0x30); p.x += rf(-10, 10); p.y += rf(-10, 10); p.z += rf(-5, 5); ee_ref<Vec4>(cur + 0x30) = p; }
                if (coin()) wfrand(cur, 3, -1, 1);
                else if (counters && coin()) {  // (its direction toward the data's box: 0x2f8698 true in 0x2f88e8)
                    const Vec4 b = ee_ref<Vec4>(lvd::box(lvd::BOXES, ee_read<int32_t>(d + 0x48)) + 0x30), c = ee_ref<Vec4>(cur + 0x30);
                    ee_ref<Vec4>(cur) = Vec4{b.x - c.x + rf(-0.5f, 0.5f), b.y - c.y + rf(-0.5f, 0.5f), b.z - c.z, 0.0f};
                }
                return cam;
            };
            if (!cams.empty()) {
                lvcheck("cam22_full", 0x002f8ba8, 400, Ret::Int32, [=] { return Args{{deal_cam(true), 0}, {}}; },
                        [](A a) { return r32(qua_cam_type22_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
                lvcheck("cam22_2f85a8", 0x002f85a8, 200, Ret::Int32, [=] { return Args{{deal_cam(false)}, {}}; },
                        [](A a) { return r32(qua_2f85a8_l15(a.i[0]) ? 1u : 0u); });
                lvcheck("cam22_2f8698", 0x002f8698, 300, Ret::Int32, [=] {
                    const uint32_t cam = deal_cam(false);
                    static const float tol[] = {0.0f, 30.0f, 5.0f, 90.0f};
                    return Args{{cam}, {coin() ? tol[ri(0, 3)] : rf(0, 180), coin() ? tol[ri(0, 3)] : rf(0, 180)}};
                }, [](A a) { return r32(qua_2f8698_l15(a.i[0], a.f[0], a.f[1]) ? 1u : 0u); });
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
