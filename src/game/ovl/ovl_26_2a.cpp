// Ported overlay code 0x00260000..0x002affff.
#include <algorithm>
#include <game/enhance/vendor_buy_all.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libc.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// orig: 0x002603d0 — third word of the moby's pvars (flag 0x20 at +0x34).
uint32_t moby_pvar_word2(uint32_t moby) {
    if (moby == 0) return 0;
    if ((lhu(moby + 0x34) & 0x20) == 0) return 0;
    return lwu(lwu(moby + 0x78) + 8);
}

// orig: 0x00262500 — start obj's timer block with v unless already active.
void sub_00262500(uint32_t v, uint32_t obj) {
    if (lw(obj + 0x13c) != 0) return;
    sw(obj + 0x134, v);
    sw(obj + 0x13c, 1);
    sh(obj + 0x130, 0);
    sh(obj + 0x132, 0);
    sw(obj + 0x138, 0);
}

// orig: 0x00265828
void sub_00265828(int32_t flag) {
    constexpr uint32_t base = 0x001516d0;
    if (flag != 0) {
        sh(base + 0x5c, 0x8000);
        sh(base + 0x5e, 0);
    }
    sh(base + 0x78, 0x8000);
    sh(base + 0x7a, 0);
    sh(base + 0x40, 0x8000);
    sh(base + 0x42, 0);
}

// The music slots' pause requests (the music +0x40, the second music +0x78, the dialog +0x5c)
// lifted after `t` frames: their timers set, each only while it is paused (+0xc & 0x8000).
// orig: 0x00265880
void music_pause_timer(int32_t t) {
    constexpr uint32_t base = 0x001516d0;
    if ((lh(base + 0x40) & 0x8000) != 0) sh(base + 0x42, static_cast<uint32_t>(t));
    if ((lh(base + 0x78) & 0x8000) != 0) sh(base + 0x7a, static_cast<uint32_t>(t));
    if ((lh(base + 0x5c) & 0x8000) != 0) sh(base + 0x5e, static_cast<uint32_t>(t));
}

// orig: 0x00266e80 — sample of a 30-entry float ring in the pad block,
// `back` frames ago (clamped to the number of valid samples at pad+0x190).
float pad_stick_history(int32_t back) {
    int32_t n = lw(g::PAD + 0x190);
    if (!(n < back)) n = back;
    const int32_t idx = (static_cast<int32_t>(lh(g::PAD + 0x18e)) - n + 0x1e) % 0x1e;
    return lwc1(g::PAD + static_cast<uint32_t>(idx * 4) + 0x2d0);
}

// orig: 0x0028d8c0 — slots of 0x70 bytes at 0x13e550.
int32_t sub_0028d8c0(uint32_t v, int32_t slot) {
    if (slot < 0) return 0;
    const uint32_t p = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
    if (lwu(p + 0x88) != v) return 0;
    return (static_cast<uint32_t>(lbu(p + 0x74)) - 1u) < 2u ? 1 : 0;
}

// orig: 0x0028d918 — state 7 → free the slot; states other than 0/6 → 4.
void sub_0028d918(int32_t slot) {
    if (slot < 0) return;
    const uint32_t p = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
    const uint32_t st = lbu(p + 0x74);
    if (st == 7) {
        sb(p + 0x74, 0);
        sw(p + 0x88, 0);
        sw(p + 0x8c, 0);
        return;
    }
    if (st == 0 || st == 6) return;
    sb(p + 0x74, 4);
}

// orig: 0x002a07f8
void sub_002a07f8() {
    const uint32_t a = lwu(OV(0x00173e04));
    const uint32_t b = lwu(OV(0x00173e08));
    const uint32_t v = a + lwu(OV(0x001611cc)) - lwu(OV(0x0015f578));
    sw(OV(0x001611b8), a);
    sw(OV(0x001611bc), b);
    sw(OV(0x001611d0), 0);
    sw(OV(0x0015f5fc), v - 0x2000);
    sw(OV(0x001611c0), a);
    sw(OV(0x00166c00u - 0x7608), v);
}

// orig: 0x00262b00 — *ang = angle_add(*ang, delta); obj+0x18 swaps its old offset *off for
// the new one, sin(*ang) * scale.
void sub_00262b00(uint32_t obj, uint32_t ang, uint32_t off, float scale, float delta) {
    swc1(ang, angle_add(lwc1(ang), delta));
    swc1(obj + 0x18, lwc1(obj + 0x18) - lwc1(off));
    const float o = sin_vu(lwc1(ang)) * scale;
    swc1(off, o);
    swc1(obj + 0x18, lwc1(obj + 0x18) + o);
}

// orig: 0x002a0858 — flip the double-buffered packet (0x1611b8[2], index 0x1611d0).
void sub_002a0858() {
    const uint32_t idx = 1u - lwu(OV(0x001611d0));
    const uint32_t a = lwu(OV(0x001611cc));
    const uint32_t c = lwu(OV(0x0015f578));
    const uint32_t cur = lwu(OV(0x001611c0));
    const uint32_t buf = lwu(OV(0x001611b8) + (idx << 2));
    sw(OV(0x001611c4), cur);
    const uint32_t v = buf + a - c;
    sw(OV(0x001611d0), idx);
    sw(OV(0x001611c0), buf);
    sw(OV(0x0015f5f8), v);
    sw(OV(0x00166c00u - 0x7604), v - 0x2000);
}

// orig: 0x00262c50 — d = r * (cos a cos b, sin a cos b, sin b); d.w untouched.
void vec_from_spherical_v(Vec4& d, float r, float a, float b) {
    d.x = (cos_vu(a) * r) * cos_vu(b);
    d.y = (sin_vu(a) * r) * cos_vu(b);
    d.z = sin_vu(b) * r;
}
// orig: 0x00262c50 (EE-address form)
void vec_from_spherical(uint32_t dst, float r, float a, float b) { vec_from_spherical_v(V(dst), r, a, b); }

// Auto-aim from `from` along `dir`: *yaw / *pitch start as dir's angles (pitch negated). Over
// the moby list, targetable mobys (+0x20 >= 0, flag 0x1000, class kind +0x46 == 5) are aimed
// at their centre (pvars word 0's +0x10 above the origin, else 0.5). One within 7.5 that lies
// within 60 degrees of the hero's facing and pitch is taken at once. Otherwise the nearest one
// whose angular size around the aim ray (2 asin(miss / 2d), less its radius pvars +0xa / 8 when
// the miss exceeds the cone) is under `cone` and that the camera 0x166dc0 sees (flags 6,
// `ignore` skipped); its angles go to *yaw / *pitch and its centre height to *height. The aim
// ray's pitch is raised by half PLAYER+0x2e4. Returns the moby or 0.
// orig: 0x002630c8
uint32_t auto_aim_target(uint32_t ignore, const Vec4& from, const Vec4& dir, float* yaw, float* pitch, float* height, float cone) {
    const uint32_t P = g::PLAYER;
    float best_d = u2f(0x461c4000u);
    uint32_t best = 0;
    *yaw = atan2_f(dir.x, dir.y);
    *pitch = -atan2_f(vec2_length(dir), dir.z);
    for (uint32_t m = lwu(OV(0x0015ffe4)); m != 0; m = lwu(m + 0x28)) {
        if (static_cast<int8_t>(lbu(m + 0x20)) < 0) continue;
        Vec4 tgt = V(m + 0x10);
        const uint32_t pw = moby_pvar_word0(m);
        tgt.z = tgt.z + (pw != 0 ? lwc1(pw + 0x10) : 0.5f);
        if ((lhu(m + 0x34) & 0x1000) == 0) continue;
        const uint32_t cls = lwu(m + 0x24);
        if (cls == 0) continue;
        if (lh(cls + 0x46) != 5) continue;
        const float ty = atan2_f(tgt.x - from.x, tgt.y - from.y);
        const float tp = atan2_f(vec2_distance(from, tgt), tgt.z - from.z);
        const float d = vec3_distance(from, tgt);
        if (d < 7.5f) {
            const float lim = u2f(0x3f860a92u);
            const float a = atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84));
            if (angle_dist(lwc1(lwu(P + 0x2080) + 0x48), a) < lim && abs_f(tp) < lim) {
                *pitch = -tp;
                *yaw = ty;
                return m;
            }
        }
        if (best_d < d) continue;
        Vec4 ray{0.0f, 0.0f, 0.0f, 0.0f};
        vec_from_spherical_v(ray, d, *yaw, *pitch + lwc1(P + 0x2e4) * 0.5f);
        vec3_add(ray, ray, from);
        float size = asin_f(fpu_div(vec3_distance(ray, tgt), d + d));
        size = size + size;
        if (cone < size && pw != 0) {
            const float r = int_to_float(static_cast<int32_t>(lbu(pw + 0xa) << 3)) * 0.125f;
            const float s = asin_f(fpu_div(r, d));
            if (r < d) size = size - s;
        }
        if (!(size < cone)) continue;
        if (segment_collide_v(V(OV(0x00166dc0)), tgt, 6, ignore, nullptr) != 0) continue;
        *pitch = -tp;
        best = m;
        best_d = d;
        *yaw = ty;
        *height = pw != 0 ? lwc1(pw + 0x10) : 0.5f;
    }
    return best;
}
// orig: 0x002630c8 (EE-address form)
uint32_t auto_aim_target(uint32_t ignore, uint32_t from, uint32_t dir, uint32_t yaw, uint32_t pitch, uint32_t height, float cone) {
    const Vec4 f = V(from), d = V(dir);
    return auto_aim_target(ignore, f, d, static_cast<float*>(ee_ram_ptr(yaw)), static_cast<float*>(ee_ram_ptr(pitch)), static_cast<float*>(ee_ram_ptr(height)), cone);
}

