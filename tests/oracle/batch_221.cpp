// Oracle batch 221: level 14's (Oltanis) own code on its real level data. Run with
// ORACLE_LEVEL=14 build/tests/ovl_oracle --batch 221 (needs the ISO; nothing else: the level is laid out from the disc).
// The subjects are the level's own mobys of each class (placed by its gameplay data) after the first gameplay frame,
// perturbed; the original is level 14's own function at its own address, the port the one registered for it (hand-written
// since the own-code tasks P14.* and Y5; at first the transliterations oltanis_classes_tr_N.cpp / oltanis_hero_tr_N.cpp, generated
// by tools/codegen/mkall_l14.py). Checked: the 59 class updates, the classes' 17 draw-list
// callbacks, sound emitter types 1 / 3 / 4 / 6, camera types 0x13 / 0x14 / 0x15, class 252's five hooks and class 270's +0xc (Veldin's
// port), the camera mode hook and its callback, the cutscene extras, the movement-mode hero update (the Novalis family's build), and the
// Veldin ports of moby_spawn / mobys_in_sphere against level 14's copies. The rest of the hero controller: the unit batches 44..55
// with ORACLE_LEVEL=14. The own-code tasks' checks (P14.1: classes 903 / 8 / 30 / 31, P14.2: classes 81 / 211 / 309 / 386 / 557 /
// 610 / 643 / 681 / 684 / 685 and 1.02 fix 49, "p142_class685_fix49", run with RAC1_FIXES102=49; P14.3: classes 712 / 851 / 908 / 921 /
// 922 / 923 / 924 / 1193) are near the end.

#include "ovl_oracle.h"
#include "trial_guard.h"
#include "class_hook_deal.h"

#include <game/enhance/advanced.h>
#include <game/level/level_data.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/shared_class1224.h>
#include <game/ovl/shared_class1331.h>
#include <game/ovl/shared_class1400.h>
#include <game/ovl/shared_class1511.h>
#include <game/ovl/shared_class1629.h>
#include <game/ovl/shared_class250.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class28.h>
#include <game/ovl/shared_class28_draw.h>
#include <game/ovl/shared_class325.h>
#include <game/ovl/shared_class403.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class686.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class933.h>
#include <game/ovl/shared_class994.h>
#include <game/ovl/shared_cutscene_fx.h>
#include <game/ovl/shared_env_objects.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/shadows.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l14.h>
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
// a hand-written class hook (moby, position, target, float) in the hook table's uniform form: the levels' copies of
// Veldin's hooks and the levels' hooks written by hand run these (T00)
template <int32_t (*H)(uint32_t, uint32_t, uint32_t, float)>
TrRet hook_uniform(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float f12, float, float, float,
                   float, float, float, float) {
    return TrRet{static_cast<uint32_t>(H(a0, a1, a2, f12)), 0.0f};
}
// level 14's hook +8 of class 252's table (L14:0x2dae00): the shared carry_state_hook with Oltanis's states (hook +8, carried 5, released 1)
int32_t olt_hook_2dae00(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_state_hook(m, a1, a2, f12, 8, 5, 1); }

// A random state can hold a pointer outside RAM that the port's vector helpers dereference without a check (a SIGSEGV where the interpreter would wrap it): the trial run of a state
// turns it into "not a game state" (the state is dealt again), as the port's "Pointer resolution out of bounds" exception does.

constexpr uint32_t MOBY_SPAWN_L14 = 0x00256d38;

