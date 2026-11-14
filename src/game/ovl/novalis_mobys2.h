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
void gold_case_update_l1(uint32_t m);                        // L1:0x002e1ac0  classes 304, 1456..1465 (L13:0x002e7238)

// ---- the teleporter pads (novalis_teleporter.cpp)
void moby_group_set_state(int32_t group, uint32_t state);     // L1:0x0026e0e0 (and its copies on levels 5-8, 11-13, 15-18)
void teleporter_trigger_l1(uint32_t m);                        // L1:0x00308b28 (and its copies on levels 7, 8, 12, 13, 16-18)
void teleporter_pieces_update(uint32_t m);                     // L1:0x003092d0 (and its copies on levels 7, 8, 11-13, 16-18)
void teleporter_hint_l1(uint32_t m);                           // L1:0x00309430 (and its copies on levels 7, 8, 12, 13, 16-18)
void teleporter_hide_l1(uint32_t m);                           // L1:0x003097e8 (and its copies on levels 7, 8, 12, 13, 16-18)
void teleporter_show_l1(uint32_t m);                           // L1:0x00309838 (and its copies on levels 7, 8, 12, 13, 16-18)
void teleporter_update_l1(uint32_t m);                         // L1:0x00308bd8  class 1135 (and its copies on levels 7, 8, 12, 13, 16-18)

// ---- the path follower library (novalis_path.cpp)
void path_follower_init_l1(uint32_t f);                                          // L1:0x0028b410 L3:0x00264458 L4:0x00268b18 L12:0x0028eb00 L14:0x0027df78
int32_t path_nearest_l1(uint32_t at, uint32_t path, float bias);                 // L1:0x0028b510 L3:0x00264558 L4:0x00268c18 L5:0x002a0260 L7:0x0029e610 L8:0x002806a8 L9:0x00294a10 L12:0x0028ec00 L13:0x00281d70 L14:0x0027e078 L16:0x00271250
uint32_t path_point_l1(uint32_t f, int32_t i);                                   // L1:0x0028b7b0 L3:0x00264660 L4:0x00268d20 L14:0x0027e180
int32_t path_index_wrap_l1(uint32_t f, int32_t i);                               // L1:0x0028b828 L3:0x002646d8 L4:0x00268d98 L14:0x0027e1f8
int32_t path_index_advance_l1(uint32_t f);                                       // L1:0x0028b878 L3:0x00264728 L4:0x00268de8 L14:0x0027e248
void path_spline_tangent_l1(uint32_t out, uint32_t f, int32_t idx, float a, float b);  // L1:0x0028b8c8 L3:0x00264778 L4:0x00268e38 L14:0x0027e298
void hermite_l1(uint32_t out, uint32_t p0, uint32_t p1, uint32_t m0, uint32_t m1, float t);  // L1:0x0028ba80 L3:0x00264930 L4:0x00268ff0 L14:0x0027e450
float path_measure_l1(uint32_t f, uint32_t debug, uint32_t color, float fa, float fb);  // L1:0x0028bb90 L3:0x00264a40 L4:0x00269100 L14:0x0027e560
uint32_t debug_particle_l1(uint32_t pos, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, int32_t t1, uint32_t t2, float f12);  // L1:0x00285768 L2:0x00271d80 L3:0x0025e9e0 L4:0x00263098 L6:0x0027f648 L10:0x002634e8 L14:0x00278390

// ---- the aircraft base (novalis_jet_base.cpp)
void jet_banking_l1(uint32_t m, uint32_t out);                // L1:0x002f5040 L3:0x00293b50 L4:0x002cb790 L14:0x002e8078
// the same with the sway pair at pvar `sway` (0x130; Kerwan's second build L3:0x0029c8a8: 0x140)
void jet_banking(uint32_t m, uint32_t out, uint32_t sway);
void smoke_particle_l1(uint32_t pos, uint32_t vel);           // L1:0x0027f660 L3:0x00258a30 L4:0x0025d0e8 L14:0x002723e0
uint32_t jet_debris_spawn_l1(uint32_t m, uint32_t dir, float scale);  // L1:0x0030be70
void jet_update_base_l1(uint32_t m);                          // L1:0x002f5168 L3:0x00293c78 L4:0x002cb8b8 L14:0x002e81a0 (Kerwan's class 845)
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
uint32_t nov_2ef350_l1(uint32_t m);                                                                // L1:0x002ef350 L5:0x003003a0 L11:0x002fb048
uint32_t nov_2ef478_l1(uint32_t m, uint32_t group);                                                // L1:0x002ef478 L5:0x003004c8 L11:0x002fb170
void nov_2ef770_l1(uint32_t m, uint32_t vel);                                                      // L1:0x002ef770 L5:0x003007c0 L11:0x002fb468
void nov_2ef560_l1(uint32_t m);                                                                    // L1:0x002ef560 L5:0x003005b0 L11:0x002fb258
void nov_2eec68_l1(uint32_t m);                                                                    // L1:0x002eec68 L5:0x002ffcb8 L11:0x002fa960
void amoeboid_update_l1(uint32_t m);                                                               // L1:0x002edca0 L5:0x002fecf0 L11:0x002f9998  classes 572 / 865 / 866
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

