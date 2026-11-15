// Ported overlay code 0x00230000..0x0023ffff.
#include <game/enhance/hero_modes.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>

#include <game/ovl/ovl_reloc.h>
namespace rac1 {

// orig: 0x002321a8
void sub_002321a8() {
    if (on_rilgar()) { ril_call(ril_sub_2321a8_u); return; }  // (Rilgar's own: L5:0x0025b538)
    if (on_kalebo()) { kal_call(kal_sub_002321a8_u); return; }  // (level 16's own: L16:0x0022dc48)
    sw(g::PLAYER + 0x227c, 0);
    sw(g::PLAYER + 0x2278, 0);
}

// orig: 0x002321c0 — look `key` up in the (s16 key, s16 value) list at
// *(0x1415cc), terminated by key -1. Found: *out = value, return 1.
int32_t sub_002321c0(int32_t key, int32_t* out) {
    uint32_t p = lwu(0x001415cc);
    if (p == 0) return 0;
    int32_t k = lh(p);
    while (k != -1) {
        if (key == k) {
            *out = lh(p + 2);
            return 1;
        }
        p += 4;
        k = lh(p);
    }
    *out = 0;
    return 0;
}

// orig: 0x002325e0 — select (a, b) when b is a valid index for the hero's
// current animation set (count byte at +0x10 of the set descriptor).
void sub_002325e0(int32_t a, int32_t b) {
    if (a < 0) return;
    const uint32_t hero = lwu(g::PLAYER + 0x2080);
    const uint32_t set = lwu(lwu(hero + 0x24) + lbu(hero + 0x53) * 4u + 0x48);
    if (b < static_cast<int32_t>(lbu(set + 0x10))) {
        sw(g::PLAYER + 0xab4, static_cast<uint32_t>(b));
        sw(g::PLAYER + 0xab0, static_cast<uint32_t>(a));
    }
}

// orig: 0x00232628
void sub_00232628() {
    sw(g::PLAYER + 0xab0, 0xffffffffu);
    sw(g::PLAYER + 0xab8, 0);
}

// orig: 0x00233210 — set bit 0 of the u16 flags (+0x34) of both hero mobys.
void hero_mobys_set_flag1() {
    if (on_batalia()) { bat_call(bat_hero_mobys_set_flag1_u); return; }  // (level 8's own: L8:0x0023e4c0)
    if (on_pokitaru()) { pok_call(pok_hero_mobys_set_flag1_u); return; }  // (level 11's own: L11:0x00259c80)
    if (on_hoven()) { hov_call(hov_hero_mobys_set_flag1_u); return; }  // (level 12's own: L12:0x0024cac0)
    if (on_oltanis()) { olt_call(olt_hero_mobys_set_flag1_u); return; }  // (level 14's own: L14:0x0023c550)
    if (on_quartu()) { qua_call(qua_hero_mobys_set_flag1_u); return; }  // (level 15's own: L15:0x00223e48)
    if (on_kalebo()) { kal_call(kal_hero_mobys_set_flag1_u); return; }  // (level 16's own: L16:0x0022ecd8)
    if (on_veldin_finale()) { vf_call(vf_hero_mobys_set_flag1_u); return; }  // (level 18's own: L18:0x00236830)
    if (on_l1_family() || on_rilgar()) return hero_mobys_set_flag1_l1();
    const uint32_t m = lwu(g::PLAYER + 0x2080);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    const uint32_t m2 = lwu(g::PLAYER + 0x1620);
    if (m2 != 0) sh(m2 + 0x34, lhu(m2 + 0x34) | 1u);
}

// orig: 0x00233248
void hero_mobys_clear_flag1() {
    if (on_batalia()) { bat_call(bat_hero_mobys_clear_flag1_u); return; }  // (level 8's own: L8:0x0023e4d8)
    if (on_pokitaru()) { pok_call(pok_hero_mobys_clear_flag1_u); return; }  // (level 11's own: L11:0x00259c98)
    if (on_hoven()) { hov_call(hov_hero_mobys_clear_flag1_u); return; }  // (level 12's own: L12:0x0024cad8)
    if (on_oltanis()) { olt_call(olt_hero_mobys_clear_flag1_u); return; }  // (level 14's own: L14:0x0023c568)
    if (on_quartu()) { qua_call(qua_hero_mobys_clear_flag1_u); return; }  // (level 15's own: L15:0x00223e60)
    if (on_kalebo()) { kal_call(kal_hero_mobys_clear_flag1_u); return; }  // (level 16's own: L16:0x0022ecf0)
    if (on_veldin_finale()) { vf_call(vf_hero_mobys_clear_flag1_u); return; }  // (level 18's own: L18:0x00236848)
    if (on_l1_family() || on_rilgar()) return hero_mobys_clear_flag1_l1();
    const uint32_t m = lwu(g::PLAYER + 0x2080);
    sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    const uint32_t m2 = lwu(g::PLAYER + 0x1620);
    if (m2 != 0) sh(m2 + 0x34, lhu(m2 + 0x34) & 0xfffeu);
}

// orig: 0x00233288
void sub_00233288(int32_t a, int32_t b, int32_t c, int32_t d, float e, float f) {
    const uint32_t flags = (lhu(g::PLAYER + 0x12ae) & ~(static_cast<uint32_t>(d) & 0xffu)) |
                           (static_cast<uint32_t>(c) & 0xffu);
    swc1(g::PLAYER + 0x12a4, f);
    sw(g::PLAYER + 0x12a8, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(a))));
    sh(g::PLAYER + 0x12ac, static_cast<uint32_t>(b) & 0xffu);
    sh(g::PLAYER + 0x12ae, flags);
    swc1(g::PLAYER + 0x12a0, e);
}

// orig: 0x00233e98 — idx -1 means "current" (0x140408).
uint32_t sub_00233e98(int32_t idx) {
    if (idx == -1) idx = lw(0x00140408);
    if (lhu(OV(0x001c40b0) + static_cast<uint32_t>(idx * 0x18) + 0xe) == 0) return 1;
    return lwu(0x0013d428 + static_cast<uint32_t>(idx) * 4u);
}

// orig: 0x00233f38 — 0x65/0x66 → 1, 0x67 → 2, anything else → 0.
int32_t sub_00233f38() {
    const int32_t s = lw(g::HERO_STATE);
    if (s < 0x65) return 0;
    if (s < 0x67) return 1;
    return s == 0x67 ? 2 : 0;
}

// orig: 0x00234e48 — index of `key` in the u16 list at *(0x17e4f4) (stride 8,
// terminated by 0xffff); the list length when not found.
int32_t sub_00234e48(uint32_t key) {
    uint32_t p = lwu(OV(0x0017e4f4));
    int32_t i = 0;
    for (uint32_t v = lhu(p); v != 0xffffu && v != key; v = lhu(p)) {
        p += 8;
        ++i;
    }
    return i;
}

// orig: 0x002322c8
void sub_002322c8() {
    if (lw(g::PLAYER + 0x2278) == 0) return;
    if (lw(g::PLAYER + 0x227c) == 0) return;
    int32_t value;  // stack slot in the original; only the found flag is used
    if (sub_002321c0(lbu(lwu(g::PLAYER + 0x2080) + 0x53), &value) == 0) return;
    swc1(lwu(g::PLAYER + 0x2278) + 0x58, lwc1(g::PLAYER + 0xa90));
}

// orig: 0x00232640 — -1 means the default of 5 frames.
void sub_00232640(int32_t frames) {
    if (frames == -1) frames = frame_scale_i(5);
    sw(0x0013fe08, static_cast<uint32_t>(frames));
}

// orig: 0x00233660 — dst = PLAYER matrix * (x, y, z, 0) + PLAYER+0x80.
void sub_00233660_v(Vec4& dst, float x, float y, float z) {
    vec_clear(dst);
    dst.x = x;
    dst.y = y;
    dst.z = z;
    mat4_transform(dst, dst, M(g::PLAYER));
    vec3_add(dst, dst, V(g::PLAYER + 0x80));
}
// orig: 0x00233660 (EE-address form)
void sub_00233660(uint32_t dst, float x, float y, float z) { sub_00233660_v(V(dst), x, y, z); }