// (stage 5, task C2a) The checks named here reach mobys_in_sphere (the level's copy), whose cell list and vertices the port keeps on
// the stack (the original: 0x70003800, 0x70002000): A2's collision scratch ranges are left out of these checks, and only of
// them. They also hide what moby_pose_matrices (called for a posed moby) writes inside them: the joint positions at
// 0x70000000 .., read back by the primitive tests, so a wrong one still shows in the hit list.
// (Task C2b: the checks added to the list reach sphere_collide or capsule_collide, whose chunk buffers,
// vertices and cell walk the port keeps on the host stack too.)
bool c2a_spr_check(const char* name) {
    static const char* const kNames[] = {"class8", "class8_cov", "class31_cov", "class28", "mobys_in_sphere", "class923", "hero_move_mode",
                                         "cam_type20_init", "class1134", "class1224", "class1354", "class1400",
                                         "class1818", "class252", "class805",
                                         // (P14.2: the wings' sphere test, the shots' segment tests, the explosions' debris of
                                         // the drone and the mine)
                                         "p142_class81", "p142_class681", "p142_class211", "p142_class643",
                                         // (P14.3: the carrier craft's look at its cargo, the carrier's fall, the bomb's spheres and way)
                                         "p143_class921", "p143_class923", "p143_class1193",
                                         // (P14.3: class 851's floor band, as class31_cov's)
                                         "p143_class851",
                                         // (P14.4: the pickup's floor probe; type 20's hero_place, as cam_type20_init's)
                                         "p144_class1354", "p144_cam20_init", "p144_cam20_update"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is level 14's own function at `l9addr` (not a Veldin address to translate).
void l14check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B221_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 14) {
        std::printf("%-26s L14:0x%08x  skipped (level 14 only: run with ORACLE_LEVEL=14)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L14:0x%08x\n", name, l9addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2a_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_221() {
    if (g_only_batch != 221) return;
    g_batch = 221;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 (Pokitaru) builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
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
            w8(0x0013d3f0, 1);  // (the arrival scene seen: class 684 would start scene 0 on the first frame, its stream read from the disc)
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's mobys whose update function is `fn` (level 14's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    static int redeal_count = 0;
    auto redeals = []() -> int& { return redeal_count; };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with, redeals](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 14) { l14check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L14:0x%08x  no moby on this level\n", name, fn); return; }
        l14check(name, fn, n, Ret::None, [level, ms, vary, port, redeals] {
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
                ++redeals();
            }
            return Args{{m}, {}};
        }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
        // B221_REDEALS=1: how many states the trial run of the port refused (dealt again); a fault that makes the port throw
        // shows here, not as a difference
        if (std::getenv("B221_REDEALS") != nullptr) std::printf("REDEALS %s: %d\n", name, redeals());
        redeals() = 0;
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
    if (g_oracle_level == 14) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (transliterated by tools/codegen/mkall_l14.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, per class, below)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class903", 0x002aba80, olt_class903_update_l14, 0x0, false},
        {"class8", 0x002ac618, olt_class8_update_l14, 0x0, false, 0, 0xffff},  // (pass 1: the pvars' small counters moved send its path walk (path_advance) round a path that has no length: both sides loop forever)
        {"class28", 0x002b17d8, class28_update, 0x0, false},
        {"class30", 0x002b3bf0, olt_class30_update_l14, 0x0, false},
        {"class31", 0x002b46e8, olt_class31_update_l14, 0x0, false},
        {"class81", 0x002bac78, olt_class81_update_l14, 0x0, false},
        {"class211", 0x002d5f40, olt_class211_update_l14, 0x0, false},
        {"class250", 0x002d96e0, class250_update, 0x0, false},
        {"class252", 0x002d98f0, class252_update, 0x0, false},
        {"class258", 0x002db378, class258_update, 0x0, false},
        {"class309", 0x002de1f8, olt_class309_update_l14, 0x0, false},
        {"class325", 0x002de670, class325_update, 0x0, false},
        {"class386", 0x002dee28, olt_class386_update_l14, 0x0, false},
        {"class403", 0x002dfd10, class403_update, 0x0, false},
        {"class455", 0x002e1b20, class455_update, 0x0, false},
        {"class531", 0x002e7190, empty_update, 0x0, false},
        {"class545", 0x002e72d8, class545_update, 0x0, false},
        {"class557", 0x002e7bd8, olt_class557_update_l14, 0x0, false},
        {"class604", 0x002ea9d0, class604_update, 0x0, false},
        {"class605", 0x002ead20, zone_marker_update, 0x0, false},
        {"class610", 0x002eaf88, olt_class610_update_l14, 0x0, false},
        {"class643", 0x002ec810, olt_class643_update_l14, 0x0, false},
        {"class681", 0x002ece00, olt_class681_update_l14, 0x0, false},
        {"class684", 0x002ed280, olt_class684_update_l14, 0x0, false},
        {"class685", 0x002ee8d8, olt_class685_update_l14, 0x0, false},
        {"class686", 0x002ef840, class686_update, 0x0, false},
        {"class712", 0x002f0538, olt_class712_update_l14, 0xb8, false},  // (states 3 / 4 / 5 / 7 follow a pointer the first frame has not set: every random state sends the port outside RAM)
        {"class758", 0x002f2cb8, swingshot_target_update, 0x0, false},
        {"class765", 0x002f3ef0, empty_update, 0x0, false},
        {"class805", 0x002f4190, spawn_point_update_l1, 0x0, false},
        {"class832", 0x002f6338, class832_update, 0x0, false},
        {"class851", 0x002fba20, olt_class851_update_l14, 0x0, false},
        {"class908", 0x002fc0f0, olt_class908_update_l14, 0x0, false},
        {"class913", 0x002fcba8, empty_update, 0x0, false},
        {"class921", 0x002fcbb0, olt_class921_update_l14, 0x0, false},
        {"class922", 0x002fdbb8, olt_class922_update_l14, 0x0, false},
        {"class923", 0x002fe2a0, olt_class923_update_l14, 0x0, false},
        {"class924", 0x002fefe0, olt_class924_update_l14, 0x0, false},
        {"class933", 0x002ff640, class933_update, 0x0, false},
        {"class1134", 0x002ffc18, gold_bolt_update, 0x4, false},  // (state 2 requests the level change)
        {"class1193", 0x00300e00, olt_class1193_update_l14, 0x0, false},
        {"class1224", 0x003015d0, class1224_update, 0x0, false},
        {"class1260", 0x00303370, class994_update, 0x0, false},
        {"class1331", 0x003039e0, class1331_update, 0x0, false},
        {"class1352", 0x00305408, olt_class1352_update_l14, 0x0, false},
        {"class1354", 0x00305758, olt_class1354_update_l14, 0x4, false},  // (state 2 requests the level change)
        {"class1395", 0x00305d28, olt_class1395_update_l14, 0x0, false},
        {"class1396", 0x00306150, empty_update, 0x0, false},
        {"class1397", 0x003061d8, olt_class1397_update_l14, 0x0, false},
        {"class1400", 0x00306390, class1400_update, 0x0, false},
        {"class1416", 0x00306b78, olt_class1416_update_l14, 0x0, false},
        {"class1417", 0x00306ee0, olt_class1417_update_l14, 0x0, false},
        {"class1418", 0x00307a80, olt_class1418_update_l14, 0x0, false},
        {"class1511", 0x00307ad8, class1511_update, 0x0, false},
        {"class1514", 0x00308350, empty_update, 0x0, false},
        {"class1559", 0x003087c0, olt_class1559_update_l14, 0x0, false},
        {"class1629", 0x003088b0, class1629_update, 0x0, false},
        {"class1633", 0x00308be0, class1633_update, 0x0, false},
        {"class1818", 0x00309c88, sandmouse_update, 0x0, false},
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
    if (g_oracle_level == 14 && std::getenv("B221_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B221_CLS") != nullptr && std::strcmp(std::getenv("B221_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 14 && std::getenv("B221_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B221_ST") == nullptr || std::atoi(std::getenv("B221_ST")) == st) && (std::getenv("B221_PASS") == nullptr || std::atoi(std::getenv("B221_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 14's own moby callbacks (draw-list callbacks queued by `jal 0x20c5d0 / 0x20c698 / 0x20c760 / 0x20c828`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 14) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        // the display packet cursor (*0x1611c0) is null in the unit batches' memory: the draw helpers' packet writes land in low memory (0x100 .. 0x1500; the cursor is given a real buffer below) and carry a stale
        // callee-saved register (s7) of the original's frame: not game state
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        static const Cb cbs[] = {
        {"cb2aee28", 0x002aee28, olt_cb_2aee28_l14, 0x002ac618, 8},
        {"cb2b2928", 0x002b2928, class28_draw_glows, 0x002b17d8, 28},
        {"cb2d71f0", 0x002d71f0, olt_cb_2d71f0_l14, 0x002d5f40, 211},
        {"cb2da678", 0x002da678, class252_draw_beam, 0x002d98f0, 252},
        {"cb2daf08", 0x002daf08, class252_draw_glow, 0x002d98f0, 252},
        {"cb2dec40", 0x002dec40, class325_draw, 0x002de670, 325},
        {"cb2df458", 0x002df458, olt_cb_2df458_l14, 0x002dee28, 386},
        {"cb2eb810", 0x002eb810, olt_cb_2eb810_l14, 0x002eaf88, 610},
        {"cb2edc18", 0x002edc18, olt_cb_2edc18_l14, 0x002ed280, 684},
        {"cb2f6128", 0x002f6128, class832_draw, 0x002f6338, 832},
        {"cb301fa8", 0x00301fa8, class1224_draw, 0x003015d0, 1224},
        {"cb303350", 0x00303350, [](uint32_t m) { ovl_call_moby_fn(0x00303350, m); }, 0x00303370, 1260},  // (task Y5: the level's registration, the shared sea_draw)
        {"cb307a28", 0x00307a28, env_objects_callback, 0x00307a80, 1418},
        {"cb308150", 0x00308150, class1511_draw, 0x00307ad8, 1511},
        {"cb309bb0", 0x00309bb0, sandmouse_glow, 0x00309c88, 1818},
        {"cb2ae260", 0x002ae260, olt_cb_2ae260_l14, 0x002ac618, 8},
        {"cb2b3a20", 0x002b3a20, class28_draw_spot, 0x002b17d8, 28},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B221_CLS") != nullptr && std::strcmp(std::getenv("B221_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L14:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            // cb2fd6c0 / cb311d50: their GIF packet words carry a callee-saved register (s7 / s?) the callback reads on a path that never set it (the original's interpreter holds the
            // harness' leftover there, the port's local 0): the packets (the cursor's buffer) are not compared for these two, everything else is
            const size_t ign_pk = g_ignored_words.size();
            if (std::strcmp(cb.name, "cb2fd6c0") == 0 || std::strcmp(cb.name, "cb311d50") == 0)
                for (uint32_t a = 0x00800000u; a < 0x00810000u; a += 4) g_ignored_words.push_back(a);
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : olt_class8_update_l14, false); w32(OV(0x001611c0), 0x00800000u); }, 12, subj);  // (the display packet cursor: a real buffer for the draw callbacks)
            }
            g_ignored_words.resize(ign_pk);
        }
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 14's sound emitter types 1 / 3 / 4 / 6 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 14) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter1", 0x00316990, emitter_fade_box_update}, {"emitter3", 0x00316d30, emitter_reverb_box_update}, {"emitter4", 0x00316e90, emitter_open_bed_update}, {"emitter6", 0x00317118, emitter_music_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L14:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B221_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l14check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 14's class hooks (class 252's table at 0x1fd9d0): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 14) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_2dad60", 0x2dad60, 0x0, hook_uniform<enemy10e_hook0>},
            {"hook_2dadb0", 0x2dadb0, 0x4, hook_uniform<enemy10e_hook4>},
            {"hook_2dae00", 0x2dae00, 0x8, hook_uniform<olt_hook_2dae00>},
            {"hook_2dae50", 0x2dae50, 0xc, hook_uniform<class252_hook_c>},
            {"hook_2daed8", 0x2daed8, 0x10, hook_dispatched<0x2daed8>},  // (Y1: the level's dispatch, L6:0x2ea800's port)
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L14:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B221_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l14check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- moby_spawn (the Veldin port) against level 14's own copy (L14:0x00256d38): the classes' spawns all go through it
    if (g_oracle_level == 14) {
        l14check("moby_spawn", MOBY_SPAWN_L14, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x8, 0x1c, 0x51, 0xfc, 0x387, 0x58e, 0x71a};  // (classes of level 14's own table: 8, 28, 81, 252, 903, 1422 (none: a missing class), 1818)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 14's camera hook (0x3131e0, the pointer its setter 0x3131a8 puts at 0x167710) and the callback it queues (0x313240)
    if (g_oracle_level == 14) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x003131e0u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l14check("cam0_hook", 0x003131e0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l14check("cam_callback", 0x00313240, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 14's copy L14:0x00205a58) on the level's own mobys: class 794 (the Gadgebot's helper) calls it on level 11; the level's own classes call it
    if (g_oracle_level == 14) {
        l14check("mobys_in_sphere", 0x00205a58, 600, Ret::Int32, [level] {
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

    // ---- level 14's camera types 0x13 / 0x14 / 0x15 (accept / init / update / exit: section 4 of the overlay)
    // (the segment record's word Veldin 0x173e9c, written by segment_collide 0x1efc70's sqc2 under hero_place: -0 or +0 by the VU0 register earlier checks left, as in batch 124)
    if (g_oracle_level == 14) g_ignored_words.push_back(OV(0x00173e9c));
    if (g_oracle_level == 14) {
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
        static const CamT cams[] = {{19, 0x00314d48, 0x003142a8, 0x00314668, 0x00314e10}, {20, 0x00315920, 0x00314f00, 0x00315290, 0x003159d0}, {21, 0x00316748, 0x003159d8, 0x00315d48, 0x003167e0}};
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
            l14check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? (type == 19 ? static_cast<uint32_t>(cam_type19_accept(a.i[0], a.i[1])) : (type == 20 ? olt_cam_type20_accept_u : olt_cam_type21_accept_u)(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0) : 0u); });
            l14check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 19 ? cam_type19_init : type == 20 ? olt_cam_type20_init_l14 : olt_cam_type21_init_l14)(a.i[0]); return uint64_t{0}; });
            l14check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 19 ? cam_type19_update : type == 20 ? olt_cam_type20_update_l14 : olt_cam_type21_update_l14)(a.i[0]); return uint64_t{0}; });
            l14check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) (type == 19 ? empty_update : type == 20 ? empty_update : olt_cam_type21_exit_l14)(a.i[0]); return uint64_t{0}; });
        }
    }


    // ---- level 14's class 270 hook +0xc (L14:0x2dd1a0, the table 0x1fd9e8): Veldin's 0x2caf10 (enemy10e_hookc), the same code
    if (g_oracle_level == 14) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        level();
        std::vector<uint32_t> subj;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
        // (no chicken on the level after the first frame: the level's live mobys stand in, the hook only touches its argument and calls the generic body)
        l14check("hook_2dd1a0", 0x002dd1a0, 40, Ret::Int32, [level, subj] {
            level();
            const uint32_t m = subj[static_cast<size_t>(ri(0, static_cast<int>(subj.size()) - 1))];
            return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, 0u}, {rf(-1, 3)}};
        }, [](A a) { return r32(static_cast<uint32_t>(enemy10e_hookc(a.i[0], a.i[1], a.i[2], a.f[0]))); });
        g_extra_noop_hooks.clear();
    }

    // ---- level 14's cutscene extras (L14:0x2460f8: in scene 0, frames 0x1c..0x21, the scene's second moby's effect 0x245e98 and its draw
    // 0x2459c0 queued) and that draw callback: the scene state (Veldin's 0x16c860) given scene 0 and a frame, a live moby of the level as the scene's moby
    if (g_oracle_level == 14) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        const size_t ign_before = g_ignored_words.size();
        for (uint32_t a = 0x100; a < 0x1500; a += 4) g_ignored_words.push_back(a);
        level();
        std::vector<uint32_t> live;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) live.push_back(m);
        auto scene_prep = [level, live] {
            level();
            const uint32_t SC = OV(0x0016c860);  // (the scene state: L14:0x16cf60)
            w32(SC + 0x30, ri(0, 4) == 0 ? u(ri(1, 9)) : 0u);
            w32(SC + 0x34, u(ri(0x18, 0x24)));
            w32(SC + 0x17c, ri(0, 6) == 0 ? 0u : live[static_cast<size_t>(ri(0, static_cast<int>(live.size()) - 1))]);
            w32(OV(0x001611c0), 0x00800000u);
            w32(OV(0x0015f5cc), R());
            seed_rand();
            return ee_read<uint32_t>(SC + 0x17c);
        };
        l14check("cutscene_level_extras", 0x002460f8, 120, Ret::None, [scene_prep] { scene_prep(); return Args{}; },
                 [](A) { cutscene_level_extras(); return uint64_t{0}; });  // (Y1: Veldin's port, its level 14 branch)
        l14check("cb2459c0", 0x002459c0, 60, Ret::None, [scene_prep, live] { uint32_t m = scene_prep(); if (m == 0) m = live[0]; return Args{{m}, {}}; },
                 [](A a) { cutscene_l14_effect_draw(a.i[0]); return uint64_t{0}; });
        g_ignored_words.resize(ign_before);
        g_extra_noop_hooks.clear();
    }

    // ---- level 14's movement-mode hero update (L14:0x002197e8, the Novalis family's 0x3a8-byte build): the hero in a movement mode (PLAYER+0x20a4 = 1 / 2 / 3) in one of the states of the mode
    if (g_oracle_level == 14) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        g_ignored_words.push_back(OV(0x0017431c));  // (a float whose zero's sign depends on a VU0 register left behind)
        g_ignored_words.push_back(OV(0x00173e9c));  // (the same: the segment record's word is -0 or +0 by the VU0 vf register the earlier checks left)
        l14check("hero_move_mode", 0x002197e8, 300, Ret::None, [level] {
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

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 14) {
        static const uint32_t empties[] = {0x002e7190, 0x002f3ef0, 0x002fcba8, 0x00306150, 0x00308350, 0x00314e10, 0x003159d0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l14check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- P14.1: the checks of classes 903 (the lift), 8 (the searchlight drones), 30 (the pop-up turrets) and 31 (the
    // bombers) through every state and helper (oltanis_lift.cpp, oltanis_searchlight.cpp, oltanis_popup_turret.cpp,
    // oltanis_bomber.cpp), with the frame scale at 50 Hz in a third of the cases; without the batch's ignored w lanes of
    // the pvar area (the spawned shots', bombs' and wings' pvars are compared whole); the particle pool's +0x2c is still
    // left out (the explosions' and smoke's particles: the stale w lane of the shared spawns, batch-wide note above)
    if (g_oracle_level == 14) {
        const std::vector<uint32_t> batch_ignored = g_ignored_words;
        g_ignored_words = saved_ignored;
        {
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
        if (std::getenv("B221_INFO") != nullptr) {  // the play-test spots: each placed moby of the four classes (index, uid, place)
            level();
            const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
            static const struct { const char* name; uint32_t fn; } cls[] = {{"lift 903", 0x002aba80}, {"searchlight 8", 0x002ac618},
                                                                            {"turret 30", 0x002b3bf0}, {"bomber 31", 0x002b46e8}};
            for (const auto& c : cls)
                for (const uint32_t x : mobys_with(c.fn)) {
                    const Vec4 q = ee_ref<Vec4>(x + 0x10);
                    std::printf("INFO P14.1 %s: moby %u uid %d at %.1f %.1f %.1f state %u\n", c.name, (x - mpool) >> 8,
                                ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(x + 0x20));
                }
        }
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        auto set_50hz = [] {
            wf(0x0015ed60u, 1.2f);
            wf(0x0015ed64u, 1.44f);
            wf(0x0015ed68u, 1.2f);
            wf(0x0015ed6cu, 0.02f);
            wf(0x0015ed70u, 0.0004f);
        };
        // the camera behind the moby along the view's forward row (0x166fd0), 8 to 60 units back (within the floor band's 40
        // or not), the screen and projection set up (the snapshot's display leaves them broken) and the view from it, so that
        // the drones' on-screen tests pass (batch 510's way)
        auto look_at_moby = [](uint32_t m) {
            if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
            screen_setup();
            projection_setup();
            const Vec4 f = ee_ref<Vec4>(OV(0x00166fd0));
            const Vec4 p = ee_ref<Vec4>(m + 0x10);
            const float d = ri(0, 3) == 0 ? rf(45.0f, 60.0f) : rf(8.0f, 30.0f);
            Vec4 cam = ee_ref<Vec4>(OV(0x00166dc0));
            cam.x = p.x - d * f.x; cam.y = p.y - d * f.y; cam.z = p.z - d * f.z;
            ee_ref<Vec4>(OV(0x00166dc0)) = cam;
            render_view_setup();
        };

        // the hero standing on moby m (moby_hero_on_l1's two ways: the ground moby +0x2fc, or +0x4f8 in hero state 3)
        auto hero_on = [](uint32_t m) {
            const uint32_t P = 0x0013f350;
            w32(P + 0x2fc, m);
            w32(P + 0x4f8, m);
            w16(P + 0x30e, 0);
        };
        // a sound channel (0x13e550, 0x70 each: +0x88 its moby, +0x74 playing) in the pvar word `slot`: the moby's own and
        // playing, someone else's, or none
        auto sound_slot = [](uint32_t m, uint32_t slot) {
            if (ri(0, 3) == 0) { w32(slot, 0xffffffffu); return; }
            const int ch = ri(0, 7);
            w32(slot, u(ch));
            w32(0x0013e550u + u(ch * 0x70) + 0x88, ri(0, 3) == 0 ? 0u : m);
            w8(0x0013e550u + u(ch * 0x70) + 0x74, ri(0, 3) == 0 ? 0u : 1u);
        };
        // the hit record of give_hit naming a source (+0x20): none, a moby of the subject's own class (ignored by the turret's
        // and the bomber's hit tests) or the hero
        auto hit_source = [](uint32_t m) {
            if (ee_read<uint8_t>(m + 0xa4) == 0xff) return;
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            const int k = ri(0, 2);
            w32(e + 0x20, k == 0 ? 0u : k == 1 ? m : ee_read<uint32_t>(0x001413d0));
            // (the reactions: the record's kind +0x28 and strength +0x29 read when +0x1c holds the "applied" mark)
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
        };

        // class 903, the lift: every sub-state, the hide and activation boxes (hero in, the byte table, the save bit), the hero
        // riding it, the pause under it, the sound channel, the speed clamp, the trip's end, 50 Hz
        classcheck("class903_cov", 0x002aba80, olt_class903_update_l14, [set_50hz, hero_on, sound_slot](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int mode = ri(0, 10);
            const uint32_t boxes = ee_read<uint32_t>(OV(0x001600ec));
            // (a lift the snapshot has not updated yet: its state 0 first, by the port, as in class31_cov)
            if (mode >= 4 && ee_read<uint8_t>(m + 0x20) == 0 && ee_read<int32_t>(pv + 0xb4) != -1) olt_class903_update_l14(m);
            if (mode == 0) {
                w32(pv + 0xb4, 0xffffffffu);  // no path -> early return
            } else if (mode == 1) {
                w32(pv + 0xcc, 0u);  // hide box: hero outside or inside
                if (coin()) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(boxes + 0x30);
            } else if (mode == 2) {
                w32(pv + 0xc0, 0u);  // activation box: hero inside, the level's byte by uid, the save bit by uid, or none
                const int16_t uid = static_cast<int16_t>(ee_read<uint16_t>(m + 0xb2));
                const int k = ri(0, 3);
                if (k == 0) ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(boxes + 0x30);
                else if (k == 1) w8(OV(0x001bb230) + static_cast<uint32_t>(static_cast<int32_t>(uid)) + 0x454u, 1);
                else if (k == 2) {
                    const uint32_t w = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84) << 8) + (static_cast<uint32_t>(uid >> 5) << 2);
                    w32(w, ee_read<uint32_t>(w) | (1u << (static_cast<uint32_t>(uid) & 0x1f)));
                }
                w8(m + 0xbc, u(ri(0, 2)));
            } else if (mode == 3) {
                w8(m + 0x20, 0);
                w8(pv + 0xa0, coin() ? 0u : 1u);
            } else if (mode == 4 || mode == 5) {
                w8(m + 0x20, 1);
                w8(m + 0xbc, mode == 4 ? 0u : 1u);
                wf(pv + 0xa4, mode == 4 ? 1.0f : 0.0f);
                w16(pv + 0xb8, coin() ? 1u : 0u);
                const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0xb4) * 4u);
                const uint32_t pt = coin() ? (path + 0x10u) : (path + (ee_read<uint32_t>(path) << 4));
                ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(pt);
                if (coin()) hero_on(m);
            } else if (mode == 10) {
                w8(m + 0x20, 1);  // a sub-state the game never sets (3): the lift only springs to its end
                w8(m + 0xbc, 3);
            } else {
                w8(m + 0x20, 1);
                w8(m + 0xbc, 2);
                w32(pv + 0xc8, coin() ? 0u : 1u);
                w16(pv + 0xba, ri(0, 4) == 0 ? 5u : 0u);
                wf(pv + 0xac, coin() ? 0.05f : -0.05f);
                static const float vs[] = {0.02f, -0.02f, 0.2f, -0.2f};
                wf(pv + 0xa8, vs[ri(0, 3)]);
                static const float ts[] = {0.01f, 0.5f, 0.99f};
                wf(pv + 0xa4, ts[ri(0, 2)]);
                if (coin()) hero_on(m);
                sound_slot(m, pv + 0xc4);
                if (mode >= 8) {  // the pause: the hero under a descending lift (beside it, too far below, just under it)
                    wf(pv + 0xbc, -0.2f);
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    static const float dz[] = {2.0f, 0.5f, 6.0f, 3.0f};
                    q.z -= dz[ri(0, 3)];
                    if (ri(0, 3) == 0) q.x += 3.0f;
                    ee_ref<Vec4>(0x0013f3d0) = q;
                }
            }
            seed_rand();
        }, 160);

        // class 8: security searchlight drone (all states 0..9, on-screen view, turret group wake/retract, crash, 50 Hz)
        classcheck("class8_cov", 0x002ac618, olt_class8_update_l14, [set_50hz, look_at_moby, give_hit, hit_source, sound_slot, mobys_with](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            int st = ri(0, 11);
            // (a master of the beams (+0x1b6 = 1) is never in a drone's states: its paths are not measured)
            if (ee_read<int16_t>(pv + 0x1b6) == 1 && st != 10 && st != 11) st = 8;
            if (st == 10) {
                w8(pv + 0x2e, 2);  // retire branch
                return;
            }
            if (st == 11) {
                w8(m + 0x20, 0);
                w16(pv + 0x1b6, 1);  // master searchlight controller init -> state 8
                if (ri(0, 3) == 0) {  // (a group with no list: its count 0)
                    for (int32_t g = 0; g < 112; ++g)
                        if (lvd::group(g) == 0) { w8(m + 0x21, u(g)); break; }
                }
                return;
            }
            if (st == 8) {
                w8(m + 0x20, 8);
                wf(pv + 0x164, coin() ? 0.2f : 0.7f);  // alpha table pulse
                return;
            }
            // (states 1 .. 9 on the snapshot's drones, whose first frame ran their state 0: running it again here would link
            // their joint nodes (sub_0024f590) a second time, and the chain's evaluation then never ends, on both sides)
            if (st == 0) wf(pv + 0x210, coin() ? 0.0f : 0.25f);
            w8(m + 0x20, u(st));
            // the group's turrets (class 30) in states the drone's tests tell apart: down (1), up (3), going down (5), done
            // (+0x8b), woken (+0x88)
            if (coin()) {
                uint32_t grp = lvd::group(ee_read<int32_t>(pv + 0x70));
                const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                for (; grp != 0; grp += 2) {
                    const uint16_t raw = ee_read<uint16_t>(grp);
                    const uint32_t tm = pool + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
                    static const uint8_t sts[] = {1, 3, 3, 5};
                    w8(tm + 0x20, sts[ri(0, 3)]);
                    w8(ee_read<uint32_t>(tm + 0x78) + 0x8b, coin() ? 1u : 0u);
                    w16(ee_read<uint32_t>(tm + 0x78) + 0x88, ri(0, 3) == 0 ? 1u : 0u);
                    if (static_cast<int16_t>(raw) < 0) break;
                }
            }
            if (st == 1 && ri(0, 2) != 0) {  // the hero in the beam (the drone above the hero's moby, its spot on him), or a turret
                const std::vector<uint32_t> t30 = mobys_with(0x002b3bf0);
                const uint32_t who = ri(0, 3) == 0 ? t30[static_cast<size_t>(ri(0, static_cast<int>(t30.size()) - 1))] : ee_read<uint32_t>(0x001413d0);
                const Vec4 h = ee_ref<Vec4>(who + 0x10);
                ee_ref<Vec4>(pv + 0x170) = h;
                Vec4 up = h;
                up.z += rf(4.0f, 9.0f);
                ee_ref<Vec4>(m + 0x10) = up;
                ee_ref<Vec4>(pv + 0xd0) = up;
            }
            if (ri(0, 3) != 0) look_at_moby(m);
            if (coin()) {
                w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                const uint8_t an = coin() ? 1u : 2u;
                w8(m + 0x52, an);
                w8(m + 0x53, an);
            }
            if (ri(0, 2) != 0) {
                give_hit(m, 0x330000u);
                hit_source(m);
            }
            w8(m + 0xbc, coin() ? 1u : 0u);  // (the hit sparks of the tail: animation 1 playing, +0xbc set)
            if (ri(0, 3) == 0) { w8(m + 0x70, ee_read<uint8_t>(m + 0x70) & ~2u); w8(m + 0x53, 1); }
            sound_slot(m, pv + 0x1d0);
            if (st == 1) {
                w16(pv + 0x1b4, coin() ? 1u : 5u);
                if (coin()) {  // a drone whose light does not loop round its path, at the path's end
                    w16(pv + 0x8a, 0);
                    const uint32_t sp = lvd::path(ee_read<int32_t>(pv + 0x78));
                    const int32_t last = ee_read<int32_t>(sp) - 2;
                    if (last >= 0) {
                        w32(pv + 0x84, u(last));
                        wf(pv + 0x1cc, ee_read<float>(sp + 0x1cu + (u(last) << 4)) - 0.01f);
                        wf(pv + 0x168, std::fabs(ee_read<float>(pv + 0x168)));
                    }
                }
            } else if (st == 3) {  // the target: none, the hero's moby, or another moby gone (its state byte negative)
                const int k = ri(0, 3);
                if (k == 0) w32(pv + 0x18c, 0u);
                else if (k == 1) {  // the hero's moby raised in RAM (not in the collision grid): no ground within 4 under it
                    const uint32_t hm = ee_read<uint32_t>(0x001413d0);
                    w32(pv + 0x18c, hm);
                    wf(hm + 0x18, ee_read<float>(hm + 0x18) + 40.0f);
                } else if (k == 3) {
                    const std::vector<uint32_t> t30 = mobys_with(0x002b3bf0);
                    const uint32_t t = t30[static_cast<size_t>(ri(0, static_cast<int>(t30.size()) - 1))];
                    w32(pv + 0x18c, t);
                    w8(t + 0x20, 0xfd);
                } else w32(pv + 0x18c, ee_read<uint32_t>(0x001413d0));
                w16(pv + 0x1f2, coin() ? 1u : 10u);
            } else if (st == 4) {
                wf(pv + 0x1bc, coin() ? 0.3f : 0.0f);
                wf(pv + 0x1c0, coin() ? -0.2f : 0.0f);
                w16(pv + 0x208, coin() ? 1u : 0u);
                // the group's turrets up and done (class30_group_try_retract then sends them down and the drone to state 5)
                if (coin()) {
                    uint32_t grp = ee_read<uint32_t>(OV(0x001ab840) + ee_read<uint32_t>(pv + 0x70) * 4u);
                    const uint32_t pool = ee_read<uint32_t>(OV(0x0015ffd8));
                    if (grp != 0) {
                        for (;; grp += 2) {
                            const uint16_t raw = ee_read<uint16_t>(grp);
                            const uint32_t tm = pool + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
                            w8(tm + 0x20, 3);
                            w8(ee_read<uint32_t>(tm + 0x78) + 0x8b, 1);
                            if (static_cast<int16_t>(raw) < 0) break;
                        }
                    }
                }
            } else if (st == 5) {
                w16(pv + 0x208, coin() ? 1u : 0u);
            } else if (st == 9) {
                w16(pv + 0x1f0, ri(0, 3) == 0 ? 1u : 20u);
                wf(pv + 0x1e0, 0.4f); wf(pv + 0x1e4, -0.4f); wf(pv + 0x1e8, 0.2f);
                if (ri(0, 4) == 0) wf(m + 0x10, 1.0f);  // out-of-bounds retire
            }
            seed_rand();
        }, 160);

        // class 30, the pop-up turret: every state, the shot fired (class30_spawn_shot), the hit's source, the target in the box, 50 Hz
        classcheck("class30_cov", 0x002b3bf0, olt_class30_update_l14, [set_50hz, give_hit, hit_source](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 6);
            // (the snapshot's turrets have not been updated yet: their state 0 first, by the port, as in class31_cov)
            if (st != 0 && ee_read<uint8_t>(m + 0x20) == 0) olt_class30_update_l14(m);
            w8(m + 0x20, u(st));
            if (ri(0, 1) == 0) {
                give_hit(m, 0x330000u);
                hit_source(m);
            }
            // the target (target_select_any: the hero's moby) in the turret's trigger box, or the hero far
            const bool in_box = ri(0, 2) != 0;
            if (ri(0, 5) == 0) w32(0x0013f350u + 0x208c, 0x18);  // (the hero hidden: target_select_any finds nothing)
            if (in_box && ee_read<int32_t>(pv + 0x90) >= 0) {
                const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + ee_read<uint32_t>(pv + 0x90) * 0x80u;
                ee_ref<Vec4>(0x0013f3d0) = ee_ref<Vec4>(box + 0x30);
                ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = ee_ref<Vec4>(box + 0x30);
            }
            if (coin()) {
                w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                w8(m + 0x52, u(ri(0, 4)));
            }
            if (st == 0) {
                w16(pv + 0x88, coin() ? 1u : 0u);
                const int k = ri(0, 5);
                if (k == 0) w32(pv + 0x80, 0xffffffffu);
                else if (k == 1) w32(pv + 0x90, 0xffffffffu);
            } else if (st == 1) {
                w16(pv + 0x88, coin() ? 1u : 0u);
                w32(pv + 0x98, coin() ? 1u : 5u);
            } else if (st == 2 || st == 5) {
                w16(pv + 0x74, coin() ? 1u : 15u);
                wf(pv + 0x78, 0.05f);
                if (coin()) {
                    w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                    w8(m + 0x52, st == 2 ? 1u : 4u);
                }
            } else if (st == 3) {
                w8(pv + 0x8a, coin() ? 0u : 1u);
                w16(pv + 0x76, coin() ? 1u : 9u);
            } else if (st == 4) {
                w16(pv + 0x9e, coin() ? 1u : 0u);
                w16(pv + 0x9c, coin() ? 0u : 2u);
                w16(pv + 0x74, ri(0, 3) != 0 ? 1u : 9u);
                w32(OV(0x0015f5cc), coin() ? 0u : 1u);  // the frame counter: a shot fired (class30_spawn_shot) on 0
                if (coin()) {
                    w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                    w8(m + 0x52, 3u);
                }
            }
            seed_rand();
        }, 240);

        // class 31, the bomber: every state; the target found (the hero inside the zone, the polygon of path +0xd4) or not; the
        // target near (the crossing); the paths' ends; the mortar fired (its animation at the crossing of frame 3); the
        // camera near (the floor band); a bad path in each of the four words; the sound channel; hits with sources; 50 Hz
        auto zone_hero = [](uint32_t pv) {  // a point inside the zone polygon (its centroid, else halfway to a vertex), the hero there
            const uint32_t zone = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0xd4) * 4u);
            const int32_t n = ee_read<int32_t>(zone);
            if (n <= 0) return;
            Vec4 c{0.0f, 0.0f, 0.0f, 1.0f};
            for (int32_t i = 0; i < n; ++i) {
                const Vec4 q = ee_ref<Vec4>(zone + 0x10u + u(i) * 0x10u);
                c.x += q.x / static_cast<float>(n); c.y += q.y / static_cast<float>(n); c.z += q.z / static_cast<float>(n);
            }
            const Vec4 keep = ee_ref<Vec4>(0x0013f3d0);
            for (int32_t i = -1; i < n; ++i) {
                Vec4 p = c;
                if (i >= 0) {
                    const Vec4 q = ee_ref<Vec4>(zone + 0x10u + u(i) * 0x10u);
                    p.x = (p.x + q.x) * 0.5f; p.y = (p.y + q.y) * 0.5f;
                }
                ee_ref<Vec4>(0x0013f3d0) = p;
                if (point_in_polygon(0x0013f3d0, zone + 0x10u, n) != 0) return;
            }
            ee_ref<Vec4>(0x0013f3d0) = keep;
        };
        classcheck("class31_cov", 0x002b46e8, olt_class31_update_l14, [set_50hz, give_hit, hit_source, zone_hero, sound_slot](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 9);
            // (the snapshot's bombers have not been updated yet (state 0: too far from the hero): their state 0 is run first, by
            // the port, as the game would before any other state; never twice: see class8_cov)
            if (st != 0 && ee_read<uint8_t>(m + 0x20) == 0) olt_class31_update_l14(m);
            w8(m + 0x20, u(st));
            if (coin()) {
                w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                const uint8_t an = u(ri(0, 2));
                w8(m + 0x52, an);
                w8(m + 0x53, coin() ? an : u(ri(0, 2)));
            }
            if (ri(0, 2) == 0) {
                give_hit(m, 0x330000u);
                hit_source(m);
            }
            if (ri(0, 3) != 0) zone_hero(pv);
            w8(m + 0x31, coin() ? 1u : 0u);
            if (ri(0, 2) == 0) {  // the camera near the bomber (the floor band) or far
                Vec4 c = ee_ref<Vec4>(m + 0x10);
                c.x += coin() ? 10.0f : 60.0f;
                ee_ref<Vec4>(OV(0x00166dc0)) = c;
            }
            if (st >= 2) sound_slot(m, pv + 0x108);
            if (st == 1) w32(pv + 0x10c, coin() ? 0u : 1u);
            if (st >= 4 && st <= 8 && ri(0, 3) != 0) {  // the hover at the station as state 2's arrival sets it (or 7's crossing)
                const bool cross = coin();
                wf(pv + 0x170, ee_read<float>(L14_ADDR(0x00161574) + (cross ? 0x18u : 0u)));
                wf(pv + 0x174, ee_read<float>(L14_ADDR(0x00161578)));
                wf(pv + 0x178, ee_read<float>(L14_ADDR(0x0016157c)));
                wf(pv + 0x154, ee_read<float>(L14_ADDR(0x00161580) + (cross ? 0x10u : 0u)));
                wf(pv + 0x158, ee_read<float>(L14_ADDR(0x00161584)));
                wf(pv + 0x15c, ee_read<float>(L14_ADDR(0x00161588)));
                Vec4 at = ee_ref<Vec4>(m + 0x10);
                at.x += rf(-3.0f, 3.0f); at.y += rf(-3.0f, 3.0f); at.z += rf(-2.0f, 2.0f);
                ee_ref<Vec4>(pv + 0x180) = at;
                wf(pv + 0x17c, rf(-3.1f, 3.1f));
            }
            if (st == 0) {
                static const uint32_t words[] = {0xd0, 0xd4, 0xd8, 0x150};
                if (ri(0, 2) == 0) w32(pv + words[ri(0, 3)], 0xffffffffu);  // a bad path -> retire
            } else if (st == 2 || st == 3) {
                w32(pv + 0x10c, coin() ? 0u : 1u);
                const uint32_t pidx = ee_read<uint32_t>(pv + (ee_read<uint32_t>(pv + 0x10c) ? 0x150u : 0xd0u));
                const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + pidx * 4u);
                if (st == 2 && coin()) {  // at the entry path's end
                    const int32_t last = ee_read<int32_t>(path) - 2;
                    w32(pv + 0xe8, u(last));
                    wf(pv + 0xec, ee_read<float>(path + 0x1cu + (u(last) << 4)));
                } else if (st == 3 && coin()) {  // back at its start
                    w32(pv + 0xe8, 0u);
                    wf(pv + 0xec, 0.0f);
                }
            } else if (st == 4) {
                w32(pv + 0x10c, coin() ? 0u : 1u);
                w16(pv + 0x102, coin() ? 1u : 9u);
                if (coin()) {  // the target (the hero's moby) near: the crossing
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x += 1.0f;
                    ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10u) = q;
                }
            } else if (st == 5) {
                w16(pv + 0x102, coin() ? 1u : 9u);
                const uint32_t drop = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0xd8) * 4u);
                w16(pv + 0x100, u(ri(0, ee_read<int32_t>(drop) - 1)));
                w8(m + 0x53, ri(0, 3) == 0 ? 2u : 0u);
            } else if (st == 6) {  // the mortar: the animation's time just past frame 3 (anim_crossed(m, 3.0)) in 3 cases of 4
                const uint32_t drop = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0xd8) * 4u);
                const int32_t n = ee_read<int32_t>(drop);
                w16(pv + 0x100, coin() ? 0u : u(ri(0, n - 1)));
                const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
                if (cur != 0 && ri(0, 3) != 0) {
                    const float frame = static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f;
                    w8(m + 0x53, u(ee_read<uint8_t>(m + 0x52) ^ 1u));
                    wf(m + 0x54, 3.0f - frame + rf(0.05f, 0.4f));
                    wf(m + 0x58, 1.0f);
                    wf(m + 0x5c, 0.5f);
                }
                wf(pv + 0x104, rf(-1.0f, 1.0f));
            } else if (st == 7) {
                w32(pv + 0x10c, coin() ? 0u : 1u);
                const uint32_t cp = ee_read<uint32_t>(OV(0x001b04b0) + ee_read<uint32_t>(pv + 0xdc) * 4u);
                if (ee_read<uint32_t>(pv + 0x10c) == 0 && coin()) {  // at the crossing path's far end, either way
                    const int32_t last = ee_read<int32_t>(cp) - 2;
                    w32(pv + 0xe8, u(last));
                    wf(pv + 0xec, ee_read<float>(cp + 0x1cu + (u(last) << 4)));
                } else if (ee_read<uint32_t>(pv + 0x10c) != 0 && coin()) {
                    w32(pv + 0xe8, 0u);
                    wf(pv + 0xec, 0.0f);
                }
            }
            seed_rand();
        }, 300);

        // ---- P14.2: classes 81 (the bombers' wings), 211 (the rail drone), 309 (the tilting platform), 386 (the electric
        // arc), 557 (the flying craft), 610 (the wind), 643 (the floating mine), 681 (the turrets' shot), 684 (the arrival
        // scene and its lightning) and 685 (the gondola) through every state and helper (oltanis_wing.cpp ...
        // oltanis_gondola.cpp), in the same setting as P14.1's checks (no pvar-area w-lane ignore, 50 Hz in a third of the
        // cases); the wings and the shots are spawned through their spawners' ports (a bomber shot down, a turret firing)
        // and aged by their own updates (the port) before the frame compared; class 684 never reaches its cutscene's start
        // (the scene seen, or a scene already running: the original would read the scene's stream).
        if (std::getenv("B221_INFO") != nullptr) {  // the play-test spots of P14.2's classes (index, uid, place, state)
            level();
            const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
            static const struct { const char* name; uint32_t fn; } cls2[] = {
                {"drone 211", 0x002d5f40}, {"tilting platform 309", 0x002de1f8}, {"arc 386", 0x002dee28}, {"craft 557", 0x002e7bd8},
                {"wind 610", 0x002eaf88}, {"mine 643", 0x002ec810}, {"lightning 684", 0x002ed280}, {"gondola 685", 0x002ee8d8}};
            for (const auto& c : cls2)
                for (const uint32_t x : mobys_with(c.fn)) {
                    const Vec4 q = ee_ref<Vec4>(x + 0x10);
                    const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                    std::printf("INFO P14.2 %s: moby %u uid %d at %.1f %.1f %.1f state %u pv+0x00 %d +0x60 %d +0x88 %d +0xa0 %d +0xb8 %d "
                                "+0x114 %d +0x118 %d +0x11c %d +0x120 %d +0x150 %d +0x1cc %d +0x1f4 %d +0x200 %d +0x21c %d +0x500 %d +0x504 %d\n",
                                c.name, (x - mpool) >> 8, ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(x + 0x20),
                                ee_read<int32_t>(xp), ee_read<int32_t>(xp + 0x60), ee_read<int32_t>(xp + 0x88), ee_read<int32_t>(xp + 0xa0),
                                ee_read<int32_t>(xp + 0xb8), ee_read<int32_t>(xp + 0x114), ee_read<int32_t>(xp + 0x118),
                                ee_read<int32_t>(xp + 0x11c), ee_read<int32_t>(xp + 0x120), ee_read<int32_t>(xp + 0x150),
                                ee_read<int32_t>(xp + 0x1cc), ee_read<int32_t>(xp + 0x1f4), ee_read<int32_t>(xp + 0x200),
                                ee_read<int32_t>(xp + 0x21c), ee_read<int32_t>(xp + 0x500), ee_read<int32_t>(xp + 0x504));
                }
        }
        auto alive = [](uint32_t x) { return ee_read<uint8_t>(x + 0x20) < 0xfe; };
        // the hero (his place and his moby's) at p
        auto hero_at = [](const Vec4& p) {
            ee_ref<Vec4>(0x0013f3d0) = p;
            ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = p;
        };
        auto box_centre = [](int32_t i) { return ee_ref<Vec4>(lvd::box(lvd::BOXES, i) + 0x30); };
        // the camera behind a point (look_at_moby's way, for points of a path)
        auto look_at_point = [](const Vec4& p) {
            if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
            screen_setup();
            projection_setup();
            const Vec4 f = ee_ref<Vec4>(OV(0x00166fd0));
            const float d = rf(8.0f, 40.0f);
            Vec4 cam = ee_ref<Vec4>(OV(0x00166dc0));
            cam.x = p.x - d * f.x; cam.y = p.y - d * f.y; cam.z = p.z - d * f.z;
            ee_ref<Vec4>(OV(0x00166dc0)) = cam;
            render_view_setup();
        };
        // a moby the generator makes (spawned and aged by the ports): as classcheck, the state tried on the port first
        auto spawncheck = [level, redeals](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<uint32_t()> make, int n) {
            if (g_only_batch != 0 && g_only_batch != g_batch) return;
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
            l14check(name, fn, n, Ret::None, [level, make, port, redeals] {
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 40; ++attempt) {
                    level();
                    m = make();
                    if (m == 0) { ++redeals(); continue; }
                    uint8_t* ram = ee_get_ram_base();
                    saved.assign(ram, ram + 0x02000000);
                    const uint32_t stack_top = ee_stack_ptr();
                    const bool ok = run_trial(port, m);
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ram, saved.data(), saved.size());
                    if (ok) break;
                    ++redeals();
                }
                return Args{{m}, {}};
            }, [port](A a) { port(a.i[0]); return uint64_t{0}; });
            if (std::getenv("B221_REDEALS") != nullptr) std::printf("REDEALS %s: %d\n", name, redeals());
            redeals() = 0;
        };
        // a port run in a generator (aging, state 0 first): a throw ends it, the stack put back
        auto run_port = [](void (*port)(uint32_t), uint32_t x) {
            const uint32_t sp0 = ee_stack_ptr();
            try { port(x); return true; } catch (const std::exception&) { ee_stack_ptr() = sp0; return false; }
        };

        // class 81, a bomber's wing: a bomber shot down (its state 9, the port) sheds both; one aged 0 .. 80 frames, then
        // near the hero, at the ground, its timer out, out of the level, fast, its light new; 50 Hz
        spawncheck("p142_class81", 0x002bac78, olt_class81_update_l14, [set_50hz, mobys_with, alive, run_port]() -> uint32_t {
            if (ri(0, 2) == 0) set_50hz();
            const std::vector<uint32_t> bombers = mobys_with(0x002b46e8);
            const uint32_t b = bombers[static_cast<size_t>(ri(0, static_cast<int>(bombers.size()) - 1))];
            if (ee_read<uint8_t>(b + 0x20) == 0 && !run_port(olt_class31_update_l14, b)) return 0u;
            w8(b + 0x20, 9);
            seed_rand();
            if (!run_port(olt_class31_update_l14, b)) return 0u;
            const std::vector<uint32_t> wings = mobys_with(0x002bac78);
            if (wings.empty()) return 0u;
            const uint32_t w = wings[static_cast<size_t>(ri(0, static_cast<int>(wings.size()) - 1))];
            const uint32_t pv = ee_read<uint32_t>(w + 0x78);
            Vec4 far = ee_ref<Vec4>(w + 0x10);
            far.x += 60.0f; far.y += 60.0f;
            ee_ref<Vec4>(0x0013f3d0) = far;
            const int age = ri(0, 80);
            for (int k = 0; k < age && alive(w); ++k) { seed_rand(); if (!run_port(olt_class81_update_l14, w)) return 0u; }
            if (!alive(w)) return 0u;
            const int mode = ri(0, 8);
            if (mode == 8) {  // the hero within 10 along x only
                Vec4 h = ee_ref<Vec4>(w + 0x10);
                h.x += rf(-9.0f, 9.0f); h.y += coin() ? rf(11.0f, 20.0f) : -rf(11.0f, 20.0f);
                ee_ref<Vec4>(0x0013f3d0) = h;
            } else if (mode == 1) {  // the hero within 10 across the ground, the wing above him
                Vec4 h = ee_ref<Vec4>(w + 0x10);
                h.x += rf(-9.0f, 9.0f); h.y += rf(-9.0f, 9.0f); h.z -= rf(3.0f, 8.0f);
                ee_ref<Vec4>(0x0013f3d0) = h;
            } else if (mode == 2) {  // at the ground (the hero's feet), the hero away: the sphere meets the world
                const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10);
                Vec4 g = h;
                g.x += rf(3.0f, 6.0f); g.z += rf(0.2f, 0.6f);
                ee_ref<Vec4>(w + 0x10) = g;
            } else if (mode == 3) {
                w16(pv + 0x10, u(ri(0, 1)));  // the timer out (or idle)
            } else if (mode == 4) {  // out of the level's cube
                static const float edge[] = {1.5f, 1021.5f};
                wf(w + 0x10 + 4u * u(ri(0, 2)), edge[ri(0, 1)]);
            } else if (mode == 5) {  // fast, rising: the drag's branches
                wf(pv + 0x0, ee_read<float>(pv + 0x0) * rf(4.0f, 9.0f));
                wf(pv + 0x4, ee_read<float>(pv + 0x4) * rf(4.0f, 9.0f));
                wf(pv + 0x8, rf(0.05f, 0.4f));
            } else if (mode == 6) {
                w16(pv + 0x12, 0xffffu);  // no light yet: light_add
            }
            seed_rand();
            return w;
        }, 240);

        // class 681, the turrets' shot: a turret firing (its state 4 on the frame counter 0, the port) spawns it; aged 0 .. 25
        // frames, then flying on, down onto the ground, into the hero, through a class 252 moby, at its range's end, out
        // of the level, with no owner and the other segment flags; visible or not; 50 Hz
        // (the segment record's words 0x173e90 .. 0x173ebc, written by segment_collide's VU0 leftovers: the shared port's known
        // difference, harmless (batches 514 / 520 leave them out too), met when the shot's second test starts from a class 252
        // moby it went through: left out of this check only)
        const size_t ign_681 = g_ignored_words.size();
        for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        spawncheck("p142_class681", 0x002ece00, olt_class681_update_l14, [set_50hz, mobys_with, alive, run_port]() -> uint32_t {
            if (ri(0, 2) == 0) set_50hz();
            const std::vector<uint32_t> turrets = mobys_with(0x002b3bf0);
            const uint32_t t = turrets[static_cast<size_t>(ri(0, static_cast<int>(turrets.size()) - 1))];
            const uint32_t tpv = ee_read<uint32_t>(t + 0x78);
            if (ee_read<uint8_t>(t + 0x20) == 0 && !run_port(olt_class30_update_l14, t)) return 0u;
            const std::vector<uint32_t> before = mobys_with(0x002ece00);
            w8(t + 0x20, 4);
            w16(tpv + 0x9c, 0);
            w32(OV(0x0015f5cc), 0);
            seed_rand();
            if (!run_port(olt_class30_update_l14, t)) return 0u;
            uint32_t s = 0;
            for (const uint32_t x : mobys_with(0x002ece00))
                if (std::find(before.begin(), before.end(), x) == before.end()) s = x;
            if (s == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(s + 0x78);
            const int age = ri(0, 25);
            for (int k = 0; k < age && alive(s); ++k) { seed_rand(); if (!run_port(olt_class681_update_l14, s)) return 0u; }
            if (!alive(s)) return 0u;
            const int mode = ri(0, 7);
            const float a = rf(-3.1f, 3.1f);
            const Vec4 d{std::cos(a), std::sin(a), 0.0f, 0.0f};
            if (mode == 1) {  // down onto the ground a little way from the hero
                Vec4 p = ee_ref<Vec4>(0x0013f3d0);
                p.x += 4.0f * d.x; p.y += 4.0f * d.y; p.z += rf(0.4f, 1.5f);
                ee_ref<Vec4>(s + 0x10) = p;
                ee_ref<Vec4>(pv) = Vec4{rf(-0.1f, 0.1f), rf(-0.1f, 0.1f), -rf(0.6f, 2.5f), 0.0f};
            } else if (mode == 2) {  // into the hero's moby
                const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10);
                const float sp = rf(0.8f, 2.0f);
                ee_ref<Vec4>(s + 0x10) = Vec4{h.x - 1.2f * d.x, h.y - 1.2f * d.y, h.z + rf(0.3f, 1.2f), 1.0f};
                ee_ref<Vec4>(pv) = Vec4{sp * d.x, sp * d.y, 0.0f, 0.0f};
            } else if (mode == 3) {  // through a class 252 moby (or two): it passes on
                const std::vector<uint32_t> c252 = mobys_with(0x002d98f0);
                if (!c252.empty()) {
                    const Vec4 c = ee_ref<Vec4>(c252[static_cast<size_t>(ri(0, static_cast<int>(c252.size()) - 1))] + 0x10);
                    const float sp = rf(1.5f, 4.0f);
                    ee_ref<Vec4>(s + 0x10) = Vec4{c.x - rf(0.5f, 1.5f) * d.x, c.y - rf(0.5f, 1.5f) * d.y, c.z + rf(0.0f, 1.5f), 1.0f};
                    ee_ref<Vec4>(pv) = Vec4{sp * d.x, sp * d.y, rf(-0.2f, 0.2f), 0.0f};
                }
            } else if (mode == 4) {  // at its range's end
                const Vec4 p = ee_ref<Vec4>(s + 0x10);
                const Vec4 v = ee_ref<Vec4>(pv);
                const float l = std::sqrt(v.x * v.x + v.y * v.y);
                if (l > 0.0f) ee_ref<Vec4>(pv + 0x10) = Vec4{p.x - v.x / l * rf(28.5f, 30.5f), p.y - v.y / l * rf(28.5f, 30.5f), p.z, 1.0f};
            } else if (mode == 5) {  // (out of the level's cube, or just inside its edges)
                static const float edge[] = {1.5f, 1021.5f, 2.5f, 1020.5f};
                wf(s + 0x10 + 4u * u(ri(0, 2)), edge[ri(0, 3)]);
            } else if (mode == 6) {
                w32(pv + 0x20, 0);
                w32(pv + 0x24, 0);
            }
            if (mode >= 1 && mode <= 3) ee_ref<Vec4>(pv + 0x10) = ee_ref<Vec4>(s + 0x10);  // (its start moved along: within range)
            w8(s + 0x31, coin() ? 1u : 0u);
            seed_rand();
            return s;
        }, 280);
        g_ignored_words.resize(ign_681);

        // class 211, the rail drone: every state (its state 0 by the port first when the snapshot has not run it: run
        // twice it would link its joint nodes again); the camera type 20 live or not; the timers at their edges; the
        // unfolding's frame; the chase's beam sub-states and timers, its light; the end box; 50 Hz
        // (the same segment record words: the beam's segment_collide in sub-state 3, the shared port's known difference)
        for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        classcheck("p142_class211", 0x002d5f40, olt_class211_update_l14, [set_50hz, look_at_moby, run_port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 7);
            if (st == 0) {  // the set-up's checks (on a drone the snapshot has not run)
                seed_rand();
                if (ee_read<uint8_t>(m + 0x20) != 0) { w8(m + 0x20, 2); return; }
                const int k = ri(0, 5);
                if (k == 0) w32(pv + 0x60, 0xffffffffu);
                else if (k == 1) w32(pv + 0x88, 0xffffffffu);
                else if (k == 2) w32(pv + 0x1cc, 0xffffffffu);
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class211_update_l14, m)) return;
            const int s = st == 7 ? 6 : st;
            w8(m + 0x20, u(s));
            if (s == 1) {
                if (coin()) w16(ee_read<uint32_t>(L14_ADDR(0x00167500)) + 0x86, 0x14);
            } else if (s == 2 || s == 4) {
                w32(pv + 0x6c, ri(0, 3) == 0 ? 0u : coin() ? 1u : u(ri(2, 40)));  // (idle, running out, running)
                if (s == 4) wf(pv + 0x70, rf(0.01f, 0.1f));
            } else if (s == 3) {
                w32(pv + 0x6c, u(ri(1, 40)));
                wf(pv + 0x70, rf(0.01f, 0.1f));
                w8(m + 0x70, coin() ? (ee_read<uint8_t>(m + 0x70) | 2u) : (ee_read<uint8_t>(m + 0x70) & ~2u));
                if (coin()) w8(m + 0x53, ee_read<uint8_t>(m + 0x52));
                const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
                if (cur != 0 && coin()) {  // the unfolding's frame past 3 (moby_anim_time: the frame plus +0x54 while blending)
                    const float frame = static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f;
                    w8(m + 0x53, u(ee_read<uint8_t>(m + 0x52) ^ 1u));
                    wf(m + 0x54, (coin() ? 3.0f : 1.0f) - frame + rf(0.05f, 1.5f));
                }
            } else if (s == 5) {
                w32(pv + 0x64, u(ri(0, 3)));
                wf(pv + 0x68, rf(0.0f, 2.0f));
            } else if (s == 6) {
                const int sub = ri(0, 5);
                w8(m + 0xbc, u(sub));
                w32(pv + 0x1c0, ri(0, 2) == 0 ? 1u : u(ri(2, 60)));
                w32(pv + 0x1c4, u(ri(0, 34)));
                w32(pv + 0x1c8, coin() ? 1u : u(ri(2, 30)));
                if (coin()) w32(pv + 0xf0, 0xffffffffu);
                {  // (before the climb's segments, on them, or past them: the lag's growth and its clamp)
                    const int w = ri(0, 2);
                    w32(pv + 0x64, w == 0 ? u(ri(0, 8)) : w == 1 ? u(ee_read<int16_t>(pv + 0x8e) + ri(0, 4)) : u(ee_read<int16_t>(pv + 0x8c) + ri(0, 3)));
                }
                wf(pv + 0x68, rf(0.0f, 1.0f));
                if (ri(0, 2) == 0) w32(pv + 0xf0, u(ri(0, 3)));  // (a light to free)
                if (ri(0, 3) == 0) {  // at the end box: it blows up
                    ee_ref<Vec4>(m + 0x10) = Vec4{0, 0, 0, 1};
                    const int32_t b = ee_read<int32_t>(pv + 0x1cc);
                    if (b >= 0) { Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, b) + 0x30); c.w = 1.0f; ee_ref<Vec4>(m + 0x10) = c; }
                }
                if (ri(0, 2) == 0) look_at_moby(m);
                Vec4 h = ee_ref<Vec4>(m + 0x10);
                h.x += rf(-10.0f, 10.0f); h.y += rf(-10.0f, 10.0f);
                ee_ref<Vec4>(0x0013f3d0) = h;
            }
            if (coin() && (s >= 3 && s <= 5)) w32(pv + 0xf0, coin() ? 0xffffffffu : 0u);
            seed_rand();
        }, 240);
        g_ignored_words.resize(ign_681);

        // class 309, the tilting platform: the hero on it (either way) or not, at its centre or off it; the creak's timer;
        // its up row far from the goal; the set-up; 50 Hz
        classcheck("p142_class309", 0x002de1f8, olt_class309_update_l14, [set_50hz](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            if (ri(0, 6) == 0) { w8(m + 0x20, 0); seed_rand(); return; }
            w8(m + 0x20, 1);
            const int on = ri(0, 3);
            w32(P + 0x2fc, on == 1 ? m : 0u);
            w16(P + 0x30e, ri(0, 3) == 0 ? 1u : 0u);
            w32(P + 0x4f8, on == 2 ? m : 0u);
            w32(P + 0x208c, on == 2 && coin() ? 3u : u(ri(0, 2)));
            Vec4 h = ee_ref<Vec4>(m + 0x10);
            if (ri(0, 3) != 0) { h.x += rf(-3.0f, 3.0f); h.y += rf(-3.0f, 3.0f); h.z += rf(0.0f, 1.5f); }
            ee_ref<Vec4>(0x0013f3d0) = h;
            w32(pv + 0x70, u(ri(0, 3)));
            if (coin()) wfrand(m + 0xe0, 3, -1.0f, 1.0f);
            if (coin()) wfrand(pv + 0x60, 3, -0.05f, 0.05f);
            seed_rand();
        }, 200);

        // class 386, the electric arc: the set-up's checks; the hero grinding one of its rails, near, far; in its box
        // (shocked); its buzz's channel; the camera on it (the strings move and are queued); a state past 1; 50 Hz
        classcheck("p142_class386", 0x002dee28, olt_class386_update_l14, [set_50hz, look_at_moby, sound_slot, hero_at, box_centre](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            const int k = ri(0, 9);
            if (k == 0) {
                w8(m + 0x20, 0);
                static const uint32_t words[] = {0x1f4, 0x1f8, 0x1fc, 0x200};
                if (coin()) w32(pv + words[ri(0, 3)], 0xffffffffu);
                seed_rand();
                return;
            }
            w8(m + 0x20, k == 9 ? u(ri(2, 5)) : 1u);
            Vec4 h = ee_ref<Vec4>(m + 0x10);
            const int where = ri(0, 3);
            if (where == 0) { h.x += rf(-15.0f, 15.0f); h.y += rf(-15.0f, 15.0f); }
            else if (where == 1) {  // (beyond 16 across x, or just inside / outside 16 along either axis)
                if (coin()) { h.x += coin() ? rf(17.0f, 40.0f) : -rf(17.0f, 40.0f); h.y += rf(-30.0f, 30.0f); }
                else { const float e = rf(15.0f, 17.0f); if (coin()) h.x += coin() ? e : -e; else h.y += coin() ? e : -e; }
            }
            else if (where == 2 && ee_read<int32_t>(pv + 0x200) >= 0) h = box_centre(ee_read<int32_t>(pv + 0x200));
            else h.x += 60.0f;
            hero_at(h);
            if (ri(0, 2) == 0) {  // grinding one of its rails (or another's)
                w32(P + 0x208c, 0xf);
                static const uint32_t rails[] = {0x1f4, 0x1f8, 0x1fc};
                const int32_t r = ee_read<int32_t>(pv + rails[ri(0, 2)]);
                w32(P + 0x560, ri(0, 4) == 0 || r < 0 ? 0u : ee_read<uint32_t>(lvd::rail(r) + 0x10));
            } else if (coin()) {
                w32(P + 0x208c, u(ri(0, 3)));
            }
            sound_slot(m, pv + 0x204);
            if (ri(0, 3) != 0) look_at_moby(m);
            seed_rand();
        }, 240);

        // class 557, the flying craft: the hero in its hide box; hidden or shown again (its platform too); the set-up;
        // the platform held at its chain or its place; the hum's channel; the hero aboard (hero state 0xd on the platform),
        // getting off, in the air above it; the ease's timer; 50 Hz
        classcheck("p142_class557", 0x002e7bd8, olt_class557_update_l14, [set_50hz, sound_slot, hero_at, box_centre](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            if (ri(0, 5) == 0) {
                w8(m + 0x20, 0);
                if (ri(0, 3) == 0) w32(pv + 0x150, 0xffffffffu);
            } else {
                if (ee_read<uint8_t>(m + 0x20) == 0) olt_class557_update_l14(m);  // (the snapshot's crafts: their state 0 first)
                w8(m + 0x20, 1);
            }
            const int32_t hide = ee_read<int32_t>(pv + 0x21c);
            const bool hidden = ri(0, 3) == 0;
            if (hidden) {  // the hero in its hide box (box 0 for a craft without one)
                if (hide < 0) w32(pv + 0x21c, 0);
                hero_at(box_centre(hide < 0 ? 0 : hide));
            } else if (ri(0, 4) == 0) {
                w32(pv + 0x21c, 0xffffffffu);
            }
            if (coin()) w32(m + 0x94, 0);
            if (coin()) {
                w32(pv + 0x158, 0x3039);
                w32(pv + 0x154, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 1)));
                wfrand(pv + 0x160, 3, -2.0f, 2.0f);
            }
            if (coin()) w32(pv + 0x15c, 0xffffffffu); else sound_slot(m, pv + 0x15c);
            const int32_t di = ee_read<int32_t>(pv + 0x150);
            const uint32_t deck = di >= 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + (u(di) << 8) : 0u;
            const int ride = ri(0, 3);
            w32(P + 0x208c, ride == 0 || ri(0, 3) == 0 ? 0xdu : u(ri(0, 3)));
            w32(P + 0x964, ride == 0 || coin() ? deck : 0u);
            w16(pv + 0x204, ri(0, 5) == 0 ? 2u : u(ri(0, 1)));
            w16(P + 0x30e, coin() ? 1u : 0u);
            if (deck != 0 && !hidden) {
                Vec4 h = ee_ref<Vec4>(deck + 0x10);
                h.x += rf(-2.0f, 2.0f); h.y += rf(-2.0f, 2.0f);
                const int band = ri(0, 2);  // (below the craft's 3, just above it, higher)
                h.z = ee_read<float>(m + 0x18) + (band == 0 ? rf(-6.0f, -3.1f) : band == 1 ? rf(-2.95f, -2.05f) : rf(-2.0f, 3.0f));
                ee_ref<Vec4>(0x0013f3d0) = h;
                if (ri(0, 3) == 0) { w16(pv + 0x204, 1); w16(P + 0x30e, 1); w32(P + 0x208c, 0); }  // (off the platform, in the air: the height test)
            }
            w16(pv + 0x20c, u(ri(0, 30)));
            wf(pv + 0x210, rf(0.02f, 0.2f));
            seed_rand();
        }, 240);

        // class 610, the wind: the set-up (its checks, the motes laid out); the hero in its outer box or not, in the air,
        // swinging, on the ground, in the blowing box; the push's timers; the motes' timers at their edges and fading; the
        // camera among the motes facing the wind (the clamp); 50 Hz
        classcheck("p142_class610", 0x002eaf88, olt_class610_update_l14, [set_50hz, hero_at, box_centre](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            if (ri(0, 6) == 0) {
                w8(m + 0x20, 0);
                static const uint32_t words[] = {0x0, 0x504, 0x500};
                if (ri(0, 2) == 0) w32(pv + words[ri(0, 2)], 0xffffffffu);
                seed_rand();
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0) olt_class610_update_l14(m);  // (the motes laid out as the game does)
            w8(m + 0x20, 1);
            const int where = ri(0, 3);
            const int32_t outer = ee_read<int32_t>(pv + 0x500), blow = ee_read<int32_t>(pv + 0x0), motes = ee_read<int32_t>(pv + 0x504);
            if (where == 0 && blow >= 0) hero_at(box_centre(blow));
            else if (where == 1 && outer >= 0) hero_at(box_centre(outer));
            else if (where == 2 && motes >= 0) hero_at(box_centre(motes));
            w16(P + 0x308, ri(0, 3) == 0 ? 1u : 0u);
            w32(P + 0x2284, ri(0, 5) == 0 ? 0xdu : 0u);
            w8(P + 0x12e2, ri(0, 3) == 0 ? 1u : 0u);
            w16(P + 0x30c, coin() ? 1u : 0u);
            w16(pv + 0xe, coin() ? 1u : 0u);
            w16(pv + 0xc, ri(0, 3) == 0 ? 0u : u(ri(1, 30)));
            wf(pv + 0x10, rf(0.02f, 0.5f));
            w16(pv + 0x50a, u(ri(0, 40)));
            for (int k = 0; k < 48; ++k) {  // the motes' timers: spent, fading in, fading out
                const uint32_t h2 = pv + u(k) * 2u;
                const int life = ri(20, 120);
                w16(h2 + 0x380, u(life));
                w16(h2 + 0x320, ri(0, 5) == 0 ? 1u : u(ri(2, life)));
            }
            if (ri(0, 2) != 0 && motes >= 0) {  // the camera among the motes, facing into the wind
                const uint32_t mote = pv + 0x20u + u(ri(0, 47)) * 0x10u;
                Vec4 c = ee_ref<Vec4>(mote);
                c.x += rf(-6.0f, 6.0f); c.y += rf(-6.0f, 6.0f);
                ee_ref<Vec4>(L14_ADDR(0x001674c0)) = c;
                wf(L14_ADDR(0x001674d8), ee_read<float>(m + 0x48) + (coin() ? 3.1f : rf(-1.0f, 1.0f)));
            }
            seed_rand();
        }, 240);

        // class 643, the floating mine: the set-up (no path, an empty path, the measure); hits of each strength from a mine
        // (class 0x283) or the hero; the hero touching it; the pulse; 50 Hz
        classcheck("p142_class643", 0x002ec810, olt_class643_update_l14, [set_50hz, give_hit, hit_source](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            if (ri(0, 5) == 0) {
                seed_rand();
                if (ee_read<uint8_t>(m + 0x20) != 0) { w8(m + 0x20, 1); return; }
                if (ri(0, 3) == 0) w32(pv + 0x60, 0xffffffffu);
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0) olt_class643_update_l14(m);
            w8(m + 0x20, ri(0, 6) == 0 ? 2u : 1u);
            if (ri(0, 2) != 0) {
                give_hit(m, 0x230000u);
                hit_source(m);
            }
            w32(0x0013f350u + 0x23c, ri(0, 4) == 0 ? m : 0u);
            if (coin()) wf(pv + 0x6c, coin() ? 1.0f : -1.0f);
            seed_rand();
        }, 240);

        // class 684, the arrival scene and its lightning: the set-up and state 1 without the cutscene's start (the scene
        // seen, or a scene running), the scene playing or over; a new strike with the camera on a point of its path; the
        // bolt laid out (the port) and then growing, bright, flashing, rumbling, smoking, over; another moby's bolt; 50 Hz
        classcheck("p142_class684", 0x002ed280, olt_class684_update_l14, [set_50hz, look_at_point, run_port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 9);
            if (st <= 1) {  // (0x13d3f0 is set in the snapshot; or 0 with a scene already running: the original returns first)
                w8(m + 0x20, u(st));
                if (coin()) { w8(0x0013d3f0, 0); w32(L14_ADDR(0x0015f5c4), u(ri(1, 2))); }
                seed_rand();
                return;
            }
            if (st == 2) {
                w8(m + 0x20, 2);
                w32(L14_ADDR(0x0015f5c4), coin() ? 2u : 0u);
                seed_rand();
                return;
            }
            w8(m + 0x20, st == 9 ? u(ri(4, 7)) : 3u);
            const int k = ri(0, 3);
            if (k == 0) {  // a new strike: the camera behind a point of the path
                w32(L14_ADDR(0x00161c58), 0);
                const uint32_t path = lvd::path(ee_read<int32_t>(pv + 0x0));
                const int32_t n = ee_read<int32_t>(path);
                if (n > 0 && ri(0, 3) != 0) look_at_point(ee_ref<Vec4>(path + 0x10u + u(ri(0, std::min(n, 5) - 1)) * 0x10u));
            } else {  // the bolt laid out by the port, then its timers
                w32(L14_ADDR(0x00161c58), 0);
                w32(L14_ADDR(0x00161c50), 0);
                seed_rand();
                if (!run_port(olt_class684_update_l14, m)) return;
                const int32_t life = ee_read<int32_t>(L14_ADDR(0x00161cec));
                w32(L14_ADDR(0x00161c58), 1);
                const int t = ri(0, 4);
                w32(L14_ADDR(0x00161cb4), t == 0 ? 1u : t == 1 ? u(std::max(life, 2)) : u(ri(1, std::max(life, 2))));
                if (coin()) w32(L14_ADDR(0x00161cec), u(ri(30, 140)));
                wf(L14_ADDR(0x00161c6c), coin() ? rf(0.0f, 6.0f) : rf(6.3f, 9.0f));
                wf(L14_ADDR(0x00161ce4), rf(-3.1f, 3.1f));  // (the flash's phase)
                w32(L14_ADDR(0x00161cf4), coin() ? 1u : 0u);
                if (ri(0, 5) == 0) w32(L14_ADDR(0x00161ce8), m + 0x100u);
                if (coin()) {  // the points' and branches' timers at the bolt's life (their clamp)
                    const int32_t top = ee_read<int32_t>(L14_ADDR(0x00161c70));
                    for (uint32_t k = 0; k < 40; ++k) if (coin()) w16(L14_ADDR(0x001e0000) + 2u * k, u(top + ri(-1, 3)));
                    for (uint32_t k = 0; k < 48; ++k) if (coin()) w16(L14_ADDR(0x001e0350) + 2u * k, u(top + ri(-1, 3)));
                }
            }
            w32(L14_ADDR(0x00161c50), u(ri(0, 4) == 0 ? 6 : ri(0, 1)));
            seed_rand();
        }, 300);

        // class 685, the gondola: the set-up (the ride done by the uid's byte, the save bit, the save's byte); waiting:
        // the hero in each of its boxes, aboard within 1 (or not, or off the ground), the arrival sound's volume; riding
        // each path either way, near its end; the branch's return; arriving with the springs moving or still; settled with
        // the hero aboard or off; the hum's channels; the pulse near and far; 50 Hz
        classcheck("p142_class685", 0x002ee8d8, olt_class685_update_l14, [set_50hz, sound_slot, hero_at, box_centre](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 5);
            if (st == 0) {
                w8(m + 0x20, 0);
                const int16_t uid = static_cast<int16_t>(ee_read<uint16_t>(m + 0xb2));
                const int k = ri(0, 3);
                if (k == 0) w8(OV(0x001bb230) + static_cast<uint32_t>(static_cast<int32_t>(uid)) + 0x454u, 1);
                else if (k == 1) {
                    const uint32_t w = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84) << 8) + (static_cast<uint32_t>(uid >> 5) << 2);
                    w32(w, ee_read<uint32_t>(w) | (1u << (static_cast<uint32_t>(uid) & 0x1f)));
                } else if (k == 2) {
                    w8(ee_read<uint32_t>(pv + 0x110) + (ee_read<uint32_t>(0x0015ed84) << 4) + 0x0014c050u, 0xff);
                }
                if (ri(0, 5) == 0) w32(pv + 0xa0, 0xffffffffu);  // (the set-up's retirements)
                else if (ri(0, 5) == 0) w32(pv + 0x114, 0xffffffffu);
                seed_rand();
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0) olt_class685_update_l14(m);
            w8(m + 0x20, u(st == 5 ? ri(1, 4) : st));
            const uint8_t now = ee_read<uint8_t>(m + 0x20);
            w8(m + 0xbc, u(ri(0, 4) == 0 ? 2 : ri(0, 1)));
            wf(pv + 0xac, coin() ? 1.0f : -1.0f);
            w16(pv + 0x124, coin() ? 1u : 0u);
            const bool aboard = ri(0, 2) != 0;
            w32(P + 0x2fc, aboard ? m : 0u);
            w16(P + 0x30c, ri(0, 4) == 0 ? 1u : 0u);
            Vec4 h = ee_ref<Vec4>(m + 0x10);
            if (now == 1) {
                const int where = ri(0, 4);
                static const uint32_t boxes[] = {0x118, 0x11c, 0x120};
                if (ri(0, 4) == 0) w32(pv + boxes[ri(0, 2)], 0xffffffffu);
                if (where < 3 && ee_read<int32_t>(pv + boxes[where]) >= 0) h = box_centre(ee_read<int32_t>(pv + boxes[where]));
                else { h.x += coin() ? rf(-0.9f, 0.9f) : rf(1.1f, 3.0f); h.y += rf(-0.6f, 0.6f); }
                if (ri(0, 3) == 0) h.x += coin() ? 45.0f : rf(39.0f, 41.0f);  // (the pulse's 40 across the ground)
                w32(pv + 0x134, coin() ? 0xffffffffu : u(ri(0, 0x400)));
                hero_at(h);
                if (aboard && ri(0, 2) != 0) {  // aboard at its centre (the boxes' place kept in the other cases), or near 1 off it
                    Vec4 c = ee_ref<Vec4>(m + 0x10);
                    c.x += coin() ? rf(-0.9f, 0.9f) : (coin() ? 1.0f : -1.0f) * rf(0.85f, 1.05f);
                    hero_at(c);
                }
            } else {
                h.x += rf(-0.5f, 0.5f);
                hero_at(h);
            }
            if (now == 2) {
                const uint32_t path = lvd::path(ee_read<int32_t>(pv + (ee_read<uint8_t>(m + 0xbc) == 0 ? 0xa0u : 0xb8u)));
                const int32_t n = ee_read<int32_t>(path);
                const bool fwd = ee_read<float>(pv + 0xac) > 0.0f;
                if (n >= 2) {
                    const int32_t seg = coin() ? (fwd ? n - 2 : 0) : ri(0, n - 2);
                    w32(pv + 0xa4, u(seg));
                    const float len = ee_read<float>(path + 0x1cu + u(seg) * 0x10u);
                    wf(pv + 0xa8, coin() ? len * rf(0.0f, 1.0f) : (fwd ? (coin() ? len : len - rf(0.0f, 0.01f)) : (coin() ? 0.0f : rf(0.0f, 0.01f))));  // (at the end it reaches)
                }
                {  // (the distance run: early, about the ride's middle (the pitch's turn), past the path's length (t clamped))
                    const float total = ee_read<float>(pv + (ee_read<uint8_t>(m + 0xbc) == 0 ? 0xb4u : 0xbcu));
                    const int w = ri(0, 2);
                    wf(pv + 0xb0, w == 0 ? rf(0.0f, 40.0f) : w == 1 ? total * rf(0.35f, 0.65f) : rf(40.0f, 600.0f));
                }
                sound_slot(m, pv + 0x12c);
                w32(pv + 0x130, coin() ? 0xffffffffu : u(ri(0, 0x400)));
            } else if (now == 3) {
                if (coin()) wfrand(pv + 0xc0, 3, -0.0004f, 0.0004f); else wfrand(pv + 0xc0, 3, -0.2f, 0.2f);
                sound_slot(m, pv + 0x12c);
                w16(pv + 0x126, u(ri(0, 0x400)));
                w32(pv + 0x130, u(ri(0, 0x400)));
                w32(pv + 0x134, u(ri(0, 0x400)));
            }
            if (coin()) wf(pv + 0x138, rf(-3.1f, 3.1f));
            if (now != 2 && ri(0, 3) == 0) {  // (the hero about 40 off across the ground: the pulse's edge)
                Vec4 f = ee_ref<Vec4>(m + 0x10);
                f.x += (coin() ? 1.0f : -1.0f) * rf(38.5f, 41.5f);
                f.y += rf(-5.0f, 5.0f);
                hero_at(f);
            }
            seed_rand();
        }, 300);

        // 1.02 fix 49 (fixes102_oltanis_gondola_upright, the PAL 2.00 disc's own test): the gondola waiting with the hero
        // aboard within 1; its up row against the hero's moby's either way. With the fix the ride starts only when they
        // agree: a flipped case must equal the port without the fix whose hero is off the ground (+0x30c set, put back),
        // an agreeing one the port without the fix. Run with RAC1_FIXES102=49 (ORACLE_ONLY=p142_class685_fix49).
        if (const char* only = std::getenv("ORACLE_ONLY"); only == nullptr || std::strcmp(only, "p142_class685_fix49") == 0) {
            const std::vector<uint32_t> gondolas = mobys_with(0x002ee8d8);
            if (std::getenv("RAC1_FIXES102") == nullptr || gondolas.empty()) {
                std::printf("%-26s skipped (run with RAC1_FIXES102=49 on level 14)\n", "p142_class685_fix49");
                ++g_skipped;
            } else {
                int bad = 0, n = 0, blocked = 0, started = 0;
                static std::vector<uint8_t> start, with_fix;
                const std::unordered_set<uint32_t> ignored(g_ignored_words.begin(), g_ignored_words.end());
                for (int c = 0; c < 120; ++c) {
                    level();
                    const uint32_t m = gondolas[static_cast<size_t>(ri(0, static_cast<int>(gondolas.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const uint32_t P = 0x0013f350;
                    if (ee_read<uint8_t>(m + 0x20) == 0) olt_class685_update_l14(m);
                    w8(m + 0x20, 1);
                    if (ri(0, 2) == 0) set_50hz();
                    w32(P + 0x2fc, m);
                    w16(P + 0x30c, 0);
                    Vec4 h = ee_ref<Vec4>(m + 0x10);
                    h.x += rf(-0.9f, 0.9f);
                    hero_at(h);
                    wf(pv + 0xac, coin() ? 1.0f : -1.0f);
                    const bool flip = coin();
                    const float up = rf(0.2f, 1.0f);
                    ee_ref<Vec4>(m + 0xe0) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), flip ? -up : up, 0.0f};
                    if (ri(0, 4) == 0) wf(m + 0xe8, 0.0f);  // (a dot of 0: not positive)
                    seed_rand();
                    uint8_t* ram = ee_get_ram_base();
                    start.assign(ram, ram + 0x02000000);
                    const float dot = vec3_dot(ee_ref<Vec4>(m + 0xe0), ee_ref<Vec4>(ee_read<uint32_t>(P + 0x2080) + 0xe0));
                    g_advanced_disabled = false;
                    olt_class685_update_l14(m);
                    g_advanced_disabled = true;
                    with_fix.assign(ram, ram + 0x02000000);
                    const bool went = ee_read<uint8_t>(m + 0x20) == 2;
                    std::memcpy(ram, start.data(), start.size());
                    const bool block = !(0.0f < dot);
                    if (block) w16(P + 0x30c, 1);
                    olt_class685_update_l14(m);
                    if (block) { w16(P + 0x30c, 0); ++blocked; } else if (went) ++started;
                    ++n;
                    for (uint32_t a = 0x00100000; a < 0x02000000; a += 4) {
                        if (ignored.count(a) != 0) continue;
                        if (a >= ee_stack_ptr() - 0x10000 && a < ee_stack_ptr() + 0x10000) continue;
                        uint32_t xw, yw;
                        std::memcpy(&xw, &with_fix[a], 4); std::memcpy(&yw, &ram[a], 4);
                        if (xw != yw) { if (bad < 8) std::printf("  p142_class685_fix49 case %d: %08x fix=%08x expected=%08x\n", c, a, xw, yw); ++bad; break; }
                    }
                }
                std::printf("%-26s %s (%d cases, %d blocked, %d started)\n", "p142_class685_fix49",
                            bad == 0 && blocked > 0 && started > 0 ? "ok" : "FAIL", n, blocked, started);
                if (bad != 0 || blocked == 0 || started == 0) ++g_fail;
            }
        }
        // ---- P14.3: classes 712 (the raft), 851 and 924 (the talkers), 908, 921 and 922 (the flying craft and the rocket),
        // 923 (the carrier of class 933) and 1193 (the bombers' mortar bomb) through every state and helper (oltanis_raft.cpp,
        // oltanis_talkers.cpp, oltanis_crafts.cpp, oltanis_carrier.cpp, oltanis_mortar_bomb.cpp), in the same setting as P14.1's
        // checks; the bombs are spawned through the bombers' port (state 6, the mortar's frame) and aged by their own port
        if (std::getenv("B221_INFO") != nullptr) {  // the play-test spots of P14.3's classes (index, uid, place, state, pvars)
            level();
            const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
            static const struct { const char* name; uint32_t fn; } cls3[] = {
                {"raft 712", 0x002f0538}, {"talker 851", 0x002fba20}, {"craft 908", 0x002fc0f0}, {"carrier craft 921", 0x002fcbb0},
                {"rocket 922", 0x002fdbb8}, {"carrier 923", 0x002fe2a0}, {"talker 924", 0x002fefe0}};
            for (const auto& c : cls3)
                for (const uint32_t x : mobys_with(c.fn)) {
                    const Vec4 q = ee_ref<Vec4>(x + 0x10);
                    const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                    std::printf("INFO P14.3 %s: moby %u uid %d at %.1f %.1f %.1f state %u", c.name, (x - mpool) >> 8,
                                ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(x + 0x20));
                    for (uint32_t o = 0; o < 0x110; o += 4) std::printf(" %x:%x", o, ee_read<uint32_t>(xp + o));
                    std::printf("\n");
                }
            for (const uint32_t fn : {0x002fba20u, 0x002fefe0u})  // the talkers' scripts (their set-up run): each record's scene, next, kind, flags
                for (const uint32_t x : mobys_with(fn)) {
                    if (ee_read<uint8_t>(x + 0x20) == 0) (fn == 0x002fba20u ? olt_class851_update_l14 : olt_class924_update_l14)(x);
                    const uint32_t xp = ee_read<uint32_t>(x + 0x78), sc = ee_read<uint32_t>(xp + 0x3c);
                    std::printf("INFO P14.3 talker 0x%x script 0x%x cur %d:", fn, sc, ee_read<int16_t>(xp + 0x36));
                    for (uint32_t r = 0; sc != 0 && r < 10; ++r)
                        std::printf(" [%u text %x scene %d next %d kind %d c %d e %d fl %x]", r, ee_read<uint32_t>(sc + r * 0x1c), ee_read<int16_t>(sc + r * 0x1c + 4),
                                    ee_read<int16_t>(sc + r * 0x1c + 6), ee_read<int16_t>(sc + r * 0x1c + 8), ee_read<int16_t>(sc + r * 0x1c + 0xc),
                                    ee_read<int16_t>(sc + r * 0x1c + 0xe), ee_read<uint16_t>(sc + r * 0x1c + 0x10));
                    std::printf("\n");
                }
            for (const uint32_t x : mobys_with(0x002fcbb0u))  // the carrier craft's path: its points
                std::printf("INFO P14.3 carrier craft 921 path %d points %d\n", ee_read<int32_t>(ee_read<uint32_t>(x + 0x78) + 0x68),
                            ee_read<int32_t>(lvd::path(ee_read<int32_t>(ee_read<uint32_t>(x + 0x78) + 0x68))));
        }

        // sound_slot's channel kept in a halfword (the raft's +0xb2)
        auto sound_slot16 = [](uint32_t m, uint32_t slot) {
            if (ri(0, 3) == 0) { w16(slot, 0xffffu); return; }
            const int ch = ri(0, 7);
            w16(slot, u(ch));
            w32(0x0013e550u + u(ch * 0x70) + 0x88, ri(0, 3) == 0 ? 0u : m);
            w8(0x0013e550u + u(ch * 0x70) + 0x74, ri(0, 3) == 0 ? 0u : 1u);
        };
        auto path_of = [](uint32_t pv, uint32_t word) { return lvd::path(ee_read<int32_t>(pv + word)); };
        // a moby's uid marked done: the level's byte (k 0) or the save's bit (k 1)
        auto uid_done = [](uint32_t m, int k) {
            const int16_t uid = static_cast<int16_t>(ee_read<uint16_t>(m + 0xb2));
            if (k == 0) {
                w8(OV(0x001bb230) + static_cast<uint32_t>(static_cast<int32_t>(uid)) + 0x454u, 1);
            } else {
                const uint32_t w = 0x0014c190u + (ee_read<uint32_t>(0x0015ed84) << 8) + (static_cast<uint32_t>(uid >> 5) << 2);
                w32(w, ee_read<uint32_t>(w) | (1u << (static_cast<uint32_t>(uid) & 0x1f)));
            }
        };
        auto slot_done = [](uint32_t idx) { w8(0x0014c050u + idx + (ee_read<uint32_t>(0x0015ed84) << 4), 0xff); };
        // a place along a path: its segment `seg` (clamped to the path) and a fraction of it
        auto along = [](uint32_t path, int32_t seg, float t) {
            const int32_t n = ee_read<int32_t>(path);
            if (seg > n - 2) seg = n - 2;
            if (seg < 0) seg = 0;
            const Vec4 a = ee_ref<Vec4>(path + 0x10u + (u(seg) << 4)), b = ee_ref<Vec4>(path + 0x20u + (u(seg) << 4));
            return Vec4{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f};
        };

        // class 712, the raft: every state; the hero aboard on his feet or not, within its deck or past it, the button and
        // the message holder (0x15f594: none, the raft's 4, another); the stages 0 .. 4 and outside the table; not aboard,
        // in each activation box during the level's first ten frames or later; the glides near their ends; the ride along
        // its path; the turn back; its slots done; its sound; 50 Hz
        classcheck("p143_class712", 0x002f0538, olt_class712_update_l14, [set_50hz, hero_at, box_centre, sound_slot16, run_port, path_of, slot_done](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            if (ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class712_update_l14, m)) return;  // (the set-up first, by the port)
            const int st0 = ri(0, 11);
            const int st = st0 >= 9 ? 1 : st0;  // (state 1 a third of the time: the deck, the boxes)
            w8(m + 0x20, u(st == 8 ? ri(8, 12) : st));
            const bool riding = st == 3 || st == 4 || st == 5 || st == 7;
            int16_t stage = static_cast<int16_t>(ri(0, 4));
            if (st == 1 && ri(0, 3) == 0) stage = static_cast<int16_t>(coin() ? -1 : ri(5, 7));  // (the message's default)
            w16(pv + 0xb4, u(stage));
            w16(pv + 0xb6, u(ri(0, 4)));
            {  // the path its ride takes (as raft_route sets it)
                static const uint32_t words[] = {0x94, 0x98, 0x9c, 0xa0};
                const uint32_t word = words[ri(0, 3)];
                if (ee_read<int32_t>(pv + word) >= 0) w32(pv + 0xac, path_of(pv, word));
            }
            if (riding || ri(0, 3) == 0) {  // the progress: early, near the end, at it
                const int k = ri(0, 2);
                wf(pv + 0xc0, k == 0 ? rf(0.0f, 0.9f) : k == 1 ? rf(0.97f, 0.9999f) : 1.0f);
                wf(pv + 0xc4, rf(0.0f, 0.03f));
            }
            const bool aboard = st0 >= 9 ? false : ri(0, 2) != 0;
            w32(P + 0x2fc, aboard ? m : 0u);
            w16(P + 0x30e, ri(0, 4) == 0 ? 1u : 0u);
            if (aboard) {  // on its deck, within 1.2 across or past it (or about 1.2 away in any direction: the edge)
                const Vec4 c = ee_ref<Vec4>(m + 0x10);
                const float a = rf(-1.5f, 1.5f), b = coin() ? rf(-1.1f, 1.1f) : (coin() ? 1.0f : -1.0f) * rf(1.3f, 2.5f);
                const float yaw = ee_read<float>(m + 0x48);
                if (ri(0, 2) == 0) {
                    const float r = coin() ? rf(1.15f, 1.35f) : rf(0.3f, 10.0f), d = rf(-3.1f, 3.1f);  // (the deck's frame may be scaled)
                    hero_at(Vec4{c.x + r * std::cos(d), c.y + r * std::sin(d), c.z + 0.6f, 1.0f});
                } else {
                    hero_at(Vec4{c.x + a * std::cos(yaw) - b * std::sin(yaw), c.y + a * std::sin(yaw) + b * std::cos(yaw), c.z + 0.6f, 1.0f});
                }
            } else if (ri(0, 3) != 0) {  // in one of its activation boxes (the stage each one tests, either way)
                static const uint32_t bw[] = {0x100, 0x104, 0x108};
                const int which = ri(0, 2);
                const int32_t b = ee_read<int32_t>(pv + bw[which]);
                if (b >= 0) hero_at(box_centre(b));
                if (st0 >= 9) w16(pv + 0xb4, u(which == 0 ? (coin() ? 2 : ri(0, 4)) : which == 1 ? (coin() ? 1 : ri(0, 4)) : ri(0, 3)));
                if (ri(0, 4) == 0) w32(pv + bw[ri(0, 2)], 0xffffffffu);
            }
            {  // (the level's first ten frames, or later; about the tenth often)
                const int k = ri(0, 3);
                w32(L14_ADDR(0x0015f5cc), k == 0 ? u(ri(0, 8)) : k == 1 ? u(ri(9, 12)) : u(ri(13, 5000)));
            }
            { const int k = ri(0, 3); w32(L14_ADDR(0x0015f594), k == 0 ? 0u : k == 1 ? 2u : 4u); }
            w32(0x0013cae4, coin() ? 0x10u : (R() & ~0x10u));
            if (st == 6) {  // its slots of things done
                const int k = ri(0, 3);
                if (k == 0) slot_done(ee_read<uint32_t>(pv + 0xb8));
                else if (k == 1) slot_done(ee_read<uint32_t>(pv + 0xbc));
            } else if (st == 7) {  // where it turned back from
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += rf(-4.0f, 4.0f); q.y += rf(-4.0f, 4.0f); q.z += rf(-1.0f, 1.0f);
                ee_ref<Vec4>(pv + 0xe0) = q;
                ee_ref<Vec4>(pv + 0xf0) = Vec4{rf(-0.2f, 0.2f), rf(-0.2f, 0.2f), rf(-3.1f, 3.1f), 0.0f};
            }
            sound_slot16(m, pv + 0xb2);
            seed_rand();
        }, 300);

        // classes 851 and 924, the talkers: the set-up's retirements (the uid's byte, the save bit, a missing index; 924's
        // story flag); talking with the hero in its box the first time or not, its checkpoint's slot taken or not, the talk
        // kept short of a scene (the game mode, the reach, the button); the scene's end with each choice; the head toward
        // the hero (near, ahead or behind, moving or still) or a wandering point, its timers at their edges; 50 Hz
        auto talker_gen = [set_50hz, hero_at, box_centre, run_port, uid_done, slot_done](uint32_t m, void (*port)(uint32_t), bool is924) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 4);
            if (st == 0) {
                w8(m + 0x20, 0);
                const int k = ri(0, 6);
                if (k <= 1) uid_done(m, k);
                else if (k == 2) w32(pv + 0x44, 0xffffffffu);
                else if (k == 3) w32(pv + 0x48, 0xffffffffu);
                else if (k == 4 && is924) w8(0x0013dd4f, 1);
                w32(OV(0x0015f5c4), 1);  // (the talk's set-up only)
                seed_rand();
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0 && !run_port(port, m)) return;
            w8(m + 0x20, u(st == 4 ? ri(4, 8) : st));
            const uint32_t boxw = is924 ? 0x44u : 0x48u;
            bool talked = false;
            if (st == 1) {
                const bool first = ri(0, 2) != 0;
                w8(pv + 8, first ? 1u : 0u);
                const bool in_box = coin();
                if (in_box && ee_read<int32_t>(pv + 0x40) >= 0) hero_at(box_centre(ee_read<int32_t>(pv + 0x40)));
                if (coin()) slot_done(ee_read<uint8_t>(m + 0xb0));
                if (ee_read<int32_t>(pv + boxw) < 0) w32(pv + boxw, 0u);
                // (the talk short of a record: another mode, or the button up and the hero away)
                w32(OV(0x0015f5c4), coin() ? 1u : 0u);
                if (ee_read<uint32_t>(OV(0x0015f5c4)) == 0) {
                    w32(0x0013cae4, 0);
                    w8(pv + 8, 0);
                }
                if (ri(0, 2) == 0) {  // the talk done: a record without a scene (851's 1 / 2, 924's 1) taken by the button, the
                                      // hero within reach facing the talker and it facing him
                    w16(pv + 0x36, is924 ? 1u : u(ri(1, 2)));
                    w8(pv + 8, 0);
                    w8(pv + 9, 1);
                    w32(OV(0x0015f5c4), 0);
                    const Vec4 c = ee_ref<Vec4>(m + 0x10);
                    const float yaw = ee_read<float>(m + 0x48) + rf(-0.5f, 0.5f), r = rf(0.5f, 1.5f);
                    hero_at(Vec4{c.x + r * std::cos(yaw), c.y + r * std::sin(yaw), c.z, 1.0f});
                    wf(P + 0x98, yaw + (yaw > 0.0f ? -3.14159265f : 3.14159265f));
                    w32(0x0013cae4, 0x10);
                    w32(OV(0x00179110), 0);
                    talked = true;
                }
            } else if (st == 2) {
                w32(OV(0x0015f5c4), coin() ? 2u : 0u);
                static const uint16_t ch[] = {3, 4, 0, 1, 2, 5, 0xffff};
                w16(pv + 4, ch[ri(0, 6)]);
            }
            if (!is924) {  // (851's floor band: on screen, the camera within 28 or not)
                w8(m + 0x31, coin() ? 1u : 0u);
                Vec4 c = ee_ref<Vec4>(m + 0x10);
                c.x += coin() ? rf(5.0f, 27.0f) : rf(29.0f, 60.0f);
                ee_ref<Vec4>(L14_ADDR(0x001674c0)) = c;
            }
            // the head
            if (ri(0, 3) == 0) w8(m + 0x53, u(ri(1, 2)));
            else w8(m + 0x53, 0);
            if (!talked && (st != 1 || ri(0, 2) == 0)) {
                Vec4 h = ee_ref<Vec4>(m + 0x10);
                const float yaw = ee_read<float>(m + 0x48) + (coin() ? rf(-1.4f, 1.4f) : rf(1.7f, 4.6f));
                const float r = coin() ? rf(1.0f, 7.9f) : rf(8.1f, 14.0f);
                h.x += r * std::cos(yaw); h.y += r * std::sin(yaw);
                ee_ref<Vec4>(P + 0x80) = h;
                ee_ref<Vec4>(P + 0xd0) = h;  // (its target)
            }
            ee_ref<Vec4>(P + 0x100) = coin() ? Vec4{0, 0, 0, 0} : Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 0.0f, 0.0f};
            const uint32_t follow = is924 ? 0x160u : 0x194u, wander = is924 ? 0x164u : 0x198u;
            { static const uint32_t ts[] = {0, 1, 2, 60}; w32(pv + follow, ts[ri(0, 3)]); w32(pv + wander, ts[ri(0, 3)]); }
            w8(0x0015edb0, coin() ? 1u : 0u);
            seed_rand();
        };
        {  // (the deals end in a level change request: run for real on both sides, as batch 514 runs the gold bolt's -- the
           // kernel's CD / movie calls no-ops, the console clock and the disc reads the host's, a fixed clock)
            const std::time_t saved_clock = g_host_clock_fixed;
            g_host_clock_fixed = 1759658400;
            const auto saved_hooks = g_extra_hooks;
            g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                             {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                             {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
            g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
            classcheck("p143_class851", 0x002fba20, olt_class851_update_l14, [talker_gen](uint32_t m) { talker_gen(m, olt_class851_update_l14, false); }, 300);
            classcheck("p143_class924", 0x002fefe0, olt_class924_update_l14, [talker_gen](uint32_t m) { talker_gen(m, olt_class924_update_l14, true); }, 300);
            g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
            g_extra_hooks = saved_hooks;
            g_host_clock_fixed = saved_clock;
        }

        // class 908, the flying craft: set up (its path missing); flying (off its path's point, either side of the 0.01 of the
        // turn); gone, its timer at its end; a hit of kind 0x800000 counting toward the third (0x15ee08, the flag 0x13d420
        // set or not; another level); on screen or not (its jets); its sound; 50 Hz
        classcheck("p143_class908", 0x002fc0f0, olt_class908_update_l14, [set_50hz, give_hit, sound_slot, along, path_of, run_port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 4);
            // (the snapshot's craft have not been updated (state 0): the set-up first, by the port -- it lays the path's
            // segments' lengths the walk along it needs)
            if (st != 0 && ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class908_update_l14, m)) return;
            w8(m + 0x20, u(st == 4 ? ri(3, 6) : st));
            if (st == 0) {
                if (ri(0, 2) == 0) w32(pv + 0x68, 0xffffffffu);
            } else if (st == 1) {
                const uint32_t path = path_of(pv, 0x68);
                const int32_t seg = ri(0, ee_read<int32_t>(path) - 2);  // (a segment of the path, a distance along it)
                w32(pv + 0x60, u(seg));
                const float t = rf(0.0f, 1.0f);
                wf(pv + 0x64, ee_read<float>(path + 0x1cu + (u(seg) << 4)) * t);
                Vec4 q = along(path, seg, t);
                const int k = ri(0, 3);
                if (k == 0) { q.x += rf(-3.0f, 3.0f); q.y += rf(-3.0f, 3.0f); q.z += rf(-2.0f, 2.0f); }
                else if (k == 1) { q.x += rf(-0.009f, 0.009f); }  // (the way within 0.01 on x)
                else if (k == 2) { q.y += rf(-0.009f, 0.009f); }
                ee_ref<Vec4>(m + 0x10) = q;
                wfrand(pv + 0x70, 3, -0.3f, 0.3f);
            } else if (st == 2) {
                w32(pv + 0x7c, coin() ? 1u : u(ri(2, 200)));
                wfrand(pv + 0x70, 3, -0.3f, 0.3f);  // (the springs and turns its return clears)
                wfrand(pv + 0x80, 3, -0.3f, 0.3f);
            }
            if (ri(0, 2) == 0) give_hit(m, 0x00800000u);
            w32(0x0015ee08, u(ri(0, 3)));
            w8(0x0013d420, ri(0, 3) == 0 ? 1u : 0u);
            if (ri(0, 7) == 0) w32(0x0015ed84, 13);
            w8(m + 0x31, coin() ? 1u : 0u);
            sound_slot(m, pv + 0x6c);
            seed_rand();
        }, 240);

        // class 921, the carrier craft: set up (done before with its cargo, a path or a slot missing, no cargo); hidden until
        // the slots are done and the hero in its box sees the cargo (the camera put to look at it, near the angles' edges);
        // flying with the cargo held, past its path's 56th point, at the path's end, the camera of type 19 or not; gone; a hit
        // while it carries nothing; on screen; its sound; 50 Hz
        // (the segment record's words 0x173e90 .. 0x173ebc, segment_collide's known VU0 leftovers: left out of the two checks
        // whose functions test lines against the world, 921's look at its cargo and 1193's way, and only of them)
        const size_t ign_p143 = g_ignored_words.size();
        for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        classcheck("p143_class921", 0x002fcbb0, olt_class921_update_l14, [set_50hz, give_hit, sound_slot, along, path_of, hero_at, box_centre, uid_done, slot_done, run_port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 4);
            if (st != 0 && ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class921_update_l14, m)) return;  // (the set-up first)
            w8(m + 0x20, u(st == 4 ? ri(4, 6) : st));
            if (st == 0) {
                const int k = ri(0, 8);
                if (k <= 1) {
                    uid_done(m, k);
                    const uint32_t mp = ee_read<uint32_t>(OV(0x0015ffd8));
                    const uint32_t p = mp + (ee_read<uint32_t>(pv + 0x8c) << 8);
                    const uint32_t q = mp + (ee_read<uint32_t>(ee_read<uint32_t>(p + 0x78) + 4) << 8);
                    const int d = ri(0, 3);  // (the cargo or its target already gone)
                    if (d == 1) w8(p + 0x20, 0xff);
                    else if (d == 2) w8(q + 0x20, 0xff);
                } else if (k == 2) w32(pv + 0x68, 0xffffffffu);
                else if (k <= 5) w32(pv + 0x98u + 4u * u(k - 3), 0xffffffffu);
                else if (k == 6) w32(pv + 0x8c, 0);
            } else if (st == 1) {
                if (ri(0, 3) != 0) slot_done(ee_read<uint32_t>(pv + 0x98));
                if (ri(0, 3) != 0) slot_done(ee_read<uint32_t>(pv + 0x9c));
                if (ri(0, 3) != 0) slot_done(ee_read<uint32_t>(pv + 0xa0));
                w16(P + 0x30c, ri(0, 5) == 0 ? 1u : 0u);
                w32(P + 0x2fc, ri(0, 5) == 0 ? m : 0u);
                if (ri(0, 4) != 0 && ee_read<int32_t>(pv + 0xa4) >= 0) hero_at(box_centre(ee_read<int32_t>(pv + 0xa4)));
                // the camera looking at the cargo's moby (its pvar +4), within the angles or near their edges
                const uint32_t mp = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t p = mp + (ee_read<uint32_t>(pv + 0x8c) << 8);
                const uint32_t q = mp + (ee_read<uint32_t>(ee_read<uint32_t>(p + 0x78) + 4) << 8);
                if (q >= mp && q < ee_read<uint32_t>(OV(0x0015ffe0))) {
                    const Vec4 t = ee_ref<Vec4>(q + 0x10);
                    const float yaw = rf(-3.1f, 3.1f), d = rf(6.0f, 30.0f), up = rf(0.0f, 8.0f);
                    const Vec4 c{t.x - d * std::cos(yaw), t.y - d * std::sin(yaw), t.z + up, 1.0f};
                    ee_ref<Vec4>(L14_ADDR(0x001674c0)) = c;
                    const float look = std::atan2(t.y - c.y, t.x - c.x);
                    const float pitch = -std::atan2(t.z + 4.0f - c.z, std::sqrt((t.x - c.x) * (t.x - c.x) + (t.y - c.y) * (t.y - c.y)));
                    static const float offs[] = {0.0f, 0.2f, 0.48f, 0.5f, 0.34f, 0.36f, 1.0f};
                    wf(L14_ADDR(0x001674d8), look + (coin() ? 1.0f : -1.0f) * offs[ri(0, 6)]);
                    wf(L14_ADDR(0x001674d4), pitch + (coin() ? 1.0f : -1.0f) * offs[ri(0, 6)]);
                }
            } else if (st == 2) {
                const uint32_t path = path_of(pv, 0x68);
                const int32_t n = ee_read<int32_t>(path);
                const int k = ri(0, 3);
                int32_t seg = k == 0 ? n - 2 : k == 1 ? ri(55, 57) : ri(0, n - 2);  // (its end, about its 56th point, anywhere)
                if (seg > n - 2) seg = n - 2;
                if (seg < 0) seg = 0;
                w32(pv + 0x60, u(seg));
                wf(pv + 0x64, ee_read<float>(path + 0x1cu + (u(seg) << 4)) * (k == 0 ? rf(0.95f, 1.0f) : rf(0.0f, 1.0f)));
                ee_ref<Vec4>(m + 0x10) = along(path, seg, rf(0.0f, 1.0f));
                w32(pv + 0x94, k == 1 && coin() ? 0u : u(ri(0, 3)));
                const int c = ri(0, 3);
                if (c == 1) w32(pv + 0x8c, 0);
                else if (c == 2) w32(pv + 0x8c, 0xffffffffu);
                const uint32_t cam = ee_read<uint32_t>(L14_ADDR(0x00167500));
                if (cam != 0) w16(cam + 0x86, coin() ? 0x13u : u(ri(0, 0x18)));
            } else if (st == 3) {
                w32(pv + 0x7c, coin() ? 1u : u(ri(2, 200)));
            }
            if (ri(0, 2) == 0) { give_hit(m, 0x00800000u); if (coin()) w32(pv + 0x8c, 0xffffffffu); }
            w32(0x0015ee08, u(ri(0, 3)));
            w8(0x0013d420, ri(0, 3) == 0 ? 1u : 0u);
            w8(m + 0x31, coin() ? 1u : 0u);
            sound_slot(m, pv + 0x6c);
            seed_rand();
        }, 300);

        g_ignored_words.resize(ign_p143);

        // class 922, the rocket: set up (its box or its target missing); waiting; flying toward its box, far, within a step,
        // at it; its target; its sound; 50 Hz
        classcheck("p143_class922", 0x002fdbb8, olt_class922_update_l14, [set_50hz, sound_slot, box_centre](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 3);
            w8(m + 0x20, u(st == 3 ? ri(3, 6) : st));
            if (st == 0) {
                const int k = ri(0, 2);
                if (k == 0) w32(pv, 0xffffffffu);
                else if (k == 1) w32(pv + 4, 0xffffffffu);
            } else if (st == 2 && ee_read<int32_t>(pv) >= 0) {
                const Vec4 t = box_centre(ee_read<int32_t>(pv));
                const float yaw = rf(-3.1f, 3.1f), pitch = rf(-0.5f, 0.5f);
                const int k = ri(0, 3);
                const float d = k == 0 ? rf(0.0f, 0.04f) : k == 1 ? 0.0f : rf(0.5f, 40.0f);
                ee_ref<Vec4>(m + 0x10) = Vec4{t.x - d * std::cos(yaw) * std::cos(pitch), t.y - d * std::sin(yaw) * std::cos(pitch), t.z - d * std::sin(pitch), 1.0f};
                wf(m + 0x40, rf(-3.1f, 3.1f)); wf(m + 0x44, rf(-1.0f, 1.0f)); wf(m + 0x48, rf(-3.1f, 3.1f));
                w32(pv + 0xc, u(ri(0, 3)));
                w8(m + 0x31, coin() ? 1u : 0u);
            }
            sound_slot(m, pv + 8);
            seed_rand();
        }, 240);

        // class 923, the carrier of class 933: every state and kind (+0xfc 0 / 1 / 2); its animation's ends; the hero
        // touching it; a hit of kind 0x210000 (the fall that follows, aged by the port); hidden with the hero in its box or
        // not; flying in / out near the paths' ends (its own speed, Kalebo III's); at the first path's end letting its moby
        // go; the missing paths and box of the set-up; 50 Hz
        // (the fall, state 6, runs enemy_physics: the w lanes its original stores from a VU0 register into the collision
        // record's row 0x173e70 and the moby's position, and the segment record 0x173e90 .. 0x173e9c, which the shared ports
        // do not reproduce -- batch 514's left-out words for class 28's fall -- are left out of this check only)
        const size_t ign_923 = g_ignored_words.size();
        for (const uint32_t a : {OV(0x00173e90), OV(0x00173e94), OV(0x00173e98), OV(0x00173e9c), OV(0x00173e7c)}) g_ignored_words.push_back(a);
        // (and the knock-back's move_state_start leaves the same stale lane in the position the bolts it drops copy: the w
        // lanes of every moby's position and of the pvars' quadwords -- the dropped bolts' known difference, batch 112's --
        // but for the carriers' own pvars, compared whole)
        level();
        for (uint32_t x = ee_read<uint32_t>(OV(0x0015ffd8)); x < ee_read<uint32_t>(OV(0x0015ffe0)); x += 0x100) g_ignored_words.push_back(x + 0x1c);
        {
            const std::vector<uint32_t> own = mobys_with(0x002fe2a0);
            for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < 0x02000000u; a += 0x10) {
                bool mine = false;
                for (const uint32_t x : own) { const uint32_t p = ee_read<uint32_t>(x + 0x78); if (a >= p && a < p + 0x100) mine = true; }
                if (!mine) g_ignored_words.push_back(a + 0xc);
            }
        }
        classcheck("p143_class923", 0x002fe2a0, olt_class923_update_l14, [set_50hz, give_hit, hit_source, hero_at, box_centre, path_of, mobys_with, run_port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 7);
            if (st != 0 && ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class923_update_l14, m)) return;
            w32(pv + 0xfc, u(ri(0, 3) == 0 ? ri(1, 2) : 0));
            if (st == 0) {
                w8(m + 0x20, 0);
                const int k = ri(0, 4);
                if (k == 0) w32(pv + 0xd8, 0xffffffffu);
                else if (k == 1) w32(pv + 0xdc, 0xffffffffu);
                else if (k == 2) w32(pv + 0xe0, 0xffffffffu);
                seed_rand();
                return;
            }
            if (st == 6) {  // shot: the knock-back started by the port, then a few frames of the fall
                give_hit(m, 0x00210000u);
                hit_source(m);
                seed_rand();
                if (!run_port(olt_class923_update_l14, m)) return;
                w8(m + 0x20, 6);
                const int age = ri(0, 30);
                for (int k = 0; k < age && ee_read<uint8_t>(m + 0x20) < 0xfe; ++k) { seed_rand(); if (!run_port(olt_class923_update_l14, m)) return; }
                const int k = ri(0, 3);
                if (k == 0) wf(m + 0x18, coin() ? 4.9f : 5.1f);
                else if (k == 1) {  // just above the ground some way from the hero (not on his moby), falling: it lands
                    Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10);
                    q.x += rf(6.0f, 10.0f); q.z += rf(0.05f, 0.4f);
                    ee_ref<Vec4>(m + 0x10) = q;
                }
                seed_rand();
                return;
            }
            w8(m + 0x20, u(st == 7 ? ri(7, 9) : st));
            if (st == 2 && ee_read<uint32_t>(pv + 0xfc) == 0 && ee_read<int32_t>(pv + 0xe0) >= 0 && coin()) {  // its moby spawned by the port
                hero_at(box_centre(ee_read<int32_t>(pv + 0xe0)));
                seed_rand();
                if (!run_port(olt_class923_update_l14, m)) return;
                w8(m + 0x20, 3);
            }
            if (ee_read<uint8_t>(m + 0x20) == 3 ? ri(0, 3) != 0 : coin()) {  // its animation at its end (or the hold anew)
                w8(m + 0x70, ee_read<uint8_t>(m + 0x70) | 2u);
                const uint8_t an = u(ri(0, 4));
                w8(m + 0x52, an);
                w8(m + 0x53, coin() ? an : u(ri(0, 4)));
            }
            w32(P + 0x23c, ri(0, 3) == 0 ? m : 0u);
            if (ri(0, 3) == 0) { give_hit(m, 0x00210000u); hit_source(m); }
            if (st == 2) {
                if (coin() && ee_read<int32_t>(pv + 0xe0) >= 0) hero_at(box_centre(ee_read<int32_t>(pv + 0xe0)));
            } else if (st == 4 || st == 5) {
                const uint32_t path = path_of(pv, st == 5 ? 0xdcu : 0xd8u);
                const int32_t n = ee_read<int32_t>(path);
                const bool end = coin();
                const int32_t seg = end ? n - 2 : ri(0, n - 2);
                w32(pv + 0xd0, u(seg < 0 ? 0 : seg));
                wf(pv + 0xd4, end ? ee_read<float>(path + 0x1cu + (u(seg < 0 ? 0 : seg) << 4)) * rf(0.97f, 1.0f) : rf(0.0f, 1.0f));
                wf(pv + 0xf8, coin() ? 0.0f : rf(0.05f, 0.5f));
                if (ri(0, 7) == 0) w32(0x0015ed84, 16);
            }
            seed_rand();
        }, 300);
        g_ignored_words.resize(ign_923);

        // class 1193, the bombers' mortar bomb: a bomber firing (its state 6 at the mortar's frame, the port) spawns it; aged 0 ..
        // 90 frames by its port (bouncing, rolling), then: its fuse at its end; touching another moby or its owner; at the
        // ground about the hero (a bounce, the first or a later one); rolling with its speed at the floor or above; still
        // rising under something; its smoke's timer out; 50 Hz
        for (uint32_t k = 0; k < 0x30; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
        spawncheck("p143_class1193", 0x00300e00, olt_class1193_update_l14, [set_50hz, mobys_with, alive, run_port]() -> uint32_t {
            if (ri(0, 2) == 0) set_50hz();
            const std::vector<uint32_t> bombers = mobys_with(0x002b46e8);
            const uint32_t b = bombers[static_cast<size_t>(ri(0, static_cast<int>(bombers.size()) - 1))];
            const uint32_t bpv = ee_read<uint32_t>(b + 0x78);
            if (ee_read<uint8_t>(b + 0x20) == 0 && !run_port(olt_class31_update_l14, b)) return 0u;
            const std::vector<uint32_t> before = mobys_with(0x00300e00);
            w8(b + 0x20, 6);
            {  // the mortar's frame: the animation just past frame 3 (anim_crossed(m, 3.0))
                const uint32_t drop = lvd::path(ee_read<int32_t>(bpv + 0xd8));
                const int32_t n = ee_read<int32_t>(drop);
                w16(bpv + 0x100, coin() ? 0u : u(ri(0, n - 1)));
                const uint32_t cur = ee_read<uint8_t>(b + 0x52) == 0xff ? ee_read<uint32_t>(b + 0x6c) : ee_read<uint32_t>(b + 0x68);
                if (cur == 0) return 0u;
                const float frame = static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f;
                w8(b + 0x53, u(ee_read<uint8_t>(b + 0x52) ^ 1u));
                wf(b + 0x54, 3.0f - frame + rf(0.05f, 0.4f));
                wf(b + 0x58, 1.0f);
                wf(b + 0x5c, 0.5f);
                wf(bpv + 0x104, rf(-1.0f, 1.0f));
            }
            seed_rand();
            if (!run_port(olt_class31_update_l14, b)) return 0u;
            uint32_t x = 0;
            for (const uint32_t y : mobys_with(0x00300e00))
                if (std::find(before.begin(), before.end(), y) == before.end()) x = y;
            if (x == 0) return 0u;
            const uint32_t pv = ee_read<uint32_t>(x + 0x78);
            const int age = ri(0, 3) == 0 ? ri(0, 4) : ri(5, 90);
            for (int k = 0; k < age && alive(x); ++k) { seed_rand(); if (!run_port(olt_class1193_update_l14, x)) return 0u; }
            if (!alive(x)) return 0u;
            const int mode = ri(0, 10);
            if (mode == 1) {
                w32(pv + 0x20, coin() ? 1u : 2u);  // the fuse at its end
            } else if (mode == 2) {  // touching another moby (a turret), or its owner
                const std::vector<uint32_t> ts = mobys_with(0x002b3bf0);
                const uint32_t o = coin() || ts.empty() ? ee_read<uint32_t>(pv + 0x24) : ts[static_cast<size_t>(ri(0, static_cast<int>(ts.size()) - 1))];
                if (o != 0) { Vec4 q = ee_ref<Vec4>(o + 0x10); q.z += rf(0.0f, 1.0f); ee_ref<Vec4>(x + 0x10) = q; }
            } else if (mode == 3 || mode == 4) {  // at the ground about the hero, falling: a bounce (the first, or a later one)
                Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10);
                q.x += rf(2.0f, 5.0f); q.y += rf(-3.0f, 3.0f); q.z += rf(0.3f, 0.9f);
                ee_ref<Vec4>(x + 0x10) = q;
                ee_ref<Vec4>(pv + 0x10) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), -rf(0.1f, 0.6f), 0.0f};
                w8(x + 0x20, 0);
                w16(pv + 0x2a, u(mode == 3 ? 0 : ri(1, 3)));
            } else if (mode == 5) {  // rolling, its speed at the floor or above
                w8(x + 0x20, 2);
                wf(pv + 0x1c, coin() ? rf(0.0f, 0.01f) : rf(0.05f, 0.4f));
                wf(pv + 0x2c, 1.0f / static_cast<float>(ee_read<int32_t>(pv + 0x20) > 0 ? ee_read<int32_t>(pv + 0x20) : 1));
            } else if (mode == 6) {  // still rising, under something (the hero's feet as a ceiling: under the ground)
                Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10);
                q.x += rf(2.0f, 5.0f); q.z -= rf(0.3f, 0.6f);
                ee_ref<Vec4>(x + 0x10) = q;
                ee_ref<Vec4>(pv + 0x10) = Vec4{rf(-0.1f, 0.1f), rf(-0.1f, 0.1f), rf(0.1f, 0.5f), 0.0f};
                w8(x + 0x20, 1);
            } else if (mode == 7) {
                w16(pv + 0x28, coin() ? 0u : 1u);  // the smoke's timer out
            } else if (mode == 8) {  // flying at the owner (its way meets it), or fast (the spin's top)
                const uint32_t o = ee_read<uint32_t>(pv + 0x24);
                if (o != 0 && coin()) {
                    const Vec4 t = ee_ref<Vec4>(o + 0x10);
                    const Vec4 v{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), 0.0f};
                    ee_ref<Vec4>(x + 0x10) = Vec4{t.x - 2.0f * v.x, t.y - 2.0f * v.y, t.z - 2.0f * v.z, 1.0f};
                    ee_ref<Vec4>(pv + 0x10) = Vec4{v.x * 3.0f, v.y * 3.0f, v.z * 3.0f, 0.0f};
                } else {
                    ee_ref<Vec4>(pv + 0x10) = Vec4{rf(-1.0f, 1.0f), rf(-1.0f, 1.0f), rf(-0.2f, 0.2f), 0.0f};
                }
            }
            if ((mode == 3 || mode == 4) && coin()) {  // slow along the ground: the damping's floor and the roll's
                Vec4 v = ee_ref<Vec4>(pv + 0x10);
                v.x *= 0.05f; v.y *= 0.05f;
                ee_ref<Vec4>(pv + 0x10) = v;
                if (coin()) w16(pv + 0x2a, 3);
            }
            seed_rand();
            return x;
        }, 300);
        g_ignored_words.resize(ign_p143);

        // ---- P14.4 (part): classes 1352 (a group raised when its watched moby reaches state 2) and 1559 (a group's watcher,
        // the scenes' smoke) through every state (oltanis_small_classes.cpp; classes 1397 and 1418 are covered whole by
        // the generic class checks, 1418's queued draw by cb307a28)
        auto group_members = [](uint32_t pv) {
            std::vector<uint32_t> v;
            uint32_t e = lvd::group(ee_read<int32_t>(pv));
            if (ee_read<int32_t>(pv) < 0 || e == 0) return v;
            for (;; e += 2) {
                v.push_back(ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8));
                if (ee_read<int16_t>(e) < 0) break;
            }
            return v;
        };
        // class 1352: set up (each index missing, done before by the uid's byte or the save bit); waiting, its watched
        // moby in state 2 or not; rising, the timer at its start, middle and end; 50 Hz
        classcheck("p144_class1352", 0x00305408, olt_class1352_update_l14, [set_50hz, uid_done, run_port](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 3);
            if (st == 0) {
                w8(m + 0x20, 0);
                const int k = ri(0, 5);
                if (k == 0) w32(pv + 0xc, 0xffffffffu);
                else if (k == 1) w32(pv, 0xffffffffu);
                else if (k == 2) w32(pv + 0x10, 0xffffffffu);
                else if (k <= 4) uid_done(m, k - 3);
                seed_rand();
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class1352_update_l14, m)) return;  // (the group sunk first)
            w8(m + 0x20, u(st == 3 ? ri(3, 5) : st));
            if (st == 1) {
                const uint32_t watched = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0xc) << 8);
                w8(watched + 0x20, coin() ? 2u : u(ri(0, 3)));
            } else if (st == 2) {
                static const int32_t ts[] = {1, 2, 60, 119, 120};
                w32(pv + 4, u(ts[ri(0, 4)]));
                wf(pv + 8, 1.0f / 120.0f);
            }
            seed_rand();
        }, 120);
        // class 1559: its group gone (every member retired) or not, the flag 0x13d41f set or not, no group; set up; in
        // scenes 2 .. 7 (3 .. 5 puff at the scene's second moby) in game mode 2 or another
        classcheck("p144_class1559", 0x003087c0, olt_class1559_update_l14, [set_50hz, group_members, mobys_with](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            if (coin()) for (const uint32_t x : group_members(pv)) w8(x + 0x20, 0xff);
            w8(0x0013d41f, ri(0, 3) == 0 ? 1u : 0u);
            if (ri(0, 4) == 0) w32(pv, 0xffffffffu);
            w8(m + 0x20, u(ri(0, 2)));
            w32(L14_ADDR(0x0015f5c4), coin() ? 2u : u(ri(0, 3)));
            const uint32_t S = L14_ADDR(0x0016cf60);
            w32(S + 0x30, u(ri(2, 7)));
            const std::vector<uint32_t> live = mobys_with(0x002fefe0);  // (a moby of the level as the scene's)
            w32(S + 0x17c, live.empty() || ri(0, 3) == 0 ? m : live[0]);
            w32(S + 0x178, m);
            if (coin()) w16(ee_read<uint32_t>(S + 0x17c) + 0xa6, 0x2ee);  // (the class scene_moby_smoke puffs for: none on Oltanis)
            seed_rand();
        }, 120);

        // ---- P14.4: classes 1354 (the pickup, oltanis_pickup.cpp), 1395 and 1416 (the bridge and the pad,
        // oltanis_bridge.cpp), 1417 (the path platform, oltanis_path_platform.cpp) through every state and helper
        if (std::getenv("B221_INFO") != nullptr) {  // the play-test spots: each placed moby of the four classes (index, uid, place)
            level();
            const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
            static const struct { const char* name; uint32_t fn; } cls[] = {{"pickup 1354", 0x00305758}, {"bridge 1395", 0x00305d28},
                                                                            {"pad 1416", 0x00306b78}, {"path platform 1417", 0x00306ee0}};
            for (const auto& c : cls)
                for (const uint32_t x : mobys_with(c.fn)) {
                    const Vec4 q = ee_ref<Vec4>(x + 0x10);
                    const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                    std::printf("INFO P14.4 %s: moby %u uid %d at %.1f %.1f %.1f state %u slot %u pvars %d %d %d %d path %d box %d carried %d\n", c.name,
                                (x - mpool) >> 8, ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(x + 0x20), ee_read<uint8_t>(x + 0xb0),
                                ee_read<int32_t>(xp), ee_read<int32_t>(xp + 4), ee_read<int32_t>(xp + 8), ee_read<int32_t>(xp + 0x1c),
                                ee_read<int32_t>(xp + 0x78), ee_read<int32_t>(xp + 0xc4), ee_read<int32_t>(xp + 0xa0));
                }
        }
        // the help line and the dialogue idle (half the cases), so that the messages' requests are taken and compared
        auto help_idle = [] {
            if (coin()) return;
            w32(0x00151720, 0);
            w32(0x001516d0 + 0x50, 0);
            w32(0x001516d0 + 0x1c, 0xffffffffu);
            w32(OV(0x00179410), 0);
            w32(OV(0x00179434), 0xffffffffu);
        };
        {  // class 1354: its state 2 requests the level change: run for real on both sides (the talkers' way above)
            const std::time_t saved_clock = g_host_clock_fixed;
            g_host_clock_fixed = 1759658400;
            const auto saved_hooks = g_extra_hooks;
            g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                             {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                             {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
            g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80};
            // set up (its checkpoint slot 0xff or not); spinning, every tenth frame or not, the hero within 1 across and 2 up or
            // just past either (his +0x22a8 at 0: the scene's start takes its early way out, the scene's stream is never read),
            // the flag 0x13d4d5 either way; after the scene (game mode 2 or not, its slot 0xff or not, either message); fading
            // out near its end; other states; 50 Hz
            classcheck("p144_class1354", 0x00305758, olt_class1354_update_l14, [set_50hz, hero_at, run_port, slot_done, help_idle](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t P = 0x0013f350;
                if (ri(0, 2) == 0) set_50hz();
                const int st = ri(0, 4);
                if (st != 0 && ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class1354_update_l14, m)) return;  // (the set-up first)
                w8(m + 0x20, u(st == 4 ? ri(4, 9) : st));
                if ((st == 0 || st == 2) && coin()) slot_done(ee_read<uint8_t>(m + 0xb0));
                if (st == 1) {
                    w32(L14_ADDR(0x0015f5cc), coin() ? u(10 * ri(0, 500)) : (R() & 0xffff));
                    // (its place this frame: its kept place plus the bob)
                    const Vec4 c = ee_ref<Vec4>(pv), b = ee_ref<Vec4>(m + 0xd0);
                    const float k = ee_read<float>(L14_ADDR(0x001621f0));
                    const int rk = ri(0, 2);  // (within 1, about 1, past it)
                    const float r = rk == 0 ? rf(0.0f, 0.95f) : rk == 1 ? rf(0.95f, 1.25f) : rf(1.25f, 3.0f), a = rf(-3.1f, 3.1f);
                    const float dz = coin() ? rf(-1.9f, 1.9f) : (coin() ? 1.0f : -1.0f) * rf(2.1f, 3.0f);
                    hero_at(Vec4{c.x + k * b.x + r * std::cos(a), c.y + k * b.y + r * std::sin(a), c.z + k * b.z + dz, 1.0f});
                    w8(0x0013d4d5, coin() ? 1u : 0u);
                    w32(P + 0x22a8, 0);
                    if (coin()) {  // (the glitter: its phases about to wrap past 255 or 0, its timers at their ends)
                        for (uint32_t g = 0; g < 4; ++g) {
                            wf(pv + 0x20 + g * 4, coin() ? rf(252.0f, 254.9f) : rf(0.0f, 2.5f));
                            if (coin()) w32(pv + 0x40 + g * 4, u(ri(1, 2)));
                        }
                    }
                } else if (st == 2) {
                    w32(L14_ADDR(0x0015f5c4), coin() ? 2u : u(ri(0, 1)));
                    w32(pv + 0x18, coin() ? 1u : 0u);
                } else if (st == 3) {
                    const int k = ri(0, 2);
                    wf(m + 0x2c, k == 0 ? 0.0f : k == 1 ? rf(0.0f, 0.004f) : rf(0.1f, 2.0f));
                }
                help_idle();
                seed_rand();
            }, 300);
            g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
            g_extra_hooks = saved_hooks;
            g_host_clock_fixed = saved_clock;
        }
        // class 1395, the bridge: set up (its lid there or not, some pieces laid before); waiting, the watched moby in its
        // state 2 or not; a piece tipping (each of the six, the timer running or at its end); the lid turning (the timer
        // running or at its end, its sound slot its own and playing or not); 50 Hz
        classcheck("p144_class1395", 0x00305d28, olt_class1395_update_l14, [set_50hz, run_port, sound_slot](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 4);
            if (st == 0) {
                w8(m + 0x20, 0);
                if (ri(0, 3) == 0) {  // (laid before: the lid or some of the pieces)
                    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                    if (coin()) w32(pv, first + 0x100 * u(ri(0, 60)));
                    for (uint32_t k = 0; k < 6; ++k) if (ri(0, 3) == 0) w32(pv + 4 + k * 4, first + (ee_read<uint32_t>(pv + 0x28 + k * 4) << 8));
                }
                seed_rand();
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class1395_update_l14, m)) return;  // (the set-up first)
            if (ee_read<int32_t>(pv) == 0) return;
            w8(m + 0x20, u(st == 4 ? ri(4, 7) : st));
            if (st == 1) {
                const uint32_t watched = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0x1c) << 8);
                w8(watched + 0x20, coin() ? 2u : u(ri(0, 3)));
            } else if (st == 2 || st == 3) {
                static const int16_t ts[] = {1, 2, 17, 30};
                w16(pv + 0x20, u(ts[ri(0, 3)]));
                wf(pv + 0x24, 1.0f / rf(20.0f, 40.0f));
                w16(pv + 0x22, u(ri(0, 5)));
                sound_slot(m, pv + 0x40);
            }
            seed_rand();
        }, 240);
        // class 1416, the pad: set up (done before by its uid's byte or save bit, a checkpoint slot (+8) 0xff or not; its way's
        // path, one of its points' w lane its +4); pulsing, the hero on it or not, the halfword 0x13f65e set or not; 50 Hz
        classcheck("p144_class1416", 0x00306b78, olt_class1416_update_l14, [set_50hz, uid_done, slot_done, path_of](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 2);
            w8(m + 0x20, u(st == 2 ? ri(2, 5) : st));
            if (ee_read<int32_t>(pv) < 0 && coin()) w32(pv, u(ri(100, 140)));  // (the level's pads have no way (-1): one dealt, the code seen)
            if (ee_read<int32_t>(pv) >= 0 && coin()) {  // (one of its way's points carrying the pad's value)
                const uint32_t path = path_of(pv, 0);
                const int32_t n = ee_read<int32_t>(path);
                if (n > 0) {
                    const uint32_t w = path + 0x1cu + (u(ri(0, n - 1)) << 4);
                    w32(w, ee_read<uint32_t>(pv + 4));
                }
            }
            if (st == 0) {
                const int k = ri(0, 3);
                if (k <= 1) uid_done(m, k);
                w32(pv + 8, coin() ? 1u : 0u);
                if (coin()) slot_done(ee_read<uint8_t>(m + 0xb0));
            } else if (st == 1) {
                wf(pv + 0xc, rf(-3.1f, 3.1f));
                w32(0x0013f64c, ri(0, 3) != 0 ? m : 0u);
                w16(0x0013f65e, ri(0, 3) == 0 ? 1u : 0u);
            }
            seed_rand();
        }, 200);
        // class 1417, the path platform: set up (no path, a path without points, its box or none, its carried moby or none);
        // hidden, the hero in its box or not; flying (along its path, near its end looping or not, low near the level's floor;
        // ridden in each of the two ways and the two hero states of the camera's test or not; the current camera of type 7 or
        // not; the carried moby's timer and class 803); gone, its delay running or at its end; hits of kind 0x800000; its
        // sound; 50 Hz
        classcheck("p144_class1417", 0x00306ee0, olt_class1417_update_l14, [set_50hz, run_port, give_hit, sound_slot, hero_at, box_centre, path_of, along](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            const uint32_t P = 0x0013f350;
            if (ri(0, 2) == 0) set_50hz();
            const int st = ri(0, 4);
            if (st == 0) {
                w8(m + 0x20, 0);
                const int k = ri(0, 7);
                if (k == 0) w32(pv + 0x78, 0xffffffffu);
                else if (k == 1) w32(pv + 0xc4, 0xffffffffu);
                else if (k == 2) w32(pv + 0xa0, 0xffffffffu);
                if (ri(0, 4) == 0) give_hit(m, 0x00800000u);
                seed_rand();
                return;
            }
            if (ee_read<uint8_t>(m + 0x20) == 0 && !run_port(olt_class1417_update_l14, m)) return;  // (the set-up first)
            if (ee_read<uint8_t>(m + 0x20) >= 0x80) return;
            w8(m + 0x20, u(st == 4 ? ri(4, 7) : st));
            const int32_t carried = ee_read<int32_t>(pv + 0xa0);
            const uint32_t o = carried >= 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + (u(carried) << 8) : 0u;
            if (st == 1) {
                if (ee_read<int32_t>(pv + 0xc4) >= 0 && ri(0, 2) != 0) hero_at(box_centre(ee_read<int32_t>(pv + 0xc4)));
            } else if (st == 2) {
                const uint32_t path = path_of(pv, 0x78);
                const int32_t n = ee_read<int32_t>(path);
                const int k = ri(0, 3);
                const int32_t seg = k == 0 ? n - 2 : ri(0, n - 1);  // (near its end, or anywhere)
                const float t = k == 0 ? rf(0.9f, 1.0f) : rf(0.0f, 1.0f);
                w32(pv + 0x70, u(seg));
                wf(pv + 0x74, ee_read<float>(path + 0x1cu + (u(seg < 0 ? 0 : seg) << 4)) * t);
                w16(pv + 0x8e, coin() ? 1u : 0u);
                Vec4 q = along(path, seg, t);
                const float path_z = q.z;
                q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f);
                if (k == 0 && coin()) q.x += (coin() ? 1.0f : -1.0f) * rf(0.0f, 0.009f);
                q.z += ri(0, 4) == 0 ? -(q.z - ee_read<float>(L14_ADDR(0x0016228c))) + rf(-0.5f, 0.5f) : rf(-0.5f, 0.5f);  // (near the floor)
                ee_ref<Vec4>(m + 0x10) = q;
                wf(pv + 0xb0, coin() ? 0.0f : ri(0, 2) == 0 ? path_z - rf(0.0f, 9.0f) : rf(0.0f, 30.0f));  // (low: its point lowered below the 10)
                wf(pv + 0xb4, rf(0.0f, 0.5f));
                wfrand(pv + 0x80, 3, -0.3f, 0.3f);
                // ridden: the hero's riding states on the carried moby, or not
                const int ride = ri(0, 5);
                w32(P + 0x2084, ride == 0 ? 0x25u : ride == 1 ? 0x26u : u(ri(0, 0x24)));
                w32(P + 0x208c, ride == 2 ? 0xeu : u(ri(0, 13)));
                w32(P + 0x964, ride <= 1 && coin() ? o : (coin() ? o : 0u));
                w32(P + 0x994, ride == 2 || coin() ? o : 0u);
                w32(0x001413dc, ri(0, 2) == 0 ? 0xdu : ri(0, 1) == 0 ? 0xeu : u(ri(0, 20)));
                {  // (the current camera of type 7 or not: its state block in the test area)
                    const uint32_t cam = ee_read<uint32_t>(L14_ADDR(0x00167500));
                    if (cam >= 0x00100000u && cam < 0x02000000u && coin()) {
                        w16(cam + 0x86, coin() ? 7u : u(ri(0, 21)));
                        if (ee_read<uint32_t>(cam + 0x70) < 0x00100000u || ee_read<uint32_t>(cam + 0x70) >= 0x02000000u) w32(cam + 0x70, TEST_AREA + 0x6800);
                    }
                }
                if (o != 0) {
                    static const int32_t ts[] = {0, 1, 2, 40};
                    w32(pv + 0xcc, u(ts[ri(0, 3)]));
                    if (coin()) w16(o + 0xa6, 0x323);
                }
            } else if (st == 3) {
                static const int16_t ts[] = {1, 2, 30};
                w16(pv + 0x8c, u(ts[ri(0, 2)]));
                if (coin()) w32(pv + 0xc4, 0xffffffffu);
            }
            if (ri(0, 3) == 0) give_hit(m, 0x00800000u);
            sound_slot(m, pv + 0x7c);
            seed_rand();
        }, 300);

        // ---- P14.4: camera types 20 and 21 (oltanis_cameras.cpp). No camera record of the level is of these types after the
        // first frame (the cam_type20 / 21 checks above run them on another type's data): here a camera of the test area named
        // by the level's camera record 0, its data block in the test area laid out as each type reads it (batch 506's way for
        // type 19), from the level's own paths, boxes, rails and mobys
        {
            const uint32_t CAM = TEST_AREA + 0x6000, ST = CAM + 0x100, D = CAM + 0x200, OTHER = CAM + 0x300, SRC = CAM + 0x400, CUR = CAM + 0x500;
            level();
            std::vector<int32_t> paths;
            for (int32_t i = 0; i < 176; ++i) {
                const uint32_t p = lvd::path(i);
                if (p >= 0x00100000u && p < 0x02000000u && ee_read<int32_t>(p) >= 2 && ee_read<int32_t>(p) <= 64) paths.push_back(i);
            }
            const int32_t nboxes = lvd::box_count(lvd::BOXES), nrails = lvd::rail_count();
            // the rail drones (class 211: their rail +0x60, their path +0x88) and the mobys whose group's first three are drones
            const std::vector<uint32_t> drones = mobys_with(0x002d5f40);
            std::vector<uint32_t> owners;
            for (uint32_t x = ee_read<uint32_t>(OV(0x0015ffd8)); x < ee_read<uint32_t>(OV(0x0015ffe0)); x += 0x100) {
                uint32_t e = lvd::group(ee_read<uint8_t>(x + 0x21));
                if (ee_read<uint8_t>(x + 0x20) >= 0xfe || e == 0) continue;
                int n = 0;
                for (;; e += 2) {
                    const uint32_t y = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e) & 0x7fffu) << 8);
                    if (ee_read<uint32_t>(y + 0x74) == 0x002d5f40u) ++n;
                    if (n >= 3 || ee_read<int16_t>(e) < 0) break;
                }
                if (n >= 3) owners.push_back(x);
            }
            if (std::getenv("B221_INFO") != nullptr)
                std::printf("INFO P14.4 cameras: %zu paths, %d boxes, %d rails, %zu drones, %zu group owners\n", paths.size(), nboxes, nrails, drones.size(), owners.size());
            auto pickp = [paths] { return paths[static_cast<size_t>(ri(0, static_cast<int>(paths.size()) - 1))]; };
            auto path_at = [](int32_t i) { return lvd::path(i); };
            auto rail_path = [](int32_t i) { return ee_read<uint32_t>(lvd::rail(i) + 0x10); };
            // the hero on a rail (its path, a leg and a fraction of it)
            auto hero_rail = [](uint32_t path) {
                const uint32_t P = 0x0013f350;
                w32(P + 0x560, path);
                const int32_t n = ee_read<int32_t>(path);
                w32(P + 0x564, u(ri(0, n > 1 ? n - 2 : 0)));
                wf(P + 0x568, rf(0.0f, 1.0f));
            };
            // the common part: the camera (its matrix source SRC, its rows from it or random), the other camera, the current one
            auto cam_common = [CAM, ST, D, OTHER, SRC, CUR, set_50hz](int type) {
                if (ri(0, 2) == 0) set_50hz();  // (the frame scale's calls: the timers, the speed)
                for (uint32_t o = 0; o < 0x600; o += 4) w32(CAM + o, 0);
                w32(lvd::camera(0) + 0x1c, D);
                w16(CAM + 0x84, 0);
                w32(CAM + 0x70, ST);
                w16(CAM + 0x86, u(type));
                w8(CAM + 0x7c, u(ri(0, 3)));
                {
                    const float a = rf(-3.1f, 3.1f), ca = std::cos(a), sa = std::sin(a);
                    for (uint32_t r = 0; r < 3; ++r) {
                        const Vec4 v = ee_ref<Vec4>(OV(0x00166fd0) + r * 0x10);
                        ee_ref<Vec4>(SRC + r * 0x10) = coin() ? v : Vec4{ca * v.x - sa * v.y, sa * v.x + ca * v.y, v.z, v.w};
                    }
                    const Vec4 c = ee_ref<Vec4>(OV(0x00166dc0));
                    ee_ref<Vec4>(SRC + 0x30) = Vec4{c.x + rf(-2, 2), c.y + rf(-2, 2), c.z + rf(-1, 1), c.w};
                    w32(L14_ADDR(0x00167504), SRC);
                }
                const int start = ri(0, 2);
                for (uint32_t r = 0; r < 4; ++r) {
                    if (start == 0) ee_ref<Vec4>(CAM + r * 0x10) = ee_ref<Vec4>(SRC + r * 0x10);
                    else if (start == 1) wfrand(CAM + r * 0x10, 3, -1.0f, 1.0f);
                }
                w16(OTHER + 0x86, coin() ? 0x14u : u(ri(0, 0x20)));
                w16(OTHER + 0x7e, u(ri(0, 1)));
                w8(OTHER + 0x7c, u(ri(0, 3)));
                w16(CUR + 0x86, coin() ? 0x14u : u(ri(0, 0x20)));
                if (coin()) w32(L14_ADDR(0x00167500), CUR);
                w32(0x001413dc, ri(0, 3) == 0 ? 0xfu : u(ri(0, 20)));
            };
            // type 20's data: the zone (as type 19's), the paths (the camera path's w lanes: none or a field of view), the moby
            // looked at, the flags, frames and speed, the hero's states, the group's owner, the hero's box; the hero near
            auto cam20_setup = [level, cam_common, D, pickp, path_at, nboxes, owners, drones](bool fresh) {
                level();
                cam_common(20);
                const int kind = ri(0, 4);
                w32(D + 0xc, kind == 0 ? u(ri(0, nboxes > 0 ? nboxes - 1 : 0)) : 0xffffffffu);
                w32(D + 0x10, kind == 1 ? u(ri(0, 4)) : 0xffffffffu);
                w32(D + 8, kind == 2 ? u(ri(0, 4)) : 0xffffffffu);
                w32(D + 0x14, kind == 3 ? u(pickp()) : 0xffffffffu);
                w32(D + 0x20, u(pickp()));
                w32(D + 0x24, u(pickp()));
                if (fresh && coin()) {
                    const uint32_t path = path_at(ee_read<int32_t>(D + 0x20));
                    for (int32_t k = 0; k < ee_read<int32_t>(path); ++k) wf(path + 0x1c + u(k) * 0x10, ri(0, 2) == 0 ? rf(20.0f, 120.0f) : 0.0f);
                }
                w32(D + 0x28, coin() ? 0xffffffffu : u(ri(0, 40)));
                wfrand(D + 0x2c, 3, -2.0f, 2.0f);
                w8(D + 0x38, ri(0, 3) == 0 ? 1u : 0u);
                w8(D + 0x39, ri(0, 4) == 0 ? 1u : 0u);
                w16(D + 0x3a, u(ri(0, 3) == 0 ? 0 : ri(1, 300)));
                wf(D + 0x3c, rf(0.5f, 20.0f));
                w32(D + 0x44, coin() ? 0xffffffffu : ee_read<uint32_t>(0x001413d4));
                w32(D + 0x48, coin() ? 0xffffffffu : (coin() ? ee_read<uint32_t>(0x001413dc) : 7u));
                const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t owner = !owners.empty() && ri(0, 4) != 0 ? owners[static_cast<size_t>(ri(0, static_cast<int>(owners.size()) - 1))]
                                                                         : (!drones.empty() ? drones[0] : mpool);
                w32(D + 0x50, (owner - mpool) >> 8);
                w32(D + 0x54, u(ri(0, nboxes > 0 ? nboxes - 1 : 0)));
                const uint32_t path = path_at(ee_read<int32_t>(kind == 3 && coin() ? D + 0x14 : D + 0x20));
                const Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ri(0, ee_read<int32_t>(path) - 1)) * 0x10);
                const float r = coin() ? rf(0.0f, 3.0f) : rf(10.0f, 60.0f);
                ee_ref<Vec4>(0x0013f3d0) = Vec4{q.x + rf(-r, r), q.y + rf(-r, r), q.z + rf(-1, 2), 1.0f};
                if (kind == 0 && coin()) {
                    const Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, ee_read<int32_t>(D + 0xc)) + 0x30);
                    const float e = coin() ? 0.2f : rf(0.5f, 6.0f);
                    ee_ref<Vec4>(0x0013f3d0) = Vec4{c.x + rf(-e, e), c.y + rf(-e, e), c.z + rf(-e, e), 1.0f};
                }
            };
            // type 21's data: the state, the rails' three mobys (drones) and their rails, the counters; the hero on one of the
            // rails (or another), his moby's facing
            auto cam21_setup = [level, cam_common, D, drones, nrails, rail_path, hero_rail] {
                level();
                cam_common(21);
                w32(D + 0x20, u(ri(0, 3)));
                const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
                for (uint32_t k = 0; k < 3; ++k) {
                    const uint32_t x = drones.empty() ? mpool : drones[static_cast<size_t>(ri(0, static_cast<int>(drones.size()) - 1))];
                    w32(D + 0x24 + k * 4, (x - mpool) >> 8);
                    const int32_t rail = drones.empty() || ri(0, 3) == 0 ? ri(0, nrails - 1) : ee_read<int32_t>(ee_read<uint32_t>(x + 0x78) + 0x60);
                    w32(D + 0x34 + k * 4, u(rail));
                }
                w16(D + 0x30, u(ri(0, 4)));
                w16(D + 0x32, u(ri(0, 2)));
                const int which = ri(0, 4);
                const int32_t rail = which < 3 ? ee_read<int32_t>(D + 0x34 + u(which) * 4)
                                               : (which == 3 && !drones.empty() ? ee_read<int32_t>(ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(D + 0x24) << 8) + 0x78) + 0x60)
                                                                                : ri(0, nrails - 1));
                hero_rail(rail_path(rail));
                const uint32_t hm = ee_read<uint32_t>(0x001413d0);
                wf(hm + 0xd0, rf(-1, 1));
                wf(hm + 0xd4, rf(-1, 1));
            };
            if (!paths.empty() && nboxes > 0 && nrails > 0) {
                l14check("p144_cam20_accept", 0x00315920, 300, Ret::Int32, [cam20_setup, CAM, OTHER] {
                    cam20_setup(true);
                    seed_rand();
                    return Args{{CAM, coin() ? OTHER : 0u}, {}};
                }, [](A a) { return r32(olt_cam_type20_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
                l14check("p144_cam20_init", 0x00314f00, 150, Ret::None, [cam20_setup, CAM] {
                    cam20_setup(true);
                    seed_rand();
                    return Args{{CAM}, {}};
                }, [](A a) { olt_cam_type20_init_l14(a.i[0]); return uint64_t{0}; });
                // the update: the camera started by the init (the port's, checked above), then its phase, timer, leg varied; at its
                // end the hero on a rail (a drone's of the group, or another); the w lanes of the camera's side and up rows left
                // out (VCROSS's stale lane, as type 19's check)
                const size_t n_ign = g_ignored_words.size();
                g_ignored_words.push_back(CAM + 0x1c);
                g_ignored_words.push_back(CAM + 0x2c);
                l14check("p144_cam20_update", 0x00315290, 400, Ret::None, [cam20_setup, CAM, ST, D, path_at, rail_path, hero_rail, nrails] {
                    for (int attempt = 0; attempt < 10; ++attempt) {
                        cam20_setup(true);
                        const uint32_t sp0 = ee_stack_ptr();
                        try { olt_cam_type20_init_l14(CAM); break; } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                    }
                    const uint32_t blk = ST + 0x40;
                    w16(blk + 0x14, ri(0, 3) == 0 ? 0u : 1u);
                    if (coin()) w16(blk + 0x16, u(ri(0, 3) == 0 ? 1 : ri(0, 30)));
                    const uint32_t path = path_at(ee_read<int32_t>(D + 0x20));
                    const int32_t n = ee_read<int32_t>(path);
                    const int k = ri(0, 3);
                    w32(blk, u(k == 0 ? n - 2 : ri(0, n - 1)));
                    wf(blk + 4, k == 0 ? rf(0.0f, ee_read<float>(path + 0x1c + u(n - 2) * 0x10)) : rf(0.0f, 2.0f));
                    if (coin()) wf(blk + 0x10, rf(0.0f, ee_read<float>(blk + 8)));
                    // the end's rails: a drone's of the group (the hero put by the nearest member), or another
                    const uint32_t owner = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(D + 0x50) << 8);
                    const uint32_t e = lvd::group(ee_read<uint8_t>(owner + 0x21));
                    if (e != 0 && coin()) {
                        const uint32_t y = ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(e + u(ri(0, 2)) * 2) & 0x7fffu) << 8);
                        hero_rail(rail_path(ee_read<int32_t>(ee_read<uint32_t>(y + 0x78) + 0x60)));
                        if (coin()) {  // (the drone's own path's start moved off the rail: the nearest point's search range seen)
                            const uint32_t own = path_at(ee_read<int32_t>(ee_read<uint32_t>(y + 0x78) + 0x88));
                            wf(own + 0x10, ee_read<float>(own + 0x10) + (coin() ? 1.0f : -1.0f) * rf(3.0f, 30.0f));
                            wf(own + 0x14, ee_read<float>(own + 0x14) + rf(-10.0f, 10.0f));
                        }
                    } else {
                        hero_rail(rail_path(ri(0, nrails - 1)));
                    }
                    seed_rand();
                    return Args{{CAM}, {}};
                }, [](A a) { olt_cam_type20_update_l14(a.i[0]); return uint64_t{0}; });
                g_ignored_words.resize(n_ign);
                l14check("p144_cam21_accept", 0x00316748, 300, Ret::Int32, [cam21_setup, CAM, OTHER] {
                    cam21_setup();
                    seed_rand();
                    return Args{{CAM, coin() ? OTHER : 0u}, {}};
                }, [](A a) { return r32(olt_cam_type21_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
                g_ignored_words.push_back(CAM + 0x1c);
                g_ignored_words.push_back(CAM + 0x2c);
                l14check("p144_cam21_init", 0x003159d8, 150, Ret::None, [cam21_setup, CAM] {
                    cam21_setup();
                    seed_rand();
                    return Args{{CAM}, {}};
                }, [](A a) { olt_cam_type21_init_l14(a.i[0]); return uint64_t{0}; });
                // the update: started by the init (the port's), then its keyframe, counters, last targets and ease varied
                l14check("p144_cam21_update", 0x00315d48, 400, Ret::None, [cam21_setup, CAM, ST, D] {
                    for (int attempt = 0; attempt < 10; ++attempt) {
                        cam21_setup();
                        const uint32_t sp0 = ee_stack_ptr();
                        try { olt_cam_type21_init_l14(CAM); break; } catch (const std::exception&) { ee_stack_ptr() = sp0; }
                    }
                    // (the keyframe: the first ones, near the list's end, or any)
                    int32_t last = 0;
                    while (last < 38 && ee_read<int32_t>(L14_ADDR(0x001f6488) + u(last + 1) * 0x30 + 0x14) != 0x2710) ++last;
                    const int k = ri(0, 3);
                    w32(ST + 0x8c, u(k == 0 ? ri(0, 2) : k == 1 ? (last > 0 ? last - 1 : 0) + ri(0, 1) : ri(0, last)));
                    w16(D + 0x30, u(ri(0, 4)));
                    w16(D + 0x32, u(ri(0, 2)));
                    if (coin()) { wf(ST + 0x90, coin() ? 0.0f : rf(-3.1f, 3.1f)); wf(ST + 0x94, coin() ? 0.0f : rf(-3.1f, 3.1f)); }
                    wf(ST + 0x98, rf(0.0f, 1.0f));
                    if (coin()) wfrand(ST + 0x74, 5, -0.2f, 0.2f);
                    if (coin()) wf(ST + 0x70, rf(-0.5f, 0.5f));  // (the turn about the up row: the keyframes' +0x10 are 0 on the level)
                    seed_rand();
                    return Args{{CAM}, {}};
                }, [](A a) { olt_cam_type21_update_l14(a.i[0]); return uint64_t{0}; });
                l14check("p144_cam21_exit", 0x003167e0, 60, Ret::None, [cam21_setup, CAM] {
                    cam21_setup();
                    seed_rand();
                    return Args{{CAM}, {}};
                }, [](A a) { olt_cam_type21_exit_l14(a.i[0]); return uint64_t{0}; });
                g_ignored_words.resize(n_ign);
            }
        }

        // ---- P14.5: the draw-list callbacks of classes 8 (oltanis_searchlight_beams.cpp), 211, 386, 610 (oltanis_draws.cpp) and
        // the level's copy of shadows_vu1_setup the beams' cone calls. The screen and projection set up, the camera dist away from
        // the moby (a random side, dz above it) and the view from it, the packet cursor on a buffer of the test area (0x800000 +
        // up to 8 quadwords, its end 0x8f0000), so that quad_draw / prim_list_packet write their packets there and they are
        // compared. VU0: none of these callbacks has a VU0 instruction of its own; the last VU0 writers in the original's run are
        // its matrix and drawing routines (mat4_mul, mat4_transform, quad_draw ...), whose ports keep no VU0 state, as in the
        // generated code before: no lane is compared (mat4_scale_diag's lanes, the cone's only VU0 hand-off, are batch 506's).
        {
            auto view_from = [](uint32_t m, float dist, float dz) {
                if (ee_read<uint16_t>(0x001518d0) == 0) { w16(0x001518d0, 640); w16(0x001518d2, 448); }
                screen_setup();
                projection_setup();
                if (m != 0) {
                    const Vec4 t = ee_ref<Vec4>(m + 0x10);
                    const float a = rf(-3.1f, 3.1f);
                    ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{t.x - dist * std::cos(a), t.y - dist * std::sin(a), t.z + dz, 1.0f};
                    wf(OV(0x00166dd8), a);
                    wf(OV(0x00166dd4), 0.1f);
                }
                render_view_setup();
                w32(OV(0x001611c0), 0x00800000u + u(ri(0, 8)) * 0x10u);
                w32(OV(0x0015f5f8), 0x008f0000u);
            };
            auto near_point = [](uint32_t at, uint32_t m, float r, float w) {  // a point within r of moby m, its w lane w
                const Vec4 c = ee_ref<Vec4>(m + 0x10);
                ee_ref<Vec4>(at) = Vec4{c.x + rf(-r, r), c.y + rf(-r, r), c.z + rf(-r, r), w};
            };
            auto unit_dir = [](uint32_t at) {
                const float a = rf(-3.1f, 3.1f), e = rf(-1.4f, 0.3f);
                ee_ref<Vec4>(at) = Vec4{std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e), coin() ? 0.0f : 1.0f};
            };
            const std::vector<uint32_t> drones = mobys_with(0x002ac618);
            // L14:0x20f9d0, the level's copy of shadows_vu1_setup (Veldin's 0x1fc740): the VU1 program's upload or not, the Z bias
            l14check("p145_shadows_vu1_setup", 0x0020f9d0, 40, Ret::None, [level, view_from, drones] {
                level();
                view_from(drones.empty() ? 0u : drones[static_cast<size_t>(ri(0, static_cast<int>(drones.size()) - 1))], rf(5.0f, 40.0f), rf(0.5f, 6.0f));
                w32(OV(0x0015f5e0), coin() ? 7u : u(ri(0, 6)));
                wf(OV(0x0015f308), coin() ? 0.0f : rf(-0.05f, 0.05f));
                return Args{};
            }, [](A) { shadows_vu1_setup(); return uint64_t{0}; });
            if (!drones.empty()) {
                // class 8's spot: its place near the drone, its size and pull, the alarm, the camera from on the spot (no pull)
                // to 40 away, so that the pull is the most, the distance less the margin, or 0
                l14check("p145_cb_2ae260", 0x002ae260, 300, Ret::None, [level, view_from, near_point, drones] {
                    level();
                    const uint32_t m = drones[static_cast<size_t>(ri(0, static_cast<int>(drones.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    near_point(pv + 0x90, m, 6.0f, rf(-2.0f, 2.0f));
                    w16(pv + 0x202, coin() ? 0u : u(ri(1, 3)));
                    wf(pv + 0x214, rf(0.3f, 4.0f));
                    wf(pv + 0x218, ri(0, 3) == 0 ? 0.0f : rf(0.5f, 12.0f));
                    view_from(m, rf(3.0f, 40.0f), rf(0.5f, 8.0f));
                    const int k = ri(0, 5);
                    if (k == 0) {  // the camera on the raised spot: no pull
                        Vec4 s = ee_ref<Vec4>(pv + 0x90);
                        s.z = fpu_add(s.z, u2f(0x3c23d70au));
                        s.w = 1.0f;
                        ee_ref<Vec4>(OV(0x00166dc0)) = s;
                    } else if (k == 1) {  // the camera about the margin away: the pull clamped at 0
                        const Vec4 s = ee_ref<Vec4>(pv + 0x90);
                        const float d = ee_read<float>(L14_ADDR(0x0016147c)) * rf(0.5f, 1.5f);
                        ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{s.x + d, s.y, s.z, 1.0f};
                    }
                    return Args{{m}, {}};
                }, [](A a) { olt_cb_2ae260_l14(a.i[0]); return uint64_t{0}; });

                // class 8's beams, from a drone as their master: 0 .. all of its group's drones drawn (never more: the original
                // then reads the null moby), each drone's glow / cone bits (+0x20c: 8, 2, both, none), its alarm, its glow's place and
                // angle, its beam's origin and direction, the drawn list left over from before; the camera among the group
                l14check("p145_cb_2aee28", 0x002aee28, 300, Ret::None, [level, view_from, near_point, unit_dir, drones] {
                    level();
                    const uint32_t m = drones[static_cast<size_t>(ri(0, static_cast<int>(drones.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    std::vector<uint32_t> group;
                    const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
                    if (uint32_t e = lvd::group(ee_read<uint8_t>(m + 0x21)); e != 0)
                        for (;; e += 2) {
                            group.push_back(mpool + ((ee_read<uint16_t>(e) & 0x7fffu) << 8));
                            if (ee_read<int16_t>(e) < 0) break;
                        }
                    w16(pv + 0x20a, group.empty() ? 0u : u(ri(0, static_cast<int>(group.size()))));
                    for (const uint32_t d : group) {
                        const uint32_t dp = ee_read<uint32_t>(d + 0x78);
                        if (ee_read<uint32_t>(d + 0x74) != 0x002ac618u) continue;  // (a member of another class: left as it is)
                        static const uint32_t bits[] = {0, 2, 8, 0xa, 0xb};
                        w32(dp + 0x20c, bits[ri(0, 4)]);
                        w16(dp + 0x202, coin() ? 0u : 1u);
                        wf(dp + 0x160, rf(-3.14f, 3.14f));
                        near_point(dp + 0x1d4, d, 3.0f, 0.0f);
                        near_point(dp + 0xd0, d, 2.0f, rf(-1.0f, 1.0f));
                        unit_dir(dp + 0xa0);
                    }
                    for (uint32_t i = 0; i < 20; ++i) w32(L14_ADDR(0x001d8680) + i * 4, coin() ? 0xffffffffu : R());
                    if (coin())  // the glow's colour and its fade per ring (the level's never reach 0 in 16 rings): to the 0 clamps
                        for (uint32_t k = 0; k < 4; ++k) {
                            w32(L14_ADDR(0x00161444) + k * 4, u(ri(0, 255)));
                            w32(L14_ADDR(0x00161454) + k * 4, u(ri(0, 40)));
                        }
                    view_from(group.empty() ? m : group[static_cast<size_t>(ri(0, static_cast<int>(group.size()) - 1))], rf(4.0f, 60.0f), rf(0.5f, 10.0f));
                    w32(OV(0x0015f5e0), coin() ? 7u : 0u);
                    return Args{{m}, {}};
                }, [](A a) { olt_cb_2aee28_l14(a.i[0]); return uint64_t{0}; });
            }
            // class 211's two lights: their places near the drone (the second's w lane as it comes), the camera around it
            if (const std::vector<uint32_t> ms = mobys_with(0x002d5f40); !ms.empty())
                l14check("p145_cb_2d71f0", 0x002d71f0, 200, Ret::None, [level, view_from, near_point, ms] {
                    level();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    near_point(pv + 0xe0, m, 2.0f, rf(-2.0f, 2.0f));
                    near_point(pv + 0x200, m, 2.0f, coin() ? 1.0f : rf(-2.0f, 2.0f));
                    view_from(m, rf(3.0f, 40.0f), rf(-2.0f, 8.0f));
                    return Args{{m}, {}};
                }, [](A a) { olt_cb_2d71f0_l14(a.i[0]); return uint64_t{0}; });
            // class 386's ribbons: the scroll about its wrap at -8, the clock at 60 or 50 Hz, the moby's heading and angles, its two
            // point lists along a random curve (distinct points)
            if (const std::vector<uint32_t> ms = mobys_with(0x002dee28); !ms.empty())
                l14check("p145_cb_2df458", 0x002df458, 200, Ret::None, [level, view_from, set_50hz, ms] {
                    level();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (ri(0, 2) == 0) set_50hz();
                    wf(pv + 0x1ec, coin() ? rf(-8.3f, -7.6f) : rf(-8.0f, 1.0f));
                    wf(m + 0x48, rf(-3.1f, 3.1f));
                    if (coin()) { wf(m + 0x40, rf(-0.5f, 0.5f)); wf(m + 0x44, rf(-0.5f, 0.5f)); }
                    for (uint32_t list = 0; list < 2; ++list) {
                        const uint32_t base = pv + list * 0xa0, n = list == 0 ? 10u : 20u;
                        Vec4 p{rf(-1.0f, 1.0f), rf(-1.0f, 1.0f), rf(-1.0f, 1.0f), 1.0f};
                        const float a = rf(-3.1f, 3.1f);
                        for (uint32_t i = 0; i < n; ++i) {
                            const float b = a + rf(-0.6f, 0.6f), step = rf(0.2f, 1.0f);
                            p.x += step * std::cos(b); p.y += step * std::sin(b); p.z += rf(-0.3f, 0.3f);
                            p.w = coin() ? 1.0f : rf(-1.0f, 1.0f);
                            ee_ref<Vec4>(base + i * 0x10) = p;
                        }
                    }
                    view_from(m, rf(4.0f, 40.0f), rf(-1.0f, 8.0f));
                    return Args{{m}, {}};
                }, [](A a) { olt_cb_2df458_l14(a.i[0]); return uint64_t{0}; });
            // class 610's sparks: each one's timer running or 0, its alpha, its place near the moby and its size (w)
            if (const std::vector<uint32_t> ms = mobys_with(0x002eaf88); !ms.empty())
                l14check("p145_cb_2eb810", 0x002eb810, 200, Ret::None, [level, view_from, near_point, ms] {
                    level();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const int lit = ri(0, 3);
                    for (uint32_t i = 0; i < 48; ++i) {
                        w16(pv + 0x320 + i * 2, ri(0, 3) < lit ? u(ri(1, 60)) : 0u);
                        w16(pv + 0x3e0 + i * 2, u(ri(0, 0x80)));
                        near_point(pv + 0x20 + i * 0x10, m, 3.0f, rf(0.05f, 2.0f));
                    }
                    view_from(m, rf(3.0f, 40.0f), rf(-1.0f, 8.0f));
                    return Args{{m}, {}};
                }, [](A a) { olt_cb_2eb810_l14(a.i[0]); return uint64_t{0}; });
            // class 684's bolt (the level's globals; the moby is not read): the points' and branches' timers about their ends and
            // past the duration (the alpha 0 and 255 clamps), the duration, the fade byte, the colours, widths and ends perturbed,
            // the branches' starts and offsets, 50 Hz (the duration is frame-scaled); the camera about the bolt's middle
            if (const std::vector<uint32_t> ms = mobys_with(0x002ed280); !ms.empty())
                l14check("p145_cb_2edc18", 0x002edc18, 150, Ret::None, [level, view_from, set_50hz, ms] {
                    level();
                    const uint32_t m = ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
                    if (ri(0, 2) == 0) set_50hz();
                    const int dur = ri(0, 3) == 0 ? ri(1, 4) : ri(10, 60);
                    w32(L14_ADDR(0x00161c70), u(dur));
                    w8(L14_ADDR(0x00161ce0), coin() ? 0u : u(ri(0, 300) & 0xff));
                    auto timer = [dur] { return u(ri(0, 4) == 0 ? ri(-3, 0) : ri(0, 4) == 0 ? ri(dur * 2, dur * 3) : ri(0, dur * 2)); };
                    for (uint32_t i = 0; i < 40; ++i) w16(L14_ADDR(0x001e0000) + i * 2, timer());
                    for (uint32_t i = 0; i < 48; ++i) w16(L14_ADDR(0x001e0350) + i * 2, timer());
                    if (coin()) { w32(L14_ADDR(0x00161c78), R()); w32(L14_ADDR(0x00161c7c), R()); }
                    if (coin()) { wf(L14_ADDR(0x00161c80), rf(0.0f, 1.0f)); wf(L14_ADDR(0x00161c84), rf(0.0f, 1.0f)); wf(L14_ADDR(0x00161c88), rf(0.5f, 3.0f)); }
                    for (uint32_t b = 0; b < 3; ++b) if (coin()) w32(L14_ADDR(0x00161cf8) + b * 4, u(ri(0, 38)));
                    if (coin()) for (uint32_t k = 0; k < 0x300; k += 0x10) { wf(L14_ADDR(0x001e0050) + k, rf(-0.5f, 0.5f)); wf(L14_ADDR(0x001e0050) + k + 8, rf(-0.5f, 0.5f)); }
                    // the ends (zero in the snapshot, set when the bolt strikes): its end at the moby, its start 5 .. 30 away (in a few
                    // cases left as they are)
                    Vec4 s = ee_ref<Vec4>(L14_ADDR(0x00161cc0)), e = ee_ref<Vec4>(L14_ADDR(0x00161cd0));
                    if (ri(0, 9) != 0) {
                        const Vec4 c = ee_ref<Vec4>(m + 0x10);
                        e = Vec4{c.x + rf(-1.0f, 1.0f), c.y + rf(-1.0f, 1.0f), c.z + rf(-1.0f, 1.0f), coin() ? 1.0f : 0.0f};
                        const float a = rf(-3.1f, 3.1f), el = rf(-0.3f, 1.4f), d = rf(5.0f, 30.0f);
                        s = Vec4{e.x + d * std::cos(el) * std::cos(a), e.y + d * std::cos(el) * std::sin(a), e.z + d * std::sin(el), e.w};
                        ee_ref<Vec4>(L14_ADDR(0x00161cc0)) = s;
                        ee_ref<Vec4>(L14_ADDR(0x00161cd0)) = e;
                        // the bolt's points in its frame (x along it from the end, zero in the snapshot too): a jagged line
                        for (uint32_t i = 0; i < 40; ++i)
                            ee_ref<Vec4>(L14_ADDR(0x001dfd80) + i * 0x10) = Vec4{d * static_cast<float>(i) / 39.0f, rf(-1.0f, 1.0f), rf(-1.0f, 1.0f), 1.0f};
                    }
                    if (std::getenv("B221_INFO") != nullptr) std::printf("INFO p145_cb_2edc18 start %.1f %.1f %.1f end %.1f %.1f %.1f\n", s.x, s.y, s.z, e.x, e.y, e.z);
                    // the camera from a side of the bolt's middle (a stand-in moby there for view_from)
                    const uint32_t stand = TEST_AREA + 0x7000;
                    ee_ref<Vec4>(stand + 0x10) = Vec4{(s.x + e.x) * 0.5f, (s.y + e.y) * 0.5f, (s.z + e.z) * 0.5f, 1.0f};
                    view_from(stand, rf(5.0f, 60.0f), rf(-5.0f, 10.0f));
                    return Args{{m}, {}};
                }, [](A a) { olt_cb_2edc18_l14(a.i[0]); return uint64_t{0}; });
            if (std::getenv("B221_INFO") != nullptr) {  // the play-test spots of P14.5's classes (index, uid, place, state)
                level();
                const uint32_t mpool = ee_read<uint32_t>(OV(0x0015ffd8));
                static const struct { const char* name; uint32_t fn; } cls[] = {{"searchlight 8", 0x002ac618}, {"rail drone 211", 0x002d5f40},
                                                                                {"class 386", 0x002dee28}, {"class 610", 0x002eaf88},
                                                                                {"class 684", 0x002ed280}};
                for (const auto& c : cls)
                    for (const uint32_t x : mobys_with(c.fn)) {
                        const Vec4 q = ee_ref<Vec4>(x + 0x10);
                        const uint32_t xp = ee_read<uint32_t>(x + 0x78);
                        std::printf("INFO P14.5 %s: moby %u uid %d at %.1f %.1f %.1f state %u group %u master %d beams %d bits %x\n", c.name,
                                    (x - mpool) >> 8, ee_read<int16_t>(x + 0xb2), q.x, q.y, q.z, ee_read<uint8_t>(x + 0x20), ee_read<uint8_t>(x + 0x21),
                                    c.fn == 0x002ac618 ? ee_read<int16_t>(xp + 0x1b6) : 0, c.fn == 0x002ac618 ? ee_read<int16_t>(xp + 0x20a) : 0,
                                    c.fn == 0x002ac618 ? ee_read<uint32_t>(xp + 0x20c) : 0u);
                    }
            }
        }

        g_ignored_words = batch_ignored;
        g_extra_noop_hooks.clear();
    }

    // ---- Y1: class 252's hook +0x10 (table 0x001fd9d0: Blarg's L6:0x2ea800, word for word; not Veldin's 0x2d5800, the overlay map's
    // pair) through the level's dispatch class_hook_call_level, the moby 0 in half, its pvars 0 in a sixth
    if (g_oracle_level == 14) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, OVF(0x0024d430)};
        auto chk = [](const char* nm, uint32_t a, int n, Ret r, const std::function<Args()>& g, const std::function<uint64_t(const Args&)>& p) {
            l14check(nm, a, n, r, g, p);
        };
        class_hook_deal_checks(chk, level, {0x001fd9d0u}, {{"y1_hook_2daed8", 0x2daed8, 0x10, 0, 0}}, 120);
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