// Spawn a class 0x100 shot from `owner` at pos, facing (yaw, pitch) = (ry, rx) and scaled by
// *(0x1613c4). Without a target, auto_aim_target picks one along that direction (range 20 x
// frame scale, cone 0.35), which also overrides (aim_yaw, aim_pitch, aim_h). Its pvars: owner
// +0x10, target +0x18, steering on (+0x38, off for targets whose pvars flag +0xb), speed
// +0x34 (100 x frame scale or the target's +0xc x frame scale), aim angles +0x2c / +0x30, life
// +0x14 (150 frames), the target point +0 (its centre +aim_h, aim_h at +0x1c), +0x3c / +0x3e
// timers (60 frames when the hero moves faster than 0.1 x frame scale, else 1). A wall between
// the owner and pos (flags 0, ignoring the hero moby) moves it to the contact (+0xbc = 1).
// orig: 0x002a96f8
uint32_t spawn_projectile_100_v(uint32_t owner, const Vec4& pos, uint32_t target, float aim_yaw, float aim_pitch, float aim_h, float ry, float rx) {
    float yaw = aim_yaw, pitch = aim_pitch, h = aim_h;
    const uint32_t m = moby_spawn(0x100);
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    sh(m + 0x32, 0xff);
    const float sc = lwc1(m + 0x2c) * lwc1(OV(0x001613c4));
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x48, ry);
    swc1(m + 0x44, rx);
    swc1(m + 0x2c, sc);
    if (target == 0) {
        Vec4 dir{0.0f, 0.0f, 0.0f, 0.0f};
        vec_from_spherical_v(dir, lwc1(0x0015ed6c) * 20.0f, ry, rx);
        const Vec4 from = pos;
        target = auto_aim_target(owner, from, dir, &yaw, &pitch, &h, u2f(0x3eb2b8c2u));
    }
    sw(pv + 0x10, owner);
    V(m + 0x10) = pos;
    sw(pv + 0x38, 1);
    sw(pv + 0x24, 0);
    sw(pv + 0x28, 0);
    sw(pv + 0x20, 0);
    swc1(pv + 0x34, lwc1(0x0015ed6c) * 100.0f);
    sw(pv + 0x18, target);
    if (target != 0) {
        const uint32_t pw = moby_pvar_word0(target);
        if (pw != 0) {
            sh(pw + 0x1e, lhu(pw + 0x1e) | 0x80u);
            if (lbu(pw + 0xb) != 0) sw(pv + 0x38, 0);
            if (lbu(pw + 0xc) != 0) swc1(pv + 0x34, static_cast<float>(lbu(pw + 0xc)) * lwc1(0x0015ed6c));
        }
    }
    swc1(pv + 0x2c, yaw);
    swc1(pv + 0x30, pitch);
    sw(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0x96)));
    if (target != 0) {
        V(pv) = V(target + 0x10);
        swc1(pv + 8, lwc1(pv + 8) + h);
        swc1(pv + 0x1c, h);
    }
    const bool fast = lwc1(0x0015ed6c) * u2f(0x3dcccccdu) < vec3_length(V(g::PLAYER + 0x140));
    const int32_t t = frame_scale_i(fast ? 0x3c : 1);
    sh(pv + 0x3c, static_cast<uint32_t>(t));
    sh(pv + 0x3e, static_cast<uint32_t>(t));
    Vec4 a = V(owner + 0x10);
    a.z = pos.z;
    if (segment_collide_v(a, pos, 0, lwu(g::PLAYER + 0x2080), nullptr) != 0) {
        sb(m + 0xbc, 1);
        V(m + 0x10) = V(OV(0x00173e60));
    }
    moby_update_transform(m);
    sub_0025f780(m, OV(0x001b0770), 0x1f);
    swc1(m + 0x2c, lwc1(m + 0x2c) + lwc1(m + 0x2c));
    return m;
}
// orig: 0x002a96f8 (EE-address form)
uint32_t spawn_projectile_100(uint32_t owner, uint32_t pos, uint32_t target, float aim_yaw, float aim_pitch, float aim_h, float ry, float rx) {
    return spawn_projectile_100_v(owner, V(pos), target, aim_yaw, aim_pitch, aim_h, ry, rx);
}

// orig: 0x00266d60 — most recent of the last `back` frames (ring at pad+0x1e0) where a button
// in `mask` was held: returns those bits and stores how many frames ago in *ago.
uint32_t pad_button_history(uint32_t mask, int32_t back, int32_t* ago) {
    int32_t n = lw(g::PAD + 0x190);
    if (!(n < back)) n = back;
    const int32_t end = n + 1;
    for (int32_t k = 1; k < end; ++k) {
        const int32_t idx = (static_cast<int32_t>(lh(g::PAD + 0x18e)) - k + 0x1e) % 0x1e;
        const uint32_t w = lwu(g::PAD + 0x1e0 + static_cast<uint32_t>(idx * 4));
        if ((w & mask) != 0) {
            if (ago != nullptr) *ago = k - 1;
            return w & mask;
        }
    }
    return 0;
}

// orig: 0x00262cf0 — dst = base + spherical(r, a, b). dst.w is the w lane of an
// uninitialised stack vector in the original; the port writes 0.
void sub_00262cf0(uint32_t dst, uint32_t base, float r, float a, float b) {
    Vec4 d{0.0f, 0.0f, 0.0f, 0.0f};
    vec_from_spherical_v(d, r, a, b);
    vec3_add(V(dst), d, V(base));
}

// orig: 0x00263d40 — copy the name of record v (sub_00203ad8) to 0x179118 and set the
// display timer 0x15f640 (-1: 180 frames).
void sub_00263d40(int32_t v, int32_t frames) {
    if (frames == -1) frames = frame_scale_i(0xb4);
    libc_strcpy(OV(0x00179118), sub_00203ad8(v));
    sw(OV(0x0015f640), static_cast<uint32_t>(frames));
}

// orig: 0x00266e00 — both button sets pressed within the last `back` frames, less than
// `max_gap` frames apart.
int32_t pad_buttons_together(uint32_t mask_a, uint32_t mask_b, int32_t back, int32_t max_gap) {
    int32_t ago_a, ago_b;
    if (pad_button_history(mask_a, back, &ago_a) == 0) return 0;
    if (pad_button_history(mask_b, back, &ago_b) == 0) return 0;
    return abs_i(ago_a - ago_b) < max_gap ? 1 : 0;
}

// orig: 0x00262b80 — dst+0x40/+0x44 = r sin(*a) (sin, cos)(*b); then advance both angles.
void sub_00262b80(uint32_t dst, uint32_t a, uint32_t b, float r, float da, float db) {
    const float x = (r * sin_vu(lwc1(a))) * sin_vu(lwc1(b));
    swc1(dst + 0x40, x);
    const float y = (r * sin_vu(lwc1(a))) * cos_vu(lwc1(b));
    swc1(dst + 0x44, y);
    swc1(a, angle_add(lwc1(a), da));
    swc1(b, angle_add(lwc1(b), db));
}

// orig: 0x002a0ba8 — append a 0x30-byte GIF packet (A+D, one register write: data `d`
// to register `reg`) at the packet cursor 0x1611c0.
void sub_002a0ba8(uint32_t reg, uint64_t d) {
    constexpr OvConst<0x001611c0u> CUR{};
    sw(lwu(CUR) + 0x00, 0x10000002u);
    sw(lwu(CUR) + 0x04, 0);
    sw(lwu(CUR) + 0x08, 0);
    sw(lwu(CUR) + 0x0c, 0x50000002u);
    sw(lwu(CUR) + 0x10, 0x8001u);
    sw(lwu(CUR) + 0x14, 0x10000000u);
    sw(lwu(CUR) + 0x18, 0xeu);
    sw(lwu(CUR) + 0x1c, 0);
    sd(lwu(CUR) + 0x20, d);
    sw(lwu(CUR) + 0x28, reg);
    sw(lwu(CUR) + 0x2c, 0);
    sw(CUR, lwu(CUR) + 0x30);
}

// orig: 0x002678b8 (sets a1 = 0 and jumps into 0x002678c0) — allocate one of the 0x800
// 64-byte particles: index *(0x160170) (bitmap 0x1b1880), advance the next-free index,
// update the high-water mark *(0x160174) and the count *(0x160178), clear the first 32 bytes
// and store `kind` in byte 0. Returns the particle (base *(0x16016c)) or 0 when full.
uint32_t alloc_particle(uint32_t kind) {
    const uint32_t NEXT = OV(0x00160170), HIGH = OV(0x00160174), COUNT = OV(0x00160178), BASE = OV(0x0016016c);
    constexpr OvConst<0x001b1880u> BITS{};
    int32_t t8 = lw(NEXT);
    const int32_t hi = lw(HIGH);
    if (t8 - 0x800 >= 0) return 0;
    const int32_t idx = t8;
    const uint32_t byte = BITS + (static_cast<uint32_t>(idx) >> 3);
    sb(byte, lbu(byte) | (1u << (idx & 7)));
    const bool search = !(t8 - hi > 0);
    t8 += 1;
    if (search) {
        uint32_t at = static_cast<uint32_t>(t8) >> 3;
        t8 = static_cast<int32_t>(at << 3);
        for (;;) {
            if (t8 - 0x800 >= 0) break;
            const uint32_t bits = lbu(at + BITS);
            uint32_t bit = 1;
            bool found = false;
            for (;;) {
                const uint32_t busy = bits & bit;
                bit <<= 1;
                if (busy == 0) { found = true; break; }
                t8 += 1;
                if ((bit & 0xffu) == 0) break;
            }
            if (found) break;
            at += 1;
        }
    }
    sw(NEXT, static_cast<uint32_t>(t8));
    sw(HIGH, static_cast<uint32_t>(hi > idx ? hi : idx));
    sw(COUNT, lwu(COUNT) + 1u);
    const uint32_t p = (static_cast<uint32_t>(idx) << 6) + lwu(BASE);
    for (uint32_t i = 0; i < 0x20; i += 8) sd(p + i, 0);
    sb(p, kind);
    return p;
}

// Frees particle p: its bits in the busy map 0x1b1880 (and the 0x40-flag map 0x1b1980) cleared,
// the count down, the high-water mark *(0x160174) lowered to the highest busy one when it was p,
// the next-free index *(0x160170) at most p's; p's byte 1 marks it free (0x80).
// orig: 0x00267a08
void free_particle(uint32_t p) {
    const uint32_t NEXT = OV(0x00160170), HIGH = OV(0x00160174), COUNT = OV(0x00160178), BASE = OV(0x0016016c);
    constexpr OvConst<0x001b1880u> BITS{};
    const int32_t idx = static_cast<int32_t>((p - lwu(BASE)) >> 6);
    const uint32_t mask = 0xffu ^ (1u << (idx & 7));
    sw(COUNT, lwu(COUNT) - 1u);
    if ((lbu(p + 1) & 0x40u) != 0) sb(BITS + 0x100 + (static_cast<uint32_t>(idx) >> 3), lbu(BITS + 0x100 + (static_cast<uint32_t>(idx) >> 3)) & mask);
    sb(BITS + (static_cast<uint32_t>(idx) >> 3), lbu(BITS + (static_cast<uint32_t>(idx) >> 3)) & mask);
    int32_t hi = lw(HIGH);
    if (idx == hi) {
        hi -= 1;
        while (hi >= 0 && (lbu(BITS + (static_cast<uint32_t>(hi) >> 3)) & (1u << (hi & 7))) == 0) hi -= 1;
        sw(HIGH, static_cast<uint32_t>(hi));
    }
    sb(p + 1, 0x80);
    sw(NEXT, static_cast<uint32_t>(std::min(lw(NEXT), idx)));
}