// orig: 0x00233b20 — move mode (PLAYER+0x20b3): 0 copies, 1/2 go through the two camera matrices.
void sub_00233b20_v(Vec4& dst, const Vec4& src, float z) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) {
        dst = src;
        dst.z = z;
        return;
    }
    if (!(mode < 3)) return;
    mat4_transform(dst, src, M(g::PLAYER + 0x40));
    dst.z = z;
    mat4_transform(dst, dst, M(g::PLAYER));
}
// orig: 0x00233b20 (EE-address form)
void sub_00233b20(uint32_t dst, uint32_t src, float z) { sub_00233b20_v(V(dst), V(src), z); }

// orig: 0x00233ba0
void sub_00233ba0_v(Vec4& dst, const Vec4& src, float len) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) {
        vec2_set_length(dst, src, len);
        return;
    }
    if (!(mode < 3)) return;
    mat4_transform(dst, src, M(g::PLAYER + 0x40));
    vec2_set_length(dst, dst, len);
    mat4_transform(dst, dst, M(g::PLAYER));
}
// orig: 0x00233ba0 (EE-address form)
void sub_00233ba0(uint32_t dst, uint32_t src, float len) { sub_00233ba0_v(V(dst), V(src), len); }

// orig: 0x00233d30 — pitch of v (atan2(z, |xy|)) in the current move mode's frame.
float sub_00233d30(uint32_t v) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) {
        const float l = vec2_length(V(v));
        return atan2_f(lwc1(v + 8), l);
    }
    if (!(mode < 3)) return 0.0f;
    Vec4 t = V(v);
    mat4_transform(t, t, M(g::PLAYER + 0x40));
    const float l = vec2_length(t);
    return atan2_f(t.z, l);
}

// orig: 0x00233db8 — take `amount` from counter idx (0x13d428) into 0x13dea0; 0 if not enough.
int32_t sub_00233db8(int32_t idx, int32_t amount) {
    if (idx == -1) idx = lw(0x00140408);
    if (lhu(OV(0x001c40b0) + static_cast<uint32_t>(idx * 0x18) + 0xe) == 0) return 1;
    const uint32_t p = 0x0013d428 + static_cast<uint32_t>(idx) * 4u;
    const int32_t cur = lw(p);
    if (cur < amount) return 0;
    sw(p, static_cast<uint32_t>(cur - amount));
    const uint32_t q = 0x0013dea0 + static_cast<uint32_t>(idx) * 4u;
    sw(q, static_cast<uint32_t>(lw(q) + amount));
    return 1;
}

// orig: 0x00235988 — fill obj from the entry of `key` in the list at *(0x17e4f4).
void sub_00235988(uint32_t obj, uint32_t key) {
    const int32_t idx = sub_00234e48(key);
    const uint32_t off = static_cast<uint32_t>(idx) << 3;
    const uint32_t v0 = lhu(off + lwu(OV(0x0017e4f4)));
    sh(obj + 0x40, static_cast<uint32_t>(idx));
    sw(obj + 0x00, v0);
    sb(obj + 0x42, lbu(off + lwu(OV(0x0017e4f4)) + 6));
    sw(obj + 0x44, lhu(off + lwu(OV(0x0017e4f4)) + 4));
}

namespace {
// orig: 0x002359f8 (search loop, duplicated in 0x00235e18). Index of the 0x90-byte record (13 at 0x17dc50) whose word +0x64 is key; 13 if none.
int32_t find_rec_17dc50(uint32_t key) {
    if (lwu(OV(0x0017dc50) + 0x64) == key) return 0;
    uint32_t p = OV(0x0017dc50) + 0x64;
    int32_t i = 1;
    for (;;) {
        if (!(i < 0xd)) return i;
        p += 0x90;
        if (lwu(p) == key) return i;
        ++i;
    }
}
}  // namespace

// orig: 0x002359f8 — returns the record address (0 if none).
uint32_t sub_002359f8(uint32_t key, uint32_t v) {
    const int32_t i = find_rec_17dc50(key);
    if (!(i < 0xd)) return 0;
    const uint32_t e = OV(0x0017dc50) + static_cast<uint32_t>(i * 0x90);
    sw(e + 0x24, v);
    if (lw(e + 0x68) == 0) sw(e + 4, v);
    return e;
}

// orig: 0x00235e18 — raise the record's +0x7c to at least v; 1 when not applicable.
int32_t sub_00235e18(uint32_t key, int32_t v) {
    const int32_t i = find_rec_17dc50(key);
    if (!(i < 0xd)) return 1;
    const uint32_t e = OV(0x0017dc50) + static_cast<uint32_t>(i * 0x90);
    if (lw(e + 0x68) != 0) return 1;
    const int32_t cur = lw(e + 0x7c);
    sw(e + 0x7c, static_cast<uint32_t>(v < cur ? cur : v));
    return 0;
}

// orig: 0x00232320 — sync the attached moby (PLAYER+0x2278) with the hero's animation state.
void sub_00232320() {
    const uint32_t m = lwu(g::PLAYER + 0x2278);
    if (m == 0) return;
    swc1(m + 0x5c, lwc1(g::PLAYER + 0xa94));
    const uint32_t sets = lwu(m + 0x24) + 0x48;
    const uint32_t set_a = sets + lbu(m + 0x52) * 4u;
    swc1(m + 0x54, lwc1(lwu(g::PLAYER + 0x2080) + 0x54));
    const uint32_t set_b = sets + lbu(m + 0x53) * 4u;
    const uint32_t ia = lbu(lwu(g::PLAYER + 0x2080) + 0x51);
    sb(m + 0x50, ia);
    const uint32_t ib = lbu(lwu(g::PLAYER + 0x2080) + 0x51);
    sb(m + 0x51, ib);
    sw(m + 0x68, lwu(lwu(set_a) + ia * 4u + 0x1c));
    sw(m + 0x6c, lwu(lwu(set_b) + ib * 4u + 0x1c));
}

// orig: 0x00233440
int32_t sub_00233440() {
    const int32_t s = lw(g::HERO_STATE);
    int32_t r = lh(g::PLAYER + 0x308);
    if (s == 0x3f || s == 0x71 || s == 0x70) return 1;
    if (lbu(g::PLAYER + 0x12e7) == 0) return r;
    if (lbu(0x0013d4dc) == 0) return r;
    if (s != 0) return r;
    if (lh(g::PLAYER + 0x30e) < frame_scale_i(4)) r = 1;
    return r;
}

// orig: 0x002335c8 — dst = PLAYER matrix * (r cos a, r sin a, z) + PLAYER+0x80.
// dst.w is left as it was (it feeds the matrix's row 3).
void sub_002335c8_v(Vec4& dst, float r, float a, float z) {
    dst.x = cos_vu(a) * r;
    const float y = sin_vu(a) * r;
    dst.z = z;
    dst.y = y;
    mat4_transform(dst, dst, M(g::PLAYER));
    vec3_add(dst, dst, V(g::PLAYER + 0x80));
}
// orig: 0x002335c8 (EE-address form)
void sub_002335c8(uint32_t dst, float r, float a, float z) { sub_002335c8_v(V(dst), r, a, z); }

// orig: 0x00233a78 — component of v along the hero's local up axis (move modes 1/2), or v.z.
float sub_00233a78_v(const Vec4& v) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) return v.z;
    if (!(mode < 3)) return 0.0f;
    Vec4 up;
    vec_clear(up);
    Mat4 m;
    up.z = 1.0f;
    euler_to_mat4_vu(m, V(lwu(g::PLAYER + 0x2080) + 0x40));
    mat4_transform(up, up, m);
    return vec3_dot(up, v);
}
// orig: 0x00233a78 (EE-address form)
float sub_00233a78(uint32_t v) { return sub_00233a78_v(V(v)); }

// orig: 0x002339d0 — horizontal length of v in the hero's local frame (move modes 1/2).
float sub_002339d0_v(const Vec4& v) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) return vec2_length(v);
    if (!(mode < 3)) return 0.0f;
    Mat4 m, mt;
    euler_to_mat4_vu(m, V(lwu(g::PLAYER + 0x2080) + 0x40));
    mat3_transpose(mt, m);
    Vec4 t;
    mat4_transform(t, v, mt);
    return vec2_length(t);
}
// orig: 0x002339d0 (EE-address form)
float sub_002339d0(uint32_t v) { return sub_002339d0_v(V(v)); }

