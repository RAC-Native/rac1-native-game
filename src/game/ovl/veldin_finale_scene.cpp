// Veldin's finale (level 18): the classes the level's scenes drive (the scene record L18:0x16d260: +0x30 the scene
// playing, +0x34 its frame, +0x178 .. +0x194 its mobys), while the level's mode L18:0x15f5c4 is 2 (a scene). Class 1432
// (13 placed) is hidden at its first frame for good. Class 1434 (3 placed) hides in scene 3, or at the level's first
// frames when L18:0x1623a0 is set and it is not turned (+0x40 = 0); hidden, it turns half round and mirrors its height
// about 101.6 when L18:0x1623a0 is set, and shows again once the scene is over. Class 1568 (none placed) hides and turns
// half round in scene 3, shows again (state 3) when it is over. Class 1799 (one placed) blows smoke from joint 8 toward
// joint 9 of the scene's moby (+0x184; +0x188 in scene 1) in scenes 0 .. 3. Class 1750 (one placed) has the game's save
// image built once (vf_save_image_once_l18). Class 1563 (one placed) plays the end scenes' effects by the scene's frame:
// in scene 4 the smoke of the scene's mobys, their blasts, a last blast at the fixed point (634.6, 454.2, 103.0), and
// queues the scene's draws (L18:0x2f9c20 / 0x2f9eb8 on the class 0x59b moby, L18:0x2f9780 on the scene's moby +0x178)
// in their frame windows (the windows' words L18:0x162498 .. 0x1624c8, set by its state 0, re-set from L18:0x1624a4 ..
// at scene 7's first frame); in scene 1 the beam of the scene's moby +0x188 (morph_2bd0a0 with the morph speed 24); in
// scene 5 two trails of sparks (vf_scene_sparks_l18) between joints 0 / 2 of the moby +0x180 and the points they were at;
// in scenes 0, 1, 2, 3 (before frame 0x24a) the draw L18:0x2f9a98, and at scene 3's frame 0x24a a ring of 32 particles
// before the camera. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's
// bare. Registered in veldin_finale_mobys.cpp; the helpers' declarations in veldin_finale_own.h.
#include <game/math/vecmath.h>
#include <game/ovl/memory_card_ops.h>
#include <game/ovl/morph.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr float PI = 3.1415927410125732f;
constexpr int16_t SCENE_MOBY_CLASS = 0x59b;  // (1435: the end scene's moby the draws L18:0x2f9c20 / 0x2f9eb8 are queued on)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the level in a scene (its mode L18:0x15f5c4 2)
bool in_scene() { return lw(L18_ADDR(0x0015f5c4)) == 2; }

// the first moby of class 0x59b on the drawn mobys' chain (L18:0x15ffe4, linked by +0x28) kept in L18:0x162528 (not
// changed when there is none)
void find_scene_moby() {
    for (uint32_t o = lwu(L18_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
        if (lh(o + 0xa6) == SCENE_MOBY_CLASS) {
            sw(L18_ADDR(0x00162528), o);
            return;
        }
    }
}

// a 16-sided spark (particle 0x17) record's finish: its life `life` frames, its size byte (4 + 0x40), kind 2 at +0x24,
// alpha 0x7f, its life's low byte again at +0x2b
void spark_finish(uint32_t p, int32_t life) {
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(life)));
    sb(p + 0x9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sw(p + 0x24, 2);
    sb(p + 0x2a, 0x7f);
    sb(p + 0x2b, lbu(p + 0xa));
}

// a spin of up to 15 either way (the original's two random draws)
int32_t random_spin() {
    const int32_t r = rand_mod(0x10);
    return rand_mod(2) == 0 ? r : -r;
}

