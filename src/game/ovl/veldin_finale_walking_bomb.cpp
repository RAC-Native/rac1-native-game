// Veldin's finale (level 18) class 568: the little walking bombs (twenty of them on the level). The Suck Cannon takes them
// (the carry hooks below: carried, they are state 4); shot out, they fall, slide to a stop and walk to the nearest target
// in reach, and go off on a hit, at the end of their fuse, near the hero, held in his hand or once they reached a target.
// Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare (the hero
// 0x13f350, his state 0x1413d4, the frame time 0x15ed6c and its square 0x15ed70). Registered in veldin_finale_mobys.cpp,
// the hooks dispatched by carry_hooks_levels.cpp.
//
// The pvars: +0x40 the walk's step (+0x48 its z, kept from rising), +0x110 the carry record (carry_update), +0xc8 the carry's
// halfword, +0x178 the floor it falls to, +0x180 its velocity falling (+0x188 the z; the slide's speed after), +0x194 the
// fuse (frames), +0x19c set when it reached a target (it goes off on its next frame).
//
// States (the table L18:0x1f2630): 0 set up (state 5), 1 falls, 2 slides, 3 walks, 4 carried, 5 at rest (nothing).
#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t HERO = 0x0013f350;       // the hero (+0x80 his position; +0x23c / +0x240 what his hands hold)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// at rest: state 5, shown, out of the collision (+0x34 bits 0x41 on, 0x1000 off), its collision record +0x94 cleared
void to_rest(uint32_t m) {
    sb(m + 0x20, 5);
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
}
}  // namespace

// The bomb's checks before its state (not at rest, set up or carried): the hero dead puts it at rest; held in the hero's
// hands or having reached a target it goes off big; at the end of its fuse, hit (the mask 0x330000) or, falling, within 2
// of the hero it goes off small; then at rest.
// orig: L18:0x002d5f20
void vf_bomb_fuse_l18(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 5 || st == 0 || st == 4) return;
    const uint32_t pv = lwu(m + 0x78);
    if (lw(g::HERO_STATE) == 0x72) {
        to_rest(m);
        return;
    }
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    sb(m + 0xa4, 0xff);
    if (lwu(HERO + 0x23c) == m || lwu(HERO + 0x240) == m || lw(pv + 0x19c) != 0) {
        explosion_effect(m, L18_ADDR(0x0015f580), 0, 0x14, 6, 0x20, 1, 1, 0, -1, 0, 1.5f, 1.0f, 3.0f, 1.5f, 9.0f, 1.5f, 0.0f);
        to_rest(m);
        return;
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x194))) == 0 && hit == 0) {
        if (lbu(m + 0x20) != 1) return;
        if (!(vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 2.0f)) return;
    }
    explosion_effect(m, L18_ADDR(0x0015f580), 0, 5, 2, 8, 1, 0, 0, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 9.0f, 0.5f, 0.0f);
    to_rest(m);
}