// orig: 0x00231db0 — value for `key` in the per-kind list selected by sub_0020d498(3)
// (kinds 2/3/4; s16 value, u8 key; ends with -1). Found: *out = value, 1.
int32_t sub_00231db0(uint32_t key, int32_t* out) {
    uint32_t list;
    if (sub_0020d498(3) == 2) {
        list = OV(0x0017bbf0);
    } else if (sub_0020d498(3) == 3) {
        list = OV(0x0017bbd0);
    } else {
        list = sub_0020d498(3) == 4 ? OV(0x0015f6b8u) : 0u;
    }
    if (list != 0 && lh(list) != -1) {
        do {
            if (key == lbu(list + 2)) {
                *out = lh(list);
                return 1;
            }
            list += 4;
        } while (lh(list) != -1);
    }
    *out = 0;
    return 0;
}

// orig: 0x00233810 — dst = src raised by h along the hero's up axis (move modes 1/2),
// or along z (mode 0).
void sub_00233810_v(Vec4& dst, const Vec4& src, float h) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) {
        dst = src;
        dst.z = dst.z + h;
        return;
    }
    if (!(mode < 3)) return;
    Vec4 up;
    vec_clear(up);
    up.z = h;
    Mat4 m;
    euler_to_mat4_vu(m, V(lwu(g::PLAYER + 0x2080) + 0x40));
    mat4_transform(up, up, m);
    vec3_add(dst, src, up);
}
// orig: 0x00233810 (EE-address form)
void sub_00233810(uint32_t dst, uint32_t src, float h) { sub_00233810_v(V(dst), V(src), h); }

// orig: 0x00232fe8 — set flag bit 0 (+0x34) on every hero-related moby; clear bits 0/6 of the
// slot-0 moby in state 8 when allowed.
void sub_00232fe8() {
    auto set1 = [](uint32_t m) { sh(m + 0x34, lhu(m + 0x34) | 1u); };
    set1(lwu(g::PLAYER + 0xa88));
    for (uint32_t p = g::PLAYER; p < g::PLAYER + 0x230; p += 0x50) {
        const uint32_t a = lwu(p + 0x1090);
        if (a != 0) set1(a);
        const uint32_t b = lwu(p + 0x1094);
        if (b != 0) set1(b);
    }
    const uint32_t c = lwu(g::PLAYER + 0x118c);
    if (c != 0) set1(c);
    if (lbu(g::PLAYER + 0x20a4) == 1) {
        const uint32_t d = lwu(g::PLAYER + 0x1624);
        if (d != 0) set1(d);
    }
    if (lw(g::PLAYER + 0x10b8) != 8) return;
    const uint32_t m = lwu(g::PLAYER + 0x1090);
    if (m == 0) return;
    if (lw(OV(0x0015f5c4)) != 0) return;
    if (lbu(m + 0x20) == 0) return;
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
}

// orig: 0x002334d0 — dst = src lowered by h: along z (mode 0), against PLAYER+0x270 (mode 1,
// and mode 2 in state 0x3e at frame 0), else along 0x13f5e0 (mode 2). Mode 0 writes only z.
void sub_002334d0_v(Vec4& dst, const Vec4& src, float h) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    Vec4 t;
    if (mode == 0) {
        dst.z = src.z - h;
        return;
    }
    if (mode == 1) {
        vec3_set_length(t, V(g::PLAYER + 0x270), -h);
    } else if (mode == 2) {
        if (lw(g::HERO_STATE) == 0x3e && lh(g::PLAYER + 0x30e) == 0)
            vec3_set_length(t, V(g::PLAYER + 0x270), -h);
        else
            vec3_set_length(t, V(0x0013f5e0), h);
    } else {
        return;
    }
    vec3_add(dst, src, t);
}
// orig: 0x002334d0 (EE-address form)
void sub_002334d0(uint32_t dst, uint32_t src, float h) { sub_002334d0_v(V(dst), V(src), h); }

// orig: 0x002336e8
void sub_002336e8_v(Vec4& dst, const Vec4& src, float h) { sub_002334d0_v(dst, src, -h); }
// orig: 0x002336e8 (EE-address form)
void sub_002336e8(uint32_t dst, uint32_t src, float h) { sub_002334d0(dst, src, -h); }

// orig: 0x00233708 — dst = src moved by d along the hero's facing (mode 0: yaw at hero+0x48;
// modes 1/2: local x axis). The local vector's w is uninitialised in the original (port: 0).
void sub_00233708_v(Vec4& dst, const Vec4& src, float d) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) {
        dst = src;
        dst.x = dst.x + cos_vu(lwc1(lwu(g::PLAYER + 0x2080) + 0x48)) * d;
        dst.y = dst.y + sin_vu(lwc1(lwu(g::PLAYER + 0x2080) + 0x48)) * d;
        return;
    }
    if (!(mode < 3)) return;
    Vec4 v{0.0f, 0.0f, 0.0f, 0.0f};
    v.x = cos_vu(0.0f) * d;
    v.y = sin_vu(0.0f) * d;
    v.z = 0.0f;
    Mat4 m;
    euler_to_mat4_vu(m, V(lwu(g::PLAYER + 0x2080) + 0x40));
    mat4_transform(v, v, m);
    vec3_add(dst, src, v);
}
// orig: 0x00233708 (EE-address form)
void sub_00233708(uint32_t dst, uint32_t src, float d) { sub_00233708_v(V(dst), V(src), d); }

// orig: 0x002338d0 — dst = src without its component along the ground normal (mode 1:
// PLAYER+0x270, mode 2: PLAYER+0x290); mode 0 zeroes z.
void sub_002338d0_v(Vec4& dst, const Vec4& src) {
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    Vec4 t;
    if (mode == 0) {
        dst = src;
        dst.z = 0.0f;
        return;
    }
    if (mode == 1) {
        vec3_set_length(t, V(g::PLAYER + 0x270), 1.0f);
        const float d = vec3_dot(t, src);
        vec3_scale(t, t, d);
    } else if (mode == 2) {
        const float d = vec3_dot(V(g::PLAYER + 0x290), src);
        vec3_scale(t, V(g::PLAYER + 0x290), d);
    } else {
        return;
    }
    vec3_sub(dst, src, t);
}
// orig: 0x002338d0 (EE-address form)
void sub_002338d0(uint32_t dst, uint32_t src) { sub_002338d0_v(V(dst), V(src)); }

// orig: 0x00233c30 — height after the jump apex for the current move mode (see sub_00259468);
// modes 1/2 use the up component (sub_00233a78) for the frame count.
float sub_00233c30(uint32_t pos, uint32_t vel, int32_t* frames_out, float g) {
    const uint32_t mode = lbu(0x00141403);
    if (mode == 0) {
        const int32_t n = float_to_int(fpu_div(lwc1(vel + 8), g));
        const float fn = int_to_float(n);
        const float tri = int_to_float(static_cast<int32_t>(static_cast<uint32_t>(n) * static_cast<uint32_t>(n) + static_cast<uint32_t>(n)) >> 1);
        const float drop = tri * g;
        const float r = (lwc1(pos + 8) + lwc1(vel + 8) * fn) - drop;
        if (frames_out != nullptr) *frames_out = n;
        return r;
    }
    if (!(mode < 3)) return 0.0f;
    const int32_t n = float_to_int(fpu_div(sub_00233a78(vel), g));
    if (frames_out != nullptr) *frames_out = n;
    return lwc1(pos + 8) + lwc1(vel + 8) * 0.5f;
}

// orig: 0x00231ff8 — random entry of the per-kind list (see sub_00231db0) whose s16 value
// equals `v`: *a = byte +2, *b = byte +3, 1; none: *a = 1, 0.
int32_t sub_00231ff8(uint32_t v, int32_t* a, int32_t* b) {
    const int32_t key = static_cast<int32_t>(sub_002050b8(v));
    uint32_t list;
    if (sub_0020d498(3) == 2) {
        list = OV(0x0017bbf0);
    } else if (sub_0020d498(3) == 3) {
        list = OV(0x0017bbd0);
    } else {
        list = sub_0020d498(3) == 4 ? OV(0x0015f6b8u) : 0u;
    }
    int32_t hits[20];
    int32_t n = 0;
    if (list != 0 && lh(list) != -1) {
        uint32_t p = list;
        int32_t i = 0;
        int32_t e = lh(p);
        for (;;) {
            if (key == e) hits[n++ % 20] = i;  // (the original's stack array holds 20)
            p += 4;
            e = lh(p);
            if (e == -1) break;
            ++i;
        }
    }
    if (n == 0) {
        *a = 1;
        return 0;
    }
    const uint32_t e = static_cast<uint32_t>(hits[rand_range(0, n - 1)] << 2) + list;
    *a = lbu(e + 2);
    *b = lbu(e + 3);
    return 1;
}

