// Ported overlay code: the Mine Glove's mine (moby class 0x4a, spawned by 0x2a9ed0): held in the
// glove (state 0: its matrix the hand's chain point turned by pi), thrown (state 1: flying,
// bouncing, rolling), armed on the ground (state 2 / 4 flags), set off by what comes near and
// exploding. Its pvars: +0 the velocity, +0x10 the aim point the glove shows (0x2aa2a8), +0x20 its
// normal, +0x30 the hand moby, +0x34 / +0x36 the arc's frames left / flag, +0x40..0x4c the
// armed spin / target, +0x50 in water, +0x54 the thing it hit, +0x58 the fuse, +0x5c its stamp
// (-1: the glove's eighth-oldest mine, to go), +0x60..0x70 roll / tilt, +0x74 the frame it landed.
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
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

// The impact point a thrown mine will have, simulated from the glove's aim (mode 0 / 1 / 2: with the
// hero's carried velocity added for 1 and 2, the target kind): into pvars +0x10 (its normal +0x20),
// the aim marker's draw callback 0x2a9c50 registered; 1 when a point was found.
// orig: 0x002aa2a8
int32_t mine_predict(uint32_t pv, uint32_t mine, int32_t mode) {
    if (lw(P + 0x2084) == 0x72) return 0;
    if (lw(OV(0x0015f5c4)) != 0) return 0;
    StackFrame fr(0xe0);
    const uint32_t sp = fr.base;
    V(sp + 0x20) = V(mine + 0x10);
    int32_t life;
    if (lbu(mine + 0x20) == 0) {
        const float x = cos_vu(angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * u2f(0x3f5be9fbu);
        const float y = sin_vu(angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * u2f(0x3f5be9fbu);
        swc1(sp, x);
        sw(sp + 8, 0);
        swc1(sp + 4, y);
        vec3_add(V(sp), V(sp), V(P + 0x80));
        swc1(sp + 8, lwc1(sp + 8) + u2f(0x3efb4cc2u));
        V(sp + 0x10) = V(pv);
        if (lwu(P + 0x2fc) != 0 && moby_pvar_word2(lwu(P + 0x2fc)) != 0 && mode != 0) vec3_add(V(sp + 0x10), V(sp + 0x10), V(P + 0x100));
        life = frame_scale_i(0x78);
    } else {
        const uint32_t hpv = lwu(lwu(pv + 0x30) + 0x78);
        V(sp) = V(mine + 0x10);
        V(sp + 0x10) = V(pv);
        life = lh(pv + 0x34);
        if (lwu(hpv + 0x50) != 0) sh(pv + 0x36, 1);
    }
    sw(sp + 0x40, static_cast<uint32_t>(life));
    const float z0 = lwc1(sp + 8);
    V(sp + 0x30) = V(lwu(P + 0x2080) + 0x10);
    swc1(sp + 0x38, z0);
    if (segment_collide(sp + 0x30, sp, 0x10, lwu(P + 0x2080), 0) != 0) {
        if (sub_001f35d8() != 0 && lw(OV(0x00173e40) + 0x1c) > 0) V(pv + 0x10) = V(OV(0x00173e60));
    }
    for (;;) {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(sp + 0x40))) != 0) break;
        V(sp + 0x20) = V(sp);
        for (int32_t s0 = 9;;) {
            vec3_add(V(sp), V(sp), V(sp + 0x10));
            --s0;
            swc1(sp + 0x18, lwc1(sp + 0x18) - lwc1(0x0015ed70) * u2f(0x411ccccdu));
            if (s0 >= 0) continue;
            break;
        }
        if (segment_collide(sp + 0x20, sp, 0x12, mine, 0) == 0) continue;
        if (sub_001f35d8() == 0 && 0.0f < lwc1(sp + 0x18)) continue;
        if (lw(OV(0x00173e40) + 0x1c) > 0) {
            V(pv + 0x10) = V(OV(0x00173e60));
            break;
        }
    }
    if (lw(sp + 0x40) == 0) return 0;
    if (mode == 0) {
        const uint32_t hit = lwu(OV(0x00173e58));
        if (hit == 0) return 0;
        if (moby_pvar_word2(hit) == 0) return 0;
    }
    bool call = true;
    if (mode == 2) {
        const uint32_t hit = lwu(OV(0x00173e58));
        if (hit != 0 && moby_pvar_word2(hit) != 0) call = false;
    }
    if (call) sub_001f93e0(OVF(0x002a9c50), mine);
    V(pv + 0x20) = V(OV(0x00173e80));
    vec3_sub(V(sp + 0x20), V(pv + 0x10), V(OV(0x00166dc0)));
    vec3_scale(V(sp + 0x20), V(sp + 0x20), u2f(0x3f733333u));
    vec3_add(V(pv + 0x10), V(sp + 0x20), V(OV(0x00166dc0)));
    return 1;
}

