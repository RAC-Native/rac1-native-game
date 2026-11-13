// Hand-ported level sky renderer and per-planet sky variants (Veldin overlay 0x23d968 + 0x288ec0..0x28bae0).
// Replaces generated sky_ee.cpp; the 5 VU0 leaf routines live in sky_hand.cpp / sky_fast.cpp.
#include <game/ovl/sky_ee.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/level_change.h>
#include <game/ovl/movie_mode.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sky_hand.h>
#include <game/ovl/veldin_mobys.h>

#include <cstdint>
#include <vector>

namespace rac1 {

namespace {

constexpr uint32_t kSkyPtrAddr = 0x0016051cu;
constexpr uint32_t kSkyMatAddr = 0x001bd7e0u;
constexpr uint32_t kFrameTimerAddr = 0x0015f5ccu;
constexpr uint32_t kGsAlphaSky = 0x42u;
constexpr uint64_t kGsAlphaStarsVal = 0x8000000044ull;

inline float kPi() { return u2f(0x40490fdbu); }

// Shared double-buffered scratchpad DMA + sphere-cull + triangle-packet loop for textured (0x28b8a0) and
// vertex-coloured (0x28bae0) sky shells.
uint32_t draw_sky_shell(uint32_t shell, bool vertex_colours) {
    const int32_t count = lw(shell + 0x0);
    if (count == 0) return 0;

    uint8_t vis_local[256];
    std::vector<uint8_t> vis_heap;
    uint8_t* vis = vis_local;
    if (count > 256) {
        vis_heap.resize(static_cast<size_t>(count));
        vis = vis_heap.data();
    }
    if (count > 0) {
        sky_hand::spheres_host(shell + 0x10, static_cast<uint32_t>(count), vis);
    } else {
        vis[0] = 0;
    }

    const uint32_t pkt = lwu(OV(0x001611c0u));
    sw(pkt + 0x0, 0x30000007u);
    sw(pkt + 0x4, vertex_colours ? 0x0013d0f0u : 0x0013d160u);
    sw(pkt + 0x8, 0u);
    sw(pkt + 0xc, 0x50000007u);
    sw(OV(0x001611c0u), pkt + 0x10);

    const uint32_t first_buf = 1u - lwu(OV(0x001604d0u));
    sw(OV(0x001604d0u), first_buf);
    if (vis[0] == 1) {
        const uint32_t qwc = static_cast<uint32_t>(static_cast<int32_t>(lh(shell + 0x2e)) >> 4);
        const uint32_t spr_dst = lwu(OV(0x001604c8u) + (first_buf << 2));
        ovl_24d678_tr(lwu(shell + 0x20), qwc, spr_dst, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    if (lw(shell + 0x0) > 0) {
        for (int32_t i = 0; i < lw(shell + 0x0); ++i) {
            if (vis[i] == 1) {
                ovl_24d698_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            const uint32_t next_buf = 1u - lwu(OV(0x001604d0u));
            sw(OV(0x001604d0u), next_buf);
            const int32_t next_i = i + 1;
            if (next_i < lw(shell + 0x0) && vis[next_i] == 1) {
                const uint32_t next_part = shell + static_cast<uint32_t>(next_i << 5);
                const uint32_t qwc = static_cast<uint32_t>(static_cast<int32_t>(lh(next_part + 0x2e)) >> 4);
                const uint32_t spr_dst = lwu(OV(0x001604c8u) + (next_buf << 2));
                ovl_24d678_tr(lwu(next_part + 0x20), qwc, spr_dst, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            if (vis[i] == 1) {
                const uint32_t cur_buf = 1u - lwu(OV(0x001604d0u));
                const uint32_t part = shell + static_cast<uint32_t>(i << 5);
                const uint32_t base = lwu(OV(0x001604c8u) + (cur_buf << 2));
                const uint32_t tri_off = static_cast<uint32_t>(lh(part + 0x2c));
                const uint32_t data_off = static_cast<uint32_t>(lh(part + 0x2a));
                const uint32_t vtx_off = static_cast<uint32_t>(lh(part + 0x28));
                const uint32_t n_vtx = static_cast<uint32_t>(lh(part + 0x24));
                const TrRet proj = ovl_28c14c_tr(base + vtx_off, 0x70002000u, n_vtx, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                if (proj.v0 == 0) {
                    const uint32_t n_tri = static_cast<uint32_t>(lh(part + 0x26));
                    if (vertex_colours) {
                        ovl_28c298_tr(n_tri, base + tri_off, base + data_off, 0x70002000u, 0, 0, 0, 0, 0, 0, 0, 0);
                    } else {
                        ovl_28c3c0_tr(n_tri, base + tri_off, base + data_off, 0x70002000u, 0, 0, 0, 0, 0, 0, 0, 0);
                    }
                }
            }
        }
    }

    if (vertex_colours) {
        sub_002a0ba8(0x47u, 0x3180bull);
        sub_002a0ba8(0x4eu, 0x101000000ull | static_cast<uint64_t>(static_cast<int64_t>(lw(0x15ee88u) >> 13)));
    }
    return 0;
}

inline void draw_shell_idx(uint32_t idx) {
    ovl_28b848_tr(idx, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

inline void set_sky_yaw(float yaw, float pitch_y = 0.0f) {
    Vec4 rot{0.0f, pitch_y, yaw, 0.0f};
    euler_to_mat4_fpu(M(OV(kSkyMatAddr)), rot);
}

inline float yaw_from_masked(uint32_t counter, uint32_t scale_bits) {
    return fpu_sub(fpu_mul(static_cast<float>(static_cast<int32_t>(counter)), u2f(scale_bits)), kPi());
}

inline float yaw_from_mod(int32_t period, uint32_t scale_bits) {
    const int32_t rem = ee_rem(lw(OV(kFrameTimerAddr)), period);
    return fpu_sub(fpu_mul(static_cast<float>(rem), u2f(scale_bits)), kPi());
}

// Twinkle RGBA update for static star sprites (used by 0x289108, Aridia 0x2895a0, Blarg 0x289cb8, Fleet 0x28ac88).
void update_twinkle_star_colour(uint32_t s) {
    const uint32_t r = static_cast<uint32_t>(libc_rand() >> 16);
    uint32_t col = lwu(s + 0xc);
    col += ((r & 0x1f00u) << 10) + 0xffdfdfe0u;
    col += (r & 0x1f0u) << 6;
    col += (r & 0x1fu) << 2;
    sw(s + 0x4, col);
}

// Orbiting satellite / shooting-star update (type 0):
//   hemisphere_abs = true  -> z = |cos(el)| * 50, colour = 0x702020f0 / 0x202020f0 (0x289108, Aridia 0x2895a0)
//   hemisphere_abs = false -> z = cos(el) * 50, preserves RGB at s+4 with alpha 0x70 / 0x24 (Blarg 0x289cb8, Fleet 0x28ac88)
void update_orbit_sprite(uint32_t s, bool hemisphere_abs) {
    const uint16_t t0 = static_cast<uint16_t>(lhu(s + 0xc) + 1u);
    sh(s + 0xc, t0);
    const uint16_t t1 = static_cast<uint16_t>(lhu(s + 0xe) + 1u);
    sh(s + 0xe, t1);

    const float step = u2f(0x3ac90fdbu);
    const float radius = 50.0f;
    const float az = fpu_mul(int_to_float(static_cast<int32_t>(lhu(s + 0xc) & 0xfffu) - 0x800), step);
    const float el = fpu_mul(int_to_float(static_cast<int32_t>(lhu(s + 0xe) & 0xfffu) - 0x800), step);

    sw(s + 0x10, f2u(fpu_mul(fpu_mul(cos_vu(az), sin_vu(el)), radius)));
    sw(s + 0x14, f2u(fpu_mul(fpu_mul(sin_vu(az), sin_vu(el)), radius)));

    if (hemisphere_abs) {
        const float cz = fpu_mul(abs_f(cos_vu(el)), radius);
        const bool flash = (lhu(s + 0xc) & 0x3fu) < 8u;
        sw(s + 0x18, f2u(cz));
        sw(s + 0x4, (flash ? 0x70200000u : 0x20200000u) | 0x20f0u);
    } else {
        const float cz = fpu_mul(cos_vu(el), radius);
        const uint32_t rgb = lwu(s + 0x4) & 0x00ffffffu;
        const bool flash = (lhu(s + 0xc) & 0x3fu) < 8u;
        sw(s + 0x18, f2u(cz));
        sw(s + 0x4, rgb | (flash ? 0x70000000u : 0x24000000u));
    }
}

// Initialises one twinkling star entry (used by 0x288ec0, Aridia, Blarg, Fleet).
void init_twinkle_star(uint32_t s, uint8_t star_type, uint8_t tex_base, bool full_sphere, uint32_t tint_bias) {
    sb(s + 0x0, star_type);
    sh(s + 0xc, static_cast<uint16_t>(rand_mod(0x100)));
    const uint32_t r_tex = static_cast<uint32_t>(libc_rand() >> 16);
    sb(s + 0x3, 0x48u);
    sb(s + 0x2, static_cast<uint8_t>((r_tex & 1u) + tex_base));
    sw(s + 0x8, f2u(rand_angle()));
    const int32_t sz = rand_mod(0x30) + 0x20;
    sw(s + 0x1c, f2u(fpu_mul(int_to_float(sz), u2f(0x3b800000u))));

    const float radius = 50.0f;
    const float az = rand_angle();
    const float el = rand_angle();
    sw(s + 0x10, f2u(fpu_mul(fpu_mul(cos_vu(az), sin_vu(el)), radius)));
    sw(s + 0x14, f2u(fpu_mul(fpu_mul(sin_vu(az), sin_vu(el)), radius)));
    if (full_sphere) {
        sw(s + 0x18, f2u(fpu_mul(cos_vu(el), radius)));
    } else {
        const float c0 = cos_vu(el);
        const float c1 = cos_vu(el);
        sw(s + 0x18, f2u(fpu_add(fpu_mul(fpu_mul(c0, c1), 36.0f), 16.0f)));
    }

    const uint32_t tint = static_cast<uint32_t>(rand_mod(0x18)) + tint_bias;
    const uint32_t alpha = static_cast<uint32_t>(rand_mod(0x20)) << 24;
    const uint32_t coin = static_cast<uint32_t>(libc_rand() >> 16) & 1u;
    uint32_t col = 0;
    if (coin != 0) {
        col = alpha + ((tint << 16) + 0x30505050u);
    } else {
        col = (alpha + ((tint << 8) + 0x30505050u)) | tint;
    }
    sw(s + 0xc, col);
}

// Shared three-turning-shells sky variant:
//   Novalis (L1, 0x289420): shells (2, 3, 4)
//   Pokitaru / Hoven / Kalebo (L11 0x28a620, L12 0x28a7a0, L16 0x28ab08): shells (1, 2, 3)
void sky_shells_turn_triple(int32_t s_slow, int32_t s_mid, int32_t s_fast) {
    const uint32_t sky0 = lwu(OV(kSkyPtrAddr));
    sh(sky0 + 0x4, 0);
    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x6); ++i) {
        if (i == s_slow) {
            set_sky_yaw(yaw_from_masked(lwu(OV(kFrameTimerAddr)) & 0x3ffffu, 0x37c90fdbu));
        } else if (i == s_mid) {
            set_sky_yaw(yaw_from_masked(lwu(OV(kFrameTimerAddr)) & 0x1ffffu, 0x38490fdbu));
        } else if (i == s_fast) {
            set_sky_yaw(yaw_from_masked(lhu(OV(kFrameTimerAddr)), 0x38c90fdbu));
        } else {
            mat4_identity(M(OV(kSkyMatAddr)));
        }
        draw_shell_idx(static_cast<uint32_t>(i));
    }
}

// Shared helper: ensure star sprites exist (n_stars, n_extra), update them via 0x289108, draw them via 0x28bd58, and set GS ALPHA_1.
void run_standard_stars(uint32_t n_stars, uint32_t n_extra) {
    const uint32_t sky = lwu(OV(kSkyPtrAddr));
    if (lh(sky + 0x8) == 0) {
        ovl_288ec0_tr(n_stars, n_extra, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    ovl_289108_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    ovl_28bd58_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(kGsAlphaSky, kGsAlphaStarsVal);
}

}  // namespace

// orig: 0x0024d678 — kick normal-mode DMA9 (toSPR) transfer into scratchpad
TrRet ovl_24d678_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    eer::hw_sw(0x1000d480u, a2);
    eer::hw_sw(0x1000d420u, a1);
    eer::hw_sw(0x1000d410u, a0);
    eer::hw_sw(0x1000d400u, 0x100u);
    return TrRet{0x100u, 0.0f};
}

// orig: 0x0024d698 — wait for DMA9 (toSPR) completion
TrRet ovl_24d698_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    while ((eer::hw_lw(0x1000d400u) & 0x100u) != 0) {}
    return TrRet{0u, 0.0f};
}

// orig: 0x0028b8a0 — draw textured sky shell
TrRet ovl_28b8a0_tr(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    return TrRet{draw_sky_shell(a0, false), 0.0f};
}

// orig: 0x0028bae0 — draw vertex-coloured sky shell
TrRet ovl_28bae0_tr(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    return TrRet{draw_sky_shell(a0, true), 0.0f};
}

// orig: 0x0028b848 — dispatch sky shell `a0` to textured or vertex-coloured path
TrRet ovl_28b848_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13,
                    float f14, float f15) {
    const uint32_t sky = lwu(OV(kSkyPtrAddr));
    if (static_cast<int32_t>(a0) >= lh(sky + 0x6)) return TrRet{0u, 0.0f};
    const uint32_t shell = lwu(sky + 0x20 + (a0 << 2));
    if (lwu(shell + 0x4) != 0) {
        return ovl_28bae0_tr(shell, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15);
    }
    return ovl_28b8a0_tr(shell, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15);
}

// orig: 0x00288ec0 — initialise `a0` twinkling stars + `a1` orbiting satellites in the sky sprite list
TrRet ovl_288ec0_tr(uint32_t a0, uint32_t a1, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    const uint32_t sky0 = lwu(OV(kSkyPtrAddr));
    const int16_t total = static_cast<int16_t>(a0 + a1);
    sh(sky0 + 0x8, static_cast<uint16_t>(total));
    if (total <= 0) return TrRet{0u, 0.0f};

    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
        const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
        if (i < static_cast<int32_t>(a0)) {
            init_twinkle_star(s, 1u, 0u, false, 0u);
        } else {
            sb(s + 0x0, 0u);
            sh(s + 0xc, static_cast<uint16_t>(libc_rand() >> 16));
            sh(s + 0xe, static_cast<uint16_t>(libc_rand() >> 16));
            sw(s + 0x1c, 0x3e23d70au);
            sb(s + 0x2, 1u);
            sb(s + 0x3, 0x48u);
        }
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x00289108 — per-frame update of standard sky sprites (type 0 satellites and type 1 stars)
TrRet ovl_289108_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    const uint32_t sky0 = lwu(OV(kSkyPtrAddr));
    if (lh(sky0 + 0x8) <= 0) return TrRet{0u, 0.0f};

    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
        const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
        const uint8_t type = lbu(s + 0x0);
        if (type == 0u) {
            update_orbit_sprite(s, true);
        } else if (type == 1u) {
            update_twinkle_star_colour(s);
        }
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x002892d0 — default sky variant: draw all shells with identity matrix
TrRet ovl_2892d0_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    mat4_identity(M(OV(kSkyMatAddr)));
    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x6); ++i) {
        draw_shell_idx(static_cast<uint32_t>(i));
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x00289330 — Veldin (L0) sky variant
TrRet ovl_289330_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    draw_shell_idx(0);
    draw_shell_idx(1);
    run_standard_stars(244u, 12u);
    draw_shell_idx(2);
    set_sky_yaw(yaw_from_masked(lwu(OV(kFrameTimerAddr)) & 0x7fffu, 0x39490fdbu));
    draw_shell_idx(3);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(4);
    return TrRet{0u, 0.0f};
}

// orig: 0x00289420 — Novalis (L1) sky variant: shells 2, 3, 4 turn at 1/4, 1/2, 1x speed
TrRet ovl_289420_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sky_shells_turn_triple(2, 3, 4);
    return TrRet{0u, 0.0f};
}

// orig: 0x002895a0 — Aridia (L2) sky variant
TrRet ovl_2895a0_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    draw_shell_idx(1);

    const uint32_t sky = lwu(OV(kSkyPtrAddr));
    if (lh(sky + 0x8) == 0) {
        sh(sky + 0x8, 0x100u);
        for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
            const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
            if (i < 246) {
                init_twinkle_star(s, 3u, 2u, false, 0u);
            } else {
                sb(s + 0x0, 0u);
                sh(s + 0xc, static_cast<uint16_t>(libc_rand() >> 16));
                sh(s + 0xe, static_cast<uint16_t>(libc_rand() >> 16));
                sw(s + 0x1c, 0x3e23d70au);
                sb(s + 0x2, 3u);
                sb(s + 0x3, 0x48u);
            }
        }
    }

    if (lh(lwu(OV(kSkyPtrAddr)) + 0x8) > 0) {
        for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
            const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
            const uint8_t type = lbu(s + 0x0);
            if (type == 0u) {
                update_orbit_sprite(s, true);
            } else if (type == 3u) {
                update_twinkle_star_colour(s);
            }
        }
    }