// A kind 0x3b particle (a glow) at `pos`, colour `color`, frame `frame`, texture from the
// table 0x1b2080[tbl], additive when `alt` (+3 0x44), `life` frames, size size * 210000;
// `rel` 1 keeps it relative to the hero (+0x20 = pos less the hero's).
// orig: 0x00272f68
uint32_t spawn_particle_3b_v(const Vec4& pos, uint32_t color, uint32_t frame, uint32_t tbl, uint32_t alt, uint32_t life, uint32_t rel, float size) {
    const uint32_t p = alloc_particle(0x3b);
    if (p == 0) return 0;
    sw(p + 0x30, rel);
    if (rel == 1) vec3_sub(V(p + 0x20), pos, V(0x0013f3d0));
    V(p + 0x10) = pos;
    sw(p + 4, color);
    sb(p + 9, static_cast<uint32_t>(float_to_int(1.0f) + 0x20));
    sb(p + 3, alt != 0 ? 0x44u : 0x48u);
    sb(p + 8, frame & 0xffu);
    sb(p + 1, 0);
    swc1(p + 0xc, size * 210000.0f);
    sh(p + 0xa, life);
    sb(p + 2, lbu(lwu(OV(0x001b2080) + tbl * 4)));
    return p;
}
// orig: 0x00272f68 (EE-address form)
uint32_t spawn_particle_3b(uint32_t pos, uint32_t color, uint32_t frame, uint32_t tbl, uint32_t alt, uint32_t life, uint32_t rel, float size) {
    return spawn_particle_3b_v(V(pos), color, frame, tbl, alt, life, rel, size);
}

// A kind 0x3c particle (a moving glow) at `pos` moving by `vel`, colour `color`, `life`
// frames (byte), frame `frame`, size size * 210000; `rel` 1 keeps it relative to the hero
// (+0x30 = pos less the hero's). None outside 2..1021 on any axis.
// orig: 0x002730e0
uint32_t spawn_particle_3c_v(const Vec4& pos, const Vec4& vel, uint32_t color, uint32_t life, uint32_t frame, uint32_t rel, float size) {
    const float lo = 2.0f, hi = u2f(0x447f4000u);
    if (pos.x < lo || hi < pos.x || pos.y < lo || hi < pos.y || pos.z < lo || hi < pos.z) return 0;
    const uint32_t p = alloc_particle(0x3c);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    V(p + 0x20) = vel;
    sw(p + 0x3c, rel);
    if (rel == 1) {
        swc1(p + 0x30, lwc1(p + 0x10) - lwc1(0x0013f3d0));
        swc1(p + 0x34, lwc1(p + 0x14) - lwc1(0x0013f3d4));
        swc1(p + 0x38, lwc1(p + 0x18) - lwc1(0x0013f3d8));
    }
    sw(p + 4, color);
    sb(p + 9, static_cast<uint32_t>(float_to_int(1.0f) + 0x20));
    sb(p + 3, 0x48);
    sb(p + 8, frame & 0xffu);
    sb(p + 1, 0);
    swc1(p + 0xc, size * 210000.0f);
    sh(p + 0xa, life & 0xffu);
    sb(p + 2, lbu(lwu(OV(0x001b2154))));
    return p;
}
// orig: 0x002730e0 (EE-address form)
uint32_t spawn_particle_3c(uint32_t pos, uint32_t vel, uint32_t color, uint32_t life, uint32_t frame, uint32_t rel, float size) {
    return spawn_particle_3c_v(V(pos), V(vel), color, life, frame, rel, size);
}

// A kind 0x3e particle (a sparkle) at `pos`: grey-blue 0x7f7f4040, size 30000, random frame,
// `life` frames (fading 127 / life a frame), kind `kind` (+0x22; texture 0x1b2178[kind < 2]).
// orig: 0x00273708
uint32_t spawn_particle_3e_v(const Vec4& pos, int32_t life, uint32_t kind) {
    const uint32_t p = alloc_particle(0x3e);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    sw(p + 4, 0x7f7f4040u);
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    swc1(p + 0xc, u2f(0x46ea6000u));
    sb(p + 9, static_cast<uint32_t>(s + 0x40));
    sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
    sh(p + 0x22, kind);
    const uint32_t k = static_cast<int32_t>(kind) < 2 ? kind : 0u;
    sb(p + 2, lbu(lwu(OV(0x001b2178)) + k));
    sh(p + 0x20, static_cast<uint32_t>(float_to_int(fpu_div(127.0f, int_to_float(life)))));
    sh(p + 0xa, static_cast<uint32_t>(life));
    return p;
}
// orig: 0x00273708 (EE-address form)
uint32_t spawn_particle_3e(uint32_t pos, int32_t life, uint32_t kind) { return spawn_particle_3e_v(V(pos), life, kind); }

// orig: 0x00271b18 — spawn particle kind 0x2f at pos with velocity vel.
uint32_t spawn_particle_2f_v(const Vec4& pos, const Vec4& vel, float f) {
    const uint32_t p = alloc_particle(0x2f);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    const int32_t r = rand_mod(8);
    sw(p + 4, (static_cast<uint32_t>(r + 0x28) << 24) | 0x00808080u);
    const int32_t s = float_to_int(2.0f);
    sb(p + 9, static_cast<uint32_t>(s + 0x20));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    const uint32_t c = lbu(lwu(OV(0x001b213c)));
    swc1(p + 0xc, f);
    sb(p + 2, c);
    V(p + 0x20) = vel;
    sb(p + 8, static_cast<uint32_t>(rand_mod(0x100)));
    return p;
}
// orig: 0x00271b18 (EE-address form)
uint32_t spawn_particle_2f(uint32_t pos, uint32_t vel, float f) { return spawn_particle_2f_v(V(pos), V(vel), f); }

// orig: 0x00274cf8 — spawn particle kind 0x46 at pos (colour index idx, size f).
uint32_t spawn_particle_46_v(const Vec4& pos, uint32_t idx, float f) {
    const uint32_t p = alloc_particle(0x46);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    sw(p + 4, 0x80404040u);
    const int32_t s = float_to_int(2.0f);
    const float k = f * 128.0f;
    sb(p + 3, 0x44);
    sb(p + 1, 1);
    sb(p + 9, static_cast<uint32_t>(s + 0x20));
    const float q = fpu_div(k, u2f(0x40490fdbu));
    sb(p + 2, lbu(lwu(OV(0x001b2198)) + idx));
    sb(p + 8, static_cast<uint32_t>(float_to_int(q) - 0x20));
    swc1(p + 0xc, u2f(0x480f8e00u));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x78)));
    return p;
}
// orig: 0x00274cf8 (EE-address form)
uint32_t spawn_particle_46(uint32_t pos, uint32_t idx, float f) { return spawn_particle_46_v(V(pos), idx, f); }

// orig: 0x0028c8b0 — map x from [lo, hi] onto the int range [obj+8, obj+0xc] (reversed:
// x <= lo gives +0xc), linearly or (bit 0 of +0x19) quadratically.
int32_t sub_0028c8b0(uint32_t obj, float x, float lo, float hi) {
    if (x <= lo) return lw(obj + 0xc);
    if (hi <= x) return lw(obj + 8);
    const float range = int_to_float(lw(obj + 0xc) - lw(obj + 8));
    float n = hi - x, dd = hi - lo;
    if ((lbu(obj + 0x19) & 1) != 0) {
        n = n * n;
        dd = dd * dd;
    }
    return lw(obj + 8) + float_to_int(fpu_div(n * range, dd));
}

// orig: 0x0028c9a0 — sub_0028c8b0 on the range object *(obj+8) with the distance of pos
// from the camera position 0x166dc0 and the range's limits.
int32_t sub_0028c9a0(uint32_t obj, uint32_t pos) {
    const float d = vec3_distance(V(pos), V(OV(0x00166dc0)));
    const uint32_t r = lwu(obj + 8);
    return sub_0028c8b0(r, d, lwc1(r + 0), lwc1(r + 4));
}

// orig: 0x00269958 — spawn particle kind 4 (8 register arguments: pos, vel, colour, a, b,
// c, d, additive flag).
void spawn_particle_4_v(const Vec4& pos, const Vec4& vel, uint32_t colour, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                      int32_t additive) {
    const uint32_t p = alloc_particle(4);
    if (p == 0) return;
    V(p + 0x10) = pos;
    sw(p + 4, colour);
    const int32_t s = float_to_int(4.0f);
    sb(p + 9, static_cast<uint32_t>(s - 0x60));
    sb(p + 3, additive != 0 ? 0x48 : 0x44);
    sb(p + 1, 0);
    sb(p + 2, lbu(lwu(OV(0x001b2090))));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sw(p + 0xc, 0);
    V(p + 0x10) = pos;
    const uint32_t q = p + 0x20;
    V(q) = vel;
    sh(p + 0xa, b);
    sh(q + 0x1a, c);
    sw(q + 0x1c, b);
    sw(q + 0x10, colour);
    sh(q + 0x18, d);
    sw(q + 0x14, a);
}
// orig: 0x00269958 (EE-address form)
void spawn_particle_4(uint32_t pos, uint32_t vel, uint32_t colour, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                      int32_t additive) {
    spawn_particle_4_v(V(pos), V(vel), colour, a, b, c, d, additive);
}

// orig: 0x0026f548 — spawn particle kind 0x23 at pos with velocity vel.
void spawn_particle_23_v(const Vec4& pos, const Vec4& vel, int32_t kind, uint32_t a) {
    const uint32_t p = alloc_particle(0x23);
    if (p == 0) return;
    V(p + 0x10) = pos;
    sw(p + 4, 0x80808080u);
    const int32_t s = float_to_int(2.0f);
    sb(p + 1, 0);
    sb(p + 9, static_cast<uint32_t>(s + 0x10));
    sb(p + 3, 0x44);
    swc1(p + 0xc, rand_range_f(10500.0f, 16800.0f));
    sb(p + 2, lbu(lwu(OV(0x001b210c))));
    const uint32_t q = p + 0x20;
    V(q) = vel;
    sw(q + 0x14, a);
    sw(q + 0x10, static_cast<uint32_t>(kind));
    swc1(q + 0x18, lwc1(p + 0x18));
    if (kind == 2) sw(q + 0x18, 0);
    const float k = lwc1(0x0015ed70);
    swc1(q + 0x1c, rand_range_f(k * 10.5f, k * 18.0f));
}
// orig: 0x0026f548 (EE-address form)
void spawn_particle_23(uint32_t pos, uint32_t vel, int32_t kind, uint32_t a) { spawn_particle_23_v(V(pos), V(vel), kind, a); }

// orig: 0x002718d0 — spawn particle kind 0x2e at pos with velocity vel.
uint32_t spawn_particle_2e_v(const Vec4& pos, const Vec4& vel, uint32_t a, float size, float b) {
    const uint32_t p = alloc_particle(0x2e);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    const uint32_t q = p + 0x20;
    const int32_t r = rand_mod(10);
    sw(p + 4, (static_cast<uint32_t>(r + 0x26) << 24) | 0x00808080u);
    const int32_t s = float_to_int(2.0f);
    sb(p + 9, static_cast<uint32_t>(s + 0x20));
    sb(p + 3, 0x48);
    sb(p + 1, 1);
    const uint32_t c = lbu(lwu(OV(0x001b2138)));
    sh(p + 0xa, 5);
    sb(p + 2, c);
    swc1(p + 0xc, size * u2f(0x484d1400u));
    V(q) = vel;
    const float f = rand_range_f(0.0f, 256.0f);
    swc1(q + 0x10, f);
    sb(p + 8, static_cast<uint32_t>(float_to_int(f)));
    sw(q + 0x18, a);
    swc1(q + 0x14, b);
    return p;
}
// orig: 0x002718d0 (EE-address form)
uint32_t spawn_particle_2e(uint32_t pos, uint32_t vel, uint32_t a, float size, float b) { return spawn_particle_2e_v(V(pos), V(vel), a, size, b); }

