// Eudora's (level 4) moby classes: the declarations of eudora_classes_tr_N.cpp (generated: `void` = a class update /
// entry, `TrRet` = a level-only helper in the uniform ABI) and of the hand ports eudora_*.cpp (classes 86, 161, 217;
// their helpers in eudora_flock.h and eudora_walker.h).
#ifndef RAC1_GAME_OVL_EUDORA_MOBYS_H
#define RAC1_GAME_OVL_EUDORA_MOBYS_H

#include <cstdint>

#include <game/ovl/eudora_flock.h>
#include <game/ovl/eudora_walker.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

// orig: (none) -- Eudora's registrations (eudora_mobys.cpp)
void eudora_moby_fns_register();

// ---- the hand ports: class 86, a flock (eudora_flock.cpp); class 161, a fidgeting prop (eudora_fidget.cpp); class 217,
// a two-legged robot (eudora_biped.cpp) and its helpers
void eud_class86_update_l4(uint32_t m);   // L4:0x0029ecf8
void eud_class161_update_l4(uint32_t m);  // L4:0x002a5a08
void eud_class217_update_l4(uint32_t m);  // L4:0x002ba520
void biped_anim_set_l4(uint32_t m, uint32_t set, int32_t frame, int32_t frames);  // L4:0x002ba3e0
void biped_anim_rate_l4(uint32_t m, float rate);  // L4:0x002ba448
void biped_explode_l4(uint32_t m);  // L4:0x002ba470
// class 340, the Circular Saw Enemy (eudora_saw.cpp); class 427, the Blarg Trooper (eudora_trooper.cpp); classes 432 /
// 434, the pump's parts (eudora_pump_parts.cpp); class 466, the flying ships (eudora_ship.cpp); and their helpers
void eud_class340_update_l4(uint32_t m);  // L4:0x002c2270
void eud_class427_update_l4(uint32_t m);  // L4:0x002c4850
void eud_class432_update_l4(uint32_t m);  // L4:0x002c6858
void eud_class434_update_l4(uint32_t m);  // L4:0x002c6bb8
void eud_class466_update_l4(uint32_t m);  // L4:0x002ca420
void saw_sound_update_l4(uint32_t m);  // L4:0x002c1b80
void saw_die_l4(uint32_t m);  // L4:0x002c1d70
int32_t saw_may_attack_l4(uint32_t m);  // L4:0x002c1e98
float saw_attack_point_l4(uint32_t m, uint32_t out);  // L4:0x002c1fb8
void trooper_retire_l4(uint32_t m);  // L4:0x002c4808
// part 3: class 481, a pump-lowered float (eudora_pump_float.cpp); 563, the Woodcut Bot (eudora_woodcut_bot.cpp); 584, a
// platform others move (eudora_ride_platform.cpp); 617, a path rider a pump drives (eudora_pump_path.cpp); 642, something
// the wind blows (eudora_windblown.cpp); 1101, the gates (eudora_gates.cpp); 1120, the Suck Cannon (eudora_suck_cannon.cpp);
// 1190, a talking NPC (eudora_npc.cpp); 1343, the help messages (eudora_help.cpp); 1516, the Woodcut Bot's wood chips
// (eudora_wood_chips.cpp); 1549, a scene's smoke (eudora_scene_smoke.cpp); the saw's class hook +0x10 (eudora_saw.cpp)
void eud_class481_update_l4(uint32_t m);   // L4:0x002cdda0
void eud_class563_update_l4(uint32_t m);   // L4:0x002d16b8
void eud_class584_update_l4(uint32_t m);   // L4:0x002d3580
void eud_class617_update_l4(uint32_t m);   // L4:0x002d6f68
void eud_class642_update_l4(uint32_t m);   // L4:0x002d7e90
void eud_class1101_update_l4(uint32_t m);  // L4:0x002e17d8
void eud_class1120_update_l4(uint32_t m);  // L4:0x002e1a10
void eud_class1190_update_l4(uint32_t m);  // L4:0x002e3078
void eud_class1343_update_l4(uint32_t m);  // L4:0x002e4418
void eud_class1516_update_l4(uint32_t m);  // L4:0x002e5178
void eud_class1549_update_l4(uint32_t m);  // L4:0x002e53a0
float gate_speed_l4(uint32_t m);  // L4:0x002e1768
void cannon_sparkles_l4(uint32_t m);  // L4:0x002e1d00
void woodcut_leg_limits_l4(uint32_t rec);  // L4:0x00292370
void walker_setup_l4(uint32_t m, uint32_t rec);  // L4:0x002923b8
void woodcut_walker_init_l4(uint32_t m);  // L4:0x002d1418
void woodcut_explode_l4(uint32_t m);  // L4:0x002d1608
uint32_t eud_chunk_spawn_l4(uint32_t src, uint32_t vel, uint32_t pos, uint32_t rot, uint32_t cls, uint32_t tag, float lo, float hi);  // L4:0x002e4fe0
uint32_t eud_hook_10_l4(uint32_t m);  // L4:0x002c41b0


}  // namespace rac1

#endif