    ovl_28bd58_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(kGsAlphaSky, kGsAlphaStarsVal);
    draw_shell_idx(2);
    draw_shell_idx(3);
    return TrRet{0u, 0.0f};
}

// orig: 0x002899a8 — Kerwan (L3) sky variant
TrRet ovl_2899a8_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    set_sky_yaw(yaw_from_masked(lhu(OV(kFrameTimerAddr)), 0x38c90fdbu));
    draw_shell_idx(1);
    set_sky_yaw(yaw_from_masked(lwu(OV(kFrameTimerAddr)) & 0x1ffffu, 0x38490fdbu));
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x00289a90 — Eudora (L4) sky variant
TrRet ovl_289a90_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x6); ++i) {
        if (i == 1) {
            set_sky_yaw(yaw_from_masked(lhu(OV(kFrameTimerAddr)), 0x38c90fdbu));
        } else if (i == 2) {
            set_sky_yaw(yaw_from_masked(lwu(OV(kFrameTimerAddr)) & 0x1ffffu, 0x38490fdbu));
        } else {
            mat4_identity(M(OV(kSkyMatAddr)));
        }
        draw_shell_idx(static_cast<uint32_t>(i));
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x00289bd8 — Rilgar (L5) sky variant
TrRet ovl_289bd8_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    run_standard_stars(120u, 8u);
    set_sky_yaw(yaw_from_mod(50000, 0x3903c498u));
    draw_shell_idx(1);
    return TrRet{0u, 0.0f};
}

