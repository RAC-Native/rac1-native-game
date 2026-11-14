// Oracle batch 111 (task 0337): Eudora's own moby classes (level 4) on its real level data. Run with
// ORACLE_LEVEL=4 build/tests/ovl_oracle --batch 111 (needs the ISO). The subjects are the level's own
// mobys of each class (placed by its gameplay data) after the first gameplay frame, perturbed; the
// original is Eudora's own function at its own address, the port the one registered for it (the
// transliterations eudora_classes_tr_N.cpp, the hand ports eudora_*.cpp).

#include "ovl_oracle.h"

#include <game/ovl/helmet_tr.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/eudora_mobys.h>
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
        {"class184", 0x002af058, eud_class184_update_l4},
        {"class217", 0x002ba520, eud_class217_update_l4},
        {"class340", 0x002c2270, eud_class340_update_l4},
        {"class427", 0x002c4850, eud_class427_update_l4},
        {"class432", 0x002c6858, eud_class432_update_l4},
        {"class434", 0x002c6bb8, eud_class434_update_l4},
        {"class466", 0x002ca420, eud_class466_update_l4},
        {"class481", 0x002cdda0, eud_class481_update_l4},
        {"class484", 0x002ce060, eud_class484_update_l4},
        {"class563", 0x002d16b8, eud_class563_update_l4},
        {"class584", 0x002d3580, eud_class584_update_l4},
        {"class615", 0x002d4ae0, eud_class615_update_l4},
        {"class617", 0x002d6f68, eud_class617_update_l4},
        {"class642", 0x002d7e90, eud_class642_update_l4},
        {"class758", 0x002d9a58, eud_class758_update_l4},
        {"class1101", 0x002e17d8, eud_class1101_update_l4},
        {"class1120", 0x002e1a10, eud_class1120_update_l4, 0x4},
        {"class1190", 0x002e3078, eud_class1190_update_l4},
        {"class1343", 0x002e4418, eud_class1343_update_l4},
        {"class1516", 0x002e5178, eud_class1516_update_l4},
        {"class1549", 0x002e53a0, eud_class1549_update_l4},
        {"class1834", 0x002e79a8, eud_class1834_update_l4},
        {"class280", 0x002bfd30, eud_class280_update_l4},
        {"class437", 0x002c7e28, eud_class437_update_l4},
        {"class604", 0x002d4368, eud_class604_update_l4},
        {"class605", 0x002d46b8, eud_class605_update_l4},
        {"class805", 0x002d9fc0, eud_class805_update_l4},
        {"class832", 0x002dc168, eud_class832_update_l4},
        {"class1134", 0x002e1f38, eud_class1134_update_l4, 0x6},
        {"class1633", 0x002e5928, eud_class1633_update_l4},
        {"class1818", 0x002e69d0, eud_class1818_update_l4},
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
                        [](A a) { return r32(a.i[0] != 0 ? eud_cam_type1_accept_u(a.i[0], a.i[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0 : 0u); });
                l4check("cam_type1_init", 0x002ed920, 100, Ret::None, [cam1_setup] { return Args{{cam1_setup(false, nullptr)}, {}}; },
                        [](A a) { if (a.i[0] != 0) eud_cam_type1_init_l4(a.i[0]); return uint64_t{0}; });
                l4check("cam_type1_update", 0x002ed940, 100, Ret::None, [cam1_setup] { return Args{{cam1_setup(false, nullptr)}, {}}; },
                        [](A a) { if (a.i[0] != 0) eud_cam_type1_update_l4(a.i[0]); return uint64_t{0}; });
                l4check("cam_type1_exit", 0x002ed960, 100, Ret::None, [cam1_setup] { return Args{{cam1_setup(false, nullptr)}, {}}; },
                        [](A a) { if (a.i[0] != 0) eud_cam_type1_exit_l4(a.i[0]); return uint64_t{0}; });
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
        l4check("cam0_hook", 0x002f0448, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { eud_cam0_hook_l4(0); return uint64_t{0}; });
        l4check("cam_callback", 0x002f04a8, 120, Ret::None, [cam_prep] { cam_prep(); return Args{}; }, [](A) { eud_cam_callback_l4(0); return uint64_t{0}; });
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

    //@@NEXT
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