// orig: 0x00260400 — transform a position/rotation from the frame of the moby the given moby
// is attached to (its pvar word 2: angles at +0, offset at +0x10) into the moby's frame.
// Returns 0 when not attached. (First argument unused.)
int32_t sub_00260400_v(uint32_t unused, uint32_t moby, Vec4& pos, Vec4& ang, Vec4& out_pos, Vec4& out_ang) {
    (void)unused;
    const uint32_t parent = moby_pvar_word2(moby);
    if (parent == 0) return 0;
    Mat4 a;
    euler_to_mat4_vu(a, V(parent));
    vec3_add(out_pos, pos, V(parent + 0x10));
    vec3_sub(out_pos, out_pos, V(moby + 0x10));
    mat3_transform(out_pos, out_pos, a);
    vec3_add(out_pos, out_pos, V(moby + 0x10));
    Mat4 r;
    euler_to_mat4_vu(r, ang);
    mat4_mul(r, a, r);
    mat_to_euler_r(r, out_ang);
    return 1;
}
// orig: 0x00260400 (EE-address form)
int32_t sub_00260400(uint32_t unused, uint32_t moby, uint32_t pos, uint32_t ang, uint32_t out_pos, uint32_t out_ang) {
    return sub_00260400_v(unused, moby, V(pos), V(ang), V(out_pos), V(out_ang));
}

// orig: 0x00260668 — position/rotation into the local frame of the moby (when attached).
int32_t sub_00260668_v(uint32_t unused, uint32_t moby, Vec4& pos, Vec4& ang, Vec4& out_pos, Vec4& out_ang) {
    (void)unused;
    if (moby_pvar_word2(moby) == 0) return 0;
    Vec4 d;
    vec3_sub(d, pos, V(moby + 0x10));
    Mat4 t;
    mat3_transpose(t, M(moby + 0xc0));
    mat4_transform(out_pos, d, t);
    Mat4 r;
    euler_to_mat4_vu(r, ang);
    mat4_mul(r, t, r);
    mat_to_euler_r(r, out_ang);
    return 1;
}
// orig: 0x00260668 (EE-address form)
int32_t sub_00260668(uint32_t unused, uint32_t moby, uint32_t pos, uint32_t ang, uint32_t out_pos, uint32_t out_ang) {
    return sub_00260668_v(unused, moby, V(pos), V(ang), V(out_pos), V(out_ang));
}

// An attachment frame: the angles at +0 of `out` are `ang` seen from `base_ang` (the rotation
// base_ang^T * ang), the offset at +0x10 is `pos`.
// orig: 0x00260738
void attach_frame_set_v(uint32_t out, const Vec4& pos, const Vec4& base_ang, const Vec4& ang) {
    Mat4 a;
    euler_to_mat4_vu(a, base_ang);
    mat3_transpose(a, a);
    Mat4 b;
    euler_to_mat4_vu(b, ang);
    Mat4 c;
    mat4_mul(c, a, b);
    mat_to_euler_v(c, out);
    V(out + 0x10) = pos;
}
// orig: 0x00260738 (EE-address form)
void attach_frame_set(uint32_t out, uint32_t pos, uint32_t base_ang, uint32_t ang) { attach_frame_set_v(out, V(pos), V(base_ang), V(ang)); }

// orig: 0x0026d598 — spawn particle kind 0x19 (white, 60% alpha) at pos with velocity vel;
// its growth vector is +1 or (flag) -1.
void spawn_particle_19_v(const Vec4& pos, const Vec4& vel, int32_t flag) {
    const uint32_t p = alloc_particle(0x19);
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    V(p + 0x10) = pos;
    const float g = flag == 0 ? 1.0f : -1.0f;
    sw(p + 4, rgba_pack(1.0f, 1.0f, 1.0f, u2f(0x3f19999au)));
    swc1(q + 0x18, g);
    swc1(q + 0x10, g);
    swc1(q + 0x14, g);
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    sb(p + 9, static_cast<uint32_t>(s + 0x40));
    sb(p + 3, 0x48);
    swc1(p + 0xc, rand_range_f(5000.0f, 30000.0f));
    sb(p + 8, 0);
    sb(p + 2, lbu(lwu(OV(0x001b20e4))));
    swc1(q + 0x1c, u2f(0x3f19999au));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x50)));
    V(q) = vel;
}
// orig: 0x0026d598 (EE-address form)
void spawn_particle_19(uint32_t pos, uint32_t vel, int32_t flag) { spawn_particle_19_v(V(pos), V(vel), flag); }

// A kind 0x13 particle (a hit streak): at `pos` heading `dir`, one of six colours, its trail
// start pos - 2 dir.
// orig: 0x0026c6e8
void spawn_particle_13_v(const Vec4& pos, const Vec4& dir) {
    const uint32_t p = alloc_particle(0x13);
    if (p == 0) return;
    const uint32_t c = (static_cast<uint32_t>(rand_mod(6)) * 6u + 0x40u) | 0x78404000u;
    sw(p + 0xc, c);
    sw(p + 4, c);
    sb(p + 9, static_cast<uint32_t>(float_to_int(2.0f) + 0x20));
    sb(p + 3, 0x48);
    sb(p + 1, 3);
    sb(p + 2, lbu(lwu(OV(0x001b20cc))));
    V(p + 0x10) = pos;
    V(p + 0x30) = dir;
    vec3_sub(V(p + 0x20), pos, V(p + 0x30));
    vec3_sub(V(p + 0x20), V(p + 0x20), V(p + 0x30));
    swc1(p + 0x1c, u2f(0x3db851ecu));
    swc1(p + 0x2c, 0.5f);
}

// A kind 0x15 particle (a hit flash): at `pos` moving `vel`, colours `c0` / `c1`, `n` frames,
// +0x3a `b`, size `size`. None without frames. Returns it.
// orig: 0x0026cbb0
uint32_t spawn_particle_15_v(const Vec4& pos, const Vec4& vel, uint32_t c0, uint32_t c1, uint32_t n, uint32_t b, float size) {
    if (n == 0) return 0;
    const uint32_t p = alloc_particle(0x15);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    sw(p + 4, c0);
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    swc1(p + 0xc, size);
    sb(p + 3, 0x48);
    sb(p + 9, static_cast<uint32_t>(s - 0x60));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    const uint32_t q = p + 0x20;
    sh(p + 0xa, n);
    sb(p + 2, lbu(lwu(OV(0x001b20d4))));
    sh(q + 0x18, n);
    swc1(q + 0x1c, size);
    sw(q + 0x10, c0);
    sw(q + 0x14, c1);
    sh(q + 0x1a, b);
    V(q) = vel;
    return p;
}

// A kind 0xb particle (debris spark) at `pos`, thrown at `vel` plus a random direction of
// length `speed`: none without a count, and only one in 3 / 2 / never when the frame load
// 0x15f5d0 / 0x15f5d4 is over 0.85 / 0.9 / 1.0. Colour `color`, size speed * 0.75.
// orig: 0x0026a9f0
void spawn_particle_0b_v(const Vec4& pos, const Vec4& vel, uint32_t color, uint32_t a3, uint32_t count, uint32_t b38, uint32_t b3a, uint32_t b39, float f3c, float speed) {
    if (count == 0) return;
    if ((0.85f < lwc1(OV(0x0015f5d0)) || 0.85f < lwc1(OV(0x0015f5d4))) && rand_mod(3) == 0) return;
    if ((u2f(0x3f666666u) < lwc1(OV(0x0015f5d0)) || u2f(0x3f666666u) < lwc1(OV(0x0015f5d4))) && rand_mod(2) == 0) return;
    if ((1.0f < lwc1(OV(0x0015f5d0)) || 1.0f < lwc1(OV(0x0015f5d4))) && rand_mod(1) == 0) return;
    const uint32_t p = alloc_particle(0xb);
    if (p == 0) return;
    V(p + 0x10) = pos;
    sw(p + 4, color);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) - 0x60));
    const uint32_t tex = rand_mod(100) < 0x14 ? 0x44u : 0x48u;
    sb(p + 1, 0);
    sw(p + 0xc, 0);
    sb(p + 3, tex);
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sb(p + 2, lbu(lwu(OV(0x001b20ac))));
    Vec4 d{};
    d.x = rand_range_f(-1.0f, 1.0f);
    d.y = rand_range_f(-1.0f, 1.0f);
    d.z = rand_range_f(-1.0f, 1.0f);
    vec3_set_length(d, d, speed);
    const uint32_t q = p + 0x20;
    vec3_add(V(q), vel, d);
    swc1(q + 0xc, speed * 0.75f);
    sh(p + 0xa, count);
    sb(q + 0x19, b39);
    sb(q + 0x1b, count);
    sb(q + 0x18, b38);
    swc1(q + 0x1c, f3c);
    sw(q + 0x10, color);
    sw(q + 0x14, a3);
    sb(q + 0x1a, b3a);
}
// orig: 0x0026a9f0 (EE-address form)
void spawn_particle_0b(uint32_t pos, uint32_t vel, uint32_t color, uint32_t a3, uint32_t count, uint32_t b38, uint32_t b3a, uint32_t b39, float f3c, float speed) {
    spawn_particle_0b_v(V(pos), V(vel), color, a3, count, b38, b3a, b39, f3c, speed);
}

// A kind 0xd particle at `pos` jittered by up to +-jitter on each axis: colour `color` (its
// rgb again at +0x34), size `size`, 10 frames, spin +-spin, growth lo..hi, +0x30 `f30`.
// orig: 0x0026b790
uint32_t spawn_particle_0d_v(const Vec4& pos, int32_t spin, uint32_t color, float jitter, float lo, float hi, float f30, float size) {
    const uint32_t p = alloc_particle(0xd);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    const uint32_t q = p + 0x20;
    sw(p + 4, color);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    swc1(p + 0xc, size);
    sb(p + 1, 0);
    sb(p + 8, 0);
    sb(p + 2, lbu(lwu(OV(0x001b20b4))));
    swc1(p + 0x10, lwc1(p + 0x10) + rand_range_signed_f(0.0f, jitter));
    swc1(p + 0x14, lwc1(p + 0x14) + rand_range_signed_f(0.0f, jitter));
    swc1(p + 0x18, lwc1(p + 0x18) + rand_range_signed_f(0.0f, jitter));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(10)));
    sw(q + 4, 0);
    if (rand_mod(2) != 0) {
        sw(q + 8, static_cast<uint32_t>(spin));
    } else {
        sw(q + 8, static_cast<uint32_t>(-spin));
    }
    sw(q + 0x14, color & 0xffffffu);
    const float g = rand_range_f(lo, hi);
    swc1(q + 0x10, f30);
    swc1(q + 0xc, g);
    return p;
}
// orig: 0x0026b790 (EE-address form)
uint32_t spawn_particle_0d(uint32_t pos, int32_t spin, uint32_t color, float jitter, float lo, float hi, float f30, float size) {
    return spawn_particle_0d_v(V(pos), spin, color, jitter, lo, hi, f30, size);
}

