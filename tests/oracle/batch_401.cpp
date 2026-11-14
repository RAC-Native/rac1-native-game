// Oracle batch 401 (issue #42, packages P6 / P8: the functions only a level's data or a dead branch reached):
//  - level 18 (ORACLE_LEVEL=18): the hook +0x14 of the class hook tables of classes 568 (L18:0x2d6280, table 0x1f3498) and 1906 (L18:0x2fcd00,
//    table 0x1f34c8), the level's own (the other classes' tables name Veldin's 0x2db868 there): the carried moby's reset after its explosion
//    (explode_and_reset 0x2dc938 calls the class's hook +0x14 through the moby dispatch). Transliterated in veldin_finale_classes_tr_6.cpp
//    (regen group veldin_finale_classes), registered in veldin_finale_mobys.cpp. Subjects: the level's live mobys (the class's own when
//    placed), their +0x53 (the animation branch), +0x34, +0x31, +0x94 varied;
//  - level 2 (ORACLE_LEVEL=2): Aridia's camera end-of-update callback L2:0x2fa770, queued by its camera hook 0x2fa710 when the look height
//    0x16760c is not 0 (the same code as Eudora's / Orxon's ported callbacks, at Aridia's data): every level's copy is
//    kerwan_camera.cpp's cam0_level_callback, wired in camera_callback_call. Its block 0x167600 varied;
//  - levels 0 and 14 (ORACLE_LEVEL=0 / 14): the scenes' level 14 effect 0x23e008 and its draw 0x23db30 (cutscene_level_extras's level 14
//    branch; Oltanis runs its own build of the function, so the port's Veldin build reaches them only on a level 14 without it):
//    transliterated at Veldin's addresses in shared_fx_tr.cpp (regen group veldin_fx), the draw registered for the moby dispatch. Subject: the
//    hero (a model with chain points: both read the moby's chain 0), its position, the frame scale and the random seed varied. On level 14 the
//    original is Oltanis's own copy (the map's 0x245e98 / 0x2459c0), the same code.
// No RAM dumps: the states are built from the level.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 401, N in 0, 2, 14, 18.

#include "ovl_oracle.h"

#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_fx_tr.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/shared_finale.h>

#include <cstring>
#include <functional>
#include <vector>

namespace rac1::oracle {

namespace {
// A check whose original is level `lvl`'s own function at `laddr` (not a Veldin address to translate), as batch 271's.
void l401check(int lvl, const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen,
               const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != lvl) {
        std::printf("%-26s L%d:0x%08x  skipped (level %d only: run with ORACLE_LEVEL=%d)\n", name, lvl, laddr, lvl, lvl);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, lvl, laddr);
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
}  // namespace

void batch_401() {
    if (g_only_batch != 401) return;
    g_batch = 401;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // the level after its first gameplay frame (the port's), as batches 340 / 400
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);
            if (g_oracle_level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221's)
            gameplay_frame();
            render_view_setup();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };

    // ---- level 18: the hook +0x14 of classes 568 / 1906
    {
        struct Hk { const char* name; uint32_t addr; uint16_t cls; void (*fn)(uint32_t); };
        // (the shared port finale_stop_hook, shared_finale.cpp, with each copy's state: 5 / 8, as batch 510 checks them)
        static const Hk hks[] = {{"vf_hook_2d6280_l18", 0x002d6280, 568, [](uint32_t m) { finale_stop_hook(m, 5); }},
                                 {"vf_hook_2fcd00_l18", 0x002fcd00, 1906, [](uint32_t m) { finale_stop_hook(m, 8); }}};
        for (const Hk& h : hks) {
            if (g_oracle_level != 18) { l401check(18, h.name, h.addr, 0, Ret::None, nullptr, nullptr); continue; }
            level();
            std::vector<uint32_t> own, all;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
                if (ee_read<uint8_t>(m + 0x20) >= 0xfe) continue;
                all.push_back(m);
                if (ee_read<uint16_t>(m + 0xa6) == h.cls) own.push_back(m);
            }
            std::printf("  %s: %zu live mobys of class %u, %zu live mobys\n", h.name, own.size(), h.cls, all.size());
            l401check(18, h.name, h.addr, 60, Ret::None, [level, own, all, pick] {
                level();
                const uint32_t m = (!own.empty() && coin()) ? pick(own) : pick(all);
                w8(m + 0x53, ri(0, 2) == 0 ? 0u : u(ri(1, 8)));
                w16(m + 0x34, R() & 0xffff);
                w8(m + 0x31, R() & 0xff);
                w32(m + 0x94, R());
                if (ri(0, 4) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));  // (the frame scale frame_scale_i reads)
                seed_rand();
                return Args{{m}, {}};
            }, [h](A a) { h.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- level 2: Aridia's camera end-of-update callback (no argument: its block 0x167600)
    if (g_oracle_level != 2) {
        l401check(2, "cam_callback_2fa770_l2", 0x002fa770, 0, Ret::None, nullptr, nullptr);
    } else {
        level();
        l401check(2, "cam_callback_2fa770_l2", 0x002fa770, 60, Ret::None, [level] {
            level();
            const uint32_t B = 0x00167600;  // (Aridia's own data: the block the callback and the hook 0x2fa710 share)
            w32(B, R());
            w32(B + 4, ri(0, 2) == 0 ? R() : 0u);
            wf(B + 8, rf(-5.0f, 5.0f));
            wf(B + 0xc, ri(0, 3) == 0 ? rf(0.0f, 0.05f) : rf(-1.0f, 4.0f));
            seed_rand();
            return Args{{}, {}};
        }, [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- levels 0 and 14: the scenes' level 14 effect and its draw (Veldin's build), on the hero
    {
        struct Fx { const char* name; uint32_t addr; void (*fn)(uint32_t); int n; };
        static const Fx fxs[] = {{"cutscene_l14_effect_tr", 0x0023e008, cutscene_l14_effect_tr, 60},
                                 {"cutscene_l14_effect_draw_tr", 0x0023db30, cutscene_l14_effect_draw_tr, 60}};
        for (const Fx& f : fxs) {
            if (g_oracle_level != 0 && g_oracle_level != 14) {
                skip_check(f.name, f.addr, "levels 0 and 14 only (ORACLE_LEVEL=0 / 14)");
                continue;
            }
            level();
            check(f.name, f.addr, f.n, Ret::None, [level] {
                level();
                const uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8));  // (the hero: a model with chain points)
                if (coin()) {
                    const Vec4 c = VV(OV(0x00166dc0));
                    VV(m + 0x10) = Vec4{c.x + rf(-15, 15), c.y + rf(-15, 15), c.z + rf(-5, 5), 1.0f};
                }
                if (ri(0, 3) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
                if (ri(0, 3) == 0) wf(0x0015ed6c, rf(0.5f, 3.0f));
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{m}, {}};
            }, [f](A a) { f.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
