// Gaspar's own draw of its lava (class 317's draw callback): the lava's streams (strips of quads with an animated, blended
// texture) and its pools (an ocean record: the shared port ocean_draw_one, aridia_ocean.cpp), then the pools'
// edges — hand-ported from Gaspar's own overlay. Header gaspar_mobys.h, registered in gaspar_mobys.cpp. Addresses are
// Gaspar's (`orig: L9:`): the engine's data at Gaspar's places are L9_ADDR (the packet cursor 0x1611c0, the camera 0x166f40
// and its matrices 0x166e40 / 0x166ec0, the view 0x16cbc0, the textures' table 0x16d300 and data 0x15f420), the base ELF's
// are bare (the VRAM cursor 0x15ee74, the packet head 0x10e800).
//
// The texture blend L9:0x26d240 the streams call is Gaspar's copy of the ocean's palette blend (relocations only): the shared
// port ari_ocean_tex_upload_l2 (aridia_ocean.cpp, with the host's texel loop of palette_blend.h; batch 513's ocean_tex_upload at
// L9). The pools' draw L9:0x002c1920 (with its set-up 0x2c1220) is Gaspar's copy of the ocean's one-record draw, relocations only
// (batch 513's ocean_draw_one at L9): the shared port, whose set-up blends the pools' texture with the same port.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_ocean.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
uint32_t cursor() { return lwu(L9_ADDR(0x001611c0)); }
void set_cursor(uint32_t a) { sw(L9_ADDR(0x001611c0), a); }

// a DMA tag at the cursor (`w0`, `w1`, 0, `w3`), the cursor past it
void tag(uint32_t w0, uint32_t w1, uint32_t w3) {
    sw(cursor(), w0);
    sw(cursor() + 4, w1);
    sw(cursor() + 8, 0);
    sw(cursor() + 0xc, w3);
}

// VU1 program 7's constants at the cursor (the head of the packet 0x10e800 sent first when `ref`): the two camera matrices
// (`world`, `view`) times the 1024 scale about the camera, `zoff` added to their z translation, the fans' tag, the screen's
// scale / offset (vf29 / vf30) and fog, OFFSET / BASE / MSCALF
void vu7_constants(uint32_t sp, uint32_t world, uint32_t view_m, bool zoff) {
    const uint32_t cam = L9_ADDR(0x00166f40), view = L9_ADDR(0x0016cbc0);
    gas_mat_scale_l9(sp, 1024.0f);
    vec3_scale(V(sp + 0x30), V(cam), -1024.0f);
    swc1(sp + 0x3c, 1.0f);
    sw(cursor(), 0x10000000u);
    sw(cursor() + 4, 0);
    sw(cursor() + 8, 0x11000000u);
    sw(cursor() + 0xc, 0x01000404u);
    const uint32_t s0 = cursor();
    sw(s0 + 0x10, 0);
    sw(s0 + 0x14, 0);
    sw(s0 + 0x18, 0);
    sw(s0 + 0x1c, 0x6c0c43a4u);
    mat4_mul(M(s0 + 0x20), M(world), M(sp));
    if (zoff) swc1(s0 + 0x58, fpu_add(lwc1(s0 + 0x58), lwc1(L9_ADDR(0x0015f308))));
    mat4_mul(M(s0 + 0x60), M(view_m), M(sp));
    if (zoff) swc1(s0 + 0x98, fpu_add(lwc1(s0 + 0x98), lwc1(L9_ADDR(0x0015f308))));
    sw(s0 + 0xa0, 0x8000u);
    sw(s0 + 0xa4, 0x303ec000u);
    sw(s0 + 0xa8, 0x412u);
    swc1(s0 + 0xac, lwc1(view + 0x210));
    lq_sq(s0 + 0xb0, view + 0x190);
    lq_sq(s0 + 0xc0, view + 0x1a0);
    swc1(s0 + 0xd0, lwc1(view + 0x22c));
    sw(s0 + 0xe4, 0x020001d2u);
    swc1(s0 + 0xd4, lwc1(view + 0x228));
    sw(s0 + 0xe8, 0x15000000u);
    sw(s0 + 0xe0, 0x03000000u);
    sw(s0 + 0xd8, 0);
    sw(s0 + 0xdc, 0);
    sw(s0 + 0xec, 0);
    const uint32_t end = s0 + 0xf0;
    sw(s0, lwu(s0) | static_cast<uint32_t>((static_cast<int32_t>(end - s0) >> 4) - 1));
    set_cursor(end);
}