// orig: 0x00289cb8 — Blarg (L6) sky variant (full-sphere stars, beacon flashes, and coloured satellites)
TrRet ovl_289cb8_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);

    const uint32_t sky = lwu(OV(kSkyPtrAddr));
    if (lh(sky + 0x8) == 0) {
        sh(sky + 0x8, 0x100u);
        for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
            const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
            if (i < 234) {
                init_twinkle_star(s, 3u, 0u, true, 8u);
            } else if (i < 240) {
                const int32_t rel = i - 234;
                const uint32_t odd = static_cast<uint32_t>(rel) & 1u;
                const uint32_t grp = static_cast<uint32_t>(rel >> 1);
                sb(s + 0x2, 2u);
                sb(s + 0x3, 0x48u);
                sw(s + 0x8, f2u(rand_angle()));
                sb(s + 0x0, 2u);
                sw(s + 0x4, lwu(OV(0x001604f0u) + (grp << 2)));
                V(s + 0x10) = V(OV(0x001bd7b0u) + (grp << 4));
                sb(s + 0xc, static_cast<uint8_t>(grp));
                sb(s + 0xd, static_cast<uint8_t>(odd));
                sb(s + 0xe, static_cast<uint8_t>(odd * 24u));
            } else {
                sb(s + 0x0, 0u);
                sh(s + 0xc, static_cast<uint16_t>(libc_rand() >> 16));
                sh(s + 0xe, static_cast<uint16_t>(libc_rand() >> 16));
                sb(s + 0x3, 0x48u);
                sb(s + 0x2, 1u);
                sw(s + 0x1c, 0x3e3851ecu);
                const uint32_t pal_off = static_cast<uint32_t>(libc_rand() >> 14) & 0xcu;
                sw(s + 0x4, lwu(OV(0x00160500u) + pal_off));
            }
        }
    }

    if (lh(lwu(OV(kSkyPtrAddr)) + 0x8) > 0) {
        for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
            const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
            const uint8_t type = lbu(s + 0x0);
            if (type == 0u) {
                update_orbit_sprite(s, false);
            } else if (type == 2u) {
                const int32_t phase = ee_rem(static_cast<int32_t>(lbu(s + 0xe)) + 1, 48);
                sb(s + 0xe, static_cast<uint8_t>(phase));
                if (phase == 0) {
                    const uint32_t r = static_cast<uint32_t>(libc_rand() >> 16) & 0xfu;
                    const int32_t ang_i = static_cast<int32_t>((lbu(s + 0xd) << 5) + r) - 8;
                    sw(s + 0x8, f2u(fpu_mul(int_to_float(ang_i), u2f(0x3cc90fdbu))));
                }
                uint32_t alpha = lbu(s + 0xe) << 2;
                if (static_cast<int32_t>(alpha) >= 97) {
                    alpha = 0xc0u - alpha;
                }
                const uint32_t r = static_cast<uint32_t>(libc_rand() >> 16);
                const uint32_t base_col = lwu(OV(0x001604f0u) + (lbu(s + 0xc) << 2));
                const uint32_t col = base_col + ((r & 0xf00u) << 8) + ((r & 0xf0u) << 4) + (r & 0xfu) + (alpha << 24);
                sw(s + 0x4, col);
            } else if (type == 3u) {
                update_twinkle_star_colour(s);
            }
        }
    }

    ovl_28bd58_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(kGsAlphaSky, kGsAlphaStarsVal);
    draw_shell_idx(1);
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a208 — Umbris (L7) sky variant
TrRet ovl_28a208_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    run_standard_stars(120u, 8u);
    draw_shell_idx(1);
    set_sky_yaw(yaw_from_mod(50000, 0x3903c498u));
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a2f0 — Batalia (L8) sky variant
TrRet ovl_28a2f0_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    set_sky_yaw(yaw_from_mod(40000, 0x3924b5beu));
    draw_shell_idx(1);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a3a8 — Gaspar (L9) sky variant