// orig: 0x002715e8 — spawn particle kind 0x2d at pos drifting in a random direction
// (colour -1: random grey alpha).
uint32_t spawn_particle_2d_v(const Vec4& pos, uint32_t a, uint32_t colour, float size, float b) {
    const uint32_t p = alloc_particle(0x2d);
    if (p == 0) return 0;
    V(p + 0x10) = pos;
    if (colour == 0xffffffffu) colour = (static_cast<uint32_t>(rand_mod(6) + 0x14) << 24) | 0x00808080u;
    const uint32_t q = p + 0x20;
    sw(p + 4, colour);
    const int32_t s = float_to_int(2.0f);
    sb(p + 1, 1);
    sb(p + 9, static_cast<uint32_t>(s + 0x20));
    sb(p + 3, 0x48);
    sb(p + 2, lbu(lwu(OV(0x001b2134))));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sh(p + 0xa, 4);
    swc1(p + 0xc, size * u2f(0x484d1400u));
    const float ang = rand_angle();
    const float spd = rand_range_f(u2f(0x3ac49ba6u), u2f(0x3b23d70au));
    swc1(q + 0, cos_vu(ang) * spd);
    const float y = sin_vu(ang) * spd;
    sw(q + 0x14, a);
    swc1(q + 0x10, b);
    sw(q + 8, 0);
    swc1(q + 4, y);
    return p;
}
// orig: 0x002715e8 (EE-address form)
uint32_t spawn_particle_2d(uint32_t pos, uint32_t a, uint32_t colour, float size, float b) { return spawn_particle_2d_v(V(pos), a, colour, size, b); }

// orig: 0x002604f0 — place a position/rotation given in the moby's local frame into world
// space; when the moby is attached (pvar word 2) follow the parent: flag 4 → full transform
// via sub_00260668 (arguments swapped), else move by the parent's offset (length <= 1) when
// obj+0x22 is below the moby's +0x22. 0 when not attached.
int32_t sub_002604f0_v(uint32_t obj, uint32_t moby, Vec4& pos, Vec4& ang, Vec4& out_pos, Vec4& out_ang) {
    const uint32_t parent = moby_pvar_word2(moby);
    if (parent == 0) return 0;
    mat3_transform(out_pos, pos, M(moby + 0xc0));
    vec3_add(out_pos, out_pos, V(moby + 0x10));
    Mat4 r, c;
    euler_to_mat4_vu(r, ang);
    mat3_copy_affine(c, M(moby + 0xc0));
    mat4_mul(r, c, r);
    mat_to_euler_r(r, out_ang);
    if ((lw(parent + 0x3c) & 4) != 0) {
        vec3_add(out_pos, out_pos, V(parent + 0x10));
        sub_00260668_v(obj, moby, out_pos, out_ang, pos, ang);
        return 1;
    }
    const uint32_t a = lbu(obj + 0x22);
    if (a == 0) return 1;
    if (a < lbu(moby + 0x22)) {
        Vec4 off = V(parent + 0x10);
        vec3_clamp_length_v(off, 1.0f);
        vec3_add(out_pos, out_pos, off);
    }
    return 1;
}
// orig: 0x002604f0 (EE-address form)
int32_t sub_002604f0(uint32_t obj, uint32_t moby, uint32_t pos, uint32_t ang, uint32_t out_pos, uint32_t out_ang) {
    return sub_002604f0_v(obj, moby, V(pos), V(ang), V(out_pos), V(out_ang));
}

// orig: 0x00272060 — spawn particle kind 0x35 (8 register arguments + 3 floats).
uint32_t spawn_particle_35_v(const Vec4& pos, uint32_t a, uint32_t colour, int32_t mode, uint32_t b, const Vec4& vec, float s0,
                             float s1, float c) {
    const uint32_t p = alloc_particle(0x35);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    V(p + 0x10) = pos;
    const float z = lwc1(p + 0x18) + u2f(0x3d4ccccdu);
    sw(p + 4, colour);
    sb(p + 3, 0x48);
    swc1(p + 0x18, z);
    const int32_t s = float_to_int(u2f(0x3ecccccdu));
    sb(p + 1, 0);
    sb(p + 9, static_cast<uint32_t>(s + 0x20));
    sh(p + 0xa, a);
    sb(p + 2, lbu(lwu(OV(0x001b2154))));
    swc1(p + 0xc, s0 * u2f(0x484d1400u));
    if (mode == 0) sb(p + 8, 0);
    else if (mode == 1) sb(p + 8, 0x20);
    else sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
    const float e = s1 * u2f(0x484d1400u);
    swc1(q + 0, lwc1(p + 0xc));
    sh(q + 8, a);
    sb(q + 0xa, b);
    swc1(q + 4, e);
    sb(q + 0xb, static_cast<uint32_t>(static_cast<int32_t>(colour) >> 24));
    swc1(q + 0x18, c);
    swc1(q + 0xc, vec.x);
    swc1(q + 0x10, vec.y);
    swc1(q + 0x14, vec.z);
    return p;
}
// orig: 0x00272060 (EE-address form)
uint32_t spawn_particle_35(uint32_t pos, uint32_t a, uint32_t colour, int32_t mode, uint32_t b, uint32_t vec, float s0,
                           float s1, float c) {
    return spawn_particle_35_v(V(pos), a, colour, mode, b, V(vec), s0, s1, c);
}

// orig: 0x002a3cd8 — index of the first region (boxes *(0x1612d4), count *(0x1612d8), cell
// masks in the 0x1190-byte records *(0x1612d0)) whose box and 4x4 cell mask contain (x, y, z);
// -1 if none.
int32_t region_find(float x, float y, float z) {
    const int32_t ix = float_to_int(x * 1024.0f);
    const int32_t iy = float_to_int(y * 1024.0f);
    const int32_t iz = float_to_int(z * 1024.0f);
    constexpr OvConst<0x001ca880u> GRID{};
    uint32_t box = lwu(OV(0x001612d4));
    for (int32_t i = 0; i < lw(OV(0x001612d8)); ++i, box += 0x10) {
        const int32_t bx = lw(box + 0), by = lw(box + 4), bz = lw(box + 8);
        if (ix < bx || iy < by || iz < bz) continue;
        if (!(ix < static_cast<int32_t>(static_cast<uint32_t>(bx) + lhu(box + 0xc)))) continue;
        if (!(iy < static_cast<int32_t>(static_cast<uint32_t>(by) + lhu(box + 0xe)))) continue;
        if (!(iz < bz + 0x800)) continue;
        const uint32_t r = lwu(OV(0x001612d0)) + static_cast<uint32_t>(i) * 0x1190u;
        const int32_t cx = float_to_int(fpu_div(x - (lwc1(r + 0) + lwc1(GRID + 8)), lwc1(GRID + 0x10)));
        const int32_t cy = float_to_int(fpu_div(y - (lwc1(r + 4) + lwc1(GRID + 0xc)), lwc1(GRID + 0x14)));
        const uint32_t bit = static_cast<uint32_t>(((cx >> 2) & 3) | (cy & 0xc));
        if ((lhu(r + 0x1e) & (1u << (bit & 31))) != 0) return i;
    }
    return -1;
}

// orig: 0x0026f080 — spawn particle kind 0x22 (smoke) at pos with velocity vel, at height h
// (-1: the hero's ground height, +0.4 above PLAYER+0x88 in state 0x31).
uint32_t spawn_particle_22_v(const Vec4& pos, const Vec4& vel, float scale, float h) {
    const uint32_t p = alloc_particle(0x22);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    sw(p + 4, 0x88a0a0a0u);
    const int32_t s = float_to_int(4.0f);
    sb(p + 1, 0);
    sb(p + 3, 0x44);
    sb(p + 9, static_cast<uint32_t>(s + 0x40));
    const float r = rand_range_f(0.75f, 1.25f);
    swc1(q + 0xc, r * scale);
    swc1(p + 0xc, u2f(0x45a41000u));
    sb(p + 8, static_cast<uint32_t>(rand_range(-10, 10) - 0x80));
    sb(p + 2, lbu(lwu(OV(0x001b2108))));
    const int32_t a = frame_scale_i(0x46);
    const int32_t b = frame_scale_i(0x8c);
    sh(p + 0xa, static_cast<uint32_t>(rand_range(a, b)));
    if (h == -1.0f) {
        if (lw(g::HERO_STATE) == 0x31) swc1(q + 0x10, lwc1(g::PLAYER + 0x88) + u2f(0x3ecccccdu));
        else swc1(q + 0x10, lwc1(g::PLAYER + 0x2f0));
    } else {
        swc1(q + 0x10, h);
    }
    swc1(q + 0x14, rand_angle());
    const float k = lwc1(0x0015ed6c);
    swc1(q + 0x18, rand_range_f(k * u2f(0x3fdf66f3u), k * u2f(0x40df66f3u)));
    const float k2 = lwc1(0x0015ed6c);
    swc1(q + 0x1c, rand_range_f(k2 * 0.5f, k2 * u2f(0x3fb33333u)));
    swc1(q + 0, vel.x);
    swc1(q + 4, vel.y);
    swc1(q + 8, vel.z);
    V(p + 0x10) = pos;
    return p;
}
// orig: 0x0026f080 (EE-address form)
uint32_t spawn_particle_22(uint32_t pos, uint32_t vel, float scale, float h) { return spawn_particle_22_v(V(pos), V(vel), scale, h); }

