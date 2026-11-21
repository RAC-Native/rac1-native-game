// Ported overlay code: the sparkles' draw (see sparkles_draw.h).
#include <game/ovl/sparkles_draw.h>

#include <game/ovl/blarg_call.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

namespace {
// orig: (helper) a 32-bit result sign-extended into a 64-bit register
uint64_t sx(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
}  // namespace

// Each live slot: the camera-relative position projected (0x166cc0, the guard band 0x16cbc0:
// outside it the slot is cleared), its screen x / y / z in GS units (+0x8000, z scaled 0.99969 +
// the Z offset 0x16cbe8), its size by the alpha and by the distance (clamped 2..18: farther is
// smaller), turned by +0x18; the quad's four corners as XYZ2 words, colour (0x80, 0x80, 0x80,
// alpha).
// orig: 0x001fead8
void sparkles_draw() {
    sub_002a0ba8(0x42, 0x8000000048ull);
    constexpr OvConst<0x0016ca40u> S{};
    const float k16 = 16.0f;
    for (int32_t i = 0; i < 0x10; ++i) {
        const uint32_t e = OV(0x0016ea40) + static_cast<uint32_t>(i) * 0x20;
        if (lh(e + 0x10) <= 0) continue;
        Vec4 p;
        vec3_sub(p, V(e), V(OV(0x00166dc0)));
        p.w = 1.0f;
        float dist = vec3_length(p);
        vec3_scale(p, p, 1024.0f);
        mat4_transform(p, p, M(OV(0x00166cc0)));
        Vec4 g;
        vec4_mul(g, p, V(OV(0x0016cbc0)));
        if (clip_outcode(g) != 0) {
            mem_fill32(e, 0, 0x20);
            continue;
        }
        vec3_scale(p, p, fpu_div(lwc1(S + 0x210), p.w));
        const uint64_t rgba = sx(static_cast<int32_t>(static_cast<uint32_t>(lh(e + 0x12)) << 24) | 0x808080);
        const uint64_t x = sx(float_to_int(fpu_mul(p.x, k16)) + 0x8000);
        const uint64_t y = sx(float_to_int(fpu_mul(p.y, k16)) + 0x8000);
        const uint64_t z = static_cast<uint32_t>(float_to_int(fpu_add(fpu_mul(p.z, u2f(0x3f7fec57u)), lwc1(S + 0x1a8))));
        const uint64_t xyz = (z << 32) | (y << 16) | x;
        if (18.0f < dist) dist = 18.0f;
        else if (dist < 2.0f) dist = 2.0f;
        const float a = fpu_mul(int_to_float(lh(e + 0x12) + 0x10), 0.015625f);
        float size = fpu_mul(lwc1(e + 0x1c), a);
        size = fpu_mul(size, fpu_mul(fpu_sub(24.0f, dist), k16));
        const int32_t sn = float_to_int(fpu_mul(size, sin_vu(lwc1(e + 0x18))));
        const int32_t cs = float_to_int(fpu_mul(size, cos_vu(lwc1(e + 0x18))));
        // the packet: CNT 9 + DIRECT 9, the GIF tag 0x1609a0, then the registers
        const uint32_t r = lwu(OV(0x001611c0));
        sw(r, 0x10000009);
        sw(r + 4, 0);
        sw(r + 8, 0);
        sw(r + 0xc, 0x50000009);
        sw(OV(0x001611c0), r + 0x10);
        lq_sq(r + 0x10, OV(0x001609a0));
        const uint32_t d = r + 0x20;
        sw(OV(0x001611c0), d);
        sd(d, 5);
        const uint64_t tex0 = effect_tex0(0x13);
        const uint64_t c0 = sx(cs << 16) + xyz + static_cast<uint64_t>(static_cast<int64_t>(sn));
        const uint64_t c1 = sx((0 - sn) << 16) + xyz + sx(cs);
        const uint64_t c2 = sx(sn << 16) + xyz - sx(cs);
        const uint64_t c3 = sx((0 - cs) << 16) + xyz - sx(sn);
        sd(d + 0x08, tex0);
        sd(d + 0x10, 0x154);
        sd(d + 0x28, c0);
        sd(d + 0x38, 0x200);
        sd(d + 0x40, c1);
        sd(d + 0x50, 0x2000000);
        sd(d + 0x58, c2);
        sd(d + 0x60, rgba);
        sd(d + 0x68, 0x2000200);
        sd(d + 0x70, c3);
        sd(d + 0x78, 0);
        sd(d + 0x18, rgba);
        sd(d + 0x20, 0);
        sd(d + 0x30, rgba);
        sd(d + 0x48, rgba);
        sw(OV(0x001611c0), lwu(OV(0x001611c0)) + 0x80);
    }
    sub_002a0ba8(0x42, 0x8000000044ull);
}

}  // namespace rac1