// orig: 0x00232128 — copy the hero animation speed to the two attached mobys when the
// current animation set has an entry in the per-kind list.
void sub_00232128() {
    if (lbu(g::PLAYER + 0x20a4) != 0) return;
    if (lw(g::PLAYER + 0x11a4) != 2) return;
    int32_t a, b;
    if (sub_00231ff8(lbu(lwu(g::PLAYER + 0x2080) + 0x53), &a, &b) == 0) return;
    const uint32_t m1 = lwu(g::PLAYER + 0x1180);
    if (m1 != 0) swc1(m1 + 0x58, lwc1(g::PLAYER + 0xa90));
    const uint32_t m2 = lwu(g::PLAYER + 0x1184);
    if (m2 != 0) swc1(m2 + 0x58, lwc1(g::PLAYER + 0xa90));
}

// orig: 0x002330d0 — clear flag bit 0 (+0x34) on the hero-related mobys (inverse of
// sub_00232fe8), then set bits 0x41 on the held items when hidden (PLAYER+0x22d8) and on the
// slot-0 moby in the 0x20ae/0x20af cases.
void sub_002330d0() {
    auto clr1 = [](uint32_t m) { sh(m + 0x34, lhu(m + 0x34) & 0xfffeu); };
    auto set41 = [](uint32_t m) { sh(m + 0x34, lhu(m + 0x34) | 0x41u); };
    clr1(lwu(g::PLAYER + 0xa88));
    for (uint32_t p = g::PLAYER; p < g::PLAYER + 0x230; p += 0x50) {
        const uint32_t a = lwu(p + 0x1090);
        if (a != 0) clr1(a);
        const uint32_t b = lwu(p + 0x1094);
        if (b != 0) clr1(b);
    }
    const uint32_t c = lwu(g::PLAYER + 0x118c);
    if (c != 0) clr1(c);
    if (lbu(g::PLAYER + 0x20a4) == 1) {
        const uint32_t d = lwu(g::PLAYER + 0x1624);
        if (d != 0) clr1(d);
    }
    if (lh(g::PLAYER + 0x22d8) != 0) {
        for (uint32_t off : {0x1180u, 0x1184u, 0x118cu}) {
            const uint32_t m = lwu(g::PLAYER + off);
            if (m != 0) set41(m);
        }
    }
    const bool hold = (lbu(g::PLAYER + 0x20ae) != 0 && lw(g::PLAYER + 0x10b8) == 8) || lbu(g::PLAYER + 0x20af) != 0;
    if (!hold) return;
    const uint32_t m = lwu(g::PLAYER + 0x1090);
    if (m != 0) set41(m);
}

// orig: 0x0023e5e0 — add a light/decal at pos (colour bytes rgb / 128, size, intensity) in the
// first free of 8 slots (0x1802c0 data, 0x1804c0 control); -1 if none or when 0x15f5d4 > 0.8.
int32_t sub_0023e5e0_v(const Vec4& pos, uint32_t rgb, float size, float intensity) {
    if (u2f(0x3f4ccccdu) < lwc1(OV(0x0015f5d4))) return -1;
    const uint32_t CTL = OV(0x001804c0), DATA = OV(0x001802c0);
    int32_t i = 0;
    if (lw(CTL + 0x10) != 0) {
        uint32_t p = CTL + 0x10;
        i = 1;
        for (;;) {
            if (!(i < 8)) break;
            p += 0x30;
            if (lw(p) == 0) break;
            ++i;
        }
    }
    if (i == 8) return -1;
    const uint32_t d = DATA + static_cast<uint32_t>(i << 5);
    const uint32_t c = CTL + static_cast<uint32_t>(i * 0x30);
    V(d + 0x10) = pos;
    swc1(d + 0x1c, size);
    const float k = 0.0078125f;  // 0x3c000000
    swc1(d + 0x0, int_to_float(static_cast<int32_t>(rgb & 0xff)) * k);
    swc1(d + 0x4, int_to_float(static_cast<int32_t>((static_cast<int32_t>(rgb) >> 8) & 0xff)) * k);
    const float b = int_to_float(static_cast<int32_t>((static_cast<int32_t>(rgb) >> 16) & 0xff)) * k;
    swc1(d + 0xc, intensity);
    swc1(d + 0x8, b);
    mem_clear128(c, 0x30);
    sw(c + 0x10, 1);
    sw(c + 0xc, static_cast<uint32_t>(i << 10) + OV(0x00180640u));
    return i;
}
// orig: 0x0023e5e0 (EE-address form)
int32_t sub_0023e5e0(uint32_t pos, uint32_t rgb, float size, float intensity) { return sub_0023e5e0_v(V(pos), rgb, size, intensity); }

// orig: 0x002332d0 — the hero's facing yaw measured in the ground plane (normal PLAYER+0x270:
// rotate into the normal's frame and back); plain yaw when the normal has no xy part.
float sub_002332d0() {
    Vec4 v{0.0f, 0.0f, 0.0f, 0.0f};
    v.x = cos_vu(0.0f);
    v.y = sin_vu(0.0f);
    v.z = 0.0f;
    Mat4 r;
    euler_to_mat4_vu(r, V(0x0013f3e0));
    mat4_transform(v, v, r);
    if (lwc1(g::PLAYER + 0x270) == 0.0f && lwc1(g::PLAYER + 0x274) == 0.0f) return atan2_f(v.x, v.y);
    Vec4 n = V(g::PLAYER + 0x270);
    Vec4 a, b;
    vec_clear(a);
    vec_clear(b);
    a.z = -atan2_f(lwc1(g::PLAYER + 0x270), lwc1(g::PLAYER + 0x274));
    euler_to_mat4_vu(r, a);
    mat4_transform(v, v, r);
    mat4_transform(n, n, r);
    b.y = -atan2_f(n.z, n.x);
    euler_to_mat4_vu(r, b);
    mat4_transform(v, v, r);
    a.z = -a.z;
    euler_to_mat4_vu(r, a);
    mat4_transform(v, v, r);
    return atan2_f(v.x, v.y);
}

// orig: 0x00234808 — pulse the colours (+0x90) of the three weapon glow mobys
// (PLAYER+0x11d0/+0x11d4/+0x1220) with their phase angles PLAYER+0x22e8/+0x22ec/+0x22f0
// (slower while *(0x15f5c4) == 2), and keep the third one's light node / quaternion.
void sub_00234808() {
    uint32_t m = lwu(g::PLAYER + 0x11d0);
    if (m != 0) {
        const float step = lw(OV(0x0015f5c4)) == 2 ? u2f(0x3f9c61aau) : u2f(0x40060a92u);
        swc1(g::PLAYER + 0x22e8, angle_add(lwc1(g::PLAYER + 0x22e8), lwc1(0x0015ed6c) * step));
        const float s = sin_vu(lwc1(g::PLAYER + 0x22e8));
        sw(m + 0x90, rgba_lerp(0x00d2d2d2u, NOT_ADDR(0x00285050u), s * 0.5f + 0.5f));
    }
    m = lwu(g::PLAYER + 0x11d4);
    if (m != 0) {
        if (lw(OV(0x0015f5c4)) == 2) {
            const float a = angle_add(lwc1(g::PLAYER + 0x22ec), lwc1(0x0015ed6c) * u2f(0x3fc90fdbu));
            swc1(g::PLAYER + 0x22ec, a);
            const float s = sin_vu(a);
            sw(m + 0x90, rgba_lerp(NOT_ADDR(0x001e1ed2u), NOT_ADDR(0x001e1e50u), s * 0.5f + 0.5f));
        } else {
            const float a = angle_add(lwc1(g::PLAYER + 0x22ec), lwc1(0x0015ed6c) * u2f(0x403de44eu));
            swc1(g::PLAYER + 0x22ec, a);
            uint32_t to = NOT_ADDR(0x001ee628u);
            if (0.0f < a && a < u2f(0x3ff5be0bu)) to = NOT_ADDR(0x001e1ed2u);
            sw(m + 0x90, rgba_lerp(lwu(m + 0x90), to, u2f(0x3d8f5c29u)));
        }
    }
    m = lwu(g::PLAYER + 0x1220);
    if (m == 0) return;
    const float step = lw(OV(0x0015f5c4)) == 2 ? u2f(0x3fb2b8c2u) : u2f(0x3ff5be0bu);
    swc1(g::PLAYER + 0x22f0, angle_add(lwc1(g::PLAYER + 0x22ec), lwc1(0x0015ed6c) * step));
    const float s = sin_vu(lwc1(g::PLAYER + 0x22f0));
    sw(m + 0x90, rgba_lerp(0x00aac8aau, 0x00965050u, s * 0.5f + 0.5f));
    if (lbu(g::PLAYER + 0x1d91) == 0) sub_0024f590(m, 0, g::PLAYER + 0x1d90);
    else quat_from_axis_angle(V(g::PLAYER + 0x1da0), 0, lwc1(g::PLAYER + 0x22f0));
}