// orig: 0x002a7e20 — n pairs of sparks (particle 0x35, coloured + white) at a jittered pos
// (pos itself is moved by the jitter, as in the original), flying out in random directions.
void spark_pairs_v(Vec4& pos, int32_t n) {
    if (n <= 0) return;
    const float k01 = u2f(0x3dcccccdu), k015 = u2f(0x3e19999au), kc = u2f(0x3b449ba6u);
    int32_t left = n;
    do {
        const float size = rand_range_f(k01, 2.0f);
        const int32_t r = rand_mod(2);
        int32_t sgn = r != 0 ? r : r - 1;
        const float yaw = rand_angle();
        const float pitch = rand_range_f(u2f(0x3eb2b8c2u), u2f(0x3fc49809u));
        const float spd = rand_range_f(u2f(0x3d4ccccdu), k01);
        Vec4 dir{0.0f, 0.0f, 0.0f, 0.0f};
        vec_from_spherical_v(dir, spd, yaw, pitch);
        pos.x = pos.x + rand_range_signed_f(0.0f, k015);
        --left;
        pos.y = pos.y + rand_range_signed_f(0.0f, k015);
        pos.z = pos.z + rand_range_signed_f(0.0f, k015);
        const int32_t a = frame_scale_i(0x1e);
        spawn_particle_35_v(pos, static_cast<uint32_t>(a), 0x7f207f7fu, 0, static_cast<uint32_t>(sgn), dir, size * k01, size, kc);
        sgn = -sgn;
        const int32_t b = frame_scale_i(0x1e);
        spawn_particle_35_v(pos, static_cast<uint32_t>(b), 0x7f7f7f7fu, 1, static_cast<uint32_t>(sgn), dir,
                            size * u2f(0x3d8f5c29u), size * u2f(0x3f333333u), kc);
    } while (left != 0);
}
// orig: 0x002a7e20 (EE-address form)
void spark_pairs(uint32_t pos, int32_t n) { spark_pairs_v(V(pos), n); }

// orig: 0x0028d970 — start sound `snd` (flags; on moby, or at pos, or global) in a free slot
// of 0x13e550 (26 slots, 30 for the hero's own sounds); -1 when no slot, when the flag-4 / +0x18
// rule rejects it, or when it is too quiet (volume below 0x20 at the listener).
int32_t sound_start(uint32_t snd, uint32_t flags, uint32_t moby, uint32_t pos, int32_t volume) {
    const bool unique = lbu(snd + 0x18) == 0;
    if (((flags & 4) != 0) == unique) return -1;
    int32_t limit = 0x1a;
    if (moby != 0) {
        if (moby == lwu(g::PLAYER + 0x2080) || lwu(g::PLAYER + 0x1090) == moby || lh(moby + 0xa6) == 0x472) limit = 0x1e;
    }
    constexpr uint32_t S = 0x0013e550;
    int32_t s = 0;
    while (s < limit && lbu(S + static_cast<uint32_t>(s) * 0x70 + 0x74) != 0) ++s;
    if (!(s < limit)) return -1;
    const uint32_t e = S + static_cast<uint32_t>(s) * 0x70;
    sw(e + 0x78, snd);
    sh(e + 0x7e, 0xffff);
    sh(e + 0x7c, lhu(snd + 0x1a));
    sw(e + 0x8c, 0);
    sw(e + 0x80, static_cast<uint32_t>(volume));
    sw(e + 0x88, 0);
    sd(e + 0xa0, 0);
    sd(e + 0xa8, 0);
    if (pos == 0 && moby == 0) {
        flags |= 0x11;
        vec_clear(V(e + 0x90));
    } else if (moby != 0) {
        lq_sq(e + 0x90, moby + 0x10);
        swc1(e + 0x98, lwc1(e + 0x98) + 1.0f);
    } else {
        lq_sq(e + 0x90, pos);
    }
    const int32_t v = (flags & 0x10) == 0 ? sub_0028c9a0(e + 0x70, e + 0x90) : volume;
    if (v < 0x20) return -1;
    sb(e + 0x75, flags);
    sb(e + 0x74, 7);
    sw(e + 0xb0, 0);
    const uint32_t hi = lwu(snd + 0x14), lo = lwu(snd + 0x10);
    uint32_t len = hi;
    if (hi != lo) len = static_cast<uint32_t>(rand_mod(static_cast<int32_t>(hi - lo))) + lwu(snd + 0x10);
    sw(e + 0x70, 0xffffffffu);
    sw(e + 0x84, len);
    return s;
}

// Sound `idx` of the sound set 0x197300[0x197bc0[set]] (its count +0xd, entries +0x28) played
// on `moby` (volume 0x400); the channel 0x13e550[ch] remembers the index and the moby.
// Returns the channel (-1: none).
// orig: 0x0028dc90
int32_t moby_sound_play(int32_t idx, uint32_t flags, uint32_t moby, uint32_t set) {
    const uint32_t t = lwu(OV(0x00197300) + lbu(OV(0x00197bc0) + set) * 4);
    if (t == 0) return -1;
    if (!(idx < static_cast<int32_t>(lbu(t + 0xd)))) return -1;
    const int32_t ch = sound_start(lwu(t + 0x28) + static_cast<uint32_t>(idx) * 0x20, flags, moby, 0, 0x400);
    if (!(ch < 0)) {
        const uint32_t c = 0x0013e550 + static_cast<uint32_t>(ch) * 0x70;
        sh(c + 0x7e, static_cast<uint32_t>(idx));
        sw(c + 0x88, moby);
    }
    return ch;
}

// orig: 0x00262528 — spawn a copy of obj's source moby (+0x134) as fragment b/c in the
// object's list (up to 4; c < 9).
void sub_00262528(uint32_t obj, uint32_t b, int32_t c) {
    if (lw(obj + 0x138) == 4) return;
    if (!(c < 9)) return;
    const uint32_t m = moby_spawn(static_cast<uint32_t>(static_cast<int32_t>(lh(lwu(obj + 0x134) + 0xa6))));
    if (m == 0) return;
    const int32_t cls = lh(m + 0xa6);
    sh(m + 0x32, lhu(lwu(obj + 0x134) + 0x32));
    const uint64_t col = ld(lwu(obj + 0x134) + 0x38);
    sb(m + 0x31, 1);
    sd(m + 0x38, col);
    sh(m + 0x34, 0x80a);
    sb(m + 0x30, 0);
    sw(m + 0x94, 0);
    if (cls == 0) sb(m + 0x72, 0);
    sb(m + 0x23, b);
    sw(obj + 0x110 + (lwu(obj + 0x138) << 2), static_cast<uint32_t>(c));
    sw(obj + 0x100 + (lwu(obj + 0x138) << 2), b);
    sw(obj + 0x120 + (lwu(obj + 0x138) << 2), m);
    sw(obj + 0x138, lwu(obj + 0x138) + 1u);
}

// orig: 0x0028dbe8 — play sound idx of the moby's class sound table on that moby.
int32_t moby_sound_start(int32_t idx, uint32_t flags, uint32_t moby) {
    if (moby == 0) return -1;
    const uint32_t c = lwu(moby + 0x24);
    if (c == 0) return -1;
    const uint32_t t = lwu(c + 0x28);
    if (t == 0) return -1;
    if (!(idx < static_cast<int32_t>(lbu(c + 0xd)))) return -1;
    const int32_t s = sound_start(t + static_cast<uint32_t>(idx << 5), flags, moby, 0, 0x400);
    if (s >= 0) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(s * 0x70);
        sh(e + 0x7e, static_cast<uint32_t>(idx));
        sw(e + 0x88, moby);
    }
    return s;
}

// orig: 0x0028de68 — play level sound (a, b, c) (index = level base 0x1bd820[level] + 4a +
// 2b + c + *(0x15f574) into the table *(0x15f5f4), count *(0x15f5f0)).
int32_t level_sound_start(int32_t a, int32_t b, int32_t c, uint32_t flags, uint32_t moby) {
    const int32_t i = static_cast<int32_t>(static_cast<uint32_t>(lbu(OV(0x001bd820) + lwu(0x0015ed84))) + static_cast<uint32_t>(a << 2) +
                                           static_cast<uint32_t>(b << 1) + static_cast<uint32_t>(c) + lwu(OV(0x0015f574)));
    if (!(i < lw(OV(0x0015f5f0)))) return -1;
    const int32_t s = sound_start(lwu(OV(0x0015f5f4)) + static_cast<uint32_t>(i << 5), flags, moby, 0, 0x400);
    if (s >= 0) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(s * 0x70);
        sh(e + 0x7e, static_cast<uint32_t>(i));
        sw(e + 0x88, moby);
    }
    return s;
}

// orig: 0x002a99b0 — spawn moby 0x593 (a light shaft) at pos (first argument unused).
uint32_t spawn_moby_593_v(uint32_t unused, const Vec4& pos, uint32_t v, float scale, float a, float b) {
    (void)unused;
    const uint32_t m = moby_spawn(0x593);
    if (m == 0) return 0;
    V(m + 0x10) = pos;
    moby_set_color_hi(m, 0x80, 0x80, 0x70);
    const uint32_t pv = lwu(m + 0x78);
    const float s2 = scale + scale;
    swc1(pv + 0, a);
    swc1(m + 0x2c, s2);
    swc1(m + 0x2c, s2 * lwc1(lwu(m + 0x24) + 0x24));
    swc1(pv + 0xc, b);
    sw(pv + 8, v);
    sw(pv + 4, v);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    return m;
}
// orig: 0x002a99b0 (EE-address form)
uint32_t spawn_moby_593(uint32_t unused, uint32_t pos, uint32_t v, float scale, float a, float b) {
    return spawn_moby_593_v(unused, V(pos), v, scale, a, b);
}

// orig: 0x002a9aa0 — spawn the single moby 0x5f3 (kept in *(0x1613c8)) at pos with its light
// (sub_0023e5e0) and reset the globals 0x15f320.. (first argument unused).
uint32_t spawn_moby_5f3_v(uint32_t unused, const Vec4& pos) {
    (void)unused;
    const uint32_t m = moby_spawn(0x5f3);
    if (m == 0) return 0;
    if (lw(OV(0x001613c8)) != 0) return m;
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    sb(m + 0x30, 0xff);
    sb(m + 0x20, 0);
    const uint32_t pv = lwu(m + 0x78);
    V(m + 0x10) = pos;
    sw(OV(0x0015f324), 0xff);
    sw(OV(0x0015f334), 0x00ffffffu);
    sd(OV(0x0015f338), 0x1f00000042ull);
    sw(OV(0x001613c8), m);
    sd(OV(0x0015f328), 0);
    sw(OV(0x0015f320), 0);
    sw(OV(0x0015f330), 0);
    sw(pv + 0x10, 0);
    sh(pv + 0x2c, static_cast<uint32_t>(sub_0023e5e0_v(pos, 0, 0.0f, 0.0f)));
    return m;
}
// orig: 0x002a9aa0 (EE-address form)
uint32_t spawn_moby_5f3(uint32_t unused, uint32_t pos) { return spawn_moby_5f3_v(unused, V(pos)); }

// orig: 0x002b4138 — spawn moby 0xa7 carrying `v` in its pvars.
uint32_t spawn_moby_a7(uint32_t v) {
    const uint32_t m = moby_spawn(0xa7);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    sw(pv + 4, v);
    sw(pv + 0xc, 0);
    sw(pv + 0, 0);
    sw(pv + 8, 0);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0x7e);
    sb(m + 0x31, 0);
    sb(m + 0x20, 0);
    return m;
}

