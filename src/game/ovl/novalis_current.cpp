// Novalis's own moby classes: the water's current (class 613; hand-ported from Novalis's own overlay). Header
// novalis_mobys2.h, registered in novalis_mobys2.cpp. Addresses are Novalis's (`orig: L1:`); Novalis-only data is L1_ADDR,
// the base ELF's are bare (the hero 0x13f350: its position +0x80, its push +0xf0, its states +0x2084 .. +0x20a0).
//
// The pvars: the current's velocity +0 (z kept 0), its path +0x20 (-1 none), its top speed +0x24, its reach +0x28, the hero
// pulled toward the path's line +0x2c, the path's lengths set +0x2e.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
// a stack frame of EE memory for the vectors handed to the engine by address (cleared when it goes)
struct Frame {
    uint32_t sp, n;
    explicit Frame(uint32_t size) : sp(ee_stack_alloc(size)), n(size) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t kSwim = 0x11, kDive = 0x12;  // the hero's states in water

// the hero in the water (swimming or diving now, or in his last or previous states)
bool hero_in_water() {
    if (lwu(HERO + 0x208c) - kSwim < 2) return true;
    const uint32_t last = lwu(HERO + 0x2094);
    return last == kSwim || last == kDive || lwu(HERO + 0x2084) == kDive || lwu(HERO + 0x2090) == kDive;
}

// the current's speed eased by the time step (-0.005 or -0.015 a step of 0x15ed60), its push given to the hero (+0xf0)
void ease(uint32_t pv, float rate) {
    const float len = vec3_length(V(pv));
    vec3_set_length(V(pv), V(pv), fpu_add(fpu_mul(fpu_mul(lwc1(0x0015ed60), rate), len), len));
    V(HERO + 0xf0) = V(pv);
}
}  // namespace

// Class 613: a current along a path. Its path's segment lengths are set once (each point's +0xc, the last one back to the
// first). With the hero in the water and within its reach of the path's nearest point, the current carries him: swimming or
// diving, while this current is the nearest one this frame (+0x22b8), it speeds up toward the next point (7 a step, to its
// top speed) or, at the path's end, slows; its velocity is his push (+0xf0), with a pull toward the path's line (up to 0.4
// of its top speed) when +0x2c is set; diving, he is held under for 3 frames (+0x194 to 1.5 dt, +0x150 cleared). On the
// surface of a current in states 4 / 5 it slows (and keeps his +0x1d8 at 35 frames or more).
// orig: L1:0x002f3120
void nov_class613_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const int32_t pi = lw(pv + 0x20);
    if (pi == -1) return;
    const uint32_t path = lvd::path(pi);
    if (lh(pv + 0x2e) == 0) {
        sh(pv + 0x2e, 1);
        int32_t i = 0;
        for (; i < lw(path) - 1; ++i) {
            const uint32_t p = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
            swc1(p + 0xc, vec3_distance(V(p), V(p + 0x10)));
        }
        const uint32_t last = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
        swc1(last + 0xc, vec3_distance(V(last), V(path + 0x10)));
        sb(m + 0x30, 0xff);
    }
    if (!hero_in_water()) return;
    const Frame f(0x50);
    const uint32_t sp = f.sp;
    const uint32_t nearest = sp, ahead = sp + 0x10;
    // (the index and fraction words start as the frame holds them, as in the original)
    int32_t idx = lw(sp + 0x40);
    float frac = lwc1(sp + 0x44);
    path_closest_point_v(path, V(HERO + 0x80), V(nearest), &idx, &frac, 0, 999.0f, 5.0f, 0.0f);
    sw(sp + 0x40, static_cast<uint32_t>(idx));
    swc1(sp + 0x44, frac);
    const int32_t at_end = path_advance_v(path, V(ahead), &idx, &frac, 0, 2.0f);
    sw(sp + 0x40, static_cast<uint32_t>(idx));
    swc1(sp + 0x44, frac);
    const float d = vec2_distance(V(HERO + 0x80), V(nearest));
    if (lwc1(pv + 0x28) < d) return;
    const uint32_t state = lwu(HERO + 0x208c);
    if (state - kSwim < 2) {
        if (lwc1(HERO + 0x22b8) < d) return;
        swc1(HERO + 0x22b8, d);
        if (at_end != 0) {
            ease(pv, u2f(0xbba3d700u));
        } else {
            const uint32_t push = sp + 0x20;
            vec3_sub(V(push), V(ahead), V(nearest));
            vec3_set_length(V(push), V(push), fpu_mul(lwc1(0x0015ed70), 7.0f));
            vec3_add(V(pv), V(pv), V(push));
            swc1(pv + 8, 0.0f);
            const float top = lwc1(pv + 0x24);
            if (top < vec3_length(V(pv))) vec3_set_length(V(pv), V(pv), top);
            V(HERO + 0xf0) = V(pv);
            if (lh(pv + 0x2c) != 0) {
                const uint32_t pull = sp + 0x30;
                vec3_sub(V(pull), V(nearest), V(HERO + 0x80));
                swc1(pull + 8, 0.0f);
                const float most = fpu_mul(lwc1(pv + 0x24), u2f(0x3ecccccdu));
                if (most < vec3_length(V(pull))) vec3_set_length(V(pull), V(pull), most);
                vec3_add(V(HERO + 0xf0), V(HERO + 0xf0), V(pull));
            }
        }
        if (lwu(HERO + 0x208c) != kDive || lwu(HERO + 0x2090) != kDive) return;
        if (!(lw(HERO + 0x198) < frame_scale_i(3))) return;
        const float hold = fpu_mul(lwc1(0x0015ed6c), 1.5f);
        if (hold < lwc1(HERO + 0x194)) swc1(HERO + 0x194, hold);
        vec_clear(V(HERO + 0x150));
        return;
    }
    if (lwu(HERO + 0x2094) != kDive && lwu(HERO + 0x20a0) != kDive) return;
    if (state - 4 >= 2) return;
    ease(pv, u2f(0xbc75c280u));
    if (lh(HERO + 0x1d8) < frame_scale_i(35)) sh(HERO + 0x1d8, static_cast<uint32_t>(frame_scale_i(35)));
}

}  // namespace rac1