// a blast of the scene (its smoke, sparks and shards: explosion_effect) at the EE point `at`, from `m`, with no velocity
// (the EE vector `vel` cleared, its z then 1); `big` the last one's larger sizes
void scene_blast(uint32_t m, uint32_t vel, uint32_t at, bool big) {
    vec_clear(V(vel));
    swc1(vel + 8, 1.0f);
    if (big) explosion_effect(m, vel, at, 0x12c, 0xc8, 0xc8, -1, 0, 0x3c, -1, 0, 0.0f, 0.0f, 12.0f, 6.0f, 9.0f, 1.0f, 60.0f);
    else explosion_effect(m, vel, at, 0x32, 0x19, 0x19, -1, 0, 0xf, -1, 0, 0.0f, 0.0f, 8.0f, 4.0f, 9.0f, 1.0f, 30.0f);
}

// the fraction of the window `from` .. `to` (frames) the frame `frame` is at
float window_part(int32_t frame, int32_t from, int32_t to) {
    return fpu_div(int_to_float(frame - from), int_to_float(to - from));
}
}  // namespace

// The game's save image built once into `buf` (the full-screen menu's state L18:0x1ba6f0 keeps it at +0xe0; nothing
// when one is kept already).
// orig: L18:0x00281fa8
void vf_save_image_once_l18(uint32_t buf) {
    const uint32_t ms = L18_ADDR(0x001ba6f0);
    if (lw(ms + 0xe0) != 0) return;
    sw(ms + 0xe0, buf);
    save_blocks_write(buf);
}

// Sparks of the end scene: two of particle 0x17 at the EE point `at` (dark red, spin up to 15 either way, life 15
// frames, size `size_a`), then three white ones at `at2` (spins 16, -16, 16, life 4 frames, a random alpha byte, size
// `size_b`), all moving by the EE vector `vel`.
// orig: L18:0x002fa250
void vf_scene_sparks_l18(uint32_t at, uint32_t at2, uint32_t vel, float size_a, float size_b) {
    const Frame f(192);
    const uint32_t a = f.sp, b = f.sp + 0x10;
    lq_sq(a, at);
    lq_sq(b, at2);
    for (int k = 0; k < 2; ++k) {
        const int32_t spin = random_spin();
        const uint32_t p = spawn_particle_17(a, spin, vel, 0x7f204080u, 0.0f, 1.0f, u2f(0x3f666666u), size_a);  // (0.9)
        if (p != 0) spark_finish(p, 0xf);
    }
    int32_t spin = 0x10;
    for (int k = 0; k < 3; ++k) {
        const uint32_t p = spawn_particle_17(b, spin, vel, 0x7fffffffu, 0.0f, 1.0f, u2f(0x3f7851ecu), size_b);  // (0.97)
        spin = -spin;
        if (p == 0) continue;
        sb(p + 0x9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(4)));
        sb(p + 0x8, static_cast<uint32_t>(rand_mod(0xff)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x7f);
        sb(p + 0x2b, lbu(p + 0xa));
    }
}

// orig: L18:0x002f7ab0
void vf_class1432_update_l18(uint32_t m) {
    if (lbu(m + 0x20) != 0) return;
    sb(m + 0x20, 1);
    sh(m + 0x34, lhu(m + 0x34) | 3);
    sb(m + 0x31, 0);
}

// orig: L18:0x002f7ad8
void vf_class1434_update_l18(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st == 0 || st == 1) {
        if (st == 0) sb(m + 0x20, 1);
        // (0x002f7b28) shown: hidden in scene 3, or at the level's first frames with L18:0x1623a0 set when not turned
        const bool scene3 = in_scene() && lw(L18_ADDR(0x0016d290)) == 3;
        if (!scene3 && !(lw(L18_ADDR(0x0015f5cc)) < 5 && lw(L18_ADDR(0x001623a0)) != 0 && lwc1(m + 0x40) == 0.0f)) return;
        sb(m + 0x20, 2);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        sb(m + 0x31, 0);
    } else if (st == 2) {  // (0x002f7b9c) hidden: turned half round, its height mirrored about 101.6; shown again after the scene
        if (lw(L18_ADDR(0x001623a0)) != 0 && lwc1(m + 0x40) == 0.0f) {
            const float up = fpu_add(fpu_sub(101.5999984741211f, lwc1(m + 0x18)), fpu_sub(101.5999984741211f, lwc1(m + 0x18)));
            swc1(m + 0x40, angle_add(lwc1(m + 0x40), PI));
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), up));
        }
        if (in_scene()) return;
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffe);
        sb(m + 0x31, 1);
    }
}