TrRet ovl_28a3a8_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    draw_shell_idx(1);
    draw_shell_idx(2);
    set_sky_yaw(yaw_from_mod(25000, 0x3983c498u), u2f(0x3e99999au));
    draw_shell_idx(3);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(4);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a480 — Orxon (L10) sky variant
TrRet ovl_28a480_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x6); ++i) {
        const uint32_t t = lwu(OV(kFrameTimerAddr));
        if (i == 0) {
            set_sky_yaw(yaw_from_masked((t << 1) & 0x1ffffu, 0x38490fdbu));
        } else if (i == 1) {
            set_sky_yaw(yaw_from_masked((t * 5u) & 0x1ffffu, 0x38490fdbu));
        } else if (i == 2) {
            set_sky_yaw(yaw_from_masked((t * 10u) & 0x3ffffu, 0x37c90fdbu));
        } else if (i == 3) {
            set_sky_yaw(yaw_from_masked((t * 10u) & 0x1ffffu, 0x38490fdbu));
        } else {
            mat4_identity(M(OV(kSkyMatAddr)));
        }
        draw_shell_idx(static_cast<uint32_t>(i));
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a620 — Pokitaru (L11) sky variant (identical to L12 / L16)
TrRet ovl_28a620_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sky_shells_turn_triple(1, 2, 3);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a7a0 — Hoven (L12) sky variant (identical to L11 / L16)
TrRet ovl_28a7a0_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sky_shells_turn_triple(1, 2, 3);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a920 — Gemlik (L13) sky variant
TrRet ovl_28a920_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    draw_shell_idx(1);
    run_standard_stars(120u, 8u);
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028a998 — Oltanis (L14) sky variant
TrRet ovl_28a998_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    set_sky_yaw(yaw_from_mod(50000, 0x3903c498u));
    draw_shell_idx(1);
    set_sky_yaw(yaw_from_mod(100000, 0x3883c498u));
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028aa98 — Quartu (L15) sky variant
TrRet ovl_28aa98_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);
    run_standard_stars(120u, 8u);
    draw_shell_idx(1);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028ab08 — Kalebo (L16) sky variant (identical to L11 / L12)
