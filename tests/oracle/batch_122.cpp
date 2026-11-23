// Oracle batch 122 (task 0357): level 10's (Orxon) own moby classes on its real level data. Run with
// ORACLE_LEVEL=10 build/tests/ovl_oracle --batch 122 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 10's own function at its own address, the port the one registered for it (the level's classes
// written by hand, orxon_classes.h, the hand ports that replaced orxon_classes_tr_N.cpp, and the shared ports of the classes, hooks
// and camera functions other levels carry too: shared_*.cpp, novalis_*.cpp, kerwan_*.cpp, aridia_ocean.cpp). The
// "p10_" checks (task P10.1) drive the hand-written classes through Clank's part and the states the generic ones miss.

#include "ovl_oracle.h"
#include "class_hook_deal.h"

#include <game/ovl/helmet_tr.h>
#include <game/lib/libc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_bg_ships.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/shared_class1038.h>
#include <game/ovl/shared_class1068.h>
#include <game/ovl/shared_class1302.h>
#include <game/ovl/shared_class1378.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_class819.h>
#include <game/ovl/shared_class827.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class857.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_puffer.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>
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
constexpr uint32_t MOBY_SPAWN_L10 = 0x002412b0;

// (stage 5, task C2a) The checks named here reach mobys_in_sphere (the level's copy), whose cell list and vertices the port keeps on
// the stack (the original: 0x70003800, 0x70002000): A2's collision scratch ranges are left out of these checks, and only of
// them. They also hide what moby_pose_matrices (called for a posed moby) writes inside them: the joint positions at
// 0x70000000 .., read back by the primitive tests, so a wrong one still shows in the hit list.
// (Task C2b: the checks added to the list reach sphere_collide or capsule_collide, whose chunk buffers,
// vertices and cell walk the port keeps on the host stack too.)
bool c2a_spr_check(const char* name) {
    static const char* const kNames[] = {"class794", "class1031", "class1068", "class1199", "mobys_in_sphere", "class1196",
                                         "class1202", "class827", "class857", "p10_class1031", "p10_class1090",
                                         "p10_class1196", "p10_class1199", "p10_class1202", "p10_class794", "p10_class938",
                                         "class1229", "class1544", "class351", "class737", "class805", "p10_class1229",
                                         "p10_class1544", "p10_class22", "p10_class351", "p10_class859"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

// A check whose original is level 10's own function at `l9addr` (not a Veldin address to translate).
void l10check(const char* name, uint32_t l9addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B122_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 10) {
        std::printf("%-26s L10:0x%08x  skipped (level 10 only: run with ORACLE_LEVEL=10)\n", name, l9addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L10:0x%08x\n", name, l9addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2a_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l9addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_122() {
    if (g_only_batch != 122) return;
    g_batch = 122;
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
    // the level's mobys whose update function is `fn` (level 10's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 10) { l10check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L10:0x%08x  no moby on this level\n", name, fn); return; }
        l10check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    if (g_oracle_level == 10) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) g_ignored_words.push_back(pool + k * 0x40 + 0x2c);
        }
    }

    // ---- the level's own classes (once transliterated by tools/codegen/mkall_l10.py; hand ports now): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; uint32_t skip_p0 = 0; uint32_t skip_p1 = 0; };  // (skip_p0 / skip_p1: states skipped in pass 0 / 1 only, see docs/reversing/orxon_classes.md)  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class1033", 0x00295a38, orx_class1033_update_l10, 0x0, false},
        {"class1117", 0x00295c20, orx_class1117_update_l10, 0x0, false},
        {"class18", 0x00298668, orx_class18_update_l10, 0x4, false},
        {"class22", 0x00298b68, orx_class22_update_l10, 0x0, false},
        {"class87", 0x0029e1c8, empty_update, 0x0, false},
        {"class258", 0x002bae38, class258_update, 0x0, false},
        {"class302", 0x002bd0e8, class302_update, 0x0, false},
        {"class303", 0x002bd768, class303_update, 0x0, false},
        {"class351", 0x002be858, orx_class351_update_l10, 0x0, false},
        {"class353", 0x002befe8, orx_class353_update_l10, 0x0, false},
        {"class532", 0x002c4bc8, empty_update, 0x0, false},
        {"class605", 0x002c6470, zone_marker_update, 0x0, false},
        {"class648", 0x002c74b8, puffer_update, 0x0, false},
        {"class702", 0x002c7a20, orx_class702_update_l10, 0x0, false},
        {"class737", 0x002c7cf8, scripted_camera_trigger_update_l1, 0x0, false},
        {"class758", 0x002c9590, swingshot_target_update, 0x0, false},
        {"class794", 0x002c9c70, orx_class794_update_l10, 0x0, false},
        {"class805", 0x002ca220, spawn_point_update_l1, 0x0, false},
        {"class819", 0x002cc2e8, class819_update, 0x0, false},
        {"class827", 0x002cc8b8, class827_update, 0x0, true, 0x10, 0x0},
        {"class832", 0x002cea78, class832_update, 0x0, false},
        {"class857", 0x002d4078, class857_update, 0x0, false},
        {"class859", 0x002d7b80, orx_class859_update_l10, 0x0, false},
        {"class915", 0x002d7fe8, fred_update_l3, 0x0, false},
        {"class938", 0x002d85c8, orx_class938_update_l10, 0x0, false},
        {"class939", 0x002d8c00, orx_class939_update_l10, 0x0, false},
        {"class947", 0x002d8d30, orx_class947_update_l10, 0x0, false},
        {"class1015", 0x002d90a8, orx_class1015_update_l10, 0x0, false},
        {"class1031", 0x002d92b8, orx_class1031_update_l10, 0x0, false},
        {"class1038", 0x002d9550, class1038_update, 0x0, false},
        {"class1040", 0x002d9bf8, class1040_update, 0x0, false},
        {"class1047", 0x002d9eb8, orx_class1047_update_l10, 0x0, false},
        {"class1067", 0x002da2c8, orx_class1067_update_l10, 0x0, false},
        {"class1068", 0x002dabe8, class1068_update, 0x0, false},
        {"class1073", 0x002dcc58, orx_class1073_update_l10, 0x0, false},
        {"class1085", 0x002dd010, beam_shard_update, 0x0, false},
        {"class1090", 0x002dd3d8, orx_class1090_update_l10, 0x0, false},
        {"class1100", 0x002dd650, orx_class1100_update_l10, 0x0, false},
        {"class1122", 0x002dd8d8, empty_update, 0x0, false},
        {"class1134", 0x002dd8e0, gold_bolt_update, 0x4, false},
        {"class1141", 0x002de860, lift_update_l1, 0x0, false},
        {"class1196", 0x002df270, orx_class1196_update_l10, 0x0, false},
        {"class1199", 0x002e01a8, orx_class1199_update_l10, 0x0, false},
        {"class1202", 0x002e1d38, orx_class1202_update_l10, 0x0, false},
        {"class1229", 0x002e4a88, orx_class1229_update_l10, 0x0, false},
        {"class1240", 0x002e5be0, orx_class1240_update_l10, 0x0, false},
        {"class1258", 0x002e6a78, shell_update, 0x0, false},
        {"class1302", 0x002e7cd8, class1302_update, 0x0, false},
        {"class1326", 0x002e8358, orx_class1326_update_l10, 0x2, false},
        {"class1344", 0x002e85b8, orx_class1344_update_l10, 0x0, false},
        {"class1346", 0x002e8ab0, orx_class1346_update_l10, 0x0, false},
        {"class1378", 0x002e9028, class1378_update, 0x0, false},
        {"class1421", 0x002e9648, orx_class1421_update_l10, 0x0, false},
        {"class1424", 0x002e9990, orx_class1424_update_l10, 0x0, false},
        {"class1544", 0x002ea1f0, orx_class1544_update_l10, 0x0, false},
        {"class1555", 0x002eacd8, orx_class1555_update_l10, 0x0, false},
        {"class1806", 0x002eb1b0, rock_update, 0x0, false},
        {"class1855", 0x002eb6f0, orx_class1855_update_l10, 0x0, false},
        {"class1856", 0x002eb7e0, orx_class1856_update_l10, 0x0, false},
        {"class1857", 0x002eb8b8, empty_update, 0x0, false},
        {"class1921", 0x002eb950, empty_update, 0x0, false},
        {"class1973", 0x002ebb08, bg_ship2_update, 0x0, false},
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
    if (g_oracle_level == 10 && std::getenv("B122_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B122_CLS") != nullptr && std::strcmp(std::getenv("B122_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 10 && std::getenv("B122_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B122_ST") == nullptr || std::atoi(std::getenv("B122_ST")) == st) && (std::getenv("B122_PASS") == nullptr || std::atoi(std::getenv("B122_PASS")) == pass) && (c.skip_states & (1u << st)) == 0 && !(pass == 0 && (c.skip_p0 & (1u << st)) != 0) && !(pass == 1 && (c.skip_p1 & (1u << st)) != 0)) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- level 10's own moby callbacks (draw callbacks installed by `jal 0x1ecc00 / 0x1eca48`): a0 = the moby; the subjects are
    // the mobys of the class whose code installs it, after one update of the class in a state, a hit, the hero near
    if (g_oracle_level == 10) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};
        struct Cb { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t cls_addr; int cls; };
        static const Cb cbs[] = {
        {"cb2bd280", 0x002bd280, class302_draw, 0x002bd0e8, 302},
        {"cb2ce868", 0x002ce868, class832_draw, 0x002cea78, 832},
        {"cb2d7808", 0x002d7808, class857_draw, 0x002d4078, 857},
        {"cb2da690", 0x002da690, orx_cb_2da690_l10, 0x002da2c8, 1067},
        {"cb2dc698", 0x002dc698, beam_draw, 0x002dabe8, 1068},
        {"cb2e7520", 0x002e7520, class1302_draw, 0x002e7cd8, 1302},
        {"cb2e91b8", 0x002e91b8, class1378_draw, 0x002e9028, 1378},
        {"cb2eb958", 0x002eb958, ari_bg_ship2_glow_draw_l2, 0x002ebb08, 1973},
        };
        for (const Cb& cb : cbs) {
            if (std::getenv("B122_CLS") != nullptr && std::strcmp(std::getenv("B122_CLS"), cb.name) != 0) continue;
            level();
            std::vector<uint32_t> subj;
            if (cb.cls_addr != 0) subj = mobys_with(cb.cls_addr);
            else for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) if (ee_read<uint8_t>(m + 0x20) < 0xfe) subj.push_back(m);
            void (*clsfn)(uint32_t) = nullptr;
            for (const TrCls& c : trcls) if (c.addr == cb.cls_addr) clsfn = c.fn;
            if (subj.empty()) { std::printf("%-26s L10:0x%08x  no moby on this level\n", cb.name, cb.addr); continue; }
            for (int st = 0; st < 4; ++st)
            for (int pass = 0; pass < 3; ++pass) {
                if (clsfn == nullptr && (st != 0 || pass != 0)) continue;
                classcheck(cb.name, cb.addr, cb.fn, [st, pass, clsfn, generic_prep](uint32_t m) { generic_prep(m, st, pass, clsfn != nullptr ? clsfn : orx_class1033_update_l10, false); }, 12, subj);
            }
        }
        g_extra_noop_hooks.clear();
    }

    // ---- level 10's sound emitter type 3 (section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 10) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter3", 0x002f6e58, emitter_reverb_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L10:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B122_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l10check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 10's class hooks (class 827's table at 0x1dde58): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 10) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};
        // (hooks +0 .. +0xc: the shared ports of class 827's carry hooks, shared_class827.cpp; +0x10: Blarg's L6:0x2ea800's port, Y1)
        struct Hk { const char* name; uint32_t addr; uint32_t slot; int32_t (*fn)(uint32_t, uint32_t, uint32_t, float); };
        static const Hk hks[] = {
            {"hook_2ce618", 0x2ce618, 0x0, class827_hook_carry0},
            {"hook_2ce6e0", 0x2ce6e0, 0x4, [](uint32_t m, uint32_t a1, uint32_t a2, float f12) { return class827_hook_carry(m, a1, a2, f12, 4); }},
            {"hook_2ce770", 0x2ce770, 0x8, [](uint32_t m, uint32_t a1, uint32_t a2, float f12) { return class827_hook_carry(m, a1, a2, f12, 8); }},
            {"hook_2ce800", 0x2ce800, 0xc, class827_hook_carryc},
            {"hook_2ce838", 0x2ce838, 0x10, [](uint32_t m, uint32_t a1, uint32_t a2, float f12) {  // (Y1: the level's dispatch, L6:0x2ea800's port)
                 return static_cast<int32_t>(hook_dispatched<0x2ce838>(m, a1, a2, 0, 0, 0, 0, 0, f12, 0, 0, 0, 0, 0, 0, 0).v0); }},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L10:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B122_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l10check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                const uint32_t m = own[static_cast<size_t>(ri(0, static_cast<int>(own.size()) - 1))];
                if (ri(0, 4) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                const uint32_t tgt = ri(0, 3) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
                return Args{{m, ri(0, 2) == 0 ? 0x0013f3d0u : m + 0x10, tgt}, {rf(-1, 3)}};
            }, [fn](A a) { return r32(static_cast<uint32_t>(fn(a.i[0], a.i[1], a.i[2], a.f[0]))); });
        }
        g_extra_noop_hooks.clear();
    }

    // ---- moby_spawn (the Veldin port) against level 10's own copy (L10:0x...): the classes' spawns all go through it
    if (g_oracle_level == 10) {
        l10check("moby_spawn", MOBY_SPAWN_L10, 60, Ret::Int32, [level] {
            level();
            static const uint32_t cls[] = {0x12, 0x16, 0x102, 0x25d, 0x359, 0x42c, 0x4cd};  // (classes of level 10's own table: 18, 22, 258, 605, 857, 1068, 1229)
            if (ri(0, 2) == 0) w32(OV(0x0015ffe0) , ee_read<uint32_t>(OV(0x0015ffe0)));
            return Args{{cls[ri(0, 6)]}, {}};
        }, [](A a) { return r32(moby_spawn(a.i[0])); });
    }

    // ---- level 10's camera hook (0x2f4b18, the pointer its setter 0x2f4ae0 puts at 0x167490) and the callback it queues (0x2f4b78)
    if (g_oracle_level == 10) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = OV(0x00167010) - 0x10;  // (Veldin 0x167010 is the hook pointer: S + 0x10)
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002f4b18u);
            wf(0x00161f3c, rf(0, 0.5f));  // $gp - 0x4cc4: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));  // the queue's count (it holds up to 16)
        };
        l10check("cam0_hook", 0x002f4b18, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l10check("cam_callback", 0x002f4b78, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- level 10's camera type 0x13 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 10) {
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
        static const CamT cams[] = {{19, 0x002f65f8, 0x002f5b58, 0x002f5f18, 0x002f66c0}};
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
            l10check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(cam_type19_accept(a.i[0], a.i[1])) : 0u); });
            l10check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) cam_type19_init(a.i[0]); return uint64_t{0}; });
            l10check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) cam_type19_update(a.i[0]); return uint64_t{0}; });
            l10check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- mobys_in_sphere (Veldin 0x1f2868, level 10's copy L10:0x001e5ed0) on the level's own mobys: class 794 (the Gadgebot's helper) calls it and one of its random states differed
    if (g_oracle_level == 10) {
        l10check("mobys_in_sphere", 0x001e5ed0, 600, Ret::Int32, [level] {
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

    // ---- libc_memcpy (the base ELF's memcpy 0x115248, ported by this task: class 794 calls it). Overlapping and unaligned ranges included.
    if (g_oracle_level == 10) {
        l10check("libc_memcpy", 0x00115248, 400, Ret::Int32, [] {
            const uint32_t base = 0x00180000u;
            for (uint32_t k = 0; k < 0x200; k += 4) w32(base + k, R());
            const uint32_t n = ri(0, 3) == 0 ? u(ri(0, 0x1f)) : u(ri(0, 0xa0));
            auto pick = [&] { return base + 0x40 + (ri(0, 1) != 0 ? 0x10 * u(ri(0, 8)) : u(ri(0, 0x80))); };
            return Args{{pick(), pick(), n}, {}};
        }, [](A a) { return r32(libc_memcpy(a.i[0], a.i[1], a.i[2])); });
    }

    // T00: the level's copy of Veldin's helmet_init_fx (0x2e06a8; the scan's body hash pairs them, the overlay map does not) at
    // L10:0x00298860: the hand port runs in its place; a live moby's pvars +0x10 .. +0x4c set up (the random bytes, the timers by
    // the frame scale)
    if (g_oracle_level == 10) {
        l10check("helmet_init_fx_298860", 0x00298860, 120, Ret::None, [level] {
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
    if (g_oracle_level == 10) {
        static const uint32_t empties[] = {0x0029e1c8, 0x002c4bc8, 0x002dd8d8, 0x002eb8b8, 0x002eb950, 0x002f66c0};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l10check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- task P10.1 (2026-11-14): the hand ports of 16 of the level's own classes (orxon_platforms / doors / clank / hazards
    // / props.cpp) driven where the generic generator above does not take them: Clank (the hero's form 1) and Ratchet, the
    // game's bytes the classes read (Clank's part done 0x13d4c6, the O2 Mask taken 0x13d504, the scene state, the frame
    // counter, the frame scale at its 50 Hz value), the classes' one-shot flags and "gone for good" bytes, the hero on /
    // near / inside the subject, timers at their edges, and the classes the level places no moby of (859, 938) spawned.
    // B122_P10=<class> runs one of them (e.g. B122_P10=class351).
    if (g_oracle_level == 10) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};
        const uint32_t P = 0x0013f350;
        auto moby_at = [](int32_t i) { return ee_read<uint32_t>(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); };
        auto hero_at = [P](const Vec4& q) {  // the hero and his moby at q
            ee_ref<Vec4>(P + 0x80) = q;
            const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
            if (hm != 0) ee_ref<Vec4>(hm + 0x10) = q;
        };
        auto flag_set = [](uint32_t m, int how) {  // the one-shot flag of the moby's index: 0 clear, 1 the level's byte, 2 the bit
            const int32_t idx = ee_read<int16_t>(m + 0xb2);
            const uint32_t byte = OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454;
            const uint32_t word = 0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
            const uint32_t bit = 1u << (ee_read<uint16_t>(m + 0xb2) & 0x1f);
            w8(byte, how == 1 ? u(ri(1, 4)) : 0u);
            w32(word, how == 2 ? (ee_read<uint32_t>(word) | bit) : (ee_read<uint32_t>(word) & ~bit));
        };
        auto gone_set = [](uint32_t m, bool gone) {
            const uint32_t b = ee_read<uint8_t>(m + 0xb0);
            if (b == 0xff) return;
            w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + b, gone ? 0xffu : u(ri(0, 3)));
        };
        // the game's switches every case deals
        auto world = [flag_set, gone_set](uint32_t m) {
            w8(0x001413f4, ri(0, 2) == 0 ? 0u : 1u);  // (the hero's form: Clank in two thirds)
            w8(0x0013d4c6, coin() ? 1u : 0u);         // (Clank's part done)
            w8(0x0013d504, ri(0, 3) == 0 ? 1u : 0u);  // (the O2 Mask taken)
            { static const uint32_t sc[] = {0, 0, 1, 2}; w32(OV(0x0015f5c4), sc[ri(0, 3)]); }
            if (ri(0, 2) == 0) w32(OV(0x0015f5cc), 60u * u(ri(0, 400)));
            else w32(OV(0x0015f5cc), u(ri(0, 30000)));
            if (ri(0, 3) == 0) wf(0x0015ed68, 1.2f);
            flag_set(m, ri(0, 3) == 0 ? 1 : (ri(0, 2) == 0 ? 2 : 0));
            gone_set(m, ri(0, 2) == 0);
            w32(0x001415f8, ri(0, 4) == 0 ? 0u : 100u);
        };
        auto near = [](uint32_t m, float r, float dz) {  // a point within r (plane) / dz (height) of the moby
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float d = rf(0, r), a = rf(-3.1f, 3.1f);
            q.x += d * std::cos(a); q.y += d * std::sin(a); q.z += rf(-dz, dz);
            return q;
        };
        // a spawned moby of class `cls` (the level places none): the same slot every case (the level's state restored)
        auto spawned = [level](uint32_t cls) { level(); const uint32_t m = moby_spawn(cls); return m == 0 ? std::vector<uint32_t>{} : std::vector<uint32_t>{m}; };
        auto respawn = [](uint32_t cls, uint32_t m) {
            const uint32_t s = moby_spawn(cls);
            if (s != m) std::printf("P10 respawn: class 0x%x spawned at 0x%08x, not 0x%08x\n", cls, s, m);
        };
        auto live_moby = [](uint32_t not_this) {  // a live moby of the level, not `not_this`
            for (int k = 0; k < 50; ++k) {
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), last = ee_read<uint32_t>(OV(0x0015ffe0));
                const uint32_t o = first + 0x100 * u(ri(0, static_cast<int32_t>((last - first) >> 8) - 1));
                if (o != not_this && ee_read<uint8_t>(o + 0x20) < 0xfe) return o;
            }
            return 0u;
        };
        auto hit_with = [](uint32_t m, uint32_t kinds) {  // a hit on the moby (mask `kinds` among others) by a live moby
            const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
            w8(m + 0xa4, idx);
            w32(e + 0x34, m);
            w32(e + 0x24, kinds | (R() & 0xff00ffu));
            wf(e + 0x2c, rf(0.5f, 8));
            wfrand(e, 4, -1, 1);
            w32(e + 0x20, coin() ? ee_read<uint32_t>(0x0013f350 + 0x2080) : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 60)));
            w8(e + 0x28, coin() ? 0u : u(ri(1, 4)));
        };
        auto own_slot = [](uint32_t m, uint32_t slot_word) {  // the subject's sound slot (if any) its own and playing, or not
            const int32_t s = ee_read<int32_t>(slot_word);
            if (s < 0 || s > 0x40) return;
            const uint32_t e = 0x0013e550 + static_cast<uint32_t>(s) * 0x70;
            w32(e + 0x88, coin() ? m : 0u);
            w8(e + 0x74, coin() ? 1u : 0u);
        };
        // (P10.3) a point of box `idx` of the level (its frame +0x40 maps the box onto -1 .. 1, its centre +0x30) at the local
        // coordinates (lx, ly, lz): inside for |l| < 1
        auto box_point = [](int32_t idx, float lx, float ly, float lz) {
            const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + (u(idx) << 7);
            Vec4 q = ee_ref<Vec4>(box + 0x30);
            const Mat4& f = ee_ref<Mat4>(box + 0x40);
            Vec4 a[3];
            for (int j = 0; j < 3; ++j) {
                Vec4 e{0, 0, 0, 0};
                (j == 0 ? e.x : j == 1 ? e.y : e.z) = 1.0f;
                mat3_transform(a[j], e, f);
            }
            const float t[3] = {lx, ly, lz};
            auto det = [](const float c0[3], const float c1[3], const float c2[3]) {
                return c0[0] * (c1[1] * c2[2] - c1[2] * c2[1]) - c1[0] * (c0[1] * c2[2] - c0[2] * c2[1]) + c2[0] * (c0[1] * c1[2] - c0[2] * c1[1]);
            };
            const float c[3][3] = {{a[0].x, a[0].y, a[0].z}, {a[1].x, a[1].y, a[1].z}, {a[2].x, a[2].y, a[2].z}};
            const float dd = det(c[0], c[1], c[2]);
            if (dd != 0.0f) {
                q.x += det(t, c[1], c[2]) / dd;
                q.y += det(c[0], t, c[2]) / dd;
                q.z += det(c[0], c[1], t) / dd;
            }
            return q;
        };
        // (P10.3) a point of box `idx`: inside it (2 in 3) or just outside
        auto in_or_out = [box_point](int32_t idx) {
            if (ri(0, 2) != 0) return box_point(idx, rf(-0.9f, 0.9f), rf(-0.9f, 0.9f), rf(-0.9f, 0.9f));
            return box_point(idx, coin() ? rf(1.05f, 1.6f) : rf(-1.6f, -1.05f), rf(-0.9f, 0.9f), rf(-0.9f, 0.9f));
        };
        // (P10.3) point `i` of path `p` of the level (0 when there is none)
        auto path_pt = [](int32_t p, int32_t i) -> uint32_t {
            if (p < 0 || p >= ee_read<int32_t>(OV(0x00160104))) return 0u;
            const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4);
            const int32_t n = ee_read<int32_t>(path);
            if (n <= 0) return 0u;
            return path + 0x10 + u(std::min(std::max(i, 0), n - 1)) * 16;
        };
        auto path_len = [](int32_t p) -> int32_t {
            if (p < 0 || p >= ee_read<int32_t>(OV(0x00160104))) return 0;
            return ee_read<int32_t>(ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4));
        };
        // (P10.3) the moby's animation time set about `t` (its frames' start +0x54 on, another animation before +0x52)
        auto anim_time = [](uint32_t m, uint32_t anim, float t) {
            w8(m + 0x53, anim);
            w8(m + 0x52, anim);
            w8(m + 0x51, 0);
            w8(m + 0x50, 1);
            const uint32_t cur = ee_read<uint32_t>(m + 0x68);
            if (cur == 0) return;
            const float off = t - static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f;
            wf(m + 0x54, off != 0.0f ? off : 0.01f);
        };
        // (P10.3) the camera (0x166dc0) moved about `at` (xyz, its radius in w) until the port's screen test (0x2002e0) sees it with radius r,
        // the frame's view matrices made from it (render_view_setup: they are zero after an update alone, as batch 128 has it)
        auto camera_sees = [](const Vec4& at, float r, float spread) {
            for (int k = 0; k < 100; ++k) {
                ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{at.x + rf(-spread, spread), at.y + rf(-spread, spread), at.z + rf(-spread / 4, spread / 4), 1.0f};
                render_view_setup();
                ee_ref<Vec4>(T4) = at;
                if (point_on_screen(T4, r) != -1) return true;
            }
            return false;
        };
        struct P10 { const char* name; uint32_t addr; void (*fn)(uint32_t); int states; std::function<void(uint32_t, int)> vary; uint32_t spawn_cls = 0; };
        const std::vector<P10> p10 = {
            {"class1033", 0x00295a38, orx_class1033_update_l10, 5, [](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (pv == 0) return;
                 w32(pv + 0xb0, u(ri(0, 3)));
                 if (coin()) wf(m + 0x18, ee_read<float>(pv + 0xa8) + rf(-0.1f, 1.6f));
             }},
            {"class1117", 0x00295c20, orx_class1117_update_l10, 4, [P](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (coin()) { w32(P + 0x2fc, m); w16(P + 0x30e, 0); w32(P + 0x208c, 0); w32(P + 0x2084, 0); }
                 w32(pv + 0xbc, u(ri(-1, 3)));
                 wf(pv + 0xa4, ri(0, 3) == 0 ? 0.0f : (coin() ? rf(-0.4f, 0.4f) : rf(-2.5f, 2.5f)));
                 if (coin()) wf(m + 0x18, ee_read<float>(pv + 0xa0) - rf(-0.05f, 1.5f));
             }},
            {"class18", 0x00298668, orx_class18_update_l10, 4, [hero_at, near](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (coin()) hero_at(near(m, 4, 3));
                 if (pv != 0 && coin()) w32(pv + 4, coin() ? 0xffffffffu : u(ri(0, 60)));
             }},
            {"class22", 0x00298b68, orx_class22_update_l10, 4, [](uint32_t m, int) {  // (the class has no states: 4 x 40 cases)
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (coin()) {  // its two spawn points: two of the level's class 805 mobys (or one of another class)
                     const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), last = ee_read<uint32_t>(OV(0x0015ffe0));
                     std::vector<int32_t> pts;
                     for (uint32_t o = first; o < last; o += 0x100) if (ee_read<int16_t>(o + 0xa6) == 0x325 && ee_read<uint8_t>(o + 0x20) < 0xfe) pts.push_back(static_cast<int32_t>((o - first) >> 8));
                     if (!pts.empty()) {
                         w32(pv + 0x18, u(pts[static_cast<size_t>(ri(0, static_cast<int>(pts.size()) - 1))]));
                         w32(pv + 0x1c, ri(0, 4) == 0 ? u(ri(0, 40)) : u(pts[static_cast<size_t>(ri(0, static_cast<int>(pts.size()) - 1))]));
                     }
                 }
                 w32(ORACLE_LADDR(10, 0x00161388), coin() ? 1u : 0u);
                 w16(0x00141848 + 0x100, coin() ? u(ri(0, 400)) : 0xffffu);
                 w16(0x00141848 + 0x102, u(ri(0, 3)));
                 w32(0x0015eea4, u(ri(0, 50000)));
                 if (ri(0, 4) == 0) w32(pv + 8, 0xffffffffu);
                 if (ri(0, 4) == 0) w32(pv, 0xffffffffu);
                 if (ri(0, 3) == 0) w32(pv + 0x14, 0xffffffffu);
                 if (ri(0, 5) == 0) w32(pv + 0x18 + 4 * u(ri(0, 1)), 0xffffffffu);
             }},
            {"class351", 0x002be858, orx_class351_update_l10, 2, [hero_at, near](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (ri(0, 3) != 0) hero_at(near(m, 0.6f, 1));
                 if (ri(0, 5) == 0) w32(pv, 0xffffffffu);
                 w32(ORACLE_LADDR(10, 0x001618a8), ri(0, 2) == 0 ? 0u : u(ri(1, 200)));
                 w32(ORACLE_LADDR(10, 0x001618a4), u(ri(1, 200)));
                 w32(ORACLE_LADDR(10, 0x001618a0), u(ri(1, 200)));
                 const uint32_t cam = ee_read<uint32_t>(OV(0x00166e00));
                 if (cam != 0) w16(cam + 0x86, coin() ? 0u : 1u);
             }},
            {"class353", 0x002befe8, orx_class353_update_l10, 3, [gone_set](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (coin()) wf(m + 0x14, ee_read<float>(pv + 4) - 3.75f + rf(-0.3f, 0.6f));
                 w8(m + 0xb0, ri(0, 3) == 0 ? 0xffu : u(ri(0, 15)));  // (the level's own moby has 0xff)
                 const uint32_t b = ee_read<uint8_t>(m + 0xb0);
                 if (b != 0xff) { w8(OV(0x0015fc88) + b, coin() ? 0xffu : 0u); gone_set(m, coin()); }
                 if (ri(0, 4) == 0) w32(0x0015ed84, 9);  // (another level's number: the byte 0x13d3d8 stays)
             }},
            {"class702", 0x002c7a20, orx_class702_update_l10, 2, [own_slot](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 w32(pv + 0xc, u(ri(0, 3)));
                 w32(pv + 0x10, ri(0, 3) == 0 ? 0xffffffffu : u(ri(0, 5)));
                 own_slot(m, pv + 0x10);
             }},
            {"class794", 0x002c9c70, orx_class794_update_l10, 4, [moby_at, gone_set, hero_at, near](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 w32(pv + 8, R() & 0x1ffu);
                 w32(pv + 0x40, u(ri(0, 3)));
                 if (coin()) { w8(m + 0xb0, u(ri(0, 15))); gone_set(m, ri(0, 3) == 0); }  // (the level's own have 0xff)
                 const bool all = coin();  // every watched moby at its state
                 const uint32_t words = 0x0014c190 + (ee_read<uint32_t>(0x0015ed84) << 8);
                 for (uint32_t k = 0; k < 5; ++k) {  // the watched mobys at their states or short of them, gone, flagged
                     const uint32_t w = pv + 0x18 + k * 8;
                     const int32_t i = ee_read<int32_t>(w);
                     if (i == -1) continue;
                     const uint32_t o = moby_at(i);
                     if (!all && ri(0, 5) == 0) w8(w + 6, coin() ? 0xffu : u(ri(0, 4)));
                     const int8_t want = static_cast<int8_t>(ee_read<uint8_t>(w + 6));
                     if ((all || coin()) && want >= 0) w8(o + 0x20, static_cast<uint32_t>(want));
                     else if (ri(0, 3) == 0) w8(o + 0x20, ri(0, 3) == 0 ? (coin() ? 0xfeu : 0xfdu) : u(ri(0, 4)));
                     if (!all && ri(0, 6) == 0) w16(w + 4, ee_read<uint16_t>(w + 4) ^ 1u);
                     const int32_t idx = ee_read<int16_t>(o + 0xb2);
                     const uint32_t fw = words + (static_cast<uint32_t>(idx >> 5) << 2), bit = 1u << (ee_read<uint16_t>(o + 0xb2) & 0x1f);
                     w32(fw, !all && ri(0, 5) == 0 ? (ee_read<uint32_t>(fw) | bit) : (ee_read<uint32_t>(fw) & ~bit));
                 }
                 if (ri(0, 3) == 0) w8(pv + 0x16, coin() ? 0xffu : u(ri(0, 4)));
                 if (ri(0, 2) == 0) w8(pv + 0x17, coin() ? 0xffu : u(ri(0, 4)));
                 const int32_t pi = ee_read<int32_t>(pv + 0x10);
                 if (pi != -1 && coin()) w8(moby_at(pi) + 0x20, ee_read<uint8_t>(pv + 0x17));
                 if (ri(0, 6) == 0) w32(pv + 0x10, 0xffffffffu);
                 if (coin()) hero_at(near(m, 3, 1));  // (the hero near the box: its push)
             }},
            {"class859", 0x002d7b80, orx_class859_update_l10, 2, [live_moby, near, P](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                 Vec4 q = near(hm != 0 ? hm : m, 6, 1);
                 if (ri(0, 5) == 0) q.x = coin() ? rf(0, 3) : rf(508, 512);
                 ee_ref<Vec4>(m + 0x10) = q;
                 wfrand(pv, 3, -0.6f, 0.6f);
                 if (hm != 0 && ri(0, 2) == 0) {  // flying through the hero (his moby's own place): the shot hits him
                     Vec4 t = ee_ref<Vec4>(hm + 0x10), v = ee_ref<Vec4>(pv);
                     t.z += rf(0.3f, 1.2f);
                     ee_ref<Vec4>(m + 0x10) = Vec4{t.x - 0.5f * v.x, t.y - 0.5f * v.y, t.z - 0.5f * v.z, t.w};
                 }
                 w16(pv + 0x10, u(ri(0, 3) == 0 ? ri(0, 1) : ri(2, 40)));
                 w16(pv + 0x12, u(ri(0, 4)));
                 w32(pv + 0x14, coin() ? 0u : T4 + 0x40);
                 wf(pv + 0x18, rf(-0.3f, 0.3f));
                 wf(pv + 0x1c, rf(0.2f, 2.0f));
                 w32(pv + 0x20, coin() ? 0u : live_moby(m));
                 w8(T4 + 0x40, 0);
             }, 0x35b},
            {"class938", 0x002d85c8, orx_class938_update_l10, 5, [hit_with, near, P](uint32_t m, int st) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 Vec4 q = near(ee_read<uint32_t>(P + 0x2080) != 0 ? ee_read<uint32_t>(P + 0x2080) : m, 5, 1);
                 if (ri(0, 5) == 0) q.z = rf(0.5f, 3);
                 ee_ref<Vec4>(m + 0x10) = q;
                 wfrand(pv, 3, -0.5f, 0.5f);
                 const uint32_t hm2 = ee_read<uint32_t>(P + 0x2080);
                 const int how = hm2 != 0 ? ri(0, 2) : 2;
                 if (how == 0) {  // flying through the hero's moby (at its own place) at mid height: the bounce off a moby
                     const Vec4 c = ee_ref<Vec4>(hm2 + 0x10);
                     const float a = rf(-3.1f, 3.1f), sp = rf(0.5f, 1.0f);
                     const Vec4 v{sp * std::cos(a), sp * std::sin(a), rf(-0.1f, 0.1f), 0.0f};
                     ee_ref<Vec4>(pv) = v;
                     const float off = ee_read<float>(ORACLE_LADDR(10, 0x00161bec));
                     ee_ref<Vec4>(m + 0x10) = Vec4{c.x - 0.5f * v.x, c.y - 0.5f * v.y, c.z + off + rf(0.3f, 1.0f), c.w};
                 } else if (how == 1) {  // falling onto the floor beside the hero's moby: the bounce off the world, a landing that stops it
                     const Vec4 c = ee_ref<Vec4>(hm2 + 0x10);
                     const float off = ee_read<float>(ORACLE_LADDR(10, 0x00161bec));
                     ee_ref<Vec4>(m + 0x10) = Vec4{c.x + rf(-2, 2), c.y + rf(-2, 2), c.z + off + rf(0.05f, 0.5f), c.w};
                     wf(pv, rf(-0.2f, 0.2f)); wf(pv + 4, rf(-0.2f, 0.2f)); wf(pv + 8, -rf(0.6f, 1.2f));
                 }
                 w16(pv + 0x14, u(ri(0, 2) == 0 || (how < 2 && coin()) ? ri(0, 1) : ri(2, 300)));  // (0 / 1: the bounce is tested)
                 w32(pv + 0x10, 0);
                 if (st != 3 && ri(0, 2) == 0) hit_with(m, 0x830000u);
                 else w8(m + 0xa4, 0xff);
             }, 0x3aa},
            {"class939", 0x002d8c00, orx_class939_update_l10, 2, [](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 w32(pv, u(ri(0, 3)));
             }},
            {"class947", 0x002d8d30, orx_class947_update_l10, 2, [own_slot](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 w32(pv + 8, u(ri(0, 3)));
                 wf(pv + 4, rf(0, 60));
                 w32(pv + 0xc, coin() ? 0xffffffffu : u(ri(0, 3)));
                 w32(pv + 0x10, coin() ? 0xffffffffu : u(ri(0, 5)));
                 own_slot(m, pv + 0x10);
                 if (ri(0, 7) == 0) w32(pv, 0xffffffffu);
             }},
            {"class1015", 0x002d90a8, orx_class1015_update_l10, 5, [hero_at, near](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (coin()) wf(m + 0x18, ee_read<float>(pv + 8) + rf(-0.2f, 4.2f));
                 if (coin()) hero_at(near(m, 3, 1));
                 if (ri(0, 3) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = near(m, 3, 1);
                 if (ri(0, 3) == 0) w32(pv + 0x14, coin() ? 0xffffffffu : u(ri(0, 10)));
                 if (coin()) {  // a group of the level whose first moby is alive (the walk then ends at once: group_first's 0). A group
                     // without a live first moby sends the original's walk round forever (group_next's -1 from moby 0): no game state
                     const int32_t n = ee_read<int32_t>(OV(0x0015fff4));
                     const int32_t g = ri(0, n > 0 ? n - 1 : 0);
                     const uint32_t list = ee_read<uint32_t>(OV(0x001ab840) + u(g) * 4);
                     if (list != 0 && static_cast<int8_t>(ee_read<uint8_t>(ee_read<uint32_t>(OV(0x0015ffd8)) + ((ee_read<uint16_t>(list) & 0x7fffu) << 8) + 0x20)) >= 0)
                         w32(pv + 0x10, u(g));
                 }
             }},
            {"class1031", 0x002d92b8, orx_class1031_update_l10, 1, [hero_at, moby_at](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 const int32_t d = ee_read<int32_t>(pv);
                 if (d != -1) { const int ds = ri(0, 5); w8(moby_at(d) + 0x20, u(ds == 5 ? 4 : ds)); }  // (open, 4, twice as often: the release)
                 if (coin()) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-0.4f, 0.4f); q.y += rf(-0.4f, 0.4f); q.z += rf(0, 0.3f); hero_at(q); }
                 if (ri(0, 2) != 0) {  // a moby of the level standing on the switch: moved onto its surface and into the grid's cell there
                     const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), last = ee_read<uint32_t>(OV(0x0015ffe0));
                     std::vector<uint32_t> boxes, any;
                     for (uint32_t o = first; o < last; o += 0x100) {
                         if (o == m || ee_read<uint8_t>(o + 0x20) >= 0xfe || ee_read<uint32_t>(o + 0x94) == 0) continue;  // (with collision: the search skips the others)
                         (ee_read<int16_t>(o + 0xa6) == 0x3ef ? boxes : any).push_back(o);
                     }
                     const std::vector<uint32_t>& from = !boxes.empty() && ri(0, 3) == 0 ? boxes : any;
                     if (!from.empty()) {
                         const uint32_t o = from[static_cast<size_t>(ri(0, static_cast<int>(from.size()) - 1))];
                         Vec4 q = ee_ref<Vec4>(m + 0x10);
                         q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.z += 0.4f;
                         ee_ref<Vec4>(o + 0x10) = q;
                         const float fz = floor_probe_v(0.5f, q, 0);
                         if (ee_read<uint32_t>(OV(0x00173e58)) == m) q.z = fz + (ri(0, 3) == 0 ? rf(-0.3f, 0.3f) : rf(-0.08f, 0.08f));
                         ee_ref<Vec4>(o + 0x10) = q;
                         const uint32_t cx = static_cast<uint32_t>(static_cast<int32_t>(q.x) >> 4), cy = static_cast<uint32_t>(static_cast<int32_t>(q.y) >> 4);
                         moby_grid_move(o, cx | cy << 8 | cx << 16 | cy << 24);
                         moby_update_transform(o);  // (P10.2: its matrix moved too: the sphere search tests the moby's collision there)
                     }
                 }
                 if (coin()) for (uint32_t k = 0; k < 4; ++k) w8(0x0013e550 + k * 0x70 + 0x74, 0);  // (free sound slots: the door's sounds start)
                 if (d != -1 && coin()) {  // (the listener, the camera 0x166dc0, by the door: its sound is loud enough to keep its slot,
                                           //  else the switch's own sound takes the same slot and leaves no trace of the door's)
                     Vec4 c = ee_ref<Vec4>(moby_at(d) + 0x10);
                     c.x += rf(-3, 3); c.y += rf(-3, 3); c.z += rf(0, 3);
                     ee_ref<Vec4>(OV(0x00166dc0)) = c;
                 }
                 w32(pv + 0xc, coin() ? 0u : 1u);
                 w32(pv + 0x10, u(ri(0, 2)));
                 if (ri(0, 3) == 0) w32(pv + 4, 0xffffffffu);
                 if (ri(0, 8) == 0) w32(pv, 0xffffffffu);
             }},
            {"class1047", 0x002d9eb8, orx_class1047_update_l10, 1, [hit_with](uint32_t m, int) {
                 if (ri(0, 2) != 0) hit_with(m, 0x40000u);
                 else w8(m + 0xa4, 0xff);
             }},
            {"class1067", 0x002da2c8, orx_class1067_update_l10, 3, [near, own_slot, camera_sees](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (ri(0, 6) == 0) w32(pv + 0xc, 0xffffffffu);
                 w32(pv + 4, u(ri(1, 10)));
                 if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = near(m, 60, 5);
                 if (coin()) {  // the panel moved before the view's camera (0x16ccf0, its rows 0x16ccc0 ..: on screen or just off)
                     const Vec4 cam = ee_ref<Vec4>(OV(0x0016ccf0));
                     const float fx = ee_read<float>(OV(0x0016ccc0) + 8), fy = ee_read<float>(OV(0x0016ccd0) + 8), fz = ee_read<float>(OV(0x0016cce0) + 8);
                     const float d = (ri(0, 3) == 0 ? -1.0f : 1.0f) * rf(4, 40);
                     ee_ref<Vec4>(m + 0x10) = Vec4{cam.x + fx * d + rf(-3, 3), cam.y + fy * d + rf(-3, 3), cam.z + fz * d + rf(-3, 3), ee_ref<Vec4>(m + 0x10).w};
                     ee_ref<Vec4>(OV(0x00166dc0)) = cam;
                 }
                 w32(pv + 0x30, coin() ? 0xffffffffu : u(ri(0, 5)));
                 own_slot(m, pv + 0x30);
                 if (ri(0, 2) == 0) {  // (P10.3) its centre on screen within 80 of the camera: its draw callback queued
                     const float half_h = static_cast<float>(ee_read<int32_t>(pv + 4)) * ee_read<float>(pv + 8) * 0.5f, half_w = ee_read<float>(pv) * 0.5f;
                     const Vec4 q = ee_ref<Vec4>(m + 0x10);
                     camera_sees(Vec4{q.x, q.y, q.z + half_h, std::sqrt(half_h * half_h + half_w * half_w)}, 80.0f, 40.0f);
                 }
                 if (ri(0, 4) == 0) w32(0x0015ed84, 9);
             }},
            // ---- task P10.2 (2026-11-15): classes 1073, 1090, 1100, 1196, 1199, 1202
            {"class1073", 0x002dcc58, orx_class1073_update_l10, 2, [](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 for (uint32_t i = 0; i < 4; ++i)  // (some strands' timers out: laid out again)
                     for (uint32_t j = 0; j < 10; ++j) if (ri(0, 3) == 0) w32(pv + 0x12c00 + i * 0x78 + j * 4, u(ri(0, 2)));
                 wf(pv + 0x12de0, rf(0, 1.2f));
                 if (coin()) w32(ORACLE_LADDR(10, 0x00161d64), u(ri(1, 60)));
                 if (ri(0, 4) == 0) w32(0x0015ed84, 9);
             }},
            {"class1090", 0x002dd3d8, orx_class1090_update_l10, 1, [mobys_with, live_moby](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 const std::vector<uint32_t> owners = mobys_with(0x002d8d30);  // (the class 947 sparks that shed it, and their paths)
                 const uint32_t owner = owners.empty() ? 0u : owners[static_cast<size_t>(ri(0, static_cast<int>(owners.size()) - 1))];
                 const int32_t pidx = owner != 0 ? ee_read<int32_t>(ee_read<uint32_t>(owner + 0x78)) : -1;
                 const uint32_t path = pidx >= 0 ? ee_read<uint32_t>(OV(0x001b04b0) + u(pidx) * 4) : ee_read<uint32_t>(OV(0x001b04b0));
                 w32(pv, path);
                 wf(pv + 4, rf(0, 40));
                 wf(pv + 8, rf(0, 0.6f));
                 wf(pv + 0xc, rf(0.5f, 3));
                 w32(pv + 0x10, coin() && owner != 0 ? owner : live_moby(m));
                 w32(pv + 0x14, u(ri(0, 2) == 0 ? ri(0, 1) : ri(2, 140)));
                 wf(pv + 0x18, rf(0.97f, 1.0f));
                 wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0, 0.6f));
             }, 0x442},
            {"class1100", 0x002dd650, orx_class1100_update_l10, 1, [live_moby](uint32_t m, int) {
                 // hit records naming it (kind 0x20000 or not) from shots of class 0xac / 0x99 (or another) on either side: fake shots in
                 // the test area (M2, M3)
                 for (int k = 0; k < 3; ++k) {
                     const uint32_t e = OV(0x00178100) + (u(ri(0, 63)) << 6);
                     const uint32_t src = k == 0 ? M2 : (k == 1 ? M3 : live_moby(m));
                     if (k < 2) {
                         static const uint16_t cls[] = {0xac, 0x99, 0x31};
                         w16(src + 0xa6, cls[ri(0, 2)]);
                         Vec4 q = ee_ref<Vec4>(m + 0x10);
                         q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 1);
                         ee_ref<Vec4>(src + 0x10) = q;
                     }
                     w32(e + 0x34, ri(0, 4) == 0 ? live_moby(m) : m);
                     w32(e + 0x24, ri(0, 3) == 0 ? (R() & ~0x20000u) : (0x20000u | (R() & 0xff00ffu)));
                     w32(e + 0x20, src);
                 }
             }},
            {"class1196", 0x002df270, orx_class1196_update_l10, 12, [hit_with, hero_at, near, live_moby, P](uint32_t m, int st) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 const int32_t paths = ee_read<int32_t>(OV(0x00160104));
                 w32(pv + 0x174, u(ri(0, 2)));
                 w32(pv + 0x104, coin() ? 2u : 0u);
                 w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
                 w32(pv + 0x184, u(ri(0, 2)));
                 w32(pv + 0x100, coin() ? ee_read<uint32_t>(P + 0x2080) : live_moby(m));
                 if (coin()) w8(m + 0x70, R() & 0xff);
                 if (coin()) w8(m + 0x53, u(ri(0, 6)));
                 if (paths > 0) {
                     for (const uint32_t o : {0x150u, 0x17cu, 0x18cu}) if (coin()) w32(pv + o, ri(0, 4) == 0 ? 0xffffffffu : u(ri(0, paths - 1)));
                     const int32_t p = ee_read<int32_t>(pv + 0x150);
                     if (p >= 0 && p < paths) {
                         const int32_t n = ee_read<int32_t>(ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4));
                         if (n > 0) w32(pv + 0x158, u(ri(0, n)));
                     }
                 }
                 w32(pv + 0x19c, u(ri(0, 2)));
                 wf(pv + 0x20, rf(0.3f, 2));
                 if (ri(0, 2) == 0) hit_with(m, 0x330000u);
                 else w8(m + 0xa4, 0xff);
                 w32(ORACLE_LADDR(10, 0x00161f38), coin() ? 1u : 0u);
                 w8(0x0013d418, coin() ? 1u : 0u);
                 if (ri(0, 3) == 0) {  // the hero in its box or about it
                     const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + (u(ee_read<int32_t>(pv + 0x194)) << 7);
                     Vec4 q = ee_ref<Vec4>(box + 0x30);
                     q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 1);
                     hero_at(q);
                 }
                 if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = near(m, 35, 5);
                 w8(m + 0x31, ri(0, 3) == 0 ? 0u : 1u);  // (visible: else its target is lost, +0x104 = 2)
                 if (paths > 0 && coin()) {  // at its path's point (within 1), the point's w lane a cue or not, or at the path's end
                     const int32_t p = ee_read<int32_t>(pv + 0x150);
                     if (p >= 0 && p < paths) {
                         const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4);
                         const int32_t n = ee_read<int32_t>(path);
                         if (n > 0) {
                             const int32_t i = coin() ? n - 1 : ri(0, n - 1);
                             w32(pv + 0x158, u(i));
                             Vec4 q = ee_ref<Vec4>(path + 0x10 + u(i) * 16);
                             q.x += rf(-0.6f, 0.6f); q.y += rf(-0.6f, 0.6f);
                             ee_ref<Vec4>(m + 0x10) = Vec4{q.x, q.y, q.z, ee_ref<Vec4>(m + 0x10).w};
                         }
                     }
                 }
                 if (ee_read<uint8_t>(m + 0xa4) != 0xff) w32(OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6) + 0x30, R() & 1u);
                 if (st == 4 && coin()) {  // rising: within its stop of the height (its start +0x138 and 2), slow
                     wf(m + 0x18, ee_read<float>(pv + 0x138) + rf(1.2f, 2.8f));
                     wf(pv + 0x128, rf(-0.1f, 0.1f));
                 }
             }},
            {"class1199", 0x002e01a8, orx_class1199_update_l10, 14, [hit_with, hero_at, near, live_moby, P](uint32_t m, int st) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 const int32_t paths = ee_read<int32_t>(OV(0x00160104));
                 w32(pv + 0x174, u(ri(0, 3)));
                 w32(pv + 0x104, coin() ? 2u : 0u);
                 w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
                 w32(pv + 0x184, u(ri(0, 2)));
                 w32(pv + 0x100, coin() ? ee_read<uint32_t>(P + 0x2080) : live_moby(m));
                 if (coin()) w8(m + 0x70, R() & 0xff);
                 if (coin()) w8(m + 0x53, u(ri(0, 5)));
                 if (paths > 0) for (const uint32_t o : {0x17cu, 0x18cu}) if (coin()) w32(pv + o, ri(0, 4) == 0 ? 0xffffffffu : u(ri(0, paths - 1)));
                 for (const uint32_t o : {0x198u, 0x1a4u}) w32(pv + o, u(ri(0, 2)));
                 w8(m + 0xbc, u(ri(0, 2)));
                 if (ri(0, 3) == 0) w8(m + 0x21, 0xff);
                 w32(0x001413dc, ri(0, 4) == 0 ? 7u : 0u);
                 wf(pv + 0x20, rf(0.3f, 2));
                 if (ri(0, 2) == 0) hit_with(m, 0x330000u);
                 else w8(m + 0xa4, 0xff);
                 if (coin()) {  // the target near it, at its bite's reach, or beyond 3
                     const uint32_t w = ee_read<uint32_t>(pv + 0x100);
                     static const float reach[] = {0.8f, 2.5f, 6.0f, 12.0f};
                     if (w != 0) ee_ref<Vec4>(w + 0x10) = near(m, reach[ri(0, 3)], 1.5f);
                 }
                 w8(m + 0x31, ri(0, 3) == 0 ? 0u : 1u);  // (visible: else its target is lost, +0x104 = 2)
                 if (ee_read<uint8_t>(m + 0xa4) != 0xff) w32(OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6) + 0x30, R() & 1u);
                 if (coin()) wf(m + 0x54, rf(0, 30));  // (the animation's time: the bite's cue)
                 if (ri(0, 3) == 0) {
                     const uint32_t box = ee_read<uint32_t>(OV(0x001600ec)) + (u(ee_read<int32_t>(pv + 0x194)) << 7);
                     Vec4 q = ee_ref<Vec4>(box + 0x30);
                     if (coin()) {
                         q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 1);
                     } else {  // in its box's own frame (the frame +0x40 maps the box to -1 .. 1): x about its upper limit 1
                         w32(pv + 0x174, 2);
                         const Mat4& f = ee_ref<Mat4>(box + 0x40);
                         Vec4 a[3];
                         for (int j = 0; j < 3; ++j) {
                             Vec4 e{0, 0, 0, 0};
                             (j == 0 ? e.x : j == 1 ? e.y : e.z) = 1.0f;
                             mat3_transform(a[j], e, f);  // (column j of the map)
                         }
                         const float t[3] = {rf(0.3f, 1.2f), rf(-0.9f, 0.9f), rf(-0.9f, 0.9f)};
                         auto det = [](const float c0[3], const float c1[3], const float c2[3]) {
                             return c0[0] * (c1[1] * c2[2] - c1[2] * c2[1]) - c1[0] * (c0[1] * c2[2] - c0[2] * c2[1]) + c2[0] * (c0[1] * c1[2] - c0[2] * c1[1]);
                         };
                         const float c[3][3] = {{a[0].x, a[0].y, a[0].z}, {a[1].x, a[1].y, a[1].z}, {a[2].x, a[2].y, a[2].z}};
                         const float dd = det(c[0], c[1], c[2]);
                         if (dd != 0.0f) {  // (Cramer's rule: the offset whose image is t)
                             q.x += det(t, c[1], c[2]) / dd;
                             q.y += det(c[0], t, c[2]) / dd;
                             q.z += det(c[0], c[1], t) / dd;
                         }
                     }
                     hero_at(q);
                 }
                 if (st == 3 && coin()) {  // to its point: there (within 0.1), or about to be
                     const Vec4 g = ee_ref<Vec4>(pv + 0x140);
                     ee_ref<Vec4>(m + 0x10) = Vec4{g.x + rf(-0.05f, 0.05f), g.y + rf(-0.05f, 0.05f), g.z + rf(-0.05f, 0.05f), ee_ref<Vec4>(m + 0x10).w};
                 }
                 if (st == 7 && coin()) {  // the swoop: the hero (its target's moby, +0x100) a little below it, in reach or not
                     const Vec4 c = ee_ref<Vec4>(m + 0x10);
                     const float a = rf(-3.1f, 3.1f), d = rf(1, 7);
                     hero_at(Vec4{c.x + d * std::cos(a), c.y + d * std::sin(a), c.z - rf(0.05f, 0.6f), c.w});
                 }
             }},
            {"class1202", 0x002e1d38, orx_class1202_update_l10, 13, [hit_with, hero_at, near, live_moby, mobys_with, P](uint32_t m, int st) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 const std::vector<uint32_t> gadgebots = mobys_with(0x002d4078);
                 const int pick = ri(0, 3);
                 w32(pv + 0x160, pick == 0 ? ee_read<uint32_t>(P + 0x2080) : (pick == 1 && !gadgebots.empty() ? gadgebots[0] : (pick == 2 ? 0u : live_moby(m))));
                 if (ri(0, 4) == 0) {  // a Gadgebot (class 0x359) as its leader: a fake one in the test area (M3)
                     w16(M3 + 0xa6, 0x359);
                     w8(M3 + 0x20, 1);
                     w32(pv + 0x160, M3);
                 }
                 if (ee_read<uint32_t>(pv + 0x160) != 0 && ri(0, 3) != 0) {  // the leader at the ring's gap, nearer, or farther, on its level
                     static const float lo[] = {1.0f, 1.8f, 3.3f, 4.5f}, hi[] = {1.8f, 2.6f, 3.9f, 9.0f};
                     const int k = ri(0, 3);
                     const float a = rf(-3.1f, 3.1f), d = rf(lo[k], hi[k]);
                     const Vec4 c = ee_ref<Vec4>(m + 0x10);
                     ee_ref<Vec4>(ee_read<uint32_t>(pv + 0x160) + 0x10) = Vec4{c.x + d * std::cos(a), c.y + d * std::sin(a), c.z + (coin() ? 0.0f : rf(-2, 2)), c.w};
                 }
                 w8(m + 0x31, ri(0, 3) == 0 ? 0u : 1u);
                 if (ee_read<int32_t>(pv + 0x1bc) != -1 && coin()) {  // its guarded moby's one-shot bit set (gone, or not for good)
                     const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + (ee_read<uint32_t>(pv + 0x1bc) << 8);
                     const int32_t idx = ee_read<int16_t>(o + 0xb2);
                     const uint32_t w = 0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
                     w32(w, ee_read<uint32_t>(w) | (1u << (ee_read<uint16_t>(o + 0xb2) & 0x1f)));
                     const uint32_t b = ee_read<uint8_t>(o + 0xb0);
                     if (b != 0xff) w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + b, coin() ? 0xffu : 0u);
                 }
                 w32(pv + 0x1bc, coin() ? 0xffffffffu : u(ri(0, 80)));
                 w32(pv + 0x1dc, coin() ? 1u : 0u);
                 if (ri(0, 3) == 0) {  // the hero's state 0x15 holding a moby whose pvar +0x10 is this one
                     const uint32_t o = live_moby(m);
                     w32(0x00140408, 0x15);
                     w32(0x001403e0, o);
                     if (o != 0 && ee_read<uint32_t>(o + 0x78) != 0 && coin()) w32(ee_read<uint32_t>(o + 0x78) + 0x10, m);
                 }
                 w32(pv + 0x164, coin() ? 2u : 0u);
                 w32(pv + 0x1b4, coin() ? 1u : 0u);
                 w32(pv + 0x1a8, u(ri(0, 2)));
                 w32(pv + 0x1d8, coin() ? 1u : 0u);
                 wf(pv + 0xf4, rf(0, 0.3f));
                 wf(pv + 0x20, rf(0.3f, 6));
                 w32(0x0013f350 + 0x1c0, u(ri(0, 20)));
                 if (coin()) w32(OV(0x0015f5cc), 7u * u(ri(0, 3000)));
                 if (coin()) w8(m + 0x53, u(ri(0, 2) == 0 ? 14 : ri(0, 7)));
                 if (coin()) w8(m + 0x70, R() & 0xff);
                 if (coin()) wf(m + 0x54, rf(0, 8));
                 if (ri(0, 2) == 0) {
                     hit_with(m, 0xb30000u);
                     const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                     static const uint16_t kinds[] = {0x47, 0x31a, 0, 0xba};
                     w16(e + 0x2a, coin() ? kinds[ri(0, 3)] : ee_read<uint16_t>(m + 0xa6));
                 } else {
                     w8(m + 0xa4, 0xff);
                 }
                 w32(0x0013f350 + 0x2084, ri(0, 3) == 0 ? 0x20u : 0u);  // (the hero's state +0x2084: the damage cap)
                 // its frame's part (orx_2e3ef0_l10): woken by +0x38, alerted (+0x1ac), still pulsing (+0x26), the other damage
                 // cap (+0x2090 with +0x198), the hero's own two (+0x23c / +0x240), the 300 frames that send it home (+0x1d4), a
                 // hit by class 0x31a (a fake one in the test area, M2)
                 w32(pv + 0x38, ri(0, 4) == 0 ? 1u : 0u);
                 w32(pv + 0x1ac, coin() ? u(ri(1, 200)) : 0u);
                 w16(pv + 0x26, coin() ? u(ri(1, 30)) : 0u);
                 w32(0x0013f350 + 0x2090, ri(0, 3) == 0 ? 0x20u : 0u);
                 w32(0x0013f350 + 0x198, u(ri(0, 60)));
                 if (ri(0, 3) == 0) w32(0x0013f350 + (coin() ? 0x240 : 0x23c), m);
                 w32(pv + 0x1d4, coin() ? u(ri(290, 310)) : u(ri(0, 5)));
                 if (ee_read<uint8_t>(m + 0xa4) != 0xff && ri(0, 3) == 0) {
                     const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                     w16(M2 + 0xa6, 0x31a);
                     ee_ref<Vec4>(M2 + 0x10) = near(m, 6, 1);
                     w32(e + 0x20, M2);
                 }
                 if (st == 5 && coin()) {  // the chase: the hero (its frame part takes him as the leader, +0x160 is its target's moby) about the ring's edges
                     static const float at[] = {3.52f, 3.55f, 3.2f, 3.25f, 3.88f, 3.94f};  // (radius 3.57; 0.35 either side)
                     const float a = rf(-3.1f, 3.1f), d = at[ri(0, 5)] + rf(-0.01f, 0.01f);
                     const Vec4 c = ee_ref<Vec4>(m + 0x10);
                     hero_at(Vec4{c.x + d * std::cos(a), c.y + d * std::sin(a), c.z, c.w});
                 }
                 if (st == 5 && coin()) {  // the chase: in a group of the level's (the gap between its members about the leader)
                     std::vector<uint32_t> groups;
                     for (const uint32_t o : mobys_with(0x002e1d38)) if (ee_read<uint8_t>(o + 0x21) != 0xff) groups.push_back(ee_read<uint8_t>(o + 0x21));
                     if (!groups.empty()) w8(m + 0x21, groups[static_cast<size_t>(ri(0, static_cast<int>(groups.size()) - 1))]);
                     w8(0x001413f4, 0);  // (the hero, not Clank: the heading steers it)
                 }
                 if (st == 7 && coin()) {  // the swing: its animation's time about the blow's start (4 .. 7)
                     w8(m + 0x53, coin() ? 6u : 7u);
                     w8(m + 0x52, 0xfe);  // (another animation before: the time is the frame's start and +0x54)
                     const uint32_t cur = ee_read<uint32_t>(m + 0x68);
                     if (cur != 0) {
                         const float base = static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f;
                         const float off = rf(3.8f, 4.7f) - base;
                         wf(m + 0x54, off != 0.0f ? off : 0.01f);
                     }
                 }
                 if ((st == 3 || st == 5 || st == 7 || st == 8) && coin()) {  // above the floor, falling about its speed cap (7 a frame time)
                     wf(m + 0x18, ee_read<float>(m + 0x18) + rf(0.5f, 4));
                     wf(pv + 0x78, -ee_read<float>(0x0015ed6c) * rf(5.5f, 9));
                     wf(pv + 0x80, rf(0, 0.05f));
                 }
                 if (st == 7 && coin()) {  // the swing's end: its animation 6 / 7 done, out of the blow's time, home far or near
                     w8(m + 0x53, coin() ? 6u : 7u);
                     w8(m + 0x52, 0xfe);
                     w8(m + 0x70, (R() & 0xff) | 2u);
                     const uint32_t cur = ee_read<uint32_t>(m + 0x68);
                     if (cur != 0) {
                         const float base = static_cast<float>(ee_read<int16_t>(cur + 4)) * 0.0625f;
                         const float off = (coin() ? rf(7.2f, 9) : rf(0.5f, 3.8f)) - base;
                         wf(m + 0x54, off != 0.0f ? off : 0.01f);
                     }
                     if (coin()) ee_ref<Vec4>(pv + 0x170) = near(m, coin() ? 40.0f : 10.0f, 1);
                 }
             }},
            // ---- task P10.3 (2026-11-15): the rest of the level's own classes (orxon_class1229 / effects / scenes.cpp, doors, props)
            {"class1229", 0x002e4a88, orx_class1229_update_l10, 9, [hit_with, hero_at, near, P, box_point, in_or_out, path_pt, path_len, anim_time](uint32_t m, int st) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (ri(0, 10) == 0) w32(pv + 0x140, 0xffffffffu);  // (no path: printed and gone)
                 if (ri(0, 2) == 0) {  // a hit (its damage, the 0x40000 kind that kills at once)
                     hit_with(m, 0x370000u);
                     const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                     w32(e + 0x24, ri(0, 3) == 0 ? (ee_read<uint32_t>(e + 0x24) | 0x40000u) : (ee_read<uint32_t>(e + 0x24) & ~0x40000u));
                 } else {
                     w8(m + 0xa4, 0xff);
                 }
                 wf(pv + 0x20, coin() ? rf(0.2f, 2) : rf(2, 9));
                 const int32_t p140 = ee_read<int32_t>(pv + 0x140), p144 = ee_read<int32_t>(pv + 0x144);
                 if (ri(0, 3) == 0) hero_at(in_or_out(ee_read<int32_t>(pv + 0x148)));
                 else if (coin()) hero_at(near(m, 12, 3));
                 if (ri(0, 4) == 0) {  // Ratchet inside its area (path +0x190's polygon), the drone about the edge of its reach (101) from him
                     w8(0x001413f4, 0);
                     const int32_t area = ee_read<int32_t>(pv + 0x190), na = path_len(area);
                     if (na > 2) {
                         float cx = 0, cy = 0;
                         for (int32_t k = 0; k < na; ++k) { const Vec4 q = ee_ref<Vec4>(path_pt(area, k)); cx += q.x; cy += q.y; }
                         const Vec4 h{cx / static_cast<float>(na), cy / static_cast<float>(na), ee_ref<Vec4>(m + 0x10).z, 1.0f};
                         hero_at(h);
                         const float a = rf(-3.1f, 3.1f), d = rf(100.05f, 100.95f);
                         ee_ref<Vec4>(m + 0x10) = Vec4{h.x + d * std::cos(a), h.y + d * std::sin(a), h.z, ee_ref<Vec4>(m + 0x10).w};
                     }
                 }
                 const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
                 if (st == 1) {  // the help message's conditions
                     w32(OV(0x00179410), ri(0, 3) == 0 ? 1u : 0u);
                     w32(OV(0x00179434), ri(0, 3) == 0 ? 0u : 0xffffffffu);
                     w16(0x00141c48, ri(0, 3) == 0 ? 1u : 0u);
                     w8(0x0013d4cb, ri(0, 3) == 0 ? 1u : 0u);
                     if (coin()) w8(0x001413f4, 0);
                 }
                 if (st == 2) {  // rising: at its path's first point or not, its animation done or not
                     const uint32_t pt = path_pt(p140, 0);
                     if (pt != 0 && coin()) { Vec4 q = ee_ref<Vec4>(pt); q.x += rf(-0.3f, 0.3f); q.y += rf(-0.3f, 0.3f); q.z += rf(-0.6f, 0.2f); ee_ref<Vec4>(m + 0x10) = Vec4{q.x, q.y, q.z, ee_ref<Vec4>(m + 0x10).w}; }
                     w8(m + 0x70, coin() ? 2u : 0u);
                     w8(m + 0x53, u(ri(0, 3)));
                 }
                 if (st == 3) {  // along its path: at a point (the last or not)
                     const int32_t n = path_len(p140);
                     if (n > 0) {
                         const int32_t i = coin() ? n - 1 : ri(0, n - 1);
                         w32(pv + 0x158, u(i));
                         if (coin()) { const Vec4 q = ee_ref<Vec4>(path_pt(p140, i)); ee_ref<Vec4>(m + 0x10) = Vec4{q.x + rf(-0.3f, 0.3f), q.y + rf(-0.3f, 0.3f), q.z, ee_ref<Vec4>(m + 0x10).w}; }
                     }
                 }
                 if (st == 4 || st == 5) {
                     if (ri(0, 4) == 0) hero_at(box_point(ee_read<int32_t>(pv + 0x1b0), rf(-0.8f, 0.8f), rf(-0.8f, 0.8f), rf(-0.8f, 0.8f)));
                     else if (coin()) hero_at(near(m, 10, 3));
                     ee_ref<Vec4>(pv + 0xf0) = near(m, 10, 3);
                     const int32_t area = ee_read<int32_t>(pv + 0x190), na = path_len(area);
                     if (na > 2 && coin()) {  // the hero inside its area (the polygon of path +0x190: about its points' middle)
                         float cx = 0, cy = 0;
                         for (int32_t k = 0; k < na; ++k) { const Vec4 q = ee_ref<Vec4>(path_pt(area, k)); cx += q.x; cy += q.y; }
                         hero_at(Vec4{cx / static_cast<float>(na) + rf(-1, 1), cy / static_cast<float>(na) + rf(-1, 1), ee_ref<Vec4>(m + 0x10).z - rf(0, 4), 1.0f});
                     }
                     if (hm != 0 && coin()) {  // aimed at the hero (within 10 degrees) or not
                         const Vec4 h = ee_ref<Vec4>(hm + 0x10), q = ee_ref<Vec4>(m + 0x10);
                         const int k = ri(0, 2);
                         wf(m + 0x48, std::atan2(h.y - q.y, h.x - q.x) + (k == 0 ? rf(-0.15f, 0.15f) : (k == 1 ? (coin() ? 1.0f : -1.0f) * rf(0.172f, 0.205f) : rf(0.2f, 3.0f))));
                     }
                     if (coin()) {  // its shooting animation about the cues 1 and 16
                         static const float cue[] = {1.0f, 16.0f, 8.0f};
                         anim_time(m, 3, cue[ri(0, 2)] + rf(0, 0.6f));
                         wf(m + 0x58, rf(0.3f, 1.2f));
                         wf(m + 0x5c, 1.0f);
                     } else if (coin()) {
                         w8(m + 0x52, u(ri(2, 3)));
                         w8(m + 0x53, u(ri(2, 3)));
                     }
                     w32(pv + 0x114, coin() ? 2u : 0u);
                     if (hm != 0 && coin()) w32(OV(0x00173e58), hm);
                     w32(pv + 0x168, u(ri(0, 2)));
                     const int32_t n = path_len(p144);
                     if (n > 0) {
                         const int32_t i = coin() ? n - 1 : ri(0, n - 1);
                         w32(pv + 0x158, u(i));
                         if (coin()) {  // at its point (within a frame's step 1/60, or two)
                             const Vec4 q = ee_ref<Vec4>(path_pt(p144, i));
                             const float a = rf(-3.1f, 3.1f), d = coin() ? rf(0.0f, 0.016f) : rf(0.0175f, 0.033f);
                             ee_ref<Vec4>(m + 0x10) = Vec4{q.x + d * std::cos(a), q.y + d * std::sin(a), q.z, ee_ref<Vec4>(m + 0x10).w};
                         }
                     }
                 }
                 if (st == 6) {
                     if (coin()) wf(m + 0x18, rf(240, 252));
                     if (coin()) wf(m + 0x2c, rf(0.005f, 0.02f));
                 }
                 if (st == 7) {
                     w8(m + 0x52, ri(0, 3) == 0 ? 3u : 4u);
                     w8(m + 0x70, ri(0, 3) == 0 ? 0u : 2u);
                     static const uint32_t was[] = {4, 6, 5, 3};
                     w8(m + 0xbc, was[ri(0, 3)]);
                 }
                 w8(m + 0x31, ri(0, 3) == 0 ? 0u : 1u);
                 if (coin()) ee_ref<Vec4>(OV(0x00166dc0)) = near(m, 35, 5);
                 if (coin()) w8(m + 0x53, u(ri(0, 4)));
             }},
            {"class1240", 0x002e5be0, orx_class1240_update_l10, 2, [](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 w32(pv + 0xc, u(ri(0, 6)));
                 if (coin()) wf(pv + 0x10, rf(0, 2));
                 if (coin()) wf(pv + 0x14, rf(0, 2));
                 if (coin()) wf(pv + 4, rf(0.1f, 2));
                 if (coin()) wf(pv + 8, rf(0.1f, 3));
                 if (coin()) wf(m + 0x44, rf(-1.5f, 1.5f));
             }},
            {"class1326", 0x002e8358, orx_class1326_update_l10, 2, [hero_at](uint32_t m, int st) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (coin()) {  // the hero before it (or behind), within 4 or not
                     const Vec4 q = ee_ref<Vec4>(m + 0x10);
                     const float yaw = ee_read<float>(m + 0x48) + (ri(0, 3) == 0 ? rf(1.7f, 4.5f) : rf(-1.4f, 1.4f));
                     const float d = ri(0, 3) == 0 ? rf(4.2f, 8) : rf(0.5f, 3.8f);
                     hero_at(Vec4{q.x + d * std::cos(yaw), q.y + d * std::sin(yaw), q.z, q.w});
                 }
                 w32(pv + 4, ri(0, 3) == 0 ? u(ri(1, 30)) : 0u);
                 w8(0x0013d38c, coin() ? 1u : 0u);
                 w8(0x0013d38d, ri(0, 3) == 0 ? 1u : 0u);
                 static const uint32_t bolts[] = {3000, 4000, 4001, 9000, 29999, 30000, 30001, 60000};
                 w32(0x0015ed98, bolts[ri(0, 7)]);
                 w32(0x0013cae4, coin() ? 0x10u : (R() & ~0x10u));
                 { static const uint32_t holder[] = {0, 0, 1, 3}; w32(OV(0x0015f594), holder[ri(0, 3)]); }  // (the HUD message's holder: none, the vendor, another)
                 if (st == 0 && coin()) w32(OV(0x0015f5c4), 0);
                 if (st == 0 && coin()) {  // a purchase made: before it within 4, its pause over, no scene, the message shown, the button
                     const Vec4 q = ee_ref<Vec4>(m + 0x10);
                     const float yaw = ee_read<float>(m + 0x48) + rf(-1.2f, 1.2f), d = rf(0.5f, 3.5f);
                     hero_at(Vec4{q.x + d * std::cos(yaw), q.y + d * std::sin(yaw), q.z, q.w});
                     w32(pv + 4, 0);
                     w32(OV(0x0015f5c4), 0);
                     w32(OV(0x0015f594), coin() ? 0u : 1u);
                     w32(0x0013cae4, 0x10u | (R() & 0xffu));
                     w32(0x0015ed98, ee_read<uint8_t>(0x0013d38c) == 0 ? u(ri(4001, 9000)) : u(ri(30001, 60000)));
                     if (ee_read<uint8_t>(0x0013d38c) != 0) w8(0x0013d38d, 0);
                 }
             }},
            {"class1344", 0x002e85b8, orx_class1344_update_l10, 2, [hero_at, in_or_out, P, moby_at](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 static const uint32_t boxes[] = {0x0, 0x4, 0x8, 0x18, 0x1c};
                 hero_at(in_or_out(ee_read<int32_t>(pv + boxes[ri(0, 4)])));
                 { static const uint32_t hs[] = {0, 1, 9, 5, 0x10}; w32(P + 0x208c, hs[ri(0, 4)]); }  // (the hero's state 0x1413dc)
                 w32(OV(0x00179410), ri(0, 3) == 0 ? 1u : 0u);
                 w32(OV(0x00179434), ri(0, 3) == 0 ? 0u : 0xffffffffu);
                 for (const uint32_t f : {0x00141b10u, 0x00141b18u, 0x00141b20u, 0x00141b28u, 0x00141c48u, 0x00141cb8u}) w16(f, ri(0, 3) == 0 ? 1u : 0u);
                 w8(0x0013d38c, coin() ? 1u : 0u);
                 w8(0x0013d38d, coin() ? 1u : 0u);
                 w8(0x0013d4cb, ri(0, 3) == 0 ? 1u : 0u);
                 for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffe4)); o != 0; o = ee_read<uint32_t>(o + 0x28))  // (the Gadgebots: standing by, or not)
                     if (ee_read<int16_t>(o + 0xa6) == 0x359) w8(o + 0x20, ri(0, 2) == 0 ? 1u : u(ri(2, 5)));
                 if (coin() && ee_read<int32_t>(pv + 0x10) != -1 && ee_read<int32_t>(pv + 0x14) != -1) {  // the hero between the two mobys
                     const Vec4 a = ee_ref<Vec4>(moby_at(ee_read<int32_t>(pv + 0x10)) + 0x10), b = ee_ref<Vec4>(moby_at(ee_read<int32_t>(pv + 0x14)) + 0x10);
                     if (coin()) {
                         hero_at(Vec4{(a.x + b.x) * 0.5f + rf(-3, 3), (a.y + b.y) * 0.5f + rf(-3, 3), (a.z + b.z) * 0.5f, 1.0f});
                     } else {  // (14 .. 15 from both: on their bisector)
                         const float dx = b.x - a.x, dy = b.y - a.y, l = std::sqrt(dx * dx + dy * dy), d = rf(14.05f, 14.95f);
                         if (l > 0.001f && l < 2 * d) {
                             const float h = std::sqrt(d * d - l * l / 4), sgn = coin() ? 1.0f : -1.0f;
                             hero_at(Vec4{(a.x + b.x) * 0.5f - dy / l * h * sgn, (a.y + b.y) * 0.5f + dx / l * h * sgn, a.z, 1.0f});
                         }
                     }
                 }
                 if (ee_read<int32_t>(pv + 0x20) != -1) {  // the goal moby (class 0x3f7) in state 4 or not
                     const uint32_t g = moby_at(ee_read<int32_t>(pv + 0x20));
                     w8(g + 0x20, ri(0, 3) == 0 ? 4u : u(ri(0, 3)));
                 }
                 w16(0x00141968 + 0x358, ri(0, 2) == 0 ? 0u : u(ri(1, 5)));
                 w16(0x00141968 + 0x35a, ri(0, 3) == 0 ? 0u : u(ri(1, 200)));
                 w32(0x0015eea4, coin() ? u(ri(0, 200000)) : u(ee_read<uint16_t>(0x00141968 + 0x35a) * 600 + (coin() ? ri(-300, 1500) : ri(1070, 1150))));
                 if (coin()) {  // the messages' conditions met (the hero standing, no message or scene, the flags clear)
                     w32(P + 0x208c, coin() ? 0u : 9u);
                     w32(OV(0x00179410), 0);
                     w32(OV(0x00179434), 0xffffffffu);
                     for (const uint32_t f : {0x00141b10u, 0x00141b18u, 0x00141b20u, 0x00141b28u, 0x00141c48u, 0x00141cb8u}) w16(f, 0);
                     w8(0x0013d4cb, 0);
                     w32(OV(0x0015f5c4), 0);
                     w8(0x0013d504, 1);
                     w32(0x001516d0 + 0x50, 0);
                 }
                 if (ee_read<int32_t>(pv + 0x20) != -1 && coin()) w16(moby_at(ee_read<int32_t>(pv + 0x20)) + 0xa6, 0x3f7);
                 if (ri(0, 3) == 0 && ee_read<int32_t>(pv + 0x10) != -1 && ee_read<int32_t>(pv + 0x14) != -1) {  // the hero 14 .. 15 from both mobys
                     // (on their bisector), the help system free, the other messages already shown (0x2711 would take its turn)
                     const Vec4 a = ee_ref<Vec4>(moby_at(ee_read<int32_t>(pv + 0x10)) + 0x10), b = ee_ref<Vec4>(moby_at(ee_read<int32_t>(pv + 0x14)) + 0x10);
                     const float dx = b.x - a.x, dy = b.y - a.y, l = std::sqrt(dx * dx + dy * dy), d = rf(14.05f, 14.95f);
                     if (l > 0.001f && l < 2 * d) {
                         const float h = std::sqrt(d * d - l * l / 4), sgn = coin() ? 1.0f : -1.0f;
                         hero_at(Vec4{(a.x + b.x) * 0.5f - dy / l * h * sgn, (a.y + b.y) * 0.5f + dx / l * h * sgn, a.z, 1.0f});
                     }
                     for (const uint32_t f : {0x00141b10u, 0x00141b18u, 0x00141b20u, 0x00141b28u, 0x00141c48u}) w16(f, 1);
                     w16(0x00141cb8, 0);
                     w32(0x001516d0 + 0x50, 0);  // (the help system's own busy word: -1 in the snapshot refuses every message)
                     w32(OV(0x00179410), 0);
                     w32(OV(0x00179434), 0xffffffffu);
                     for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffe4)); o != 0; o = ee_read<uint32_t>(o + 0x28))
                         if (ee_read<int16_t>(o + 0xa6) == 0x359) w8(o + 0x20, 3);
                 } else if (coin() && ee_read<int32_t>(pv + 0x20) != -1) {  // the run's goal reached: in box +0x18, a Gadgebot not standing by, the goal moby
                     hero_at(in_or_out(ee_read<int32_t>(pv + 0x18)));
                     for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffe4)); o != 0; o = ee_read<uint32_t>(o + 0x28))
                         if (ee_read<int16_t>(o + 0xa6) == 0x359) w8(o + 0x20, u(ri(2, 5)));
                     const uint32_t g = moby_at(ee_read<int32_t>(pv + 0x20));
                     w16(g + 0xa6, 0x3f7);
                     w8(g + 0x20, u(ri(0, 3)));
                 }
             }},
            {"class1346", 0x002e8ab0, orx_class1346_update_l10, 4, [live_moby, moby_at, flag_set](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (ri(0, 3) != 0) flag_set(m, 0);
                 const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
                 // the targets and the two mobys it signals: its own (else a live moby that is in the moby grid: retiring one that
                 // is not hits the original's `teq` trap), dead, or none
                 auto gridded = [first, live_moby, m]() -> uint32_t {
                     for (int k = 0; k < 40; ++k) {
                         const uint32_t t = live_moby(m);
                         if (t == 0 || ee_read<int32_t>(t + 0xa0) < 0) continue;
                         const uint32_t cell = OV(0x0019b7e0) + (ee_read<uint8_t>(t + 0xa0) * 4u) + (u(ee_read<uint8_t>(t + 0xa1)) << 8);
                         const uint32_t pool = OV(0x0019f7e0) + (u(ee_read<uint16_t>(cell)) << 5);
                         for (uint32_t i = 0; i < ee_read<uint8_t>(cell + 2); ++i)
                             if (ee_read<uint16_t>(pool + i * 2) == ee_read<uint16_t>(t + 0xac)) return t;
                     }
                     (void)first;
                     return 0u;
                 };
                 for (const uint32_t o : {0x0u, 0x4u, 0x8u, 0xcu, 0x14u}) {
                     const int k = ri(0, 4);
                     if (k == 0) { w32(pv + o, 0xffffffffu); continue; }
                     uint32_t t = ee_read<int32_t>(pv + o) != -1 ? moby_at(ee_read<int32_t>(pv + o)) : 0u;
                     if (t == 0 || ee_read<uint8_t>(t + 0x20) >= 0xfd) {
                         t = gridded();
                         if (t == 0) continue;
                         w32(pv + o, (t - first) >> 8);
                     }
                     if (k == 4) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
                 }
                 uint32_t aim = 0;  // the first of its targets still alive
                 for (const uint32_t o : {0x4u, 0x8u, 0xcu}) {
                     if (aim != 0 || ee_read<int32_t>(pv + o) == -1) continue;
                     const uint32_t t = moby_at(ee_read<int32_t>(pv + o));
                     if (ee_read<uint8_t>(t + 0x20) != 0xfe && ee_read<uint8_t>(t + 0x20) != 0xfd) aim = t;
                 }
                 if (aim != 0 && coin()) {  // aimed at it (its back) within 2.5 degrees, or not
                     const Vec4 t = ee_ref<Vec4>(aim + 0x10), q = ee_ref<Vec4>(m + 0x10);
                     const int k = ri(0, 2);
                     float yaw = std::atan2(t.y - q.y, t.x - q.x) + 3.14159265f + (k == 0 ? rf(-0.03f, 0.03f) : (k == 1 ? (coin() ? 1.0f : -1.0f) * rf(0.0438f, 0.0495f) : rf(0.1f, 2.0f)));
                     if (yaw > 3.14159265f) yaw -= 6.2831853f;
                     wf(m + 0x48, yaw);
                     wf(pv + 0x10, 0.0f);
                 }
             }},
            {"class1421", 0x002e9648, orx_class1421_update_l10, 4, [hero_at, in_or_out](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 hero_at(in_or_out(ee_read<int32_t>(pv + 0xc)));
                 w8(0x0013dd4c, ri(0, 3) == 0 ? 1u : 0u);
                 if (ri(0, 4) == 0) w32(pv, 0);
                 if (ri(0, 4) == 0) w32(pv + 0x10, 0);
                 w32(pv + 8, u(ri(0, 2)));
                 if (coin()) w8(0x001413f4, 0);
             }},
            {"class1424", 0x002e9990, orx_class1424_update_l10, 4, [](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 w8(0x0013dd4c, coin() ? 1u : 0u);
                 if (ri(0, 4) == 0) w32(pv + 0x1c, 0);
                 if (ri(0, 4) == 0) w32(pv + 0x10, 0);
                 w32(pv + 0x14, ri(0, 2) == 0 ? u(ri(0, 2)) : u(ri(3, 90)));
                 if (coin()) ee_ref<Vec4>(pv) = ee_ref<Vec4>(m + 0x10);
                 if (coin()) w8(0x001413f4, 0);
             }},
            {"class1544", 0x002ea1f0, orx_class1544_update_l10, 4, [path_pt, path_len, camera_sees](uint32_t m, int) {
                 const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                 if (ri(0, 4) == 0) w32(pv, u(ri(-1, 3)));
                 if (ri(0, 4) == 0) w32(pv + 4, 0xffffffffu);
                 const int32_t p = ee_read<int32_t>(pv + 4);
                 for (uint32_t k = 0; k < 0x20; ++k) {  // the vents' timers: off counting up, at their edges, on
                     static const int16_t v[] = {-1, -2, -30, 0, 1, 2, 30};
                     w16(pv + 8 + k * 2, static_cast<uint16_t>(v[ri(0, 6)]));
                     w16(pv + 0x48 + k * 2, static_cast<uint16_t>(v[ri(0, 6)]));
                 }
                 if (coin()) {  // the camera at the vents (one of them on screen)
                     const uint32_t at = p == -1 ? m + 0x10 : path_pt(p, ri(0, std::max(0, std::min(path_len(p), 32) - 1)));
                     if (at != 0) {
                         const bool drip = coin();  // (the drips test a point 4 below with radius 3, the others 6 above or at it)
                         Vec4 a = ee_ref<Vec4>(at);
                         a.z += drip ? -4.0f : (coin() ? 6.0f : 0.0f);
                         a.w = drip ? 3.0f : (coin() ? 5.0f : 2.0f);
                         camera_sees(a, 32.0f, 25.0f);
                     }
                 }
                 if (coin()) wf(pv + 0x88, rf(0.5f, 12));
                 if (coin())  // the vents on and their puffs due
                     for (uint32_t k = 0; k < 0x20; ++k) { w16(pv + 0x48 + k * 2, u(ri(2, 30))); w16(pv + 8 + k * 2, u(ri(0, 1))); }
             }},
            {"class1555", 0x002eacd8, orx_class1555_update_l10, 2, [live_moby](uint32_t m, int) {
                 const uint32_t SC = OV(0x0016c860);
                 w32(OV(0x0015f5c4), ri(0, 3) == 0 ? u(ri(0, 1)) : 2u);
                 static const uint32_t scenes[] = {3, 5, 6, 7, 3, 7, 1, 4};
                 w32(SC + 0x30, scenes[ri(0, 7)]);
                 std::vector<uint32_t> infobot;  // (the level's class 0x2ee moby, the Infobot, else any live one)
                 for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                     if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<int16_t>(o + 0xa6) == 0x2ee) infobot.push_back(o);
                 for (uint32_t k = 0; k < 4; ++k) {
                     const uint32_t o = !infobot.empty() && ri(0, 2) != 0 ? infobot[0] : live_moby(m);
                     w32(SC + 0x178 + k * 4, o);
                 }
                 w16(SC + 0x44, 4);
             }},
            {"class1855", 0x002eb6f0, orx_class1855_update_l10, 3, [hit_with](uint32_t m, int) {
                 if (ri(0, 3) != 0) {
                     hit_with(m, 0x10000u);
                     const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                     wf(e + 0x2c, coin() ? rf(0.5f, 5) : (coin() ? 0.0f : -1.0f));
                 } else {
                     w8(m + 0xa4, 0xff);
                 }
             }},
            {"class1856", 0x002eb7e0, orx_class1856_update_l10, 3, [hit_with](uint32_t m, int) {
                 if (ri(0, 3) != 0) {
                     hit_with(m, 0x10000u);
                     const uint32_t e = OV(0x00178100) + (u(ee_read<uint8_t>(m + 0xa4)) << 6);
                     wf(e + 0x2c, coin() ? rf(0.5f, 5) : (coin() ? 0.0f : -1.0f));
                 } else {
                     w8(m + 0xa4, 0xff);
                 }
             }},
        };
        const char* only = std::getenv("B122_P10");
        if (std::getenv("B122_INFO") != nullptr) {  // (P10.2) the floor switches' doors: their class's sound table and count
            level();
            std::printf("INFO class1202 half-angle L10:0x161e48 %g\n", ee_read<float>(ORACLE_LADDR(10, 0x00161e48)));
            for (uint32_t k = 0; k < 0x14; ++k) {  // (P10.3) the targets' list 0x1dd100 (class 1229's reach counts for class 0x10e there)
                const uint32_t c = ee_read<uint32_t>(OV(0x001dd100) + k * 4);
                if (c != 0) std::printf("INFO list1dd100 %u: 0x%08x class %d state %u\n", k, c, ee_read<int16_t>(c + 0xa6), ee_read<uint8_t>(c + 0x20));
            }
            // (P10.3) the play-test spots: each moby of the part's classes (its index, place, state)
            for (const uint32_t a : {0x002e4a88u, 0x002e5be0u, 0x002e8358u, 0x002e85b8u, 0x002e8ab0u, 0x002e9648u, 0x002e9990u, 0x002ea1f0u, 0x002eacd8u,
                                     0x002eb6f0u, 0x002eb7e0u, 0x002da2c8u})
                for (const uint32_t o : mobys_with(a)) {
                    const Vec4 q = ee_ref<Vec4>(o + 0x10);
                    const uint32_t opv = ee_read<uint32_t>(o + 0x78);
                    std::printf("SPOT L10:0x%08x class %d moby %u at (%.1f, %.1f, %.1f) state %u pvars %d %d %d %d %d %d %d %d %d\n", a, ee_read<int16_t>(o + 0xa6),
                                (o - ee_read<uint32_t>(OV(0x0015ffd8))) >> 8, q.x, q.y, q.z, ee_read<uint8_t>(o + 0x20), ee_read<int32_t>(opv), ee_read<int32_t>(opv + 4),
                                ee_read<int32_t>(opv + 8), ee_read<int32_t>(opv + 0xc), ee_read<int32_t>(opv + 0x10), ee_read<int32_t>(opv + 0x14), ee_read<int32_t>(opv + 0x18),
                                ee_read<int32_t>(opv + 0x1c), ee_read<int32_t>(opv + 0x20));
                }
            for (const uint32_t o : mobys_with(0x002e1d38)) std::printf("INFO class1202 0x%08x group 0x%02x\n", o, ee_read<uint8_t>(o + 0x21));
            for (const uint32_t sw1031 : mobys_with(0x002d92b8)) {
                const int32_t d = ee_read<int32_t>(ee_read<uint32_t>(sw1031 + 0x78));
                if (d == -1) continue;
                const uint32_t door = moby_at(d), cls = ee_read<uint32_t>(door + 0x24);
                const uint32_t table = cls != 0 ? ee_read<uint32_t>(cls + 0x28) : 0u;
                std::printf("INFO class1031 0x%08x door 0x%08x class %d sounds 0x%08x count %u unique0 %u unique1 %u\n", sw1031, door, ee_read<int16_t>(door + 0xa6),
                            table, cls != 0 ? ee_read<uint8_t>(cls + 0xd) : 0u, table != 0 ? ee_read<uint8_t>(table + 0x18) : 0u, table != 0 ? ee_read<uint8_t>(table + 0x20 + 0x18) : 0u);
            }
        }
        for (const P10& c : p10) {
            if (only != nullptr && std::strcmp(only, c.name) != 0) continue;
            std::vector<uint32_t> subj = c.spawn_cls != 0 ? spawned(c.spawn_cls) : std::vector<uint32_t>{};
            if (c.spawn_cls != 0 && subj.empty()) { std::printf("%-26s L10:0x%08x  could not spawn the class\n", c.name, c.addr); continue; }
            char nm[40];
            std::snprintf(nm, sizeof nm, "p10_%s", c.name);
            static std::vector<std::string> names;
            names.emplace_back(nm);
            const char* name = names.back().c_str();
            const std::vector<uint32_t> ignored_before = g_ignored_words;
            if (std::strcmp(c.name, "class18") == 0 || std::strcmp(c.name, "class1326") == 0) {  // (P10.3: class 1326's state 1 too)
                // class 18's state 2 ends in a level change: the original's run stops at the level change's noop hook
                // (0x23fea8, its CD / movie path), the port's level_change_request writes the save record's words
                // 0x13d970 .. 0x13d9bb and the console clock 0x15ee98 / 0x15ee9c (the generic check skips state 2 for it)
                for (uint32_t a = 0x0013d970; a < 0x0013d9bc; a += 4) g_ignored_words.push_back(a);
                g_ignored_words.push_back(0x0015ee98);
                g_ignored_words.push_back(0x0015ee9c);
            }
            // the segment records' VU0 leftovers of segment_collide's port (a -0 for a 0 at +0xc of the third record: COMMON.md's
            // known differences), met where the class tests a line against the world (859, 938) or places the hero (22, 351)
            for (const char* seg : {"class859", "class938", "class22", "class351"})
                if (std::strcmp(c.name, seg) == 0)
                    for (uint32_t a = OV(0x00173e90); a < OV(0x00173ec0); a += 4) g_ignored_words.push_back(a);
            // (P10.2) the same records' VU0 leftovers of sphere_collide's port: class 1196 drifting (state 10, its sphere test) leaves the
            // words +0x50 .. +0x7c of OV(0x173e40) (its three triangle points) at 1/1024 of the original's (the known differences
            // of the collision records, classified harmless)
            if (std::strcmp(c.name, "class1196") == 0)
                for (uint32_t a = OV(0x00173e90); a < OV(0x00173ec0); a += 4) g_ignored_words.push_back(a);
            // (P10.3) class 1229's death drops bolts (moby_drop_bolts): each bolt's pvars +0x20 take bolts_drop's velocity, whose w
            // lane (+0x2c) is an unset stack lane in the original (-1.0 here, after the explosion's calls) and 0 in the engine's
            // port (moby_classes.cpp's bolts_drop notes it): +0x2c of every pvar block (0x80 bytes a moby slot)
            if (std::strcmp(c.name, "class1229") == 0) {
                level();
                const uint32_t pvb = ee_read<uint32_t>(OV(0x0015ffe8));
                const uint32_t slots = (ee_read<uint32_t>(OV(0x0015ffe0)) - ee_read<uint32_t>(OV(0x0015ffdc))) >> 8;
                for (uint32_t i = 0; i < slots; ++i) g_ignored_words.push_back(pvb + i * 0x80 + 0x2c);
            }
            for (int st = 0; st < c.states; ++st)
                for (int pass = 0; pass < 2; ++pass) {
                    const P10 cc = c;
                    classcheck(name, c.addr, c.fn, [cc, st, pass, world, generic_prep, respawn](uint32_t m) {
                        if (cc.spawn_cls != 0) respawn(cc.spawn_cls, m);
                        generic_prep(m, st, pass == 0 ? 2 : 1, cc.fn, false);
                        world(m);
                        cc.vary(m, st);
                        w8(m + 0x20, u(st));
                        seed_rand();
                    }, 20, subj);
                }
            g_ignored_words = ignored_before;
        }
        // (P10.3) class 1067's draw callback L10:0x2da690 (orxon_effects.cpp) on the panels, its packets on a buffer of the test area
        // (the display packet cursor 0x1611c0), the panel on screen (or beyond 48 of the camera: faded), its strips, lamps, spacing, the
        // grid owner's cell values about the level's thresholds and its scroll varied, on (1) or switched off (2, fading); and the
        // draw list 0x1f95e8 (L10:0x1ecc50) that runs it, filled by the panels' own update (orx_class1067_update_l10) as in the game
        {
            const std::vector<uint32_t> panels = [&] { level(); return mobys_with(0x002da2c8); }();
            auto panel_case = [moby_at, camera_sees](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, st == 0 ? 1u : 2u);
                w32(pv + 4, u(ri(1, 10)));
                for (uint32_t k = 0; k < 10; ++k) w8(pv + 0x10 + k, u(ri(0, 9)));
                if (coin()) wf(pv, rf(0.5f, 3));
                if (coin()) wf(pv + 8, rf(0.1f, 1.2f));
                w32(pv + 0x34, u(ri(0, 120)));
                const uint32_t other = ee_read<uint32_t>(moby_at(ee_read<int32_t>(pv + 0xc)) + 0x78);
                if (coin()) {  // the level's two thresholds (0 on the disc: the colour blend then divides by 0) set
                    const int32_t l = ri(5, 60);
                    w32(ORACLE_LADDR(10, 0x00161d68), u(l));
                    w32(ORACLE_LADDR(10, 0x00161d64), u(l + ri(5, 80)));
                }
                const int32_t lo = ee_read<int32_t>(ORACLE_LADDR(10, 0x00161d68)), hi = ee_read<int32_t>(ORACLE_LADDR(10, 0x00161d64));
                for (uint32_t row = 0; row < 4; ++row)
                    for (uint32_t k = 0; k < 10; ++k) w32(other + 0x10000 + 0x2c00 + k * 4 + row * 0x78, u(ri(0, std::max(hi, lo) + 10)));
                wf(other + 0x10000 + 0x2de0, rf(0, 1));
                if (coin()) {  // the grid's points (by row, column and lamp) spread about the panel
                    for (uint32_t row = 0; row < 4; ++row)
                        for (uint32_t col = 0; col < 20; ++col)
                            for (uint32_t k = 0; k < 10; ++k)
                                ee_ref<Vec4>(other + row * 0x2580 + col * 0x140 + k * 0x10) =  // (in the strip's own frame: its matrix places it)
                                    Vec4{static_cast<float>(col) * 0.15f - 1.5f + rf(-0.02f, 0.02f), static_cast<float>(k) * 0.1f - 0.5f, static_cast<float>(row) * 0.3f, 1.0f};
                }
                const float half_h = static_cast<float>(ee_read<int32_t>(pv + 4)) * ee_read<float>(pv + 8) * 0.5f, half_w = ee_read<float>(pv) * 0.5f;
                const Vec4 q = ee_ref<Vec4>(m + 0x10);
                const Vec4 at{q.x, q.y, q.z + half_h, std::sqrt(half_h * half_h + half_w * half_w)};
                if (ri(0, 2) == 0) {  // the camera about the fade's start (48 from the panel)
                    for (int k = 0; k < 100; ++k) {
                        const float a = rf(-3.1f, 3.1f), e = rf(-0.3f, 0.3f), d = coin() ? rf(47.05f, 47.95f) : rf(48.05f, 48.95f);
                        ee_ref<Vec4>(OV(0x00166dc0)) = Vec4{q.x + d * std::cos(a) * std::cos(e), q.y + d * std::sin(a) * std::cos(e), q.z + d * std::sin(e), 1.0f};
                        render_view_setup();
                        ee_ref<Vec4>(T4) = at;
                        if (point_on_screen(T4, 80.0f) != -1) break;
                    }
                } else {
                    camera_sees(at, 80.0f, coin() ? 30.0f : 70.0f);
                }
                if (coin()) wfrand(m + 0x40, 3, -0.4f, 0.4f);
                w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);
            };
            if (panels.empty()) std::printf("%-26s L10:0x%08x  no panel on this level\n", "p10_cb2da690", 0x002da690u);
            else if (only == nullptr || std::strcmp(only, "p10_cb2da690") == 0)
                for (int st = 0; st < 2; ++st)
                    classcheck("p10_cb2da690", 0x002da690, orx_cb_2da690_l10, [panel_case, st](uint32_t m) { panel_case(m, st); seed_rand(); }, 20, panels);
            if (!panels.empty() && (only == nullptr || std::strcmp(only, "p10_drawlist") == 0) && g_oracle_level == 10 &&
                (std::getenv("ORACLE_ONLY") == nullptr || std::strcmp(std::getenv("ORACLE_ONLY"), "p10_drawlist") == 0)) {
                l10check("p10_drawlist", 0x001ecc50, 30, Ret::None, [level, panels, panel_case] {
                    level();
                    w32(OV(0x0015f428), 0);
                    for (const uint32_t m : panels) {
                        if (ri(0, 3) == 0) continue;
                        panel_case(m, ri(0, 1));
                        orx_class1067_update_l10(m);  // (its update queues the callback while it is seen)
                    }
                    w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);
                    if (std::getenv("B122_INFO") != nullptr) std::printf("INFO p10_drawlist queued %d\n", ee_read<int32_t>(OV(0x0015f428)));
                    seed_rand();
                    return Args{};
                }, [](A) { draw_list_da80(); return uint64_t{0}; });
            }
        }
        g_extra_noop_hooks.clear();
    }

    // ---- Y1: class 827's hook +0x10 (table 0x001dde58: Blarg's L6:0x2ea800, word for word; not Veldin's 0x2d5800, the overlay map's
    // pair) through the level's dispatch class_hook_call_level, the moby 0 in half, its pvars 0 in a sixth
    if (g_oracle_level == 10) {
        g_extra_noop_hooks = {0x00122298, 0x00120478, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fea8};
        auto chk = [](const char* nm, uint32_t a, int n, Ret r, const std::function<Args()>& g, const std::function<uint64_t(const Args&)>& p) {
            l10check(nm, a, n, r, g, p);
        };
        class_hook_deal_checks(chk, level, {0x001dde58u}, {{"y1_hook_2ce838", 0x2ce838, 0x10, 0, 0}}, 120);
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
