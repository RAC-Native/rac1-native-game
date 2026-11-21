// Novalis's own moby classes: the effects of the level's scenes (class 1546) and Novalis's build of the scripted camera's
// angles (hand-ported from Novalis's own overlay). Header novalis_mobys2.h, registered in novalis_mobys2.cpp. Addresses are
// Novalis's (`orig: L1:`); Novalis-only data is L1_ADDR, the base ELF's are bare (the frame time 0x15ed6c).
//
// The scene record 0x16cce0: the scene playing +0x30, its frame +0x34, its mobys +0x178 (one word each).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

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

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
int32_t scene_number() { return lw(L1_ADDR(0x0016cce0) + 0x30); }
int32_t scene_frame() { return lw(L1_ADDR(0x0016cce0) + 0x34); }
uint32_t scene_moby(uint32_t k) { return lwu(L1_ADDR(0x0016cce0) + 0x178 + k * 4); }

// a splash where a moby lands in water: the ring (class 307, scale `scale`, its byte +0x23 0x70) and 16 drops (particle
// 0x23) thrown up from `pos` (an EE vector), the velocities in `vel` (an EE vector)
void water_splash(uint32_t pos, uint32_t vel, float scale) {
    const uint32_t ring = spawn_moby_307(pos, scale);
    if (ring != 0) sb(ring + 0x23, 0x70);
    for (int i = 0; i < 16; ++i) {
        const float a = rand_angle();
        const float r = rand_range_f(fpu_mul(dt(), 0.0f), fpu_mul(dt(), 3.0f));
        swc1(vel, fpu_mul(cos_vu(a), r));
        swc1(vel + 4, fpu_mul(sin_vu(a), r));
        swc1(vel + 8, rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.5f)));
        const int32_t life = rand_range(90, 120);
        spawn_particle_23(pos, vel, rand_mod(2), static_cast<uint32_t>(life));
    }
}
}  // namespace

// The scripted camera's (the moby with uid 5: the current camera 0x167280 when it is that one) pvars +0x90 set from `src`
// (an EE quadword). Novalis's own build of Veldin's uid5_set_pvar_90 (0x2eaa30); shared_class280.cpp and
// shared_gold_bolt.cpp call it on Novalis.
// orig: L1:0x00316e28
void nov_316e28_l1(uint32_t src) {
    const uint32_t cam = lwu(L1_ADDR(0x00167280));
    const uint32_t m = lh(cam + 0x86) == 5 ? cam : moby_table_find_by_uid(5);
    lq_sq(lwu(m + 0x70) + 0x90, src);
}

