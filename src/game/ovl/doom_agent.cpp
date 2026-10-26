// Ported overlay code: the Glove of Doom's thrown agent (moby class 0xe6, spawned by 0x2c8218 and
// launched by 0x2c82f0). Its update 0x2c89e8 is a state machine (jump table 0x1e9d60):
// 0 start, 1 wait in the hand (the glove must stay the equipped weapon), 2 flight (gravity, the
// collision step 0x2c8440 first), 3 on the ground spawning the agents proper (class 0xba, one every
// 8 frames, 4 of them, at most 8 alive: 0x2bfe08), 4 fade out, 5 / 6 end. Its pvars: +0 the velocity,
// +0x10 the last contact point (its normal +0x20), +0x30 the glove moby, +0x34 the glow timer, +0x38
// the bounce / spawn timer (u16), +0x3a a flag (the throw is under way), +0x3c the agents spawned.
// The draw callback 0x2c7df8 (two crossed discs at the contact point) is queued by the collision step.
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/lib/libc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
// orig: (helper) a function's stack frame in the EE stack (its slots addressed as in the original)
struct StackFrame {
    uint32_t n, base;
    explicit StackFrame(uint32_t bytes) : n(bytes), base(ee_stack_alloc(bytes)) {}
    ~StackFrame() { ee_stack_free(n); }
    StackFrame(const StackFrame&) = delete;
    StackFrame& operator=(const StackFrame&) = delete;
};
}  // namespace

// A type 5 particle (a puff: colour bytes r / g / b over 0x7f alpha, +0xc `f13`, size / life) at
// `pos`; none without a life.
// orig: 0x00269b70
void spawn_particle_05(uint32_t pos, uint32_t r, uint32_t g, uint32_t b, int32_t life, float size, float f13) {
    if (life == 0) return;
    const uint32_t p = alloc_particle(5);
    if (p == 0) return;
    V(p + 0x10) = V(pos);
    const uint32_t q = p + 0x20;
    sw(p + 4, ((g & 0xffu) << 8) | 0x7f000000u | ((b & 0xffu) << 16) | (r & 0xffu));
    const int32_t four = float_to_int(4.0f);
    swc1(p + 0xc, f13);
    sb(p + 9, static_cast<uint32_t>(four - 0x60));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    sb(p + 8, 0);
    sb(p + 2, lbu(lwu(OV(0x001b2094))));
    sw(q, static_cast<uint32_t>(life));
    swc1(q + 4, fpu_div(size, int_to_float(life)));
}

// A type 0x20 particle (a glow) following the moby `m` (its +0x10 place): the fade rate 0x15ed60 x
// 0.016, a random phase, white, 4 * ..., 50000.0 bright; returns it (0 when none).
// orig: 0x0026ed28
uint32_t glow_particle_20(uint32_t m) {
    const uint32_t p = alloc_particle(0x20);
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    sw(q, m);
    sw(q + 4, 0);
    swc1(q + 8, lwc1(0x0015ed60) * u2f(0x3c83126fu));
    swc1(q + 0xc, rand_range_f(lwc1(q + 4), 1.0f));
    swc1(q + 0x10, 1.0f);
    swc1(q + 0x14, 1.0f);
    swc1(q + 0x18, 1.0f);
    V(p + 0x10) = V(m + 0x10);
    sw(p + 4, rgba_pack(1.0f, 1.0f, 1.0f, lwc1(q + 4)));
    const int32_t four = float_to_int(4.0f);
    sb(p + 1, 0);
    sb(p + 3, 0x48);
    sb(p + 9, static_cast<uint32_t>(four + 0x40));
    swc1(p + 0xc, rand_range_f(u2f(0x47435000u), u2f(0x47435000u)));
    sb(p + 8, static_cast<uint32_t>(float_to_int(lwc1(q + 0xc) * u2f(0x437f0000u))));
    sb(p + 2, lbu(lwu(OV(0x001b20a0))));
    return p;
}