namespace {
// drop owner nibble `id` from a 4-nibble owner stack (0xf = free slot); 0xffff = no owners
static uint32_t owner_stack_remove(uint32_t s, uint32_t id) {
    if ((s & 0xf) == id) return (s >> 4) | 0xf000;
    if ((s & 0xf0) == ((id << 4) & 0xffff)) return (s & 0xf) | ((s >> 4) & 0xff0) | 0xf000;
    if ((s & 0xf00) == ((id << 8) & 0xffff)) return (s & 0xff) | ((s >> 4) & 0xf00) | 0xf000;
    if ((s & 0xf000) == ((id << 12) & 0xffff)) return s | 0xf000;
    debug_printf_stub();  // "not found" diagnostic
    return s;
}
static void owner_list_release(uint32_t list, uint32_t n, uint32_t table, uint32_t stride, uint32_t field, uint32_t flag, uint32_t id) {
    for (uint32_t p = list; p != list + n; p += 2) {
        const uint32_t e = lw(table) + static_cast<uint32_t>(lh(p)) * stride;
        const uint32_t s = owner_stack_remove(lhu(e + field), id);
        if (s == 0xffff) sb(e + flag, 1);
        sh(e + field, s);
    }
}
}  // namespace

// orig: 0x0023edf0 — release owner `id` (0..15) from the three object lists of its record
// 0x1804c0 + id*0x30 (index arrays at +0xc: ties +0/+2, shrubs +8/+0xa, mobies +4/+6): pop
// it from each object's owner nibbles, flag objects left without owners, and empty the lists.
void region_release(uint32_t id) {
    const uint32_t r = OV(0x001804c0) + id * 0x30;
    const uint32_t key = id & 0xffff;
    owner_list_release(lw(r + 0xc) + static_cast<uint32_t>(lh(r + 0)) * 2, static_cast<uint32_t>(lh(r + 2)) * 2, OV(0x00160fc0), 0x20, 0x1e, 0x1b, key);
    const uint32_t lb = lw(r + 0xc) + static_cast<uint32_t>(lh(r + 8)) * 2;
    const uint32_t nb = static_cast<uint32_t>(lh(r + 0xa)) * 2;
    sh(r + 0, 0);
    sh(r + 2, 0);
    owner_list_release(lb, nb, OV(0x00160f4c), 0x40, 0x36, 0x35, key);
    const uint32_t lc = lw(r + 0xc) + static_cast<uint32_t>(lh(r + 4)) * 2;
    const uint32_t nc = static_cast<uint32_t>(lh(r + 6)) * 2;
    sh(r + 8, 0);
    sh(r + 0xa, 0);
    owner_list_release(lc, nc, OV(0x00160494), 0x20, 0x1e, 0x1b, key);
    sh(r + 6, 0);
    sh(r + 4, 0);
}

// orig: 0x0023e838 — release region owner `id` and clear its two records (0x1802c0 + id*0x20,
// 0x1804c0 + id*0x30).
void region_clear(uint32_t id) {
    region_release(id);
    mem_clear128(OV(0x001802c0) + id * 0x20, 0x20);
    mem_clear128(OV(0x001804c0) + id * 0x30, 0x30);
}

// orig: 0x002347c0 — keep the hero (PLAYER+0x80) out of the tube of its current path
// PLAYER+0x22da (radius PLAYER+0x234); on contact set PLAYER+0x1f6 = 4.
void hero_path_push_out() {
    const uint32_t P = g::PLAYER;
    if (path_push_out(static_cast<uint32_t>(lh(P + 0x22da)), P + 0x80, P + 0x80, lwc1(P + 0x234)) != 0) sh(P + 0x1f6, 4);
}

// orig: 0x00232670 — advance the hero moby's animation: +0x54 += speed PLAYER+0xa90 * rate +0xa94
// (rate alone while blending, or scripted table 0x17bdf0 row +0xaa0), snapped near 0/1; whole frames
// step the frame pair (flags +0xa98; +0xab8/+0xab0 force frame jumps), rescaled per frame. Then one
// sound event, looping-sound sync, per-frame hooks, anim time +0xaa8/+0xaac and blend flag +0xa9c.
void hero_anim_advance() {
    const uint32_t P = g::PLAYER;
    const uint32_t m = lw(P + 0x2080);
    sub_002321a8();
    sw(P + 0xa98, static_cast<uint32_t>(lw(P + 0xa98)) & ~3u);
    const bool same = lbu(m + 0x52) == lbu(m + 0x53);
    const int32_t old_frame = lbu(m + 0x50);
    const float old_pos = lwc1(m + 0x54);
    auto anim_of = [m](uint32_t id) { return static_cast<uint32_t>(lw(static_cast<uint32_t>(lw(m + 0x24)) + id * 4 + 0x48)); };
    if (same) {
        swc1(m + 0x54, fpu_add(old_pos, fpu_mul(lwc1(P + 0xa90), lwc1(P + 0xa94))));  // +0xa94 can be 0x7f800000 (0x15f708)
    } else if (lw(P + 0xaa0) < 0) {
        swc1(m + 0x54, fpu_add(old_pos, lwc1(P + 0xa94)));
    } else {
        const int32_t k = lw(P + 0xaa4);
        swc1(m + 0x54, lwc1(OV(0x0017bdf0) + static_cast<uint32_t>(k) * 4 + static_cast<uint32_t>(lw(P + 0xaa0)) * 100));
        sw(P + 0xaa4, static_cast<uint32_t>(k + 1));
    }
    float f = lwc1(m + 0x54);
    if (u2f(0x3f7d70a4u) < f && f < u2f(0x3f8147aeu)) {
        swc1(m + 0x54, 1.0f);
        f = 1.0f;
    }
    if (u2f(0xbc23d70au) < f && f < u2f(0x3c23d70au)) sw(m + 0x54, 0);
    f = lwc1(m + 0x54);
    const float delta = f - old_pos;
    if (1.0f <= f) {
        const float rate0 = lwc1(OV(0x0015f708));
        do {
            const uint32_t nxt = lbu(m + 0x53);
            if (lbu(m + 0x52) != nxt) {
                sb(m + 0x52, nxt);
                sb(m + 0x7e, lbu(anim_of(nxt) + 0x12));
            }
            sw(P + 0xaa0, 0xffffffffu);
            sw(P + 0xa98, static_cast<uint32_t>(lw(P + 0xa98)) | 1u);
            const uint32_t f1 = lbu(m + 0x51);
            sb(m + 0x50, f1);
            sw(m + 0x68, lw(m + 0x6c));
            sb(m + 0x51, f1 + 1);
            if (lw(P + 0xab8) != 0) {
                sw(P + 0xab8, 0);
                swc1(P + 0xa94, rate0);
                swc1(m + 0x54, 0.0f);
                sb(m + 0x51, lbu(P + 0xab4));
                sw(m + 0x6c, lw(anim_of(lbu(m + 0x53)) + (lbu(P + 0xab4) & 0xffu) * 4 + 0x1c));
                sub_00232628();
                sub_00232320();
            } else if (lw(P + 0xab0) != -1 && lw(P + 0xab4) < static_cast<int32_t>(lbu(m + 0x51))) {
                swc1(P + 0xa94, u2f(0x3eaaaaabu));
                sw(m + 0x54, 0);
                sb(m + 0x51, lbu(P + 0xab0));
                sw(m + 0x6c, lw(anim_of(lbu(m + 0x53)) + (lbu(P + 0xab0) & 0xffu) * 4 + 0x1c));
                sub_00232320();
            } else {
                if (!(lbu(m + 0x51) < lbu(anim_of(lbu(m + 0x53)) + 0x10))) {
                    sb(m + 0x51, 0);
                    sw(P + 0xa98, static_cast<uint32_t>(lw(P + 0xa98)) | 2u);
                }
                const uint32_t anim = anim_of(lbu(m + 0x53));
                const uint32_t prev = lw(m + 0x68);
                float p = fpu_sub(lwc1(m + 0x54), 1.0f);
                sw(m + 0x6c, lw(anim + lbu(m + 0x51) * 4u + 0x1c));
                swc1(m + 0x54, p);
                p = fpu_div(p, lwc1(P + 0xa94));
                swc1(m + 0x54, p);
                const float r = lwc1(prev);
                swc1(P + 0xa94, r);
                swc1(m + 0x54, fpu_mul(lwc1(m + 0x54), r));
            }
            f = lwc1(m + 0x54);
        } while (1.0f <= f);
    }
    if (same && lbu(m + 0x7e) != 0) {
        const int32_t now = static_cast<int32_t>(lbu(m + 0x50)) * 16 + float_to_int(f * 16.0f);
        const int32_t before = old_frame * 16 + float_to_int(old_pos * 16.0f);
        const uint32_t anim = anim_of(lbu(m + 0x52));
        uint32_t ev = anim + lbu(anim + 0x10) * 4u + 0x1c;
        for (int32_t i = 0; i < static_cast<int32_t>(lbu(anim + 0x12)); ++i, ev += 4) {
            const int32_t w = lw(ev);
            const int32_t t = w >> 16;
            if (before < t && !(now < t)) {
                moby_sound_start(w & 0xffff, 0, m);
                break;
            }
        }
    }
    const uint32_t slot = lbu(m + 0x7d);
    if (slot != 0xff) {
        const uint32_t e = 0x0013e550 + slot * 0x70;
        if (static_cast<uint32_t>(lw(e + 0x88)) != m) {
            sb(m + 0x7d, 0xff);
        } else if (lh(e + 0x7e) != static_cast<int32_t>(lbu(m + 0x7c))) {
            sub_0028d918(static_cast<int32_t>(slot));
            sb(m + 0x7d, 0xff);
        }
    } else if (lbu(m + 0x7c) != 0xff) {
        const int32_t g = lw(OV(0x00166c00u - 0x763c));
        if (g != 2 && g != 6) sb(m + 0x7d, static_cast<uint32_t>(moby_sound_start(lbu(m + 0x7c), 4, m)));
    }
    sub_00232128();
    sub_002322c8();
    const float prev_t = lwc1(P + 0xaa8);
    const float t = moby_anim_time(m);
    swc1(P + 0xaa8, t);
    swc1(P + 0xaac, prev_t <= t ? t - prev_t : delta);
    sw(P + 0xa9c, lbu(m + 0x52) != lbu(m + 0x53) ? 1u : 0u);
}

