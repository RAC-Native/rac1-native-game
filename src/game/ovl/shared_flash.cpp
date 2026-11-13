// Blarg's (level 6) and Veldin's finale's (18) flash, class 1898: spawned with its length, it is drawn each frame (its
// level's own draw callback) while its phase (pvar +0) grows by its step (+4) up to 2. With two small helpers both levels
// have: a camera type 0x12's flag, and a class's state query (another build on each level). The copies are the same code,
// relinked: the draw callback is each level's own (L6_ADDR / L18_ADDR); the cameras' records 0x15ef50 go through OV().
// Header shared_flash.h.
#include <game/ovl/shared_flash.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t kFlashClass = 0x76a;
uint32_t flash_draw() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x002fb690) : L6_ADDR(0x0030bfa8); }
}  // namespace

// orig: L6:0x0030be60 L18:0x002fb548
void flash_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        moby_retire(m);
        return;
    case 1: {
        sub_001f93e0(flash_draw(), m);
        const float phase = fpu_add(lwc1(pv), lwc1(pv + 4));
        swc1(pv, phase);
        if (2.0f < phase) moby_retire(m);
        return;
    }
    default:
        return;
    }
}

// orig: L6:0x0030bef8 L18:0x002fb5e0
uint32_t flash_spawn(uint32_t pos, int32_t frames, uint32_t a, uint32_t b) {
    const Vec4 at = V(pos);
    const uint32_t m = moby_spawn(kFlashClass);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    V(m + 0x10) = at;
    sw(pv + 0xc, b);
    sw(pv + 8, a);
    swc1(pv + 4, fpu_div(2.0f, int_to_float(frames)));
    sb(m + 0x20, 1);
    sb(m + 0x30, 0xff);
    return 1;
}

// orig: L6:0x00316330 L18:0x00306338
void camera_type12_flag(uint32_t cam) {
    if (lh(cam + 0x86) != 0x12) return;
    const uint32_t rec = lwu(OV(0x0015ef50)) + (static_cast<uint32_t>(static_cast<int32_t>(lh(cam + 0x84))) << 5);
    sw(lwu(rec + 0x1c) + 0x50, 1);
}

// orig: L6:0x003047f8
uint32_t state_query_blarg(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 7) return 2;
    return 3 < state ? 1 : 0;
}

// orig: L18:0x002dfba0
uint32_t state_query_finale(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 2) return 1;
    return state == 3 ? 2 : 0;
}

}  // namespace rac1
