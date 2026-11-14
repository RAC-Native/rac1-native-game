// Ported overlay code 0x001e0000..0x001fffff (excluding the math library, see game/math/vecmath.cpp, and the collision
// routines, see game/collision/).
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: 0x001eb1f8 — linear search of the 48-entry table at 0x167150 (stride 0xa0) by the s16 at +0x86.
uint32_t moby_table_find_by_uid(int32_t uid) {
    const uint32_t end = g::MOBY_TABLE_167150 + 0x1e00;
    uint32_t p = g::MOBY_TABLE_167150;
    int32_t v = lh(p + 0x86);
    for (;;) {
        if (v == uid) return p;
        p += 0xa0;
        if (!(static_cast<int32_t>(p) < static_cast<int32_t>(end))) return 0;
        v = lh(p + 0x86);
    }
}

// orig: 0x001eb078 — printf-style stub: spills its varargs to the stack and returns.
void debug_printf_stub() {}

// orig: 0x001f35d8
int32_t sub_001f35d8() {
    const int32_t v = lw(OV(0x00173e5c));
    if (v < 0) return -1;
    const int32_t m = v & 0x1f;
    return m == 0x1f ? -1 : m;
}

// orig: 0x001f3608
int32_t sub_001f3608() {
    const int32_t v = lw(OV(0x00173e5c));
    if (v < 0) return 0;
    const int32_t m = v & 0x60;
    return m == 0x60 ? 0 : (m >> 5);
}

// orig: 0x001f6c20 — empty in this build.
void sub_001f6c20() {}

// orig: 0x001f6c30 — empty in this build (takes a position and a radius).
void sub_001f6c30(uint32_t, float) {}

// orig: 0x001f93e0 — append (a, b) to two parallel 64-entry arrays (count at 0x15f424).
void sub_001f93e0(uint32_t a, uint32_t b) {
    const int32_t n = lw(OV(0x0015f424));
    if (!(n < 0x40)) return;
    sw(OV(0x0016d880) + n * 4, a);
    sw(OV(0x0016d980) + n * 4, b);
    sw(OV(0x0015f424), static_cast<uint32_t>(n + 1));
}

// orig: 0x001eb128 — copy the hero moby (*(gp+0x200), 0xa0 bytes) to 0x168f50 and its
// 0x280-byte block 0x169110 to 0x169610, then point the copy's +0x70 at the latter.
void sub_001eb128() {
    const uint32_t copy = OV(0x00168f50), blk = OV(0x00169610);
    mem_copy128(copy, lwu(OV(0x00166e00)), 0xa0);
    mem_copy128(blk, blk - 0x500, 0x280);
    sw(copy + 0x70, blk);
}

// orig: 0x001ed630 — yaw/pitch/distance from b to a: out = {yaw, pitch, dist}.
// (The original also calls angle_sub(yaw, old out.x) and discards the result.)
int32_t sub_001ed630(uint32_t out, uint32_t a, uint32_t b) {
    Vec4 d;
    vec3_sub(d, V(a), V(b));
    const float len2 = vec2_length(d);
    const float pitch = atan2_f(len2, d.z);
    swc1(out + 4, pitch);
    const float yaw = atan2_f(d.x, d.y);
    (void)angle_sub(yaw, lwc1(out + 0));
    swc1(out + 0, yaw);
    swc1(out + 8, vec3_length(d));
    return 0;
}

// orig: 0x001fa050 — GS register write of the packed colour (r | g<<8 | b<<16 | a<<24, 64-bit
// shifts of the sign-extended arguments) to register 1, then a 0x14-qword DMA tag to 0x13cc90.
void sub_001fa050(int64_t r, int64_t g, int64_t b, int64_t a) {
    const uint64_t d = static_cast<uint64_t>(r) | (static_cast<uint64_t>(g) << 8) | (static_cast<uint64_t>(b) << 16) |
                       (static_cast<uint64_t>(a) << 24);
    sub_002a0ba8(1, d);
    constexpr OvConst<0x001611c0u> CUR{};
    sw(lwu(CUR) + 0x0, 0x30000014u);
    sw(lwu(CUR) + 0x4, 0x0013cc90u);
    sw(lwu(CUR) + 0x8, 0);
    sw(lwu(CUR) + 0xc, 0x50000014u);
    sw(CUR, lwu(CUR) + 0x10);
}

// Adds a sparkle to the 16-slot list 0x16ea40 (0x20 each; free when the timer +0x10 is not
// positive): at `pos`, 24 frames, kind `kind` (+0x14), size `size`, a random angle. Returns its
// slot, -1 when full.
// orig: 0x001fe990
int32_t sparkle_add(const Vec4& pos, uint32_t kind, float size) {
    for (int32_t i = 0; i < 0x10; ++i) {
        const uint32_t e = OV(0x0016ea40) + static_cast<uint32_t>(i) * 0x20;
        if (0 < lh(e + 0x10)) continue;
        V(e) = pos;
        swc1(e + 0x1c, size);
        sh(e + 0x10, 0x18);
        sh(e + 0x14, kind);
        swc1(e + 0x18, rand_angle());
        return i;
    }
    return -1;
}

// Queues a camera effect (at most 3 a frame, 0x169040, 0x30 each; none in mode 2): kind `kind`,
// `a`, `b` (+0x18), the values f0 / f1, the point `at` (when given, flag +0x24); its curve
// `curve` with `f2` (-1: by kind, 0xd and 0x1f..0x27 a quarter turn, curve 2).
// orig: 0x001edf60
void cam_effect_add(uint32_t kind, uint32_t a, const Vec4* at, uint32_t b, int32_t curve, float f0, float f1, float f2) {
    if (lw(OV(0x0015f5c4)) == 2) return;
    const int32_t n = lw(OV(0x00169100));
    if (!(n < 3)) return;
    const uint32_t e = OV(0x00169040) + static_cast<uint32_t>(n) * 0x30;
    sw(e + 0x20, kind);
    sw(e + 0x14, a);
    swc1(e + 0x10, f0);
    swc1(e + 0x1c, f1);
    sw(e + 0x18, b);
    if (at != nullptr) {
        V(e) = *at;
        sh(e + 0x24, 1);
    } else {
        sh(e + 0x24, 0);
    }
    if (curve == -1) {
        const int32_t k = static_cast<int32_t>(b);
        if (k == 0xd || (!(k < 0xd) && k < 0x28 && !(k < 0x1f))) {
            sw(e + 0x2c, 2);
            swc1(e + 0x28, u2f(0x3fc90fdbu));
            sh(e + 0x26, 0);
        }
    } else {
        sw(e + 0x2c, static_cast<uint32_t>(curve));
        sh(e + 0x26, 4);
        swc1(e + 0x28, f2);
    }
    sw(OV(0x00169100), static_cast<uint32_t>(lw(OV(0x00169100)) + 1));
}

}  // namespace rac1