// orig: 0x00233f80 — ride moving platforms: track the platform moby PLAYER+0x360 (the floor
// +0x2fc, or +0x4f8 while hanging; flags +0x364: 1 = position latched, 2 = rotation latched),
// convert the hero's position/heading (and the ledge point +0x4d0) into the platform frame and
// back each frame (0x260400/0x260668/0x2604f0), and add the resulting carry velocity +0x350 to
// +0xf0; after leaving it the carry decays (by the airborne time). +0xfc = yaw change.
void hero_platform_ride() {
    const uint32_t P = g::PLAYER;
    int32_t mode = 0;
    float blend = 0.0f;
    const float yaw0 = lwc1(P + 0x98);
    if (lh(P + 0x30e) == 0) {
        const uint32_t floor = lw(P + 0x2fc);
        if (floor != 0) {
            mode = 1;
            if (static_cast<uint32_t>(lw(P + 0x360)) != floor) sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~1u);
            sw(P + 0x360, lw(P + 0x2fc));
        } else {
            sw(P + 0x360, 0);
            sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~3u);
        }
    } else if ((lw(g::HERO_STATE) == 0x1c || lw(P + 0x208c) == 3) && lw(P + 0x4f8) != 0) {
        sw(P + 0x360, lw(P + 0x4f8));
        mode = 1;
    } else if (lw(P + 0x360) != 0) {
        mode = 1;
        blend = static_cast<float>(lh(P + 0x30e));  // (the original also computes and drops 120 - this)
    } else {
        sw(P + 0x360, 0);
        sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~3u);
    }
    if (mode == 0) return;
    const uint32_t info = moby_pvar_word2(lw(P + 0x360));
    if (info == 0) return;
    // stack locals (uninitialised in the original: 0 here)
    Vec4 s00 = {}, s10 = {}, s20 = {}, s30 = {}, s40 = {}, s50 = {}, s60 = {}, s70 = {}, s80 = {}, s90 = {}, sa0 = {};
    auto hm = [P] { return static_cast<uint32_t>(lw(P + 0x2080)); };
    auto plat = [P] { return static_cast<uint32_t>(lw(P + 0x360)); };
    const int32_t sub = lw(P + 0x208c);
    bool latch = false;
    if ((sub == 0 || sub == 0xc) && vec3_length(V(P + 0x150)) == 0.0f && lh(P + 0x30e) == 0 && lh(P + 0x1f6) == 0) latch = true;
    if (!latch) latch = static_cast<uint32_t>(lw(g::HERO_STATE) - 0x18) < 2u;
    if (latch) {
        if ((lw(P + 0x364) & 1) == 0) {
            if (static_cast<uint32_t>(lw(g::HERO_STATE) - 0x18) < 2u) {
                s20 = V(P + 0x4d0);
                const float pi = u2f(0x40490fdau), k = u2f(0x3ee66666u);
                s20.x = s20.x + cos_vu(angle_add(lwc1(P + 0x4e4), pi)) * k;
                const float sy = sin_vu(angle_add(lwc1(P + 0x4e4), pi)) * k;
                s20.y = s20.y + sy;
                s20.z = s20.z - u2f(0xbfb70a3du);
                sub_00260400_v(hm(), plat(), V(P + 0x4d0), V(P + 0x90), s30, s40);
                sub_00260400_v(hm(), plat(), s20, V(P + 0x90), s20, s40);
                sub_00260668_v(hm(), plat(), V(P + 0x4d0), V(P + 0x90), V(P + 0x310), s40);
                sub_00260668_v(hm(), plat(), s20, V(P + 0x90), V(P + 0x330), s40);
            } else if ((lw(info + 0x3c) & 4) != 0) {
                sub_00260668_v(hm(), plat(), V(P + 0x80), V(P + 0x90), V(P + 0x310), s20);
            } else {
                sub_00260400_v(hm(), plat(), V(P + 0x80), V(P + 0x90), s30, s40);
                sub_00260668_v(hm(), plat(), s30, s40, V(P + 0x310), s20);
            }
            sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) | 1u);
        }
    } else {
        sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~1u);
    }
    const int32_t sub2 = lw(P + 0x208c);
    if ((sub2 == 0 || sub2 == 0xc) && lwc1(P + 0x184) == 0.0f && lh(P + 0x30e) == 0) {
        if ((lw(P + 0x364) & 2) == 0) {
            sub_00260668_v(hm(), plat(), V(P + 0x80), V(P + 0x90), s20, V(P + 0x320));
            sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) | 2u);
        }
    } else {
        sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~2u);
    }
    if ((lw(P + 0x364) & 3) != 0) mode = 2;
    vec_clear(s20);
    vec_clear(s30);
    bool store_carry = true;
    if (!(blend == 0.0f)) {
        float len = vec2_length(V(P + 0x350));
        float z = lwc1(P + 0x358);
        if ((lw(info + 0x3c) & 2) == 0) {
            approach_f_v(z, 0.0f, lwc1(0x0015ed70) * 25.0f * blend);
            const float k = lwc1(0x0015ed70);
            approach_f_v(len, 0.0f, (k + k) * blend);
            vec2_set_length(s20, V(P + 0x350), len);
            s20.z = z;
        } else {
            s20 = V(P + 0x350);
        }
        s30 = s20;
        store_carry = false;
    } else if (mode == 1) {
        sub_00260400_v(hm(), plat(), V(P + 0x80), V(P + 0x90), s00, s10);
        swc1(P + 0x98, s10.z);
        vec3_sub(s20, s00, V(P + 0x80));
    } else if (mode == 2) {
        sub_00260400_v(hm(), plat(), V(P + 0x80), V(P + 0x90), s40, s50);
        sub_002604f0_v(hm(), plat(), V(P + 0x310), V(P + 0x320), s60, s70);
        sub_002604f0_v(hm(), plat(), V(P + 0x330), V(P + 0x320), s80, s90);
        const int32_t st = lw(g::HERO_STATE);
        if ((st == 1 || st == 0x1e) && lbu(P + 0x20a5) != 0) {
            swc1(P + 0x98, s50.z);
        } else {
            swc1(P + 0x98, (lw(P + 0x364) & 2) != 0 ? s70.z : s50.z);
        }
        if ((lw(P + 0x364) & 1) == 0) {
            vec3_sub(s20, s40, V(P + 0x80));
            s30 = s20;
        } else if (static_cast<uint32_t>(lw(g::HERO_STATE) - 0x18) < 2u) {
            const float k = u2f(0x3ee66666u);
            sa0.x = cos_vu(lwc1(P + 0x4e4)) * k;
            sa0.y = sin_vu(lwc1(P + 0x4e4)) * k;
            sa0.z = 0.0f;
            vec3_add(sa0, sa0, s80);
            sa0.z = sa0.z + u2f(0xbfb70a3du);
            vec3_sub(s20, s40, V(P + 0x80));
            V(P + 0x4d0) = sa0;
            swc1(P + 0x4e4, atan2_f(s60.x - s80.x, s60.y - s80.y));
        } else {
            vec3_sub(s20, s60, V(P + 0x80));
            const float l = vec3_length(s20);
            if (l < u2f(0x38d1b717u)) {
                vec_clear(s20);
                V(P + 0x80) = s60;
            } else if (2.0f < l) {
                vec_clear(s20);
                sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~3u);
            }
            vec3_add(V(P + 0x4d0), V(P + 0x4d0), s20);
            vec3_sub(sa0, s20, V(P + 0x140));
            if (u2f(0x3dcccccdu) < vec3_length(sa0)) sw(P + 0x364, static_cast<uint32_t>(lw(P + 0x364)) & ~1u);
        }
    } else {
        store_carry = false;
    }
    if (store_carry) V(P + 0x350) = s20;
    vec3_add(V(P + 0xf0), V(P + 0xf0), s20);
    swc1(P + 0xfc, angle_sub(lwc1(P + 0x98), yaw0));
}


