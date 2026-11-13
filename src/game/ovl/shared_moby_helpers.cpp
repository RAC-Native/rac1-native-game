// Small moby helpers: one port of the copies the levels carry (the same code relinked; the state test's state differs per
// level: a parameter). The group lists 0x1ab840 and the moby table 0x15ffd8 are the engine's (OV). Header
// shared_moby_helpers.h.
#include <game/ovl/shared_moby_helpers.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: L13:0x002650a8 L16:0x00254a30 L17:0x002539c8
void group_set_flags(int32_t group, int32_t far, int32_t shown, int32_t solid) {
    uint32_t e = lwu(OV(0x001ab840) + (static_cast<uint32_t>(group) << 2));
    if (e == 0) return;
    for (;; e += 2) {
        const uint32_t m = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (static_cast<int8_t>(lbu(m + 0x20)) >= 0) {  // (live)
            if (far != -1) sh(m + 0x34, far == 0 ? (lhu(m + 0x34) | 2u) : (lhu(m + 0x34) & 0xfffdu));
            if (shown != -1) {
                sb(m + 0x31, shown == 0 ? 0u : 1u);
                sh(m + 0x34, shown == 0 ? (lhu(m + 0x34) | 1u) : (lhu(m + 0x34) & 0xfffeu));
            }
            if (solid != -1) sw(m + 0x94, solid == 0 ? 0u : lwu(lwu(m + 0x24) + 0x10));
        }
        if (lh(e) < 0) break;  // (the group's last member)
    }
}

// orig: L13:0x002e1c50 L17:0x002c5360
uint32_t spawn_at_place(uint32_t pos, uint32_t cls) {
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    sb(m + 0xbc, 0);
    lq_sq(m + 0x10, pos);
    moby_update_transform(m);
    moby_copy_hero_color(m);
    return m;
}

// orig: L13:0x002ba088 L17:0x002ec270
float ease_between(int32_t angles, int32_t power, float t, float from, float to, float t0, float t1) {
    if (!(t0 < t)) return from;
    float k = fpu_div(fpu_sub(t, t0), fpu_sub(t1, t0));
    if (k < 0.0f) k = 0.0f;
    if (1.0f < k) k = 1.0f;
    for (int32_t i = 1; i < power; ++i) k = fpu_mul(k, k);
    if (angles != 0) return angle_add(from, fpu_mul(angle_sub(to, from), k));
    return fpu_add(fpu_mul(from, fpu_sub(1.0f, k)), fpu_mul(to, k));
}

// orig: L11:0x0030a830 L11:0x0030e078 L16:0x002c4710
bool moby_in_state(uint32_t m, uint32_t state) { return lbu(m + 0x20) == state; }

}  // namespace rac1
