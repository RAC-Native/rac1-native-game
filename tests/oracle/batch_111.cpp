// Oracle batch 111 (task 0337): Eudora's own moby classes (level 4) on its real level data. Run with
// ORACLE_LEVEL=4 build/tests/ovl_oracle --batch 111 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Eudora's own function at its own address, the port the one registered for it (the
// transliterations eudora_classes_tr_N.cpp, the hand ports eudora_*.cpp, the shared ports of the classes and camera
// functions other levels carry too: shared_*.cpp, novalis_*.cpp, kerwan_camera.cpp; class 484's retail update is a bare
// tail call of moby_retire).

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
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
        }, [](A a) { return r32(eud_hook_10_l4(a.i[0])); });
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
                const uint32_t r = eud_hook_10_l4(m), hero = ee_read<uint32_t>(0x001413d0);  // (the record the saw's hook +0x10 returns)
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
                    if (ri(0, 3) == 0) for (uint32_t o = 0x24; o < 0x30; ++o) if (o != 0x2e && coin()) w8(pv + o, R() & 0xff);
                    // a hit of the kinds the reaction table turns into each reaction (+0x28 the kind, +0x29 its strength,
                    // +0x2a the weapon, +0x1c the push's mark: batch 112's hit_kinds), from none, the hero's moby or a saw
                    const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
                    w8(m + 0xa4, idx);
                    w32(e + 0x34, m);
                    w32(e + 0x24, 0x210000u | (R() & 0xffff));
                    wfrand(e, 4, -1, 1);
                    w8(e + 0x28, u(ri(0, 7))); w8(e + 0x29, u(ri(0, 3)));
                    w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 3)));
                    wf(e + 0x1c, coin() ? u2f(0x45afdf66u) : 0.0f);
                    const uint32_t other = saws[static_cast<size_t>(ri(0, static_cast<int>(saws.size()) - 1))];
                    switch (ri(0, 2)) {
                    case 0: w32(e + 0x20, 0); break;
                    case 1: w32(0x001403e0, other); w32(e + 0x20, other); break;
                    default: w32(e + 0x20, coin() ? ee_read<uint32_t>(0x0013f350 + 0x2080) : other); break;
                    }
                    wf(e + 0x2c, rf(0.1f, 3));
                    // the repeat timer of the hit state (+0x1c, with its last weapon +0x14 the hit's): only while it runs does the
                    // reaction keep the table's animation (the saw's reactions 3 .. 6) instead of 0xb / 0xc / 0xd
                    if (ri(0, 2) != 0) {
                        const uint32_t att = ee_read<uint32_t>(e + 0x20);
                        const uint32_t weap = ee_read<float>(e + 0x1c) == u2f(0x45afdf66u) ? ee_read<uint16_t>(e + 0x2a)
                                                                                        : (att != 0 ? ee_read<uint16_t>(att + 0xa6) : 0x47u);
                        w16(pv + 0x34, ri(0, 5) == 0 ? u(ri(0, 3)) : weap);
                        w16(pv + 0x3c, coin() ? u(ri(2, 40)) : 1u);
                    }
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
                    const uint32_t m = pick(saws);
                    saw_prep(m, 9);
                    w8(m + 0xa4, 0xff);
                    const uint32_t r = eud_hook_10_l4(m);
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
        if (!troopers.empty())
            l4check("eud_trooper_band", 0x002c4850, 200, Ret::None, [level, troopers, pick, trooper_prep, trial] {
                return Args{{trial(eud_class427_update_l4, [&] {
                    level();
                    const uint32_t m = pick(troopers), pv = ee_read<uint32_t>(m + 0x78);
                    const Vec4 h = ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10);  // (the hero's moby where the level has it)
                    const uint32_t e0 = ee_read<uint32_t>(pv + 0xe0), e4 = ee_read<uint32_t>(pv + 0xe4);
                    trooper_prep(m, 1);
                    w32(pv + 0x38, 0); w8(pv + 0xfe, 0);
                    w8(pv + 0x2e, 1); w32(pv + 0xe0, e0); w32(pv + 0xe4, e4);  // (on the level, its paths as placed)
                    w16(pv + 0xf2, 0);
                    w32(pv + 0x108, coin() ? u(ri(2, 60)) : 0u);  // (its run's wait: ticked before the state's test, 1 is 0 there)
                    w8(m + 0xa4, 0xff);
                    // (the trooper put 2.4 .. 3.6 from the hero's moby where the level placed it, at its height: the line of fire
                    // meets the moby only where the collision's grid has it, so the hero and his moby go back there)
                    ee_ref<Vec4>(0x0013f3d0) = h;
                    ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = h;
                    {
                        const float r = rf(2.4f, 3.6f), a = rf(-3.1f, 3.1f);
                        Vec4 t = h; t.x += r * std::cos(a); t.y += r * std::sin(a); t.z = h.z - rf(0, 0.5f);
                        ee_ref<Vec4>(m + 0x10) = t;
                    }
                    const Vec4 q = h;
                    if (coin()) wf(m + 0x48, std::atan2(q.y - ee_read<float>(m + 0x14), q.x - ee_read<float>(m + 0x10)));
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
        // ---- P4.3: class 563, the Woodcut Bot (eudora_woodcut_bot.cpp) and its four helpers, and Eudora's copy of the debris
        // chunk's spawn (L4:0x2e4fe0) against the shared port chunk_spawn: every state with its timers, sight, alert, area,
        // home, animations and their frames (the swing's 16 .. 27, the lunge's 28, the chips' 36), the walker set up or not,
        // the hero within reach, at the sight's edges or far, in its area or out, faced or not; hits of the weapons with their
        // reactions and sources; the push, the twitch, the fall (and the fall off the level). (Paste before
        // `g_extra_noop_hooks.clear();` at the end of the P4.2 section.)
        {
            // (left out of these checks only, as in the saw's: the collision's pushed w lane and the segment record +0x50 ..
            // +0x7c, the VU0 leftovers the ports of walker_step / segment_collide leave as they are: COMMON's known
            // differences, harmless. The class calls walker_step (the push) and segment_collide (the swing).)
            const std::vector<uint32_t> before_bot_ignored = g_ignored_words;
            g_ignored_words.push_back(OV(0x00173e7c));
            for (uint32_t a = OV(0x00173e90); a < OV(0x00173ec0); a += 4) g_ignored_words.push_back(a);
            // (and the w lane of a dropped bolt's velocity, its pvars' +0x2c: bolts_drop's port sets it 0, the original
            // leaves a stale stack word there, COMMON's kind of known difference; only the pvars of the slots free on the
            // level, where the bolts of the fall off the level and of the death spawn)
            {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffdc)), last = ee_read<uint32_t>(OV(0x0015ffe0)), pvs = ee_read<uint32_t>(OV(0x0015ffe8));
                bool past_end = false;  // (0xff: the used range's end, every slot past it free as the spawns move it up)
                for (uint32_t o = first; o < last && last - first < 0x00400000u; o += 0x100) {
                    past_end = past_end || ee_read<uint8_t>(o + 0x20) == 0xff;
                    if (past_end || ee_read<uint8_t>(o + 0x20) == 0xfe) g_ignored_words.push_back(pvs + ((o - first) >> 8 << 7) + 0x2c);
                }
                if (std::getenv("B111_P41") != nullptr) {
                    std::printf("P43 bolt lanes ignored %zu (pvars %08x, mobys %08x .. %08x)\n", g_ignored_words.size() - before_bot_ignored.size() - 13, pvs, first, last);
                }
            }
            // the centre of a path's polygon (the mean of its points)
            auto area_centre = [path_at](int32_t p) {
                const uint32_t a = path_at(p);
                const int32_t n = std::max(1, std::min(ee_read<int32_t>(a), 200));
                Vec4 c{};
                for (int32_t k = 0; k < n; ++k) { const Vec4 q = ee_ref<Vec4>(a + 0x10 + u(k) * 16); c.x += q.x; c.y += q.y; c.z += q.z; }
                c.x /= static_cast<float>(n); c.y /= static_cast<float>(n); c.z /= static_cast<float>(n);
                return c;
            };
            // the animation's time exactly `t`: its current key copied to T2 + 0x200 with the time t (in 1/16ths), no blend
            auto bot_frame = [](uint32_t m, float t) {
                const uint32_t key = T2 + 0x200;
                const uint32_t cur = ee_read<uint8_t>(m + 0x52) == 0xff ? ee_read<uint32_t>(m + 0x6c) : ee_read<uint32_t>(m + 0x68);
                if (cur >= 0x00100000u && cur < 0x02000000u - 0x20u) for (uint32_t o = 0; o < 0x20; o += 4) w32(key + o, ee_read<uint32_t>(cur + o));
                w16(key + 4, u(static_cast<int>(t * 16.0f)) & 0xffffu);
                w32(m + 0x68, key);
                w32(m + 0x6c, key);
                wf(m + 0x54, 0.0f);
            };
            // a joint node's "linked" byte (+1) set to `v`: 0 unlinks the node from the moby's list (+0x64) first, as a node
            // never linked is (a 0 on a node still on the list puts it on twice: the list a loop, the pose's walk endless)
            auto node_flag = [](uint32_t m, uint32_t node, uint32_t v) {
                if (v == 0) {
                    uint32_t at = m + 0x64;
                    for (int k = 0; k < 64; ++k) {
                        const uint32_t o = ee_read<uint32_t>(at);
                        if (o == 0 || o >= 0x02000000u) break;
                        if (o == node) { w32(at, ee_read<uint32_t>(node + 8)); break; }
                        at = o + 8;
                    }
                }
                w8(node + 1, v);
            };
            // the hero (and its moby: what target_select picks) at `q`
            auto hero_put = [](const Vec4& q) { ee_ref<Vec4>(0x0013f3d0) = q; ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = q; };
            // its heading: at the hero (within a few degrees), within pi/2, at the pi/2 edge, or away
            auto bot_face = [](uint32_t m) {
                const Vec4 h = ee_ref<Vec4>(0x0013f3d0), q = ee_ref<Vec4>(m + 0x10);
                const float to = std::atan2(h.y - q.y, h.x - q.x);
                const int k = ri(0, 3);
                const float off = k == 0 ? rf(-0.3f, 0.3f) : k == 1 ? rf(-1.5f, 1.5f) : k == 2 ? (coin() ? 1.0f : -1.0f) * rf(1.52f, 1.62f) : rf(1.7f, 4.5f);
                float y = to + off;
                while (y > 3.14159f) y -= 6.28318f;
                while (y < -3.14159f) y += 6.28318f;
                wf(m + 0x48, y);
            };
            // the hero: at the reach's edge (3), within it, at the sight's edge or the alerted sight's (twice it), far, or
            // about the area's centre
            auto bot_hero = [hero_at, hero_put](uint32_t m, int32_t area, const Vec4& centre) {
                const float s = std::max(1.0f, std::min(ee_read<float>(ee_read<uint32_t>(m + 0x78) + 0x3e8), 40.0f));
                const int k = ri(0, 7);
                if (k >= 6) { hero_at(m, 2.85f, 3.2f); return; }  // (about the reach's 3, more often)
                if (k == 5 && area >= 0) {
                    Vec4 q = centre;
                    const float r = rf(0, 12), a = rf(-3.1f, 3.1f);
                    q.x += r * std::cos(a); q.y += r * std::sin(a);
                    hero_put(q);
                    return;
                }
                switch (k) {
                case 0: hero_at(m, 2.6f, 3.4f); break;
                case 1: hero_at(m, 0, 3); break;
                case 2: hero_at(m, s * 0.9f, s * 1.1f); break;
                case 3: hero_at(m, s * 1.9f, s * 2.1f); break;
                default: hero_at(m, 3, 60); break;
                }
            };
            // a hit's source (+0x20): none, the hero's moby, the moby at 0x1403e0, a moby of class 0x131 or 0xb0 (the halved
            // push), or the bot itself
            auto hit_source = [](uint32_t m, uint32_t e) {
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), last = ee_read<uint32_t>(OV(0x0015ffe0));
                const int n = std::max(1, static_cast<int>((last - first) / 0x100u));
                switch (ri(0, 4)) {
                case 0: w32(e + 0x20, 0); break;
                case 1: w32(e + 0x20, ee_read<uint32_t>(0x001413d0)); break;
                case 2: w32(e + 0x20, ee_read<uint32_t>(0x001403e0)); break;
                case 3: { const uint32_t o = first + u(ri(0, n - 1)) * 0x100u; w32(e + 0x20, o); if (o != m) w16(o + 0xa6, coin() ? 0x131u : 0xb0u); break; }
                default: w32(e + 0x20, m); break;
                }
            };
            auto bot_prep = [walker_prep, weapon_hit, node_flag, hit_source, some_path, path_at, area_centre, bot_frame, bot_face, bot_hero, rate50](uint32_t m, int st) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), rec = pv + 0x160;
                rate50();
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class563_update_l4(m); }
                if (ri(0, 2) != 0) walker_prep(Walker{m, rec});
                w8(m + 0x20, u(st >= 12 ? ri(12, 40) : st));
                // alerted (+0x38, the timer +0x3c6 read before its tick), stunned (+0x3c4, read after it), the action timer
                if (ri(0, 3) == 0) w32(pv + 0x38, 1);
                w16(pv + 0x3c6, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                w16(pv + 0x3c4, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                w16(pv + 0x3c2, ri(0, 2) == 0 ? u(ri(1, 3)) : 0u);
                if (coin()) w8(pv + 0x3c0, u(ri(0, 1)));
                if (ri(0, 3) == 0) node_flag(m, pv + 0x120, u(ri(0, 1)));
                if (coin()) wf(pv + 0x3e8, rf(2, 30));
                if (coin()) wf(pv + 0x3e4, coin() ? 0.0f : rf(0, 0.3f));
                if (coin()) wf(pv + 0x3e0, coin() ? 0.0f : rf(0, 1));
                if (coin()) wf(pv + 0x3c8, rf(-0.6f, 0.6f));
                if (coin()) wf(pv + 0x3d4, rf(-3.1f, 3.1f));
                {   // the lunge's speed: none, below, about or above its cap (a ninth of 0x15ed60)
                    const float cap = ee_read<float>(0x0015ed60) * 0.11111111f;
                    if (coin()) wf(pv + 0x3d8, ri(0, 3) == 0 ? (coin() ? 0.0f : -rf(0, 0.1f)) : rf(0, 2 * cap));
                }
                // its area: as placed, none, or a level path's polygon (the bot about its centre or not)
                int32_t area = ee_read<int32_t>(pv + 0x3dc);
                if (ri(0, 3) == 0) area = -1;
                else if (coin()) area = some_path(3);
                w32(pv + 0x3dc, u(area));
                Vec4 centre = ee_ref<Vec4>(m + 0x10);
                if (area >= 0 && area < 1000 && path_at(area) != 0) {
                    centre = area_centre(area);
                    if (coin()) { Vec4 q = centre; q.x += rf(-4, 4); q.y += rf(-4, 4); q.z += rf(0, 1); ee_ref<Vec4>(m + 0x10) = q; }
                } else {
                    area = -1;
                }
                // its home within 2 or not
                if (coin()) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    const float r = coin() ? rf(0, 2.2f) : rf(1.8f, 12), a = rf(-3.1f, 3.1f);
                    q.x += r * std::cos(a); q.y += r * std::sin(a);
                    ee_ref<Vec4>(pv + 0x3b0) = q;
                }
                if (coin()) w8(m + 0xbc, R() & 0x1f);
                if (ri(0, 11) == 0) w8(m + 0xbc, 6);  // (the value the fall's test skips on)
                // the animations the class plays (the state's own in most cases), ended or not, at a frame's edge
                static const uint8_t sets[] = {0, 1, 2, 3, 4, 5, 6, 0xa, 0xb, 0xc, 0x10};
                uint8_t set = sets[ri(0, 10)];
                if (ri(0, 2) != 0) {
                    switch (st) {
                    case 2: set = 2; break;
                    case 3: set = coin() ? 0xb : 0xc; break;
                    case 8: set = 4; break;
                    case 9: set = coin() ? 5 : 0x10; break;
                    case 10: set = 0x10; break;
                    default: break;
                    }
                }
                w8(m + 0x53, set);
                w8(m + 0x52, ri(0, 3) == 0 ? sets[ri(0, 10)] : set);
                w8(m + 0x70, coin() ? 2u : 0u);
                if (coin()) wf(m + 0x58, rf(0.5f, 1.5f));
                if (ri(0, 2) != 0) {
                    static const float edges[] = {15.5f, 16.0f, 16.5f, 26.5f, 27.0f, 27.5f, 27.9375f, 28.0f, 28.5f, 36.0f, 36.0625f};
                    bot_frame(m, coin() ? edges[ri(0, 10)] : rf(0, 40));
                }
                if (st == 7 && coin()) { w8(rec + 0xb7, 0); wf(rec + 0xc0, 0.0f); }  // (the walker standing)
                if (coin()) {  // its floor band near the camera point (L4:0x166f40)
                    w8(m + 0x31, 1);
                    Vec4 c = ee_ref<Vec4>(m + 0x10);
                    const float r = coin() ? rf(0, 27) : rf(25, 40), a = rf(-3.1f, 3.1f);
                    c.x += r * std::cos(a); c.y += r * std::sin(a);
                    ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c;
                }
                if (ri(0, 7) == 0) wf(m + 0x18, coin() ? rf(-5.0f, 2.5f) : rf(1.3f, 2.7f));  // (low: off the level with no floor under it; about the fall's 2)
                else if (st == 11 && coin()) wf(m + 0x18, rf(0.5f, 1.5f));  // (the dying fall's end at 1)
                if (coin()) wf(pv + 0x20, coin() ? rf(0.1f, 1) : 2.0f);
                bot_hero(m, area, centre);
                bot_face(m);
                weapon_hit(m);
                if (ee_read<uint8_t>(m + 0xa4) != 0xff && coin()) hit_source(m, OV(0x00178100) + (ee_read<uint8_t>(m + 0xa4) << 6));
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
            };
            if (bots.empty()) std::printf("eud_woodcut                L4:0x002d16b8  no class 563 moby on this level\n");
            else {
                // every state (12: a state past the table, 12 .. 40)
                for (int st = 0; st <= 12; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_woodcut_st%d", st);
                    l4check(name, 0x002d16b8, 60, Ret::None, [level, bots, pick, bot_prep, trial, st] {
                        return Args{{trial(eud_class563_update_l4, [&] { level(); const uint32_t m = pick(bots); bot_prep(m, st); return m; })}, {}};
                    }, [](A a) { eud_class563_update_l4(a.i[0]); return uint64_t{0}; });
                }
                // the hit: every case a hit (the mask's 0x210000 or 0x800000 bits), its reaction bytes, damage (killing or
                // not), source and marked direction varied; mid-swing (animation 4 at its frames 16 .. 27) or not
                l4check("eud_woodcut_hit", 0x002d16b8, 240, Ret::None, [level, bots, pick, bot_prep, trial, hit_source, bot_frame] {
                    return Args{{trial(eud_class563_update_l4, [&] {
                        level();
                        const uint32_t m = pick(bots), pv = ee_read<uint32_t>(m + 0x78);
                        static const int sts[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
                        bot_prep(m, sts[ri(0, 9)]);
                        w16(pv + 0x3c4, ri(0, 5) == 0 ? u(ri(1, 2)) : 0u);
                        if (coin()) {
                            w8(m + 0x52, 4); w8(m + 0x53, 4);
                            bot_frame(m, coin() ? rf(15, 28) : (coin() ? 16.0f : 27.0f));
                        }
                        if (coin()) wf(pv + 0x20, coin() ? rf(0.1f, 1) : 2.0f);
                        for (uint32_t o = 0x24; o < 0x30; ++o) if (o != 0x2e && coin()) w8(pv + o, R() & 0xff);
                        const uint32_t idx = u(ri(0, 15)), e = OV(0x00178100) + (idx << 6);
                        w8(m + 0xa4, idx);
                        w32(e + 0x34, m);
                        w32(e + 0x24, (coin() ? 0x210000u : 0x800000u) | (R() & 0xffff));
                        wfrand(e, 4, -1, 1);
                        wfrand(e + 0x10, 3, -1, 1);
                        w32(e + 0x1c, coin() ? 0x45afdf66u : 0u);  // (vec_yaw_marked's marker or not)
                        w8(e + 0x28, u(ri(0, 11))); w8(e + 0x29, u(ri(0, 4))); w16(e + 0x2a, coin() ? 0x47u : u(ri(0, 0x60)));
                        hit_source(m, e);
                        wf(e + 0x2c, ri(0, 7) == 0 ? 0.0f : rf(0.1f, 3));
                        seed_rand();
                        return m;
                    })}, {}};
                }, [](A a) { eud_class563_update_l4(a.i[0]); return uint64_t{0}; });
                // the swing: state 8, animation 4 at its frames 16 .. 27 (and their edges), the hero within the arms' reach
                l4check("eud_woodcut_swing", 0x002d16b8, 150, Ret::None, [level, bots, pick, bot_prep, trial, bot_frame, hero_at] {
                    return Args{{trial(eud_class563_update_l4, [&] {
                        level();
                        const uint32_t m = pick(bots);
                        bot_prep(m, 8);
                        w8(m + 0xa4, 0xff);
                        w8(m + 0x52, 4); w8(m + 0x53, 4);
                        w8(m + 0x70, coin() ? 2u : 0u);
                        bot_frame(m, ri(0, 3) != 0 ? rf(16.0625f, 26.9375f) : (coin() ? 16.0f : 27.0f));
                        hero_at(m, 0, coin() ? 2.5f : 6.0f);
                        seed_rand();
                        return m;
                    })}, {}};
                }, [](A a) { eud_class563_update_l4(a.i[0]); return uint64_t{0}; });
                // the recoil's end and the lunge: state 9, its area a level path with the bot about its centre (a way found)
                // or none, the look asked (+0xbc bit 4) or the lunge on (animation 16 before or after its frame 28, the
                // speed about its cap)
                l4check("eud_woodcut_lunge", 0x002d16b8, 200, Ret::None, [level, bots, pick, bot_prep, trial, bot_frame, hero_at, some_path, area_centre] {
                    return Args{{trial(eud_class563_update_l4, [&] {
                        level();
                        const uint32_t m = pick(bots), pv = ee_read<uint32_t>(m + 0x78);
                        bot_prep(m, 9);
                        w8(m + 0xa4, 0xff);
                        const int32_t p = some_path(3);
                        if (p >= 0 && ri(0, 3) != 0) {
                            w32(pv + 0x3dc, u(p));
                            Vec4 c = area_centre(p);
                            c.x += rf(-2, 2); c.y += rf(-2, 2); c.z += rf(0, 1);
                            ee_ref<Vec4>(m + 0x10) = c;
                        } else if (coin()) {
                            w32(pv + 0x3dc, 0xffffffffu);
                        }
                        w8(m + 0xbc, (ee_read<uint8_t>(m + 0xbc) & 0xfbu) | (coin() ? 4u : 0u));
                        const uint8_t set = coin() ? 5 : 0x10;
                        w8(m + 0x53, set);
                        w8(m + 0x52, ri(0, 3) == 0 ? (set == 5 ? 0x10 : 5) : set);
                        w8(m + 0x70, coin() ? 2u : 0u);
                        bot_frame(m, coin() ? rf(26, 30) : rf(0, 40));
                        hero_at(m, 1, 10);
                        seed_rand();
                        return m;
                    })}, {}};
                }, [](A a) { eud_class563_update_l4(a.i[0]); return uint64_t{0}; });
                // the area's edges: the bot by one of its polygon's corners (inside or out), heading out or in; the hero
                // inside or outside it; noticed already or not (the states that read the area)
                l4check("eud_woodcut_area", 0x002d16b8, 240, Ret::None, [level, bots, pick, bot_prep, trial, some_path, path_at, area_centre, hero_put] {
                    return Args{{trial(eud_class563_update_l4, [&] {
                        level();
                        const uint32_t m = pick(bots), pv = ee_read<uint32_t>(m + 0x78);
                        static const int sts[] = {1, 2, 3, 4, 5, 6, 10};
                        bot_prep(m, sts[ri(0, 6)]);
                        w8(m + 0xa4, 0xff);
                        const int32_t p = some_path(3);
                        if (p < 0) return m;
                        w32(pv + 0x3dc, u(p));
                        const uint32_t path = path_at(p);
                        const int32_t n = ee_read<int32_t>(path);
                        const Vec4 c = area_centre(p);
                        auto by_corner = [&](float lo, float hi) {
                            const Vec4 k = ee_ref<Vec4>(path + 0x10 + u(ri(0, n - 1)) * 16);
                            const float t = rf(lo, hi);
                            Vec4 q = c;
                            q.x = c.x + (k.x - c.x) * t; q.y = c.y + (k.y - c.y) * t; q.z = c.z + rf(0, 1);
                            return q;
                        };
                        const Vec4 q = by_corner(0.6f, 1.15f);
                        ee_ref<Vec4>(m + 0x10) = q;
                        const float out = std::atan2(q.y - c.y, q.x - c.x);
                        wf(m + 0x48, ri(0, 2) == 0 ? rf(-3.1f, 3.1f) : (coin() ? out : out + 3.14159f) + rf(-0.5f, 0.5f));
                        if (ri(0, 2) == 0) w16(pv + 0x3c2, 0);
                        if (coin()) w8(m + 0xbc, ee_read<uint8_t>(m + 0xbc) | 1u);
                        hero_put(by_corner(0.0f, 1.6f));
                        seed_rand();
                        return m;
                    })}, {}};
                }, [](A a) { eud_class563_update_l4(a.i[0]); return uint64_t{0}; });

                // ---- the helpers
                l4check("eud_woodcut_leg_limits", 0x00292370, 100, Ret::None, [level, bots, pick] {
                    level();
                    const uint32_t m = pick(bots), rec = ee_read<uint32_t>(m + 0x78) + 0x160;
                    wfrand(rec + 0x10c, 2, -2, 2);
                    wfrand(rec + 0x1bc, 2, -2, 2);
                    return Args{{m, rec}, {}};  // (the original's a0, the moby, unused)
                }, [](A a) { woodcut_leg_limits_l4(a.i[1]); return uint64_t{0}; });
                // the walker's setup: its gaits as its state 0 sets them, a slot or two on a copy of a gait (its animation
                // set, stride and foot ranges varied: each span on both sides of its wrap), the legs' own rotations set or not
                l4check("eud_walker_setup", 0x002923b8, 200, Ret::None, [level, bots, pick, rate50, node_flag] {
                    level();
                    const uint32_t m = pick(bots), pv = ee_read<uint32_t>(m + 0x78), rec = pv + 0x160;
                    if (coin() || ee_read<uint32_t>(rec + 0x70) == 0) { w8(m + 0x20, 0); eud_class563_update_l4(m); }
                    for (int k = ri(0, 2); k > 0; --k) {
                        const uint32_t g = ee_read<uint32_t>(rec + 0x70 + u(ri(0, 12)) * 4), cp = T3 + 0x100u * u(k);
                        for (uint32_t o = 0; o < 0x50; o += 4) w32(cp + o, ee_read<uint32_t>(g + o));
                        static const uint32_t sets[] = {0, 1, 2, 3, 4, 5, 6, 0xa, 0xb, 0xc, 0x10};
                        if (coin()) w32(cp, sets[ri(0, 10)]);
                        if (coin()) wf(cp + 4, rf(0.5f, 4));
                        for (uint32_t o = 0x28; o < 0x50; o += 4) if ((o < 0x38 || o >= 0x40) && coin()) wf(cp + o, rf(0, 40));
                        w32(rec + 0x70 + u(ri(0, 12)) * 4, cp);
                    }
                    for (uint32_t leg = rec; leg < rec + 0x160; leg += 0xb0) {
                        if (coin()) node_flag(m, leg + 0x120, u(ri(0, 1)));
                        if (coin()) node_flag(m, leg + 0x160, u(ri(0, 1)));
                    }
                    if (coin()) wfrand(rec + 0xb8, 12, -1, 1);
                    rate50();
                    seed_rand();
                    return Args{{m, rec}, {}};
                }, [](A a) { walker_setup_l4(a.i[0], a.i[1]); return uint64_t{0}; });
                l4check("eud_woodcut_walker_init", 0x002d1418, 150, Ret::None, [level, bots, pick, rate50, node_flag] {
                    level();
                    const uint32_t m = pick(bots), pv = ee_read<uint32_t>(m + 0x78);
                    if (coin()) { w8(m + 0x20, 0); eud_class563_update_l4(m); }
                    for (uint32_t leg = pv + 0x160; leg < pv + 0x2c0; leg += 0xb0) {
                        if (coin()) node_flag(m, leg + 0x120, u(ri(0, 1)));
                        if (coin()) node_flag(m, leg + 0x160, u(ri(0, 1)));
                    }
                    if (coin()) wfrand(pv + 0x160 + 0xb8, 12, -1, 1);
                    rate50();
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { woodcut_walker_init_l4(a.i[0]); return uint64_t{0}; });
                l4check("eud_woodcut_explode", 0x002d1608, 100, Ret::None, [level, bots, pick, rate50] {
                    level();
                    const uint32_t m = pick(bots);
                    rate50();
                    seed_rand();
                    return Args{{m}, {}};
                }, [](A a) { woodcut_explode_l4(a.i[0]); return uint64_t{0}; });
            }
            g_ignored_words = before_bot_ignored;

            // Eudora's copy of the debris chunk's spawn (L4:0x2e4fe0, eud_chunk_spawn_l4): a0 src, a1 velocity, a2 position, a3
            // rotation, t0 class, t1 tag, f12 / f13 the scale's range. Its spin range, the level's own L4:0x161d78 / 0x161d7c
            // (not the engine's pair the shared chunk_spawn reads, Veldin 0x161448 = L4:0x161418), varied in most cases
            l4check("eud_chunk_spawn", 0x002e4fe0, 200, Ret::Int32, [level, bots, pick, rate50] {
                level();
                const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8)), last = ee_read<uint32_t>(OV(0x0015ffe0));
                const int n = std::max(1, static_cast<int>((last - first) / 0x100u));
                const uint32_t src = !bots.empty() && coin() ? pick(bots) : first + u(ri(0, n - 1)) * 0x100u;
                wfrand(T, 4, -0.5f, 0.5f);
                Vec4 q = ee_ref<Vec4>(src + 0x10);
                q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-1, 3);
                ee_ref<Vec4>(T + 0x10) = q;
                wfrand(T + 0x20, 4, -3.1f, 3.1f);
                if (ri(0, 3) != 0) { wf(L4_ADDR(0x00161d78), rf(-10, 10)); wf(L4_ADDR(0x00161d7c), rf(-10, 10)); }
                static const uint32_t cls[] = {0x5ec, 0x5ec, 0x6cb, 0x6cc, 0x6cd};
                const float lo = rf(0.1f, 1);
                rate50();
                seed_rand();
                return Args{{src, T, T + 0x10, T + 0x20, cls[ri(0, 4)], u(ri(0, 3))}, {lo, coin() ? lo : rf(0.1f, 1.5f)}};
            }, [](A a) { return r32(eud_chunk_spawn_l4(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0], a.f[1])); });
        }
