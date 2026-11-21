// Small spawn and group helpers of Eudora (level 4), Gaspar (9), Orxon (10) and Hoven (12). Each one's copies are the same
// code apart from the class they spawn; Gaspar's spawn is another build (it copies other words), written apart. The group
// lists 0x1ab840 and the moby list 0x15ffd8 go through OV(). Header shared_spawns.h.
#include <game/ovl/shared_spawns.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: L4:0x002ce080 L4:0x002d34d8 L12:0x002f1118
uint32_t spawn_like(uint32_t src, uint32_t pos, uint32_t cls) {
    const Vec4 at = V(pos);
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    sb(m + 0x31, 1);
    sh(m + 0x32, lhu(src + 0x32));
    sw(m + 0x40, 0);
    sw(m + 0x44, 0);
    swc1(m + 0x48, lwc1(src + 0x48));
    sd(m + 0x38, ld(src + 0x38));
    V(m + 0x10) = at;
    moby_update_transform(m);
    return m;
}

// orig: L9:0x002efda8
uint32_t spawn_like_gaspar(uint32_t src, uint32_t pos) {
    const Vec4 at = V(pos);
    const uint32_t m = moby_spawn(0x144);
    if (m == 0) return 0;
    sb(m + 0x30, lbu(src + 0x30));
    sb(m + 0x31, 1);
    sh(m + 0x32, lbu(src + 0x30));
    V(m + 0x10) = at;
    V(m + 0x40) = V(src + 0x40);
    sd(m + 0x38, ld(src + 0x38));
    moby_update_transform(m);
    return m;
}

// orig: L10:0x002dd848 L10:0x002eb8c0
uint32_t spawn_over_at_59(uint32_t src, uint32_t cls) {
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    sw(m + 0x40, 0);
    sw(m + 0x44, 0);
    swc1(m + 0x48, lwc1(src + 0x48));
    sd(m + 0x38, ld(src + 0x38));
    V(m + 0x10) = V(src + 0x10);
    swc1(m + 0x18, 59.0f);
    moby_update_transform(m);
    return m;
}

// orig: L10:0x002e0138 L10:0x002e47d8
void group_wake(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return;
    uint32_t e = lwu(OV(0x001ab840) + (group << 2));
    if (e == 0) return;
    for (;; e += 2) {  // (the group's halves, the last with bit 15)
        const uint32_t member = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lbu(member + 0x20) == 1) sb(member + 0x20, 2);
        if (lh(e) < 0) break;
    }
}

}  // namespace rac1
