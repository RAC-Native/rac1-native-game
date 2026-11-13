// Novalis's (level 1) own moby classes, part 2: Birdbot, Beehive, the teleporter, the jets, the ship,
// the amoeboids, the gold bolt / crank / cases, the small classes. Same conventions as novalis_mobys.h
// (`// orig: L1:0x00xxxxxx`, registered per level).
#ifndef RAC1_GAME_OVL_NOVALIS_MOBYS2_H
#define RAC1_GAME_OVL_NOVALIS_MOBYS2_H

#include <cstdint>

#include <game/ovl/novalis_mobys.h>

namespace rac1 {

// The uniform ABI of the transliterated helpers: every argument register the
// original can read ($a0..$a3, $t0..$t3, $f12..$f15), the results $v0 / $f0.
struct TrRet {
    uint32_t v0;
    float f0;
};

// ---- the gold weapon cases (novalis_cases.cpp)
void gold_case_update_l1(uint32_t m);                        // L1:0x002e1ac0  classes 304, 1456..1465

// ---- the teleporter pads (novalis_teleporter.cpp)
void moby_group_set_state(int32_t group, uint32_t state);     // L1:0x0026e0e0 (and its copies on levels 5-8, 11-13, 15-18)
void teleporter_trigger_l1(uint32_t m);                        // L1:0x00308b28
void teleporter_pieces_update(uint32_t m);                     // L1:0x003092d0 (and its copies on levels 7, 8, 11-13, 16-18)
void teleporter_hint_l1(uint32_t m);                           // L1:0x00309430
void teleporter_hide_l1(uint32_t m);                           // L1:0x003097e8
void teleporter_show_l1(uint32_t m);                           // L1:0x00309838
void teleporter_update_l1(uint32_t m);                         // L1:0x00308bd8  class 1135

// ---- the path follower library (novalis_path.cpp)
void path_follower_init_l1(uint32_t f);                                          // L1:0x0028b410 L3:0x00264458 L4:0x00268b18 L12:0x0028eb00 L14:0x0027df78
int32_t path_nearest_l1(uint32_t at, uint32_t path, float bias);                 // L1:0x0028b510 L3:0x00264558 L4:0x00268c18 L5:0x002a0260 L7:0x0029e610 L8:0x002806a8 L9:0x00294a10 L12:0x0028ec00 L13:0x00281d70 L14:0x0027e078 L16:0x00271250
uint32_t path_point_l1(uint32_t f, int32_t i);                                   // L1:0x0028b7b0
int32_t path_index_wrap_l1(uint32_t f, int32_t i);                               // L1:0x0028b828
int32_t path_index_advance_l1(uint32_t f);                                       // L1:0x0028b878
void path_spline_tangent_l1(uint32_t out, uint32_t f, int32_t idx, float a, float b);  // L1:0x0028b8c8
void hermite_l1(uint32_t out, uint32_t p0, uint32_t p1, uint32_t m0, uint32_t m1, float t);  // L1:0x0028ba80
float path_measure_l1(uint32_t f, uint32_t debug, uint32_t color, float fa, float fb);  // L1:0x0028bb90
uint32_t debug_particle_l1(uint32_t pos, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, int32_t t1, uint32_t t2, float f12);  // L1:0x00285768 L2:0x00271d80 L3:0x0025e9e0 L4:0x00263098 L6:0x0027f648 L10:0x002634e8 L14:0x00278390

// ---- the aircraft base (novalis_jet_base.cpp)
void jet_banking_l1(uint32_t m, uint32_t out);                // L1:0x002f5040
void smoke_particle_l1(uint32_t pos, uint32_t vel);           // L1:0x0027f660
uint32_t jet_debris_spawn_l1(uint32_t m, uint32_t dir, float scale);  // L1:0x0030be70
void jet_update_base_l1(uint32_t m);                          // L1:0x002f5168
void jet_fighter_update_l1(uint32_t m);                       // L1:0x002f4428  class 660
uint32_t bomber_shot_spawn_l1(uint32_t m, uint32_t pos, uint32_t dir, uint32_t target, uint32_t flag);  // L1:0x002f68a0
void jet_bomber_update_l1(uint32_t m);                        // L1:0x002f7728  class 688
void jet_ship_update_l1(uint32_t m);                          // L1:0x002f4960  class 666

// ---- the Amoeboids (novalis_amoeboid.cpp)
uint32_t nov_276c40_l1(int32_t path, uint32_t want, uint32_t have);                                // L1:0x00276c40 L5:0x0028be08 L6:0x00270af8 L7:0x0028a3d0 L10:0x00254a78 L11:0x00287340
uint32_t nov_276a48_l1(uint32_t walls, int32_t n, int32_t path, uint32_t pos, float radius);       // L1:0x00276a48 L5:0x0028bc10 L6:0x00270900 L7:0x0028a1d8 L10:0x00254880 L11:0x00287148
uint32_t nov_276fe8_l1(uint32_t walls, int32_t n, int32_t path, uint32_t from, uint32_t to, uint32_t out, float radius);  // L1:0x00276fe8 L5:0x0028c5f8 L6:0x002712e8 L7:0x0028a910 L10:0x00255268 L11:0x00287b30
uint32_t nov_276820_l1(int32_t path, uint32_t from, uint32_t to, uint32_t out);                   // L1:0x00276820
uint32_t nov_287158_l1(uint32_t pos, uint32_t a, uint32_t b, int32_t life, float size, float f);  // L1:0x00287158 L5:0x0029c180 L10:0x00264ed8 L11:0x002972c8 L12:0x0028a7d0 L13:0x0027da80
uint32_t nov_2ef350_l1(uint32_t m);                                                                // L1:0x002ef350
uint32_t nov_2ef478_l1(uint32_t m, uint32_t group);                                                // L1:0x002ef478
void nov_2ef770_l1(uint32_t m, uint32_t vel);                                                      // L1:0x002ef770
void nov_2ef560_l1(uint32_t m);                                                                    // L1:0x002ef560
void nov_2eec68_l1(uint32_t m);                                                                    // L1:0x002eec68
void amoeboid_update_l1(uint32_t m);                                                               // L1:0x002edca0  classes 572 / 865 / 866
// (L1:0x002747a0 is Veldin's metal_25f8e0 and L1:0x00305260 Veldin's carry_update 0x2dc9f0: the same code, called directly)

// ---- the Birdbot (novalis_birdbot.cpp)
float moby_anim_scalar_l1(uint32_t m);                         // L1:0x00263920
void moby_extent_set_l1(uint32_t m, float z);                  // L1:0x0026eff8
uint32_t bird_shot_spawn_l1(uint32_t m, uint32_t pos, uint32_t dir, uint32_t life);  // L1:0x002fa068
void bird_sparks_l1(uint32_t m, uint32_t from);                // L1:0x002e6518
uint32_t bird_path_pick_l1(uint32_t m, uint32_t idx);          // L1:0x002e6790
void birdbot_update_l1(uint32_t m);                            // L1:0x002e6bf0  class 459

// ---- the beehive (novalis_hive.cpp)
int32_t moby_group_count_l1(int32_t group, int32_t state);  // L1:0x0026e008 L6:0x00267ee8 L9:0x00278c78 L11:0x0027f058 L12:0x00271ff8 L13:0x00264f78 L14:0x00261728 L15:0x002489f0 L16:0x00254900
uint32_t bee_spawn_l1(uint32_t m);                          // L1:0x00302328
void beehive_update_l1(uint32_t m);                         // L1:0x003021b8  class 815

// ---- novalis_classes_tr (transliterated: the translator)
TrRet nov_2f8530_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002f8530
void nov_class700_update_l1(uint32_t a0);  // L1:0x002f8c58
void nov_class679_update_l1(uint32_t a0);  // L1:0x002f6328
TrRet nov_27e900_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L1:0x0027e900
void nov_class27_update_l1(uint32_t a0);  // L1:0x002bd100
TrRet nov_2f9000_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002f9000
void nov_class701_update_l1(uint32_t a0);  // L1:0x002f9080
void nov_class704_update_l1(uint32_t a0);  // L1:0x002f9810
void nov_class761_update_l1(uint32_t a0);  // L1:0x002feb58
void nov_class729_update_l1(uint32_t a0);  // L1:0x002fa800
TrRet nov_2a2450_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002a2450
TrRet nov_2a2480_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002a2480
TrRet nov_2fac80_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002fac80
void nov_class730_update_l1(uint32_t a0);  // L1:0x002fad68
void nov_class1504_update_l1(uint32_t a0);  // L1:0x0030b618
void nov_class613_update_l1(uint32_t a0);  // L1:0x002f3120
void nov_class709_update_l1(uint32_t a0);  // L1:0x002f9d80
void nov_class1848_update_l1(uint32_t a0);  // L1:0x0030f208
void nov_class604_update_l1(uint32_t a0);  // L1:0x002f2b68
TrRet nov_2ffa90_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002ffa90
void nov_class778_update_l1(uint32_t a0);  // L1:0x002ff860
TrRet nov_225a88_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00225a88
TrRet nov_2cb458_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002cb458
TrRet nov_317e70_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00317e70
TrRet nov_2cb788_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002cb788
void nov_class832_update_l1(uint32_t a0);  // L1:0x00302648
TrRet nov_27e538_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0027e538
void nov_class567_update_l1(uint32_t a0);  // L1:0x002eda00
TrRet nov_2fa1b0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002fa1b0
void nov_class722_update_l1(uint32_t a0);  // L1:0x002fa4c0
void nov_class779_update_l1(uint32_t a0);  // L1:0x002ffb28
void nov_class787_update_l1(uint32_t a0);  // L1:0x002ffdc0
TrRet nov_280970_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00280970
TrRet nov_2c72c0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002c72c0
TrRet nov_2c72c8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002c72c8
void nov_class156_update_l1(uint32_t a0);  // L1:0x002c7530
void nov_class1546_update_l1(uint32_t a0);  // L1:0x0030c190
TrRet nov_27f2b0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0027f2b0
void nov_class686_update_l1(uint32_t a0);  // L1:0x002f6a30
void nov_class696_update_l1(uint32_t a0);  // L1:0x002f8718
void nov_class1510_update_l1(uint32_t a0);  // L1:0x0030ba18
void nov_class1633_update_l1(uint32_t a0);  // L1:0x0030c9a8
TrRet nov_287b00_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00287b00
TrRet nov_287c80_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00287c80
TrRet nov_2b7a48_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002b7a48
TrRet nov_2b7c68_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002b7c68
TrRet nov_2b7ef0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002b7ef0
TrRet nov_261d78_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00261d78
TrRet nov_261c10_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00261c10
TrRet nov_262038_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00262038
TrRet nov_2b7f30_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002b7f30
TrRet nov_2b7fe0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002b7fe0
TrRet nov_2b8288_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002b8288
TrRet nov_2ffcd0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002ffcd0
void nov_class751_update_l1(uint32_t a0);  // L1:0x002fd0e8
TrRet nov_26de80_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0026de80
TrRet nov_26eec8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0026eec8
TrRet nov_287328_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00287328
TrRet nov_30d208_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030d208
TrRet nov_30d260_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030d260
TrRet nov_30c898_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030c898
TrRet nov_30d308_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L1:0x0030d308
TrRet nov_30d5f0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030d5f0
TrRet nov_30d880_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030d880
TrRet nov_28a7a8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0028a7a8
TrRet nov_30dda0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030dda0
TrRet nov_30ded0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x0030ded0
void nov_class1818_update_l1(uint32_t a0);  // L1:0x0030df40
TrRet nov_263a28_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00263a28
TrRet nov_2e30b8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002e30b8
TrRet nov_2e3110_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002e3110
TrRet nov_2e3208_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002e3208
TrRet nov_2e32a8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L1:0x002e32a8
TrRet nov_2e35a8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002e35a8
TrRet nov_2e3830_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002e3830
void nov_class437_update_l1(uint32_t a0);  // L1:0x002e3f20
TrRet nov_2e0bd8_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x002e0bd8
TrRet nov_316e28_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00316e28
TrRet nov_316e88_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00316e88
void nov_class280_update_l1(uint32_t a0);  // L1:0x002e0c68
TrRet nov_231450_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00231450
TrRet nov_308550_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00308550
TrRet nov_3089f0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x003089f0
TrRet nov_308380_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00308380
TrRet nov_308470_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x00308470
TrRet nov_3087e0_l1(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15);  // L1:0x003087e0
void nov_class1134_update_l1(uint32_t a0);  // L1:0x00307ca0

// orig: (none) -- the registrations of this part (called by novalis_moby_fns_register)
void novalis_moby_fns_register2();

}  // namespace rac1

#endif