// the VRAM cursor's texture base word (TEX0's TBP / TBW / PSM part)
uint64_t vram_tex0() {
    return static_cast<uint64_t>(static_cast<int64_t>(lw(0x0015ee74) >> 8)) | 0x18100000ull | (0xb000ull << 19);
}
}  // namespace

// A scale matrix at `out` (an EE address): `s` on the diagonal's first three, 1 last; VU0 left as the original leaves it.
// orig: L9:0x0021f6f8
void gas_mat_scale_l9(uint32_t out, float s) {
    V(out) = Vec4{s, 0.0f, 0.0f, 0.0f};
    V(out + 0x10) = Vec4{0.0f, s, 0.0f, 0.0f};
    V(out + 0x20) = Vec4{0.0f, 0.0f, s, 0.0f};
    V(out + 0x30) = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    mat4_scale_diag_vu0(s);  // (the VU0 registers the original leaves: vf1..vf5, shadows.cpp; the moby pass reads vf5's w lane)
}

// VU1 program 7's constants for the lava's streams (the packet's head sent once, the level's 0x15f5e0 noting it), the
// level's z offset 0x15f308 added to both matrices.
// orig: L9:0x0021baf8
void gas_21baf8_l9() {
    const uint32_t sp = ee_stack_alloc(0x40);
    if (lw(L9_ADDR(0x0015f5e0)) != 7) {
        packet_ref(0x0010e810, lhu(0x0010e800));
        sw(L9_ADDR(0x0015f5e0), 7);
    }
    vu7_constants(sp, L9_ADDR(0x00166f40) - 0x100, L9_ADDR(0x00166f40) - 0x80, true);
    packet_ref_13cf10();
    ee_stack_free(0x40);
}

// The streams' texture: frame `a` of the level's animated textures, or, between two frames (`b` the next), their blend by
// `blend` uploaded to 0x161fd0 (the palette blend L9:0x26d240: ari_ocean_tex_upload_l2) and sent to VRAM at its cursor (moved 0x4000 on), its
// TEX0 / TEX1 / CLAMP / the alpha words, the texture flushed.
// orig: L9:0x0021e560
void gas_21e560_l9(int32_t a, int32_t b, float blend) {
    if (a == b) {
        tag(0x10000002u, 0, 0x50000002u);
        const uint32_t s1 = cursor(), s0 = s1 + 0x10;
        set_cursor(s0);
        sd(s1 + 0x10, 0x1000000000008001ull);
        sd(s0 + 8, 0x0eeeeeeeull);
        sd(s0 + 0x10, effect_tex0(b));
        sd(s0 + 0x18, 6);
        set_cursor(s1 + 0x30);
        return;
    }
    const uint32_t tbl = L9_ADDR(0x0016d300), data = lwu(L9_ADDR(0x0015f420));
    const uint32_t ea = tbl + (static_cast<uint32_t>(a) << 4), eb = tbl + (static_cast<uint32_t>(b) << 4);
    const uint32_t buf = L9_ADDR(0x00161fc0);
    sd(buf, 0x0800000000000400ull);
    sd(buf + 8, 0);
    ari_ocean_tex_upload_l2(buf + 0x10, data + (lhu(ea + 8) << 4), data + (lhu(ea + 0xa) << 4), data + (lhu(eb + 8) << 4),
                            data + (lhu(eb + 0xa) << 4), 0x1000, blend);
    tag(0x10000006u, 0, 0x50000006u);
    const uint32_t a2 = cursor(), a1 = a2 + 0x10;
    set_cursor(a1);
    sd(a2 + 0x10, 0x5000000000008001ull);
    sd(a1 + 8, 0x0eeeeeeeull);
    sd(a1 + 0x18, 6);
    sd(a1 + 0x10, vram_tex0());
    sd(a1 + 0x58, 0x53);
    sd(a1 + 0x28, 0x50);
    sd(a1 + 0x38, 0x51);
    sd(a1 + 0x40, (0x8000ull << 23) | 0x40);
    sd(a1 + 0x20, (static_cast<uint64_t>(static_cast<int64_t>(lw(0x0015ee74)) >> 8) << 32) | (0x8000ull << 33));
    sd(a1 + 0x48, 0x52);
    sd(a1 + 0x30, 0);
    sd(a1 + 0x50, 0);
    set_cursor(a2 + 0x70);
    sw(a2 + 0x70, 0x30000401u);
    sw(cursor() + 4, buf);
    sw(cursor() + 8, 0);
    sw(cursor() + 0xc, 0x50000401u);
    set_cursor(cursor() + 0x10);
    tag(0x10000002u, 0, 0x50000002u);
    const uint32_t f = cursor();
    set_cursor(f + 0x10);
    sd(f + 0x10, 0x1000000000008001ull);
    sd(f + 0x28, 0x3f);
    sd(f + 0x18, 0xe);
    sd(f + 0x20, 0);
    set_cursor(f + 0x30);
    sub_002a0ba8(7, vram_tex0());
    sw(0x0015ee74, lwu(0x0015ee74) + 0x4000);
}

