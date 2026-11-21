// Veldin's finale (level 18) class 644: a scripted camera move between two points, behind a fade. Started (state 2 by the
// level's script) it fades the screen out, holds the hero (his state 0x72) where the move puts him, then the camera
// glides from the pvars' first point and angles to the second (state 3; state 4 when the move asks for Clank's line
// 0x4650 first, and lets the hero go half way through when no talk is running); at the end the fade lifts, the hero is
// let go and the level's hydro routine L18:0x2eac18 is told the move is over (2 or 3). State 5 lifts what is left of the
// fade, then the moby waits (state 1) for the next start. Level 18 only: the addresses are the finale's (`orig: L18:`),
// its own data L18_ADDR, the base ELF's bare. Registered in veldin_finale_mobys.cpp.
//
// The pvars: +0 the camera's first point, +0x10 its last, +0x20 / +0x30 its first / last angles (y at +4, z at +8), +0x40
// where the hero stands (his yaw +0x4c), +0x60 the move's progress 0..1, +0x64 its speed, +0x68 its acceleration, +0x6c the
// fade, +0x70 the top speed, +0x74 the wait before the move, +0x78 the move with Clank's line.
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/globals.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the fade toward `goal` (4 a second); returns the fade now
float fade_to(uint32_t pv, float goal) {
    approach_f(pv + 0x6c, goal, fpu_mul(dt(), 4.0f));
    return lwc1(pv + 0x6c);
}

// the hero put at the pvars' +0x40, turned by their +0x4c, in `state` (0x72 held, 0 let go); `rot` a quadword of the frame
void hero_put(uint32_t pv, uint32_t rot, int32_t state, int32_t cam) {
    vec_clear(V(rot));
    swc1(rot + 8, lwc1(pv + 0x4c));
    hero_place(pv + 0x40, rot, state, cam);
}

// one step of the move: after the wait, the progress toward 1; the camera at the point and the angles of the progress
// (`buf`: two quadwords of the frame)
void camera_step(uint32_t pv, uint32_t buf) {
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x74))) != 0)
        seek_1d(pv + 0x60, pv + 0x64, 1.0f, lwc1(pv + 0x68), lwc1(pv + 0x68), lwc1(pv + 0x70));
    const uint32_t at = buf, angles = buf + 0x10;
    vec3_lerp(V(at), V(pv), V(pv + 0x10), lwc1(pv + 0x60));
    vec_clear(V(angles));
    swc1(angles + 4, angle_add(lwc1(pv + 0x24), fpu_mul(angle_sub(lwc1(pv + 0x34), lwc1(pv + 0x24)), lwc1(pv + 0x60))));
    swc1(angles + 8, angle_add(lwc1(pv + 0x28), fpu_mul(angle_sub(lwc1(pv + 0x38), lwc1(pv + 0x28)), lwc1(pv + 0x60))));
    uid5_set_pvar_80(at);
    uid5_set_pvar_90(angles);
}

// the end of the move: the fade gone, the hero let go, the hydro routine told (`how`), the moby in state 5
void finish(uint32_t m, uint32_t pv, uint32_t rot, uint32_t how) {
    sw(L18_ADDR(0x0015f404), 0);
    hero_put(pv, rot, 0, 0);
    hydro_2eac18(how);
    sb(m + 0x20, 5);
}
}  // namespace

// orig: L18:0x002df608
void vf_class644_update_l18(uint32_t m) {
    const Frame f(128);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), rot = sp + 0x20;
    switch (lbu(m + 0x20)) {  // (the table L18:0x1f2740)
    case 0:  // (0x002df650)
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        return;
    case 2: {  // the fade out, then the hero held and the camera put at the start (0x002df664)
        sw(L18_ADDR(0x0017ef08), 1);
        const float fade = fade_to(pv, 1.0f);
        if (lwc1(L18_ADDR(0x0015f3fc)) < fade) swc1(L18_ADDR(0x0015f3fc), fade);
        if (!(1.0f <= fade)) return;
        sw(L18_ADDR(0x0015f404), 1);
        hero_put(pv, sp, 0x72, 1);
        camera_scripted_set(pv, pv + 0x20, 1, 0, 0);
        if (lw(pv + 0x78) != 1) {
            sb(m + 0x20, 3);
        } else {
            sub_00203908(0x4650, 0x85);  // (Clank's line: a dialogue request)
            sb(m + 0x20, 4);
        }
        return;
    }
    case 3: {  // the move, the fade lifting (0x002df724)
        sw(L18_ADDR(0x0017ef08), 1);
        camera_step(pv, sp);
        const float fade = fade_to(pv, 0.0f);
        const float t = lwc1(pv + 0x60);
        swc1(L18_ADDR(0x0015f3fc), fade);
        if (1.0f <= t) {
            finish(m, pv, rot, 2);
        } else if (lw(g::HERO_STATE) != 0x72) {
            hero_put(pv, rot, 0x72, 1);
        }
        return;
    }
    case 4: {  // the move with Clank's line (0x002df870)
        camera_step(pv, sp);
        const float t = lwc1(pv + 0x60);
        // past a fifth of the move with no talk running, or at its end: the fade back in, then the end
        if (1.0f <= t || (0.2f < t && lw(L18_ADDR(0x00179e10)) == 0 && lw(L18_ADDR(0x00179e34)) == -1)) {
            const float fade = fade_to(pv, 1.0f);
            swc1(L18_ADDR(0x0015f3fc), fade);
            if (!(1.0f <= fade)) return;
            swc1(pv + 0x60, 1.0f);
            finish(m, pv, rot, 3);
        } else if (lw(g::HERO_STATE) == 0x72) {
            swc1(L18_ADDR(0x0015f3fc), fade_to(pv, 0.0f));
        } else {
            hero_put(pv, rot, 0x72, 1);
        }
        return;
    }
    case 5:  // what is left of the fade lifted, then the wait (0x002dfa40)
        if (0.0f < lwc1(pv + 0x6c)) swc1(L18_ADDR(0x0015f3fc), fade_to(pv, 0.0f));
        else sb(m + 0x20, 1);
        return;
    default:  // 1 (0x002dfa84) and beyond the table
        return;
    }
}

}  // namespace rac1
