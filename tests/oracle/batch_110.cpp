// Oracle batch 110 (task 0334): the EE side of the sky (0x23d968 and its routines) on the level's real data (needs the ISO). The level
// started as a game does, then per case the camera (position and rotation, the view setup run), the frame cursor, the sky's per-frame
// state and (on Veldin's image, whose code carries all twenty sky variants) the level number the variant is picked by were varied. The
// original runs in the EE interpreter, the port on the same memory, every word of EE RAM and the scratchpad compared (the frame packet's
// chain: the sky's GIF packets, the texture uploads, the tags; the sky's state words).
//
// Veldin: build/tests/ovl_oracle --batch 110. Another level: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 110 (the level's own sky
// function at its own address, the port registered for it).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <chrono>
#include <game/ovl/sky_ee.h>
#include <game/ovl/sky_levels.h>
#include <game/ovl/variants.h>
#include <game/ovl/vu0_macro.h>
#include <runtime/vu/vu0.h>

namespace rac1::oracle {

namespace {
// The sky's sprites run VU0 micro programs (vcallms 0x190 / 0x192) and macro-mode code: the port's VU0 gets the micro memory the game uploads
// (the two VIF0 chains of the ELF) and the registers the original started from (vf / ACC / Q: stale lanes the code may read)
vu0m::Regs g_vu0_in;
void vu0_capture(EeInterpreter& ee) {
    for (int i = 0; i < 32; ++i)
        for (int k = 0; k < 4; ++k) g_vu0_in.vf[i].v[k] = ee.vu0().vf[static_cast<size_t>(i)].f[k];
    for (int k = 0; k < 4; ++k) g_vu0_in.acc.v[k] = ee.vu0().acc.f[k];
    g_vu0_in.q = ee.vu0().q;
}
void vu0_prelude() {
    vu0::vif0_send_chain(0x00100080, true);
    vu0::vif0_send_chain(0x0010e4c0, true);
    vu0m::regs() = g_vu0_in;
}
// the original of `veldin` on the level: the level's own function at `lvaddr` (a level variant, not in the map), or Veldin's translated
uint32_t sky_original(uint32_t veldin) {
    if (g_oracle_level == 0) return veldin;
    return ovl_code_lookup(veldin);  // the variants' rows are merged into the translation tables (variants_lN.cpp)
}
}  // namespace

void batch_110() {
    if (g_only_batch != 110) return;
    g_batch = 110;
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    g_pre_call = vu0_capture;
    static std::vector<uint8_t> snap;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            w16(0x001518d0, 512);
            w16(0x001518d2, 416);
            particle_table_init();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the frame's state the sky pass starts from: render enables on, the packet cursor in a free area, the camera (position and rotation rows) and
    // the view matrices from it (render_view_setup); the camera looks in random directions (the sky follows the camera's rotation)
    auto frame_state = [&] {
        level();
        screen_setup();
        fog_setup();
        render_enables_init();
        w32(OV(0x001611c0), 0x01e20000u + 16u * u(ri(0, 64)));
        const uint32_t tb = ee_read<uint32_t>(OV(0x00160f4c));
        const int32_t nt = ee_read<int32_t>(OV(0x00160f50));
        auto center = [&](int32_t k, float* c) { for (int i = 0; i < 3; ++i) c[i] = ee_read<float>(tb + u(k) * 0x40 + 4 * u(i)) / 1024.0f; };
        float a[3] = {0, 0, 0}, b[3] = {1, 0, 0};
        if (nt > 0) {
            center(ri(0, nt - 1), a);
            center(ri(0, nt - 1), b);
        }
        if (ri(0, 7) != 0) {
            const float dist = ri(0, 3) == 0 ? rf(0, 300) : rf(0, 60);
            float p[3], f[3];
            for (int i = 0; i < 3; ++i) p[i] = a[i] + rf(-1, 1) * dist;
            for (int i = 0; i < 3; ++i) f[i] = (ri(0, 2) == 0 ? rf(-1, 1) : b[i] - p[i] + rf(-1, 1) * 20.0f);
            auto norm = [](float* v) { const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); for (int i = 0; i < 3; ++i) v[i] = l > 1e-6f ? v[i] / l : (i == 0 ? 1.0f : 0.0f); };
            norm(f);
            const float roll = ri(0, 3) == 0 ? 1.0f : 0.3f;
            float up[3] = {rf(-roll, roll), rf(-roll, roll), 1.0f}, l[3] = {f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]};
            norm(l);
            const float u2[3] = {l[1] * f[2] - l[2] * f[1], l[2] * f[0] - l[0] * f[2], l[0] * f[1] - l[1] * f[0]};
            for (int i = 0; i < 3; ++i) {
                wf(OV(0x00166dc0) + 4 * u(i), p[i]);
                wf(OV(0x00166fd0) + 4 * u(i), f[i]);
                wf(OV(0x00166fe0) + 4 * u(i), l[i]);
                wf(OV(0x00166ff0) + 4 * u(i), u2[i]);
            }
        }
        render_view_setup();
        if (ri(0, 3) == 0) w32(OV(0x0015ee78), u(ri(0, 0x1800)) * 0x100u);  // (a frame counter / timer the sky reads)
        if (ri(0, 2) != 0) w32(OV(0x0015ee88), ri(0, 3) == 0 ? R() : u(ri(0, 0x7ffff)));  // (the word the tail's second GS write is made from)
        if (ri(0, 2) == 0) w32(OV(0x0015f5cc), R());  // (the frame counter the turning dome's matrix is made from)
    };

    if (std::getenv("ORACLE_STATS") != nullptr) {  // (RE aid) what the sky left: the chain's size
        frame_state();
        const uint32_t cur = ee_read<uint32_t>(OV(0x001611c0));
        sky_render_tr(0);
        std::printf("stats sky: chain %u bytes, sky pointer 0x%08x, level %d, shells %d sprites %d (max %d)\n", ee_read<uint32_t>(OV(0x001611c0)) - cur, ee_read<uint32_t>(OV(0x0016051c)), ee_read<int32_t>(0x0015ed84), ee_read<int16_t>(ee_read<uint32_t>(OV(0x0016051c)) + 6), ee_read<int16_t>(ee_read<uint32_t>(OV(0x0016051c)) + 8), ee_read<int16_t>(ee_read<uint32_t>(OV(0x0016051c)) + 10));
    }

    if (std::getenv("ORACLE_BENCH") != nullptr) {  // (the port's cost per frame on the level's camera: the EE side of the sky)
        frame_state();
        vu0_prelude();
        w32(0x0015ed84, u(g_oracle_level));
        const uint32_t cur = ee_read<uint32_t>(OV(0x001611c0));
        const int reps = 200;
        double ms = 0;
        for (int r = 0; r < reps; ++r) {
            ee_write<uint32_t>(OV(0x001611c0), cur);
            const auto t0 = std::chrono::steady_clock::now();
            sky_render();
            ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
        std::printf("bench sky (level %d): %.3f ms per frame, chain %u bytes\n", g_oracle_level, ms / reps, ee_read<uint32_t>(OV(0x001611c0)) - cur);
    }

    // the sky's sprites (stars): a leaf of the variants, run alone
    check("sky_sprites", 0x0028bd58, 24, Ret::None, [&] {
        frame_state();
        for (int k = ri(1, 3); k > 0; --k) ovl_289108_tr();  // the sprites live (updated: proved by sky_289108)
        return Args{{}, {}};
    }, [](A) { vu0_prelude(); ovl_28bd58_tr(); return uint64_t{0}; });

    check("sky_289108", 0x00289108, 24, Ret::None, [&] {
        frame_state();
        return Args{{}, {}};
    }, [](A) { vu0_prelude(); ovl_289108_tr(); return uint64_t{0}; });
    check("sky_288ec0", 0x00288ec0, 24, Ret::None, [&] {
        frame_state();
        return Args{{u(ri(0, 300)), u(ri(1, 20))}, {}};
    }, [](A a) { vu0_prelude(); ovl_288ec0_tr(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sky_28b848", 0x0028b848, 20, Ret::None, [&] {
        frame_state();
        return Args{{u(ri(0, 4))}, {}};
    }, [](A a) { vu0_prelude(); ovl_28b848_tr(a.i[0]); return uint64_t{0}; });

    // each variant alone (the level's equivalent where the matcher paired it, else skipped)
    {
        using TrFn = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float);
        static const struct { const char* name; uint32_t addr; TrFn fn; } vars[] = {
            {"sky_variant_289330", 0x00289330, ovl_289330_tr}, {"sky_variant_289420", 0x00289420, ovl_289420_tr}, {"sky_variant_2895a0", 0x002895a0, ovl_2895a0_tr},
            {"sky_variant_2899a8", 0x002899a8, ovl_2899a8_tr}, {"sky_variant_289a90", 0x00289a90, ovl_289a90_tr}, {"sky_variant_289bd8", 0x00289bd8, ovl_289bd8_tr},
            {"sky_variant_289cb8", 0x00289cb8, ovl_289cb8_tr}, {"sky_variant_28a208", 0x0028a208, ovl_28a208_tr}, {"sky_variant_28a2f0", 0x0028a2f0, ovl_28a2f0_tr},
            {"sky_variant_28a3a8", 0x0028a3a8, ovl_28a3a8_tr}, {"sky_variant_28a480", 0x0028a480, ovl_28a480_tr}, {"sky_variant_28a620", 0x0028a620, ovl_28a620_tr},
            {"sky_variant_28a7a0", 0x0028a7a0, ovl_28a7a0_tr}, {"sky_variant_28a920", 0x0028a920, ovl_28a920_tr}, {"sky_variant_28a998", 0x0028a998, ovl_28a998_tr},
            {"sky_variant_28aa98", 0x0028aa98, ovl_28aa98_tr}, {"sky_variant_28ab08", 0x0028ab08, ovl_28ab08_tr}, {"sky_variant_28ac88", 0x0028ac88, ovl_28ac88_tr},
            {"sky_variant_28b080", 0x0028b080, ovl_28b080_tr}, {"sky_variant_28b1a0", 0x0028b1a0, ovl_28b1a0_tr}, {"sky_variant_2892d0", 0x002892d0, ovl_2892d0_tr},
        };
        if (std::getenv("ORACLE_VARIANTS") != nullptr) {  // (a level's variant on its own; the whole sky is checked below)
            for (const auto& v : vars) {
                const TrFn fn = v.fn;
                check(v.name, v.addr, 12, Ret::None, [&] {
                    frame_state();
                    return Args{{}, {}};
                }, [fn](A) { vu0_prelude(); fn(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); return uint64_t{0}; });
            }
        }
    }

    if (g_oracle_level == 0) {
        // Veldin's image carries every level's sky variant: the level number (0x15ed84) picks one (the sky asset and the camera are Veldin's)
        for (int variant = 0; variant < 21; ++variant) {
            char name[40];
            std::snprintf(name, sizeof name, "sky_render_v%d", variant);
            check(name, 0x0023d968, variant == 20 ? 40 : 12, Ret::None, [&, variant] {
                frame_state();
                w32(0x0015ed84, u(variant == 20 ? ri(0, 40) : variant));
                return Args{{}, {}};
            }, [](A) { vu0_prelude(); sky_render(); return uint64_t{0}; });
        }
    } else {
        // the level's own dispatcher (its address from the translation row of sky_levels.cpp): the level number it tests is the level's
        // (the variant) or another (the default)
        const int own = g_oracle_level;
        check("sky_render", 0x0023d968, 40, Ret::None, [&, own] {
            frame_state();
            w32(0x0015ed84, u(ri(0, 2) != 0 ? own : ri(0, 30)));
            return Args{{}, {}};
        }, [](A) { vu0_prelude(); sky_render(); return uint64_t{0}; });
    }
    (void)sky_original;
    g_pre_call = nullptr;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
