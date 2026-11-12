// Novalis's (level 1) variants of the hero controller functions: see variants_l1.h. Each one is
// the function at Novalis's own address; the Veldin function of the same name dispatches to it
// while the translation target is Novalis's overlay (`on_novalis()`).
#ifndef RAC1_GAME_OVL_HERO_L1_H
#define RAC1_GAME_OVL_HERO_L1_H

#include <cstdint>

#include <game/math/vecmath.h>
#include <game/ovl/hero_hoverboard.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>

namespace rac1 {

// Playable Clank (mode 1: levels 0, 4, 6, 7, 9, 10, 13, 17).
inline bool hero_has_clank() {
    return !on_novalis() && !on_aridia() && !on_kerwan() && !on_rilgar() && !on_batalia() &&
           !on_pokitaru() && !on_hoven() && !on_oltanis() && !on_quartu() && !on_kalebo() &&
           !on_veldin_finale();
}

// Giant Clank (mode 2: levels 0, 4, 7, 9, 10, 13, 15, 18).
inline bool hero_has_giant_clank() {
    return !on_novalis() && !on_aridia() && !on_kerwan() && !on_rilgar() && !on_blarg() &&
           !on_batalia() && !on_pokitaru() && !on_hoven() && !on_oltanis() && !on_kalebo() &&
           !on_veldin_orbit();
}

// Swingshot states 0x24..0x26, 0x2c (all levels except Novalis).
inline bool hero_has_swingshot() {
    return !on_novalis();
}

// Warp-out / sink state 0x7f..0x80 (all levels except Quartu and Veldin finale).
inline bool hero_has_sink_7f() {
    return !on_quartu() && !on_veldin_finale();
}

// Water swimming / wading / drowning states 0x33..0x37, 0x6a, 0x73, 0x75, 0x76, 0x82
// (all levels except Aridia, Blarg, and Oltanis).
inline bool hero_has_water() {
    return !on_aridia() && !on_blarg() && !on_oltanis();
}

// Slippery surface slide state 0x31 (all levels except Aridia, Blarg, Pokitaru, Hoven, Oltanis).
inline bool hero_has_ice_slide() {
    return !on_aridia() && !on_blarg() && !on_pokitaru() && !on_hoven() && !on_oltanis();
}

// Scripted wrench pose state 0x3b (all levels except Aridia, Rilgar, Blarg, Pokitaru, Hoven, Oltanis).
inline bool hero_has_state_3b() {
    return !on_aridia() && !on_rilgar() && !on_blarg() && !on_pokitaru() && !on_hoven() &&
           !on_oltanis();
}

// Magneboots / beam states 0x3f, 0x70, 0x71 (all levels except 1, 3, 5, 6, 11, 15).
inline bool hero_has_beam() {
    return !on_novalis() && !on_kerwan() && !on_rilgar() && !on_blarg() && !on_pokitaru() &&
           !on_quartu();
}

// Grind rail states 0x28..0x2b, 0x42 (levels 0, 4, 6, 7, 8, 9, 10, 13, 14, 16, 17, 18).
inline bool hero_has_grind() {
    return !on_novalis() && !on_aridia() && !on_kerwan() && !on_rilgar() && !on_pokitaru() &&
           !on_hoven() && !on_quartu();
}

// Lava / quicksand death states 0x68, 0x69, 0x7b (levels 0, 2, 4, 5, 7, 9, 10, 13, 15, 17, 18).
inline bool hero_has_lava() {
    return !on_novalis() && !on_kerwan() && !on_blarg() && !on_batalia() && !on_pokitaru() &&
           !on_hoven() && !on_oltanis() && !on_kalebo();
}

// Quicksand surface kind 0xb probe / flag (all levels except Blarg and Hoven).
inline bool hero_has_quicksand() {
    return !on_blarg() && !on_hoven();
}

// Hydro-Pack states 0x38..0x3a (levels 0, 4, 5, 7, 9, 10, 11, 12, 13, 17, 18).
inline bool hero_has_hydropack() {
    return !on_novalis() && !on_aridia() && !on_kerwan() && !on_blarg() && !on_batalia() &&
           !on_oltanis() && !on_quartu() && !on_kalebo();
}

// Deadly-water drowning state 0x7c (levels 0, 4, 5, 6, 7, 9, 10, 13, 17).
inline bool hero_has_state_7c() {
    return !on_novalis() && !on_aridia() && !on_kerwan() && !on_batalia() && !on_pokitaru() &&
           !on_hoven() && !on_oltanis() && !on_quartu() && !on_kalebo() && !on_veldin_finale();
}

// Ice-skating / slick-footing state 0x2f (levels 0, 4, 7, 9, 10, 12, 13, 14, 17).
inline bool hero_has_skate() {
    return !on_novalis() && !on_aridia() && !on_kerwan() && !on_rilgar() && !on_blarg() &&
           !on_batalia() && !on_pokitaru() && !on_quartu() && !on_kalebo() && !on_veldin_finale();
}

// Overhead rail-hang state 0x74 (levels 0, 3, 4, 7, 9, 10, 13, 17).
inline bool hero_has_state_74() {
    return !on_novalis() && !on_aridia() && !on_rilgar() && !on_blarg() && !on_batalia() &&
           !on_pokitaru() && !on_hoven() && !on_oltanis() && !on_quartu() && !on_kalebo() &&
           !on_veldin_finale();
}

// Hoverboard race states 0x6b..0x6f (levels 5, 16).
inline bool hero_has_hoverboard() {
    return on_rilgar() || on_kalebo();
}

// Kalebo state 0x3e (level 16).
inline bool hero_has_state_3e() {
    return on_kalebo();
}

void hero_mobys_set_flag1_l1();    // L1:0x002488e8 (Veldin 0x233210)
void hero_mobys_clear_flag1_l1();  // L1:0x00248900 (Veldin 0x233248)

uint32_t sub_00205000_l1(int32_t flag);                        // L1:0x00226f10 (Veldin 0x205000)
void hero_footstep_l1(int32_t a, uint32_t foot, int32_t sound);  // L1:0x00227e48 (Veldin 0x205f60)
void sub_002076a8_l1();                                        // L1:0x00228e38 (Veldin 0x2076a8)
void hero_frame_update_l1();                                   // L1:0x00228870 (Veldin 0x2070d0)

void hero_surface_update_l1();  // L1:0x0022cd48 (Veldin 0x20b960)

int32_t hero_ledge_check_v_l1(const Vec4& pos, const float* ref_yaw);  // L1:0x0022d838 (Veldin 0x20c758)

void hero_ledge_grab_check_l1();  // L1:0x0022d090 (Veldin 0x20bed0)

void hero_mode_enter_l1(uint32_t mode, uint32_t state, uint32_t moby);  // L1:0x00231348 (Veldin 0x210a08)

void hero_mode_exit_l1();  // L1:0x00231450 (Veldin 0x210b30)

int32_t hero_hurt_l1(int32_t knock);  // L1:0x00231580 (Veldin 0x210ce8)

void hero_gait_update_l1();  // L1:0x00231ae0 (Veldin 0x211380)

void hero_stick_speed_l1();  // L1:0x00232290 (Veldin 0x211be8)

void hero_wall_slide_l1(int32_t facing);  // L1:0x00232978 (Veldin 0x212318)

void hero_ground_update_l1();  // L1:0x00232dc0 (Veldin 0x2127b8)

int32_t hero_collide_push_l1(int32_t mode);  // L1:0x00233940 (Veldin 0x2133a8)

void hero_move_integrate_l1();  // L1:0x00233de0 (Veldin 0x213880)

void sub_00213e68_l1(float dt);  // L1:0x00234358 (Veldin 0x213e68)

void hero_horizontal_velocity_l1();  // L1:0x00234b40 (Veldin 0x214658)

void hero_timers_update_l1();  // L1:0x0023c710 (Veldin 0x2215c8)

void hero_enter_idle_l1();  // L1:0x002405a0 (Veldin 0x227638)

void hero_land_l1();  // L1:0x00242198 (Veldin 0x2293a8)

int32_t hero_item_use_l1();  // L1:0x00240ed8 L2:0x0022ee00 (Veldin 0x227fa0)
void hero_item_swingshot(uint32_t mask);  // (helper) Veldin's item_swingshot (hero_input.cpp), Aridia's build's case 0xc

void hero_grind_update_v();  // (helper) Veldin's state 0x74 update block (hero_state_update.cpp), Kerwan's L3:0x00213874
void hero_state_update_l1();  // L1:0x002370b8 (Veldin 0x217970)

int32_t hero_set_state_l1(uint32_t state, int32_t mode);  // L1:0x0023cf98 (Veldin 0x2223f8)

void hero_input_update_l1();  // L1:0x00242930 (Veldin 0x229b70)

}  // namespace rac1

#endif
