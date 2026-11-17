// Oracle batch 119 (task 0352): level 7's (Umbris) own moby classes on its real level data. Run with
// ORACLE_LEVEL=7 build/tests/ovl_oracle --batch 119 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is level 7's own function at its own address, the port the one registered for it (the
// transliterations umbris_classes_tr_N.cpp).

#include "ovl_oracle.h"

#include <game/ovl/carry_hooks_levels.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/level/level_data.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/shared_boss_bar.h>
#include <game/ovl/shared_breakables.h>
#include <game/ovl/shared_class1135.h>
#include <game/ovl/shared_class1221.h>
#include <game/ovl/shared_class258.h>
#include <game/ovl/shared_class341.h>
#include <game/ovl/shared_class447.h>
#include <game/ovl/shared_class631.h>
#include <game/ovl/shared_class823.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_class848.h>
#include <game/ovl/shared_class886.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_movers.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/shared_swingshot_target.h>
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
    static const char* const kNames[] = {"class1059", "class1106", "class1110", "class1112", "p71_class1049",
                                         "p71_class882", "p71b_class1059_a", "p71b_class1059_b", "p71b_class1059_c",
                                         "p71b_f30eee8", "p71b_f30f0f8", "p71b_f30f308", "p71b_f30f4d0", "class1134",
                                         "class1135", "class436", "class737", "class805", "class871", "p71_class1041",
                                         "p71_class436", "p71_class871", "p71_class880", "p71b_class1059_setup",
                                         "p71b_f30ebc8", "p71b_f30ed30"};
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


