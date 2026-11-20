// Veldin's finale (level 18) class 582: the switch of the level's grind rails (one moby). The level lays each stretch of
// rail twice, one way and the other (a rail's path starts where its twin's ends); this moby pairs them at the start and
// then, every frame, opens the twin whose start is the nearer to the hero and closes the other (a rail is closed by its
// radius +0xc at 0), leaving a pair alone while the hero grinds on it. At the start it also puts two Nanotech pickups
// (class 806) at the centres of two of the level's boxes. Level 18 only: the addresses are the finale's (`orig: L18:`);
// the rails and the boxes come from the level-data facade, the hero's words are the base ELF's. Registered in
// veldin_finale_mobys.cpp.
//
// The pvars: +0 .. +0x1c four pairs of rail records (each the rail, then its twin), +0x20 .. +0x3c their radii,
// +0x40 / +0x44 the boxes of the two pickups.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position, +0x560 the path he grinds, +0x208c his move: 0xf grinding)

// an EE stack frame of the original's size
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// a rail's path's first point and its last
uint32_t path_first(uint32_t rail) { return lwu(rail + 0x10) + 0x10; }
uint32_t path_last(uint32_t rail) {
    const uint32_t path = lwu(rail + 0x10);
    return path + (lwu(path) << 4);
}
}  // namespace

// A Nanotech pickup (class 806) at `at`: shown (+0x30 0x40), its pvars +0xc itself, unturned. Returns it (0: none).
// orig: L18:0x002e22c0
uint32_t vf_nanotech_spawn_l18(uint32_t at) {
    const Frame f(48);
    const uint32_t c = moby_spawn(0x326);
    if (c == 0) return 0;
    sb(c + 0x30, 0x40);
    sb(c + 0x20, 0);
    sw(lwu(c + 0x78) + 0xc, c);
    sw(c + 0x94, 0);
    sh(c + 0x34, lhu(c + 0x34) | 0x41);
    lq_sq(c + 0x10, at);
    vec_clear(V(c + 0x40));
    euler_to_mat3_vu(M(c + 0xc0), V(c + 0x40));
    return c;
}

// State 0 pairs the rails (open ones, a rail with the first rail whose path ends within 0.5 of where its own starts; both
// closed once paired; the scan runs again until four pairs are made), spawns the two pickups and goes to state 1; state 1
// opens and closes the pairs.
// orig: L18:0x002d62e8
void vf_class582_update_l18(uint32_t m) {
    const Frame f(208);
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        int32_t pairs = 0;
        do {
            for (int32_t i = 0; i < lvd::rail_count(); ++i) {
                const uint32_t rail = lvd::rail(i);
                if (lwc1(rail + 0xc) == 0.0f) continue;
                const uint32_t slot = pv + static_cast<uint32_t>(pairs) * 8;
                sw(slot, rail);
                swc1(slot + 0x20, lwc1(rail + 0xc));
                for (int32_t j = 0; j < lvd::rail_count(); ++j) {
                    const uint32_t twin = lvd::rail(j);
                    if (lwc1(twin + 0xc) == 0.0f) continue;
                    if (!(vec3_distance(V(path_first(rail)), V(path_last(twin))) < 0.5f)) continue;
                    ++pairs;
                    sw(slot + 4, twin);
                    swc1(slot + 0x24, lwc1(twin + 0xc));
                    sw(twin + 0xc, 0);
                    sw(rail + 0xc, 0);
                    break;
                }
            }
        } while (pairs < 4);
        for (uint32_t k = 0; k < 2; ++k) vf_nanotech_spawn_l18(lvd::box(lvd::BOXES, lw(pv + 0x40 + k * 4)) + 0x30);
        return;
    }
    case 1:
        sh(HERO + 0x1ca, 5);
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t slot = pv + k * 8;
            if (lw(HERO + 0x208c) == 0xf) {
                const uint32_t on = lwu(HERO + 0x560);
                if (on == lwu(lwu(slot) + 0x10) || on == lwu(lwu(slot + 4) + 0x10)) continue;
            }
            const float to_first = vec2_distance(V(HERO + 0x80), V(path_first(lwu(slot))));
            const float to_twin = vec2_distance(V(HERO + 0x80), V(path_first(lwu(slot + 4))));
            if (to_first < to_twin) {
                swc1(lwu(slot) + 0xc, lwc1(slot + 0x20));
                sw(lwu(slot + 4) + 0xc, 0);
            } else {
                sw(lwu(slot) + 0xc, 0);
                swc1(lwu(slot + 4) + 0xc, lwc1(slot + 0x24));
            }
        }
        return;
    default:
        return;
    }
}

}  // namespace rac1
