// Novalis's own moby classes, part 2a: the beehive. Header novalis_mobys2.h. Addresses are Novalis's (`orig: L1:`);
// Novalis-only data is L1_ADDR, the globals the
// engine shares with Veldin are the level's own (OV).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// How many of group `group`'s mobys (the list 0x1ab840 / Novalis 0x1abcc0, halfword ids, the last with bit 15) are
// alive (state byte +0x20 not negative) and, unless `state` is -1, not in state `state`.
// (Shared: the same code on Blarg, Gaspar, Pokitaru, Hoven, Gemlik, Oltanis, Quartu and Kalebo III.)
// orig: L1:0x0026e008 L6:0x00267ee8 L9:0x00278c78 L11:0x0027f058 L12:0x00271ff8 L13:0x00264f78 L14:0x00261728 L15:0x002489f0 L16:0x00254900
int32_t moby_group_count_l1(int32_t group, int32_t state) {
    uint32_t p = lwu(OV(0x001ab840) + (static_cast<uint32_t>(group) << 2));
    if (p == 0) return 0;
    int32_t n = 0;
    const uint32_t base = lwu(OV(0x0015ffd8));
    for (;; p += 2) {
        const uint32_t id = lhu(p);
        const uint32_t mm = base + ((id & 0x7fffu) << 8);
        if (!(static_cast<int8_t>(lbu(mm + 0x20)) < 0)) {
            if (state == -1) ++n;
            else if (lbu(mm + 0x20) != static_cast<uint32_t>(state)) ++n;
        }
        if (static_cast<int32_t>(id << 16) < 0) break;
    }
    return n;
}

// A bee for the hive `m`: the first moby of the group in the hive's pvar +0 (group_first_r, filter 0, 0), made
// visible (+0x30 = 0x80, +0x32 = 0x80, +0x31 = 1), with the hive's +0x38 pair, its class's +0x10 at +0x94, the flags
// |= 0x1020, its class's +0x24 at +0x2c, the hive's position and angles; its pvar: +0x250 = 1, +0x20 = 3, +0x170 =
// float_to_int(819.2), +0x224 = 30; then its transform is updated.
// orig: L1:0x00302328
uint32_t bee_spawn_l1(uint32_t m) {
    uint32_t b = 0;
    group_first_r(b, lw(lwu(m + 0x78)), 0, 0);
    sb(b + 0x30, 0x80);
    const uint32_t pv = lwu(b + 0x78);
    sh(b + 0x32, 0x80);
    sd(b + 0x38, ld(m + 0x38));
    sb(b + 0x31, 1);
    sw(b + 0x94, lwu(lwu(b + 0x24) + 0x10));
    sh(b + 0x34, lhu(b + 0x34) | 0x1020u);
    swc1(b + 0x2c, lwc1(lwu(b + 0x24) + 0x24));
    lq_sq(b + 0x10, m + 0x10);
    lq_sq(b + 0x40, m + 0x40);
    swc1(pv + 0x250, 1.0f);
    swc1(pv + 0x20, u2f(0x40400000u));
    sw(pv + 0x170, static_cast<uint32_t>(float_to_int(u2f(0x444ccccdu))));
    swc1(pv + 0x224, u2f(0x41f00000u));
    moby_update_transform(b);
    return b;
}

// Class 815, the beehive: state 0 -> 1; state 1 (its group in pvar +0 >= 0), when none of the group's bees is alive
// (the count with 0xc) and the count pvar +8 is below the level's limit (Novalis 0x161e90), launches one (bee_spawn,
// the count + 1): its pvar +0x84 = 8, +0x80 = float_to_int(+0x250 * 0.8 * 1024), +0x88 = +0x250 * 0.8, +0xb0 = 7,
// +0xb4 = 13, +0x78 / +0x7c cleared, a walker move (set 5, 8 frames) facing away from the camera (atan2 of the
// camera - the hive, + pi), state 9.
// orig: L1:0x003021b8
void beehive_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    const int32_t group = lw(pv);
    if (group < 0) return;
    if (moby_group_count_l1(group, 0xc) != 0) return;
    const int32_t n = lw(pv + 8);
    if (!(n < lw(L1_ADDR(0x00161e90)))) return;
    sw(pv + 8, static_cast<uint32_t>(n + 1));
    const uint32_t b = bee_spawn_l1(m);
    if (b == 0) return;
    const uint32_t bp = lwu(b + 0x78);
    const uint32_t rec = bp + 0x60;
    const float k = u2f(0x3f4ccccdu);
    sw(bp + 0x84, 8);
    sw(bp + 0x80, static_cast<uint32_t>(float_to_int(lwc1(bp + 0x250) * k * u2f(0x44800000u))));
    swc1(bp + 0xb0, u2f(0x40e00000u));
    swc1(bp + 0x88, lwc1(bp + 0x250) * k);
    swc1(bp + 0xb4, u2f(0x41500000u));
    sw(bp + 0x78, 0);
    sw(bp + 0x7c, 0);
    const float dy = lwc1(0x0013f350 + 0x84) - lwc1(m + 0x14);
    const float dx = lwc1(0x0013f350 + 0x80) - lwc1(m + 0x10);
    const float ang = angle_add(atan2_f(dx, dy), u2f(0x40490fdbu));
    move_state_start(b, rec, 5, 8, 0, ang);
    sb(b + 0x20, 9);
}

}  // namespace rac1