// A check whose original is level 7's own function at `l6addr` (not a Veldin address to translate).
void l7check(const char* name, uint32_t l6addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B119_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 7) {
        std::printf("%-26s L7:0x%08x  skipped (level 7 only: run with ORACLE_LEVEL=7)\n", name, l6addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L7:0x%08x\n", name, l6addr);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2b_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, l6addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_119() {
    if (g_only_batch != 119) return;
    g_batch = 119;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // segment_collide's hit point (the global at Veldin 0x174610, 3 words): written by the callee from the segment's end points; class 911's
    // state 1 builds the segment from stale stack lanes (the port's EE stack and the interpreter's hold other leftovers), nothing reads it
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(0x00174610u + k);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the console clock / the disc reads the originals poll (IOP calls): the host's on both sides, as batch 138 does (task 0391: the Gold Bolt
    // 1134's state 2 starts a dialogue stream whose read the original polls: it was skipped. Class 436's states 1 / 5 / 10 / 11 stay skipped:
    // their cutscene_start waits in the music streams' frame 0x265aa8 for the IOP; cutscene_start itself is batch 80's)
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }}};
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
            if (std::getenv("B119_REVISIT") != nullptr) {  // (batch 310's question: the level finished and every moby broken / collected
                w8(0x0013dd58 + 7, 2);                       // once, every checkpoint passed: a return to Umbris after the end)
                w8(0x0013dd48, 1);
                for (uint32_t w = 0; w < 0x40; ++w) w32(0x0014c190 + 7 * 0x100 + w * 4, 0xffffffffu);
                for (uint32_t c = 0; c < 0x10; ++c) w8(0x0014c050 + 7 * 0x10 + c, 0xff);
            }
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's mobys whose update function is `fn` (level 7's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60, std::vector<uint32_t> subjects = {}) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 7) { l7check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = subjects.empty() ? mobys_with(fn) : subjects;
        if (ms.empty()) { std::printf("%-26s L7:0x%08x  no moby on this level\n", name, fn); return; }
        l7check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    std::vector<uint32_t> effect_lanes;  // (the shared debris_spawn's / explosion_effect's spawns: P7.1 names them for its own checks that make them)
    if (g_oracle_level == 7) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { effect_lanes.push_back(a + 0xc); effect_lanes.push_back(a + 0x1c); }
        {  // the particle pool (0x800 records of 0x40 bytes at *(0x16016c)): the w lane of the velocity vector a spray copies (a stale lane of vec_from_spherical in the original)
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            for (uint32_t k = 0; k < 0x800; ++k) effect_lanes.push_back(pool + k * 0x40 + 0x2c);
        }
        g_ignored_words.insert(g_ignored_words.end(), effect_lanes.begin(), effect_lanes.end());
    }

    // ---- the level's own classes (transliterated by tools/codegen/generate_all.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};  // (+ the level change request: the original would run the CD / movie path)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; bool walk = false; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class38", 0x002cd2b0, umb_class38_update_l7, 0x0, false},
        {"class1474", 0x002cdb28, umb_class1474_update_l7, 0x0, false},
        {"class258", 0x002f2688, class258_update, 0x0, false},
        {"class341", 0x002f5508, class341_update, 0x0, false},
        {"class436", 0x002f5ba0, umb_class436_update_l7, 0xc22, false},
        {"class447", 0x002f61c0, class447_update, 0x0, false},
        {"class460", 0x002f8058, sea_update, 0x0, false},
        {"class529", 0x002fbdb8, umb_class529_update_l7, 0x0, false},
        {"class532", 0x002fbdf0, empty_update, 0x0, false},
        {"class605", 0x002fd698, zone_marker_update, 0x0, false},
        {"class631", 0x002fd908, class631_update, 0x0, false},
        {"class696", 0x00300338, rock_update, 0x0, false},
        {"class737", 0x00300970, scripted_camera_trigger_update_l1, 0x0, false},
        {"class758", 0x00302238, swingshot_target_update, 0x0, false},
        {"class805", 0x003027a0, spawn_point_update_l1, 0x0, false},
        {"class823", 0x00304738, class823_update, 0x0, false},
        {"class832", 0x00304e50, class832_update, 0x0, false},
        {"class848", 0x00304ff0, class848_update, 0x0, false},
        {"class871", 0x0030b000, umb_class871_update_l7, 0x0, false},
        {"class880", 0x0030b6a8, umb_class880_update_l7, 0x0, false},
        {"class882", 0x0030bbc8, umb_class882_update_l7, 0x0, false},
        {"class886", 0x0030bf90, class886_update, 0x0, false},
        {"class902", 0x0030c508, umb_class902_update_l7, 0x0, false},
        {"class1013", 0x0030cf90, umb_class1013_update_l7, 0x0, false},
        {"class1018", 0x0030d440, umb_class1018_update_l7, 0x0, false},
        {"class1041", 0x0030d4d8, umb_class1041_update_l7, 0x0, false},
        {"class1046", 0x0030e1f8, umb_class1046_update_l7, 0x0, false},
        {"class1049", 0x0030e458, umb_class1049_update_l7, 0x0, false},
        {"class1059", 0x0030f5f0, umb_class1059_update_l7, 0x0, false},
        {"class1063", 0x00310df0, mover_door_update, 0x0, false},
        {"class1069", 0x003112c8, umb_class1069_update_l7, 0x0, false},
        {"class1080", 0x00311bc8, umb_class1080_update_l7, 0x0, false},
        {"class1104", 0x00311eb0, empty_update, 0x0, false},
        {"class1106", 0x00314150, umb_class1106_update_l7, 0x0, false},
        {"class1110", 0x00319040, umb_class1110_update_l7, 0x0, false},
        {"class1112", 0x003196e0, umb_class1112_update_l7, 0x0, false},
        {"class1113", 0x00319f48, umb_class1113_update_l7, 0x0, false},
        {"class1126", 0x0031a250, umb_class1126_update_l7, 0x0, false},
        {"class1128", 0x0031a9e8, umb_class1128_update_l7, 0x0, false},
        {"class1129", 0x0031aee0, mover_platform_update, 0x0, false},
        {"class1133", 0x0031b3f0, umb_class1133_update_l7, 0x0, false},
        {"class1134", 0x0031b5f0, gold_bolt_update, 0x0, false},
        {"class1135", 0x0031c528, teleporter_update_l1, 0x0, false},
        {"class1142", 0x0031d2e0, umb_class1142_update_l7, 0x0, false},
        {"class1221", 0x0031d9f8, class1221_update, 0x0, false},
        {"class1222", 0x0031dae0, class1221_update, 0x0, false},
        {"class1552", 0x0031eba8, umb_class1552_update_l7, 0x0, false},
        {"class1650", 0x0031f3d8, class1650_update, 0x0, false},
        {"class1651", 0x0031f5a8, empty_update, 0x0, false},
        {"class1663", 0x0031f5b0, class1663_update, 0x0, false},
        {"class1686", 0x0031f780, umb_class1686_update_l7, 0x0, false},
        {"class1789", 0x0031f900, umb_class1789_update_l7, 0x0, false},
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
    if (g_oracle_level == 7 && std::getenv("B119_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B119_CLS") != nullptr && std::strcmp(std::getenv("B119_CLS"), c.name) != 0) continue;
        // the hand ports (P7.1): the batch's base list, none of the loop's (the spawns' and particle records' w lanes)
        const bool hand = c.fn == umb_class38_update_l7 || c.fn == umb_class1474_update_l7 || c.fn == umb_class436_update_l7 || c.fn == umb_class529_update_l7 ||
                          c.fn == umb_class871_update_l7 || c.fn == umb_class880_update_l7 || c.fn == umb_class882_update_l7 || c.fn == umb_class902_update_l7 ||
                          c.fn == umb_class1013_update_l7 || c.fn == umb_class1018_update_l7 || c.fn == umb_class1041_update_l7 || c.fn == umb_class1046_update_l7 ||
                          c.fn == umb_class1049_update_l7 || c.fn == umb_class1059_update_l7;
        const std::vector<uint32_t> loop_ignored = g_ignored_words;
        if (hand) {
            g_ignored_words = saved_ignored;
            if (c.fn == umb_class871_update_l7 || c.fn == umb_class880_update_l7 || c.fn == umb_class882_update_l7 || c.fn == umb_class1041_update_l7 || c.fn == umb_class1049_update_l7 ||
                c.fn == umb_class1059_update_l7)
                g_ignored_words.insert(g_ignored_words.end(), effect_lanes.begin(), effect_lanes.end());
        }
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 7 && std::getenv("B119_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B119_ST") == nullptr || std::atoi(std::getenv("B119_ST")) == st) && (std::getenv("B119_PASS") == nullptr || std::atoi(std::getenv("B119_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn, c.walk); }, 12);
        }
        g_ignored_words = loop_ignored;
    }
    g_extra_noop_hooks.clear();

    // ---- level 7's sound emitter types (3, 6: section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 7) {
        struct Em { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Em ems_t[] = {{"emitter3", 0x00329740, emitter_reverb_box_update}, {"emitter6", 0x003298a0, emitter_music_box_update}};
        for (const Em& e : ems_t) {
            level();
            std::vector<uint32_t> ems;
            for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
                const uint32_t em = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
                if (ee_read<uint32_t>(em + 4) == e.addr) ems.push_back(em);
            }
            if (ems.empty()) { std::printf("%-26s L7:0x%08x  no emitter of the type on this level\n", e.name, e.addr); continue; }
            if (std::getenv("B119_INFO") != nullptr) std::printf("INFO %s 0x%08x emitters %zu\n", e.name, e.addr, ems.size());
            auto fn = e.fn;
            l7check(e.name, e.addr, 120, Ret::None, [level, ems] {
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


    // ---- level 7's camera type 0x12 (accept / init / update / exit: section 4 of the overlay)
    if (g_oracle_level == 7) {
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
        static const CamT cams[] = {{18, 0x003290b8, 0x00329120, 0x00329128, 0x00329130}};
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
            l7check(name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, type] { uint32_t other = 0; const uint32_t cam = cam_setup(type, true, &other); return Args{{cam, other}, {}}; },
                    [type](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(cam_type18_accept(a.i[0], a.i[1])) : 0u); });
            l7check(name("init"), ct.init, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            l7check(name("update"), ct.update, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
            l7check(name("exit"), ct.exit, 100, Ret::None, [cam_setup, type] { return Args{{cam_setup(type, false, nullptr)}, {}}; },
                    [type](A a) { if (a.i[0] != 0) empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- level 7's camera hook (0x328090, the pointer its setter 0x328058 puts at 0x167090) and the callback it queues (0x3280f0)
    if (g_oracle_level == 7) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167080;
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x00328090u);
            if (ri(0, 3) == 0) w32(OV(0x0015ef4c), u(ri(0, 15)));  // the queue's count (it holds up to 16)
            wf(0x00161e24, rf(0, 0.5f));  // $gp - 0x4ddc: the frame step the callback counts down by
            w32(OV(0x0015ef4c), u(ri(0, 14)));
        };
        l7check("cam0_hook", 0x00328090, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l7check("cam_callback", 0x003280f0, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- level 7's class hook +0xc 0x2f44b0 (Veldin's 0x2d57d0 with one constant; the class' hook table at +0x2c): a0 = the moby, a1 = a position, a2 = a target moby, f12 a float
    // (the carry hooks' shape: their callees read all four), on the mobys whose class owns the hook
    if (g_oracle_level == 7) {
        g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        struct Hk { const char* name; uint32_t addr; uint32_t slot; TrRet (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float); };
        static const Hk hks[] = {
            {"hook_c_2f44b0", 0x002f44b0, 0xc, hook_uniform<umb_carry_hookc_l7>},
        };
        for (const Hk& hk : hks) {
            level();
            std::vector<uint32_t> own;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                const uint32_t cls = ee_read<uint32_t>(m + 0x24);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + hk.slot) == hk.addr) own.push_back(m);
            }
            if (own.empty()) { std::printf("%-26s L7:0x%08x  no moby owns it on this level\n", hk.name, hk.addr); continue; }
            if (std::getenv("B119_INFO") != nullptr) std::printf("INFO %s 0x%08x owners %zu\n", hk.name, hk.addr, own.size());
            auto fn = hk.fn;
            l7check(hk.name, hk.addr, 40, Ret::Int32, [level, own] {
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

    // ---- level 7's own builds of two engine functions: level_sound_play (L7:0x002b4a00) and cutscene_level_extras (L7:0x00265bd8)
    if (g_oracle_level == 7) {
        l7check("level_sound_play", 0x002b4a00, 200, Ret::Int32, [level] {
            level();
            return Args{{u(ri(-2, 12)), R() & 0xffff, ri(0, 3) == 0 ? 0u : OV(0x00173e40u), u(ri(0, 0x400))}, {}};
        }, [](A a) { return r32(static_cast<uint32_t>(sound_play_indexed(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2]))); });  // (L7:0x2b4a00 is Veldin's sound_play_indexed: the same code)
        l7check("cutscene_level_extras", 0x00265bd8, 100, Ret::None, [level] {
            level();
            if (ri(0, 2) == 0) w32(OV(0x0015ed84), ri(0, 1) ? 7u : u(ri(0, 18)));
            return Args{};
        }, [](A) { umb_cutscene_level_extras_u(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); return uint64_t{0}; });
    }

    // T00: the level's bare `jr ra` functions its classes and cameras reach through a pointer (class updates, camera
    // callbacks; the scan pairs them with Veldin's 0x1f6bf8): empty_update runs in their places, each against the original
    // called directly (the level may have no moby of the class)
    if (g_oracle_level == 7) {
        static const uint32_t empties[] = {0x002fbdf0, 0x00311eb0, 0x0031f5a8, 0x00329120, 0x00329128, 0x00329130};
        for (const uint32_t a : empties) {
            char nm[24];
            std::snprintf(nm, sizeof nm, "empty_%x", a);
            l7check(nm, a, 20, Ret::None, [level] {
                level();
                return Args{{R(), R(), R(), R()}, {rf(-9, 9)}};
            }, [](A a) { empty_update(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- P7.1: the hand ports of Umbris's own code, part 1 (umbris_lifts / _director / _path_group / _shots / _water / _doors /
    // _turret / _snagglebeast_shots.cpp): each update on states made for its branches (its subjects spawned where the level has
    // none), each helper called directly. They need none of the class loop's ignores (the spawns' and the particle records' w
    // lanes): only the batch's base list, plus the words a check names for itself. Class 436's generator stays short of its
    // talks (cutscene_start), its movie and its level change: those paths are for a play test.
    if (g_oracle_level == 7) {
        const std::vector<uint32_t> loop_ignored = g_ignored_words;
        g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x002763f8};
        std::vector<uint32_t> own_ignored;  // (a check's own words: set before it, cleared after)
        auto p71 = [&](const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
            g_ignored_words = saved_ignored;
            g_ignored_words.insert(g_ignored_words.end(), own_ignored.begin(), own_ignored.end());
            l7check(name, addr, n, ret, gen, port);
            g_ignored_words = loop_ignored;
            own_ignored.clear();
        };
        const uint32_t P = 0x0013f350, CAM = L7_ADDR(0x00166e40), X = 0x01e00000;  // the hero, the camera, the test area
        const uint32_t T0 = L7_ADDR(0x0015ffd8), FRAME = L7_ADDR(0x0015f5cc);       // the mobys' table, the level's frame counter
        // the frame rate: 60 Hz (the snapshot's), or 50 Hz one case in four
        auto rates = [] {
            if (ri(0, 3) != 0) return;
            wf(0x0015ed68, 1.2f);
            wf(0x0015ed6c, 0.02f);
            wf(0x0015ed70, 0.0004f);
        };
        bool keep_frame = false;  // (a generator that set the frame counter itself)
        auto finish = [rates, &keep_frame, FRAME] {
            rates();
            if (!keep_frame) w32(FRAME, R() & 0xffff);
            keep_frame = false;
            seed_rand();
        };
        auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto near = [](uint32_t dst, uint32_t at, float r, float dz) {
            Vec4 q = ee_ref<Vec4>(at);
            q.x += rf(-r, r); q.y += rf(-r, r); q.z += rf(-dz, dz);
            ee_ref<Vec4>(dst) = q;
        };
        auto index_of = [T0](uint32_t o) { return u(static_cast<int32_t>(o - ee_read<uint32_t>(T0)) >> 8); };
        auto moby_at = [T0](uint32_t i) { return ee_read<uint32_t>(T0) + (i << 8); };
        auto any_moby = [] {  // a live moby of the level
            for (int k = 0; k < 50; ++k) {
                const uint32_t lo = ee_read<uint32_t>(OV(0x0015ffd8)), hi = ee_read<uint32_t>(OV(0x0015ffe0));
                const uint32_t m = lo + (u(ri(0, static_cast<int>((hi - lo) >> 8) - 1)) << 8);
                if (ee_read<uint8_t>(m + 0x20) < 0xfe) return m;
            }
            return ee_read<uint32_t>(OV(0x0015ffd8));
        };
        // a moby of class `cls` spawned near `at` (0: the class is not on the level)
        auto spawn_at = [near](uint32_t cls, uint32_t at, float r) {
            const uint32_t m = moby_spawn(cls);
            if (m == 0) return 0u;
            near(m + 0x10, at, r, 1.0f);
            moby_update_transform(m);
            return m;
        };
        // a sound slot: none, a playing one of the moby's, another's, an idle one (`half`: a halfword slot)
        auto sound_slot = [](uint32_t m, uint32_t at, bool half) {
            const int k = ri(0, 3);
            if (k == 0) { if (half) w16(at, 0xffffu); else w32(at, 0xffffffffu); return; }
            const uint32_t s = u(ri(0, 15)), e = 0x0013e550 + s * 0x70;
            if (half) w16(at, s); else w32(at, s);
            w32(e + 0x88, k == 3 ? R() : m);
            w8(e + 0x74, k == 2 ? 0u : 1u);
        };
        auto hero_in_box = [P](int32_t box) {  // the hero at the box's centre (when it has one)
            if (box < 0) return;
            Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, box) + 0x30);
            c.x += rf(-0.2f, 0.2f); c.y += rf(-0.2f, 0.2f);
            ee_ref<Vec4>(P + 0x80) = c;
        };
        auto hero_on = [P](uint32_t m) {  // the hero standing on `m`, on something else, or in the air
            const int k = ri(0, 2);
            w32(P + 0x2fc, k == 0 ? m : (k == 1 ? R() : 0u));
            w16(P + 0x30e, ri(0, 4) == 0 ? 1u : 0u);
        };
        // the moby's hit: none, or a hit with the kinds of `mask` (or others) by the hero / nobody / `by`, its damage
        auto hit_on = [give_hit, P](uint32_t m, uint32_t mask, uint32_t by) {
            give_hit(m, mask);
            if (ee_read<uint8_t>(m + 0xa4) == 0xff) return;
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
            const int k = ri(0, 3);
            w32(e + 0x20, k == 0 ? 0u : (k == 1 && by != 0 ? by : ee_read<uint32_t>(P + 0x2080)));
            wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
        };
        // the camera near `at`, at a middle distance, far, or where it is
        auto camera_near = [near, CAM](uint32_t at) {
            const int k = ri(0, 3);
            if (k == 0) near(CAM, at, 5.0f, 3.0f);
            else if (k == 1) near(CAM, at, 40.0f, 10.0f);
            else if (k == 2) near(CAM, at, 120.0f, 30.0f);
        };
        // the level's out-of-box values for a shot's place (2 .. 1021): one axis put just out, one case in five
        auto out_of_box = [](uint32_t m) {
            if (ri(0, 4) != 0) return;
            wf(m + 0x10 + u(ri(0, 2)) * 4, coin() ? rf(1.0f, 1.99f) : rf(1021.01f, 1022.0f));
        };
        auto view_on = [](uint32_t at) {  // the view's camera put behind `at` along its forward axis (the point on screen)
            const Vec4 r0 = ee_ref<Vec4>(OV(0x0016ccc0)), r1 = ee_ref<Vec4>(OV(0x0016ccd0)), r2 = ee_ref<Vec4>(OV(0x0016cce0));
            Vec4& cam = ee_ref<Vec4>(OV(0x0016ccf0));
            const Vec4 c = ee_ref<Vec4>(at);
            const float k = coin() ? rf(5, 60) : -rf(5, 60);  // (either way along the axis: the one in front)
            cam.x = c.x * cam.w - r0.z * k; cam.y = c.y * cam.w - r1.z * k; cam.z = c.z * cam.w - r2.z * k;
        };
        auto ripple_due = [] {  // the RNG's state set so that its next rand_mod(200) is 0 (902's ripple, 1 in 200)
            const uint32_t st = ee_read<uint32_t>(0x0012f76c) + 0x58;
            for (int k = 0; k < 4000; ++k) {
                const uint32_t v = R();
                w32(st, v);
                const bool hit = rand_mod(200) == 0;
                w32(st, v);
                if (hit) return;
            }
        };
        level();
        if (std::getenv("B119_INFO") != nullptr) {
            for (uint32_t fn : {0x0030b000u, 0x0030d4d8u, 0x0030f5f0u})
                for (uint32_t m : mobys_with(fn)) std::printf("INFO p71 hit kind %08x %u\n", fn, ee_read<uint8_t>(ee_read<uint32_t>(m + 0x78) + 0x28));
        }
        if (std::getenv("B119_INFO") != nullptr) std::printf("INFO p71 L7 0x15f5c4 -> %08x = %d, frame %08x\n", L7_ADDR(0x0015f5c4), ee_read<int32_t>(L7_ADDR(0x0015f5c4)), FRAME);

        // classes 38 / 1474 (the lifts; 1474 checked on class 38's moby too: the same pvars)
        {
            const std::vector<uint32_t> lifts = mobys_with(0x002cd2b0);
            auto lift_prep = [&](uint32_t m, bool w1474) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, ri(0, 4) == 0 ? 0u : 1u);
                w8(pv + 0xa0, coin() ? 1u : 0u);
                if (ri(0, 7) == 0) w32(pv + 0xb4, 0xffffffffu);
                if (w1474) w8(L7_ADDR(0x0015fc88) + ee_read<uint8_t>(m + 0xb0), ri(0, 3) == 0 ? u(ri(0, 0xfe)) : 0xffu);
                const int st = ri(0, w1474 ? 4 : 3);
                w8(m + 0xbc, u(st));
                if (st == 0) { const float step = ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) / 60.0f; wf(m + 0x2c, coin() ? rf(0.0f, 2.0f * step) : rf(0.05f, 3.0f)); }
                const int32_t pi = ee_read<int32_t>(pv + 0xb4);
                if (pi >= 0) {
                    const uint32_t path = lvd::path(pi), n = ee_read<uint32_t>(path);
                    near(P + 0x80, coin() ? path + 0x10 : path + (n << 4), 3.0f, 1.0f);
                    if (st == 2) {
                        wf(pv + 0xa4, coin() ? rf(-0.1f, 1.1f) : (coin() ? rf(-0.02f, 0.02f) : rf(0.98f, 1.02f)));
                        wf(pv + 0xa8, rf(-0.05f, 0.05f));
                        wf(pv + 0xac, coin() ? rf(-0.02f, 0.02f) : rf(-0.2f, 0.2f));
                        wf(pv + 0xbc, rf(-0.2f, 0.1f));
                        if (coin()) { near(P + 0x80, m + 0x10, 1.5f, 0.0f); wf(P + 0x88, ee_read<float>(m + 0x18) - (coin() ? rf(4.2f, 8.0f) : rf(-1.0f, 4.0f))); }
                    }
                }
                if (ri(0, 4) == 0) near(P + 0x80, m + 0x10, 3.0f, 1.0f);
                wf(pv + 0xb0, rf(0.5f, 6.0f));
                w16(pv + 0xb8, coin() ? 1u : 0u);
                w16(pv + 0xba, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                w32(pv + 0xc8, coin() ? 1u : 0u);
                sound_slot(m, pv + 0xc4, false);
                hero_on(m);
                finish();
            };
            if (!lifts.empty()) {
                p71("p71_class38", 0x002cd2b0, 200, Ret::None, [&] { level(); const uint32_t m = pick(lifts); lift_prep(m, false); return Args{{m}, {}}; },
                    [](A a) { umb_class38_update_l7(a.i[0]); return uint64_t{0}; });
                p71("p71_class1474", 0x002cdb28, 240, Ret::None, [&] { level(); const uint32_t m = pick(lifts); lift_prep(m, true); return Args{{m}, {}}; },
                    [](A a) { umb_class1474_update_l7(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 436 (the story director): its states short of the talks, the movie and the level change
        {
            const std::vector<uint32_t> dirs = mobys_with(0x002f5ba0);
            if (!dirs.empty()) {
                p71("p71_class436", 0x002f5ba0, 400, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(dirs);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    static const int sts[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 4, 4, 4, 8, 8, 8, 12, 12};
                    const int st = sts[ri(0, 25)];
                    w8(m + 0x20, u(st));
                    const uint32_t S = 0x0013d388;
                    for (uint32_t k = 0x28; k <= 0x2a; ++k) w8(S + k, coin() ? 1u : 0u);
                    w32(L7_ADDR(0x0015f5c4), u(ri(0, 2)));
                    int32_t mine[4];  // (its own four: a random moby retired can be one the original's retire never returns from)
                    for (uint32_t o = 0x8; o <= 0x14; o += 4) mine[(o - 8) / 4] = ee_read<int32_t>(pv + o);
                    for (uint32_t o = 0x8; o <= 0x14; o += 4) {  // its mobys: kept, none, another of its own; removed or dying
                        const int k = ri(0, 4);
                        if (k == 0) w32(pv + o, 0xffffffffu);
                        else if (k == 1) w32(pv + o, u(mine[ri(0, 3)]));
                        const int32_t i = ee_read<int32_t>(pv + o);
                        if (i != -1 && ri(0, 3) == 0) w8(moby_at(u(i)) + 0x20, coin() ? 0xfeu : 0xfdu);
                    }
                    for (uint32_t o = 0x0; o <= 0x4; o += 4) if (ri(0, 4) == 0) w32(pv + o, 0xffffffffu);
                    for (uint32_t o = 0x18; o <= 0x1c; o += 4) if (ri(0, 4) == 0) w32(pv + o, 0xffffffffu);
                    // the hero in a volume (the start's or the trapdoor's) or not
                    if (coin()) hero_in_box(ee_read<int32_t>(pv + (coin() ? 0x0u : 0x4u)));
                    // (no talk, movie or level change: a scene playing in 1 .. 2 / 5 / 9 .. 11 or the story's bytes set)
                    if (st == 1) w8(S + 0x28, 1);
                    if (st == 5 || st == 9 || st == 10 || st == 11) w32(L7_ADDR(0x0015f5c4), 2);
                    if (st == 4) w8(S + 0x29, coin() ? 1u : ee_read<uint8_t>(S + 0x29));
                    if (st == 4 && ee_read<uint8_t>(S + 0x29) == 0 && ee_read<int32_t>(pv + 0x0) != -1) {
                        // (outside the start's volume: far from it)
                        wf(P + 0x80, ee_read<float>(P + 0x80) + 500.0f); wf(P + 0x84, ee_read<float>(P + 0x84) + 500.0f);
                    }
                    if (st == 8 && ee_read<uint8_t>(S + 0x2a) == 0 && ee_read<int32_t>(pv + 0x4) != -1) {
                        // (the trapdoor's talk out of reach: its gate alive, or the hero far from the volume)
                        const int32_t g = ee_read<int32_t>(pv + 0xc);
                        const bool gate_alive = g != -1 && moby_at(u(g)) != 0 && ee_read<uint8_t>(moby_at(u(g)) + 0x20) < 0xfd;
                        if (!gate_alive) { wf(P + 0x80, ee_read<float>(P + 0x80) + 500.0f); wf(P + 0x84, ee_read<float>(P + 0x84) + 500.0f); }
                    }
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class436_update_l7(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 529 and the sprite-cache flush 0x270248 (the bitmap 0x184270 +0xc, 64 bytes a row: random bits)
        {
            const std::vector<uint32_t> props = mobys_with(0x002fbdb8);
            if (!props.empty())
                p71("p71_class529", 0x002fbdb8, 30, Ret::None, [&] { level(); const uint32_t m = pick(props); w8(m + 0x30, R() & 0xff); finish(); return Args{{m}, {}}; },
                    [](A a) { umb_class529_update_l7(a.i[0]); return uint64_t{0}; });
            p71("p71_f270248", 0x00270248, 80, Ret::None, [&] {
                level();
                const int32_t col = ri(0, 4) == 0 ? ri(-9, -1) : ri(0, 200), row = ri(0, 60), cols = ri(0, 3) == 0 ? ri(-2, 0) : ri(1, 24), rows = ri(0, 4) == 0 ? 0 : ri(1, 5);
                const uint32_t bm = ee_read<uint32_t>(L7_ADDR(0x00184270) + 0xc);
                for (int32_t r = row; r < row + rows && r < 0x80; ++r)
                    for (int32_t c = (col > 8 ? col - 8 : 0); c < col + cols + 8; c += 8) {
                        const uint32_t b = bm + u(r << 6) + u((c > -1 ? c : c + 7) >> 3);
                        const int k = ri(0, 3);
                        w8(b, k == 0 ? 0u : (k == 1 ? 0xffu : R() & 0xff));
                    }
                finish();
                return Args{{u(col), u(row), u(cols), u(rows)}, {}};
            }, [](A a) { umb_270248_l7(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
        }

        // class 871 (the path group's leader) and 0x30af40 (a member put back): set up (the members spread), moving them (members
        // gone, timers, the leader's segment), hit (its health), no path / no group / fewer members than it says
        {
            const std::vector<uint32_t> leaders = mobys_with(0x0030b000);
            auto members = [X](uint32_t pv) {
                std::vector<uint32_t> v;
                w32(X + 0x100, 0);
                if (group_first(X + 0x100, ee_read<int32_t>(pv + 0x74), 1, 0) != 0) return v;
                for (int k = 0; k < 64 && ee_read<uint32_t>(X + 0x100) != 0; ++k) {
                    v.push_back(ee_read<uint32_t>(X + 0x100));
                    if (group_next(X + 0x100, ee_read<uint32_t>(X + 0x100), 1, 0) != 0) break;
                }
                return v;
            };
            if (!leaders.empty()) {
                own_ignored = effect_lanes;
                p71("p71_class871", 0x0030b000, 260, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(leaders);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const int st = ri(0, 4) == 0 ? 0 : (ri(0, 5) == 0 ? 2 : 1);
                    if (st != 0) {  // (the set-up done first: the members on the path)
                        w8(m + 0x20, 0);
                        umb_class871_update_l7(m);
                    }
                    w8(m + 0x20, u(st));
                    w8(pv + 0x94, coin() ? 1u : 0u);
                    w8(pv + 0x95, coin() ? 1u : 0u);
                    if (ri(0, 7) == 0) w32(pv + 0x70, 0xffffffffu);
                    if (st == 0 && ri(0, 5) == 0) w32(pv + 0x74, coin() ? 0xffffffffu : 0x7fffu);
                    if (st == 0 && ri(0, 3) == 0) w32(pv + 0x78, u(ri(0, 12)));
                    wf(pv + 0x7c, coin() ? 0.0f : rf(0.0f, 30.0f));
                    wf(pv + 0x80, rf(0.5f, 8.0f));
                    w32(pv + 0x84, coin() ? 0xffffffffu : 0u);
                    if (st == 1) {
                        for (uint32_t o : members(pv)) {
                            const uint32_t mp = ee_read<uint32_t>(o + 0x78);
                            const int k = ri(0, 3);
                            if (k == 1) w8(o + 0x20, 0xfe); else if (k == 2) w8(o + 0x20, 0xfd);
                            w32(mp + 0x98, u(ri(0, 2)));
                            if (ri(0, 2) != 0) w32(mp + 0x90, ee_read<uint32_t>(pv + 0x90));
                        }
                    }
                    hit_on(m, 0x00230000u, 0);
                    if (ri(0, 2) == 0) w8(pv + 0x28, 0);  // (its hit state's kind 0: the reactions 1 / 2 of the table 0x1b0180)
                    wf(pv + 0x20, coin() ? rf(0.0f, 3.0f) : rf(3.0f, 20.0f));
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class871_update_l7(a.i[0]); return uint64_t{0}; });
                p71("p71_f30af40", 0x0030af40, 60, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(leaders);
                    w8(m + 0x20, 0);
                    umb_class871_update_l7(m);
                    std::vector<uint32_t> ms = members(ee_read<uint32_t>(m + 0x78));
                    if (ms.empty()) ms.push_back(m);  // (its callers' subjects: the group's members; a random moby can be one whose grid move never returns)
                    const uint32_t o = pick(ms);
                    if (coin()) { w8(o + 0x31, 0); w8(o + 0x30, R() & 0xff); w8(o + 0xbc, R() & 0xff); }  // (its fields not already as it sets them)
                    if (const uint32_t hs = ee_read<uint32_t>(ee_read<uint32_t>(o + 0x78) + 0x10); hs != 0 && coin()) w16(hs + 4, R() & 0xffff);
                    finish();
                    return Args{{o}, {}};
                }, [](A a) { umb_30af40_l7(a.i[0]); return uint64_t{0}; });
            }
        }

        // classes 880 / 882 (the shots, spawned near the hero): flying, out of range (880: its sparks), hitting the ground or a
        // moby (882: another shell), out of the box
        {
            auto shot_prep = [&](uint32_t m, bool is880) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                Vec4 v{rf(-1, 1), rf(-1, 1), coin() ? rf(-3, -0.5f) : rf(-1, 1), 0.0f};
                const float s = rf(0.05f, 2.0f);
                v.x *= s; v.y *= s;
                if (is880) {
                    ee_ref<Vec4>(pv + 0x30) = v;
                    near(pv + 0x40, m + 0x10, coin() ? 20.0f : 40.0f, 2.0f);
                    w32(pv + 0x50, ri(0, 2) == 0 ? 0u : (coin() ? m : ee_read<uint32_t>(0x001413d0)));
                    w32(pv + 0x54, coin() ? 1u : 0u);
                } else {
                    ee_ref<Vec4>(pv) = v;
                    w32(pv + 0x10, ri(0, 2) == 0 ? 0u : (coin() ? m : ee_read<uint32_t>(0x001413d0)));
                    w32(pv + 0x14, u(ri(-3, 12)));
                    wf(pv + 0x18, rf(-0.3f, 0.3f));
                    wf(pv + 0x1c, rf(-0.3f, 0.3f));
                    if (ri(0, 3) == 0) {  // another shell in its way
                        const uint32_t o = spawn_at(0x372, m + 0x10, 0.0f);
                        if (o != 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += v.x * 0.5f; q.y += v.y * 0.5f; q.z += v.z * 0.5f; ee_ref<Vec4>(o + 0x10) = q; moby_update_transform(o); }
                    }
                }
                w8(m + 0x31, coin() ? 1u : 0u);
                if (coin()) near(P + 0x80, m + 0x10, coin() ? 4.0f : 40.0f, 1.0f);
                camera_near(m + 0x10);
                out_of_box(m);
                finish();
            };
            level();
            const uint32_t t880 = spawn_at(0x370, P + 0x80, 0.0f), t882 = spawn_at(0x372, P + 0x80, 0.0f);
            if (t880 == 0) std::printf("p71_class880               class 880 not on this level\n");
            else if ((own_ignored = effect_lanes, true))
                p71("p71_class880", 0x0030b6a8, 200, Ret::None, [&] { level(); const uint32_t m = spawn_at(0x370, P + 0x80, 8.0f); shot_prep(m, true); return Args{{m}, {}}; },
                    [](A a) { umb_class880_update_l7(a.i[0]); return uint64_t{0}; });
            if (t882 == 0) std::printf("p71_class882               class 882 not on this level\n");
            else if ((own_ignored = effect_lanes, true))
                p71("p71_class882", 0x0030bbc8, 200, Ret::None, [&] { level(); const uint32_t m = spawn_at(0x372, P + 0x80, 8.0f); shot_prep(m, false); return Args{{m}, {}}; },
                    [](A a) { umb_class882_update_l7(a.i[0]); return uint64_t{0}; });
            // 0x30ba90 (the turret's shell spawned): at a place, a velocity, an ignored moby, its kind (a halfword, sign-extended)
            p71("p71_f30ba90", 0x0030ba90, 60, Ret::Int32, [&] {
                level();
                near(X, P + 0x80, 20.0f, 3.0f);
                ee_ref<Vec4>(X + 0x10) = Vec4{rf(-1, 1), rf(-1, 1), rf(-1, 1), rf(-1, 1)};
                finish();
                return Args{{X, X + 0x10, coin() ? any_moby() : 0u, R()}, {}};
            }, [](A a) { return r32(umb_30ba90_l7(a.i[0], a.i[1], a.i[2], static_cast<int16_t>(a.i[3]))); });
        }

        // class 902 (the water tiles) and its helpers 0x281ef8 / 0x2cb1c0
        {
            const std::vector<uint32_t> tiles = mobys_with(0x0030c508);
            if (!tiles.empty()) {
                uint32_t tile0 = 0;
                std::vector<uint32_t> vol_tiles;  // (the tiles with a flood volume)
                for (uint32_t m : tiles) {
                    if (ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x14) == 0) tile0 = m;
                    if (ee_read<int32_t>(ee_read<uint32_t>(m + 0x78) + 0x10) >= 0) vol_tiles.push_back(m);
                }
                if (std::getenv("B119_INFO") != nullptr) std::printf("INFO p71 tiles %zu with a volume %zu\n", tiles.size(), vol_tiles.size());
                for (int part = 0; part < 3; ++part) {
                    // (part 0: the set-up and the flood's states; 1: high / low, following the group; 2: the draw's tail)
                    static const char* const nms[] = {"p71_class902_a", "p71_class902_b", "p71_class902_c"};
                    p71(nms[part], 0x0030c508, part == 1 ? 400 : 160, Ret::None, [&, part] {
                        level();
                        const uint32_t m = part == 0 && tile0 != 0 && ri(0, 3) == 0 ? tile0 : pick(tiles);
                        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                        int st = 0;
                        if (part == 0) st = ri(0, 4) == 0 ? 0 : ri(3, 8);
                        else if (part == 1) st = ri(1, 2);
                        else st = ri(0, 8);
                        w8(m + 0x20, u(st));
                        w32(L7_ADDR(0x00161b38), coin() ? 1u : 0u);
                        w32(pv + 0x8, coin() ? 1u : 0u);
                        w32(pv + 0xc, coin() ? 1u : 0u);
                        const float hi = ee_read<float>(pv + 0x0), lo = ee_read<float>(pv + 0x4);
                        wf(m + 0x18, coin() ? rf(lo - 2, hi + 2) : (coin() ? lo : hi) + rf(-0.05f, 0.05f));
                        if (st == 1 || st == 2) {
                            static const uint32_t bcs[] = {0, 1, 2, 4, 8, 5, 6, 9, 10, 12};
                            w8(m + 0xbc, bcs[ri(0, 9)]);
                        }
                        if (st == 3) {
                            if (ri(0, 4) == 0) w32(pv + 0x10, 0xffffffffu);
                            if (coin()) {  // (no tile of the level has a flood volume: one of the level's boxes given)
                                if (ee_read<int32_t>(pv + 0x10) < 0 && lvd::box_count(lvd::BOXES) > 0) w32(pv + 0x10, u(ri(0, lvd::box_count(lvd::BOXES) - 1)));
                                hero_in_box(ee_read<int32_t>(pv + 0x10));
                            }
                        }
                        if (st >= 4 && st <= 5) w32(pv + 0x18, u(ri(0, 2)));
                        if (st == 6) wf(m + 0x18, rf(52.0f, 55.0f));
                        if (st == 7) wf(m + 0x18, hi + rf(-1, 1));
                        if (part == 2 || ri(0, 3) == 0) {
                            if (ri(0, 5) == 0) w32(pv + 0x14, 0xffffffffu);
                            w32(m + 0x94, coin() ? 0u : (ee_read<uint32_t>(ee_read<uint32_t>(m + 0x24) + 0x10) != 0 ? ee_read<uint32_t>(ee_read<uint32_t>(m + 0x24) + 0x10) : 0x42c80000u));  // (the tiles' class has no draw distance: one given)
                            if (coin()) w32(L7_ADDR(0x00161b30), ee_read<uint32_t>(pv + 0x14));
                            w16(L7_ADDR(0x00161b34), R() & 0xffff);
                            if (coin()) {  // (on screen: moby_on_screen_l1 tests the moby's +0x0, scaled by 1/1024)
                                Vec4 q = ee_ref<Vec4>(m); q.x *= 1.0f / 1024; q.y *= 1.0f / 1024; q.z *= 1.0f / 1024; q.w *= 1.0f / 1024;
                                ee_ref<Vec4>(X + 0x200) = q;
                                view_on(X + 0x200);
                            } else camera_near(m + 0x10);
                        }
                        finish();
                        if ((st == 1 || st == 2) && ri(0, 2) == 0) ripple_due();
                        return Args{{m}, {}};
                    }, [](A a) { umb_class902_update_l7(a.i[0]); return uint64_t{0}; });
                }
                p71("p71_f281ef8", 0x00281ef8, 60, Ret::Int32, [&] {
                    level();
                    const uint32_t m = coin() ? pick(tiles) : any_moby();
                    const int32_t g = coin() ? static_cast<int32_t>(ee_read<uint8_t>(m + 0x21)) : ri(-1, ee_read<int32_t>(OV(0x0015fff4)) + 1);
                    finish();
                    return Args{{m, u(g), u(ri(0, 1)), u(ri(0, 1))}, {}};
                }, [](A a) { return r32(umb_281ef8_l7(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])) ? 1u : 0u); });
                p71("p71_f2cb1c0", 0x002cb1c0, 60, Ret::None, [&] {
                    level();
                    const uint32_t t = L7_ADDR(0x001dce40) + u(ri(0, 35)) * 0x1190;
                    finish();
                    return Args{{L7_ADDR(0x001dce40), u(ri(1, 36))}, {ee_read<float>(t) + rf(-8, 8), ee_read<float>(t + 4) + rf(-8, 8), rf(0.5f, 4.0f), rf(-1.0f, 1.0f)}};
                }, [](A a) { umb_2cb1c0_l7(a.i[0], static_cast<int32_t>(a.i[1]), a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
            }
        }

        // class 1013 (the sliding gates): set up, closed / opening / open / closing on their driver's state (none, gone,
        // dying, the states that open and close it, another), at home or away, turned over (classes 0x3f5 / 0x429)
        {
            const std::vector<uint32_t> gates = mobys_with(0x0030cf90);
            if (!gates.empty()) {
                p71("p71_class1013", 0x0030cf90, 300, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(gates);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const int st = ri(0, 6);
                    w8(m + 0x20, u(st));
                    if (ri(0, 4) == 0) w8(m + 0xb0, 0xff);
                    if (ee_read<uint8_t>(m + 0xb0) != 0xff)
                        w8(0x0014c050 + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4), ri(0, 4) == 0 ? 0xffu : 0u);
                    const int k = ri(0, 3);
                    if (k == 0) w32(pv + 0x10, 0xffffffffu);
                    else if (k == 1) w32(pv + 0x10, index_of(any_moby()));
                    w16(pv + 0x16, u(ri(0, 4)));
                    w32(pv + 0x18, u(ri(0, 4)));
                    if (ee_read<int32_t>(pv + 0x10) != -1) {
                        const uint32_t d = moby_at(ee_read<uint32_t>(pv + 0x10));
                        const int j = ri(0, 4);
                        w8(d + 0x20, j == 0 ? ee_read<uint16_t>(pv + 0x16) : (j == 1 ? ee_read<uint32_t>(pv + 0x18) & 0xff : (j == 2 ? (coin() ? 0xfeu : 0xfdu) : u(ri(0, 6)))));
                    }
                    w8(pv + 0xe, ri(0, 3) == 0 ? 0xffu : u(ri(0, 3)));
                    w8(pv + 0xf, ri(0, 3) == 0 ? 0xffu : u(ri(0, 3)));
                    w16(pv + 0x14, ri(0, 3) == 0 ? 0xffffu : u(ri(0, 15)));
                    wf(pv + 0x1c, rf(0.5f, 10.0f));
                    ee_ref<Vec4>(pv) = ee_ref<Vec4>(m + 0x10);
                    if (coin()) {  // away from home (a little or past its distance)
                        Vec4 q = ee_ref<Vec4>(m + 0x10);
                        q.x += rf(-12, 12); q.y += rf(-12, 12); q.z += rf(-3, 3);
                        if (ri(0, 3) == 0) { q = ee_ref<Vec4>(pv); q.x += rf(-0.002f, 0.002f); }
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                    { const int j = ri(0, 3); if (j == 0) w16(m + 0xa6, 0x3f5); else if (j == 1) w16(m + 0xa6, 0x429); }
                    sound_slot(m, pv + 0xc, true);
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class1013_update_l7(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 1018 (the sea): set up, the camera in its volume or out of it
        {
            const std::vector<uint32_t> seas = mobys_with(0x0030d440);
            if (!seas.empty()) {
                p71("p71_class1018", 0x0030d440, 40, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(seas);
                    w8(m + 0x20, u(ri(0, 2)));
                    const int32_t box = ee_read<int32_t>(ee_read<uint32_t>(m + 0x78));
                    if (coin() && box >= 0) { Vec4 c = ee_ref<Vec4>(lvd::box(lvd::BOXES, box) + 0x30); ee_ref<Vec4>(CAM) = c; }
                    else camera_near(m + 0x10);
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class1018_update_l7(a.i[0]); return uint64_t{0}; });
            }
        }

        // class 1041 (the turrets): woken, resting, firing (its timer, the volley's barrel pair, the camera within 40 / 90, the
        // hero within 20), hit (by the hero, by its own shell 0x372), shot down
        {
            const std::vector<uint32_t> turrets = mobys_with(0x0030d4d8);
            if (!turrets.empty()) {
                own_ignored = effect_lanes;
                p71("p71_class1041", 0x0030d4d8, 300, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(turrets);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    const int st = ri(0, 4) == 0 ? ri(0, 1) * 3 : ri(1, 2);
                    w8(m + 0x20, u(st));
                    w8(m + 0x31, coin() ? 1u : 0u);
                    w32(pv + 0xb0, u(ri(0, 120)));
                    const uint32_t f = R() & 0xffff;
                    w32(FRAME, f);
                    keep_frame = true;
                    w32(pv + 0xb4, f + u(ri(-2, 2)));
                    w16(pv + 0xb8, u(ri(0, 2)));
                    w16(pv + 0xba, u(ri(0, 40)));
                    w32(pv + 0xc4, u(ri(0, 3)));
                    wf(pv + 0xbc, rf(-3.1f, 3.1f));
                    wf(pv + 0xc0, rf(-3.1f, 3.1f));
                    camera_near(m + 0x10);
                    if (coin()) near(P + 0x80, m + 0x10, st == 2 && coin() ? 6.0f : 15.0f, 2.0f);
                    uint32_t shell = 0;
                    if (ri(0, 3) == 0) shell = spawn_at(0x372, m + 0x10, 3.0f);
                    hit_on(m, 0x00230000u, shell);
                    if (ri(0, 2) == 0) w8(pv + 0x28, 0);  // (its hit state's kind 0: the reactions 1 / 2)
                    wf(pv + 0x20, coin() ? rf(0.0f, 3.0f) : rf(3.0f, 20.0f));
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class1041_update_l7(a.i[0]); return uint64_t{0}; });
            }
        }

        // classes 1046 (the stomp's ring) / 1049 (the ball), spawned near the hero, and 0x30e0e0 (a moby pushed by the ring)
        {
            auto ring_prep = [&](uint32_t m) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const float r0 = rf(0.0f, 2.0f), r1 = r0 + rf(2.0f, 20.0f);
                wf(pv + 0x0, r0);
                wf(pv + 0x4, r1);
                const float t = coin() ? rf(-0.05f, 1.05f) : (coin() ? rf(0.15f, 0.25f) : rf(0.75f, 0.85f));
                wf(pv + 0x8, r0 + (r1 - r0) * t);
                wf(pv + 0xc, coin() ? rf(0.0f, 0.5f) : 0.0f);
                wf(pv + 0x10, rf(0.0f, 6.0f));
                wf(pv + 0x1c, rf(0.0f, 3.0f));
                return pv;
            };
            // a moby put on the ring's edge (within 0.3 of it, or off it), below its top or above
            auto on_edge = [](uint32_t t, uint32_t m, uint32_t pv) {
                const int band = ri(0, 2);  // (within 0.3 of the edge, off it, or at 0.3 itself: the threshold)
                const float r = ee_read<float>(pv + 0x8) + (band == 0 ? rf(-0.25f, 0.25f) : band == 1 ? rf(-2.0f, 2.0f) : (coin() ? 1.0f : -1.0f) * rf(0.295f, 0.31f)), a = rf(-3.1f, 3.1f);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                q.x += r * std::cos(a); q.y += r * std::sin(a);
                q.z += coin() ? rf(-1.0f, ee_read<float>(pv + 0x10) * 0.3f) : rf(0.0f, 4.0f);
                ee_ref<Vec4>(t + 0x10) = q;
            };
            level();
            std::vector<uint32_t> beast;  // (the level's mobys of the beast's classes 0x415 / 0x452)
            for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                if (ee_read<uint8_t>(o + 0x20) < 0xfe && (ee_read<int16_t>(o + 0xa6) == 0x415 || ee_read<int16_t>(o + 0xa6) == 0x452)) beast.push_back(o);
            if (std::getenv("B119_INFO") != nullptr) std::printf("INFO p71 beast parts %zu\n", beast.size());
            const uint32_t t1046 = spawn_at(0x416, P + 0x80, 0.0f), t1049 = spawn_at(0x419, P + 0x80, 0.0f);
            if (t1046 == 0) std::printf("p71_class1046              class 1046 not on this level\n");
            else {
                p71("p71_class1046", 0x0030e1f8, 160, Ret::None, [&] {
                    level();
                    const uint32_t m = spawn_at(0x416, P + 0x80, 4.0f);
                    const uint32_t pv = ring_prep(m);
                    if (coin()) on_edge(ee_read<uint32_t>(0x001413d0), m, pv);
                    for (uint32_t o = ee_read<uint32_t>(L7_ADDR(0x0015ffe4)); o != 0; o = ee_read<uint32_t>(o + 0x28))
                        if (ri(0, 2) == 0) on_edge(o, m, pv);
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class1046_update_l7(a.i[0]); return uint64_t{0}; });
                p71("p71_f30e0e0", 0x0030e0e0, 100, Ret::None, [&] {
                    level();
                    const uint32_t m = spawn_at(0x416, P + 0x80, 4.0f);
                    const uint32_t pv = ring_prep(m);
                    const uint32_t t = coin() ? ee_read<uint32_t>(0x001413d0) : any_moby();
                    if (ri(0, 4) != 0) on_edge(t, m, pv);
                    finish();
                    return Args{{t, m, pv}, {}};
                }, [](A a) { umb_30e0e0_l7(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
            }
            if (t1049 == 0) std::printf("p71_class1049              class 1049 not on this level\n");
            else if ((own_ignored = effect_lanes, true))
                p71("p71_class1049", 0x0030e458, 200, Ret::None, [&] {
                    level();
                    const uint32_t m = spawn_at(0x419, P + 0x80, 8.0f);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    Vec4 v{rf(-1, 1), rf(-1, 1), coin() ? rf(-3, -0.5f) : rf(-1, 1), 0.0f};
                    ee_ref<Vec4>(pv) = v;
                    w32(pv + 0x10, ri(0, 2) == 0 ? 0u : (coin() ? m : ee_read<uint32_t>(0x001413d0)));
                    wf(m + 0x2c, ee_read<float>(ee_read<uint32_t>(m + 0x24) + 0x24) * rf(0.5f, 3.2f));
                    w8(m + 0x31, coin() ? 1u : 0u);
                    w32(FRAME, R() & 0xffff);
                    keep_frame = true;
                    if (ri(0, 3) == 0) {  // one of the beast's own (the level's 0x415 / 0x452, or another ball) or a shell in its way
                        // (a 0x415 / 0x452 spawned has no model on its own: the level's are moved instead)
                        const int k = ri(0, 2);
                        const uint32_t o = k == 0 && !beast.empty() ? pick(beast) : spawn_at(k == 1 ? 0x419u : 0x372u, m + 0x10, 0.0f);
                        if (o != 0) { Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += v.x * 0.5f; q.y += v.y * 0.5f; q.z += v.z * 0.5f; ee_ref<Vec4>(o + 0x10) = q; moby_update_transform(o); }
                    }
                    camera_near(m + 0x10);
                    out_of_box(m);
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class1049_update_l7(a.i[0]); return uint64_t{0}; });
        }

        // ---- P7.1b: class 1059 (umbris_walker.cpp) and its thirteen helpers: the level's own walkers, their set-up done by the
        // port first (their paths, floor moby, place), then a state, the hero near / in its chase area / held, its group's
        // states, its timers, its footing changed, a hit, the frame counter on its splash / spray slots
        {
            const std::vector<uint32_t> walkers = mobys_with(0x0030f5f0);
            // (the hit record's triangle, 0x173e90 .. +0x2f: the shared collision ports (walker_move_fx's, sphere_collide's) fill
            // it otherwise than the original when the hero's moby, moved here next to the walker, is the one met: not these
            // ports' code, ignored for these checks only, reported)
            std::vector<uint32_t> tri;
            for (uint32_t k = 0; k < 0x30; k += 4) tri.push_back(OV(0x00173e90) + k);
            auto with_tri = [&](bool effects) { own_ignored = tri; if (effects) own_ignored.insert(own_ignored.end(), effect_lanes.begin(), effect_lanes.end()); };
            if (std::getenv("B119_INFO") != nullptr) {
                std::printf("INFO p71b walkers %zu hit record moby OV %08x\n", walkers.size(), OV(0x00173e58));
                for (uint32_t m : walkers) {
                    const uint32_t cls = ee_read<uint32_t>(m + 0x24), pv = ee_read<uint32_t>(m + 0x78);
                    std::printf("INFO p71b walker %08x group %u hooks %08x paths %d %d floor %d place %d b2 %d\n", m, ee_read<uint8_t>(m + 0x21),
                                cls != 0 ? ee_read<uint32_t>(cls + 0x2c) : 0u, ee_read<int32_t>(pv + 0x110), ee_read<int32_t>(pv + 0x144),
                                ee_read<int32_t>(pv + 0x118), ee_read<int32_t>(pv + 0x114), ee_read<int16_t>(m + 0xb2));
                }
            }
            // the walker set up by the port (state 0 -> its rest state), then the case's own state and surroundings
            auto walker = [&](bool setup_only) {
                level();
                const uint32_t m = pick(walkers);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, 0);
                umb_class1059_update_l7(m);
                if (setup_only) return m;
                // the hero: near it, in its chase area's middle (a point of its chase path), far; held (+0x14c)
                const int k = ri(0, 6);
                if (k == 0) near(P + 0x80, m + 0x10, 2.5f, 0.5f);
                else if (k == 1) near(P + 0x80, m + 0x10, 12.0f, 1.0f);
                else if (k == 2 && ee_read<uint32_t>(pv + 0x148) != 0) {  // the hero in its chase area, it near him
                    const uint32_t ch = ee_read<uint32_t>(pv + 0x148);
                    near(P + 0x80, ch + 0x10 + (u(ri(0, std::max(0, ee_read<int32_t>(ch) - 1))) << 4), 1.0f, 0.5f);
                    if (coin()) { near(m + 0x10, P + 0x80, coin() ? 1.5f : 4.0f, 0.3f); moby_update_transform(m); }
                } else if (k == 3) {  // the hero at its chain point 0 (its grab, its bite)
                    moby_chain_point_v(m, 0, ee_ref<Vec4>(X + 0x300));
                    near(P + 0x80, X + 0x300, 0.3f, 0.3f);
                } else if (k == 4) {  // in the water of a tile of the rooms (its splash and spray)
                    const uint32_t t = L7_ADDR(0x001dce40) + u(ri(0, 35)) * 0x1190;
                    Vec4 q{ee_read<float>(t), ee_read<float>(t + 4), ee_read<float>(t + 8) + rf(-1.0f, 0.5f), 1.0f};
                    ee_ref<Vec4>(m + 0x10) = q;
                    moby_update_transform(m);
                    if (ee_read<int16_t>(m + 0xb2) < 45 && coin()) { w32(FRAME, u(ee_read<int16_t>(m + 0xb2)) + 45 * u(ri(0, 100))); keep_frame = true; }
                } else if (k == 5) {  // the hero at its thresholds: 3 (close in the chase), 1.9 (the bite)
                    const float r = coin() ? rf(2.9f, 3.05f) : rf(1.85f, 2.0f), a = rf(-3.1f, 3.1f);
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    q.x += r * std::cos(a); q.y += r * std::sin(a);
                    ee_ref<Vec4>(P + 0x80) = q;
                } else wf(P + 0x80, ee_read<float>(P + 0x80) + 200.0f);
                if (k == 5 || coin()) {  // (the hero's moby where he is: the sphere tests and the targeting see the moby)
                    const uint32_t hm = ee_read<uint32_t>(0x001413d0);
                    if (hm != 0) { ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(P + 0x80); moby_update_transform(hm); }
                }
                if (coin()) wf(m + 0x48, ee_read<float>(pv + 0x13c) + rf(-0.1f, 0.1f));  // (facing its heading)
                else if (coin()) wf(m + 0x48, atan2_f(ee_read<float>(pv + 0xf0) - ee_read<float>(m + 0x10), ee_read<float>(pv + 0xf4) - ee_read<float>(m + 0x14)) + rf(-0.1f, 0.1f));
                w8(pv + 0x28, u(ri(0, 3)));  // (its hit state's kind: the reaction table's columns)
                w16(0x0013f65c, ri(0, 2) == 0 ? 0u : 1u);
                wf(pv + 0x14c, coin() ? 0.0f : (coin() ? rf(0.96f, 0.999f) : rf(0.0f, 1.2f)));
                w32(pv + 0x134, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 300)));
                wf(pv + 0x13c, rf(-3.1f, 3.1f));
                wf(m + 0x48, rf(-3.1f, 3.1f));
                wf(m + 0x44, rf(-0.5f, 0.5f));
                ee_ref<Vec4>(pv + 0xd0) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 0.0f};
                if (coin()) w8(m + 0xbc, u(ri(0, 2)));  // (its footing as last frame's: a change gives the tables' state)
                w8(m + 0x70, R() & 0xff);
                w8(m + 0x53, u(ri(0, 9)));
                if (coin()) {  // another of its group in a state 4 .. 6 (or not)
                    for (uint32_t o : walkers)
                        if (o != m && ee_read<uint8_t>(o + 0x21) == ee_read<uint8_t>(m + 0x21)) w8(o + 0x20, u(ri(1, 8)));
                }
                if (!keep_frame) {
                    const uint32_t f = coin() ? u(ee_read<int16_t>(m + 0xb2)) + 45 * u(ri(0, 100)) : 20 * 27 * u(ri(0, 4)) + (coin() ? 0u : R() & 0xff);
                    w32(FRAME, f);
                    keep_frame = true;
                }
                hit_on(m, 0x00330000u, 0);
                wf(pv + 0x20, coin() ? rf(0.0f, 3.0f) : rf(3.0f, 20.0f));
                return m;
            };
            if (!walkers.empty()) {
                own_ignored = effect_lanes;
                p71("p71b_class1059_setup", 0x0030f5f0, 60, Ret::None, [&] {
                    level();
                    const uint32_t m = pick(walkers);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, 0);
                    static const uint32_t offs[] = {0x110, 0x144, 0x118, 0x114};
                    if (ri(0, 2) == 0) w32(pv + offs[ri(0, 3)], 0xffffffffu);
                    finish();
                    return Args{{m}, {}};
                }, [](A a) { umb_class1059_update_l7(a.i[0]); return uint64_t{0}; });
                static const char* const nms[] = {"p71b_class1059_a", "p71b_class1059_b", "p71b_class1059_c"};
                for (int part = 0; part < 3; ++part) {
                    // (a: rest, roam, chase, grab, carry; b: the low footing's states and the attack; c: the hit states, past the table)
                    with_tri(true);
                    p71(nms[part], 0x0030f5f0, 300, Ret::None, [&, part] {
                        const uint32_t m = walker(false);
                        static const int sa[] = {1, 2, 4, 5, 6, 0, 3}, sb[] = {7, 8, 9, 10, 11}, sc[] = {12, 13, 14, 15, 16, 17, 18, 0xc, 0xe};
                        const int st = part == 0 ? sa[ri(0, 6)] : part == 1 ? sb[ri(0, 4)] : sc[ri(0, 8)];
                        w8(m + 0x20, u(st));
                        finish();
                        return Args{{m}, {}};
                    }, [](A a) { umb_class1059_update_l7(a.i[0]); return uint64_t{0}; });
                }
                // the helpers, each on a set-up walker
                with_tri(true);
                p71("p71b_f30e998", 0x0030e998, 200, Ret::None, [&] {
                    const uint32_t m = walker(false);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (ri(0, 5) == 0) w8(m + 0x20, 0xe);
                    if (ri(0, 4) == 0) w8(m + 0xbc, 0);
                    finish();
                    return Args{{m, pv, pv + 0x20}, {}};
                }, [](A a) { umb_30e998_l7(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
                for (int which = 0; which < 2; ++which)
                    p71(which == 0 ? "p71b_f30e720" : "p71b_f30e858", which == 0 ? 0x0030e720 : 0x0030e858, 40, Ret::None, [&] {
                        const uint32_t m = walker(true);
                        ee_ref<Vec4>(X) = Vec4{rf(-2, 2), rf(-2, 2), rf(-1, 2), rf(-1, 1)};
                        finish();
                        return Args{{m, ee_read<uint32_t>(m + 0x78) + 0x70, X}, {}};
                    }, [which](A a) { if (which == 0) umb_30e720_l7(a.i[0], a.i[1], a.i[2]); else umb_30e858_l7(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
                p71("p71b_f30ebc8", 0x0030ebc8, 60, Ret::Int32, [&] {
                    const uint32_t m = walker(true);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1, 1));
                    if (coin()) wf(ee_read<uint32_t>(pv + 0x12c), ee_read<float>(m + 0x18) + rf(-0.5f, 2.0f));
                    else if (coin()) wf(ee_read<uint32_t>(pv + 0x12c), floor_probe_20(m + 0x10) + rf(0.4f, 0.43f));  // (at its 0.41 threshold)
                    finish();
                    return Args{{m, pv}, {}};
                }, [](A a) { return r32(static_cast<uint32_t>(umb_30ebc8_l7(a.i[0], a.i[1]))); });
                p71("p71b_f30ec58", 0x0030ec58, 40, Ret::None, [&] {
                    const uint32_t m = walker(true);
                    w8(m + 0xbc, u(ri(0, 3)));
                    w8(m + 0x53, u(ri(0, 9)));
                    finish();
                    return Args{{m, ee_read<uint32_t>(m + 0x78)}, {}};
                }, [](A a) { umb_30ec58_l7(a.i[0]); return uint64_t{0}; });
                p71("p71b_f30eb38", 0x0030eb38, 30, Ret::None, [&] {
                    level();
                    const int32_t n = lvd::path_count();
                    const uint32_t path = ri(0, 4) == 0 || n <= 0 ? 0u : lvd::path(ri(0, n - 1));
                    finish();
                    return Args{{path}, {}};
                }, [](A a) { umb_30eb38_l7(a.i[0]); return uint64_t{0}; });
                p71("p71b_f30ed30", 0x0030ed30, 60, Ret::Int32, [&] {
                    level();
                    const uint32_t m = pick(walkers);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    static const uint32_t offs[] = {0x110, 0x144, 0x118, 0x114};
                    if (ri(0, 2) == 0) w32(pv + offs[ri(0, 3)], 0xffffffffu);
                    finish();
                    return Args{{m, pv}, {}};
                }, [](A a) { return r32(static_cast<uint32_t>(umb_30ed30_l7(a.i[0], a.i[1]))); });
                with_tri(false);
                p71("p71b_f30eee8", 0x0030eee8, 120, Ret::None, [&] {
                    const uint32_t m = walker(false);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                    if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
                    if (ri(0, 2) == 0) {  // pushed out to its area's edge
                        const uint32_t a = ee_read<uint32_t>(pv + 0x128);
                        if (a != 0 && ee_read<int32_t>(a) > 0) near(m + 0x10, a + 0x10 + (u(ri(0, ee_read<int32_t>(a) - 1)) << 4), 1.0f, 0.0f);
                    }
                    wf(pv + 0x140, coin() ? rf(0.1f, 0.6f) : -rf(0.1f, 0.6f));
                    w32(pv + 0x134, u(ri(0, 2) == 0 ? 0 : ri(1, 50)));
                    finish();
                    return Args{{m, pv}, {}};
                }, [](A a) { umb_30eee8_l7(a.i[0], a.i[1]); return uint64_t{0}; });
                with_tri(false);
                p71("p71b_f30f0f8", 0x0030f0f8, 80, Ret::None, [&] {
                    const uint32_t m = walker(false);
                    if (coin()) wf(m + 0x18, ee_read<float>(ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + 0x12c)) + rf(-1.0f, 2.0f));
                    finish();
                    return Args{{m, ee_read<uint32_t>(m + 0x78)}, {rf(0, 0.05f), rf(0, 0.2f), rf(0, 0.3f)}};
                }, [](A a) { umb_30f0f8_l7(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
                with_tri(false);
                p71("p71b_f30f1e0", 0x0030f1e0, 80, Ret::Int32, [&] {
                    const uint32_t m = walker(false);
                    const float lo = rf(0, 0.1f);
                    finish();
                    return Args{{m, ee_read<uint32_t>(m + 0x78)}, {rf(-3.1f, 3.1f), rf(0.1f, 0.6f), rf(0, 0.05f), rf(0.4f, 1.2f), rf(0, 0.05f), lo + rf(0, 0.3f), lo}};
                }, [](A a) { return r32(static_cast<uint32_t>(umb_30f1e0_l7(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4], a.f[5], a.f[6]))); });
                with_tri(false);
                p71("p71b_f30f308", 0x0030f308, 120, Ret::None, [&] {
                    const uint32_t m = walker(false);
                    w8(m + 0x20, coin() ? 0xau : u(ri(0, 12)));
                    w8(m + 0x52, coin() ? 5u : u(ri(0, 9)));
                    const float t = moby_anim_time(m);
                    const uint32_t target = ri(0, 3) != 0 ? ee_read<uint32_t>(0x001413d0) : any_moby();
                    if (ri(0, 3) != 0) { moby_chain_point_v(m, 0, ee_ref<Vec4>(X + 0x300)); near(P + 0x80, X + 0x300, 0.3f, 0.3f); const uint32_t hm = ee_read<uint32_t>(0x001413d0); if (hm != 0) { ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(P + 0x80); moby_update_transform(hm); } }
                    finish();
                    return Args{{m, ee_read<uint32_t>(m + 0x78), 0xa, 5, 4, 4, 0xb, target}, {coin() ? t - 0.5f : rf(0, 20), coin() ? t + 0.5f : rf(0, 30), 1.0f, 1.0f, rf(0.5f, 3.0f)}};
                }, [](A a) { umb_30f308_l7(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], static_cast<int32_t>(a.i[5]), a.i[6], a.i[7], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4]); return uint64_t{0}; });
                // (the hit record's triangle, 0x173e90 .. +0x2f: the shared sphere_collide port fills it for a moby it meets, the
                // original leaves it; not this port's code: ignored here only, reported)
                with_tri(false);
                p71("p71b_f30f4d0", 0x0030f4d0, 60, Ret::Int32, [&] {
                    const uint32_t m = walker(true);
                    if (ri(0, 3) != 0) { moby_chain_point_v(m, 0, ee_ref<Vec4>(X + 0x300)); near(P + 0x80, X + 0x300, 0.3f, 0.3f); const uint32_t hm = ee_read<uint32_t>(0x001413d0); if (hm != 0) { ee_ref<Vec4>(hm + 0x10) = ee_ref<Vec4>(P + 0x80); moby_update_transform(hm); } }
                    finish();
                    return Args{{m, coin() ? ee_read<uint32_t>(0x001413d0) : any_moby()}, {rf(0.5f, 3.0f)}};
                }, [](A a) { return r32(umb_30f4d0_l7(a.i[0], a.i[1], a.f[0]) ? 1u : 0u); });
                p71("p71b_f30f540", 0x0030f540, 60, Ret::Int32, [&] {
                    const uint32_t m = walker(true);
                    for (uint32_t o : walkers) if (o != m && coin()) w8(o + 0x20, u(ri(1, 9)));
                    finish();
                    return Args{{m, u(ri(0, 6)), u(ri(3, 8))}, {}};
                }, [](A a) { return r32(umb_30f540_l7(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2])) ? 1u : 0u); });
            }
        }
        g_extra_noop_hooks.clear();
    }

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
}

}  // namespace rac1::oracle