TrRet ovl_28ab08_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sky_shells_turn_triple(1, 2, 3);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028ac88 — Fleet / Veldin Orbit (L17) sky variant
TrRet ovl_28ac88_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(0);

    const uint32_t sky = lwu(OV(kSkyPtrAddr));
    if (lh(sky + 0x8) == 0) {
        sh(sky + 0x8, 0x100u);
        for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
            const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
            if (i < 240) {
                init_twinkle_star(s, 1u, 0u, true, 8u);
            } else {
                sb(s + 0x0, 0u);
                sh(s + 0xc, static_cast<uint16_t>(libc_rand() >> 16));
                sh(s + 0xe, static_cast<uint16_t>(libc_rand() >> 16));
                sb(s + 0x2, 1u);
                sb(s + 0x3, 0x48u);
                sw(s + 0x1c, 0x3e3851ecu);
                const uint32_t pal_off = static_cast<uint32_t>(libc_rand() >> 14) & 0xcu;
                sw(s + 0x4, lwu(OV(0x00160500u) + pal_off));
            }
        }
    }

    if (lh(lwu(OV(kSkyPtrAddr)) + 0x8) > 0) {
        for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x8); ++i) {
            const uint32_t s = lwu(lwu(OV(kSkyPtrAddr)) + 0x1c) + static_cast<uint32_t>(i << 5);
            if (lbu(s + 0x0) == 0u) {
                update_orbit_sprite(s, false);
            } else {
                update_twinkle_star_colour(s);
            }
        }
    }

    ovl_28bd58_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(kGsAlphaSky, kGsAlphaStarsVal);
    draw_shell_idx(1);
    draw_shell_idx(2);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028b080 — Veldin Finale (L18) sky variant