// The mine's frame (see the file's head). Control flow follows the original's blocks (the labels are
// its addresses): the flags and the aim (0x2aa7e8), the mine held in the hand (state 0), the thrown
// mine's flight / water splash / bounce / tilt (0x2aa924..0x2ab258), the stuck mine's (state bit 4)
// follow of what it is on and its roll (0x2ab268..0x2ab7e4), the fuse and what sets it off
// (0x2ab7e4..0x2abba0), then the blast (0x2abba0..0x2ac8c0): air burst, water burst or ground burst,
// camera shake, sound and effect, the mine retired.
// orig: 0x002aa670
void mine_update(uint32_t m) {
    StackFrame fr(0x280);
    const uint32_t sp = fr.base;
    const uint32_t pos = m + 0x10;  // s5
    const uint32_t oldp = sp + 0xd0;  // s8
    uint32_t pv = 0;
    uint32_t s6 = 0;
    uint32_t s7 = 0, s1 = 0, s4 = 0;
    float f27 = 0.0f;
    float f20 = 0.0f, f22 = 0.0f, f21 = 0.0f;
    uint32_t hitm = 0;
    int32_t life = 0;
    for (uint32_t i = 0; i < 0x50; i += 4) sw(sp + 0x20 + i, lwu(OV(0x001e9830) + i));
    for (uint32_t i = 0; i < 0x50; i += 4) sw(sp + 0x70 + i, lwu(OV(0x001e9880) + i));
    {
        const float gs2 = lwc1(0x0015ed6c) + lwc1(0x0015ed6c);
        const Vec4 z0{};
        V(sp + 0xd0) = z0;
        V(sp + 0xe0) = z0;
        swc1(sp + 0xd8, gs2);
        swc1(sp + 0xe8, 1.0f);
        V(sp + 0xc0) = V(sp + 0xd0);
    }
    pv = lwu(m + 0x78);
    {
        uint32_t fl = lhu(m + 0x34);
        if ((lbu(P + 0x20a5) != 0 || lbu(P + 0x20af) != 0) && lbu(m + 0x20) == 0) fl |= 0x41;
        else fl &= 0xffbe;
        sh(m + 0x34, fl);
    }
    {
        const uint32_t item = lwu(P + 0x1090);
        if (!(item != 0 && lh(item + 0xa6) == 0xbe)) {
            if (lbu(m + 0x20) == 0) goto L_retire;
        }
        if (lbu(P + 0x20a4) != 0) {
            if (lbu(m + 0x20) == 0) goto L_retire;
        }
    }
    {
        const uint32_t hand = lwu(pv + 0x30);
        if (hand != 0 && lbu(hand + 0x20) != 0xfe && lbu(hand + 0x20) != 0xfd && lh(hand + 0xa6) == 0xbe && lh(pv + 0x36) == 0) {
            if (0.0f < vec3_length(V(pv))) {
                if (lwu(P + 0x2fc) == 0 || moby_pvar_word2(lwu(P + 0x2fc)) == 0) {
                    (void)mine_predict(pv, m, 1);
                } else if (mine_predict(pv, m, 0) == 0) {
                    (void)mine_predict(pv, m, 2);
                }
            }
        }
    }
    if (lbu(m + 0x20) == 0) {
        // the mine in the hand: its matrix the hand's chain point turned by pi
        const uint32_t item = lwu(P + 0x1090);
        if (item == 0) goto L_retire;
        if (lh(item + 0xa6) != 0xbe) goto L_retire;
        Vec4 ang{};
        ang.x = u2f(0x40490fdbu);
        euler_to_mat4_vu(M(sp + 0xf0), ang);
        moby_chain_world_one_v(lwu(pv + 0x30), 0, M(sp + 0x130));
        const Mat4 b = M(sp + 0xf0);
        mat4_mul(M(sp + 0xf0), M(sp + 0x130), b);
        mat3_copy(M(m + 0xc0), M(sp + 0xf0));
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 0x100u);
        return;
    }
    sh(m + 0x34, lhu(m + 0x34) & 0xfeffu);
    V(oldp) = V(pos);
    if ((lbu(m + 0x20) & 4) == 0) {
        vec3_add(V(pos), V(pos), V(pv));
        if (lh(pv + 0x50) == 0) {
            swc1(pv + 8, lwc1(pv + 8) - lwc1(0x0015ed70) * u2f(0x411ccccdu));
        } else {
            const float v = fpu_div(lwc1(0x0015ed70) * u2f(0x411ccccdu), u2f(0x41200000u));
            swc1(pv + 8, lwc1(pv + 8) - v);
            const int32_t fs6 = frame_scale_i(6);
            if (rand_mod(fs6 - 1) == 0) {
                f20 = surface_height(pos, sp + 0xf0);
                vec3_scale(V(sp + 0xf0), V(pv), u2f(0x3e800000u));
                const float r = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccdu));
                (void)spawn_particle_22(pos, sp + 0xf0, r * u2f(0x484d1400u), f20);
            }
        }
    }
    // 2aaa30
    V(sp + 0xf0) = V(pv);
    vec2_set_length(V(sp + 0xf0), V(sp + 0xf0), 1.0f);
    sw(sp + 0x104, 0x830000u);
    swc1(sp + 0x10c, 3.0f);
    sw(sp + 0x100, m);
    sw(sp + 0x110, 1);
    sb(sp + 0x108, 2);
    swc1(sp + 0xfc, u2f(0x45afdf66u));
    sb(sp + 0x109, 1);
    sh(sp + 0x10a, lhu(m + 0xa6));
    swc1(m + 0x18, lwc1(m + 0x18) - u2f(0x3ecccccdu));
    swc1(sp + 0xf8, 1.0f);
    f20 = int_to_float(lw(OV(0x0015f5cc)) - lw(pv + 0x74));
    f20 = fpu_div(f20, frame_scale_f(lwc1(OV(0x001613d4))));
    if (1.0f < f20) f20 = 1.0f;
    else if (f20 < 0.0f) f20 = 0.0f;
    {
        const float s = sin_vu(lwc1(pv + 0x70));
        const float f1 = (lwc1(pv + 0x6c) * s) * f20;
        swc1(m + 0x18, lwc1(m + 0x18) - f1);
    }
    if ((lbu(m + 0x20) & 4) != 0) goto L_ab268;
    // 2aab30: did it hit anything on this step?
    if (segment_collide(oldp, pos, 0x10, m, sp + 0xf0) == 0) {
        if ((lbu(m + 0x20) & 4) == 0) goto L_ab7e8;
        goto L_ab268;
    }
    if (lh(pv + 0x50) != 0) goto L_aacec;
    if (sub_001f35d8() != 0) goto L_aacd4;
    if (!(lwc1(pv + 8) < 0.0f)) goto L_aacd0;
    {
        // into the water: a splash in the height field, the drops, the ripple moby 0x307
        region_height_splash(lwu(OV(0x001612d0)), lw(OV(0x001612d8)), 1, lwc1(m + 0x10), lwc1(m + 0x14), 0.5f, u2f(0xbeb33333u));
        sh(pv + 0x50, 1);
        vec_clear(V(pv));
        swc1(pv + 8, lwc1(0x0015ed6c) * u2f(0xbfc00000u));
        swc1(sp + 0x120, lwc1(m + 0x10));
        swc1(sp + 0x124, lwc1(m + 0x14));
        swc1(sp + 0x128, lwc1(OV(0x00173e68)));
        const uint32_t r307 = spawn_moby_307(sp + 0x120, 2.0f);
        if (r307 != 0) sb(r307 + 0x23, 0x70);
        f20 = 0.0f;
        const float f24 = 0.0f;
        for (int32_t k = 15; k >= 0;) {
            const float yaw = rand_angle();
            --k;
            const float gs = lwc1(0x0015ed6c);
            const float r = rand_range_f(gs * f24, gs * u2f(0x40400000u));
            swc1(sp + 0x130, cos_vu(yaw) * r);
            swc1(sp + 0x134, sin_vu(yaw) * r);
            const float gs2 = lwc1(0x0015ed6c);
            swc1(sp + 0x138, rand_range_f(gs2 * u2f(0x40400000u), gs2 * u2f(0x40d00000u)));
            const int32_t a = rand_range(0x5a, 0x78);
            const int32_t kind = rand_mod(2);
            spawn_particle_23(sp + 0x120, sp + 0x130, kind, static_cast<uint32_t>(a));
        }
        goto L_ab7e8;
    }
