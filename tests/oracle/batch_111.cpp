// Oracle batch 111 (task 0337): Eudora's own moby classes (level 4) on its real level data. Run with
// ORACLE_LEVEL=4 build/tests/ovl_oracle --batch 111 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Eudora's own function at its own address, the port the one registered for it (the
// transliterations eudora_classes_tr_N.cpp, the hand ports eudora_*.cpp, the shared ports of the classes and camera
// functions other levels carry too: shared_*.cpp, novalis_*.cpp, kerwan_camera.cpp; class 484's retail update is a bare
// tail call of moby_retire).

#include "ovl_oracle.h"

#include <game/ovl/helmet_tr.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class280.h>
#include <game/ovl/shared_class437.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_swingshot_target.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <runtime/kernel/disc.h>

#include <cmath>
#include <cstring>
#include <string>
#include <map>

namespace rac1::oracle {

namespace {
// A check whose original is Eudora's own function at `l4addr` (not a Veldin address to translate).
void l4check(const char* name, uint32_t l4addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port,
             uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (const char* skip = std::getenv("B111_SKIP"); skip != nullptr && (std::string(",") + skip + ",").find(std::string(",") + name + ",") != std::string::npos) return;  // (the mutation test: checks already seen to fail)
    if (g_oracle_level != 4) {
        std::printf("%-26s L4:0x%08x  skipped (Eudora only: run with ORACLE_LEVEL=4)\n", name, l4addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L4:0x%08x\n", name, l4addr);
    if (!check_fn(name, l4addr, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}
}  // namespace

void batch_111() {
    if (g_only_batch != 111) return;
    g_batch = 111;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
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
    // the level's mobys whose update function is `fn` (Eudora's address)
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    // an update function on the level's own mobys, perturbed by `vary`
    auto classcheck = [level, mobys_with](const char* name, uint32_t fn, void (*port)(uint32_t), std::function<void(uint32_t)> vary, int n = 60) {
        if (g_only_batch != 0 && g_only_batch != g_batch) return;
        if (g_oracle_level != 4) { l4check(name, fn, 0, Ret::None, nullptr, nullptr); return; }
        if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
        level();
        const std::vector<uint32_t> ms = mobys_with(fn);
        if (ms.empty()) { std::printf("%-26s L4:0x%08x  no moby on this level\n", name, fn); return; }
        l4check(name, fn, n, Ret::None, [level, ms, vary, port] {
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
    if (g_oracle_level == 4) {
        level();
        for (uint32_t a = ee_read<uint32_t>(OV(0x0015ffe0)); a < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x20000; a += 0x80) { g_ignored_words.push_back(a + 0xc); g_ignored_words.push_back(a + 0x1c); }
    }

    // ---- the level's own classes (transliterated by tools/codegen/generate_all.py): the level's own mobys of each class, the first
    // update (state 0) done by the port, then a state, a hit, the hero near, the pvars' floats / counters moved as typed
    g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fe20};  // (+ the level change request: the original would run the CD / movie path; batch 139 runs it. 0x120478: ported, task 0386)
    struct TrCls { const char* name; uint32_t addr; void (*fn)(uint32_t); uint32_t skip_states = 0; };  // (the states that start a scene / a level change: the original runs the kernel / movie path for real)
    static const TrCls trcls[] = {
        {"class86", 0x0029ecf8, eud_class86_update_l4},
        {"class161", 0x002a5a08, eud_class161_update_l4},
        {"class184", 0x002af058, shot_update},
        {"class217", 0x002ba520, eud_class217_update_l4},
        {"class340", 0x002c2270, eud_class340_update_l4},
        {"class427", 0x002c4850, eud_class427_update_l4},
        {"class432", 0x002c6858, eud_class432_update_l4},
        {"class434", 0x002c6bb8, eud_class434_update_l4},
        {"class466", 0x002ca420, eud_class466_update_l4},
        {"class481", 0x002cdda0, eud_class481_update_l4},
        {"class484", 0x002ce060, moby_retire},
        {"class563", 0x002d16b8, eud_class563_update_l4},
        {"class584", 0x002d3580, eud_class584_update_l4},
        {"class615", 0x002d4ae0, class615_update},
        {"class617", 0x002d6f68, eud_class617_update_l4},
        {"class642", 0x002d7e90, eud_class642_update_l4},
        {"class758", 0x002d9a58, swingshot_target_update},
        {"class1101", 0x002e17d8, eud_class1101_update_l4},
        {"class1120", 0x002e1a10, eud_class1120_update_l4, 0x4},
        {"class1190", 0x002e3078, eud_class1190_update_l4},
        {"class1343", 0x002e4418, eud_class1343_update_l4},
        {"class1516", 0x002e5178, eud_class1516_update_l4},
        {"class1549", 0x002e53a0, eud_class1549_update_l4},
        {"class1834", 0x002e79a8, class1844_update},
        {"class280", 0x002bfd30, class280_update},
        {"class437", 0x002c7e28, class437_update},
        {"class604", 0x002d4368, class604_update},
        {"class605", 0x002d46b8, zone_marker_update},
        {"class805", 0x002d9fc0, spawn_point_update_l1},
        {"class832", 0x002dc168, class832_update},
        {"class1134", 0x002e1f38, gold_bolt_update, 0x6},
        {"class1633", 0x002e5928, class1633_update},
        {"class1818", 0x002e69d0, sandmouse_update},
    };
    auto generic_prep = [give_hit, hero_near](uint32_t m, int st, int pass, void (*fn)(uint32_t)) {
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        const uint32_t P = 0x0013f350;
        if (st != 0) {
            w8(m + 0x20, 0);
            fn(m);
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
    if (g_oracle_level == 4 && std::getenv("B111_INFO") != nullptr) {  // the level's mobys of each class: how many, the nearest to the hero's start
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
        if (std::getenv("B111_CLS") != nullptr && std::strcmp(std::getenv("B111_CLS"), c.name) != 0) continue;
        for (int st = 0; st < 16; ++st)
        for (int pass = 0; pass < 3; ++pass) {
        if (g_oracle_level == 4 && std::getenv("B111_TAGS") != nullptr) std::printf("@@ %s st %d pass %d\n", c.name, st, pass);
        if ((std::getenv("B111_ST") == nullptr || std::atoi(std::getenv("B111_ST")) == st) && (std::getenv("B111_PASS") == nullptr || std::atoi(std::getenv("B111_PASS")) == pass) && (c.skip_states & (1u << st)) == 0) classcheck(c.name, c.addr, c.fn, [st, pass, c, generic_prep](uint32_t m) { generic_prep(m, st, pass, c.fn); }, 12);
        }
    }
    g_extra_noop_hooks.clear();

    // ---- Eudora's sound emitter type (the music box, 0x2f1988: section 5 of the overlay) on the level's own emitters
    if (g_oracle_level == 4) {
        level();
        std::vector<uint32_t> ems;
        for (int32_t i = 0; i < ee_read<int32_t>(0x0013f2e0); ++i) {
            const uint32_t e = ee_read<uint32_t>(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
            if (ee_read<uint32_t>(e + 4) == 0x002f1988u) ems.push_back(e);
        }
        if (ems.empty()) std::printf("music_box_emitter          L4:0x002f1988  no emitter of the type on this level\n");
        else
            l4check("music_box_emitter", 0x002f1988, 120, Ret::None, [level, ems] {
                level();
                const uint32_t em = ems[static_cast<size_t>(ri(0, static_cast<int>(ems.size()) - 1))];
                const uint32_t prm = ee_read<uint32_t>(em + 8);
                Vec4 q = ee_ref<Vec4>(em + 0x40);  // the hero (the camera's point) inside the box or around it
                const float r = ri(0, 2) == 0 ? rf(0, 3) : rf(3, 40);
                const float a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-3, 3);
                ee_ref<Vec4>(0x0013f3d0) = q;
                if (ri(0, 4) == 0) w32(prm + 0xc, 0xffffffffu);
                else if (ri(0, 2) == 0) w32(prm + 0xc, R());
                w32(prm + 8, u(ri(0, 1)));
                return Args{{em}, {}};
            }, [](A a) { emitter_music_box_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- Eudora's camera type 1 (an in-box camera modifier: accept / init / update / exit at 0x2ed850 / 0x2ed920 / 0x2ed940 / 0x2ed960)
    if (g_oracle_level == 4) {
        // a live camera of type 1 of the level, the hero put in or near its box, its data varied
        auto cam1_setup = [level](bool with_other, uint32_t* other) -> uint32_t {
            level();
            uint32_t cam = 0;
            for (uint32_t i = 0; i < 48 && cam == 0; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 1) cam = OV(0x00167150) + i * 0xa0;
            if (cam == 0) return 0;
            const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
            const uint32_t d = ee_read<uint32_t>(e + 0x1c);
            Vec4 at = ee_ref<Vec4>(0x0013f3d0);
            if (ri(0, 3) != 0) {  // inside the first box, or near it
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
        {
            level();
            bool any = false;
            for (uint32_t i = 0; i < 48; ++i)
                if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && ee_read<int16_t>(OV(0x00167150) + i * 0xa0 + 0x8c) == 1) any = true;
            if (!any) std::printf("cam_type1                  L4:0x002ed850  no live camera of type 1 on this level\n");
            else {
                l4check("cam_type1_accept", 0x002ed850, 300, Ret::Int32, [cam1_setup] { uint32_t other = 0; const uint32_t cam = cam1_setup(true, &other); return Args{{cam, other}, {}}; },
                        [](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(cam_type1_accept(a.i[0], a.i[1])) : 0u); });
                l4check("cam_type1_init", 0x002ed920, 100, Ret::None, [cam1_setup] { return Args{{cam1_setup(false, nullptr)}, {}}; },
                        [](A a) { if (a.i[0] != 0) cam_type1_init(a.i[0]); return uint64_t{0}; });
                l4check("cam_type1_update", 0x002ed940, 100, Ret::None, [cam1_setup] { return Args{{cam1_setup(false, nullptr)}, {}}; },
                        [](A a) { if (a.i[0] != 0) cam_type1_update(a.i[0]); return uint64_t{0}; });
                l4check("cam_type1_exit", 0x002ed960, 100, Ret::None, [cam1_setup] { return Args{{cam1_setup(false, nullptr)}, {}}; },
                        [](A a) { if (a.i[0] != 0) cam_type1_exit(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    // ---- Eudora's camera hook (0x2f0448, the pointer its setter 0x2f0410 puts at 0x167190) and the callback it queues (0x2f04a8)
    if (g_oracle_level == 4) {
        auto cam_prep = [level] {
            level();
            const uint32_t S = 0x00167180;
            w32(S, ri(0, 3) == 0 ? R() : 0u);
            w32(S + 4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            wf(S + 8, rf(-3, 3));
            wf(S + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-0.5f, 3));
            w32(S + 0x10, 0x002f0448u);
            if (ri(0, 3) == 0) w32(OV(0x0015ef4c), u(ri(0, 15)));  // the queue's count (it holds up to 16)
            wf(L4_ADDR(0x00161f4c), rf(0, 0.5f));  // $gp - 0x4cb4
            w32(OV(0x0015ef4c), u(ri(0, 14)));
        };
        l4check("cam0_hook", 0x002f0448, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_hook(0); return uint64_t{0}; });
        l4check("cam_callback", 0x002f04a8, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- Eudora's class hook +0x10 (0x2c41b0: pvars + 0x180; the matcher pairs it with class 749's 0x2d5800: pvars + 0xd0), and the dispatcher on it
    if (g_oracle_level == 4) {
        l4check("hook_10", 0x002c41b0, 60, Ret::Int32, [level] {
            level();
            const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
            const uint32_t m = ri(0, 4) == 0 ? 0u : first + 0x100 * u(ri(0, 100));
            if (m != 0 && ri(0, 3) == 0) w32(m + 0x78, 0);
            return Args{{m}, {}};
        }, [](A a) { return r32(eud_hook_10_u(a.i[0], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0); });
    }

    // the dispatcher on it (Veldin's class_hook_10, run on Eudora's own copy of it) on the level's mobys whose class has a +0x10 hook
    if (g_oracle_level == 4) {
        level();
        std::vector<uint32_t> hm;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && cls != 0 && ee_read<uint32_t>(cls + 0x2c) != 0 && ee_read<uint32_t>(ee_read<uint32_t>(cls + 0x2c) + 0x10) != 0) hm.push_back(m);
        }
        if (hm.empty()) std::printf("class_hook_10              no moby with a +0x10 hook on this level\n");
        else
            l4check("class_hook_10", ovl_code_lookup(0x002db890), 120, Ret::Int32, [level, hm] {
                level();
                const uint32_t m = hm[static_cast<size_t>(ri(0, static_cast<int>(hm.size()) - 1))];
                if (ri(0, 6) == 0) w8(m + 0x20, coin() ? 0xfe : 0xfd);
                if (ri(0, 6) == 0) w32(m + 0x78, 0);
                return Args{{m}, {}};
            }, [](A a) { return r32(class_hook_10(a.i[0])); });
    }

    // T00: the level's copy of Veldin's helmet_init_fx (0x2e06a8; the scan's body hash pairs them, the overlay map does not) at
    // L4:0x002e1c20: the hand port runs in its place; a live moby's pvars +0x10 .. +0x4c set up (the random bytes, the timers by
    // the frame scale)
    if (g_oracle_level == 4) {
        l4check("helmet_init_fx_2e1c20", 0x002e1c20, 120, Ret::None, [level] {
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

    // ---- P4.1: the hand ports of classes 86 (the flock), 161 (the fidgeting prop), 217 (the two-legged robot) and the
    // legged walker (eudora_flock.cpp, eudora_fidget.cpp, eudora_biped.cpp, eudora_walker.cpp), each against Eudora's own
    // function on states built from the level's own mobys (the flocks and their spawned members, the robots, the Woodcut
    // Bots' walkers), its paths and floors; the frame scales at their 50 Hz values (frame_rate_set(1)) in a quarter of the
    // cases. `B111_P41=1` prints the subjects found.
    if (g_oracle_level == 4) {
        g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fe20};
        const bool info = std::getenv("B111_P41") != nullptr;
        auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };
        auto rate50 = [] { if (ri(0, 3) == 0) frame_rate_set(1); };
        auto paths = [] { return ee_read<int32_t>(OV(0x00160104)); };
        auto path_at = [](int32_t i) { return ee_read<uint32_t>(OV(0x001b04b0) + static_cast<uint32_t>(i) * 4); };
        // a path of the level with at least `least` points (0: none)
        auto some_path = [paths, path_at](int32_t least) -> int32_t {
            for (int k = 0; k < 40; ++k) {
                const int32_t i = ri(0, std::max(0, paths() - 1));
                const uint32_t p = path_at(i);
                if (p != 0 && ee_read<int32_t>(p) >= least && ee_read<int32_t>(p) < 200) return i;
            }
            return -1;
        };
        // a point near the path's point `k` (in the plane within `r`, `dz` up or down)
        auto near_point = [path_at](int32_t path, int32_t k, float r, float dz) {
            Vec4 q = ee_ref<Vec4>(path_at(path) + 0x10 + static_cast<uint32_t>(k) * 16);
            const float a = rf(-3.1f, 3.1f), d = rf(0, r);
            q.x += d * std::cos(a); q.y += d * std::sin(a); q.z += rf(-dz, dz);
            return q;
        };
        // the hero near the moby (or far)
        auto hero_at = [](uint32_t m, float lo, float hi) {
            Vec4 q = ee_ref<Vec4>(m + 0x10);
            const float r = rf(lo, hi), a = rf(-3.1f, 3.1f);
            q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-1, 2);
            ee_ref<Vec4>(0x0013f3d0) = q; ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = q;
        };
        level();
        const std::vector<uint32_t> flocks = mobys_with(0x0029ecf8), members = mobys_with(0x0029eb18);
        const std::vector<uint32_t> bipeds = mobys_with(0x002ba520), bots = mobys_with(0x002d16b8);
        if (info) {
            std::printf("P41 flocks %zu members %zu bipeds %zu woodcut bots %zu paths %d\n", flocks.size(), members.size(), bipeds.size(), bots.size(), paths());
            for (uint32_t m : flocks) { const uint32_t pv = ee_read<uint32_t>(m + 0x78); std::printf("P41 flock %08x count %d path %d radius %.2f members %d\n", m, ee_read<int32_t>(pv + 0x90), ee_read<int32_t>(pv + 0xa0), ee_read<float>(pv + 0x9c), ee_read<int32_t>(pv + 0xa8)); }
            for (uint32_t m : bipeds) { const uint32_t pv = ee_read<uint32_t>(m + 0x78); std::printf("P41 biped %08x st %d gait0 %08x path %d area %d box %d sight %.1f rider %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint32_t>(pv + 0x1a0 + 0x70), ee_read<int32_t>(pv + 0x434), ee_read<int32_t>(pv + 0x41c), ee_read<int32_t>(pv + 0x430), ee_read<float>(pv + 0x43c), ee_read<uint32_t>(pv + 0x424)); }
            for (uint32_t m : bots) { const uint32_t pv = ee_read<uint32_t>(m + 0x78); std::printf("P41 bot %08x st %d gait0 %08x gait7 %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint32_t>(pv + 0x160 + 0x70), ee_read<uint32_t>(pv + 0x160 + 0x8c)); }
            for (uint32_t m : bots) { const uint32_t pv = ee_read<uint32_t>(m + 0x78); w8(m + 0x20, 0); eud_class563_update_l4(m); std::printf("P41 bot %08x after its state 0: st %d gait0 %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint32_t>(pv + 0x160 + 0x70)); }
            for (uint32_t m : bipeds) { const uint32_t pv = ee_read<uint32_t>(m + 0x78); w8(m + 0x20, 0); eud_class217_update_l4(m); std::printf("P41 biped %08x after its state 0: st %d gait0 %08x rider %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<uint32_t>(pv + 0x1a0 + 0x70), ee_read<uint32_t>(pv + 0x424)); }
            level();
        }

        // ---- the flock (class 86) and its helpers
        // its members' pvars varied: the area (a path's polygon or none), the leader, the home, on the ground or flying,
        // the wander's timer, the neighbours' range, the velocity, the place (near each other: the hit ending the search)
        auto vary_member = [some_path](uint32_t b, uint32_t flock) {
            const uint32_t bp = ee_read<uint32_t>(b + 0x78);
            if (ri(0, 2) == 0) { const int32_t p = some_path(3); if (p >= 0) w16(bp + 0x5c, u(p)); }
            switch (ri(0, 3)) {
            case 0: w32(bp + 0x50, 0); break;
            case 1: w32(bp + 0x50, b); break;
            case 2: w32(bp + 0x50, flock); break;
            default: break;
            }
            if (ri(0, 4) == 0) w32(bp + 0x54, 0);
            if (ri(0, 2) == 0) wf(bp + 0x48, 0.0f);
            if (ri(0, 2) == 0) w16(bp + 0x44, u(ri(0, 1)));
            if (coin()) wf(bp + 0x4c, rf(0.5f, 15));
            if (coin()) wfrand(bp + 0x10, 3, -0.3f, 0.3f);
            if (coin()) wfrand(bp + 0x20, 3, -1, 1);
            if (ri(0, 3) == 0) { Vec4 q = ee_ref<Vec4>(flock + 0x10); q.x += rf(-1.5f, 1.5f); q.y += rf(-1.5f, 1.5f); q.z += rf(-1, 1); ee_ref<Vec4>(b + 0x10) = q; }
            if (ri(0, 9) == 0) w8(b + 0x20, 0x80);
        };
        auto flock_prep = [vary_member, some_path, hero_at, rate50](uint32_t m) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 5) == 0) w32(pv + 0x94, 0);
            if (ri(0, 5) == 0) w32(pv + 0xa8, 0);
            if (ri(0, 7) == 0) { w32(L4_ADDR(0x001613c0), 0); w32(pv + 0xa8, u(ri(0, 12))); }
            switch (ri(0, 2)) {
            case 0: w32(pv + 0xa0, 0xffffffffu); break;
            case 1: { const int32_t p = some_path(3); if (p >= 0) { w32(pv + 0xa0, u(p)); w32(pv + 0xa4, u(ri(0, ee_read<int32_t>(ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4)) - 1))); } break; }
            default: break;
            }
            if (coin()) wf(pv + 0x9c, rf(0.5f, 8));
            if (coin()) wf(pv + 0x98, rf(-3.1f, 3.1f));
            if (ee_read<uint32_t>(pv + 0x94) != 0)
                for (int32_t i = 0; i < ee_read<int32_t>(pv + 0x90) && i < 32; ++i)
                    if (const uint32_t b = ee_read<uint32_t>(pv + 0x10 + u(i) * 4); b != 0 && coin()) vary_member(b, m);
            hero_at(m, 0, ri(0, 2) == 0 ? 2.0f : 30.0f);
            rate50();
            seed_rand();
        };
        if (flocks.empty()) std::printf("eud_flock                  L4:0x0029ecf8  no class 86 moby on this level\n");
        else {
            l4check("eud_flock", 0x0029ecf8, 240, Ret::None, [level, flocks, pick, flock_prep] {
                level();
                const uint32_t m = pick(flocks);
                flock_prep(m);
                return Args{{m}, {}};
            }, [](A a) { eud_class86_update_l4(a.i[0]); return uint64_t{0}; });
            // a member's steering and move, on a flock's own members, the parameters as the flock sets them
            auto member_args = [level, flocks, pick, vary_member, rate50, hero_at](bool steer) -> Args {
                level();
                for (int k = 0; k < 20; ++k) {
                    const uint32_t f = pick(flocks), pv = ee_read<uint32_t>(f + 0x78);
                    const int32_t n = ee_read<int32_t>(pv + 0x90);
                    if (n <= 0 || n > 32) continue;
                    const uint32_t b = ee_read<uint32_t>(pv + 0x10 + u(ri(0, n - 1)) * 4);
                    if (b == 0) continue;
                    for (int32_t i = 0; i < n; ++i) if (const uint32_t o = ee_read<uint32_t>(pv + 0x10 + u(i) * 4); o != 0 && ri(0, 2) == 0) vary_member(o, f);
                    vary_member(b, f);
                    const uint32_t prm = L4_ADDR(0x00166130);
                    wf(prm + 0x18, rf(0.001f, 0.05f)); wf(prm + 0x1c, rf(0.001f, 0.2f));
                    if (ri(0, 3) == 0) wf(prm, rf(0, 0.1f));
                    if (ri(0, 4) != 0) wf(prm + 4, rf(0, 25));  // (the pulls toward the leader and home: 0 while no member was counted)
                    if (ri(0, 4) != 0) wf(prm + 8, rf(0, 3));   // (into the area)
                    if (coin()) wf(prm + 0xc, rf(0, 0.05f));
                    if (ri(0, 3) == 0) wf(prm + 0x10, rf(0.2f, 3));
                    hero_at(b, 0, ri(0, 2) == 0 ? 1.5f : 20.0f);
                    rate50();
                    seed_rand();
                    if (steer) {
                        // (the stack below the caller as a previous call left it: the original reads a quadword of its
                        // frame it never writes, the member's distance to it gating the pulls toward the leader and home)
                        Vec4 stale{};
                        if (coin()) { stale = ee_ref<Vec4>(b + 0x10); stale.x += rf(-20, 20); stale.y += rf(-20, 20); }
                        // (below both stacks' tops: the port's frames' and the interpreter's, 32 MB less 64)
                        for (uint32_t a = ee_stack_ptr() - 0x400; a < ee_stack_ptr(); a += 16) ee_ref<Vec4>(a) = stale;
                        for (uint32_t a = 0x02000000u - 64 - 0x400; a < 0x02000000u - 64; a += 16) ee_ref<Vec4>(a) = stale;
                        return Args{{pv + 0x10, u(n), b, ee_read<uint32_t>(b + 0x78), prm}, {}};
                    }
                    return Args{{b, ee_read<uint32_t>(b + 0x78), prm}, {coin() ? 0.0f : rf(-0.5f, 0.5f)}};
                }
                return Args{{0}, {}};
            };
            l4check("eud_flock_steer", 0x001f30e0, 300, Ret::None, [member_args] { return member_args(true); },
                    [](A a) { if (a.i[0] != 0) flock_steer_l4(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3], a.i[4]); return uint64_t{0}; });
            l4check("eud_flock_move", 0x001f3a80, 200, Ret::None, [member_args] { return member_args(false); },
                    [](A a) { if (a.i[0] != 0) flock_member_move_l4(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
        }
        // the polygon helpers on the level's paths (a point near one of their points, sometimes far)
        auto poly_args = [level, some_path, near_point, path_at](int kind) -> Args {
            level();
            const int32_t p = some_path(kind == 0 ? 1 : 2);
            if (p < 0) return Args{{0}, {}};
            const uint32_t path = path_at(p);
            const int32_t n = ee_read<int32_t>(path);
            ee_ref<Vec4>(T) = near_point(p, ri(0, n - 1), ri(0, 3) == 0 ? 60.0f : 6.0f, 3.0f);
            wfrand(OUT, 8, -5, 5);
            switch (kind) {
            case 0: return Args{{M, path + 0x10, u(n)}, {}};  // (the moby-like struct's place, M + 0x10)
            case 1: return Args{{T, path + 0x10, u(ri(0, 3) == 0 ? ri(0, n) : n)}, {}};
            case 2: { const int32_t k = ri(0, n - 1); return Args{{OUT, path + 0x10 + u(k) * 16, path + 0x10 + u(coin() ? (k + 1) % n : ri(0, n - 1)) * 16, T}, {}}; }
            default: return Args{{OUT, T, path + 0x10, u(n)}, {}};
            }
        };
        l4check("eud_flock_nearest", 0x001f3038, 200, Ret::Int32, [poly_args] {
            Args a = poly_args(0);
            if (a.i[0] != 0) ee_ref<Vec4>(M + 0x10) = ee_ref<Vec4>(T);
            return a;
        }, [](A a) { return a.i[0] != 0 ? r32(static_cast<uint32_t>(flock_nearest_point_l4(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])))) : 0u; });
        l4check("eud_poly_outside", 0x0024c4f0, 300, Ret::Int32, [poly_args] { return poly_args(1); },
                [](A a) { return a.i[0] != 0 ? r32(static_cast<uint32_t>(polygon_outside_edge_l4(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])))) : 0u; });
        l4check("eud_seg_closest", 0x0024c660, 300, Ret::Float, [poly_args] { return poly_args(2); },
                [](A a) { return a.i[0] != 0 ? rf32(segment_closest_l4(a.i[0], a.i[1], a.i[2], a.i[3])) : 0u; });
        l4check("eud_poly_closest", 0x0024c7d8, 300, Ret::Int32, [poly_args] { return poly_args(3); },
                [](A a) { return a.i[0] != 0 ? r32(static_cast<uint32_t>(polygon_closest_l4(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3])))) : 0u; });
        // a moby along a path: the leg it is on, the direction, ahead or not, near the ends
        if (!flocks.empty())
            l4check("eud_path_advance", 0x0029eb20, 300, Ret::Int32, [level, flocks, pick, some_path, near_point, path_at] {
                level();
                const int32_t p = some_path(3);
                if (p < 0) return Args{{0}, {}};
                const int32_t n = ee_read<int32_t>(path_at(p));
                const int32_t k = ri(0, 3) == 0 ? (coin() ? 0 : n - 1) : ri(0, n - 1);
                const uint32_t m = pick(flocks);
                ee_ref<Vec4>(m + 0x10) = near_point(p, coin() ? k : (k + 1) % n, 4.0f, 2.0f);
                return Args{{m, path_at(p), u(k), u(coin() ? 1 : (coin() ? -1 : ri(-2, 2)))}, {coin() ? 0.0f : rf(0, 6)}};
            }, [](A a) { return a.i[0] != 0 ? r32(static_cast<uint32_t>(path_advance_l4(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]), a.f[0]))) : 0u; });

        // ---- class 161 (none on the level): its update on the robots and the Woodcut Bots (moby with animation sets)
        {
            std::vector<uint32_t> subj = bipeds;
            subj.insert(subj.end(), bots.begin(), bots.end());
            if (subj.empty()) std::printf("eud_fidget                 L4:0x002a5a08  no subject on this level\n");
            else
                l4check("eud_fidget", 0x002a5a08, 200, Ret::None, [level, subj, pick, rate50] {
                    level();
                    const uint32_t m = pick(subj), pv = ee_read<uint32_t>(m + 0x78);
                    w32(pv, ri(0, 2) == 0 ? 0u : u(ri(1, 4)));
                    w8(m + 0x53, u(ri(0, 2)));
                    w8(m + 0x52, ee_read<uint8_t>(m + 0x53));
                    w8(m + 0x70, coin() ? 2u : (R() & 0xfdu));
                    rate50();
                    seed_rand();
                    return Args{{ri(0, 20) == 0 ? 0u : m}, {}};
                }, [](A a) { eud_class161_update_l4(a.i[0]); return uint64_t{0}; });
        }

        // ---- the legged walker, on the robots (record pvars + 0x1a0) and the Woodcut Bots (+0x160)
        struct Walker { uint32_t m, rec; };
        std::vector<Walker> walkers;
        for (uint32_t m : bipeds) walkers.push_back({m, ee_read<uint32_t>(m + 0x78) + 0x1a0});
        for (uint32_t m : bots) walkers.push_back({m, ee_read<uint32_t>(m + 0x78) + 0x160});
        // a walker set up on its animation (the port's init) in most cases, then its state, flags, feet and speeds varied;
        // the animation set one of its gaits' (or the idle / landing ones)
        auto walker_prep = [hero_at, rate50](const Walker& w) {
            const uint32_t m = w.m, rec = w.rec;
            // (a Woodcut Bot's gaits are set by its own first update; the robots' stay null: retail's class 217 never sets them)
            if (ee_read<uint32_t>(rec + 0x70) == 0 && rec == ee_read<uint32_t>(m + 0x78) + 0x160) { w8(m + 0x20, 0); eud_class563_update_l4(m); }
            if (ri(0, 4) != 0) {
                uint32_t set = ee_read<uint32_t>(ee_read<uint32_t>(rec + 0x70 + u(ri(0, 12)) * 4));
                if (ri(0, 5) == 0) set = ee_read<uint8_t>(rec + (coin() ? 0xb0 : 0xb3));
                if (set < 0x20) { w8(m + 0x53, set); w8(m + 0x52, coin() ? set : 0xffu); }
                if (coin()) wf(m + 0x58, rf(0.2f, 2));
                walker_init_l4(m, rec);
            }
            w8(rec + 0xb7, u(ri(0, 22)));
            if (coin()) w8(rec + 0xb6, R() & 0x3f);
            if (coin()) w16(rec + 0xee, R() & 0xf);
            if (coin()) wf(rec + 0xc0, ri(0, 3) == 0 ? 0.0f : rf(0, 0.4f));
            if (coin()) wf(rec + 0xd8, rf(-0.5f, 0.5f));
            if (coin()) wf(rec + 0xe4, coin() ? 0.0f : ee_read<float>(m + 0x18) + rf(-1, 1));
            if (ri(0, 3) == 0) wf(rec + 0x68, ee_read<float>(m + 0x18) + rf(-3, 1));
            if (ri(0, 3) == 0) { Vec4 g = ee_ref<Vec4>(m + 0x10); g.x += rf(-8, 8); g.y += rf(-8, 8); ee_ref<Vec4>(rec + 0x60) = g; }
            if (ri(0, 3) == 0) w16(rec + 0xec, u(ri(0, 4)));
            if (coin()) wf(rec + 0xe8, rf(0, 1.5f));
            if (ri(0, 3) == 0 && ee_read<uint8_t>(m + 0x52) < 0x20) {  // a second gait of the set playing (a copy, its ranges varied): the last one counts
                const uint32_t g = ee_read<uint32_t>(rec + 0x70 + u(ri(0, 12)) * 4), cp = T3 + 0x100;
                for (uint32_t o = 0; o < 0x50; o += 4) w32(cp + o, ee_read<uint32_t>(g + o));
                w32(cp, ee_read<uint8_t>(m + 0x52));
                for (uint32_t o = 0x18; o < 0x50; o += 4) if (coin()) wf(cp + o, rf(0, 40));
                w32(rec + 0x70 + u(ri(0, 12)) * 4, cp);
            }
            if (ri(0, 3) == 0) w8(m + 0x51, u(ri(0, 40)));
            if (ri(0, 3) == 0) w8(m + 0x50, u(ri(0, 40)));
            if (ri(0, 3) == 0) wf(m + 0x54, rf(0, 1));
            if (ri(0, 4) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-1, 2));
            hero_at(m, 0, 20);
            rate50();
            seed_rand();
        };
        auto walker_args = [level, walkers](const std::function<void(const Walker&)>& vary) -> Walker {
            level();
            const Walker& w = walkers[static_cast<size_t>(ri(0, static_cast<int>(walkers.size()) - 1))];
            vary(w);
            return w;
        };
        if (walkers.empty()) std::printf("eud_walker                 no walker on this level\n");
        else {
            l4check("eud_walker_update", 0x00295778, 400, Ret::None, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                const float speed = ri(0, 3) == 0 ? 0.0f : rf(0, 0.5f);
                return Args{{w.m, w.rec}, {speed, rf(-3.1f, 3.1f), coin() ? rf(0, 0.0005f) : rf(0, 0.05f)}};
            }, [](A a) { walker_update_l4(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
            l4check("eud_walker_gait", 0x00293128, 400, Ret::None, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                const uint32_t landed = ri(0, 3) == 0 ? (R() & 0x3f) : ((R() & 0xf) | (coin() ? 0x10u << ri(0, 1) : 0u));
                return Args{{w.m, w.rec, landed}, {coin() ? rf(0, 0.05f) : rf(0, 0.6f), rf(-3.1f, 3.1f), coin() ? rf(0, 0.0015f) : rf(0, 0.05f)}};
            }, [](A a) { walker_gait_update_l4(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
            // the gaits' states with the animation of the state's own gait playing (the table L4:0x1ca8e0), a foot just
            // down, the step's height, a jump asked, in the air falling toward the goal: the walking, stepping, running,
            // jumping and landing transitions
            l4check("eud_walker_steps", 0x00293128, 600, Ret::None, [walker_args, walker_prep] {
                // (a state that sends the port through a pointer outside RAM, an animation set past the class's table, is not
                // a game state: dealt again, the RAM put back after the trial)
                for (int attempt = 0;; ++attempt) {
                Walker w = walker_args(walker_prep);
                for (int k = 0; k < 40 && w.rec != ee_read<uint32_t>(w.m + 0x78) + 0x160; ++k) w = walker_args(walker_prep);  // (the Woodcut Bots: the robots' gaits are null)
                const uint32_t m = w.m, rec = w.rec;
                const int st = ri(0, 3) == 0 ? ri(13, 20) : ri(1, 11);
                w8(rec + 0xb7, u(st));
                const int32_t slot = ee_read<int32_t>(L4_ADDR(0x001ca8e0) + u(st) * 4);
                const uint32_t g = ee_read<uint32_t>(rec + 0x70 + u(slot >= 0 && slot < 13 ? slot : (st >= 17 ? (st == 17 || st == 19 ? 10 : 12) : 7)) * 4);
                if (ri(0, 4) != 0) w8(m + 0x52, ee_read<uint32_t>(g) & 0xff);
                if (st == 16 && coin()) w8(m + 0x52, ee_read<uint8_t>(rec + 0xb3));
                wf(m + 0x58, coin() ? rf(0.3f, 1.5f) : rf(0, 0.3f));
                wf(rec + 0xe0, rf(0, 1.5f));
                wf(rec + 0xb8, coin() ? 0.0f : rf(-0.6f, 0.6f));
                w16(rec + 0xee, R() & (st >= 14 ? 0xfu : 0xbu));
                if (st >= 14 && coin()) { w16(rec + 0xee, ee_read<uint16_t>(rec + 0xee) | 4u); wf(rec + 0xd8, -rf(0, 0.5f)); }
                if (coin()) w8(rec + 0xb6, R() & (coin() ? 0x3au : 0x3fu));
                if (coin()) wf(rec + 0xc0, rf(0, 0.6f));
                const uint32_t landed = ri(0, 4) == 0 ? (R() & 0x3f) : (coin() ? 1u : 4u) | (coin() ? 0x30u : 0u);
                const Args args{{m, rec, landed}, {coin() ? rf(0, 0.05f) : rf(0, 0.6f), coin() ? ee_read<float>(m + 0x48) + rf(-0.05f, 0.05f) : rf(-3.1f, 3.1f), rf(0, 0.05f)}};
                static std::vector<uint8_t> saved;
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                try { walker_gait_update_l4(m, rec, landed, args.f[0], args.f[1], args.f[2]); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;
                std::memcpy(ram, saved.data(), saved.size());
                if (ok || attempt >= 20) return args;
                }
            }, [](A a) { walker_gait_update_l4(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
            l4check("eud_walker_init", 0x00295de8, 200, Ret::Int32, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                return Args{{w.m, w.rec}, {}};
            }, [](A a) { return r32(walker_init_l4(a.i[0], a.i[1])); });
            l4check("eud_walker_ground", 0x00292828, 300, Ret::Int32, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                if (ri(0, 3) == 0) w16(w.rec + 0xee, ee_read<uint16_t>(w.rec + 0xee) | 4u);
                if (ri(0, 3) == 0) { w8(w.rec + 0xb7, 0xd); wf(w.rec + 0xe4, ee_read<float>(w.m + 0x18) + rf(-1.5f, 1.5f)); w8(w.rec + 0xb6, R() & 0x30); }
                return Args{{w.m, w.rec}, {}};
            }, [](A a) { return r32(walker_ground_follow_l4(a.i[0], a.i[1])); });
            l4check("eud_walker_legs", 0x00292d48, 300, Ret::None, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                for (uint32_t leg = w.rec; leg < w.rec + 0x160; leg += 0xb0)
                    if (coin()) { wf(leg + 0xfc, rf(-1, 1)); wf(leg + 0x104, rf(-1, 1)); wf(leg + 0xf8, rf(-3.1f, 3.1f)); }
                return Args{{w.m, w.rec, R() & 0xf}, {}};
            }, [](A a) { walker_legs_aim_l4(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
            l4check("eud_walker_floor", 0x002922d0, 300, Ret::Float, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                const uint32_t p = w.rec + u(ri(0, 5)) * 0x10;
                if (coin()) wf(p + 8, ee_read<float>(p + 8) + rf(-3, 3));
                return Args{{p}, {}};
            }, [](A a) { return rf32(walker_floor_below_l4(a.i[0])); });
            l4check("eud_walker_phase", 0x002927d0, 300, Ret::Int32, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                if (ri(0, 4) == 0) w8(w.m + 0x52, 0x1f);
                return Args{{w.m, w.rec}, {rf(-2, 60)}};
            }, [](A a) { return r32(walker_phase_flags_l4(a.i[0], a.i[1], a.f[0])); });
            l4check("eud_walker_fall", 0x00293080, 300, Ret::Float, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                wf(w.rec + 0xd8, rf(-1, 1));
                if (coin()) wf(w.rec + 0xdc, -rf(0.001f, 0.05f));
                return Args{{w.m, w.rec}, {}};
            }, [](A a) { return rf32(walker_fall_time(a.i[0], a.i[1])); });
            l4check("eud_legs_reset", 0x00292cb8, 100, Ret::None, [walker_args, walker_prep] {
                const Walker w = walker_args(walker_prep);
                for (uint32_t leg = w.rec; leg < w.rec + 0x160; leg += 0xb0) { w8(leg + 0x121, u(ri(0, 1))); w8(leg + 0x161, u(ri(0, 1))); wfrand(leg + 0x130, 4, -1, 1); wfrand(leg + 0x170, 4, -1, 1); wf(leg + 0xfc, rf(-1, 1)); wf(leg + 0x104, rf(-1, 1)); }
                return Args{{w.rec}, {}};
            }, [](A a) { walker_legs_reset(a.i[0]); return uint64_t{0}; });
            // a gait's foot ranges and the animation keys, on the walkers' own gaits and sequences
            l4check("eud_gait_flags", 0x00292688, 400, Ret::Int32, [walker_args] {
                const Walker w = walker_args([](const Walker&) {});
                const uint32_t g = ee_read<uint32_t>(w.rec + 0x70 + u(ri(0, 12)) * 4);
                const float t = coin() ? rf(-2, 60) : ee_read<float>(g + 0x18 + u(ri(0, 13)) * 4) + rf(-1.2f, 1.2f);
                return Args{{g}, {t}};
            }, [](A a) { return r32(gait_phase_flags(a.i[0], a.f[0])); });
            l4check("eud_anim_key", 0x00241708, 400, Ret::Int32, [walker_args] {
                const Walker w = walker_args([](const Walker&) {});
                const uint32_t g = ee_read<uint32_t>(w.rec + 0x70 + u(ri(0, 12)) * 4);
                const uint32_t seq = ee_read<uint32_t>(ee_read<uint32_t>(w.m + 0x24) + 0x48 + ee_read<uint32_t>(g) * 4);
                return Args{{seq}, {coin() ? rf(-2, 80) : static_cast<float>(ri(-1, 80))}};
            }, [](A a) { return r32(static_cast<uint32_t>(anim_seq_key_at(a.i[0], a.f[0]))); });
        }

        // ---- class 217, the two-legged robot: every state with its timers, sight, alert, animations, paths, hits, the
        // walker set up (and its own states), the target near or far, the rider or none; and its three helpers
        if (bipeds.empty()) std::printf("eud_biped                  L4:0x002ba520  no class 217 moby on this level\n");
        else {
            auto biped_prep = [walker_prep, give_hit, some_path, rate50](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                rate50();
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class217_update_l4(m); }
                if (ri(0, 2) != 0) walker_prep(Walker{m, pv + 0x1a0});
                w8(m + 0x20, u(st));
                if (ri(0, 3) == 0) w32(pv + 0x38, 1);
                if (ri(0, 3) == 0) w8(pv + 0x406, u(ri(0, 3)));
                w16(pv + 0x402, ri(0, 2) == 0 ? 0u : u(ri(0, 3) == 0 ? frame_scale_i(0x25) : ri(1, 3)));
                w16(pv + 0x404, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                if (coin()) w8(pv + 0x407, u(ri(0, 2)));
                static const uint8_t sets[] = {0, 2, 3, 4, 5, 6, 8, 9, 0xa, 0xb, 0xd};
                const uint8_t set = sets[ri(0, 10)];
                w8(m + 0x53, set);
                w8(m + 0x52, ri(0, 3) == 0 ? sets[ri(0, 10)] : set);
                if (coin()) w8(m + 0x70, coin() ? 2u : 0u);
                if (coin()) { w8(m + 0x51, u(ri(0, 32))); w8(m + 0x50, u(ri(0, 32))); wf(m + 0x54, rf(0, 1)); }
                if (coin()) w8(m + 0xbc, R() & 0x1f);
                if (ri(0, 3) == 0) w32(pv + 0x41c, 0xffffffffu);
                else if (ri(0, 3) == 0) { const int32_t p = some_path(3); if (p >= 0) w32(pv + 0x41c, u(p)); }
                if (ri(0, 3) == 0) w32(pv + 0x434, 0xffffffffu);
                else if (ri(0, 3) == 0) { const int32_t p = some_path(2); if (p >= 0) { w32(pv + 0x434, u(p)); } }
                if (const int32_t p = ee_read<int32_t>(pv + 0x434); p >= 0 && p < 1000) {
                    const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4);
                    const int32_t n = path != 0 ? ee_read<int32_t>(path) : 1;
                    w16(pv + 0x438, u(ri(0, std::max(0, n - 1))));
                    if (path != 0 && ri(0, 2) == 0) {  // (near its point, or at the path's last)
                        if (coin()) w16(pv + 0x438, u(n - 1));
                        Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ee_read<int16_t>(pv + 0x438)) * 16);
                        q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f);
                        ee_ref<Vec4>(m + 0x10) = q;
                    }
                }
                if (ri(0, 3) == 0) w32(pv + 0x430, 0xffffffffu);
                if (coin()) w16(pv + 0x43a, u(ri(0, 1)));
                if (coin()) wf(pv + 0x43c, rf(2, 30));
                if (coin()) wf(pv + 0x418, rf(-0.1f, 0.5f));
                if (coin()) wf(pv + 0x42c, coin() ? 0.0f : rf(0, 0.3f));
                if (coin()) wf(pv + 0x420, coin() ? 0.0f : rf(0, 1));
                if (coin()) w32(pv + 0x448, u(ri(-1, 3)));
                if (coin()) wf(pv + 0x408, rf(-0.6f, 0.6f));
                if (ri(0, 9) == 0) w8(pv + 0x2e, 0);
                if (ri(0, 5) == 0) w32(pv + 0x424, 0);
                if (ri(0, 7) == 0) wf(m + 0x18, ee_read<float>(pv + 0x3f8) - rf(7, 10));
                if (coin()) wf(pv + 0x20, coin() ? rf(0.1f, 1) : 3.0f);
                if (ri(0, 7) == 0) w32(L4_ADDR(0x0015f5c4), coin() ? 2u : 0u);
                if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 1u);
                if (coin()) {  // its floor band near the camera point (L4:0x166f40)
                    w8(m + 0x31, 1);
                    Vec4 c = ee_ref<Vec4>(m + 0x10);
                    const float r = coin() ? rf(0, 27) : rf(25, 40), a = rf(-3.1f, 3.1f);
                    c.x += r * std::cos(a); c.y += r * std::sin(a);
                    ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c;
                }
                // the target: the hero within reach, in sight or far, at its height or not
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                const float r = ri(0, 3) == 0 ? rf(3.8f, 4.7f) : (coin() ? rf(0, 4.5f) : (coin() ? rf(4.5f, 20) : rf(20, 60))), a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += coin() ? rf(-0.5f, 0.5f) : rf(-4, 4);
                ee_ref<Vec4>(P + 0x80) = q; ee_ref<Vec4>(ee_read<uint32_t>(P + 0x2080) + 0x10) = q;  // (the hero's moby: what target_select picks)
                // a hit of the weapons (the mask 0x210000) in half the cases, its damage and its source varied
                if (coin()) {
                    give_hit(m, 0x210000u);
                    if (ee_read<uint8_t>(m + 0xa4) != 0xff) {
                        const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
                        w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                        wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
                        w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(P + 0x2080) : m));
                        wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
                    }
                } else {
                    w8(m + 0xa4, 0xff);
                }
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
            };
            // (a state that sends the port through a pointer outside RAM is not a game state: dealt again, as batch 111's
            // classcheck does; the RAM put back after the trial)
            auto dealt = [](const std::function<uint32_t()>& deal) {
                static std::vector<uint8_t> saved;
                uint32_t m = 0;
                for (int attempt = 0; attempt < 20; ++attempt) {
                    m = deal();
                    uint8_t* ram = ee_get_ram_base();
                    saved.assign(ram, ram + 0x02000000);
                    bool ok = true;
                    const uint32_t stack_top = ee_stack_ptr();
                    try { eud_class217_update_l4(m); } catch (const std::exception&) { ok = false; }
                    ee_stack_ptr() = stack_top;
                    std::memcpy(ram, saved.data(), saved.size());
                    if (ok) break;
                }
                return m;
            };
            for (int st = 0; st <= 11; ++st) {
                char name[32];
                std::snprintf(name, sizeof name, "eud_biped_st%d", st);
                const std::string nm = name;
                l4check(nm.c_str(), 0x002ba520, 60, Ret::None, [level, bipeds, pick, biped_prep, st] {
                    // (a state that sends the port through a pointer outside RAM is dealt again, as batch 111's classcheck)
                    uint32_t m = 0;
                    for (int attempt = 0; attempt < 20; ++attempt) {
                        level();
                        m = pick(bipeds);
                        biped_prep(m, st);
                        static std::vector<uint8_t> saved;
                        uint8_t* ram = ee_get_ram_base();
                        saved.assign(ram, ram + 0x02000000);
                        bool ok = true;
                        const uint32_t stack_top = ee_stack_ptr();
                        try { eud_class217_update_l4(m); } catch (const std::exception&) { ok = false; }
                        ee_stack_ptr() = stack_top;
                        std::memcpy(ram, saved.data(), saved.size());
                        if (ok) break;
                    }
                    return Args{{m}, {}};
                }, [](A a) { eud_class217_update_l4(a.i[0]); return uint64_t{0}; });
            }
            l4check("eud_biped_hit", 0x002ba520, 240, Ret::None, [level, bipeds, pick, biped_prep, dealt] {
                return Args{{dealt([&] {
                level();
                const uint32_t m = pick(bipeds), pv = ee_read<uint32_t>(m + 0x78);
                static const int sts[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
                biped_prep(m, sts[ri(0, 8)]);
                if (coin()) w16(pv + 0x404, 0);
                for (uint32_t o = 0x24; o < 0x30; ++o) if (o != 0x2e && coin()) w8(pv + o, R() & 0xff);
                const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
                w8(m + 0xa4, idx);
                w32(e + 0x34, m);
                w32(e + 0x24, 0x210000u | (R() & 0xffff));
                wfrand(e, 4, -1, 1);
                w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x0013f350 + 0x2080) : m));
                wf(e + 0x2c, rf(0.1f, 6));
                seed_rand();
                return m;
                })}, {}};
            }, [](A a) { eud_class217_update_l4(a.i[0]); return uint64_t{0}; });
            // the reach's edge: standing or starting, no action timer, the walk along the path done, the hero 3.8 .. 4.7 away
            l4check("eud_biped_reach", 0x002ba520, 120, Ret::None, [level, bipeds, pick, biped_prep, dealt] {
                return Args{{dealt([&] {
                level();
                const uint32_t m = pick(bipeds), pv = ee_read<uint32_t>(m + 0x78);
                biped_prep(m, coin() ? 1 : 3);
                w16(pv + 0x402, 0);
                w16(pv + 0x43a, 1);
                w8(m + 0xa4, 0xff);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                const float r = rf(3.8f, 4.7f), a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a);
                ee_ref<Vec4>(0x0013f3d0) = q; ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = q;
                seed_rand();
                return m;
                })}, {}};
            }, [](A a) { eud_class217_update_l4(a.i[0]); return uint64_t{0}; });
            l4check("eud_biped_anim", 0x002ba3e0, 100, Ret::None, [level, bipeds, pick] {
                level();
                const uint32_t m = pick(bipeds);
                if (coin()) { w8(m + 0x20, 0); eud_class217_update_l4(m); }
                if (coin()) w32(ee_read<uint32_t>(m + 0x78) + 0x424, 0);
                return Args{{m, u(ri(0, 0xd)), u(ri(0, 4)), u(ri(0, 20))}, {}};
            }, [](A a) { biped_anim_set_l4(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
            l4check("eud_biped_rate", 0x002ba448, 100, Ret::None, [level, bipeds, pick] {
                level();
                const uint32_t m = pick(bipeds);
                if (coin()) { w8(m + 0x20, 0); eud_class217_update_l4(m); }
                if (coin()) w32(ee_read<uint32_t>(m + 0x78) + 0x424, 0);
                return Args{{m}, {rf(0, 3)}};
            }, [](A a) { biped_anim_rate_l4(a.i[0], a.f[0]); return uint64_t{0}; });
            l4check("eud_biped_explode", 0x002ba470, 100, Ret::None, [level, bipeds, pick] {
                level();
                seed_rand();
                return Args{{pick(bipeds)}, {}};
            }, [](A a) { biped_explode_l4(a.i[0]); return uint64_t{0}; });
        }

        // ---- P4.2: classes 340 (the Circular Saw Enemy), 427 (the Blarg Trooper), 432 / 434 (the pump's parts) and 466
        // (the flying ships): every state with its timers, sight, alert, animations, paths and boxes, hits of the weapons,
        // the hero's moby near or far; the saw carried (state 9, its carry record's states), the saws crowding the target
        // (the attack's spread); the pump's work at its ends or moving, its sound slots playing or not
        auto trial = [](void (*fn)(uint32_t), const std::function<uint32_t()>& deal) {
            // (a state that sends the port through a pointer outside RAM is not a game state: dealt again)
            static std::vector<uint8_t> saved;
            uint32_t m = 0;
            for (int attempt = 0; attempt < 20; ++attempt) {
                m = deal();
                uint8_t* ram = ee_get_ram_base();
                saved.assign(ram, ram + 0x02000000);
                bool ok = true;
                const uint32_t stack_top = ee_stack_ptr();
                try { fn(m); } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;
                std::memcpy(ram, saved.data(), saved.size());
                if (ok) break;
            }
            return m;
        };
        // a hit of the weapons on `m` (its slot's record varied), or none
        auto weapon_hit = [give_hit](uint32_t m) {
            const uint32_t P = 0x0013f350;
            if (coin()) { w8(m + 0xa4, 0xff); return; }
            give_hit(m, 0x210000u);
            if (ee_read<uint8_t>(m + 0xa4) == 0xff) return;
            const uint32_t e = OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6);
            w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
            wf(e + 0x1c, coin() ? 5627.95f : 0.0f);
            w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(P + 0x2080) : m));
            wf(e + 0x2c, coin() ? rf(0.1f, 6) : 0.0f);
        };
        // a sound slot word: none, or a slot playing for `m` (its record 0x13e550 + slot * 0x70) or for another moby
        auto sound_slot = [](uint32_t m, uint32_t word) {
            if (coin()) { w32(word, 0xffffffffu); return; }
            const uint32_t slot = u(ri(0, 15)), e = 0x0013e550u + slot * 0x70u;
            w32(word, slot);
            w32(e + 0x88, coin() ? m : 0u);
            w8(e + 0x74, u(ri(0, 1)));
        };
        auto anims = [](uint32_t m, int hi) {
            const uint32_t set = u(ri(0, hi));
            w8(m + 0x53, set);
            w8(m + 0x52, ri(0, 3) == 0 ? u(ri(0, hi)) : set);
            if (coin()) w8(m + 0x70, coin() ? 2u : 0u);
            if (coin()) { w8(m + 0x51, u(ri(0, 48))); w8(m + 0x50, u(ri(0, 48))); wf(m + 0x54, rf(0, 1)); }
        };
        const int32_t boxes = ee_read<int32_t>(OV(0x001600f0));
        auto some_box = [boxes] { return boxes > 0 ? u(ri(0, boxes - 1)) : 0xffffffffu; };
        const std::vector<uint32_t> saws = mobys_with(0x002c2270), troopers = mobys_with(0x002c4850);
        const std::vector<uint32_t> valves = mobys_with(0x002c6858), lifts = mobys_with(0x002c6bb8), ships = mobys_with(0x002ca420);
        if (info) std::printf("P42 saws %zu troopers %zu 432 %zu 434 %zu ships %zu boxes %d\n", saws.size(), troopers.size(), valves.size(), lifts.size(), ships.size(), boxes);

        // the saw: its pvars varied as the states read them; carried: the record (pvars +0x180) in a carry state
        auto saws_any = [saws, pick] { return pick(saws); };
        auto saw_prep = [some_path, some_box, hero_at, rate50, weapon_hit, sound_slot, anims, saws_any](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            rate50();
            if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class340_update_l4(m); }
            w8(m + 0x20, u(st));
            if (ri(0, 3) == 0) w32(pv + 0x38, 1);
            if (ri(0, 3) == 0) w16(pv + 0x136, u(ri(1, 3)));
            w16(pv + 0x146, ri(0, 2) == 0 ? 0u : u(ri(1, 3)));
            if (coin()) w16(pv + 0x144, u(ri(0, 3)));
            if (coin()) w8(pv + 0x14c, u(ri(0, 3)));
            w8(pv + 0x14d, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            if (coin()) w16(pv + 0x14e, R() & 3);
            if (ri(0, 3) == 0) w32(pv + 0x138, 0xffffffffu); else if (ri(0, 2) == 0) { const int32_t p = some_path(3); if (p >= 0) w32(pv + 0x138, u(p)); }
            if (ri(0, 2) == 0) { const int32_t p = some_path(2); if (p >= 0) w32(pv + 0x150, u(p)); }
            if (const int32_t p = ee_read<int32_t>(pv + 0x150); p >= 0 && p < 1000) {
                const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4);
                const int32_t n = path != 0 ? ee_read<int32_t>(path) : 1;
                w32(pv + 0x154, u(ri(0, std::max(0, n - 1))));
                if (path != 0 && ri(0, 2) == 0) {  // (at its point)
                    Vec4 q = ee_ref<Vec4>(path + 0x10 + ee_read<uint32_t>(pv + 0x154) * 16);
                    q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f);
                    ee_ref<Vec4>(m + 0x10) = q;
                }
            }
            for (uint32_t k = 0; k < 4; ++k) if (coin()) w32(pv + 0x160 + k * 4, coin() ? 0xffffffffu : some_box());
            if (coin()) { w16(pv + 0x158, some_box() & 0xffff); w16(pv + 0x15a, u(ri(0, 2))); }
            if (ri(0, 4) == 0) {  // (inside its box)
                const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + u(ee_read<int16_t>(pv + 0x158)) * 0x80;
                Vec4 q = ee_ref<Vec4>(b + 0x30); q.x += rf(-1.2f, 1.2f); q.y += rf(-1.2f, 1.2f);
                ee_ref<Vec4>(m + 0x10) = q;
                if (coin()) wf(m + 0x48, ee_read<float>(b + 0x78) + rf(-0.2f, 0.2f));
            }
            if (coin()) wf(pv + 0x13c, coin() ? 0.0f : rf(0, 0.3f));
            if (coin()) wf(pv + 0x140, rf(2, 30));
            if (coin()) wf(pv + 0x20, coin() ? rf(0.1f, 1) : 1.0f);
            if (ri(0, 9) == 0) w8(pv + 0x2e, 0);
            if (ri(0, 7) == 0) wf(m + 0x18, ee_read<float>(pv + 0x128) - rf(4, 7));
            anims(m, 9);
            sound_slot(m, pv + 0x238);
            if (coin()) { wf(pv + 0x230, rf(0, 1100)); wf(pv + 0x234, rf(-3, 5)); }
            if (st == 9) {  // carried: the record (pvars +0x180) in a state 1 .. 8 as the carry system leaves it (batch 530's)
                const uint32_t r = pv + 0x180, hero = ee_read<uint32_t>(0x001413d0);
                w32(pv + 0x1f0, L4_ADDR(0x00161878));  // (its parameters, as its state 0 sets them)
                w16(r + 0x68, u(ri(1, 8)));
                wf(r + 0xc, rf(-1, 2));
                wf(r + 0x6c, rf(0, 1.2f));
                w32(r + 0x60, coin() ? hero : saws_any());
                wf(r + 0x64, rf(0.5f, 5));
                w16(r + 0x6a, u(ri(0, 3)));
                w32(0x0013f350 + 0x2050 + 4 * ee_read<uint16_t>(r + 0x6a), saws_any());
                Vec4 q = ee_ref<Vec4>(m + 0x10); q.x += rf(-5, 5); q.y += rf(-5, 5); ee_ref<Vec4>(r) = q;
                wfrand(r + 0x10, 3, -0.4f, 0.4f);
                w32(r + 0x74, ri(0, 2) == 0 ? 0u : (coin() ? hero : saws_any()));
                wf(r + 0x7c, rf(0, 0.1f));
                w32(r + 0x94, u(ri(0, 400)));
                wf(r + 0x98, rf(0, 0.3f));
                if (coin()) wf(m + 0x18, ri(0, 3) == 0 ? rf(0.0f, 2.5f) : rf(1.8f, 2.6f));
            }
            if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 1u);
            hero_at(m, 0, ri(0, 2) == 0 ? 3.0f : (coin() ? 20.0f : 50.0f));
            weapon_hit(m);
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
        };
        // (left out of the saw's checks only: the collision's pushed w lane and the segment record +0x50 .. +0x7c, the VU0
        // leftovers walker_step's / segment_collide's ports leave as they are: COMMON's known differences, harmless)
        const std::vector<uint32_t> before_saw_ignored = g_ignored_words;
        g_ignored_words.push_back(OV(0x00173e7c));
        for (uint32_t a = OV(0x00173e90); a < OV(0x00173ec0); a += 4) g_ignored_words.push_back(a);
        if (saws.empty()) std::printf("eud_saw                    L4:0x002c2270  no class 340 moby on this level\n");
        else {
            for (int st = 0; st <= 10; ++st) {
                char name[32];
                std::snprintf(name, sizeof name, "eud_saw_st%d", st);
                l4check(name, 0x002c2270, 60, Ret::None, [level, saws, pick, saw_prep, trial, st] {
                    return Args{{trial(eud_class340_update_l4, [&] { level(); const uint32_t m = pick(saws); saw_prep(m, st); return m; })}, {}};
                }, [](A a) { eud_class340_update_l4(a.i[0]); return uint64_t{0}; });
            }
            // the saws crowding the target: some of the others moved next to the hero
            auto crowd = [level, saws, pick, hero_at, rate50](uint32_t& m) {
                level();
                m = pick(saws);
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                hero_at(m, 0, 12);
                const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
                for (uint32_t o : saws) if (o != m && coin()) { Vec4 q = h; q.x += rf(-6, 6); q.y += rf(-6, 6); ee_ref<Vec4>(o + 0x10) = q; }
                if (ri(0, 3) == 0) w32(pv + 0x38, 1);
                if (coin()) wf(pv + 0x140, rf(2, 30));
                if (coin()) wf(L4_ADDR(0x00161884), rf(0, 2));
                rate50();
            };
            l4check("eud_saw_may_attack", 0x002c1e98, 200, Ret::Int32, [crowd] { uint32_t m = 0; crowd(m); return Args{{m}, {}}; },
                    [](A a) { return r32(static_cast<uint32_t>(saw_may_attack_l4(a.i[0]))); });
            l4check("eud_saw_attack_point", 0x002c1fb8, 200, Ret::Float, [crowd] { uint32_t m = 0; crowd(m); wfrand(OUT, 4, -9, 9); return Args{{m, OUT}, {}}; },
                    [](A a) { return rf32(saw_attack_point_l4(a.i[0], a.i[1])); });
            l4check("eud_saw_sound", 0x002c1b80, 200, Ret::None, [level, saws, pick, anims, sound_slot, rate50] {
                level();
                const uint32_t m = pick(saws), pv = ee_read<uint32_t>(m + 0x78);
                w8(m + 0x20, coin() ? 9u : u(ri(0, 8)));
                anims(m, 10);
                sound_slot(m, pv + 0x238);
                if (ri(0, 3) != 0 && ee_read<int32_t>(pv + 0x238) != -1) {  // (playing for it)
                    const uint32_t e = 0x0013e550u + ee_read<uint32_t>(pv + 0x238) * 0x70u;
                    w32(e + 0x88, m); w8(e + 0x74, 1);
                }
                static const float goals[] = {614.4f, 819.2f, 1024.0f};
                wf(pv + 0x230, coin() ? rf(0, 1100) : goals[ri(0, 2)] + rf(-60, 60));
                wf(pv + 0x234, coin() ? rf(-3, 5) : static_cast<float>(ri(-2, 4)) + rf(-0.2f, 0.2f));
                rate50();
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { saw_sound_update_l4(a.i[0]); return uint64_t{0}; });
            l4check("eud_saw_die", 0x002c1d70, 100, Ret::None, [level, saws, pick, sound_slot] {
                level();
                const uint32_t m = pick(saws);
                sound_slot(m, ee_read<uint32_t>(m + 0x78) + 0x238);
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { saw_die_l4(a.i[0]); return uint64_t{0}; });
        }

        g_ignored_words = before_saw_ignored;

        // the trooper: its paths, timers, gun, the camera looking its way or not
        auto trooper_prep = [some_path, hero_at, rate50, weapon_hit, anims](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            rate50();
            if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class427_update_l4(m); }
            w8(m + 0x20, u(st));
            if (ri(0, 3) == 0) w32(pv + 0x38, 1);
            if (ri(0, 3) == 0) w8(pv + 0xfe, u(ri(1, 3)));
            for (uint32_t o : {0xe0u, 0xe4u}) if (ri(0, 3) == 0) { const int32_t p = some_path(3); w32(pv + o, p >= 0 && coin() ? u(p) : 0xffffffffu); }
            if (coin()) w32(pv + 0xe8, u(ri(0, 1)));
            w16(pv + 0xf0, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            w16(pv + 0xf2, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
            if (coin()) w8(pv + 0xff, u(ri(0, 2)));
            if (coin()) w16(pv + 0x108, u(ri(0, 2)));
            if (coin()) wf(pv + 0xf4, rf(0, 0.4f));
            if (ri(0, 4) == 0) w32(pv + 0xf8, 0);
            if (coin()) w8(pv + 0xfc, u(ri(0, 1)));
            if (coin()) w8(pv + 0xfd, u(ri(0, 4)));
            if (coin()) wf(pv + 0xdc, rf(2, 40));
            if (coin()) wf(pv + 0xd8, rf(0, 40));
            if (coin()) wf(pv + 0x20, coin() ? rf(0.1f, 1) : 3.0f);
            if (ri(0, 9) == 0) w8(pv + 0x2e, 0);
            if (ri(0, 7) == 0) wf(m + 0x18, ee_read<float>(pv + 0xd4) - rf(4, 7));
            anims(m, 12);
            // the camera point and heading (L4:0x166f40 / 0x166f58): looking at the trooper or away
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f), d = rf(3, 30);
            c.x += d * std::cos(a); c.y += d * std::sin(a);
            ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c;
            wf(L4_ADDR(0x00166f58), coin() ? std::atan2(ee_read<float>(m + 0x14) - c.y, ee_read<float>(m + 0x10) - c.x) + rf(-0.8f, 0.8f) : rf(-3.1f, 3.1f));
            if (coin()) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 1u);
            if (ri(0, 3) == 0) hero_at(m, 2.3f, 3.7f);
            else hero_at(m, 0, ri(0, 2) == 0 ? 3.0f : (coin() ? 25.0f : 60.0f));
            weapon_hit(m);
            w32(OV(0x0015f5cc), R() & 0xffff);
            seed_rand();
        };
        if (troopers.empty()) std::printf("eud_trooper                L4:0x002c4850  no class 427 moby on this level\n");
        else {
            for (int st = 0; st <= 12; ++st) {
                char name[32];
                std::snprintf(name, sizeof name, "eud_trooper_st%d", st);
                l4check(name, 0x002c4850, 60, Ret::None, [level, troopers, pick, trooper_prep, trial, st] {
                    return Args{{trial(eud_class427_update_l4, [&] { level(); const uint32_t m = pick(troopers); trooper_prep(m, st); return m; })}, {}};
                }, [](A a) { eud_class427_update_l4(a.i[0]); return uint64_t{0}; });
            }
            l4check("eud_trooper_retire", 0x002c4808, 100, Ret::None, [level, troopers, pick] {
                level();
                const uint32_t m = pick(troopers);
                if (coin()) { w8(m + 0x20, 0); eud_class427_update_l4(m); }
                return Args{{m}, {}};
            }, [](A a) { trooper_retire_l4(a.i[0]); return uint64_t{0}; });
        }

        // the pump's parts: the pump's work at an end, moving or still, the pump worked (state 3) or not, the sound slots,
        // the hero standing on the held moby or not; class 432 as either of its models
        auto pump_prep = [rate50, sound_slot](uint32_t m, int st, bool lift) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
            rate50();
            if (st != 0 && coin()) { w8(m + 0x20, 0); (lift ? eud_class434_update_l4 : eud_class432_update_l4)(m); }
            w8(m + 0x20, u(st));
            const int32_t idx = ee_read<int32_t>(pv);
            if (idx != -1) {
                const uint32_t pump = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(idx) << 8), pp = ee_read<uint32_t>(pump + 0x78);
                switch (ri(0, 3)) {
                case 0: wf(pp, 1.0f); break;
                case 1: wf(pp, 0.0f); break;
                default: wf(pp, rf(0, 1)); break;
                }
                if (coin()) w8(pump + 0x20, coin() ? 3u : u(ri(0, 4)));
                if (coin()) wf(pp + 0x30, rf(0, 8));
                wf(m + 0x54, coin() ? ee_read<float>(pp) : (coin() ? 1.0f - ee_read<float>(pp) : rf(0, 1)));
            } else if (coin()) {
                wf(m + 0x54, rf(0, 1));
            }
            if (ri(0, 7) == 0) w32(pv, 0xffffffffu);
            sound_slot(m, pv + (lift ? 8u : 4u));
            if (lift) {
                if (coin()) w32(pv + 0xc, u(ri(0, 1)));
                w32(P + 0x208c, coin() ? u(ri(0, 1)) : (coin() ? (coin() ? 9u : 0xcu) : u(ri(2, 20))));
                const int32_t held = ee_read<int32_t>(pv + 4);
                w32(P + 0x2fc, held != -1 && coin() ? ee_read<uint32_t>(OV(0x0015ffd8)) + (u(held) << 8) : 0u);
            } else if (coin()) {
                w16(m + 0xa6, coin() ? 0x1b0u : 0x41cu);
            }
            seed_rand();
        };
        for (int lift = 0; lift <= 1; ++lift) {
            const std::vector<uint32_t>& subj = lift ? lifts : valves;
            const char* nm = lift ? "eud_pump_part434" : "eud_pump_part432";
            if (subj.empty()) { std::printf("%-26s no moby on this level\n", nm); continue; }
            l4check(nm, lift ? 0x002c6bb8 : 0x002c6858, 300, Ret::None, [level, subj, pick, pump_prep, lift] {
                level();
                const uint32_t m = pick(subj);
                pump_prep(m, ri(0, 3), lift != 0);
                return Args{{m}, {}};
            }, [lift](A a) { (lift ? eud_class434_update_l4 : eud_class432_update_l4)(a.i[0]); return uint64_t{0}; });
        }

        // the ships: their paths, boxes and stops, the hits, hidden or shown, the camera looking or not
        auto ship_prep = [some_box, hero_at, rate50, weapon_hit](uint32_t m, int st) {
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            rate50();
            if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class466_update_l4(m); }
            w8(m + 0x20, u(st));
            for (uint32_t k = 0; k < 4; ++k) if (coin()) w32(pv + 0x150 + k * 4, coin() ? 0xffffffffu : some_box());
            if (coin()) { w8(pv + 0x148, some_box() & 0xff); w8(pv + 0x149, u(ri(0, 3))); w16(pv + 0x14a, u(ri(0, 3))); }
            if (ri(0, 3) == 0) {  // (at the box's centre, or inside the box)
                const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + u(ee_read<uint8_t>(pv + 0x148)) * 0x80;
                Vec4 q = ee_ref<Vec4>(b + 0x30); q.x += rf(-1.5f, 1.5f); q.y += rf(-1.5f, 1.5f); q.z += rf(-1.5f, 1.5f);
                ee_ref<Vec4>(m + 0x10) = q;
            }
            if (coin()) wf(pv + 0x14c, rf(-3.1f, 3.1f));
            if (coin()) { w32(pv + 0x174, u(ri(0, 1))); w32(pv + 0x170, u(ri(-1, 3))); }
            if (ri(0, 3) == 0) w32(0x0015edf8, u(ri(7, 10)));
            if (ri(0, 3) == 0) w8(0x0013d40e, u(ri(0, 1)));
            Vec4 c = ee_ref<Vec4>(m + 0x10);
            const float a = rf(-3.1f, 3.1f), d = rf(3, 40);
            c.x += d * std::cos(a); c.y += d * std::sin(a);
            ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c;
            wf(L4_ADDR(0x00166f58), rf(-3.1f, 3.1f));
            hero_at(m, 0, 40);
            weapon_hit(m);
            seed_rand();
        };
        if (ships.empty()) std::printf("eud_ship                   L4:0x002ca420  no class 466 moby on this level\n");
        else
            for (int st = 0; st <= 3; ++st) {
                char name[32];
                std::snprintf(name, sizeof name, "eud_ship_st%d", st);
                l4check(name, 0x002ca420, 100, Ret::None, [level, ships, pick, ship_prep, trial, st] {
                    return Args{{trial(eud_class466_update_l4, [&] { level(); const uint32_t m = pick(ships); ship_prep(m, st); return m; })}, {}};
                }, [](A a) { eud_class466_update_l4(a.i[0]); return uint64_t{0}; });
            }

        // the targeted cases: the saw hit (its reactions: every case a hit, its hit state's bytes varied), at its box (the
        // arrival, the rest's end, the turn back to its path), settling from a carry's slide; the trooper firing; the
        // ship at its box's centre and heading, and back where it left its path; the pump's lift waiting for the hero
        g_ignored_words.push_back(OV(0x00173e7c));
        for (uint32_t a = OV(0x00173e90); a < OV(0x00173ec0); a += 4) g_ignored_words.push_back(a);
        if (!saws.empty()) {
            l4check("eud_saw_hit", 0x002c2270, 200, Ret::None, [level, saws, pick, saw_prep, trial] {
                return Args{{trial(eud_class340_update_l4, [&] {
                    level();
                    const uint32_t m = pick(saws), pv = ee_read<uint32_t>(m + 0x78);
                    static const int sts[] = {1, 2, 3, 4, 5, 6, 7, 9};
                    saw_prep(m, sts[ri(0, 7)]);
                    w8(pv + 0x14d, 0);
                    for (uint32_t o = 0x24; o < 0x30; ++o) if (o != 0x2e && coin()) w8(pv + o, R() & 0xff);
                    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
                    w8(m + 0xa4, idx);
                    w32(e + 0x34, m);
                    w32(e + 0x24, 0x210000u | (R() & 0xffff));
                    wfrand(e, 4, -1, 1);
                    w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                    w32(e + 0x20, ri(0, 3) == 0 ? 0u : (coin() ? ee_read<uint32_t>(0x0013f350 + 0x2080) : saws[static_cast<size_t>(ri(0, static_cast<int>(saws.size()) - 1))]));
                    wf(e + 0x2c, rf(0.1f, 3));
                    seed_rand();
                    return m;
                })}, {}};
            }, [](A a) { eud_class340_update_l4(a.i[0]); return uint64_t{0}; });
            l4check("eud_saw_box", 0x002c2270, 200, Ret::None, [level, saws, pick, saw_prep, trial, some_path, boxes] {
                return Args{{trial(eud_class340_update_l4, [&] {
                    level();
                    const uint32_t m = pick(saws), pv = ee_read<uint32_t>(m + 0x78);
                    saw_prep(m, 5);
                    w8(m + 0xa4, 0xff);
                    w16(pv + 0x146, 0);
                    const int32_t p = some_path(2);
                    if (p >= 0) { w32(pv + 0x150, u(p)); w32(pv + 0x154, u(ri(0, ee_read<int32_t>(ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4)) - 1))); }
                    const uint32_t bi = u(ri(0, std::max(0, boxes - 1)));
                    w16(pv + 0x158, bi);
                    const uint32_t sub = u(ri(0, 2));
                    w16(pv + 0x15a, sub);
                    const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + bi * 0x80;
                    Vec4 q = ee_ref<Vec4>(b + 0x30); q.x += rf(-1.1f, 1.1f); q.y += rf(-1.1f, 1.1f);
                    ee_ref<Vec4>(m + 0x10) = q;
                    if (sub == 0) wf(m + 0x48, ee_read<float>(b + 0x78) + rf(-0.25f, 0.25f));
                    if (sub == 1) { w8(m + 0x53, coin() ? 7u : 0u); w8(m + 0x52, ee_read<uint8_t>(m + 0x53)); w8(m + 0x70, coin() ? 2u : 0u); }
                    if (sub == 2 && p >= 0) {
                        const Vec4 t = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001b04b0) + u(p) * 4) + 0x10 + ee_read<uint32_t>(pv + 0x154) * 16);
                        wf(m + 0x48, std::atan2(t.y - q.y, t.x - q.x) + rf(-0.25f, 0.25f));
                    }
                    seed_rand();
                    return m;
                })}, {}};
            }, [](A a) { eud_class340_update_l4(a.i[0]); return uint64_t{0}; });
            l4check("eud_saw_settle", 0x002c2270, 120, Ret::None, [level, saws, pick, saw_prep, trial] {
                return Args{{trial(eud_class340_update_l4, [&] {
                    level();
                    const uint32_t m = pick(saws), pv = ee_read<uint32_t>(m + 0x78);
                    saw_prep(m, 9);
                    w8(m + 0xa4, 0xff);
                    const uint32_t r = pv + 0x180;
                    w16(r + 0x68, 7);  // (falling and sliding: slow, on the floor)
                    wfrand(r + 0x10, 3, -0.02f, 0.02f);
                    wf(r + 0x18, coin() ? 0.0f : rf(-0.05f, 0.0f));
                    if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-0.2f, 0.2f));
                    seed_rand();
                    return m;
                })}, {}};
            }, [](A a) { eud_class340_update_l4(a.i[0]); return uint64_t{0}; });
        }
        g_ignored_words = before_saw_ignored;
        if (!troopers.empty())
            l4check("eud_trooper_fire", 0x002c4850, 200, Ret::None, [level, troopers, pick, trooper_prep, trial] {
                return Args{{trial(eud_class427_update_l4, [&] {
                    level();
                    const uint32_t m = pick(troopers), pv = ee_read<uint32_t>(m + 0x78);
                    w8(m + 0x20, 0); eud_class427_update_l4(m);  // (its gun spawned)
                    trooper_prep(m, coin() ? 6 : 5);
                    if (ee_read<uint32_t>(pv + 0xf8) == 0) { w8(m + 0x20, 0); eud_class427_update_l4(m); w8(m + 0x20, 6); }
                    w8(m + 0x53, 5); w8(m + 0x52, 5);
                    w8(m + 0x51, u(ri(0, 8))); w8(m + 0x50, u(ri(0, 8))); wf(m + 0x54, rf(0, 1));
                    wf(pv + 0xd8, rf(0, 3));
                    w8(pv + 0xfd, u(ri(0, 3)));
                    w16(pv + 0xf2, 0);
                    w8(m + 0xa4, 0xff);
                    seed_rand();
                    return m;
                })}, {}};
            }, [](A a) { eud_class427_update_l4(a.i[0]); return uint64_t{0}; });
        if (!ships.empty())
            l4check("eud_ship_box", 0x002ca420, 200, Ret::None, [level, ships, pick, ship_prep, trial, boxes] {
                return Args{{trial(eud_class466_update_l4, [&] {
                    level();
                    const uint32_t m = pick(ships), pv = ee_read<uint32_t>(m + 0x78);
                    ship_prep(m, ri(1, 3));
                    w8(m + 0xa4, 0xff);
                    const uint32_t mode = u(ri(1, 3));
                    w8(pv + 0x149, mode);
                    w16(pv + 0x14a, coin() ? 0u : u(ri(1, 3)));
                    const uint32_t bi = u(ri(0, std::max(0, boxes - 1)));
                    w8(pv + 0x148, bi);
                    const uint32_t b = ee_read<uint32_t>(OV(0x001600ec)) + bi * 0x80;
                    Vec4 q = mode == 3 ? ee_ref<Vec4>(pv + 0x160) : ee_ref<Vec4>(b + 0x30);
                    if (mode == 3 && coin()) { q = ee_ref<Vec4>(m + 0x10); ee_ref<Vec4>(pv + 0x160) = q; }
                    q.x += rf(-1.1f, 1.1f); q.y += rf(-1.1f, 1.1f);
                    ee_ref<Vec4>(m + 0x10) = q;
                    wf(m + 0x48, (mode == 3 ? ee_read<float>(pv + 0x14c) : ee_read<float>(b + 0x78)) + rf(-0.2f, 0.2f));
                    seed_rand();
                    return m;
                })}, {}};
            }, [](A a) { eud_class466_update_l4(a.i[0]); return uint64_t{0}; });
        if (!lifts.empty())
            l4check("eud_pump_wait434", 0x002c6bb8, 200, Ret::None, [level, lifts, pick, pump_prep] {
                level();
                const uint32_t m = pick(lifts), pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                pump_prep(m, 1, true);
                if (const int32_t idx = ee_read<int32_t>(pv); idx != -1) {
                    const uint32_t pump = ee_read<uint32_t>(OV(0x0015ffd8)) + (u(idx) << 8);
                    wf(ee_read<uint32_t>(pump + 0x78), 1.0f);
                    if (coin()) w8(pump + 0x20, u(ri(0, 2)));
                }
                w32(pv + 0xc, ri(0, 3) == 0 ? 1u : 0u);
                static const uint32_t sts[] = {0, 1, 9, 0xc, 2, 5};
                w32(P + 0x208c, sts[ri(0, 5)]);
                const int32_t held = ee_read<int32_t>(pv + 4);
                w32(P + 0x2fc, held != -1 && ri(0, 3) != 0 ? ee_read<uint32_t>(OV(0x0015ffd8)) + (u(held) << 8) : 0u);
                seed_rand();
                return Args{{m}, {}};
            }, [](A a) { eud_class434_update_l4(a.i[0]); return uint64_t{0}; });
        g_extra_noop_hooks.clear();
    }

    // ---- P4.1: the four Veldin functions only Eudora's code calls (eudora_walker.cpp), each against the played level's
    // own copy (the overlay map's), on records built in the test area: the animation key at a frame, a gait's foot
    // ranges, the legs' reset, the fall time
    check("anim_seq_key_at", 0x0024ed18, 400, Ret::Int32, [] {
        const uint32_t seq = T, keys = T2;
        const int n = ri(0, 3) == 0 ? ri(0, 2) : ri(3, 12);
        w8(seq + 0x10, u(n));
        int16_t t = static_cast<int16_t>(ri(-40, 40));
        for (int i = 0; i < n; ++i) {
            w32(seq + 0x1c + u(i) * 4, keys + u(i) * 0x10);
            w16(keys + u(i) * 0x10 + 4, static_cast<uint16_t>(t));
            t = static_cast<int16_t>(t + (ri(0, 3) == 0 ? 0 : ri(1, 60)));
        }
        return Args{{seq}, {coin() ? rf(-5, 50) : static_cast<float>(ri(-3, 50)) + 0.0625f * static_cast<float>(ri(0, 15))}};
    }, [](A a) { return r32(static_cast<uint32_t>(anim_seq_key_at(a.i[0], a.f[0]))); });
    check("gait_phase_flags", 0x002a13f0, 400, Ret::Int32, [] {
        for (uint32_t o = 0x18; o < 0x50; o += 4) wf(T + o, rf(0, 40));
        const float t = coin() ? rf(-2, 42) : ee_read<float>(T + 0x18 + u(ri(0, 13)) * 4) + (coin() ? 0.0f : rf(-1.2f, 1.2f));
        return Args{{T}, {t}};
    }, [](A a) { return r32(gait_phase_flags(a.i[0], a.f[0])); });
    check("walker_legs_reset", 0x002a1538, 100, Ret::None, [] {
        wrand(T, 0x200);
        for (uint32_t leg = T; leg < T + 0x160; leg += 0xb0) { w8(leg + 0x121, u(ri(0, 1))); w8(leg + 0x161, u(ri(0, 1))); }
        return Args{{T}, {}};
    }, [](A a) { walker_legs_reset(a.i[0]); return uint64_t{0}; });
    check("walker_fall_time", 0x002a15c8, 400, Ret::Float, [] {
        wf(M + 0x18, rf(-5, 50));
        wf(T + 0x68, ri(0, 3) == 0 ? ee_read<float>(M + 0x18) : rf(-5, 50));
        wf(T + 0xd8, rf(-2, 2));
        wf(T + 0xdc, coin() ? -rf(0.001f, 0.1f) : rf(-0.1f, 0.1f));
        return Args{{M, T}, {}};
    }, [](A a) { return rf32(walker_fall_time(a.i[0], a.i[1])); });

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