// orig: L18:0x002d5918
void vf_class568_update_l18(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    vf_bomb_fuse_l18(m);
    switch (lbu(m + 0x20)) {  // (the table L18:0x1f2630)
    case 0:  // (0x002d5970)
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
        swc1(pv + 0x30, u2f(0x3d4ccccdu));  // 0.05
        sw(pv + 0xd0, L18_ADDR(0x00161988));
        sb(m + 0x20, 5);
        return;
    case 1: {  // falls to its floor +0x178 (0x002d59a8)
        vec3_add(V(pos), V(pos), V(pv + 0x180));
        swc1(pv + 0x188, fpu_sub(lwc1(pv + 0x188), fpu_mul(dt_sq(), 10.0f)));
        const float floor = lwc1(pv + 0x178);
        if (!(lwc1(m + 0x18) < floor)) return;
        swc1(m + 0x18, floor);
        sb(m + 0x20, 2);
        const float k = lwc1(L18_ADDR(0x00161994));
        swc1(pv + 0x188, -fpu_mul(fpu_add(k, k), dt()));
        return;
    }
    case 2: {  // slides along its yaw, slowing to a stop (0x002d5a18)
        const float k = lwc1(L18_ADDR(0x00161994));
        const float speed = approach_f(pv + 0x188, 0.0f, fpu_mul(fpu_add(k, k), dt_sq()));
        swc1(m + 0x44, fpu_add(lwc1(m + 0x44), speed));
        swc1(sp + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), speed));
        swc1(sp + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), speed));
        swc1(sp + 8, 0.0f);
        vec3_add(V(pos), V(pos), V(sp));
        if (lwc1(pv + 0x188) != 0.0f) return;
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(12));
        sb(m + 0xa4, 0xff);
        sb(m + 0x20, 3);
        return;
    }
    case 3: {  // walks to the nearest target within 6 (0x002d5ad0)
        if (lbu(m + 0x53) == 1 && (lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 2, 0, frame_scale_i(12));
        if (lbu(m + 0x31) == 0) return;
        const float reach = 6.0f;
        if (target_select_any(m, sp + 0x10, reach) == 2) return;
        const uint32_t tgt = lwu(sp + 0x50);
        if (tgt == 0) return;
        if (!(vec2_distance(V(pos), V(tgt + 0x10)) < reach)) return;
        const float top = fpu_mul(dt(), 3.0f);
        const float acc = fpu_mul(dt_sq(), reach);
        const uint32_t was = sp + 0x60, step = pv + 0x40;
        lq_sq(was, pos);
        vec3_sub(V(sp), V(tgt + 0x10), V(pos));
        swc1(sp + 8, -fpu_mul(dt_sq(), 10.0f));
        if (acc < vec2_length(V(sp))) vec2_set_length(V(sp), V(sp), acc);
        vec3_add(V(step), V(step), V(sp));
        if (top < vec2_length(V(step))) vec2_set_length(V(step), V(step), top);
        walker_move(m, step, 0, 0.0f, 0.5f, 0.0f);
        vec3_sub(V(step), V(pos), V(was));
        const float rise = lwc1(pv + 0x48);
        if (0.0f < rise) swc1(pv + 0x48, 0.0f);
        else if (rise < -top) swc1(pv + 0x48, -top);
        if (tgt == lwu(g::HERO_MOBY)) return;
        if (vec3_distance(V(tgt + 0x10), V(pos)) < 2.0f) sw(pv + 0x19c, 1);
        return;
    }
    case 4:  // carried (0x002d5cb4)
        if (carry_update(m, pv + 0x110) != 0) {
            sb(m + 0x20, 3);
            sh(pv + 0xc8, 0);
        }
        return;
    default:
        return;
    }
}

// ---- the carry hooks of class 568 (its table L18:0x1f3498; +4 and +8 are the shared carry_state_hook)

// Hook +0: the carry hook +0; taken while walking it is carried (state 4, the state to go back to 3) and the hook says 2
// (2 also when carried already); in another state it is refused (the carry's halfword cleared). Not taken while
// carried, it goes back to its state +0xbc.
// orig: L18:0x002d6108
int32_t vf_bomb_hook0_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t taken = carry_hook0(m, a1, a2, f12);
    const uint32_t st = lbu(m + 0x20);
    if (taken == 0) {
        if (st == 4) sb(m + 0x20, lbu(m + 0xbc));
        return 0;
    }
    if (st != 3 && st != 4) {
        sh(pv + 0xc8, 0);
        return 0;
    }
    if (st == 3) {
        sb(m + 0x20, 4);
        sb(m + 0xbc, 3);
    }
    return 2;
}

// Hook +0xc: the carry hook +0xc.
// orig: L18:0x002d6230
int32_t vf_bomb_hookc_l18(uint32_t m, uint32_t a1, uint32_t a2, float f12) { return carry_hookc(m, a1, a2, f12); }

// Hook +0x10: the moby's pvars +0x60 (0 without the moby or its pvars).
// orig: L18:0x002d6250
uint32_t vf_bomb_hook10_l18(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60 : 0;
}

}  // namespace rac1