L_aacd0:
L_aacd4:
    if (lh(pv + 0x50) != 0) goto L_aacec;
    if (sub_001f35d8() == 0) goto L_ab7e4;
L_aacec:
    hitm = lwu(OV(0x00173e58));
    if (hitm == 0) goto L_aaed0;
    if (moby_in_range_class_500(hitm) == 0) {
        if ((lhu(lwu(OV(0x00173e58)) + 0x34) & 0x5000) == 0) goto L_aad88;
    }
    hitm = lwu(OV(0x00173e58));
    if (lh(hitm + 0xa6) == lh(m + 0xa6)) goto L_aad88;
    if (hitm == lwu(P + 0x2080)) goto L_aad88;
    if (hitm == lwu(pv + 0x30)) goto L_aad88;
    sb(m + 0xbc, 1);
    if (lbu(m + 0x20) == 1) goto L_ab7e4;
    V(pos) = V(OV(0x00173e60));
    swc1(m + 0x18, floor_probe_v(0.5f, V(pos), 0));
    goto L_ab7e4;
L_aad88:
    hitm = lwu(OV(0x00173e58));
    if (hitm == 0) goto L_aaed4;
    if (lh(hitm + 0xa6) != lh(m + 0xa6)) goto L_aaeb8;
    // a mine against a mine: pushed apart
    vec3_sub(V(OV(0x00173e80)), V(pos), V(hitm + 0x10));
    if (lwc1(pv + 8) < lwc1(0x0015ed6c) * u2f(0xc11ccccdu)) {
        sb(m + 0xbc, 1);
        V(pos) = V(OV(0x00173e60));
        vec3_reflect(V(sp + 0xc0), V(pv), V(OV(0x00173e80)));
        vec3_set_length(V(sp + 0xc0), V(sp + 0xc0), lwc1(0x0015ed6c) + lwc1(0x0015ed6c));
        vec3_set_length(V(sp + 0xe0), V(OV(0x00173e80)), 1.0f);
        goto L_ab7e8;
    }
    V(pos) = V(OV(0x00173e60));
    vec3_set_length(V(sp + 0x120), V(OV(0x00173e80)), lwc1(0x0015ed70) * u2f(0x411b3333u));
    vec3_add(V(pos), V(pos), V(sp + 0x120));
    vec3_reflect(V(pv), V(pv), V(OV(0x00173e80)));
    vec3_scale(V(pv), V(pv), u2f(0x3ecccccdu));
    vec3_set_length(V(sp + 0x120), V(OV(0x00173e80)), 1.0f);
    (void)vec3_dot(V(sp + 0x120), V(sp + 0xe0));
    V(sp + 0x130) = V(OV(0x00173e80));
    goto L_ab198;
L_aaeb8:
    if (hitm == lwu(P + 0x2080)) goto L_aaed4;
    if (hitm != lwu(pv + 0x30)) goto L_aaee4;
L_aaed0:
L_aaed4:
    if (lw(OV(0x00173e5c)) <= 0) goto L_ab7e8;
L_aaee4:
    if (lh(pv + 0x50) != 0) goto L_aaf24;
    if (sub_001f35d8() == 0) goto L_aaf20;
    if (!(lwc1(pv + 8) < lwc1(0x0015ed6c) * u2f(0xc11ccccdu))) goto L_aaf88;
L_aaf20:
L_aaf24:
    sb(m + 0xbc, 1);
    V(pos) = V(OV(0x00173e60));
    vec3_reflect(V(sp + 0xc0), V(pv), V(OV(0x00173e80)));
    vec3_set_length(V(sp + 0xc0), V(sp + 0xc0), lwc1(0x0015ed6c) + lwc1(0x0015ed6c));
    vec3_set_length(V(sp + 0xe0), V(OV(0x00173e80)), 1.0f);
    goto L_ab7e8;
