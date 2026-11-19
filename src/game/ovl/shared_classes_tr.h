// Veldin's shared classes (Vendor 11, Infobot 750, Thruster-Pack jets 167), hand-ported in veldin_shared_classes.cpp.
#ifndef RAC1_GAME_OVL_SHARED_CLASSES_TR_H
#define RAC1_GAME_OVL_SHARED_CLASSES_TR_H

#include <cstdint>

namespace rac1 {

void vendor_update_tr(uint32_t m);                                            // 0x002a57a8
void infobot_follow_path(uint32_t m, uint32_t path, uint8_t done_state);      // 0x002d5988
void infobot_glow_particles(uint32_t m);                                      // 0x002d6cd0
void infobot_update_tr(uint32_t m);                                           // 0x002d5de8
void class167_emit_particles(uint32_t m);                                     // 0x002b3c50
void class167_update_tr(uint32_t m);                                          // 0x002b4198
void class167_draw_tr(uint32_t m);                                            // 0x002b3628

// orig: (none) -- registers the shared classes' updates
void shared_classes_tr_register();

}  // namespace rac1

#endif
