// The path-riding rafts' helpers, one port of the copies Rilgar, Batalia, Oltanis and Kalebo III carry: the same code
// relinked (compared with the relocated fields masked, and normalised) but for the pvars' offsets of each level's raft
// class (parameters) and two of the HUD message ids (parameters). Their level-own words: the 7 offsets of the raft's
// path (below); the rest is the engine's (OV: the level's paths 0x1b04b0, its group lists 0x1ab840, its mobys 0x15ffd8) or
// the base ELF's (bare: the hero 0x13f350.., the frame time 0x15ed6c). Header shared_floaters.h.
#include <game/ovl/shared_floaters.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
// the raft's path's 7 offsets (vectors)
uint32_t path_offsets() {
    if (on_level(RILGAR)) return L5_ADDR(0x00215c00);
    if (on_level(OLTANIS)) return L14_ADDR(0x001e0460);
    return L16_ADDR(0x001d96e0);
}

// an angle kept within +-10 degrees
float within_10(float a) {
    const float most = u2f(0x3e32b8c2u);
    if (most < a) return most;
    if (a < -most) return -most;
    return a;
}
}  // namespace

// Its phases turn at 120, 97 and 53 degrees a second: it bobs (a quarter up and down) and rocks about y and x (5 degrees,
// within 10).
// orig: L5:0x00319740 L8:0x002dc4a0 L14:0x002f1490 L16:0x002e4d88
void floater_bob(uint32_t m, uint32_t phases) {
    const uint32_t pv = lwu(m + 0x78);
    const float rock = u2f(0x3db2b8c2u);
    lq_sq(m + 0x10, pv + 0x60);
    lq_sq(m + 0x40, pv + 0x70);
    const float p0 = angle_add(lwc1(pv + phases), fpu_mul(lwc1(0x0015ed6c), u2f(0x40060a92u)));
    swc1(pv + phases, p0);
    const float bob = fpu_mul(sin_vu(p0), 0.25f);
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), bob));
    const float p1 = angle_add(lwc1(pv + phases + 4), fpu_mul(lwc1(0x0015ed6c), u2f(0x3fd8b338u)));
    swc1(pv + phases + 4, p1);
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(sin_vu(p1), rock)));
    const float p2 = angle_add(lwc1(pv + phases + 8), fpu_mul(lwc1(0x0015ed6c), u2f(0x3f6cce68u)));
    swc1(pv + phases + 8, p2);
    swc1(m + 0x40, within_10(angle_add(lwc1(m + 0x40), fpu_mul(sin_vu(p2), rock))));
    swc1(m + 0x44, within_10(lwc1(m + 0x44)));
}

// orig: L5:0x003195f8 L14:0x002f1360 L16:0x002e4c40
int32_t floater_ride(uint32_t m, uint32_t along, uint32_t path) {
    const uint32_t pv = lwu(m + 0x78);
    const float rate = lwc1(pv + along + 0x18);
    seek_1d(pv + along, pv + along + 4, 1.0f, rate, rate, lwc1(pv + along + 0x14));
    const uint32_t p = lwu(pv + path);
    path_point(fpu_mul(lwc1(pv + along), static_cast<float>(lw(p) - 1)), p, 0, pv + 0x60, pv + 0x70, 0);
    return 1.0f <= lwc1(pv + along) ? 1 : 0;
}

// orig: L5:0x003198e8 L14:0x002f1638 L16:0x002e4f30
void floater_lay_path(uint32_t m, uint32_t path) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t index = lw(pv + path);
    if (index == -1) return;
    const uint32_t p = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(index) << 2));
    if (lw(p) != 7) return;
    const uint32_t offsets = path_offsets();
    for (uint32_t k = 0; k < 7; ++k) {
        const uint32_t at = p + 0x10 + k * 0x10;
        mat3_transform(V(at), V(offsets + k * 0x10), M(m + 0xc0));
        vec3_add(V(at), V(at), V(m + 0x10));
    }
    sh(0x0013f350 + 0x22da, lhu(pv + path));
}

// orig: L5:0x00319690 L16:0x002e4cd8
void floater_message(uint32_t m, uint32_t stage, int32_t first, int32_t second) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t s = lh(pv + stage);
    int32_t id;
    switch (static_cast<uint32_t>(s) < 5 ? s : -1) {  // (the stage's jump table, L5:0x00218ae0 / L16:0x001e8820)
    case 0: id = first; break;
    case 1: id = second; break;
    case 2: id = TXT(0x139a); break;
    case 3: id = TXT(0x139c); break;
    case 4: id = TXT(0x1399); break;
    default: id = TXT(0x139b); break;
    }
    hud_message_request(8, id);
}

// orig: L5:0x00319598 L16:0x002e4be0
uint32_t floater_find(uint32_t m, int32_t value, uint32_t key) {
    uint32_t e = lwu(OV(0x001ab840) + (lbu(m + 0x21) << 2));
    const uint32_t mobys = lwu(OV(0x0015ffd8));
    if (e == 0) return 0;
    for (;; e += 2) {
        const uint32_t half = lhu(e);
        const uint32_t o = mobys + ((half & 0x7fffu) << 8);
        if (lh(lwu(o + 0x78) + key) == value) return o;
        if ((half & 0x8000u) != 0) return 0;
    }
}

}  // namespace rac1