// ---- the rocks (novalis_rocks.cpp)
void nov_class700_update_l1(uint32_t m);   // L1:0x002f8c58  class 700, falling rocks and dust
void nov_2f9000_l1(uint32_t m);            // L1:0x002f9000  class 701's camera
void nov_class701_update_l1(uint32_t m);   // L1:0x002f9080  class 701, a rock that drops
void nov_class704_update_l1(uint32_t m);   // L1:0x002f9810  class 704, a rock that bursts when hit
void nov_class709_update_l1(uint32_t m);   // L1:0x002f9d80  classes 709 / 710, a rock a shell of class 686 bursts
void nov_class729_update_l1(uint32_t m);   // L1:0x002fa800  class 729, a big rock that bursts when hit

// ---- the particle emitter (novalis_emitter.cpp)
void nov_27e900_l1(uint32_t owner, uint32_t pos, int32_t life, uint32_t tex, uint32_t set, uint32_t blend, float vx, float vy, float vz,
                   float size, float fade, float r, float g, float b, float a);  // L1:0x0027e900  a particle of type 6
void nov_class27_update_l1(uint32_t m);    // L1:0x002bd100  class 27

// ---- the Infobot (novalis_infobot.cpp)
void nov_2fac80_l1();                      // L1:0x002fac80  class 811's joints marked during scenes
void nov_class730_update_l1(uint32_t m);   // L1:0x002fad68  classes 730 / 731

// ---- the water's current (novalis_current.cpp)
void nov_class613_update_l1(uint32_t m);   // L1:0x002f3120  class 613

// ---- the steam pipe (novalis_pipes.cpp)
uint32_t nov_2ffa90_l1(uint32_t m);        // L1:0x002ffa90  the leak put in the pipe's place
void nov_class778_update_l1(uint32_t m);   // L1:0x002ff860  class 778, the pipe
void nov_class779_update_l1(uint32_t m);   // L1:0x002ffb28  class 779, the leak

// ---- the shells (novalis_shells.cpp)
void nov_2fa1b0_l1(uint32_t m, uint32_t pos, int32_t sound, float scale, float flash);  // L1:0x002fa1b0  a burst
void nov_class722_update_l1(uint32_t m);   // L1:0x002fa4c0  class 722, a bomb
void nov_class1510_update_l1(uint32_t m);  // L1:0x0030ba18  class 1510, a burning shell

// ---- the water's classes (novalis_water_mobys.cpp)
void nov_2b8288_l1(uint32_t regs, int32_t n, float x, float y, float radius, float amount);  // L1:0x002b8288  a splash (flag 0)
uint32_t nov_287b00_l1(uint32_t pos, uint32_t vel, float size);               // L1:0x00287b00  a bubble (particle 0x38)
uint32_t nov_287c80_l1(uint32_t pos, uint32_t vel, float size, float grow);   // L1:0x00287c80  a ring of bubbles (particle 0x39)
uint32_t nov_2ffcd0_l1(uint32_t pos, float scale);                            // L1:0x002ffcd0  a drip (class 787)
void nov_class751_update_l1(uint32_t m);   // L1:0x002fd0e8  class 751, the water
void nov_class761_update_l1(uint32_t m);   // L1:0x002feb58  class 761, the strips 0x1fbc80
void nov_class787_update_l1(uint32_t m);   // L1:0x002ffdc0  class 787, a drip

// ---- the scenes (novalis_scenes.cpp)
void nov_class1546_update_l1(uint32_t m);  // L1:0x0030c190  class 1546, the scenes' effects
// The scripted camera's pvars +0x90 set from `src` (Novalis's build of uid5_set_pvar_90; called by shared_class280.cpp and
// shared_gold_bolt.cpp on Novalis).
void nov_316e28_l1(uint32_t src);          // L1:0x00316e28

// ---- class 1848 (novalis_reflections.cpp; its draw in novalis_draw_cbs.h)
void nov_class1848_update_l1(uint32_t m);  // L1:0x0030f208

// orig: (none) -- the registrations of this part (called by novalis_moby_fns_register)
void novalis_moby_fns_register2();

}  // namespace rac1

#endif
