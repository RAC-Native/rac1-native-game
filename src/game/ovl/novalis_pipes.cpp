// Novalis's own moby classes: the steam pipe that breaks (class 778) and the leak it leaves (class 779; hand-ported from
// Novalis's own overlay). Header novalis_mobys2.h, registered in novalis_mobys2.cpp. Addresses are Novalis's (`orig: L1:`);
// the base ELF's are bare (the frame time 0x15ed6c).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>

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

// a puff of steam (particle 0x16, white fading to grey, 45 .. 60 frames, 125000 .. 175000) at `pos` moving by `vel` (EE
// vectors)
void steam_puff(uint32_t pos, uint32_t vel) {
    const float size = rand_range_f(125000.0f, 175000.0f);
    const int32_t frames = frame_scale_i(rand_range(45, 60));
    particle_16_spawn(pos, vel, 0x207f7f7fu, NOT_ADDR(0x00272727u), frames, size);
}
}  // namespace

// The leak (class 779) put where the pipe `m` was: its place, turn and the words +0x38 / +0x3c, no sound yet (its pvars +0
// -1). The moby or 0.
// orig: L1:0x002ffa90
uint32_t nov_2ffa90_l1(uint32_t m) {
    const uint32_t leak = moby_spawn(0x30b);
    if (leak == 0) return 0;
    sb(leak + 0x30, 0x20);
    sh(leak + 0x32, 0xff);
    sb(leak + 0x31, 1);
    sb(leak + 0x20, 0);
    const uint32_t pv = lwu(leak + 0x78);
    sw(leak + 0x38, lwu(m + 0x38));
    sw(leak + 0x3c, lwu(m + 0x3c));
    lq_sq(leak + 0x10, m + 0x10);
    lq_sq(leak + 0x40, m + 0x40);
    sw(pv, 0xffffffffu);
    moby_update_transform(leak);
    return leak;
}

// Class 778: the pipe. Hit (mask 0x10000) for some damage, it bursts: a random count drawn from its pvars' range +0 .. +4 (and
// not used), its sound, 150 puffs of steam from just off its place, the leak in its place; then it goes. Its hit slot +0xa4
// is cleared every frame.
// orig: L1:0x002ff860
void nov_class778_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
    sb(m + 0xa4, 0xff);
    if (hit == 0 || !(0.0f < lwc1(hit + 0x2c))) return;
    rand_mod(lw(pv + 4) - lw(pv) + 1);  // (the original counts it down in an empty loop)
    moby_sound_start(0, 0, m);
    const Frame f(0x30);
    const uint32_t at = f.sp, vel = f.sp + 0x10;
    for (int i = 0; i < 150; ++i) {
        V(at) = Vec4{u2f(0xbe4ccccdu), 0.0f, u2f(0x3f99999au), 0.0f};
        vec3_add(V(at), V(at), V(m + 0x10));
        const float c = cos_vu(rand_angle());
        swc1(vel, fpu_mul(c, fpu_mul(rand_range_f(1.5f, 3.0f), dt())));
        const float s = sin_vu(rand_angle());
        sw(vel + 8, 0);
        swc1(vel + 4, fpu_mul(s, fpu_mul(rand_range_f(1.5f, 3.0f), dt())));
        swc1(vel + 8, fpu_mul(rand_range_f(lwc1(vel + 8), 3.0f), dt()));
        steam_puff(at, vel);
    }
    nov_2ffa90_l1(m);
    moby_retire(m);
}

// Class 779: the leak. Its hiss kept playing (its pvars +0 the sound's slot) and two puffs of steam a frame, blown along the
// moby's axes from just off its place.
// orig: L1:0x002ffb28
void nov_class779_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (sub_0028d8c0(m, lw(pv)) == 0) sw(pv, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    const Frame f(0x30);
    const uint32_t at = f.sp, vel = f.sp + 0x10, local = f.sp + 0x20;
    for (int i = 0; i < 2; ++i) {
        V(at) = Vec4{u2f(0xbe4ccccdu), 0.0f, u2f(0x3f99999au), 0.0f};
        V(local) = Vec4{};
        swc1(local, fpu_mul(rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au)), fpu_mul(dt(), 3.0f)));
        swc1(local + 8, fpu_mul(rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au)), fpu_add(dt(), dt())));
        V(vel) = V(local);
        mat3_transform(V(vel), V(vel), M(m + 0xc0));
        vec3_add(V(at), V(at), V(m + 0x10));
        steam_puff(at, vel);
    }
}

}  // namespace rac1