L_aaf88:
    if (lwu(OV(0x00173e58)) != 0) s6 = moby_pvar_word2(lwu(OV(0x00173e58)));
    V(pos) = V(OV(0x00173e60));
    vec3_set_length(V(sp + 0x120), V(OV(0x00173e80)), lwc1(0x0015ed70) * u2f(0x411b3333u));
    f20 = 0.0f;
    vec3_add(V(pos), V(pos), V(sp + 0x120));
    V(sp + 0x150) = V(pv);
    vec3_reflect(V(pv), V(pv), V(OV(0x00173e80)));
    vec3_scale(V(pv), V(pv), u2f(0x3f19999au));
    swc1(pv + 8, lwc1(pv + 8) * 0.5f);
    if (lwc1(pv + 0x6c) == f20) {
        swc1(pv + 0x6c, abs_f(lwc1(sp + 0x158)) + abs_f(lwc1(sp + 0x158)));
        float t = u2f(0xc016cbe4u);
        if (f20 < lwc1(sp + 0x158)) t = u2f(0x3f490fdbu);
        swc1(pv + 0x70, t);
        sw(pv + 0x74, lwu(OV(0x0015f5cc)));
    }
    vec3_set_length(V(sp + 0x120), V(OV(0x00173e80)), 1.0f);
    if (vec3_dot(V(sp + 0x120), V(sp + 0xe0)) > u2f(0x3f34fdf4u) && lwc1(sp + 0x158) < 0.0f) {
        const float fl = floor_probe_v(0.5f, V(pos), 0);
        if (lwc1(m + 0x18) < fl) swc1(m + 0x18, fl);
        swc1(pv + 8, -lwc1(pv + 8));
        const float len = vec3_length(V(pv));
        const float lim = lwc1(0x0015ed6c) * 0.5f;
        if (len < lim || s6 != 0) {
            vec_clear(V(pv));
            sb(m + 0x20, lbu(m + 0x20) | 4);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            const uint32_t hm = lwu(OV(0x00173e58));
            if (hm != 0 && s6 != 0) sw(pv + 0x4c, hm);
        }
    }
    V(sp + 0x130) = V(OV(0x00173e80));
L_ab198:
    // tilt toward the surface: the normal (sp130) in the mine's frame (yaw m+0x48)
    do {
        const float c1 = cos_vu(lwc1(m + 0x48));
        const float s1v = sin_vu(lwc1(m + 0x48));
        swc1(sp + 0x140, lwc1(sp + 0x130) * c1 + lwc1(sp + 0x134) * s1v);
        const float c2 = cos_vu(lwc1(m + 0x48));
        const float s2v = sin_vu(lwc1(m + 0x48));
        const float y2 = lwc1(sp + 0x134) * c2 - lwc1(sp + 0x130) * s2v;
        swc1(sp + 0x144, y2);
        swc1(sp + 0x148, lwc1(sp + 0x138));
        const float x2 = lwc1(sp + 0x140);
        const float z2 = lwc1(sp + 0x138);
        const float len = sqrt_f(x2 * x2 + z2 * z2);
        const float a = atan2_f(len, lwc1(sp + 0x144));
        swc1(m + 0x40, angle_seek_limited(lwc1(m + 0x40), -a, u2f(0x3d567750u)));
        const float b = atan2_f(lwc1(sp + 0x148), lwc1(sp + 0x140));
        swc1(m + 0x44, angle_seek_limited(lwc1(m + 0x44), b, u2f(0x3d567750u)));
    } while (false);
    goto L_ab7e4;
L_ab268:
    // the mine stuck on something (state bit 4)
    f22 = floor_probe_v(0.5f, V(pos), 0);
    {
        const float f3 = f22;
        if (lwu(pv + 0x4c) != 0) {
            const uint32_t on = lwu(pv + 0x4c);
            const uint32_t st = lbu(on + 0x20);
            if (st == 0xfe || st == 0xfd) {
                sb(m + 0xbc, 1);
                sw(pv + 0x4c, 0);
            } else if (lwu(OV(0x00173e58)) != 0) {
                s6 = moby_pvar_word2(lwu(OV(0x00173e58)));
                if (s6 == 0) goto L_ab2f0;
                (void)sub_00260400(m, lwu(pv + 0x4c), pos, m + 0x40, pos, m + 0x40);
            }
            if (s6 != 0) goto L_ab42c;
        L_ab2f0:
            sb(m + 0x20, 1);
            if (lw(pv + 0x68) != -1) {
                const uint32_t e = 0x0013e550u + static_cast<uint32_t>(lw(pv + 0x68)) * 0x70u;
                if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(lw(pv + 0x68));
            }
            sw(pv + 8, 0);
            sw(pv + 0x68, 0xffffffffu);
            sh(pv + 0x34, static_cast<uint32_t>(frame_scale_i(0x78)));
            sw(pv + 0x4c, 0);
            goto L_ab428;
        }
        // 2ab358: not on anything yet
        if ((lbu(m + 0x20) & 0x10) != 0) goto L_ab42c;
        if (lwc1(m + 0x18) - f3 < lwc1(0x0015ed70) * u2f(0x42200000u)) {
            swc1(m + 0x18, f3);
            sh(pv + 0x34, 0);
            if (lwu(OV(0x00173e58)) == 0) goto L_ab42c;
            if (moby_pvar_word2(lwu(OV(0x00173e58))) == 0) goto L_ab42c;
            sw(pv + 0x4c, lwu(OV(0x00173e58)));
            goto L_ab428;
        }
        sb(m + 0x20, 1);
        if (lw(pv + 0x68) != -1) {
            const uint32_t e = 0x0013e550u + static_cast<uint32_t>(lw(pv + 0x68)) * 0x70u;
            if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(lw(pv + 0x68));
        }
        sw(pv + 8, 0);
        sw(pv + 0x68, 0xffffffffu);
        sh(pv + 0x34, static_cast<uint32_t>(frame_scale_i(0x78)));
    }