TrRet ovl_28b080_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    draw_shell_idx(0);
    draw_shell_idx(1);
    draw_shell_idx(2);
    draw_shell_idx(3);
    set_sky_yaw(yaw_from_mod(40000, 0x3924b5beu));
    draw_shell_idx(4);
    set_sky_yaw(yaw_from_mod(60000, 0x38db9cfeu));
    draw_shell_idx(5);
    mat4_identity(M(OV(kSkyMatAddr)));
    draw_shell_idx(6);
    return TrRet{0u, 0.0f};
}

// orig: 0x0028b1a0 — unused level-19 sky variant in Veldin's jump table (scaled shells with per-shell phase accumulators)
TrRet ovl_28b1a0_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    sh(lwu(OV(kSkyPtrAddr)) + 0x4, 0);
    mat4_identity(M(OV(kSkyMatAddr)));
    Vec4 rot{0.0f, 0.0f, 0.0f, 0.0f};
    if (lh(lwu(OV(kSkyPtrAddr)) + 0x6) <= 0) return TrRet{0u, 0.0f};

    for (int32_t i = 0; i < lh(lwu(OV(kSkyPtrAddr)) + 0x6); ++i) {
        const uint32_t shell = lwu(lwu(OV(kSkyPtrAddr)) + 0x20 + (static_cast<uint32_t>(i) << 2));
        const uint32_t phase_addr = shell + 0x8;
        float scale = 1.0f;
        const uint32_t rel = static_cast<uint32_t>(i) - 1u;
        if (rel < 5u) {
            const float dt = u2f(lwu(OV(0x001604c0u)));
            switch (rel) {
            case 0:
                rot.y = u2f(0x3ca3d70au);
                scale = 3.0f;
                sw(phase_addr, f2u(fpu_add(u2f(lwu(phase_addr)), fpu_mul(dt, u2f(0x3f0ccccdu)))));
                break;
            case 1:
                rot.y = u2f(0xbca3d70au);
                scale = 2.5f;
                sw(phase_addr, f2u(fpu_add(u2f(lwu(phase_addr)), fpu_mul(dt, u2f(0x3f19999au)))));
                break;
            case 2:
                rot.y = u2f(0x3c23d70au);
                scale = 2.0f;
                sw(phase_addr, f2u(fpu_add(u2f(lwu(phase_addr)), fpu_mul(dt, u2f(0x3f333333u)))));
                break;
            case 3:
                rot.y = u2f(0xbc23d70au);
                scale = 1.5f;
                sw(phase_addr, f2u(fpu_add(u2f(lwu(phase_addr)), fpu_mul(dt, 0.75f))));
                break;
            case 4:
                scale = 1.25f;
                sw(phase_addr, f2u(fpu_add(u2f(lwu(phase_addr)), fpu_mul(dt, u2f(0x3f4ccccdu)))));
                break;
            }
        } else {
            mat4_identity(M(OV(kSkyMatAddr)));
        }

        if (i > 0) {
            const float phase = fpu_add(u2f(lwu(phase_addr)), fpu_mul(static_cast<float>(i), 2000.0f));
            const int32_t rem = ee_rem(static_cast<int32_t>(f2u(eer::cvt_w(phase))), 10000);
            rot.z = fpu_sub(fpu_mul(static_cast<float>(rem), u2f(0x3a24b5beu)), kPi());
            euler_to_mat4_fpu(M(OV(kSkyMatAddr)), rot);
            vec4_scale(V(OV(kSkyMatAddr) + 0x00), V(OV(kSkyMatAddr) + 0x00), scale);
            vec4_scale(V(OV(kSkyMatAddr) + 0x10), V(OV(kSkyMatAddr) + 0x10), scale);
            vec4_scale(V(OV(kSkyMatAddr) + 0x20), V(OV(kSkyMatAddr) + 0x20), scale);
            V(OV(kSkyMatAddr) + 0x30) = V(OV(0x00160520u));
        }

        draw_shell_idx(static_cast<uint32_t>(i));
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x0028b680 — begin sky pass: reserve opening DMA link, reset GS texture allocator, clear cached TEX0 words
TrRet ovl_28b680_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    const uint32_t cur = lwu(OV(0x001611c0u));
    sw(OV(0x00160530u), cur);
    sw(OV(0x001611c0u), cur + 0x10);
    prof_mark_end(OV(0x00160510u), 1);

    const uint32_t sky = lwu(OV(kSkyPtrAddr));
    sw(0x15ee74u, lwu(0x15ee78u));
    sw(OV(0x0015f418u), 0u);
    for (int32_t i = 0; i < lh(sky + 0xc); ++i) {
        sd(lwu(sky + 0x10) + static_cast<uint32_t>(i << 4), 0ull);
    }
    return TrRet{0u, 0.0f};
}