// One of the Glove of Doom's agents proper (class 0xba) for the glove `owner` at `pos` moving by
// `vel`: its pvars +0x10 the velocity, +0x60 the owner; scale 0.1 of its class's +0x24. 0 when none.
// orig: 0x002bfe08
uint32_t doom_spawn_agent(uint32_t owner, uint32_t pos, uint32_t vel) {
    const uint32_t a = moby_spawn(0xba);
    if (a == 0) return 0;
    sb(a + 0x30, 0xff);
    sh(a + 0x32, 0xff);
    sb(a + 0x31, 1);
    const uint32_t pv = lwu(a + 0x78);
    sb(a + 0x20, 0);
    sb(a + 0xbc, 0);
    V(a + 0x10) = V(pos);
    V(pv + 0x10) = V(vel);
    sw(pv + 0x60, owner);
    moby_update_transform(a);
    moby_copy_hero_color(a);
    swc1(a + 0x2c, lwc1(lwu(a + 0x24) + 0x24) * u2f(0x3dcccccdu));
    return a;
}

// The agent's splash when it lands on a soft surface (the surface type of the last probe 0, 1, 3,
// 8, 11, 12 or 13, jump table 0x1e9d20): 20 puffs (type 5) around its place, colours from the
// random bytes, then state 4. Other surfaces: nothing.
// orig: 0x002c8078
void doom_agent_splash(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x10);
    const int32_t surf = sub_001f35d8();
    switch (surf) {
    case 0: case 1: case 3: case 8: case 11: case 12: case 13: {
        const float lo = u2f(0xbe99999au), hi = u2f(0x3e99999au);
        for (int32_t s3 = 0x13; s3 >= 0;) {
            V(sp) = V(m + 0x10);
            const float r1 = rand_range_f(lo, hi);
            --s3;
            swc1(sp, lwc1(sp) + r1);
            const float r2 = rand_range_f(lo, hi);
            swc1(sp + 4, lwc1(sp + 4) + r2);
            const float r3 = rand_range_f(lo, hi);
            swc1(sp + 8, lwc1(sp + 8) + r3);
            uint32_t s1 = (static_cast<uint32_t>(libc_rand()) + 0x30u) & 0x3fu;
            uint32_t s0 = (static_cast<uint32_t>(libc_rand()) + 0x20u) & 0x3fu;
            const uint32_t s2 = static_cast<uint32_t>(libc_rand()) & 0x2fu;
            const float f20 = rand_range_f(u2f(0x47c35000u), u2f(0x49435000u));
            const int32_t life = frame_scale_i(ee_rem(libc_rand(), 0x28) + 10);
            spawn_particle_05(sp, s1 & 0xffu, s0 & 0xffu, s2, life, f20, 0.0f);
            if (s3 < 0) break;
        }
        sb(m + 0x20, 4);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x10);
}