L_ab428:
L_ab42c:
    {
        const uint32_t tgt = lwu(pv + 0x54);
        bool skip_to_ab4e8 = false;
        bool check_state = true;
        if (tgt != 0) {
            const uint32_t a0 = lbu(tgt + 0x20);
            if (a0 != 0xfe) {
                if (a0 == 0xfd) {
                    check_state = true;
                } else if ((lhu(tgt + 0x34) & 0x1000) != 0) {
                    skip_to_ab4e8 = true;
                }
            }
        }
        if (!skip_to_ab4e8) {
            if ((lbu(m + 0x20) & 0x10) != 0) {
                sb(m + 0x20, 1);
                if (lw(pv + 0x68) != -1) {
                    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(lw(pv + 0x68)) * 0x70u;
                    if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(lw(pv + 0x68));
                }
                sw(pv + 8, 0);
                sw(pv + 0x54, 0);
                sw(pv + 0x68, 0xffffffffu);
                sh(pv + 0x34, static_cast<uint32_t>(frame_scale_i(0x78)));
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 10);
            }
        }
        (void)check_state;
    }
    if (lwu(pv + 0x54) == 0) goto L_ab790;
    if ((lbu(m + 0x20) & 0x10) == 0) goto L_ab794;
    {
        // armed on its target: it creeps to it, the glow and the hit sphere
        V(sp + 0x130) = V(pos);
        if ((lbu(m + 0x70) & 2) != 0) {
            if (lbu(m + 0x52) == 1) {
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 10);
            }
        }
        if (sub_0028d8c0(m, lw(pv + 0x68)) == 0) sw(pv + 0x68, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        {
            const float f2 = lwc1(0x0015ed70);
            float f1 = lwc1(pv + 0x40);
            f1 = f1 + f2 * u2f(0x41700000u);
            const float f3 = lwc1(0x0015ed6c) * u2f(0x40800000u);
            swc1(pv + 0x40, f1);
            if (f3 < f1) swc1(pv + 0x40, f3);
        }
        if (lh(pv + 0x52) == 1) {
            swc1(m + 0x18, lwc1(m + 0x18) + lwc1(0x0015ed60) * u2f(0x3d4ccccdu));
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x48))) != 0) {
                sw(pv + 0x48, static_cast<uint32_t>(frame_scale_i(7)));
                sh(pv + 0x52, 2);
            }
        } else if (lh(pv + 0x52) == 2) {
            swc1(m + 0x18, lwc1(m + 0x18) - lwc1(0x0015ed60) * u2f(0x3d4ccccdu));
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x48))) != 0) sh(pv + 0x52, 0);
        }
        vec3_sub(V(sp + 0x120), V(lwu(pv + 0x54) + 0x10), V(pos));
        vec3_set_length(V(sp + 0x120), V(sp + 0x120), lwc1(pv + 0x40));
        vec3_add(V(pos), V(pos), V(sp + 0x120));
        sub_00259888(1.0f, sp + 0x140, static_cast<int32_t>(m), 0x10000, sp + 0x120);
        V(sp + 0x170) = V(pos);
        swc1(sp + 0x178, lwc1(sp + 0x178) + u2f(0x3eb33333u));
        if (mobys_in_sphere(sp + 0x170, u2f(0x3eb33333u), 0x10, m, sp + 0x140) != 0) {
            const uint32_t hm = lwu(OV(0x00173e58));
            if (hm != 0) {
                const int32_t c = lh(hm + 0xa6);
                if (c != 0x363 && c != 0x4a && c != 0x458 && c != 0x365 && c != 0x367) sb(m + 0xbc, 1);
            }
        }
        V(sp + 0x170) = V(pos);
        swc1(sp + 0x178, lwc1(sp + 0x178) + u2f(0x3e800000u));
        if (sphere_collide(sp + 0x170, u2f(0x3e800000u), 0, m) != 0) {
            vec3_sub(V(sp + 0x180), V(OV(0x00173e70)), V(sp + 0x170));
            vec3_add(V(pos), V(pos), V(sp + 0x180));
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x58))) != 0) sb(m + 0xbc, 1);
    }
L_ab790:
L_ab794:
    sub_00262b80(m, pv + 0x60, pv + 0x64, u2f(0x3f060a92u), lwc1(0x0015ed6c) * u2f(0x3f060a92u), lwc1(0x0015ed6c) * u2f(0x3e97e9d8u));
    if ((lbu(m + 0x20) & 4) != 0) {
        vec3_sub(V(sp + 0x120), V(pos), V(oldp));
        V(pv) = V(sp + 0x120);
    }