// orig: 0x0028b710 — finish sky pass: splice queued texture uploads in front of the sky DMA chain
TrRet ovl_28b710_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    const uint32_t end_tag = lwu(OV(0x001611c0u));
    sw(OV(0x00160534u), end_tag);
    sw(OV(0x001611c0u), end_tag + 0x10);

    const uint32_t start_tag = lwu(OV(0x00160530u));
    const uint32_t uploads_head = lwu(OV(0x001611c0u));
    sw(start_tag + 0x0, 0x20000000u);
    sw(start_tag + 0x4, uploads_head);
    sw(start_tag + 0x8, 0u);
    sw(start_tag + 0xc, 0u);

    if (lwu(OV(0x00169ff4u)) != 0) {
        texture_uploads();
        packet_ref_1c2f00();
    }

    const uint32_t back_tag = lwu(OV(0x001611c0u));
    sw(back_tag + 0x0, 0x20000000u);
    sw(back_tag + 0x4, lwu(OV(0x00160530u)) + 0x10);
    sw(back_tag + 0x8, 0u);
    sw(back_tag + 0xc, 0u);
    sw(OV(0x001611c0u), back_tag + 0x10);

    const uint32_t final_end = lwu(OV(0x00160534u));
    sw(final_end + 0x0, 0x20000000u);
    sw(final_end + 0x4, lwu(OV(0x001611c0u)));
    sw(final_end + 0x8, 0u);
    sw(final_end + 0xc, 0u);
    sw(0x15ee74u, lwu(0x15ee78u));
    return TrRet{final_end, 0.0f};
}

