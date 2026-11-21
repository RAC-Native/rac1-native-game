// The draw callbacks the draw-queuing classes of Kalebo III and Drek's fleet queue (shared_class1952.cpp): each draws its
// meshes (one to three) with the sky's reflection sphere-mapped from the camera, as Kalebo III's environment-mapped
// objects do (shared_envmap.cpp) but with no on-screen test. One port of the seven copies (shared_envmap_meshes.cpp); each
// copy's texture, mesh count, blend word and tables are its own. Phase 1 of stage 4: the levels' generated copies still
// run; these are proved against them by oracle batch 517.
#ifndef RAC1_GAME_OVL_SHARED_ENVMAP_MESHES_H
#define RAC1_GAME_OVL_SHARED_ENVMAP_MESHES_H

#include <cstdint>

namespace rac1 {

void class1952_draw(uint32_t m);  // L16:0x002e9ce0
void class1907_draw(uint32_t m);  // L17:0x002f5690
void class1908_draw(uint32_t m);  // L17:0x002f59a8
void class1910_draw(uint32_t m);  // L17:0x002f5cc0
void class1911_draw(uint32_t m);  // L17:0x002f5fd8
void class1913_draw(uint32_t m);  // L17:0x002f62f0
void class1918_draw(uint32_t m);  // L17:0x002f6608

}  // namespace rac1

#endif