L_ab7e4:
L_ab7e8:
    if ((lbu(m + 0x20) & 2) == 0) {
        if (timer_tick_i16(ee_ref<int16_t>(pv + 0x34)) != 0) {
            sb(m + 0x20, lbu(m + 0x20) | 2);
            sw(pv + 0x54, 0);
        }
    }
    {
        const uint32_t e = sub_0025a420(m, 0x830000u, 0);
        if (e != 0) {
            const uint32_t o = lwu(e + 0x20);
            if (o != 0) {
                if (lh(o + 0xa6) != lh(m + 0xa6)) sb(m + 0xbc, 1);
            }
        }
    }
    sb(m + 0xa4, 0xff);
    if ((lbu(m + 0x20) & 2) == 0) goto L_abba0;
    if ((lbu(m + 0x20) & 4) == 0) {
        sb(m + 0xbc, 1);
        goto L_abba0;
    }
    if (vec2_distance(V(lwu(P + 0x2080) + 0x10), V(pos)) < u2f(0x3f400000u)) {
        const float d = abs_f(lwc1(m + 0x18) - lwc1(P + 0x88));
        if (d < u2f(0x3e800000u)) {
            if ((lbu(m + 0x20) & 0x10) != 0) {
                sb(m + 0xbc, 1);
                goto L_abba0;
            }
        }
    }
    // 2ab8e0
    if (vec3_distance(V(lwu(P + 0x2080) + 0x10), V(pos)) < u2f(0x3f400000u)) {
        // the hero is on it: it is pushed out of him (a walker step)
        vec3_sub(V(sp + 0x140), V(pos), V(lwu(P + 0x2080) + 0x10));
        V(sp + 0x120) = V(sp + 0x140);
        vec3_set_length(V(sp + 0x120), V(sp + 0x120), u2f(0x3f400000u));
        vec3_add(V(sp + 0x140), V(lwu(P + 0x2080) + 0x10), V(sp + 0x120));
        V(sp + 0x130) = V(sp + 0x140);
        (void)walker_step(m, pos, sp + 0x130, 0, u2f(0x3ecccccdu), u2f(0x3e800000u), u2f(0x3e800000u), u2f(0x3f060a92u));
        goto L_abba4;
    }
    // 2ab990: the targets list: the nearest one in reach arms the mine
    s6 = 0;
    f21 = (int_to_float(lbu(0x0013e531)) + 1.0f) * u2f(0x40800000u);
    if (lwu(pv + 0x78) != 0) f21 = f21 * u2f(0x40400000u);
    s7 = 0;
    s1 = lwu(OV(0x001aba00));
    for (;;) {
        if (s1 == 0) break;
        s4 = s1 + 0x10;
        if (u2f(0x41f00000u) < vec3_distance(V(pos), V(s4))) {
            ++s7;
            s1 = lwu(OV(0x001aba00) + s7 * 4);
            continue;
        }
        if (vec3_distance(V(pos), V(s4)) < u2f(0x3fc00000u)) {
            sb(m + 0xbc, 1);
            break;
        }
        if (moby_in_range_class_500(s1) != 0) {
            ++s7;
            s1 = lwu(OV(0x001aba00) + s7 * 4);
            continue;
        }
        f20 = f21;
        {
            const uint32_t w0 = moby_pvar_word0(s1);
            if (w0 != 0) f20 = f21 + int_to_float(lbu(w0 + 0xa)) * u2f(0x3e000000u);
        }
        if (!(vec2_distance(V(pos), V(s4)) < f20)) {
            ++s7;
            s1 = lwu(OV(0x001aba00) + s7 * 4);
            continue;
        }
        if (!(abs_f(lwc1(s1 + 0x18) - lwc1(m + 0x18)) < 2.0f)) {
            ++s7;
            s1 = lwu(OV(0x001aba00) + s7 * 4);
            continue;
        }
        {
            const uint32_t t54 = lwu(pv + 0x54);
            if (t54 != 0) {
                const uint32_t st = lbu(t54 + 0x20);
                if (st != 0xfe && st != 0xfd) {
                    ++s7;
                    s1 = lwu(OV(0x001aba00) + s7 * 4);
                    continue;
                }
            }
        }
        if (s6 != 0) {
            f20 = vec2_distance(V(pos), V(s4));
            if (!(f20 < vec2_distance(V(pos), V(s6 + 0x10)))) {
                ++s7;
                s1 = lwu(OV(0x001aba00) + s7 * 4);
                continue;
            }
        }
        sb(m + 0x20, lbu(m + 0x20) | 0x10);
        s6 = s1;
        moby_anim_set(m, 1, 0, frame_scale_i(3));
        (void)moby_sound_start(0, 0, m);
        sw(pv + 0x40, 0);
        sw(pv + 0x58, static_cast<uint32_t>(frame_scale_i(0x258)));
        sh(pv + 0x52, 1);
        sw(pv + 0x48, static_cast<uint32_t>(frame_scale_i(7)));
        ++s7;
        s1 = lwu(OV(0x001aba00) + s7 * 4);
    }
    if (s6 != 0) sw(pv + 0x54, s6);
L_abba0:
L_abba4:
    life = lw(pv + 0x5c);
    sw(pv + 0x78, 0);
    if (life >= 0) {
        if (lwu(OV(0x001613d0)) == 0) goto L_abbf8;
    }
    // the eighth-oldest mine, or all of them (0x1613d0): a small burst and out
    explosion_burst(m, pos, -1, u2f(0x3ea8f5c3u), u2f(0x41500000u));
    moby_list_remove(m, OV(0x001b07b0));
    moby_retire(m);
