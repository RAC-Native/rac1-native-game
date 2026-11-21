// Two particle types' updates: 0x31, the bullets of Kerwan's helicopter (Veldin's copy, run by every level through
// veldin_extras.cpp's registration, and Rilgar's own copy L5:0x29bda0, its race HUD's), and 0x21 / 0x3f (two copies on
// Veldin), particles that only live their life out. A particle's record: +0xa its life (s16), +0x10 its place, +0x20 its
// tail, +0x30 its velocity, +0x3c the moby it does not hit. The hit point 0x173e60 goes through OV(). Header
// shared_particles.h.
#include <game/ovl/shared_particles.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
// the particle's life ticked; true when it is out (then freed)
bool life_out(uint32_t p) {
    int16_t life = static_cast<int16_t>(lh(p + 0xa));
    const int32_t ticked = timer_tick_i16(life);
    sh(p + 0xa, static_cast<uint32_t>(life));
    if (ticked == 0) return false;
    free_particle(p);
    return true;
}
}  // namespace

// orig: 0x00271d20 L5:0x0029bda0
void bullet_particle_update(uint32_t p) {
    const Vec4 vel{lwc1(p + 0x30), lwc1(p + 0x34), lwc1(p + 0x38), 0.0f};
    vec3_add(V(p + 0x10), V(p + 0x10), vel);
    vec3_add(V(p + 0x20), V(p + 0x20), vel);
    if (life_out(p)) return;
    if (segment_collide_v(V(p + 0x20), V(p + 0x10), 0x10, lwu(p + 0x3c), nullptr) == 0) return;
    sh(p + 0xa, 0);  // (it hit: its last frame, at the hit point)
    V(p + 0x10) = V(OV(0x00173e60));
}

// orig: 0x0026f048 0x002738b0
void timed_particle_update(uint32_t p) {
    life_out(p);
}

}  // namespace rac1