// orig: 0x002a9b80 — move the single moby 0x5f3 to pos (spawning it first if needed) and flag
// its pvar +0x2e (first argument unused).
void place_moby_5f3_v(uint32_t unused, const Vec4& pos) {
    const Vec4 p = pos;
    if (lw(OV(0x001613c8)) == 0) {
        spawn_moby_5f3_v(unused, p);
        if (lw(OV(0x001613c8)) == 0) return;
    }
    const uint32_t m = lwu(OV(0x001613c8));
    const uint32_t pv = lwu(m + 0x78);
    if (lh(m + 0xa6) != 0x5f3) return;
    V(m + 0x10) = p;
    sh(pv + 0x2e, 1);
}
// orig: 0x002a9b80 (EE-address form)
void place_moby_5f3(uint32_t unused, uint32_t pos) { place_moby_5f3_v(unused, V(pos)); }

// orig: 0x00262840 — if obj's spawn list is live (+0x13c), retire its +0x138 child mobies
// (+0x120[i]) and mark the list dead.
void retire_children(uint32_t obj) {
    if (lw(obj + 0x13c) == 0) return;
    for (int32_t i = 0; i < static_cast<int32_t>(lw(obj + 0x138)); ++i) {
        const uint32_t slot = obj + 0x120 + 4 * static_cast<uint32_t>(i);
        if (lw(slot) == 0) continue;
        moby_retire(lw(slot));
        sw(slot, 0);
    }
    sw(obj + 0x13c, 0);
}

namespace {
// append {id, flag, 0, word} to the item list at 0x1ca4c0 (entries of 0x14 from +0xd0, count
// +0x210). As in the original every field store re-reads the count: entry 16 overlaps it.
static void item_list_push(uint32_t id, uint32_t flag, uint32_t word) {
    constexpr OvConst<0x001ca4c0u> L{};
    sw(L + 0xd0 + lw(L + 0x210) * 0x14, id);
    sw(L + 0xd4 + lw(L + 0x210) * 0x14, flag);
    sw(L + 0xd8 + lw(L + 0x210) * 0x14, 0);
    sw(L + 0xe0 + lw(L + 0x210) * 0x14, word);
    sw(L + 0x210, lw(L + 0x210) + 1);
}
}  // namespace

// orig: 0x0029a400 — build the item list 0x1ca4c0: first the quick-select slots 0x15edd0
// (up to the first 0xff; bit 0x40 = flagged item, plain items only when `flagged_only` is 0),
// then every owned item (0x13d4c0[i]) with a model (0x1c40b0[i].+8) that is not listed yet.
void item_list_build(int32_t flagged_only) {
    constexpr OvConst<0x001ca4c0u> L{};
    int32_t n = 0;
    if (lbu(0x0015edd0) != 0xff) {
        n = 1;
        while (n < 12 && lbu(0x0015edd0 + static_cast<uint32_t>(n)) != 0xff) ++n;
    }
    sw(L + 0x210, 0);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t v = lbu(0x0015edd0 + static_cast<uint32_t>(i));
        if (v == 0xff) continue;
        const uint32_t id = v & 0x3f;
        if ((v & 0x40) != 0) {
            if (lhu(OV(0x001c40b0) + id * 0x18 + 8) == 0) continue;
            item_list_push(id, 1, lw(OV(0x001c2108) + id * 0x14));
        } else {
            if (flagged_only != 0) continue;
            item_list_push(id, 0, lw(OV(0x001c2108) + id * 0x14));
        }
    }
    for (uint32_t i = 0; i < 0x25; ++i) {
        if (lbu(0x0013d4c0 + i) == 0) continue;
        if (lhu(OV(0x001c40b0) + i * 0x18 + 8) == 0) continue;
        const int32_t cnt = lw(L + 0x210);
        bool listed = false;
        for (int32_t j = 0; j < cnt && !listed; ++j) listed = static_cast<uint32_t>(lw(L + 0xd0 + static_cast<uint32_t>(j) * 0x14)) == i;
        if (listed) continue;
        item_list_push(i, 1, lw(OV(0x001c2108) + i * 0x14));
    }
    vendor_buy_all_list_built();  // (enhancement: the Vendor's "All Ammo" entry first, vendor_buy_all.h; nothing when off)
}

// orig: 0x002a4530 — sample the height grid of the region under (x, y) (region_find): bilinear
// height from the 16x16(+edge) samples of layer gp-0x59e0, plus the region base +8, into *h;
// the surface normal of the cell into *n. Either output may be null. Returns 0 outside any region.
int32_t region_height_sample_v(float* h, Vec4* n, float x, float y, float z) {
    const int32_t r = region_find(x, y, z);
    if (r < 0) return 0;
    constexpr OvConst<0x001ca880u> S{};
    const uint32_t R = static_cast<uint32_t>(lw(OV(0x001612d0))) + static_cast<uint32_t>(r) * 0x1190;
    float dx = x - (lwc1(R + 0) + lwc1(S + 8));
    float dy = y - (lwc1(R + 4) + lwc1(S + 0xc));
    const int32_t ix = float_to_int(fpu_div(dx, lwc1(S + 0x10)));
    const int32_t iy = float_to_int(fpu_div(dy, lwc1(S + 0x14)));
    float f1 = lwc1(S + 0x10);
    dx = fpu_div(dx - int_to_float(ix) * f1, f1);
    f1 = lwc1(S + 0x14);
    const float fy = fpu_div(dy - int_to_float(iy) * f1, f1);
    const uint32_t L = static_cast<uint32_t>(lw(OV(0x00166c00u - 0x59e0))) * 0x5c0;
    const uint32_t ux = static_cast<uint32_t>(ix), uy = static_cast<uint32_t>(iy);
    const float h00 = lwc1(R + 0x50 + ux * 4 + uy * 64 + L);
    const float h10 = ix == 15 ? lwc1(R + uy * 4 + L + 0x510) : lwc1(R + 0x50 + (ux + 1) * 4 + uy * 64 + L);
    const float h01 = iy == 15 ? lwc1(R + ux * 4 + L + 0x4d0) : lwc1(R + 0x50 + ux * 4 + (uy + 1) * 64 + L);
    float h11;
    if (ix == 15) {
        h11 = iy == 15 ? lwc1(R + L + 0x5dc) : lwc1(R + (uy + 1) * 4 + L + 0x510);
    } else {
        h11 = iy == 15 ? lwc1(R + (ux + 1) * 4 + L + 0x4d0) : lwc1(R + 0x50 + (ux + 1) * 4 + (uy + 1) * 64 + L);
    }
    if (h != nullptr) {
        const float base = lwc1(R + 8);
        const float top = h01 + (h11 - h01) * dx;
        const float bot = h00 + (h10 - h00) * dx;
        *h = bot + (top - bot) * fy + base;
    }
    if (n != nullptr) {
        Vec4 a, b;
        a.x = lwc1(S + 0x10); a.y = 0.0f; a.z = h10 - h00; a.w = 1.0f;
        b.x = 0.0f; b.y = lwc1(S + 0x14); b.z = h01 - h00; b.w = 1.0f;
        vec3_cross_ba(*n, a, b);
        vec3_set_length(*n, *n, 1.0f);
    }
    return 1;
}
// orig: 0x002a4530 — EE-address wrapper (0 = no output)
int32_t region_height_sample(uint32_t h, uint32_t n, float x, float y, float z) {
    return region_height_sample_v(h != 0 ? static_cast<float*>(ee_ram_ptr(h)) : nullptr, n != 0 ? &V(n) : nullptr, x, y, z);
}

// orig: 0x0026de90 — spawn particle kind 0x1c (a falling droplet) near pos: jittered by
// +-0.03 (through the rotation of mat, if given) and 0.05 lower, random warm colour, drifting
// at `speed` along mat's x axis (or randomly in x/y without mat) and slowly down.
void spawn_particle_1c_v(const Vec4& pos, const Mat4* mat, float speed) {
    const uint32_t p = alloc_particle(0x1c);
    if (p == 0) return;
    const uint32_t q = p + 0x20;
    const float lo = u2f(0xbcf5c28fu), hi = u2f(0x3cf5c28fu);
    Vec4 j;
    j.x = rand_range_f(lo, hi);
    j.y = rand_range_f(lo, hi);
    j.z = 0.0f;
    j.w = 0.0f;  // uninitialised stack lane in the original
    if (mat != nullptr) {
        Mat4 m;
        mat3_copy_affine(m, *mat);
        Vec4 t;
        mat4_transform(t, j, m);
        vec3_add(V(p + 0x10), t, pos);
    } else {
        vec3_add(V(p + 0x10), j, pos);
    }
    swc1(p + 0x18, lwc1(p + 0x18) - u2f(0x3d4ccccdu));
    int32_t r = libc_rand() % 0x60;
    const int32_t a = r != 0 ? r : 1;
    int32_t b = ee_rem(libc_rand(), a) % 64;
    const int32_t c = libc_rand();
    if (b == 0) b = 1;
    const int32_t d = ee_rem(c, b);
    const uint32_t col = static_cast<uint32_t>(a + 0x8f) | static_cast<uint32_t>(b + 0x8f) << 8 | static_cast<uint32_t>(d % 0x30 + 0x6f) << 16;
    sw(p + 4, col);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    sb(p + 8, 0);
    sb(p + 2, lbu(lwu(OV(0x001b20f0))));
    swc1(q + 4, rand_range_f(0.0f, u2f(0x3cb70e1fu)) + u2f(0x3727c5acu));
    swc1(p + 0xc, rand_range_f(u2f(0x461d8000u), u2f(0x469bdc00u)));
    swc1(q + 8, 0.0f);
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0xfc)));
    swc1(q + 0xc, 1.0f);
    if (mat != nullptr) {
        Vec4 v{0.0f, 0.0f, 0.0f, 0.0f};
        v.x = speed;
        v.z = u2f(0xbb23d70au);
        mat3_transform(V(q + 0x10), v, *mat);
    } else {
        swc1(q + 0x10, rand_range_f(-speed, speed));
        swc1(q + 0x14, rand_range_f(-speed, speed));
        swc1(q + 0x18, u2f(0xbb23d70au));
    }
}
// orig: 0x0026de90 (EE-address form; mat = 0: none)
void spawn_particle_1c(uint32_t pos, uint32_t mat, float speed) { spawn_particle_1c_v(V(pos), mat != 0 ? &M(mat) : nullptr, speed); }

// orig: 0x002a9be0 — remove the placed moby 0x5f3 (*0x1613c8, see place_moby_5f3): forget a
// stale handle, else when it is active free its region (pvar +0x2c) and retire it.
void remove_moby_5f3() {
    const uint32_t m = lw(OV(0x001613c8));
    if (m == 0) return;
    if (lh(m + 0xa6) != 0x5f3) {
        sw(OV(0x001613c8), 0);
        return;
    }
    if (lbu(m + 0x20) != 0) return;
    const uint32_t pv = lw(m + 0x78);
    sw(OV(0x0015f320), 0);
    sw(OV(0x0015f330), 0);
    const int32_t r = lh(pv + 0x2c);
    if (r != -1) region_clear(static_cast<uint32_t>(r));
    moby_retire(lw(OV(0x001613c8)));
    sw(OV(0x001613c8), 0);
}