// orig: 0x00231e78 — with both hand mobys present (+0x1180 / +0x1184, mode +0x11a4 == 2): switch
// them to the animation pair chosen by sub_00231ff8 at `frame`, blending over `blend` frames (when
// `on` and not already playing); otherwise return them to set 1 over the default blend (frame
// scale 7, or 0x13 in state 8 with slot 3 mode 2) unless sub_00231db0 says to hold the set.
void hero_hands_anim(int32_t on, int32_t frame, float blend) {
    const uint32_t P = g::PLAYER;
    if (lw(P + 0x11a4) != 2) return;
    const uint32_t s1 = lwu(P + 0x1180), s2 = lwu(P + 0x1184);
    if (s1 == 0 || s2 == 0) return;
    int32_t x = 0, y = 0;
    if (sub_00231ff8(static_cast<uint32_t>(on), &x, &y) != 0 && on != 0) {
        if (static_cast<int32_t>(lbu(s1 + 0x53)) == x) return;
        const int32_t frames = float_to_int(blend);
        moby_anim_set(s1, static_cast<uint32_t>(x), frame, frames);
        moby_anim_set(s2, static_cast<uint32_t>(y), frame, frames);
        return;
    }
    bool go = true;
    if (lw(P + 0x208c) == 0) {
        int32_t hold = on;
        if (sub_00231db0(lbu(s1 + 0x53), &hold) != 0 && hold == 0) go = false;
    }
    if (!go) return;
    int32_t frames = frame_scale_i(7);
    if (lw(P + 0x2090) == 8 && sub_0020d498(3) == 2) frames = frame_scale_i(0x13);
    if (lbu(s1 + 0x53) != 1) moby_anim_set(s1, 1, 0, frames);
    if (lbu(s2 + 0x53) != 1) moby_anim_set(s2, 1, 0, frames);
}


// orig: 0x00231d08 — with both hand mobys on set 1: start the animation pair of
// sub_00231ff8(0) on them from frame 0 (blend frame_scale_i(7)).
void hero_hands_anim_start() {
    const uint32_t P = g::PLAYER;
    const uint32_t s1 = lwu(P + 0x1180), s2 = lwu(P + 0x1184);
    if (s1 == 0 || s2 == 0 || lbu(s1 + 0x53) != 1) return;
    int32_t x = 0, y = 0;
    if (sub_00231ff8(0, &x, &y) == 0) return;
    moby_anim_set(s1, static_cast<uint32_t>(x), 0, frame_scale_i(7));
    moby_anim_set(s2, static_cast<uint32_t>(y), 0, frame_scale_i(7));
}

// orig: 0x00232220 — with the moby pair +0x2278 / +0x227c present: put the first on the set
// sub_002321c0(key) gives at `frame` (blend `blend` frames), or back to set 0 when none.
void hero_pair_anim(int32_t key, int32_t frame, float blend) {
    const uint32_t P = g::PLAYER;
    if (lwu(P + 0x2278) == 0 || lwu(P + 0x227c) == 0) return;
    int32_t set = 0;
    if (sub_002321c0(key, &set) != 0) {
        moby_anim_set(lwu(P + 0x2278), static_cast<uint32_t>(set), frame, float_to_int(blend));
    } else if (lbu(lwu(P + 0x2278) + 0x53) != 0) {
        moby_anim_set(lwu(P + 0x2278), 0, 0, frame_scale_i(7));
    }
}


// orig: 0x002323b8 — switch the hero moby to animation `set` (0x31 / 0x32 swapped while
// 0x15edb5 is set) at `frame`: capture the current pose into a free slot as the blend source,
// blend over `blend` frames (> 0) or a fixed table entry -blend - 1 (P+0xaa0, 0x15f6e0), and
// update the hands / pair mobys to match.
void hero_anim_set(uint32_t set, uint32_t frame, float blend) {
    const uint32_t P = g::PLAYER;
    if (g_hero_seq_on) hero_seq_need(set);  // enhancement: a level mod's hero class's empty slot filled first (hero_modes.h)
    const uint32_t hm = lwu(P + 0x2080);
    if ((lwu(P + 0xa98) & 2) != 0) sw(hm + 0x54, 0);
    if (lbu(0x0015edb5) != 0) {
        if (set == 0x31) set = 0x32;
        else if (set == 0x32) set = 0x31;
    }
    const int32_t slot = sub_0024f698(hm);
    if (slot >= 0) {
        moby_pose_capture(hm, static_cast<uint32_t>(slot) | 0x300);
        lq_sq(OV(0x00196d00u) + static_cast<uint32_t>(slot) * 16, hm + 0xf0);
        const uint32_t prev = lbu(hm + 0x52);
        if (prev != 0xff) sb(hm + 0xa5, prev);
        sb(hm + 0x52, 0xff);
        sb(hm + 0x50, static_cast<uint32_t>(slot));
    }
    sb(hm + 0x51, frame);
    sb(hm + 0x53, set);
    sub_0024ebc0(hm);
    swc1(P + 0xa90, 1.0f);
    if (0.0f < blend) {
        swc1(P + 0xa94, fpu_div(1.0f, blend));
        sw(P + 0xaa0, 0xffffffffu);
    } else {
        swc1(P + 0xa94, 1.0f);
        const int32_t idx = float_to_int(-blend) - 1;
        sw(P + 0xaa4, 0);
        sw(P + 0xaa0, static_cast<uint32_t>(idx));
    }
    sw(hm + 0x54, 0);
    sub_00232628();
    sb(hm + 0x7c, lbu(lwu(lwu(hm + 0x24) + set * 4 + 0x48) + 0x11));
    sw(P + 0xa9c, 1);
    if (blend < 0.0f) blend = int_to_float(lw(OV(0x0015f6e0) + static_cast<uint32_t>(lw(P + 0xaa0)) * 4));
    if (lbu(P + 0x20a4) == 0) hero_hands_anim(static_cast<int32_t>(set), static_cast<int32_t>(frame), blend);
    sub_002321a8();
    if (lwu(P + 0x2278) != 0) hero_pair_anim(static_cast<int32_t>(set), static_cast<int32_t>(frame), blend);
}