// orig: L18:0x002fa640
void vf_class1568_update_l18(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x002fa694) hidden and turned half round in scene 3
        if (!in_scene() || lw(L18_ADDR(0x0016d290)) != 3) break;
        sh(m + 0x34, lhu(m + 0x34) | 1);
        sb(m + 0x31, 0);
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), PI));
        sb(m + 0x20, 2);
        break;
    case 2:  // (0x002fa6ec) shown again once the scene is over
        if (in_scene()) break;
        sb(m + 0x20, 3);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffe);
        sb(m + 0x31, 1);
        break;
    default:
        break;
    }
}

// orig: L18:0x002fad08
void vf_class1750_update_l18(uint32_t) { vf_save_image_once_l18(L18_ADDR(0x001dfc10)); }

// orig: L18:0x002fad28
void vf_class1799_update_l18(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1 || !in_scene()) return;
    const uint32_t scene = lwu(L18_ADDR(0x0016d290));
    if (!(scene < 4)) return;
    const uint32_t rec = L18_ADDR(0x0016d260);
    const uint32_t slot = lw(rec + 0x30) == 1 ? 4 : 3;  // (the scene's moby +0x188 in scene 1, +0x184 in scenes 0, 2, 3)
    const Frame f(96);
    const uint32_t from = f.sp, to = f.sp + 0x10, dir = f.sp + 0x20;
    const uint32_t o = lwu(rec + 0x178 + (slot << 2));
    moby_chain_point_v(o, 8, V(from));
    moby_chain_point_v(o, 9, V(to));
    vec3_sub(V(dir), V(from), V(to));
    vec3_set_length(V(dir), V(dir), lwc1(L18_ADDR(0x00162578)));
    smoke_burst(from, from, dir, lwc1(L18_ADDR(0x0016257c)), lwc1(L18_ADDR(0x00162580)));
}

