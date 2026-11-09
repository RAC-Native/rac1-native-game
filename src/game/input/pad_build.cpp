// Ported: the pad state the game reads (0x13c940), built from a controller packet.
#include <game/enhance/advanced.h>
#include <runtime/ee/ps2_fpu.h>
#include <game/input/pad_build.h>
#include <game/ovl/ovl_common.h>
#include <host/mod_code.h>
#include <host/quick_rings.h>

namespace rac1 {

namespace {
// libpad's analog axis: 0 inside 0x30 of the centre 0x7f, else (|v - 0x7f| - 0x30) / 0x4c up to 1
// orig: (helper) the stick loop of 0x002671b0
float pad_axis(uint32_t v) {
    int32_t a = static_cast<int32_t>(v) - 0x7f;
    if (!(-1 < a)) a = -a;
    if (a < 0x30) return 0.0f;
    float f = ps2fpu::div_nearest(int_to_float(a - 0x30), int_to_float(0x4c));
    if (1.0f < f) f = 1.0f;
    if (v < 0x7f) f = -f;
    return f;
}

// left and right swapped (the mirror flag 0x15edb4): d-pad left 0x8000 <-> right 0x2000
// orig: (helper) repeated in 0x002671b0
uint32_t swap_lr(uint32_t b) {
    if ((b & 0x8000u) != 0) return (b & 0xffff7fffu) | 0x2000u;
    if ((b & 0x2000u) != 0) return (b & ~0x2000u) | 0x8000u;
    return b;
}

}  // namespace

// Builds the pad state from a controller packet (data: 2 button bytes active low, then the right
// and left sticks, then 12 pressures when len >= 0x12): the held buttons +0x1a0 / +0x1b0 (d-pad in
// the high byte, the face / shoulder buttons in the low one), the sticks +0x100..+0x10c (right x / y,
// left x / y) and pressures +0x110.., copied to +0x140..; the left stick also sets the d-pad bits
// of +0x1a0; with the mirror flag 0x15edb4 the x axes and left / right are swapped; the edges
// +0x1a4 / +0x1c4 (pressed), +0x1a8 / +0x1c8 (released), +0x1b4 / +0x1b8 (raw edges), +0x1c0 and
// +0x348 the held set, +0x1d0 / +0x1d4 none held / no d-pad; the masks of modes 1 / 2 (+0x1cc);
// the stick history (+0x2d0 lengths, +0x258 angles, 30 frames at +0x18e) with the flick bit
// 0x10000 (pushed past 0.9 from under 0.25 within 4 frames, or swung past 0.96 within 5); the
// pressed history +0x1e0 (30 words, count +0x190); the auto-repeat of held buttons (+0x34d..f).
// orig: 0x002671b0 (the overlay's copy of the base ELF's 0x00217328)
void pad_build(uint32_t pad, const uint8_t* data, int32_t len) {
    // (enhancement: the modern controls remap L2 / R2 in gameplay; with three quick select rings, the shoulder buttons that
    // switch the ring are not the game's, src/host/quick_rings.h)
    const uint32_t word = advanced_pad_remap(pad, quick_rings_pad(pad, ((static_cast<uint32_t>(data[0]) << 8) | data[1]) ^ 0xffffu));
    sw(pad + 0x1b0, word);
    sw(pad + 0x1a0, word);
    for (uint32_t o = 0x13c; o >= 0x100; o -= 4) sw(pad + o, 0);
    if (!(len < 6)) {
        for (uint32_t i = 0; i < 4; ++i) swc1(pad + 0x100 + 4 * i, pad_axis(data[2 + i]));
    }
    if (!(len < 0x12)) {
        for (uint32_t i = 4; i < 0x10; ++i) swc1(pad + 0x100 + 4 * i, int_to_float(static_cast<int32_t>(data[2 + i])) * u2f(0x3b808081u));
    }
    const bool mirror = lbu(0x0015edb4) != 0;
    if (mirror) {
        swc1(pad + 0x108, -lwc1(pad + 0x108));
        swc1(pad + 0x100, -lwc1(pad + 0x100));
        sw(pad + 0x1a0, swap_lr(lwu(pad + 0x1a0)));
        sw(pad + 0x1b0, lwu(pad + 0x1a0));
    }
    for (uint32_t i = 0; i < 0x10; ++i) swc1(pad + 0x140 + 4 * i, lwc1(pad + 0x100 + 4 * i));
    sw(pad + 0x1d8, lwc1(pad + 0x108) != 0.0f || lwc1(pad + 0x10c) != 0.0f ? 1u : 0u);
    if (lwc1(pad + 0x108) < 0.0f) sw(pad + 0x1a0, lwu(pad + 0x1a0) | 0x8000u);
    if (0.0f < lwc1(pad + 0x108)) sw(pad + 0x1a0, lwu(pad + 0x1a0) | 0x2000u);
    if (lwc1(pad + 0x10c) < 0.0f) sw(pad + 0x1a0, lwu(pad + 0x1a0) | 0x1000u);
    if (0.0f < lwc1(pad + 0x10c)) sw(pad + 0x1a0, lwu(pad + 0x1a0) | 0x4000u);
    const uint32_t held = lwu(pad + 0x1a0), prev = lwu(pad + 0x1ac), prev_raw = lwu(pad + 0x1bc), raw = lwu(pad + 0x1b0);
    sw(pad + 0x1d4, (held & 0xf000u) == 0 ? 1u : 0u);
    sw(pad + 0x1d0, held == 0 ? 1u : 0u);
    sw(pad + 0x1b4, ~prev & raw);
    sw(pad + 0x1b8, ~held & prev_raw);
    sw(pad + 0x1c4, ~prev & held);
    sw(pad + 0x1c8, ~held & prev);
    sw(pad + 0x1a4, ~prev & held);
    sw(pad + 0x1a8, ~held & prev);
    sw(pad + 0x1c0, held);
    sw(pad + 0x348, held);
    if (mirror) {
        swc1(pad + 0x148, -lwc1(pad + 0x148));
        swc1(pad + 0x140, -lwc1(pad + 0x140));
        sw(pad + 0x1c0, swap_lr(held));
        sw(pad + 0x1c4, swap_lr(lwu(pad + 0x1c4)));
        sw(pad + 0x1c8, swap_lr(lwu(pad + 0x1c8)));
    }
    if (lw(pad + 0x1cc) == 1) {
        constexpr uint32_t keep = 0xffffafcfu;
        sw(pad + 0x1a0, lwu(pad + 0x1a0) & keep);
        sw(pad + 0x1a4, lwu(pad + 0x1a4) & keep);
        sw(pad + 0x1a8, lwu(pad + 0x1a8) & keep);
        sw(pad + 0x100, 0);
        sw(pad + 0x104, 0);
        sw(pad + 0x1cc, 0);
    }
    if (lw(pad + 0x1cc) == 2) {
        sw(pad + 0x1a0, lwu(pad + 0x1a0) & 0x900u);
        sw(pad + 0x1a4, lwu(pad + 0x1a4) & 0x900u);
        sw(pad + 0x1a8, lwu(pad + 0x1a8) & 0x900u);
        sw(pad + 0x1b0, lwu(pad + 0x1b0) & 0x900u);
        sw(pad + 0x1d4, 1);
        sw(pad + 0x108, 0);
        sw(pad + 0x10c, 0);
        sw(pad + 0x1cc, 0);
    }
    Vec4 st = V(pad + 0x100);
    st.x = lwc1(pad + 0x108);
    st.y = lwc1(pad + 0x10c);
    const float mag = vec2_length(st);
    const float ang = atan2_f(st.x, st.y);
    const uint32_t at = static_cast<uint32_t>(lh(pad + 0x18e)) * 4;
    swc1(pad + 0x2d0 + at, mag);
    swc1(pad + 0x258 + at, ang);
    if (u2f(0x3f666666u) < mag) {  // a flick: from under 0.25 within 4 frames, or swung within 5
        for (int32_t k = 1; k < frame_scale_i(4); ++k) {
            const float h = lwc1(pad + 0x2d0 + static_cast<uint32_t>(ee_rem(lh(pad + 0x18e) - k + 0x1e, 0x1e)) * 4);
            if (u2f(0x3f666666u) < h) break;
            if (h < 0.25f) {
                sw(pad + 0x1a4, lwu(pad + 0x1a4) | 0x10000u);
                break;
            }
        }
        if ((lwu(pad + 0x1a4) & 0x10000u) == 0) {
            for (int32_t k = 1; k < frame_scale_i(5); ++k) {
                const float h = lwc1(pad + 0x258 + static_cast<uint32_t>(ee_rem(lh(pad + 0x18e) - k + 0x1e, 0x1e)) * 4);
                if (u2f(0x3f75be0bu) < angle_dist(h, ang)) {
                    sw(pad + 0x1a4, lwu(pad + 0x1a4) | 0x10000u);
                    break;
                }
            }
        }
    }
    sw(pad + 0x1e0 + static_cast<uint32_t>(lh(pad + 0x18e)) * 4, lwu(pad + 0x1a4));
    const int32_t n = lw(pad + 0x190) + 1;
    sw(pad + 0x190, static_cast<uint32_t>(n));
    sh(pad + 0x18e, static_cast<uint32_t>(ee_rem(lh(pad + 0x18e) + 1, 0x1e)));
    if (!(n < 0x1f)) sw(pad + 0x190, 0x1e);
    const uint32_t rep = lbu(pad + 0x34e);  // the auto-repeat of a held button set
    if (rep == 0) return;
    const uint32_t h = lwu(pad + 0x1a0);
    if (h == 0 || h != lwu(pad + 0x1ac)) {
        sb(pad + 0x34f, lbu(pad + 0x34d));
        return;
    }
    const uint32_t left = (lbu(pad + 0x34f) - 1u) & 0xffu;
    sb(pad + 0x34f, left);
    if (left != 0 && left != 0xff) return;
    sw(pad + 0x1a4, h);
    sb(pad + 0x34f, rep);
}

// The per-frame pad read with the controller ready (the 0x266f50 path where libpad reports a
// stable pad in mode 1): the frame's held sets cleared and kept as the previous ones (+0x1ac from
// +0x348, +0x1bc from +0x1b0), the packet built, the busy flags +0x188 / +0x189 cleared. The libpad
// state / mode queries are the host's: `data` is the
// packet the pad would return.
// orig: 0x00266f50 (the overlay's copy of the base ELF's 0x002170c8)
void pad_read_packet(uint32_t pad, const uint8_t* data, int32_t len) {
    if (g_mod_code_any) mod_code_pad_before(pad);  // enhancement: a plugin's input_hold (src/host/mod_code.h)
    sw(pad + 0x1a0, 0);
    const uint32_t raw = lwu(pad + 0x1b0);
    const uint32_t last = lwu(pad + 0x348);
    sw(pad + 0x1b0, 0);
    sw(pad + 0x1ac, last);
    sw(pad + 0x1bc, raw);
    sw(pad + 0x19c, 1);
    sw(pad + 0x198, 1);
    pad_build(pad, data, len);
    sb(pad + 0x189, 0);
    sb(pad + 0x188, 0);
    if (g_mod_code_any) mod_code_pad_after(pad);  // enhancement: the real pad kept for the plugins, input_hold's mask
}

}  // namespace rac1