// Paste at the end of batch_111.cpp's P4 `if (g_oracle_level == 4) { ... }` block, just BEFORE its closing
// `g_extra_noop_hooks.clear();` (it uses level, mobys_with, pick, rate50, some_path, path_at, hero_at, trial, bipeds, info).
// Needs `#include <game/lib/libcdvd.h>` at the top of batch_111.cpp (g_host_clock_fixed, cd_read_clock, clock_to_local,
// cd_read, cd_sync, cd_get_error, cd_break: the Suck Cannon's state 2 as batch 139 runs it).

        // ---- P4.3 (misc): classes 481 (the pump's float), 584 (the ride platform), 617 (the pump's path mover), 642 (the
        // wind-blown leaf), 1101 / 1102 / 1531 / 1532 (the gates; gate_speed_l4) and 1120 (the Suck Cannon;
        // cannon_sparkles_l4): eudora_pump_float.cpp, eudora_ride_platform.cpp, eudora_pump_path.cpp, eudora_windblown.cpp,
        // eudora_gates.cpp, eudora_suck_cannon.cpp
        {
            const std::vector<uint32_t> p43_noops = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80, 0x0023fe20};
            g_extra_noop_hooks = p43_noops;
            level();
            const uint32_t mtab = ee_read<uint32_t>(OV(0x0015ffd8));  // (the moby table: 0x100 bytes a moby)
            auto index_of = [mtab](uint32_t m) { return (m - mtab) >> 8; };
            std::vector<uint32_t> live;  // (the live mobys with pvars: other mobys a pvars index may name)
            for (uint32_t m = mtab; m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x78) != 0) live.push_back(m);
            const std::vector<uint32_t> floats = mobys_with(0x002cdda0), platforms = mobys_with(0x002d3580), movers = mobys_with(0x002d6f68);
            const std::vector<uint32_t> blown = mobys_with(0x002d7e90), gates = mobys_with(0x002e17d8), cannons = mobys_with(0x002e1a10);
            const std::vector<uint32_t> pumps = mobys_with(0x002bfd30), pads = mobys_with(0x002d4ae0), switches = mobys_with(0x002e3078);
            if (info)
                std::printf("P43 481 %zu 584 %zu 617 %zu 642 %zu 1101 %zu 1120 %zu pumps %zu pads %zu 1190 %zu live %zu\n", floats.size(),
                            platforms.size(), movers.size(), blown.size(), gates.size(), cannons.size(), pumps.size(), pads.size(), switches.size(),
                            live.size());

            // -- class 481, the float the pump lowers: states 0 (set up), 1 (floats), other (only the ride frame). Its pump: none (-1:
            // lowered 15), a pump at its work's ends or between, another class's moby (not lowered); its height, joints' angles, the
            // level's turn rate, the animation's frames; no pvars at all in some cases (taken off; only outside code makes it)
            auto float_prep = [rate50, pumps, live, index_of, pick, mtab](uint32_t m, int st) {
                rate50();
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class481_update_l4(m); }
                w8(m + 0x20, u(st == 2 ? ri(2, 0xfd) : st));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                switch (ri(0, 3)) {
                case 0: w32(pv + 0x230, 0xffffffffu); break;
                case 1: if (!pumps.empty()) w32(pv + 0x230, index_of(pick(pumps))); break;
                case 2: w32(pv + 0x230, index_of(pick(live))); break;
                default: break;  // (as placed)
                }
                if (const int32_t p = ee_read<int32_t>(pv + 0x230); p != -1) {
                    const uint32_t pm = mtab + (u(p) << 8);
                    if (ee_read<int16_t>(pm + 0xa6) == 0x118 && ee_read<uint32_t>(pm + 0x78) != 0) {
                        const uint32_t pp = ee_read<uint32_t>(pm + 0x78);
                        switch (ri(0, 2)) {
                        case 0: wf(pp, 0.0f); break;
                        case 1: wf(pp, 1.0f); break;
                        default: wf(pp, rf(0, 1)); break;
                        }
                    }
                }
                if (coin()) wf(pv + 0x220, ee_read<float>(m + 0x18) + rf(-2, 17));
                for (uint32_t k = 0; k < 3; ++k) if (coin()) wf(pv + 0x224 + k * 4, rf(-3.14f, 3.14f));
                if (coin()) wf(L4_ADDR(0x0016195c), coin() ? 0.0f : rf(-360, 360));
                if (coin()) { w8(m + 0x51, u(ri(0, 48))); w8(m + 0x50, u(ri(0, 48))); wf(m + 0x54, rf(0, 1)); }
                if (coin()) wfrand(m + 0x40, 3, -3.1f, 3.1f);
                if (ri(0, 11) == 0) w32(m + 0x78, 0);
                seed_rand();
            };
            if (floats.empty()) std::printf("eud_pump_float             L4:0x002cdda0  no class 481 moby on this level\n");
            else
                for (int st = 0; st <= 2; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_pump_float_st%d", st);
                    l4check(name, 0x002cdda0, 100, Ret::None, [level, floats, pick, float_prep, trial, st] {
                        return Args{{trial(eud_class481_update_l4, [&] { level(); const uint32_t m = pick(floats); float_prep(m, st); return m; })}, {}};
                    }, [](A a) { eud_class481_update_l4(a.i[0]); return uint64_t{0}; });
                }

            // -- class 584, the ride platform: state 0 (its set-up) and any other; its place of the frame before moved or not, its
            // angles, its hit record's words
            auto platform_prep = [rate50](uint32_t m, int st) {
                rate50();
                w8(m + 0x20, u(st == 0 ? 0 : ri(1, 0xfd)));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (ri(0, 3) != 0) {
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    if (coin()) { q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.z += rf(-0.5f, 0.5f); }
                    ee_ref<Vec4>(pv + 0xa0) = q;
                }
                if (coin()) wfrand(m + 0x40, 3, -3.1f, 3.1f);
                if (coin()) { w32(pv + 0x20, R()); w16(pv + 0x24, R() & 0xffff); w8(pv + 0x28, R() & 0xff); w16(pv + 0x3e, R() & 0xffff); }
                seed_rand();
            };
            if (platforms.empty()) std::printf("eud_ride_platform          L4:0x002d3580  no class 584 moby on this level\n");
            else
                for (int st = 0; st <= 1; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_ride_platform_st%d", st);
                    l4check(name, 0x002d3580, 100, Ret::None, [level, platforms, pick, platform_prep, st] {
                        level();
                        const uint32_t m = pick(platforms);
                        platform_prep(m, st);
                        return Args{{m}, {}};
                    }, [](A a) { eud_class584_update_l4(a.i[0]); return uint64_t{0}; });
                }

            // -- class 617, the path mover (no states): its driver's work at 0 / 1 (the path's last / first point exactly), 0.5 (a point
            // exactly on an odd count), on a point by k / (n - 1) (exact or a hair off), between; its path, driver (a pump) and carried
            // moby as placed or others
            auto mover_prep = [rate50, pumps, live, index_of, pick, some_path, path_at, mtab](uint32_t m) {
                rate50();
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                if (ri(0, 3) == 0) { const int32_t p = some_path(2); if (p >= 0) w32(pv + 8, u(p)); }
                if (ri(0, 3) == 0 && !pumps.empty()) w32(pv, index_of(pick(pumps)));
                if (ri(0, 3) == 0) w32(pv + 4, index_of(pick(live)));
                const uint32_t dpv = ee_read<uint32_t>(mtab + (ee_read<uint32_t>(pv) << 8) + 0x78);
                const int32_t n = ee_read<int32_t>(path_at(ee_read<int32_t>(pv + 8)));
                if (dpv != 0) {
                    switch (ri(0, 4)) {
                    case 0: wf(dpv, 0.0f); break;
                    case 1: wf(dpv, 1.0f); break;
                    case 2: wf(dpv, 0.5f); break;
                    case 3: if (n > 1) wf(dpv, 1.0f - static_cast<float>(ri(0, n - 1)) / static_cast<float>(n - 1)); break;
                    default: wf(dpv, rf(0, 1)); break;
                    }
                }
                seed_rand();
            };
            if (movers.empty()) std::printf("eud_pump_path              L4:0x002d6f68  no class 617 moby on this level\n");
            else
                l4check("eud_pump_path", 0x002d6f68, 200, Ret::None, [level, movers, pick, mover_prep, trial] {
                    return Args{{trial(eud_class617_update_l4, [&] { level(); const uint32_t m = pick(movers); mover_prep(m); return m; })}, {}};
                }, [](A a) { eud_class617_update_l4(a.i[0]); return uint64_t{0}; });

            // -- class 642, the wind-blown leaf: states 0, 1 (waiting: the camera point within 32, at the edge or beyond; the countdown
            // ending or not), 2 (blown: the wind's heading / speed, the velocity faster than the wind or not, the sink both sides of
            // its goal, the swirl and the move sent into the floor (the two segment tests hit), the countdown at the grow-in's edge
            // (its first 10 frames), the shrink's edge (its last 100), ending), other (nothing). No placed moby: on the robots
            // (class 217's mobys, pvars large enough)
            const std::vector<uint32_t> blown_subj = !blown.empty() ? blown : bipeds;
            if (blown.empty()) std::printf("eud_windblown              L4:0x002d7e90  no class 642 moby on this level: run on the robots\n");
            auto blown_prep = [rate50](uint32_t m, int st) {
                rate50();
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class642_update_l4(m); }
                w8(m + 0x20, u(st == 3 ? ri(3, 0xfd) : st));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                {
                    Vec4 c = ee_ref<Vec4>(m + 0x10);
                    const float d = ri(0, 2) == 0 ? rf(0, 31) : (coin() ? rf(31.5f, 32.5f) : rf(33, 80)), a = rf(-3.1f, 3.1f);
                    c.x += d * std::cos(a); c.y += d * std::sin(a); c.z += rf(-3, 6);
                    ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c;
                }
                if (coin()) wf(L4_ADDR(0x001cb944), rf(-3.1f, 3.1f));
                if (coin()) wf(L4_ADDR(0x001cb954), ri(0, 4) == 0 ? 0.0f : rf(0.01f, 0.6f));
                if (coin()) wf(m + 0x2c, rf(0.2f, 2));
                if (coin()) w32(pv + 0x40, u(coin() ? ri(0x2d0, 0x4b0) : ri(0x78, 0x960)));
                const int32_t total = ee_read<int32_t>(pv + 0x40);
                switch (ri(0, 5)) {
                case 0: w32(pv + 0x44, u(ri(0, 2))); break;
                case 1: w32(pv + 0x44, u(total - ri(0, 14))); break;
                case 2: w32(pv + 0x44, u(ri(90, 110))); break;
                case 3: w32(pv + 0x44, u(ri(1, std::max(1, std::min(total, 0x960))))); break;
                default: break;
                }
                if (coin()) wfrand(pv, 3, -0.6f, 0.6f);
                if (coin()) wf(pv + 8, coin() ? rf(-3, -0.5f) : rf(-0.1f, 0.1f));
                if (coin()) wfrand(pv + 0x10, 3, -0.4f, 0.4f);
                if (ri(0, 3) == 0) wf(pv + 0x18, -rf(0.2f, 1.0f));
                if (coin()) wfrand(pv + 0x20, 3, -0.2f, 0.2f);
                if (coin()) { Vec4 h = ee_ref<Vec4>(m + 0x10); h.x += rf(-2, 2); h.y += rf(-2, 2); ee_ref<Vec4>(pv + 0x30) = h; }
                if (coin()) wf(pv + 0x4c, rf(0.2f, 2));
                if (ri(0, 3) == 0) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-0.3f, 0.6f));
                seed_rand();
            };
            {
                // (left out of these checks only, as in the saw's: the collision's pushed w lane and the segment record +0x50 .. +0x7c,
                // the VU0 leftovers segment_collide's port leaves: COMMON's known differences, harmless)
                const std::vector<uint32_t> before_blown_ignored = g_ignored_words;
                g_ignored_words.push_back(OV(0x00173e7c));
                for (uint32_t a = OV(0x00173e90); a < OV(0x00173ec0); a += 4) g_ignored_words.push_back(a);
                for (int st = 0; st <= 3; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_windblown_st%d", st);
                    l4check(name, 0x002d7e90, 100, Ret::None, [level, blown_subj, pick, blown_prep, trial, st] {
                        return Args{{trial(eud_class642_update_l4, [&] { level(); const uint32_t m = pick(blown_subj); blown_prep(m, st); return m; })}, {}};
                    }, [](A a) { eud_class642_update_l4(a.i[0]); return uint64_t{0}; });
                }
                g_ignored_words = before_blown_ignored;
            }

            // -- classes 1101 / 1102 / 1531 / 1532, the gates: states 0, 1 (shut: opened before by its flag byte or its level bit, or
            // not; its trigger none, a pad in state 4 or another, a class 1190 in state 2 or another, a moby of another class),
            // 2 (moving: its class's speed, the timer idle / expiring / running, the end sound of 1101), other. A trigger of a class
            // the level does not place is a live moby given that class id. No placed gate: on the robots, given the gates' classes
            auto gate_prep = [rate50, pads, switches, live, index_of, pick](uint32_t m, int st, bool own) {
                rate50();
                static const uint32_t classes[] = {0x44d, 0x44e, 0x5fb, 0x5fc};
                if (!own || coin()) w16(m + 0xa6, classes[ri(0, 3)]);
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class1101_update_l4(m); }
                w8(m + 0x20, u(st == 3 ? ri(3, 0xfd) : st));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                const uint32_t idx = ee_read<uint16_t>(m + 0xb2);
                const int32_t sidx = static_cast<int16_t>(idx);
                const uint32_t bit = 1u << (idx & 31);
                const uint32_t byte = L4_ADDR(0x001bb3b0) + u(sidx) + 0x454;
                const uint32_t word = 0x0014c190 + (u(sidx >> 5) << 2) + (ee_read<uint32_t>(0x0015ed84) << 8);
                const uint32_t bits = L4_ADDR(0x001ba650) + (u(sidx >> 5) << 2);
                w8(byte, ri(0, 3) == 0 ? u(ri(1, 255)) : 0u);  // (an earlier visit: its byte)
                switch (ri(0, 3)) {  // (or its bit of the level's words; the other bits random or as they are)
                case 0: w32(word, ee_read<uint32_t>(word) | bit); break;
                case 1: w32(word, R() & ~bit); break;
                default: w32(word, ee_read<uint32_t>(word) & ~bit); break;
                }
                if (coin()) w32(bits, R());
                uint32_t t = 0;
                switch (ri(0, 4)) {
                case 0: w32(pv, 0xffffffffu); break;
                case 1: t = !pads.empty() ? pick(pads) : pick(live); if (t == m) t = 0; else { w16(t + 0xa6, 0x267); w8(t + 0x20, coin() ? 4u : u(ri(0, 6))); } break;
                case 2: t = !switches.empty() ? pick(switches) : pick(live); if (t == m) t = 0; else { w16(t + 0xa6, 0x4a6); w8(t + 0x20, coin() ? 2u : u(ri(0, 4))); } break;
                case 3: t = pick(live); if (t == m) t = 0; break;
                default: break;  // (as placed)
                }
                if (t != 0) w32(pv, index_of(t));
                w32(pv + 8, coin() ? u(ri(0, 3)) : u(ri(4, 60)));  // (the timer: 0 idle, 1 expiring, running)
                if (coin()) wf(m + 0x18, ee_read<float>(m + 0x18) + rf(-5, 5));
                seed_rand();
            };
            const bool own_gates = !gates.empty();
            const std::vector<uint32_t> gate_subj = own_gates ? gates : bipeds;
            if (!own_gates) std::printf("eud_gate                   L4:0x002e17d8  no class 1101 family moby on this level: run on the robots\n");
            for (int st = 0; st <= 3; ++st) {
                char name[32];
                std::snprintf(name, sizeof name, "eud_gate_st%d", st);
                l4check(name, 0x002e17d8, 100, Ret::None, [level, gate_subj, pick, gate_prep, trial, own_gates, st] {
                    return Args{{trial(eud_class1101_update_l4, [&] { level(); const uint32_t m = pick(gate_subj); gate_prep(m, st, own_gates); return m; })}, {}};
                }, [](A a) { eud_class1101_update_l4(a.i[0]); return uint64_t{0}; });
            }
            l4check("eud_gate_speed", 0x002e1768, 200, Ret::Float, [level, gate_subj, pick] {
                level();
                const uint32_t m = pick(gate_subj);
                static const uint32_t classes[] = {0x44d, 0x44e, 0x5fb, 0x5fc, 0x44c, 0x5fa, 0x5fd, 0x5fe};
                w16(m + 0xa6, ri(0, 5) == 0 ? (R() & 0xffffu) : classes[ri(0, 7)]);
                return Args{{m}, {}};
            }, [](A a) { return rf32(gate_speed_l4(a.i[0])); });

            // -- class 1120, the Suck Cannon: states 0 (the hero has it: 0x13d4f1, a revisit: taken off; or not), 1 (the sparkles; the
            // hero beyond 3, or within 3 with the hero's +0x22a8 at 0: cutscene_start's early exit, hero_lost, on both sides -- the
            // scene itself is not started: its stream reads end in 989snd's callback the oracle's SIF does not run, see batch 139),
            // 2 (the scene playing (game mode 2: waits) or over: the help text by 0x13d4c9, item_give, the checkpoint at the moby of
            // pvars +4 or the debug path for -1, the level change request run for real on both sides with batch 139's host clock /
            // disc hooks), 3 (gone), other. The game mode, the flags +0x34, the sparkles' fields at their edges, the camera point
            auto cannon_prep = [rate50, hero_at](uint32_t m, int st) {
                rate50();
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class1120_update_l4(m); }
                w8(m + 0x20, u(st == 4 ? ri(4, 0xfd) : st));
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                w32(L4_ADDR(0x0015f5c4), ri(0, 3) == 0 ? 2u : 0u);
                w16(m + 0x34, (ee_read<uint16_t>(m + 0x34) & ~0x41u) | (coin() ? 1u : 0u) | (coin() ? 0x40u : 0u));
                wf(m + 0x48, rf(-3.14f, 3.14f));
                w8(0x0013d4f1, ri(0, 2) == 0 ? 1u : 0u);
                w8(0x0013d4c9, coin() ? 1u : 0u);
                if (ri(0, 3) == 0) w32(pv + 4, 0xffffffffu);
                for (uint32_t k = 0; k < 0x10; k += 4) {
                    if (coin()) wf(pv + 0x10 + k, coin() ? rf(250, 256) : rf(-1, 4));
                    if (coin()) wf(pv + 0x20 + k, rf(-6, 6));
                    if (coin()) w32(pv + 0x30 + k, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 300)));
                    if (coin()) wf(pv + 0x40 + k, rf(1, 4));
                }
                { Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-20, 20); c.y += rf(-20, 20); c.z += rf(-3, 6); ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c; }
                if (st == 1 && coin()) { hero_at(m, 0, 3.1f); w32(P + 0x22a8, 0); }
                else if (st == 1) { hero_at(m, 3.2f, 10); w32(P + 0x22a8, coin() ? 0u : 1u); }
                else hero_at(m, 0, coin() ? 3.0f : 10.0f);
                if (st == 2) {  // (the dialog / stream slot idle, as batch 139's: the help message's stream)
                    const uint32_t S = 0x001516d0;
                    w16(S + 0x5a, 3);
                    w32(S + 0x50, (R() & 0x7ffffffeu) | 1u);
                    w16(S + 0x5c, 0);
                    w32(S + 0x1c, 0xffffffffu);
                    w16(S + 8, 0);
                }
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
            };
            if (cannons.empty()) std::printf("eud_suck_cannon            L4:0x002e1a10  no class 1120 moby on this level\n");
            else {
                for (int st = 0; st <= 4; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_suck_cannon_st%d", st);
                    const std::time_t saved_clock = g_host_clock_fixed;
                    if (st == 2) {  // (the level change request runs on both sides: batch 139's hooks, the console clock the host's)
                        g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80};
                        g_host_clock_fixed = 1759658400;
                        g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                                         {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                                         {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }},
                                         {0x00120c30, [](EeCpu& c) { c.set32(2, cd_sync(static_cast<int32_t>(c.r32(4)))); }},
                                         {0x00121630, [](EeCpu& c) { c.set32(2, cd_get_error()); }},
                                         {0x001216c8, [](EeCpu& c) { c.set32(2, cd_break()); }}};
                    }
                    l4check(name, 0x002e1a10, 100, Ret::None, [level, cannons, pick, cannon_prep, st] {
                        level();
                        const uint32_t m = pick(cannons);
                        cannon_prep(m, st);
                        return Args{{m}, {}};
                    }, [](A a) { eud_class1120_update_l4(a.i[0]); return uint64_t{0}; });
                    if (st == 2) {
                        g_extra_hooks.clear();
                        g_host_clock_fixed = saved_clock;
                        g_extra_noop_hooks = p43_noops;
                    }
                }
                l4check("eud_cannon_sparkles", 0x002e1d00, 200, Ret::None, [level, cannons, pick, cannon_prep] {
                    level();
                    const uint32_t m = pick(cannons);
                    cannon_prep(m, 3);  // (the sparkles' fields, the game mode, the camera point; the state is not read)
                    return Args{{m}, {}};
                }, [](A a) { cannon_sparkles_l4(a.i[0]); return uint64_t{0}; });
            }
        }
        // ---- P4.3: class 1343, the help-message controller (eudora_help.cpp); class 1516, the Woodcut Bot's wood chips
        // (eudora_wood_chips.cpp); class 1549, a cutscene's smoke (eudora_scene_smoke.cpp). The controller: the hero in each
        // of its boxes (or far), in the hero states the hints test, the message state busy or free, the help and statistics
        // records' counts and times at the edges of the hints' waits, the items, the pads (class 615) and pumps (class 280)
        // it names in their states, a pump near the hero. The chips (none placed): spawned (moby_spawn(0x5ec)) and set up as
        // the bot's spawn helper L4:0x2e4fe0 sets them, flying, fading, and bouncing on the floor at the hero's start (the
        // bot's own chips have no bounces: the bounce is dealt). The smoke: game mode 2 or not, the scenes 0 / 1 (and
        // others), the scene's mobys 2 / 3 a class-750 moby (a robot standing in for one when the level has none), another
        // moby or none.
        {
            const std::vector<uint32_t> helpers = mobys_with(0x002e4418), smokers = mobys_with(0x002e53a0);
            auto of_class = [](int32_t cls) {
                std::vector<uint32_t> v;
                for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100)
                    if (ee_read<uint8_t>(o + 0x20) < 0xfe && ee_read<int16_t>(o + 0xa6) == cls) v.push_back(o);
                return v;
            };
            level();
            std::vector<uint32_t> live;  // (the live mobys: the list L4:0x15ffe4, next +0x28)
            for (uint32_t o = ee_read<uint32_t>(L4_ADDR(0x0015ffe4)); o != 0 && live.size() < 4000; o = ee_read<uint32_t>(o + 0x28)) live.push_back(o);
            const std::vector<uint32_t> pads = of_class(0x267), pumps = of_class(0x118), smokables = of_class(0x2ee);
            const uint32_t chip_try = moby_spawn(0x5ec);
            if (info) std::printf("P43 helpers %zu smokers %zu pads %zu pumps %zu class750 %zu live %zu chip spawn %08x\n", helpers.size(), smokers.size(), pads.size(), pumps.size(), smokables.size(), live.size(), chip_try);
            level();

            // ---- class 1343
            // a moby of class `cls`: one of the level's, or (none, or half the time when `fake`) a live moby other than `m`
            // standing in for one (its class word set)
            auto class_moby = [pick, live](const std::vector<uint32_t>& own, int32_t cls, uint32_t m) -> uint32_t {
                if (!own.empty() && (coin() || live.size() < 2)) return pick(own);
                if (live.empty()) return m;
                uint32_t o = pick(live);
                for (int k = 0; k < 8 && o == m; ++k) o = pick(live);
                w16(o + 0xa6, u(cls));
                return o;
            };
            auto help_prep = [some_box, rate50, boxes, pads, pumps, class_moby, live, pick](uint32_t m, int st, int focus) {
                const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350, S = 0x00141848, H = 0x00141968;
                const uint32_t base = ee_read<uint32_t>(OV(0x0015ffd8));
                rate50();
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class1343_update_l4(m); }
                w8(m + 0x20, u(st));
                // the boxes: the level's, some replaced by another or none; the hero in the box of `focus` (a pvar offset, -1: one
                // at random), other hints' boxes made the same box in some cases (their conditions together), or far from it
                static const uint32_t offs[] = {0, 4, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x50};
                for (uint32_t o : offs) if (ri(0, 3) == 0) w32(pv + o, ri(0, 3) == 0 ? 0xffffffffu : some_box());
                const uint32_t at = focus >= 0 ? u(focus) : offs[ri(0, 7)];
                if (ee_read<int32_t>(pv + at) == -1 && ri(0, 7) != 0) w32(pv + at, some_box());
                const uint32_t bi = ee_read<uint32_t>(pv + at);
                if (bi != 0xffffffffu && static_cast<int32_t>(bi) < boxes) {
                    for (uint32_t o : offs) if (o != at && ri(0, 2) == 0) w32(pv + o, bi);
                    Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + bi * 0x80 + 0x30);
                    if (ri(0, 5) == 0) { q.x += rf(-40, 40); q.y += rf(-40, 40); }
                    else { q.x += rf(-0.2f, 0.2f); q.y += rf(-0.2f, 0.2f); q.z += rf(-0.2f, 0.2f); }
                    ee_ref<Vec4>(0x0013f3d0) = q;
                }
                // the message state: free (+0 zero, +0x24 -1) in most cases
                w32(L4_ADDR(0x00179590), ri(0, 4) == 0 ? 1u : 0u);
                w32(L4_ADDR(0x001795b4), ri(0, 4) == 0 ? u(ri(0, 0x4e30)) : 0xffffffffu);
                // (sub_00203908's own gate, 0x1516d0 +0x50 / +0x1c: open in most cases, so that the hints asked start)
                if (ri(0, 3) != 0) { w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu); }
                // the hero: his state (0, 1, 9 at ease), the Hydrodisplacer's 0x3b, the pad's 0x72 on a pad or another moby
                static const uint32_t sts[] = {0, 1, 9, 2, 5, 0x11, 0x12, 0x3b};
                w32(P + 0x208c, ri(0, 5) == 0 ? (R() & 0x7f) : sts[ri(0, 7)]);
                w32(P + 0x2090, coin() ? 0x3bu : u(ri(0, 0x60)));
                w32(P + 0x198, ri(0, 3) == 0 ? 1u : 0u);
                w32(P + 0x2084, coin() ? 0x72u : u(ri(0, 0x80)));
                w32(P + 0x2fc, ri(0, 4) == 0 ? 0u : (ri(0, 3) == 0 ? m : class_moby(pads, 0x267, m)));
                // the pads (+0x48 / +0x4c) and pumps (+0x40 / +0x44) it names: the level's ids, one of them, or none; their
                // states the goal's (4 / 5) or another
                for (uint32_t o : {0x48u, 0x4cu, 0x40u, 0x44u}) {
                    const bool pad = o >= 0x48;
                    if (coin()) w32(pv + o, (class_moby(pad ? pads : pumps, pad ? 0x267 : 0x118, m) - base) >> 8);
                    else if (ri(0, 3) == 0) w32(pv + o, 0xffffffffu);
                    const int32_t id = ee_read<int32_t>(pv + o);
                    if (id >= 0 && id < 0x800) {
                        const uint32_t t = base + (u(id) << 8);
                        w8(t + 0x20, coin() ? (pad ? 4u : 5u) : u(ri(0, 6)));
                        if (ri(0, 5) == 0) w16(t + 0xa6, pad ? 0x118u : 0x267u);
                    }
                }
                // a pump near the hero (in the live list: the level's or a stand-in), worked out (1.0) or not
                const bool pump_box = focus == 0x34 || focus == 0x38;
                if ((coin() || pump_box) && !live.empty()) {
                    const uint32_t o = !pumps.empty() && coin() ? pick(pumps) : pick(live);
                    w16(o + 0xa6, 0x118);
                    Vec4 q = ee_ref<Vec4>(0x0013f3d0);
                    const float r = coin() ? rf(2.8f, 3.2f) : rf(0, 6), a = rf(-3.1f, 3.1f);
                    q.x += r * std::cos(a); q.y += r * std::sin(a);
                    ee_ref<Vec4>(o + 0x10) = q;
                    if (const uint32_t opv = ee_read<uint32_t>(o + 0x78); opv != 0) wf(opv, coin() ? 1.0f : rf(0, 1));
                }
                // the play time and the records' times at the edges of the hints' waits (5 minutes, an hour, 18 seconds)
                const int32_t play = ri(400000, 3000000);
                w32(0x0015eea4, u(play));
                const int32_t q = frame_scale_i(play) / 600;
                auto near_q = [q] { switch (ri(0, 5)) { case 0: return 0; case 1: return q + 1; case 2: return q; default: return q - ri(1, 5); } };
                auto count = [](uint32_t a) { w16(a, ri(0, 9) == 0 ? (coin() ? 0xffffu : 0xfffeu) : (coin() ? 0u : u(ri(1, 3)))); };
                w16(S + 0xc0, ri(0, 9) == 0 ? 0xfffeu : u(ri(0, 5)));
                for (uint32_t a : {S + 0x88, S + 0xc8, S + 0xf8, H + 0xe0, H + 0xe8, H + 0xf0, H + 0x100, H + 0x230, H + 0x238, H + 0x248, H + 0x3b8}) count(a);
                if (ri(0, 2) == 0) for (uint32_t a : {S + 0x88, S + 0xc8, S + 0xf8, H + 0xe0, H + 0xe8, H + 0xf0, H + 0x100, H + 0x230, H + 0x238, H + 0x248, H + 0x3b8}) w16(a, 0);
                w16(H + 0x210, u(ri(0, 5)));
                for (uint32_t a : {S + 0xc2, S + 0x8a, S + 0xca, H + 0xfa, H + 0x102}) w16(a, u(std::max(0, near_q())));
                w16(0x001417c2, u(std::max(0, q - ri(10, 45))));
                w16(0x00141892, u(std::max(0, q - ri(250, 450))));
                for (uint32_t a : {S + 0xc4, S + 0x8c, S + 0xcc, H + 0x104}) w32(a, R());
                const uint32_t bit = 1u << (ee_read<uint32_t>(0x0015ed84) & 31);
                w32(H + 0x21c, coin() ? (R() | bit) : (R() & ~bit));
                // the items: the Glove of Doom (0x13d4c0 + 0x14), the Trespasser (+ 0x1a), the Suck Cannon's flag (0x13d4e8 + 9)
                w8(0x0013d4d4, u(ri(0, 1))); w8(0x0013d4da, u(ri(0, 1))); w8(0x0013d4f1, u(ri(0, 1)));
                w8(0x0014c050 + (ee_read<uint32_t>(0x0015ed84) << 4) + ee_read<uint8_t>(m + 0xb0), ri(0, 3) == 0 ? 0xffu : 0u);
                w32(0x0015eeac, u(ri(0, 40)));
                // its pvars: the visit's mark +0x14, the counts +8 / +0x20 / +0x24, the latches +0xc / +0x28, the pad's frames +0x1c
                w32(pv + 0x14, coin() ? 0xffffffffu : u(ri(0, 40)));
                w32(pv + 8, u(ri(0, 100)));
                w32(pv + 0xc, ri(0, 3) == 0 ? u(ri(1, 3)) : 0u);
                w32(pv + 0x20, coin() ? u(ri(1, 6)) : u(ri(0, 12)));
                if (pump_box) {  // (the pump hint's count about its third and sixth frame, the Hydrodisplacer in hand)
                    if (ri(0, 2) != 0) w32(pv + 0x20, coin() ? 2u : 5u);
                    if (ri(0, 3) != 0) w32(P + 0x2090, 0x3bu);
                }
                w32(pv + 0x24, u(ri(0, 5)));
                w32(pv + 0x28, coin() ? 0u : u(frame_scale_i(0xb4)));
                w32(pv + 0x1c, coin() ? u(frame_scale_i(0x708) + ri(-2, 2)) : u(ri(0, 2000)));
                seed_rand();
            };
            if (helpers.empty()) std::printf("eud_help                   L4:0x002e4418  no class 1343 moby on this level\n");
            else {
                struct HelpCase { const char* name; int st; int focus; int n; };
                static const HelpCase hc[] = {
                    {"eud_help_st0", 0, -1, 100}, {"eud_help_st1", 1, -1, 300}, {"eud_help_st2", 2, -1, 60},
                    {"eud_help_glove", 1, 0, 200},       // the box +0: the visits, the glove's hints
                    {"eud_help_trespasser", 1, 4, 300},  // the box +4: the Trespasser's hints, its pad solved, on a pad
                    {"eud_help_pump34", 1, 0x34, 150}, {"eud_help_pump38", 1, 0x38, 150},  // the Hydrodisplacer's hint
                    {"eud_help_ease2c", 1, 0x2c, 200}, {"eud_help_ease30", 1, 0x30, 150},  // the states counted, the record +0xc8
                    {"eud_help_box3c", 1, 0x3c, 100}, {"eud_help_box50", 1, 0x50, 150},
                };
                for (const HelpCase& c : hc) {
                    const int st = c.st, focus = c.focus;
                    l4check(c.name, 0x002e4418, c.n, Ret::None, [level, helpers, pick, help_prep, trial, st, focus] {
                        return Args{{trial(eud_class1343_update_l4, [&] {
                            level();
                            const uint32_t m = pick(helpers);
                            help_prep(m, st == 2 ? ri(2, 255) : st, focus);
                            return m;
                        })}, {}};
                    }, [](A a) { eud_class1343_update_l4(a.i[0]); return uint64_t{0}; });
                }
            }

            // ---- class 1516 (none placed): a chip spawned (the help moby's pvars standing in if the spawn fails), set up as
            // the spawn helper L4:0x2e4fe0 does, at the hero's start (on the floor)
            const uint32_t chip_fallback = !helpers.empty() ? helpers.front() : (!live.empty() ? live.front() : 0u);
            auto chip_deal = [level, rate50, bots, pick, chip_fallback](int st, bool bounce) -> uint32_t {
                level();
                rate50();
                uint32_t m = moby_spawn(0x5ec);
                if (m == 0 || ee_read<uint32_t>(m + 0x78) == 0) m = chip_fallback;
                const uint32_t pv = ee_read<uint32_t>(m + 0x78);
                Vec4 p = ee_ref<Vec4>(0x0013f3d0);
                p.x += rf(-0.5f, 0.5f); p.y += rf(-0.5f, 0.5f);
                p.z += bounce ? rf(-0.05f, 0.5f) : rf(-1, 3);
                ee_ref<Vec4>(m + 0x10) = p;
                w32(pv, bots.empty() ? 0u : pick(bots));
                // the start height: 3 above the chip at the edge, or anywhere (a bounce: not yet 3 below it)
                wf(pv + 4, bounce ? p.z + rf(-2, 2.5f) : (coin() ? p.z + 3.0f + rf(-0.05f, 0.05f) : p.z + rf(-2, 6)));
                wf(pv + 8, rf(0.05f, 0.6f));
                w16(pv + 0xc, bounce ? u(ri(0, 3)) : u(ri(0, 1)));
                w16(pv + 0xe, bounce ? (ri(0, 4) == 0 ? u(ri(2, 20)) : u(ri(0, 1))) : u(ri(0, 20)));
                wfrand(pv + 0x10, 3, -0.3f, 0.3f);
                if (bounce && ri(0, 4) != 0) wf(pv + 0x18, -rf(0, 0.3f));
                wfrand(pv + 0x20, 3, -0.2f, 0.2f);
                wfrand(m + 0x40, 3, -3.1f, 3.1f);
                wf(m + 0x2c, rf(0.1f, 1.5f));
                w8(m + 0x23, ri(0, 3) == 0 ? u(ri(2, 5)) : u(ri(0, 0x80)));
                w8(m + 0x20, u(st));
                seed_rand();
                return m;
            };
            if (chip_fallback == 0) std::printf("eud_chips                  L4:0x002e5178  no subject on this level\n");
            else {
                struct ChipCase { const char* name; int st; bool bounce; int n; };
                static const ChipCase cc[] = {{"eud_chips_st0", 0, false, 100}, {"eud_chips_st1", 1, false, 100}, {"eud_chips_st2", 2, false, 60}, {"eud_chips_bounce", 0, true, 300}};
                for (const ChipCase& c : cc) {
                    const int st = c.st; const bool bounce = c.bounce;
                    l4check(c.name, 0x002e5178, c.n, Ret::None, [chip_deal, st, bounce] {
                        return Args{{chip_deal(st == 2 ? ri(2, 255) : st, bounce)}, {}};
                    }, [](A a) { eud_class1516_update_l4(a.i[0]); return uint64_t{0}; });
                }
            }

            // ---- class 1549: the game mode, the scene, its mobys 2 / 3 (a class-750 moby, a robot standing in, another, none)
            auto smoke_prep = [pick, smokables, bipeds, live](uint32_t m, int st) {
                if (st != 0 && coin()) { w8(m + 0x20, 0); eud_class1549_update_l4(m); }
                w8(m + 0x20, u(st));
                w32(L4_ADDR(0x0015f5c4), ri(0, 3) == 0 ? u(ri(0, 4)) : 2u);
                const uint32_t sc = L4_ADDR(0x0016c9e0);
                w32(sc + 0x30, ri(0, 4) == 0 ? (coin() ? u(ri(2, 6)) : 0xffffffffu) : u(ri(0, 1)));
                for (uint32_t k = 2; k <= 3; ++k) {
                    uint32_t o = 0;
                    switch (ri(0, 4)) {
                    case 0: o = 0; break;
                    case 1: o = live.empty() ? 0u : pick(live); break;
                    default:
                        if (!smokables.empty() && coin()) o = pick(smokables);
                        else if (!bipeds.empty()) { o = pick(bipeds); w16(o + 0xa6, 0x2ee); }
                        break;
                    }
                    w32(sc + 0x178 + k * 4, o);
                }
                seed_rand();
            };
            if (smokers.empty()) std::printf("eud_scene_smoke            L4:0x002e53a0  no class 1549 moby on this level\n");
            else
                for (int st = 0; st <= 2; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_scene_smoke_st%d", st);
                    l4check(name, 0x002e53a0, st == 1 ? 200 : 100, Ret::None, [level, smokers, pick, smoke_prep, trial, st] {
                        return Args{{trial(eud_class1549_update_l4, [&] {
                            level();
                            const uint32_t m = pick(smokers);
                            smoke_prep(m, st == 2 ? ri(2, 255) : st);
                            return m;
                        })}, {}};
                    }, [](A a) { eud_class1549_update_l4(a.i[0]); return uint64_t{0}; });
                }
        }
        // ---- P4.3: class 1190, the talking NPC (eudora_npc.cpp): its states with the talk record as talk_init leaves it (the
        // port's state 0 run first: the generator stays short of the talk's stream waits), the talk's step 2 or not (the
        // walk-to block written on the build's own flag), the game mode 2 or not, the hero in its box or not, its
        // collectible taken (0xff) or not, the floor band near the camera; class_mates_mark_fixed(m, 0) on every case
        {
            const std::vector<uint32_t> npcs = mobys_with(0x002e3078);
            if (info) std::printf("P43 npcs %zu\n", npcs.size());
            if (npcs.empty()) std::printf("eud_npc                    L4:0x002e3078  no class 1190 moby on this level\n");
            else
                for (int st = 0; st <= 4; ++st) {
                    char name[32];
                    std::snprintf(name, sizeof name, "eud_npc_st%d", st);
                    const std::vector<uint32_t> npc_noops = g_extra_noop_hooks;
                    const std::time_t npc_clock = g_host_clock_fixed;
                    if (st == 2) {  // (its level change request runs on both sides, as the Suck Cannon's state 2: batch 139's hooks)
                        g_extra_noop_hooks = {0x00122298, 0x001232c8, 0x00122fe0, 0x00118a80};
                        g_host_clock_fixed = 1759658400;
                        g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                                         {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                                         {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }},
                                         {0x00120c30, [](EeCpu& c) { c.set32(2, cd_sync(static_cast<int32_t>(c.r32(4)))); }},
                                         {0x00121630, [](EeCpu& c) { c.set32(2, cd_get_error()); }},
                                         {0x001216c8, [](EeCpu& c) { c.set32(2, cd_break()); }}};
                    }
                    l4check(name, 0x002e3078, 100, Ret::None, [level, npcs, pick, rate50, trial, hero_at, st] {
                        return Args{{trial(eud_class1190_update_l4, [&] {
                            level();
                            const uint32_t m = pick(npcs), pv = ee_read<uint32_t>(m + 0x78);
                            rate50();
                            const uint32_t row = 0x0014c050u + ee_read<uint8_t>(m + 0xb0) + (ee_read<uint32_t>(0x0015ed84) << 4);
                            if (st != 0) { w8(row, 0); w8(m + 0x20, 0); eud_class1190_update_l4(m); }  // (the talk started)
                            w8(m + 0x20, u(st));
                            w8(row, coin() ? 0xffu : u(ri(0, 3)));
                            if (st == 2 || coin()) w16(pv + 4, coin() ? 2u : u(ri(0, 4)));
                            w32(L4_ADDR(0x0015f5c4), ri(0, 2) == 0 ? 2u : u(ri(0, 1)));
                            if (coin()) {  // (the hero in its box)
                                const int32_t b = ee_read<int32_t>(pv + 0x140);
                                if (b >= 0 && b < 1000) {
                                    Vec4 q = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(b) * 0x80 + 0x30);
                                    ee_ref<Vec4>(0x0013f3d0) = q; ee_ref<Vec4>(ee_read<uint32_t>(0x001413d0) + 0x10) = q;
                                }
                            } else {
                                hero_at(m, 0, 40);
                            }
                            if (coin()) {  // (its floor band near the camera point)
                                w8(m + 0x31, 1);
                                Vec4 c = ee_ref<Vec4>(m + 0x10); c.x += rf(-25, 25); c.y += rf(-25, 25);
                                ee_ref<Vec4>(L4_ADDR(0x00166f40)) = c;
                            }
                            if (coin()) wf(m + 0x48, rf(-3.1f, 3.1f));
                            seed_rand();
                            return m;
                        })}, {}};
                    }, [](A a) { eud_class1190_update_l4(a.i[0]); return uint64_t{0}; });
                    if (st == 2) {
                        g_extra_hooks.clear();
                        g_host_clock_fixed = npc_clock;
                        g_extra_noop_hooks = npc_noops;
                    }
                }
        }
        // ---- the saw's class hook +0x10 (eud_hook_10_l4) on the saws, the level's other mobys, no pvars and no moby
        {
            const std::vector<uint32_t> saws43 = mobys_with(0x002c2270);
            l4check("eud_hook_10", 0x002c41b0, 200, Ret::Int32, [level, saws43, pick] {
                level();
                uint32_t m = 0;
                switch (ri(0, 3)) {
                case 0: m = 0; break;
                case 1: m = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(0, 100)); break;
                default: m = saws43.empty() ? 0u : pick(saws43); break;
                }
                if (m != 0 && ri(0, 3) == 0) w32(m + 0x78, 0);
                return Args{{m}, {}};
            }, [](A a) { return r32(eud_hook_10_l4(a.i[0])); });
        }
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