L_abbf8:
    {
        const float f2 = lwc1(pv + 0x6c);
        const float f3 = u2f(0x3d4ccccdu);
        if (f3 < f2) {
            float f0 = lwc1(0x0015ed60) * u2f(0xbc449bc0u);
            f0 = f0 * f2;
            f0 = f0 + f2;
            swc1(pv + 0x6c, f0);
            if (f0 < f3) swc1(pv + 0x6c, f3);
        }
    }
    f20 = int_to_float(lw(OV(0x0015f5cc)) - lw(pv + 0x74));
    f20 = fpu_div(f20, frame_scale_f(lwc1(OV(0x001613d4))));
    if (1.0f < f20) f20 = 1.0f;
    else if (f20 < 0.0f) f20 = 0.0f;
    swc1(pv + 0x70, angle_add(lwc1(pv + 0x70), lwc1(0x0015ed6c) * u2f(0x40c90fdbu)));
    do {
        const float s = sin_vu(lwc1(pv + 0x70));
        float f1 = lwc1(pv + 0x6c) * s;
        f1 = f1 * f20;
        float f2 = lwc1(m + 0x18) + f1;
        f2 = f2 + u2f(0x3ecccccdu);
        swc1(m + 0x18, f2);
    } while (false);
    if (lbu(m + 0xbc) == 0) goto L_ac8ac;
    // ---- the blast
    f27 = vec2_distance(V(pos), V(OV(0x00166dc0)));
    if (lh(pv + 0x50) == 0) {
        const float radius = (int_to_float(lbu(0x0013e531)) * 0.5f + 1.0f);
        const int32_t n = mobys_in_sphere(pos, radius + radius, 0x10, m, 0);
        V(sp + 0x120) = V(m + 0x10);
        moby_list_hit_v(m, V(sp + 0x120), OV(0x00177f00), n, 0, 0x810000u, 4, 1, 3.0f, 0.25f, 1.5f);
    }
    {
        const float fl = floor_probe_20_v(V(pos));
        if (lh(pv + 0x50) == 0) {
            if (fl + 2.0f < lwc1(m + 0x18)) {
                // an air burst
                vec_clear(V(sp + 0x120));
                explosion_effect(m, sp + 0x120, 0, 3, 3, 5, 2, 0, 1, -1, lbu(0x0013e531), 0.0f, 0.0f, 2.0f, 1.0f, 4.0f, 1.0f, 7.0f);
                goto L_ac79c;
            }
            // a ground burst: the dust / smoke / fire sprays
            {
                f22 = int_to_float(lbu(0x0013e531)) * 0.5f + 1.0f;
                const int32_t cnt = float_to_int(f27 * u2f(0x41700000u));
                if (cnt <= 0) goto L_ac798;
                int32_t s8 = cnt;
                const float f24 = 0.5f, f23 = 1.0f, f25 = -0.5f;
                do {
                    const int32_t kind = rand_mod(4);
                    const float gs = lwc1(0x0015ed6c);
                    const float f1 = gs * u2f(0x41600000u);
                    const float f2 = gs * u2f(0x41b00000u);
                    const float f0 = gs * u2f(0x41400000u);
                    swc1(sp + 0x13c, 0.0f);
                    swc1(sp + 0x130, f1);
                    swc1(sp + 0x134, f2);
                    swc1(sp + 0x138, f0);
                    V(sp + 0x120) = V(sp + 0x130);
                    vec_clear(V(sp + 0x140));
                    swc1(sp + 0x140, rand_range_f(gs * f25, gs * f24) * f22);
                    const float gsb = lwc1(0x0015ed6c);
                    swc1(sp + 0x144, rand_range_f(gsb * f25, gsb * f24) * f22);
                    const float gsc = lwc1(0x0015ed6c);
                    swc1(sp + 0x148, rand_range_f(gsc * u2f(0x40800000u), lwc1(sp + 0x120 + static_cast<uint32_t>(kind) * 4)) * f22);
                    V(sp + 0x130) = V(sp + 0x140);
                    float lim13 = u2f(0x3e800000u);
                    if (kind == 1) lim13 = 0.5f;
                    f20 = rand_range_f(0.0f, lim13) * f22;
                    f21 = rand_angle();
                    swc1(sp + 0x140, cos_vu(f21) * f20);
                    swc1(sp + 0x144, sin_vu(f21) * f20);
                    sw(sp + 0x148, 0);
                    s4 = sp + 0x140;
                    vec3_add(V(s4), V(s4), V(pos));
                    do {
                        const float c = f20 * cos_vu(u2f(0x3f490fd8u));
                        const float f20b = (f20 * lwc1(0x0015ed6c)) * u2f(0x41000000u);
                        swc1(sp + 0x148, lwc1(sp + 0x148) - c);
                        swc1(sp + 0x138, lwc1(sp + 0x138) - f20b);
                    } while (false);
                    switch (kind) {
                    case 0: {
                        const uint32_t c0 = color_swap_channels(0x1f101820u, lbu(0x0013e531));
                        const float sz = rand_range_f(u2f(0x48435000u), u2f(0x48927c00u)) * f22;
                        const int32_t a = frame_scale_i(0xb4);
                        const int32_t b = frame_scale_i(0xf0);
                        const int32_t life0 = rand_range(a, b);
                        (void)spawn_particle_10(s4, sp + 0x130, c0, 0x101010u, static_cast<uint32_t>(life0), 0u, sz);
                        break;
                    }
                    case 1: {
                        const uint32_t c0 = color_swap_channels(0x3f081020u, lbu(0x0013e531));
                        const uint32_t c1 = color_swap_channels(0x0f081020u, lbu(0x0013e531));
                        const float sz = rand_range_f(u2f(0x47435000u), u2f(0x47c35000u)) * f22;
                        const int32_t a = frame_scale_i(0xb4);
                        (void)spawn_particle_10(s4, sp + 0x130, c0, c1, static_cast<uint32_t>(a), 1u, sz);
                        break;
                    }
                    case 2: {
                        const int32_t rr = rand_mod(0x64);
                        const uint32_t sel = 0x27 < rr ? 0x2f486078u : 0x5ff8f8f8u;
                        const uint32_t c0 = color_swap_channels(sel, lbu(0x0013e531));
                        const uint32_t c1 = color_swap_channels(0x0f000020u, lbu(0x0013e531));
                        const float sz = f22 * u2f(0x48127c00u);
                        const int32_t a = frame_scale_i(0x1e);
                        const int32_t b = frame_scale_i(0x2d);
                        const int32_t life0 = rand_range(a, b);
                        (void)spawn_particle_10(s4, sp + 0x130, c0, c1, static_cast<uint32_t>(life0), 2u, sz);
                        break;
                    }
                    case 3: {
                        uint32_t s2 = rgba_lerp_ovl(0x7f000000u, 0x7f182030u, rand_range_f(u2f(0x3e800000u), f23));
                        uint32_t s1c = rgba_lerp_ovl(0u, 0x005f5f5fu, rand_range_f(0.5f, f23));
                        s2 = color_swap_channels(s2, lbu(0x0013e531));
                        s1c = color_swap_channels(s1c, lbu(0x0013e531));
                        const float r3 = rand_range_f(0.0f, f23) * lwc1(0x0015ed6c);
                        const float yaw2 = rand_angle();
                        vec_from_spherical_v(V(sp + 0x130), r3, f21, yaw2);
                        const float sz = rand_range_f(u2f(0x48435000u), u2f(0x48927c00u)) * f22;
                        const int32_t a = frame_scale_i(0xf0);
                        const int32_t b = frame_scale_i(0x12c);
                        const int32_t life0 = rand_range(a, b);
                        (void)spawn_particle_10(s4, sp + 0x130, s2, s1c, static_cast<uint32_t>(life0), 3u, sz);
                        break;
                    }
                    default:
                        break;
                    }
                    --s8;
                } while (s8 != 0);
            }
            goto L_ac798;
        }
    }
    // underwater: bubbles and a spray on the surface, then the shards
    {
        V(sp + 0x120) = V(m + 0x10);
        swc1(sp + 0x128, lwc1(sp + 0x128) + u2f(0x40600000u));
        const float hh = floor_probe_v(0.5f, V(sp + 0x120), 0);
        f22 = hh;
        {
            const float f23 = u2f(0x484d1400u);
            int32_t i = 0x95;
            do {
                const Vec4 z0{};
                V(sp + 0x140) = z0;
                swc1(sp + 0x140, rand_range_f(-1.0f, 1.0f));
                --i;
                swc1(sp + 0x144, rand_range_f(-1.0f, 1.0f));
                swc1(sp + 0x148, rand_range_f(u2f(0xbf000000u), u2f(0xc0000000u)));
                V(sp + 0x130) = V(sp + 0x140);
                const float gs = lwc1(0x0015ed6c);
                const float rr = rand_range_f(gs * 4.0f, gs * 8.0f);
                vec3_set_length(V(sp + 0x130), V(sp + 0x130), rr);
                const float sz = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccdu)) * f23;
                (void)spawn_particle_22(pos, sp + 0x130, sz, f22);
            } while (i >= 0);
        }
        {
            const float f24 = 0.0f, f23 = u2f(0x3eb33333u);
            int32_t s4i = 0x95;
            do {
                const float gs = lwc1(0x0015ed6c);
                const Vec4 z0{};
                V(sp + 0x140) = z0;
                --s4i;
                swc1(sp + 0x140, rand_range_f(gs * u2f(0xbf400000u), gs * u2f(0x3f400000u)));
                const float gsb = lwc1(0x0015ed6c);
                swc1(sp + 0x144, rand_range_f(gsb * u2f(0xbf400000u), gsb * u2f(0x3f400000u)));
                const float gsc = lwc1(0x0015ed6c);
                swc1(sp + 0x148, rand_range_f(gsc * 4.0f, gsc * 16.0f));
                f21 = rand_range_f(f24, f23);
                f20 = rand_angle();
                V(sp + 0x130) = V(sp + 0x140);
                swc1(sp + 0x140, cos_vu(f20) * f21);
                swc1(sp + 0x144, sin_vu(f20) * f21);
                swc1(sp + 0x148, f24);
                vec3_add(V(sp + 0x140), V(sp + 0x140), V(pos));
                f21 = f21 * lwc1(0x0015ed6c);
                f21 = f21 * 8.0f;
                swc1(sp + 0x148, f22 + 0.5f);
                swc1(sp + 0x138, lwc1(sp + 0x138) - f21);
                const float sz = rand_range_f(f23, u2f(0x3fa00000u));
                (void)spawn_particle_40(sp + 0x140, sp + 0x130, 0, 0x167f6060u, 0x7f6060u, 1, sz, f22, lwc1(0x0015ed70) * u2f(0x41a00000u));
            } while (s4i >= 0);
        }
        sw(sp + 0x190, 0x7f7f6000u);
        sw(sp + 0x194, 0x30);
        const float f24 = 1.0f, f23 = u2f(0xc0400000u), f21b = 3.0f;
        for (int32_t k = 0x13; k >= 0;) {
            const float lerp1 = rand_range_f(0.25f, f24);
            --k;
            const uint32_t s1c = rgba_lerp_ovl(0x7f000000u, lwu(sp + 0x190) | 0x60u, lerp1);
            const float lerp2 = rand_range_f(0.5f, f24);
            const uint32_t s2c = rgba_lerp_ovl(0u, 0x3f3000u | lwu(sp + 0x194), lerp2);
            swc1(sp + 0x140, rand_range_f(f23, f21b));
            swc1(sp + 0x144, rand_range_f(f23, f21b));
            const float gs = lwc1(0x0015ed6c);
            swc1(sp + 0x148, rand_range_f(gs * 4.0f, gs * u2f(0x41600000u)));
            const float rr = rand_range_f(u2f(0x3fc00000u), f21b);
            vec3_set_length(V(sp + 0x140), V(sp + 0x140), rr * lwc1(0x0015ed6c));
            const float rr2 = rand_range_f(f21b, u2f(0x40c00000u)) * lwc1(0x0015ed6c);
            V(sp + 0x130) = V(m + 0x10);
            swc1(sp + 0x138, f22);
            swc1(sp + 0x148, rr2);
            const uint32_t c0 = color_swap_channels(s1c, lbu(0x0013e531));
            const uint32_t c1 = color_swap_channels(s2c, lbu(0x0013e531));
            const float sz = rand_range_f(u2f(0x46f61801u), u2f(0x474d1400u));
            const int32_t a = frame_scale_i(0x5a);
            const int32_t b = frame_scale_i(0x96);
            const int32_t life0 = rand_range(a, b);
            (void)spawn_particle_0f(sp + 0x130, sp + 0x140, c0, c1, static_cast<uint32_t>(life0), 1u, static_cast<int32_t>(lbu(lwu(OV(0x001b20ac)))), -1, sz);
            if (k < 0) break;
        }
        sb(m + 0xbc, 0);
        goto L_ac7a0;
    }
L_ac798:
L_ac79c:
    sb(m + 0xbc, 0);
L_ac7a0:
    if (lbu(0x0013e531) != 0) (void)moby_sound_start(3, 0, m);
    else (void)moby_sound_start(2, 0, m);
    if (f27 < u2f(0x41a00000u)) swc1(OV(0x00166de0), u2f(0x3ecccccdu) - f27 * u2f(0x3c8f5c29u));
    else swc1(OV(0x00166de0), u2f(0x3d4cccd0u));
    sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x19)));
    (void)vec3_distance(V(pos), V(P + 0x80));
    if (lbu(0x0013e531) != 0) (void)effect_spawn(sp + 0x70, pos, 0, 0);
    else (void)effect_spawn(sp + 0x20, pos, 0, 0);
    moby_list_remove(m, OV(0x001b07b0));
L_retire:
    moby_retire(m);
    return;
L_ac8ac:
    floor_light_add(m, u2f(0x3e800000u));
}

}  // namespace rac1