// orig: 0x0023d968 — Veldin's 20-entry level sky dispatcher
void sky_render_tr(uint32_t) {
    ovl_28b680_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    using VariantFn = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float);
    static constexpr VariantFn kVariants[20] = {
        ovl_289330_tr,  // L0: Veldin
        ovl_289420_tr,  // L1: Novalis
        ovl_2895a0_tr,  // L2: Aridia
        ovl_2899a8_tr,  // L3: Kerwan
        ovl_289a90_tr,  // L4: Eudora
        ovl_289bd8_tr,  // L5: Rilgar
        ovl_289cb8_tr,  // L6: Blarg
        ovl_28a208_tr,  // L7: Umbris
        ovl_28a2f0_tr,  // L8: Batalia
        ovl_28a3a8_tr,  // L9: Gaspar
        ovl_28a480_tr,  // L10: Orxon
        ovl_28a620_tr,  // L11: Pokitaru
        ovl_28a7a0_tr,  // L12: Hoven
        ovl_28a920_tr,  // L13: Gemlik
        ovl_28a998_tr,  // L14: Oltanis
        ovl_28aa98_tr,  // L15: Quartu
        ovl_28ab08_tr,  // L16: Kalebo
        ovl_28ac88_tr,  // L17: Fleet
        ovl_28b080_tr,  // L18: Veldin Finale
        ovl_28b1a0_tr,  // L19: unused slot
    };

    const uint32_t level = lwu(0x15ed84u);
    if (level < 20u) {
        kVariants[level](0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        ovl_2892d0_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    ovl_28b710_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(0x47u, 0x5360bull);
    sub_002a0ba8(0x4eu, 0x01000000ull | static_cast<uint64_t>(static_cast<int64_t>(lw(0x15ee88u) >> 13)));
}

}  // namespace rac1
