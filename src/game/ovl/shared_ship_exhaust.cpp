// The exhaust of the ships the hero rides on Pokitaru (level 11, class 1242) and Drek's fleet (17, class 1379). One port of
// both levels' copies, the same code relinked: the flames' looks (five records of 0x10 bytes) and the speed the exhaust is
// scaled by are the level's own data (per level below); the frame time 0x15ed6c is the base ELF's; the flames are the
// shared particle_4a_spawn (the levels' copies L11:0x29a738 / L17:0x26f1a8). Header shared_ship_exhaust.h.
#include <game/ovl/shared_ship_exhaust.h>

#include <game/math/vecmath.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_particle_4a.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the speed the exhaust's length is measured against
float full_speed() { return on_level(VELDIN_ORBIT) ? lwc1(L17_ADDR(0x00162004)) : lwc1(L11_ADDR(0x00161ff4)); }
// the flames' looks: five records of 0x10 bytes
uint32_t flame_record(uint32_t k) { return (on_level(VELDIN_ORBIT) ? L17_ADDR(0x00162198) : L11_ADDR(0x001621c8)) + k * 0x10; }

// how far a flame is blown by the thrust: twice, three or four times it
enum class Blow { Twice, Thrice, Four };
float blow(float thrust, Blow b) {
    switch (b) {
    case Blow::Twice: return fpu_add(thrust, thrust);
    case Blow::Thrice: return fpu_mul(thrust, 3.0f);
    default: return fpu_mul(thrust, 4.0f);
    }
}
}  // namespace

// orig: L11:0x003112b8 L17:0x002ea688
void ship_exhaust_spawn(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t vel = sp, at = sp + 0x10, a = sp + 0x20, b = sp + 0x30, c = sp + 0x40;
    const float small = u2f(0x3ca3d70au), tiny = u2f(0x3c23d70au);  // (0.02, 0.01)
    // a flame at `pos`: its speed back along x 6 .. 7 plus the thrust's share, a second's worth, times the speed's share
    auto flame = [m, pv, vel](uint32_t pos, uint32_t look, Blow kind, float share) {
        const float r = rand_range_f(6.0f, 7.0f);
        const float push = blow(lwc1(pv + 0xc4), kind);
        swc1(vel, fpu_mul(-fpu_mul(fpu_add(r, push), lwc1(0x0015ed6c)), share));
        particle_4a_spawn(m, pos, vel, look, 1);
    };
    for (uint32_t joint = 1; joint < 3; ++joint) {
        const float speed = lwc1(pv + 0x64);
        V(vel) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        float share = fpu_div(speed, full_speed());
        if (1.0f < share) share = 1.0f;
        else if (share < 0.35f) share = 0.35f;
        moby_chain_point_v(m, joint, V(at));
        sw(at + 0xc, 0);
        lq_sq(a, at);
        lq_sq(b, at);
        lq_sq(c, at);
        metal_25f8e0(a, fpu_add(fpu_mul(lwc1(pv + 0xc4), small), small));
        metal_25f8e0(b, fpu_add(fpu_mul(lwc1(pv + 0xc4), small), small));
        metal_25f8e0(c, fpu_add(fpu_mul(lwc1(pv + 0xc4), tiny), small));
        flame(a, flame_record(0), Blow::Twice, share);
        flame(b, flame_record(1), Blow::Thrice, share);
        flame(c, flame_record(2), Blow::Four, share);
        if (!(0.5f < lwc1(pv + 0xc4))) continue;
        metal_25f8e0(a, fpu_add(fpu_mul(lwc1(pv + 0xc4), small), tiny));
        metal_25f8e0(b, fpu_add(fpu_mul(lwc1(pv + 0xc4), small), tiny));
        metal_25f8e0(c, fpu_add(fpu_mul(lwc1(pv + 0xc4), tiny), tiny));
        flame(a, flame_record(0), Blow::Twice, share);
        flame(b, flame_record(1), Blow::Thrice, share);
        flame(c, flame_record(2), Blow::Four, share);
        flame(a, flame_record(3), Blow::Twice, share);
        flame(at, flame_record(4), Blow::Thrice, share);
    }
    ee_stack_free(0x130);
}

}  // namespace rac1