// orig: L18:0x002f88e8
void vf_class1563_update_l18(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    if (st == 0) {  // (0x002f8940) the scene 4 draws' frame windows
        sw(L18_ADDR(0x00162498), 0x163);
        sw(L18_ADDR(0x001624bc), 0x3c);
        sw(L18_ADDR(0x001624c0), 0xc8);
        sw(L18_ADDR(0x001624c4), 0xbe);
        sw(L18_ADDR(0x001624c8), 0x16d);
        sw(L18_ADDR(0x0016249c), 0x16d);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1 || !in_scene()) return;
    const Frame f(304);
    const uint32_t sp = f.sp;
    const uint32_t rec = L18_ADDR(0x0016d260);
    auto frame = [rec] { return lw(rec + 0x34); };
    auto fs = [](int32_t n) { return frame_scale_i(n); };
    auto zero = [] { return L18_ADDR(0x0015f580); };  // (the zero vector)

    if (lw(L18_ADDR(0x0016d290)) == 4) {  // ---- scene 4 (0x002f89b0)
        if (lw(L18_ADDR(0x0016d294)) == 1) find_scene_moby();
        if (!(frame() < fs(0x1f4)) && !(fs(0x35c) < frame())) {  // the smoke of the scene's mobys +0x184 .. +0x194
            for (uint32_t k = 0; k < 5; ++k) {
                const uint32_t o = lwu(rec + 0x184 + (k << 2));
                if (o == 0) continue;
                const uint32_t at = sp + 0x40;
                moby_chain_point_v(o, 0, V(at));
                float size = lwc1(L18_ADDR(0x001f2c20) + (k << 2));  // (the table the original copies into its frame at +0x20)
                const uint32_t slot = k + 3;
                if (slot == 3 && !(frame() < fs(0x285)) && !(fs(0x2e0) < frame())) continue;  // (the first one quiet in 0x285 .. 0x2e0)
                if (slot == 5 && !(frame() < fs(0x285)) && !(fs(0x2da) < frame())) size = 300000.0f;
                smoke_burst(at, at, zero(), size, fpu_mul(size, 0.4000000059604645f));
            }
        }
        // the blasts on the scene's mobys +0x194, +0x188, +0x190, +0x18c at their frames
        static const struct { int32_t at; uint32_t slot; } blasts[] = {{0x2c2, 0x194}, {0x2c6, 0x188}, {0x2d0, 0x190}, {0x2da, 0x18c}};
        for (const auto& b : blasts) {
            if (frame() != fs(b.at)) continue;
            const uint32_t o = lwu(rec + b.slot);
            if (o == 0) continue;
            moby_chain_point_v(o, 0, V(sp + 0x20));
            scene_blast(o, sp + 0x30, sp + 0x20, false);
        }
        if (frame() == fs(0x2ec)) {  // the last, at a fixed point (0x002f8e08)
            sw(sp + 0x20, 0); sw(sp + 0x24, 0); sw(sp + 0x28, 0); sw(sp + 0x2c, 0);
            swc1(sp + 0x20, u2f(0x441ea396u));  // (634.556)
            swc1(sp + 0x24, u2f(0x43e314ddu));  // (454.163)
            swc1(sp + 0x28, u2f(0x42cdff7du));  // (102.999)
            swc1(sp + 0x2c, 1.0f);
            scene_blast(m, sp + 0x30, sp + 0x20, true);
        }
        // the draws in their windows (the windows' bounds frame-scaled)
        if (!(frame() < fs(lw(L18_ADDR(0x001624bc)))) && frame() < fs(lw(L18_ADDR(0x001624c0)))) {
            const float t = fpu_div(int_to_float(frame() - fs(lw(L18_ADDR(0x001624bc)))),
                                    int_to_float(fs(lw(L18_ADDR(0x001624c0)) - lw(L18_ADDR(0x001624bc)))));
            sw(L18_ADDR(0x00162524), static_cast<uint32_t>(float_to_int(fpu_mul(t, 255.0f))));
            sub_001f93e0(L18_ADDR(0x002f9c20), lwu(L18_ADDR(0x00162528)));
        }
        if (!(frame() < fs(lw(L18_ADDR(0x001624c4)))) && frame() < fs(lw(L18_ADDR(0x001624c8)))) {
            const float t = fpu_div(int_to_float(frame() - fs(lw(L18_ADDR(0x001624c4)))),
                                    int_to_float(fs(lw(L18_ADDR(0x001624c8)) - lw(L18_ADDR(0x001624c4)))));
            swc1(L18_ADDR(0x0016252c), fpu_add(fpu_mul(t, 200.0f), 0.25f));
            sub_001f93e0(L18_ADDR(0x002f9eb8), lwu(L18_ADDR(0x00162528)));
        }
        if (!(frame() < fs(lw(L18_ADDR(0x00162498)))) && frame() < fs(lw(L18_ADDR(0x001624a0)))) {
            sub_001f93e0(L18_ADDR(0x002f9780), lwu(rec + 0x178));
            if (!(frame() < fs(lw(L18_ADDR(0x0016249c))))) {
                swc1(L18_ADDR(0x00162520), 10.0f);
            } else {
                const float t = fpu_div(int_to_float(frame() - fs(lw(L18_ADDR(0x00162498)))),
                                        int_to_float(fs(lw(L18_ADDR(0x0016249c)) - lw(L18_ADDR(0x00162498)))));
                const float lo = lwc1(L18_ADDR(0x00162490));
                swc1(L18_ADDR(0x00162520), fpu_add(lo, fpu_mul(fpu_sub(lwc1(L18_ADDR(0x00162494)), lo), t)));
            }
        }
    }

    if (lw(rec + 0x30) == 7) {  // ---- scene 7 (0x002f90a8): the windows re-set at its first frame; the parts unscaled
        if (lw(rec + 0x34) == 1) {
            sw(L18_ADDR(0x001624bc), lwu(L18_ADDR(0x001624a4)));
            sw(L18_ADDR(0x001624c0), lwu(L18_ADDR(0x001624a8)));
            sw(L18_ADDR(0x001624c4), lwu(L18_ADDR(0x001624ac)));
            sw(L18_ADDR(0x001624c8), lwu(L18_ADDR(0x001624b0)));
            sw(L18_ADDR(0x00162498), lwu(L18_ADDR(0x001624b4)));
            sw(L18_ADDR(0x0016249c), lwu(L18_ADDR(0x001624b8)));
            find_scene_moby();
        }
        if (!(frame() < fs(lw(L18_ADDR(0x001624bc)))) && frame() < fs(lw(L18_ADDR(0x001624c0)))) {
            const float t = window_part(frame(), lw(L18_ADDR(0x001624bc)), lw(L18_ADDR(0x001624c0)));
            sw(L18_ADDR(0x00162524), static_cast<uint32_t>(float_to_int(fpu_mul(t, 255.0f))));
            sub_001f93e0(L18_ADDR(0x002f9c20), lwu(L18_ADDR(0x00162528)));
        }
        if (!(frame() < fs(lw(L18_ADDR(0x001624c4)))) && frame() < fs(lw(L18_ADDR(0x001624c8)))) {
            const float t = window_part(frame(), lw(L18_ADDR(0x001624c4)), lw(L18_ADDR(0x001624c8)));
            swc1(L18_ADDR(0x0016252c), fpu_add(fpu_mul(t, 200.0f), 0.25f));
            sub_001f93e0(L18_ADDR(0x002f9eb8), lwu(L18_ADDR(0x00162528)));
        }
        if (!(frame() < fs(lw(L18_ADDR(0x00162498)))) && frame() < fs(lw(L18_ADDR(0x001624a0)))) {
            sub_001f93e0(L18_ADDR(0x002f9780), lwu(rec + 0x178));
            if (!(frame() < lw(L18_ADDR(0x0016249c)))) {
                swc1(L18_ADDR(0x00162520), 10.0f);
            } else {
                const float t = window_part(frame(), lw(L18_ADDR(0x00162498)), lw(L18_ADDR(0x0016249c)));
                const float lo = lwc1(L18_ADDR(0x00162490));
                swc1(L18_ADDR(0x00162520), fpu_add(lo, fpu_mul(fpu_sub(lwc1(L18_ADDR(0x00162494)), lo), t)));
            }
        }
    }

    if (lw(rec + 0x30) == 1 && lwu(rec + 0x184) != 0) sub_001f93e0(L18_ADDR(0x002f9a98), lwu(rec + 0x184));  // (0x002f92d0)

    if (lw(rec + 0x30) == 5 && !(frame() < fs(0x8a)) && !(fs(0x108) < frame()) && lwu(rec + 0x180) != 0) {  // ---- scene 5 (0x002f9300)
        const uint32_t o = lwu(rec + 0x180);
        if (fs(0x8c) < frame()) {  // two trails of sparks from where joints 0 / 2 were to where they are
            const uint32_t now0 = sp + 0x20, now2 = sp + 0x30, at0 = sp + 0x50, at2 = sp + 0x60;
            moby_chain_point_v(o, 0, V(now0));
            moby_chain_point_v(o, 2, V(now2));
            for (int32_t i = 0; i < 12; ++i) {
                const float t = fpu_div(int_to_float(i), 12.0f);
                vec3_lerp(V(at0), V(now0), V(L18_ADDR(0x00162530)), t);
                vec3_lerp(V(at2), V(now2), V(L18_ADDR(0x00162540)), t);
                vf_scene_sparks_l18(at0, at0, zero(), lwc1(L18_ADDR(0x001624d0)), fpu_mul(lwc1(L18_ADDR(0x001624d0)), 0.4000000059604645f));
                vf_scene_sparks_l18(at2, at2, zero(), lwc1(L18_ADDR(0x001624d0)), fpu_mul(lwc1(L18_ADDR(0x001624d0)), 0.4000000059604645f));
            }
            lq_sq(L18_ADDR(0x00162530), now0);
            lq_sq(L18_ADDR(0x00162540), now2);
        } else {
            moby_chain_point_v(o, 0, V(L18_ADDR(0x00162530)));
            moby_chain_point_v(o, 2, V(L18_ADDR(0x00162540)));
        }
    }

    if (lw(rec + 0x30) == 1 && !(frame() < fs(0x1be)) && frame() < fs(0x1f6) && lwu(rec + 0x188) != 0) {  // ---- scene 1's beam (0x002f9480)
        const uint32_t o = lwu(rec + 0x188);
        const uint32_t pv = lwu(m + 0x78);
        const uint32_t from = sp + 0x20, to = sp + 0x30, dir = sp + 0x40;
        sw(pv + 0x10, 0);
        moby_chain_point_v(o, 3, V(from));
        moby_chain_point_v(o, 0xa, V(to));
        vec3_sub(V(dir), V(to), V(from));
        vec3_set_length(V(m + 0xd0), V(dir), 1.0f);
        lq_sq(pv, to);
        swc1(pv + 0x48, atan2_f(lwc1(m + 0xd0), lwc1(m + 0xd4)));
        swc1(pv + 0x4c, atan2_f(vec2_length(V(m + 0xd0)), lwc1(m + 0xd8)));
        const float speed = lwc1(L18_ADDR(0x0016174c));  // (the morph's speed, 24 for this one)
        swc1(L18_ADDR(0x0016174c), 24.0f);
        morph_2bd0a0(m);
        swc1(L18_ADDR(0x0016174c), speed);
        sw(L18_ADDR(0x001617f4), NOT_ADDR(0x00207f7fu));
        sw(L18_ADDR(0x001617f8), 0x007f7f20u);
    }

    // ---- scenes 0 .. 3 (0x002f9598): the draw L18:0x2f9a98 on the scene's moby +0x180 (scene 3 before its frame 0x24a)
    const int32_t scene = lw(rec + 0x30);
    if (scene == 0 || scene == 2 || (scene == 3 && frame() < fs(0x24a))) {
        if (lwu(rec + 0x180) != 0) sub_001f93e0(L18_ADDR(0x002f9a98), lwu(rec + 0x180));
    } else if (scene != 3) {
        return;
    }
    if (lw(rec + 0x30) != 3 || frame() != fs(0x24a)) return;
    // scene 3 at its frame 0x24a (0x002f9620): a ring of 32 white particles before the camera
    const uint32_t at = sp + 0x20, arm = sp + 0x30, vel = sp + 0x40;
    mat3_transform(V(at), V(L18_ADDR(0x001624e0)), M(L18_ADDR(0x001679d0)));
    vec3_add(V(at), V(at), V(L18_ADDR(0x001677c0)));
    lq_sq(arm, L18_ADDR(0x001679e0));
    for (int32_t k = 0; k < 32; ++k) {
        vec_rotate_axis(V(vel), V(arm), V(L18_ADDR(0x001679d0)), fpu_mul(int_to_float(k), 0.19634954631328583f));
        vec3_set_length(V(vel), V(vel), fpu_mul(lwc1(L18_ADDR(0x001624f0)), lwc1(0x0015ed60)));
        const int32_t spin = random_spin();
        const uint32_t p = spawn_particle_17(at, spin, vel, 0x7f7f7f7fu, 0.0f, 1.0f, u2f(0x3f8147aeu), lwc1(L18_ADDR(0x001624f4)));  // (1.01)
        if (p != 0) spark_finish(p, 0x3c);
    }
}

}  // namespace rac1