// orig: 0x00262030 — push pos out of the tube of radius r around path 0x1b04b0[idx] (ignoring
// the component along the axis 0x13f5e0; segments whose ends both have w = 0 are skipped).
// If any segment was touched, out = the pushed point with its original height along the axis
// restored, and 1 is returned; otherwise out is left alone.
int32_t path_push_out(uint32_t idx, uint32_t pos, uint32_t out, float r) {
    const uint32_t path = lw(OV(0x001b04b0) + idx * 4);
    const Vec4& axis = V(0x0013f5e0);
    Vec4 p = V(pos);
    const float h0 = vec3_dot(p, axis);
    int32_t hit = 0;
    for (int32_t i = 0; i < lw(path) - 1; ++i) {
        const uint32_t a = path + 0x10 + static_cast<uint32_t>(i) * 16;
        if (lwc1(a + 0xc) == 0.0f && lwc1(a + 0x1c) == 0.0f) continue;
        Vec4 d, s, t, u, c;
        vec3_sub(d, p, V(a));
        vec3_sub(s, V(a + 0x10), V(a));
        vec3_set_length(t, axis, vec3_dot(d, axis));
        vec3_sub(d, d, t);
        vec3_set_length(t, axis, vec3_dot(s, axis));
        vec3_sub(s, s, t);
        vec3_set_length(u, s, 1.0f);
        vec3_cross_ba(c, d, u);
        if (r < abs_f(vec3_dot(c, axis))) continue;
        const float len = vec3_length(s);
        const float along = vec3_dot(d, u);
        Vec4 q;
        if (len < along || along < 0.0f) {
            if (!(vec3_length(d) < r)) continue;
            hit = 1;
            vec3_set_length(q, d, r);
            vec3_add(p, q, V(a));
        } else {
            hit = 1;
            vec3_set_length(t, u, along);
            vec3_sub(q, d, t);
            vec3_set_length(q, q, r);
            vec3_add(q, q, t);
            vec3_add(p, q, V(a));
        }
    }
    if (hit == 0) return 0;
    Vec4 k;
    vec3_set_length(k, axis, h0 - vec3_dot(p, axis));
    vec3_add(V(out), p, k);
    return hit;
}

// orig: 0x00262608 — afterimage trail of obj's source moby +0x134 (retire the trail when the
// source is gone): record its position/angles in the 8-entry history obj+0/+0x80 (head +0x130,
// fill +0x132), pose each child moby +0x120[i] (delay +0x110[i] frames) with the source's anim
// and the delayed history entry, and with `fade` lower their alpha +0x23, retiring the trail
// once all are transparent.
void trail_update(uint32_t obj, int32_t fade) {
    if (lw(obj + 0x13c) == 0) return;
    const uint32_t src = lw(obj + 0x134);
    if (src == 0 || lbu(src + 0x20) == 0xfe || lbu(src + 0x20) == 0xfd) {
        retire_children(obj);
        return;
    }
    const int32_t head = lh(obj + 0x130);
    lq_sq(obj + static_cast<uint32_t>(head) * 16, src + 0x10);
    lq_sq(obj + static_cast<uint32_t>(head) * 16 + 0x80, src + 0x40);
    const uint32_t fill = lhu(obj + 0x132) + 1u;
    sh(obj + 0x130, static_cast<uint32_t>((head + 1) % 8));
    sh(obj + 0x132, fill);
    if (!(static_cast<int16_t>(fill) < 9)) sh(obj + 0x132, 8);
    int32_t n = lw(obj + 0x138);
    for (int32_t i = 0; i < n; ++i, n = lw(obj + 0x138)) {
        const int32_t delay = lw(obj + 0x110 + static_cast<uint32_t>(i) * 4);
        if (lh(obj + 0x132) < delay) continue;
        const uint32_t c = lw(obj + 0x120 + static_cast<uint32_t>(i) * 4);
        sb(c + 0x52, lbu(lw(obj + 0x134) + 0x52));
        const int32_t slot = (lh(obj + 0x130) - delay + 8) % 8;
        sb(c + 0x53, lbu(lw(obj + 0x134) + 0x53));
        sb(c + 0x50, lbu(lw(obj + 0x134) + 0x50));
        sb(c + 0x51, lbu(lw(obj + 0x134) + 0x51));
        swc1(c + 0x54, lwc1(lw(obj + 0x134) + 0x54));
        sub_0024ebc0(c);
        lq_sq(c + 0x10, obj + static_cast<uint32_t>(slot) * 16);
        lq_sq(c + 0x40, obj + static_cast<uint32_t>(slot) * 16 + 0x80);
        moby_update_transform(c);
    }
    if (fade == 0) return;
    bool gone = true;
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t c = lw(obj + 0x120 + static_cast<uint32_t>(i) * 4);
        if (c != 0) {
            const int32_t a = lbu(c + 0x23);
            const uint32_t left = static_cast<uint32_t>(a - (fade < a ? fade : a));
            sb(c + 0x23, left);
            if ((left & 0xff) != 0) gone = false;
        }
        n = lw(obj + 0x138);
    }
    if (gone) retire_children(obj);
}


// orig: 0x002a3ec8 — splash the height grids of `n` regions (0x1190-byte records at `regs`,
// layer gp-0x59e8) around (x, y): cells within `radius` cells get amount / dist (amount when
// dist < 1); add != 0 adds it to the cell of layer gp-0x59e0, else it replaces the cell unless
// that is larger in magnitude.
// Edge cells are copied into the halo rows of the neighbours (bytes +0xc..+0x13, 0xff = none).
void region_height_splash(uint32_t regs, int32_t n, int32_t add, float x, float y, float radius, float amount) {
    constexpr OvConst<0x001ca880u> S{};
    const uint32_t base = regs;
    auto layer = [] { return static_cast<uint32_t>(lw(OV(0x00166c00u - 0x59e8))) * 0x5c0u; };
    auto nb = [base](uint32_t p, uint32_t k) { return base + static_cast<uint32_t>(lbu(p + k)) * 0x1190u; };
    uint32_t p = regs;
    for (int32_t i = 0; i < n; ++i, p += 0x1190) {
        const float oy = lwc1(p + 4) + lwc1(S + 0xc);
        const float gy = fpu_div(y - oy, lwc1(S + 0x14));
        const float gx = fpu_div(x - (lwc1(p + 0) + lwc1(S + 8)), lwc1(S + 0x10));
        int32_t r0 = float_to_int(gy - radius);
        if (!(-1 < r0)) r0 = 0;
        int32_t r1 = float_to_int(gy + radius);
        if (!(r1 < 0x10)) r1 = 0xf;
        int32_t c1 = float_to_int(gx + radius);
        if (!(c1 < 0x10)) c1 = 0xf;
        int32_t c0 = float_to_int(gx - radius);
        if (!(-1 < c0)) c0 = 0;
        float wy = oy + lwc1(S + 0x14) * int_to_float(r0);
        const uint32_t g = p + 0x50;
        for (int32_t r = r0; !(r1 < r); ++r) {
            float wx = (lwc1(p + 0) + lwc1(S + 8)) + lwc1(S + 0x10) * int_to_float(c0);
            const uint32_t ur = static_cast<uint32_t>(r);
            if (!(c1 < c0)) {
                const float ddy = y - wy;
                const float dy2 = ddy * ddy;
                for (int32_t c = c0; !(c1 < c); ++c) {
                    const uint32_t uc = static_cast<uint32_t>(c);
                    const float ddx = x - wx;
                    const float d2 = ddx * ddx + dy2;
                    float v = amount;
                    if (!(d2 < 1.0f)) v = fpu_div(amount, sqrt_f(d2));
                    const uint32_t cell = ur * 0x40 + uc * 4;
                    if (add == 0) {
                        const float a = abs_f(v);
                        if (a < abs_f(lwc1(g + layer() + cell))) v = lwc1(g + layer() + cell);
                    } else {
                        v = v + lwc1(g + static_cast<uint32_t>(lw(OV(0x00166c00u - 0x59e0))) * 0x5c0u + cell);
                    }
                    swc1(g + layer() + cell, v);
                    if (r < 2 && lbu(p + 0xc) != 0xff) {
                        if (r != 0) swc1(nb(p, 0xc) + layer() + uc * 4 + 0x550, v);
                        else swc1(nb(p, 0xc) + layer() + uc * 4 + 0x4d0, lwc1(g + layer() + uc * 4));
                    }
                    if (c < 2 && lbu(p + 0xd) != 0xff) {
                        if (c == 0) swc1(nb(p, 0xd) + layer() + ur * 4 + 0x510, lwc1(g + layer() + ur * 0x40));
                        else swc1(nb(p, 0xd) + layer() + ur * 4 + 0x590, lwc1(g + layer() + ur * 0x40 + uc * 4));
                    }
                    if (r == 0xf && lbu(p + 0xe) != 0xff) swc1(nb(p, 0xe) + layer() + uc * 4 + 0x450, lwc1(g + layer() + uc * 4 + 0x3c0));
                    if (c == 0xf && lbu(p + 0xf) != 0xff) swc1(nb(p, 0xf) + layer() + ur * 4 + 0x490, lwc1(g + layer() + ur * 0x40 + 0x3c));
                    if (lbu(p + 0x10) != 0xff) {
                        if (c == 0 && r == 0) swc1(nb(p, 0x10) + layer() + 0x5dc, lwc1(g + layer()));
                        if (c == 1 && r == 0) swc1(nb(p, 0x10) + layer() + 0x5e0, lwc1(p + layer() + 0x54));
                        if (c == 0 && r == 1) swc1(nb(p, 0x10) + layer() + 0x5e4, lwc1(p + layer() + 0x90));
                    }
                    if (lbu(p + 0x11) != 0xff && c == 0xf && r == 0) swc1(nb(p, 0x11) + layer() + 0x5d8, lwc1(p + layer() + 0x8c));
                    if (lbu(p + 0x12) != 0xff && c == 0 && r == 0xf) swc1(nb(p, 0x12) + layer() + 0x5d4, lwc1(p + layer() + 0x410));
                    if (lbu(p + 0x13) != 0xff && c == 0xf && r == c) swc1(nb(p, 0x13) + layer() + 0x5d0, lwc1(p + layer() + 0x44c));
                    wx = wx + lwc1(S + 0x10);
                }
            }
            wy = wy + lwc1(S + 0x14);
        }
    }
}

// orig: 0x002a4de0 — spawn the class 0x3ef moby at pos (state +0x30 = 0xff, flags +0x34 | 0x41).
uint32_t camera_marker_spawn(uint32_t pos) {
    const uint32_t m = moby_spawn(0x3ef);
    if (m == 0) return 0;
    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    sb(m + 0x30, 0xff);
    lq_sq(m + 0x10, pos);
    moby_update_transform(m);
    return m;
}

}  // namespace rac1