// Frames until the hero, falling under gravity g from its position and velocity, comes down to
// height h (or the default t when it never does). On foot, a negative h is first replaced by
// the height of a floor (flags 0x24, normal pitch at most ~50 degrees) under the predicted
// position after t frames; then the root of z(n) = z + vz n - g n (n + 1) / 2 = h is taken. In
// move modes 1 / 2 (gravity along -PLAYER+0x270, or towards the centre (100, 100, 100)) the
// predicted segment must hit (flags 0x24) and the root is for the contact's height along the
// local up axis.
// (Aridia's build, L2:0x002360c8, is the same code with the collision flags 0x324: its constant 36 is 804 there.)
// (Blarg's Nebula G34 build, L6:0x00242950, and Oltanis's, L14:0x0023bed0, the same code with the flags 0xd24.)
// orig: 0x00232b90 L2:0x002360c8 L6:0x00242950 L14:0x0023bed0
float hero_fall_time(float t, float g, float h) {
    const auto fall_flags = [] { return on_aridia() ? 0x324u : (on_blarg() || on_oltanis()) ? 0xd24u : 0x24u; };
    const uint32_t P = g::PLAYER;
    const uint32_t mode = lbu(P + 0x20b3);
    const float k = (t * t + t) * 0.5f;
    if (mode == 0) {
        if (h < 0.0f) {
            Vec4 p;
            vec3_scale(p, V(P + 0xe0), t);
            p.z = p.z - k * g;
            vec3_add(p, p, V(P + 0x80));
            Vec4 q = V(P + 0x80);
            sub_0025d238_v(q, p);
            q = V(P + 0x80);
            if (segment_collide_v(q, p, fall_flags(), 0, nullptr) != 0 && sub_00233d30(OV(0x00173e80)) <= u2f(0x3f5f66f3u)) h = lwc1(OV(0x00173e68));
        }
        if (!(0.0f < h)) return t;
        float r_hi = 0.0f, r_lo = 0.0f;
        const float a = g * -0.5f;
        if (solve_quadratic(&r_hi, &r_lo, a, lwc1(P + 0xe8) - a, lwc1(P + 0x88) - h) <= 0) return t;
        return 0.0f < r_hi ? r_hi : t;
    }
    if (mode != 1 && mode != 2) return t;
    Vec4 gv;
    if (mode == 1) {
        gv = V(P + 0x270);
    } else {
        Vec4 c{0.0f, 0.0f, 0.0f, 0.0f};
        c.x = 100.0f;
        c.z = 100.0f;
        c.y = 100.0f;
        vec3_sub(gv, V(P + 0x80), c);
    }
    vec3_set_length(gv, gv, g);
    vec3_scale(gv, gv, -1.0f);
    Vec4 p, d;
    vec3_scale(p, V(P + 0xe0), t);
    vec3_scale(d, gv, k * g);
    vec3_add(p, d, p);
    vec3_add(p, V(P + 0x80), p);
    const Vec4 q = V(P + 0x80);
    sub_0025d238_v(q, p);
    if (segment_collide_v(V(P + 0x80), p, fall_flags(), 0, nullptr) == 0) return t;
    const float a = g * -0.5f;
    const float b = sub_00233a78(P + 0xe0) - a;
    const float z = sub_00233a78(P + 0x80);
    float r_hi = 0.0f, r_lo = 0.0f;
    if (solve_quadratic(&r_hi, &r_lo, a, b, z - sub_00233a78(OV(0x00173e60))) <= 0) return t;
    return 0.0f < r_hi ? r_hi : t;
}

// Camera/anim controller slots: 13 records of 0x90 bytes at 0x17dc50. A request names the slot
// (id & 0xf; flags id & 0xfff0), a key, three controller callbacks (start +0x30, +0x34, +0x38)
// and two arguments (+0x2c, +0x28). The same request again changes nothing; a new one gets the
// next serial number (0x17e4d8) and is pending (+0x68), started at once when the slot was
// flagged 0x20 and is asked again. On level 5 (*(0x15f5c4)) only slots 0 and 2 are used.
// Returns the request's serial (0 when refused).
// orig: 0x00235700
int32_t camera_request(uint32_t id, uint32_t key, uint32_t start, uint32_t cb2, uint32_t cb3, uint32_t arg, uint32_t arg2) {
    const uint32_t slot = id & 0xf, flags = id & 0xfff0;
    const uint32_t e = OV(0x0017dc50) + slot * 0x90;
    if (lw(OV(0x0015f5c4)) == 5 && slot != 2 && slot != 0) return 0;
    if (lwu(e + 0x2c) == arg && lwu(e + 0x28) == arg2 && lwu(e + 0x20) == key && lwu(e + 0x24) == flags && lwu(e + 0x30) == start &&
        lwu(e + 0x34) == cb2 && lwu(e + 0x38) == cb3) {
        return lw(e + 0x64);
    }
    const uint32_t old = lwu(e + 4);
    const uint32_t serial = lwu(OV(0x0017e4d8));
    sw(e + 0x2c, arg);
    sw(e + 0x64, serial);
    sw(e + 0x28, arg2);
    sw(OV(0x0017e4d8), serial + 1);
    sw(e + 0x20, key);
    sw(e + 0x30, start);
    sw(e + 0x34, cb2);
    sw(e + 0x38, cb3);
    sw(e + 0x68, 1);
    sw(e + 0x24, flags);
    sw(e + 0x7c, 0);
    sw(e + 0x70, 0);
    if (((flags & old) & 0x20) != 0) camera_slot_start(e);
    return lw(e + 0x64);
}

// Start a slot's pending request: sub_00235988 with its key, the request becomes current (+4
// flags, +8 / +0xc arguments, +0x10 / +0x14 / +0x18 callbacks) and the start callback runs.
// orig: 0x00235810
void camera_slot_start(uint32_t e) {
    sub_00235988(e, lwu(e + 0x20));
    const uint32_t start = lwu(e + 0x30);
    sw(e + 4, lwu(e + 0x24));
    sw(e + 0x14, lwu(e + 0x34));
    sw(e + 0x18, lwu(e + 0x38));
    sw(e + 0xc, lwu(e + 0x2c));
    sw(e + 8, lwu(e + 0x28));
    sw(e + 0x10, start);
    if (start != 0) ovl_call_moby_fn(start, e);
    sw(e + 0x68, 0);
}

// orig: 0x00235878 — reset the 13 controller slots (an empty request each, -6 at +0x6c).
void camera_slots_reset() {
    for (uint32_t i = 0; i < 0xd; ++i) {
        const uint32_t e = OV(0x0017dc50) + i * 0x90;
        camera_request(i, 0xffff, 0, 0, 0, 0, 1);
        sw(e + 0x7c, 0);
        sw(e + 0x6c, 0xfffffffau);
        camera_slot_start(e);
    }
}

// orig: 0x0023aef8 — reset the controller slots and the camera globals 0x15f968..0x15f970.
void camera_reset() {
    camera_slots_reset();
    sw(OV(0x0015f968), 1);
    sw(OV(0x0015f970), 0xffffffffu);
    sw(OV(0x0015f96c), 0);
}

// orig: 0x00234e00 — slot 1 request 0x7535 (controllers 0x2395b0 / 0x239630 / 0x239810, args
// 0x1415f8, 8).
int32_t camera_request_7535() {
    return camera_request(1, 0x7535, OVF(0x002395b0), OVF(0x00239630), OVF(0x00239810), 0x001415f8, 8);
}

}  // namespace rac1