// Class 1546: the effects of the level's scenes while one plays (mode 2). Scenes 1 .. 4 (2 only to its frame 148): smoke from
// one of the scene's mobys (the third in scene 2, else the fourth). Scene 1: from frame 472 to 537 sparks (particle
// 0x2e) off its first moby's joint 1, and at 472 that moby lands in water (a splash). Scene 4: its fifth moby (class 531)
// drawn by 0x2a70a8, and from frame 850 to 1160 two flames (particle 0x17) at each of its joints 1 .. 6. Scene 5, to frame
// 120: its third moby's joint 5 followed (its speed kept at 0x162070), two explosions at frame 118, its draw and four
// puffs of smoke along each of its joints 1 and 2.
// orig: L1:0x0030c190
void nov_class1546_update_l1(uint32_t m) {
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (lbu(m + 0x20) != 1) return;
    if (lw(L1_ADDR(0x0015f5c4)) != 2) return;
    const Frame f(0x100);
    const uint32_t sp = f.sp;
    const int32_t sc = scene_number();
    const bool smoke = sc == 1 || sc == 3 || sc == 4 || (sc == 2 && !(frame_scale_i(148) < scene_frame()));
    if (smoke) {
        // (the original's index: 3, set in a branch's delay slot that runs on every path, then 2 for scene 2 alone)
        scene_moby_smoke(scene_moby(scene_number() == 2 ? 2 : 3));
    }
    if (scene_number() == 1) {
        if (!(scene_frame() < frame_scale_i(472)) && !(frame_scale_i(537) < scene_frame()) && scene_moby(0) != 0) {
            moby_chain_point_v(scene_moby(0), 1, V(sp + 0x20));
            const float size = rand_range_f(0.5f, 2.0f);
            const float spin = rand_mod(2) != 0 ? 2.0f : -2.0f;
            const uint32_t p = spawn_particle_2e(sp + 0x20, L1_ADDR(0x0015f580), sp + 0x28, size, spin);
            if (p != 0) sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(60)));
        }
        if (scene_number() == 1 && scene_frame() == frame_scale_i(472) && scene_moby(0) != 0) {
            moby_chain_point_v(scene_moby(0), 1, V(sp + 0x20));
            water_splash(sp + 0x20, sp + 0x30, 3.0f);
        }
    }
    if (scene_number() == 4) {
        const uint32_t o = scene_moby(4);
        if (o != 0 && lh(o + 0xa6) == 0x213) {
            sub_001f93e0(L1_ADDR(0x002a70a8), o);
            if (!(scene_frame() < frame_scale_i(850)) && !(frame_scale_i(1160) < scene_frame())) {
                for (uint32_t joint = 1; joint < 7; ++joint) {
                    moby_chain_point_v(o, joint, V(sp + 0x20));
                    for (int k = 0; k < 2; ++k) {
                        const int32_t r = rand_mod(16);
                        const int32_t spin = rand_mod(2) == 0 ? r : -r;
                        const uint32_t p = spawn_particle_17(sp + 0x20, spin, L1_ADDR(0x0015f580), 0x7f204080u, u2f(0x3dcccccdu), 1.0f,
                                                             u2f(0x3f666666u), 200000.0f);
                        if (p == 0) continue;
                        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(scene_frame() < frame_scale_i(1100) ? 6 : 30)));
                        sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
                        sw(p + 0x24, 2);
                        sb(p + 0x2a, 0x7f);
                        sb(p + 0x2b, lbu(p + 0xa));
                    }
                }
            }
        }
    }
    if (scene_number() != 5 || !(scene_frame() < frame_scale_i(120))) return;
    const uint32_t o = scene_moby(2);
    if (o == 0) return;
    const uint32_t vel = sp + 0x20, at = sp + 0x30, a = sp + 0x40, b = sp + 0x50;
    V(vel) = Vec4{};
    moby_chain_point_v(o, 5, V(at));
    if (!(scene_frame() < 2)) vec3_sub(V(vel), V(at), V(L1_ADDR(0x00162070)));
    V(L1_ADDR(0x00162070)) = V(at);
    if (scene_frame() == frame_scale_i(118)) {
        vec_clear(V(a));
        explosion_effect(o, a, at, 3, 0xc8, 0xc8, -1, 0, 0x3c, -1, 0, 0.0f, 0.0f, 5.0f, 2.5f, 9.0f, 1.0f, 60.0f);
        V(b) = V(at);
        swc1(b + 8, fpu_sub(lwc1(b + 8), 1.0f));
        explosion_effect(o, a, b, 0x12c, 2, 2, -1, 0, 0, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 9.0f, 1.0f, 60.0f);
    }
    sub_001f93e0(L1_ADDR(0x002a70a8), o);
    moby_chain_point_v(o, 1, V(a));
    moby_chain_point_v(o, 2, V(b));
    vec3_sub(V(a), V(a), V(vel));
    vec3_sub(V(b), V(b), V(vel));
    vec3_scale(V(vel), V(vel), 0.25f);
    for (int i = 0; i < 4; ++i) {
        smoke_burst(a, a, L1_ADDR(0x0015f580), 150000.0f, 60000.0f);
        smoke_burst(b, b, L1_ADDR(0x0015f580), 150000.0f, 60000.0f);
        vec3_add(V(a), V(a), V(vel));
        vec3_add(V(b), V(b), V(vel));
    }
}

}  // namespace rac1