// The lava's streams: `n` records of 0x40 bytes at `recs` (a centre +0, the positions +0x10 (12 bytes each), the uv pairs
// +0x14, the strips' count +0x1c and their first vertices +0x20 (halfwords, the count + 1)): the texture (frames `a` / `b`
// blended by `blend`), the texture's VIF tag (0x16eaa0), the constants; the scratchpad's 256 colours (0x80787070 plus 0x20200
// times a running 0x89 step's low 4 bits); each record on screen (within 256) drawn strip by strip with two uv sets, the
// flowing offsets (the frame count's low 11 bits over 2048, from 1 down: once and twice that plus 0.5) added to v.
// orig: L9:0x0021e8c0
void gas_21e8c0_l9(uint32_t recs, int32_t n, int32_t a, int32_t b, float blend) {
    gas_21e560_l9(a, b, blend);
    tag(0x30000009u, L9_ADDR(0x0016eaa0), 0x50000009u);
    set_cursor(cursor() + 0x10);
    gas_21baf8_l9();
    const float t = fpu_mul(int_to_float(static_cast<int32_t>(lwu(L9_ADDR(0x0015f5cc)) & 0x7ff)), u2f(0x3a000000u));
    const float flow = fpu_sub(1.0f, t);
    const float flow2 = fpu_add(fpu_add(flow, flow), 0.5f);
    uint32_t k = 0;
    for (uint32_t i = 0; i < 0x100; ++i, k += 0x89) sw(0x70000000u + i * 4, (k & 0xf) * 0x20200u + 0x80787070u);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t rec = recs + (static_cast<uint32_t>(i) << 6);
        const int64_t vis = point_on_screen(rec, 256.0f);
        if (vis == -1 || lw(rec + 0x1c) <= 0) continue;
        const uint32_t clip = vis == 0 ? 1u : 0u;
        const uint32_t firsts = rec + 0x20;
        for (int32_t j = 0; j < lw(rec + 0x1c); ++j) {
            const int32_t from = lh(firsts + static_cast<uint32_t>(j) * 2), to = lh(firsts + static_cast<uint32_t>(j + 1) * 2);
            const int32_t count = to - from;
            uint32_t src = lwu(rec + 0x14) + (static_cast<uint32_t>(from) << 3);
            uint32_t st = 0x70001000u, st2 = 0x70002000u;
            for (int32_t v = 0; v < count; ++v, src += 8, st += 8, st2 += 8) {
                const float u = lwc1(src);
                swc1(st2, u);
                swc1(st, u);
                swc1(st + 4, fpu_add(lwc1(src + 4), flow));
                swc1(st2 + 4, fpu_add(lwc1(src + 4), flow2));
            }
            nov_prim_list2_l1(count, lwu(rec + 0x10) + static_cast<uint32_t>(lh(firsts + static_cast<uint32_t>(j) * 2) * 12), 0x70000000u, 0x70001000u,
                              0x70002000u, clip);
        }
    }
}

