// The sky-reflecting objects: one port of the copies Hoven (L12) and Oltanis (L14) carry (the same code relinked; compared
// word by word: the callbacks differ only in their two texture numbers and the records' count). Level-own words: the
// callback's records (per level below). The engine's (OV): the packet cursor 0x1611c0, the GS packet 0x1cb320 its first tag
// refers to, the camera 0x166dc0; the scratchpad's colours (0x70000000) and coordinates (0x70001000). The draw's matrices:
// shadows_vu1_setup (Veldin's 0x1fc740, which the overlay maps pair with L12:0x21f300 and L14's copy). Header
// shared_env_objects.h.
#include <game/ovl/shared_env_objects.h>

#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/vu0_regs.h>

namespace rac1 {

namespace {
// a packed vector's three words into a VU0 register as the original moves them (PEXTLW / PCPYLD: the w lane the third
// word's sign extension)
void load_packed(int reg, uint32_t at) {
    eer::Q128 v{};
    const uint32_t x = lwu(at), y = lwu(at + 4), z = lwu(at + 8);
    v.lo = static_cast<uint64_t>(x) | static_cast<uint64_t>(y) << 32;
    v.hi = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(z)));
    eer::qmtc2(reg, v);
}
eer::Q128 low_word(uint32_t w) {
    eer::Q128 v{};
    v.lo = w;
    return v;
}
}  // namespace

// Per point: the view from the camera (cam - p) scaled to 0.45 over its length (vf6), the eye ray vf1 = -vf6; facing away
// (the ray's dot with the normal above 0) the ray itself, else reflected off the normal; then + 0.5 on x and y.
// orig: L12:0x002667fc L14:0x0025602c
void env_objects_uvs(int32_t n, uint32_t points, uint32_t normals, uint32_t out) {
    using namespace vu0m;
    Vf* r = vf();
    eer::vmacro();
    addbc(r[3], r[0], r[0].v[3], XYZ);  // (1, 1, 1)
    eer::lqc2_(20, OV(0x00166dc0));
    eer::qmtc2(30, low_word(0x3f000000u));  // (0.5)
    eer::qmtc2(31, low_word(0x3ee66666u));  // (0.45)
    do {  // (as the original: at least one point)
        load_packed(10, points);
        points += 0xc;
        eer::vmacro();
        sub(r[10], r[20], r[10], XYZ);
        eer::vmacro();
        mul(r[4], r[10], r[10], XYZ);
        eer::vmacro();
        addabc(r[4], r[4].v[1], X);
        eer::vmacro();
        maddbc(r[4], r[3], r[4].v[2], X);
        eer::vmacro();
        q() = vrsqrt(r[31].v[0], r[4].v[0]);
        load_packed(2, normals);
        eer::vmacro();
        eer::vmacro();
        mulbc(r[6], r[10], q(), XYZ);
        eer::vmacro();
        sub(r[1], r[0], r[6], XYZ);
        eer::vmacro();
        mul(r[4], r[1], r[2], XYZ);
        eer::vmacro();
        addabc(r[4], r[4].v[1], X);
        eer::vmacro();
        maddbc(r[4], r[3], r[4].v[2], X);
        const bool away = static_cast<int32_t>(f2u(r[4].v[0])) > 0;
        eer::vmacro();
        mulbc(r[2], r[2], r[4].v[0], XYZ);
        normals += 0xc;
        eer::vmacro();
        if (!away) {
            sub(r[2], r[2], r[1], XYZ);
            eer::vmacro();
            add(r[2], r[2], r[2], XYZ);
            eer::vmacro();
            add(r[1], r[1], r[2], XYZ);
        } else {
            r[1] = r[6];
        }
        eer::vmacro();
        addbc(r[1], r[1], r[30].v[0], XY);
        --n;
        sd(out, eer::qmfc2(1).lo);
        out += 8;
    } while (n != 0);
}

// orig: L12:0x002bc210 L14:0x002ab3e8
void env_objects_draw(uint32_t recs, int32_t count, uint64_t tex0a, uint64_t tex0b) {
    sub_002a0ba8(6, tex0a);
    sub_002a0ba8(7, tex0b);
    const uint32_t cursor = OV(0x001611c0);
    sw(lwu(cursor), 0x30000009u);
    sw(lwu(cursor) + 4, OV(0x001cb320));
    sw(lwu(cursor) + 8, 0);
    sw(lwu(cursor) + 0xc, 0x50000009u);
    sw(cursor, lwu(cursor) + 0x10);
    shadows_vu1_setup();
    for (uint32_t k = 0; k < 0xb4; ++k) sw(0x70000000u + k * 4, 0x80808080u);  // (the points' colours: white)
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t rec = recs + static_cast<uint32_t>(i) * 0x20;
        const int64_t seen = point_on_screen(rec, 256.0f);
        if (seen == -1) continue;
        env_objects_uvs(lw(rec + 0x1c), lwu(rec + 0x10), lwu(rec + 0x18), 0x70001000u);
        nov_prim_list2_l1(lw(rec + 0x1c), lwu(rec + 0x10), 0x70000000u, lwu(rec + 0x14), 0x70001000u, seen == 0 ? 1u : 0u);
    }
}

// orig: L12:0x002e71b0 L14:0x00307a28
void env_objects_callback(uint32_t) {
    if (on_level(OLTANIS)) {
        const uint64_t a = effect_tex0(0x2e);
        env_objects_draw(L14_ADDR(0x001f5f80), 0x13, a, effect_tex0(0x2f));
    } else {
        const uint64_t a = effect_tex0(0x2c);
        env_objects_draw(L12_ADDR(0x001f5480), 0x2f, a, effect_tex0(0x2d));
    }
}

}  // namespace rac1