// The agent's draw callback (queued by 0x2c8440): two crossed discs (effect textures 0x11 / 0x12) at
// the contact point pvars +0x10 on the surface's frame (normal +0x20), turned about it by the
// frame counter's phase over 180 frames (the second one the other way), 0x80808080; not in game
// mode 2. As the Mine Glove's aim marker 0x2a9c50, with the spin angles on the stack.
// orig: 0x002c7df8
void doom_agent_draw(uint32_t m) {
    if (lw(OV(0x0015f5c4)) == 2) return;
    StackFrame fr(0x200);
    const uint32_t sp = fr.base;
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t s4 = 0; s4 < 2;) {
        const bool odd = (s4 & 1) != 0;
        const float a = atan2_f(sqrt_f(lwc1(pv + 0x20) * lwc1(pv + 0x20) + lwc1(pv + 0x28) * lwc1(pv + 0x28)), lwc1(pv + 0x24));
        swc1(sp + 0x110, angle_add(-a, u2f(0x3fc90fdbu)));
        swc1(sp + 0x114, atan2_f(lwc1(pv + 0x28), lwc1(pv + 0x20)));
        sw(sp + 0x118, 0);
        sw(sp + 0x11c, 0);
        sw(sp + 0x120, 0);
        const int32_t period = frame_scale_i(0xb4);
        const int32_t rem = ee_rem(lw(OV(0x0015f5cc)), period);
        float f20 = int_to_float(rem);
        const float f0 = frame_scale_f(u2f(0x43340000u));
        f20 = f20 * u2f(0x40c90fdbu);
        float ang = angle_wrap(fpu_div(f20, f0));
        if (odd) ang = -ang;
        swc1(sp + 0x124, ang);
        sw(sp + 0x128, 0);
        sw(sp + 0x12c, 0);
        euler_to_mat4_vu(M(sp + 0xd0), V(sp + 0x120));
        euler_to_mat4_vu(M(sp + 0x90), V(sp + 0x110));
        const Mat4 frame = M(sp + 0x90);
        const Mat4 spin = M(sp + 0xd0);
        mat4_mul(M(sp + 0x90), frame, spin);
        V(sp + 0xc0) = V(pv + 0x10);
        const int32_t tex = odd ? 0x12 : 0x11;
        sw(sp + 0x40, 0x80808080u);
        sw(sp + 0x44, 0x80808080u);
        sw(sp + 0x48, 0x80808080u);
        sw(sp + 0x4c, 0x80808080u);
        ++s4;
        const uint64_t tex0 = effect_tex0(tex);
        sd(sp + 0x88, 0x8000000048ull);
        sd(sp + 0x80, 0x0000ff9000000260ull);
        sd(sp + 0x78, tex0);
        sd(sp + 0x70, 0);
        swc1(sp, -1.0f);
        sw(sp + 4, 0);
        swc1(sp + 8, 1.0f);
        swc1(sp + 0xc, 1.0f);
        swc1(sp + 0x10, 1.0f);
        sw(sp + 0x14, 0);
        swc1(sp + 0x18, 1.0f);
        swc1(sp + 0x1c, 1.0f);
        swc1(sp + 0x20, -1.0f);
        sw(sp + 0x24, 0);
        swc1(sp + 0x28, -1.0f);
        swc1(sp + 0x2c, 1.0f);
        swc1(sp + 0x30, 1.0f);
        sw(sp + 0x34, 0);
        swc1(sp + 0x38, -1.0f);
        swc1(sp + 0x3c, 1.0f);
        sw(sp + 0x50, 0);
        swc1(sp + 0x54, 1.0f);
        swc1(sp + 0x58, 1.0f);
        swc1(sp + 0x5c, 1.0f);
        sw(sp + 0x60, 0);
        sw(sp + 0x64, 0);
        swc1(sp + 0x68, 1.0f);
        sw(sp + 0x6c, 0);
        quad_draw(sp, sp + 0x90);
    }
}