// The lava pools' edges, drawn as an ocean record's (`rec`): the view's fog / far words saved (0x1612f8..) and taken from the
// record (+0x20..) unless the fog mode (0x15f458) is 1, the projection, the constants (the packet's head always), the record's
// texture (the VIF tag 0x1cb470) and its GS words (its alpha +0x3f, its fog colour +0x30 or the view's), the scratchpad's 256
// colours 0x757c8e; then `n` strips of 0x20 bytes at `strips` (a centre +0, positions +0x10, uv pairs +0x14 halved, count
// +0x1c) on screen within 256; the record's end.
// orig: L9:0x002c1978
void gas_2c1978_l9(uint32_t rec, uint32_t strips, int32_t n) {
    const uint32_t view = L9_ADDR(0x0016cbc0);
    const int32_t fog_mode = lw(L9_ADDR(0x0015f458));
    swc1(L9_ADDR(0x001612f8), lwc1(L9_ADDR(0x0016cdd8)));
    swc1(L9_ADDR(0x001612fc), lwc1(L9_ADDR(0x0016cddc)));
    swc1(L9_ADDR(0x00161300), lwc1(L9_ADDR(0x0016cde8)));
    swc1(L9_ADDR(0x00161304), lwc1(L9_ADDR(0x0016cdec)));
    if (fog_mode != 1) {
        swc1(L9_ADDR(0x0016cdd8), lwc1(rec + 0x20));
        swc1(L9_ADDR(0x0016cddc), lwc1(rec + 0x24));
        swc1(L9_ADDR(0x0016cde8), lwc1(rec + 0x28));
        swc1(L9_ADDR(0x0016cdec), lwc1(rec + 0x2c));
    }
    projection_setup();
    render_view_setup();
    const uint32_t sp = ee_stack_alloc(0x40);
    packet_ref(0x0010e810, lhu(0x0010e800));
    vu7_constants(sp, L9_ADDR(0x00166e40), L9_ADDR(0x00166ec0), false);
    ee_stack_free(0x40);
    sw(cursor(), 0x30000003u);
    sw(cursor() + 4, L9_ADDR(0x001cb470));
    sw(cursor() + 8, 0);
    sw(cursor() + 0xc, 0x50000003u);
    set_cursor(cursor() + 0x10);
    sw(cursor(), 0x10000008u);
    sw(cursor() + 4, 0);
    sw(cursor() + 8, 0);
    sw(cursor() + 0xc, 0x50000008u);
    const uint32_t t2 = cursor(), t4 = t2 + 0x10;
    const int32_t mode = lw(L9_ADDR(0x0015f458));
    set_cursor(t4);
    uint64_t fog = static_cast<uint64_t>(lbu(rec + 0x30)) | (static_cast<uint64_t>(lbu(rec + 0x31)) << 8) |
                   (static_cast<uint64_t>(lbu(rec + 0x32)) << 16);
    if (mode == 1) {
        fog = static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x230))) |
              static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x234) << 8)) |
              static_cast<uint64_t>(static_cast<int64_t>(lw(view + 0x238) << 16));
    }
    const uint32_t alpha = lbu(rec + 0x3f);
    sd(t2 + 0x10, 0x7000000000008001ull);
    sd(t4 + 8, 0xeeeeeeeeull);
    sd(t4 + 0x18, 0x42);
    sd(t4 + 0x10, (static_cast<uint64_t>(alpha) << 32) | 0x64);
    sd(t4 + 0x20, ld(L9_ADDR(0x001612f0)));
    sd(t4 + 0x28, 6);
    sd(t4 + 0x78, 0x47);
    sd(t4 + 0x30, fog);
    sd(t4 + 0x38, 0x3d);
    sd(t4 + 0x48, 8);
    sd(t4 + 0x50, 0x50000);
    sd(t4 + 0x60, (0xff90ull << 32) | 0x260);
    sd(t4 + 0x68, 0x14);
    sd(t4 + 0x70, 0x60u < alpha ? 0x5360aull : 0x5370bull);
    sd(t4 + 0x40, 0);
    sd(t4 + 0x58, 0x47);
    set_cursor(t2 + 0x90);
    for (uint32_t a = 0x700003fcu; a >= 0x70000000u; a -= 4) sw(a, 0x00757c8eu);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t s = strips + (static_cast<uint32_t>(i) << 5);
        const int64_t vis = point_on_screen(s, 256.0f);
        if (vis == -1) continue;
        const int32_t count = lw(s + 0x1c);
        uint32_t src = lwu(s + 0x14), st = 0x70001000u;
        const uint32_t clip = vis == 0 ? 1u : 0u;
        for (int32_t v = 0; v < count; ++v, src += 8, st += 8) {
            swc1(st, fpu_mul(lwc1(src), 0.5f));
            swc1(st + 4, fpu_mul(lwc1(src + 4), 0.5f));
        }
        prim_list_packet(count, lwu(s + 0x10), 0x70000000u, 0x70001000u, clip);
    }
    ari_ocean_end_body_l2(rec);
}

// Class 317's draw: the lava's streams (the 0x6c records at 0x1f3080: the texture frames 44 .. 59 by the frame count over 20,
// blended by the rest), the pools (the ocean record 0x1fc740) and their edges (the 10 strips at 0x1f63c0).
// orig: L9:0x002ef750
void gas_class317_draw_l9(uint32_t) {
    const int32_t frame = lw(L9_ADDR(0x0015f5cc));
    const int32_t k = ee_div(frame, 20);
    const float blend = fpu_sub(fpu_div(int_to_float(frame), 20.0f), int_to_float(k));
    gas_21e8c0_l9(L9_ADDR(0x001f3080), 0x6c, (k & 0xf) + 0x2c, ((k + 1) & 0xf) + 0x2c, blend);
    ocean_draw_one(L9_ADDR(0x001fc740));  // (L9:0x2c1920, the shared port)
    gas_2c1978_l9(L9_ADDR(0x001fc740), L9_ADDR(0x001f63c0), 0xa);
}

}  // namespace rac1