// The agent's collision step (called with its pvars `pv`, the agent `m` and a mode: 1 while the glove
// throws it, 0 / 2 later): the agent is moved ahead (pvars +0 under gravity, in the first state 0.2 of
// the hero's facing) in rays of ten substeps, each tested against the world (the hero and the glove
// ignored for a while); on a hit its contact point (+0x10, normal +0x20) is set, 0.95 off the camera's
// origin 0x166dc0, and the draw callback 0x2c7df8 is queued (not in mode 2 with a platform hit).
// 1 on a hit, else 0.
// orig: 0x002c8440
int32_t doom_agent_step(uint32_t pv, uint32_t m, int32_t mode) {
    if (lw(P + 0x2084) == 0x72) return 0;
    if (lw(OV(0x0015f5c4)) != 0) return 0;
    StackFrame fr(0x110);
    const uint32_t sp = fr.base;
    const uint32_t rec = OV(0x00173e40);
    const uint32_t s4 = sp + 0x20, s6 = sp + 0x10;
    int32_t s8 = 0;
    int32_t v0 = 0, tick = 0, s0 = 0;
    uint32_t hit = 0, hero = 0;
    int32_t sv = 0;
    V(s4) = V(m + 0x10);
    if (lbu(m + 0x20) == 1) {
        const float f20 = u2f(0xbeb953dfu);
        const float f21 = u2f(0x3f5be9fbu);
        float b = angle_add(f20, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
        swc1(sp, cos_vu(b) * f21);
        b = angle_add(f20, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
        swc1(sp + 4, sin_vu(b) * f21);
        sw(sp + 8, 0);
        vec3_add(V(sp), V(sp), V(P + 0x80));
        swc1(sp + 8, lwc1(sp + 8) + u2f(0x3ec8198fu));
        V(s6) = V(pv);
        if (lw(P + 0x208c) == 0xf) vec3_add(V(s6), V(s6), V(P + 0x100));
        if (lwu(P + 0x2fc) != 0 && moby_pvar_word2(lwu(P + 0x2fc)) != 0 && mode != 0) vec3_add(V(s6), V(s6), V(P + 0x100));
        sw(sp + 0x50, static_cast<uint32_t>(frame_scale_i(0x12c)));
    } else {
        const uint32_t hpv = lwu(lwu(pv + 0x30) + 0x78);
        V(sp) = V(s4);
        V(s6) = V(pv);
        const int32_t t = lh(pv + 0x38);
        if (lwu(hpv + 0x50) != 0) sh(pv + 0x3a, 1);
        sw(sp + 0x50, static_cast<uint32_t>(t));
    }
    {
        const float f1 = lwc1(0x0015ed6c) * 0.5f;
        const float f2 = lwc1(sp + 8);
        hero = lwu(P + 0x2080);
        swc1(sp + 0x18, lwc1(sp + 0x18) - f1);
        V(sp + 0x30) = V(hero + 0x10);
        swc1(sp + 0x38, f2);
        if (segment_collide(sp + 0x30, sp, 0x10, hero, 0) == 0) goto L88cc;
    }
    // the first ray hit
    sv = sub_001f35d8();
    if (sv == 0 && !(0.0f < lwc1(sp + 0x18))) goto L88cc;
    hit = lwu(rec + 0x18);
    if (hit == 0) {
        if (!(lw(rec + 0x1c) > 0)) goto L88cc;
        V(pv + 0x10) = V(OV(0x00173e60));
        s8 = 1;
        goto L88cc;
    }
    if (lw(rec + 0x1c) > 0) {
        V(pv + 0x10) = V(OV(0x00173e60));
        s8 = 1;
        goto L88cc;
    }
    if (hit == lwu(P + 0x2080) || hit == lwu(pv + 0x30)) {
        s0 = frame_scale_i(0x12c) - frame_scale_i(0xa);
        if (!(lw(sp + 0x50) < s0)) goto L88cc;
    }
    hit = lwu(rec + 0x18);
    V(pv + 0x10) = V(hit + 0x10);
    sw(sp + 0x40, 0);
    sw(sp + 0x44, 0);
    sw(sp + 0x48, 0);
    sw(sp + 0x4c, 0);
    swc1(sp + 0x48, 0.5f);
    s8 = 1;
    vec3_add(V(sp + 0x40), V(sp + 0x40), V(pv + 0x10));
    (void)floor_probe(0.5f, sp + 0x40, 0);
    goto L88cc;
L8760:
    V(s4) = V(sp);
    for (s0 = 9; s0 >= 0;) {
        vec3_add(V(sp), V(sp), V(s6));
        --s0;
        swc1(sp + 0x18, lwc1(sp + 0x18) - lwc1(0x0015ed70) * u2f(0x41100000u));
    }
    if (segment_collide(s4, sp, 0x10, m, 0) == 0) goto L88cc;
    sv = sub_001f35d8();
    if (sv == 0 && 0.0f < lwc1(sp + 0x18)) goto L88cc;
    hit = lwu(rec + 0x18);
    if (hit == 0) {
        if (!(lw(rec + 0x1c) > 0)) goto L88cc;
        V(pv + 0x10) = V(OV(0x00173e60));
        v0 = lw(sp + 0x50);
        goto L88e4;
    }
    if (lw(rec + 0x1c) > 0) {
        V(pv + 0x10) = V(OV(0x00173e60));
        v0 = lw(sp + 0x50);
        goto L88e4;
    }
    if (hit == lwu(P + 0x2080) || hit == lwu(pv + 0x30)) {
        s0 = frame_scale_i(0x12c) - frame_scale_i(0xa);
        if (!(lw(sp + 0x50) < s0)) goto L88cc;
    }
    hit = lwu(rec + 0x18);
    V(pv + 0x10) = V(hit + 0x10);
    sw(sp + 0x40, 0);
    sw(sp + 0x44, 0);
    sw(sp + 0x48, 0);
    sw(sp + 0x4c, 0);
    swc1(sp + 0x48, 0.5f);
    vec3_add(V(sp + 0x40), V(sp + 0x40), V(pv + 0x10));
    (void)floor_probe(0.5f, sp + 0x40, 0);
    v0 = lw(sp + 0x50);
    goto L88e4;
L88cc:
    tick = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(sp + 0x50)));
    v0 = lw(sp + 0x50);
    if (tick == 0 && s8 == 0) goto L8760;
L88e4:
    if (v0 == 0) return 0;
    {
        bool draw = true;
        if (mode == 0) {
            const uint32_t a0 = lwu(OV(0x00173e58));
            if (a0 == 0) return 0;
            if (moby_pvar_word2(a0) == 0) return 0;
        } else if (mode == 2) {
            const uint32_t a0 = lwu(OV(0x00173e58));
            if (a0 != 0 && moby_pvar_word2(a0) != 0) draw = false;
        }
        if (draw) sub_001f93e0(OVF(0x002c7df8), m);
    }
    V(pv + 0x20) = V(OV(0x00173e80));
    vec3_sub(V(s4), V(pv + 0x10), V(OV(0x00166dc0)));
    vec3_scale(V(s4), V(s4), u2f(0x3f733333u));
    vec3_add(V(pv + 0x10), V(s4), V(OV(0x00166dc0)));
    return 1;
}

// The agent's update: see the file's head. It is taken out (0x24eb68) when it leaves the box
// 2 .. 1021 on an axis, when the glove is not the equipped weapon any more (state 1) or at the end of
// its fade (state 4).
// orig: 0x002c89e8
void doom_agent_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    StackFrame fr(0x100);
    const uint32_t sp = fr.base;
    for (uint32_t o = 0x10; o <= 0x18; o += 4) {
        const float f0 = lwc1(m + o);
        if (f0 < 2.0f || u2f(0x447f4000u) < f0) goto L9284;
    }
    {
        uint32_t fl = lhu(m + 0x34);
        if (lbu(P + 0x20a5) != 0 && lbu(m + 0x20) < 2) fl |= 0x41u;
        else fl &= 0xffbeu;
        sh(m + 0x34, fl);
    }
    if (lh(lwu(pv + 0x30) + 0xa6) == 0xe5 && lh(pv + 0x3a) == 0) {
        if (0.0f < vec3_length(V(pv))) {
            const uint32_t a0 = lwu(P + 0x2fc);
            if (a0 == 0 || moby_pvar_word2(a0) == 0) {
                (void)doom_agent_step(pv, m, 1);
            } else if (doom_agent_step(pv, m, 0) == 0) {
                (void)doom_agent_step(pv, m, 2);
            }
        }
    }
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(4)));
        sw(pv + 0x3c, 0);
        sh(pv + 0x38, 0);
        return;
    case 1: {
        sh(pv + 0x38, 0);
        const float f2 = lwc1(lwu(m + 0x24) + 0x24);
        const float f3 = lwc1(m + 0x2c);
        if (f3 < f2 * u2f(0x3e800000u)) swc1(m + 0x2c, f3 + f2 * (lwc1(0x0015ed60) * u2f(0x3d4ccccdu)));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x34))) != 0) {
            sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(4)));
            if (lbu(P + 0x20a5) == 0) (void)glow_particle_20(m);
        }
        if (lbu(P + 0x20a4) != 0) goto L9284;
        const uint32_t hand = lwu(pv + 0x30);
        if (hand == 0) goto L9284;
        if ((lbu(hand + 0x20) & 0xf0u) == 0xf0u) goto L9284;
        const uint32_t cur = lwu(P + 0x1090);
        if (cur == 0) goto L9284;
        if (lh(cur + 0xa6) == 0xe5) return;
        goto L9284;
    }
    case 2: {
        const float f22 = 0.5f;
        const float f21 = int_to_float(lbu(0x0013e534)) + 1.0f;
        const float f2 = lwc1(lwu(m + 0x24) + 0x24);
        const float f3 = lwc1(m + 0x2c);
        if (f3 < (f2 * f22) * f21) swc1(m + 0x2c, f3 + ((f2 * (lwc1(0x0015ed60) * u2f(0x3d4ccccdu))) * f21));
        const int32_t t = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x34)));
        float f1 = lwc1(0x0015ed70);
        if (t != 0) {
            sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(4)));
            (void)glow_particle_20(m);
            f1 = lwc1(0x0015ed70);
        }
        const uint32_t s1 = m + 0x10;
        swc1(pv + 8, lwc1(pv + 8) - f1 * u2f(0x41100000u));
        vec3_add(V(sp), V(s1), V(pv));
        const uint32_t s0 = sp + 0x30;
        vec3_add_inplace(V(s1), V(0x0013f490));
        const uint32_t s5 = sp + 0x10, s4 = sp + 0x20;
        V(s5) = V(s1);
        const float f23 = u2f(0x3e19999au);
        const float k = (lwc1(0x0015ed60) * f23) * lwc1(m + 0x2c);
        swc1(sp + 0x18, lwc1(sp + 0x18) - fpu_div(k, lwc1(lwu(m + 0x24) + 0x24)));
        V(s4) = V(sp);
        swc1(sp + 0x28, lwc1(sp + 0x28) - fpu_div(k, lwc1(lwu(m + 0x24) + 0x24)));
        V(s0) = V(sp);
        const float f20 = f21 * u2f(0x3e4ccccdu);
        swc1(sp + 0x38, lwc1(sp + 0x38) + f20);
        if (sphere_collide(s0, f21 * u2f(0x3e99999au), 4, lwu(P + 0x2080)) != 0) {
            V(s1) = V(OV(0x00173e70));
            swc1(m + 0x18, lwc1(m + 0x18) - f20);
        } else {
            if (segment_collide(s5, s4, 0, lwu(P + 0x2080), 0) == 0) {
                V(s1) = V(sp);
                return;
            }
            V(s1) = V(OV(0x00173e60));
            const float kk = (lwc1(0x0015ed60) * f23) * lwc1(m + 0x2c);
            swc1(m + 0x18, lwc1(m + 0x18) + fpu_div(kk, lwc1(lwu(m + 0x24) + 0x24)));
        }
        const float f21b = u2f(0x3c23d70au);
        vec3_reflect(V(pv), V(pv), V(OV(0x00173e80)));
        vec3_scale(V(pv), V(pv), f22);
        const uint32_t rec = OV(0x00173e40);
        bool count = false;
        if (vec3_length(V(pv)) < lwc1(0x0015ed60) * f21b) {
            count = true;
        } else {
            const uint32_t a0 = lwu(P + 0x2fc);
            if (a0 != 0 && moby_pvar_word2(a0) != 0) {
                const float l1 = vec3_length(V(pv));
                const float l2 = vec3_length(V(0x0013f450));
                if (l1 < lwc1(0x0015ed60) * f21b + l2) count = true;
            }
        }
        if (count) {
            const uint32_t n = lhu(pv + 0x38) + 1u;
            sh(pv + 0x38, n);
            bool stop = true;  // (to state 3)
            if (lwu(rec + 0x18) != 0 && !(lw(rec + 0x1c) > 0)) stop = frame_scale_i(0xf0) < static_cast<int16_t>(n);
            if (stop) {
                sb(m + 0x20, 3);
                sh(pv + 0x38, 0);
            }
        }
        {
            const uint32_t a0 = lwu(P + 0x2fc);
            if (a0 != 0 && moby_pvar_word2(a0) != 0) vec3_add(V(pv), V(pv), V(P + 0x100));
        }
        vec3_add(V(s1), V(s1), V(pv));
        doom_agent_splash(m);
        return;
    }
    case 3: {
        const uint32_t s0 = m + 0x10;
        if (floor_probe(0.5f, s0, 0) != 0.0f) {
            const uint32_t a1 = lwu(OV(0x00173e58));
            if (a1 != 0) (void)sub_00260400(m, a1, s0, m + 0x40, s0, m + 0x40);
        }
        int32_t v0;
        if (timer_tick_i16(ee_ref<int16_t>(pv + 0x38)) != 0) {
            int32_t n = 0;
            for (uint32_t v = lwu(OV(0x0015ffe4)); v != 0; v = lwu(v + 0x28)) {
                if (lh(v + 0xa6) == 0xba && (lbu(v + 0x20) & 0xf0u) != 0xf0u) ++n;
            }
            if (n < 8) {
                const float f21 = u2f(0x40c90fdbu), f22 = u2f(0x3e800000u), f20 = u2f(0x3d4ccccdu);
                const float cs = cos_vu((int_to_float(lw(pv + 0x3c)) * f21) * f22);
                swc1(sp + 0x10, (lwc1(0x0015ed60) * f20) * cs);
                const float sn = sin_vu((int_to_float(lw(pv + 0x3c)) * f21) * f22);
                const float f1 = lwc1(0x0015ed60);
                const float f20b = f1 * f20;
                swc1(sp + 0x18, f1 * u2f(0x3ca3d70au));
                swc1(sp + 0x14, f20b * sn);
                const uint32_t a0 = lwu(P + 0x2fc);
                if (a0 != 0 && moby_pvar_word2(a0) != 0) vec3_add(V(sp + 0x10), V(sp + 0x10), V(P + 0x100));
                vec3_add(V(sp), V(sp + 0x10), V(s0));
                (void)doom_spawn_agent(lwu(pv + 0x30), sp, sp + 0x10);
                sh(pv + 0x38, static_cast<uint32_t>(frame_scale_i(8)));
                const int32_t c = lw(pv + 0x3c) + 1;
                sw(pv + 0x3c, static_cast<uint32_t>(c));
                if (!(c < 4)) sb(m + 0x20, 4);
            } else {
                sh(pv + 0x38, static_cast<uint32_t>(frame_scale_i(0xf)));
            }
            v0 = lw(pv + 0x3c);
        } else {
            v0 = lw(pv + 0x3c);
        }
        if (!(v0 < 2)) return;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x34))) == 0) return;
        sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(4)));
        (void)glow_particle_20(m);
        return;
    }
    case 4: {
        const uint32_t s0 = m + 0x10;
        swc1(m + 0x2c, lwc1(m + 0x2c) * u2f(0x3f6b851fu));
        sb(m + 0x23, lbu(m + 0x23) - 4u);
        if (floor_probe(0.5f, s0, 0) != 0.0f) {
            const uint32_t a1 = lwu(OV(0x00173e58));
            if (a1 != 0) (void)sub_00260400(m, a1, s0, m + 0x40, s0, m + 0x40);
        }
        if (lbu(m + 0x23) != 0) return;
        goto L9284;
    }
    default:
        sb(m + 0x20, 0);
        return;
    }
L9284:
    moby_retire(m);
}

// orig: (none) — the agent's update for the class's indirect call.
void doom_agent_fns_register() {
    ovl_register_moby_fn(0x002c89e8, doom_agent_update);
    ovl_register_moby_fn(0x002c7df8, doom_agent_draw);
}

}  // namespace rac1
