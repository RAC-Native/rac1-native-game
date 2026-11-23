// Umbris (level 7) moby classes, camera type 0x12, class hooks and level-only engine
// functions: the declarations of their hand-written ports
// (once the generated umbris_classes_tr_N.cpp).
#ifndef RAC1_GAME_OVL_UMBRIS_MOBYS_H
#define RAC1_GAME_OVL_UMBRIS_MOBYS_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

// hand-ported (umbris_lifts / _director / _path_group / _shots / _water / _doors / _turret / _snagglebeast_shots / _walker /
// _lava_platforms / _snagglebeast.cpp)
void umb_class38_update_l7(uint32_t a0);  // L7:0x002cd2b0
void umb_class1474_update_l7(uint32_t a0);  // L7:0x002cdb28
void umb_270248_l7(int32_t col, int32_t row, int32_t cols, int32_t rows);  // L7:0x00270248
void umb_class436_update_l7(uint32_t a0);  // L7:0x002f5ba0
void umb_class529_update_l7(uint32_t a0);  // L7:0x002fbdb8
void umb_30af40_l7(uint32_t m);  // L7:0x0030af40
void umb_class871_update_l7(uint32_t a0);  // L7:0x0030b000
void umb_class880_update_l7(uint32_t a0);  // L7:0x0030b6a8
void umb_class882_update_l7(uint32_t a0);  // L7:0x0030bbc8
bool umb_281ef8_l7(uint32_t m, int32_t group, int32_t a, int32_t b);  // L7:0x00281ef8
void umb_2cb1c0_l7(uint32_t tiles, int32_t n, float x, float y, float radius, float amount);  // L7:0x002cb1c0
void umb_class902_update_l7(uint32_t a0);  // L7:0x0030c508
void umb_class1013_update_l7(uint32_t a0);  // L7:0x0030cf90
void umb_class1018_update_l7(uint32_t a0);  // L7:0x0030d440
uint32_t umb_30ba90_l7(uint32_t pos, uint32_t vel, uint32_t ignore, int16_t kind);  // L7:0x0030ba90
void umb_class1041_update_l7(uint32_t a0);  // L7:0x0030d4d8
void umb_30e0e0_l7(uint32_t t, uint32_t m, uint32_t pv);  // L7:0x0030e0e0
void umb_class1046_update_l7(uint32_t a0);  // L7:0x0030e1f8
void umb_class1049_update_l7(uint32_t a0);  // L7:0x0030e458
void umb_30e720_l7(uint32_t m, uint32_t rec, uint32_t push);  // L7:0x0030e720 (umbris_walker.cpp)
void umb_30e858_l7(uint32_t m, uint32_t rec, uint32_t push);  // L7:0x0030e858
void umb_30e998_l7(uint32_t m, uint32_t pv, uint32_t hs);  // L7:0x0030e998
int32_t umb_30ebc8_l7(uint32_t m, uint32_t pv);  // L7:0x0030ebc8
void umb_30ec58_l7(uint32_t m);  // L7:0x0030ec58
void umb_30eb38_l7(uint32_t path);  // L7:0x0030eb38
int32_t umb_30ed30_l7(uint32_t m, uint32_t pv);  // L7:0x0030ed30
void umb_30eee8_l7(uint32_t m, uint32_t pv);  // L7:0x0030eee8
void umb_30f0f8_l7(uint32_t m, uint32_t pv, float turn, float accel, float vmax);  // L7:0x0030f0f8
int32_t umb_30f1e0_l7(uint32_t m, uint32_t pv, float yaw, float near, float accel, float far, float decel, float hi, float lo);  // L7:0x0030f1e0
void umb_30f308_l7(uint32_t m, uint32_t pv, uint32_t state, uint32_t anim, uint32_t anim2, int32_t frame2, uint32_t next, uint32_t target, float t0, float t1, float len, float strength, float radius);  // L7:0x0030f308
bool umb_30f4d0_l7(uint32_t m, uint32_t target, float radius);  // L7:0x0030f4d0
bool umb_30f540_l7(uint32_t m, int32_t lo, int32_t hi);  // L7:0x0030f540
void umb_class1059_update_l7(uint32_t a0);  // L7:0x0030f5f0
void umb_class1069_update_l7(uint32_t a0);  // L7:0x003112c8 (umbris_lava_platforms.cpp)
void umb_class1080_update_l7(uint32_t a0);  // L7:0x00311bc8
void umb_29c4e8_l7(uint32_t pos, uint32_t vel);  // L7:0x0029c4e8 (umbris_snagglebeast.cpp)
uint32_t umb_30dbb0_l7(uint32_t owner, uint32_t pos, uint32_t extra, float r0, float r1, float height, float step, float push);  // L7:0x0030dbb0
void umb_311eb8_l7(uint32_t m, uint32_t pv, int16_t kind, uint32_t at, int16_t dur, uint32_t c0, uint32_t c1, float size);  // L7:0x00311eb8
void umb_3120d8_l7(uint32_t m);  // L7:0x003120d8
void umb_312150_l7(uint32_t pv);  // L7:0x00312150
uint32_t umb_30e370_l7(uint32_t pos, uint32_t vel, uint32_t ignore);  // L7:0x0030e370
uint32_t umb_3122b0_l7(uint32_t m, uint32_t pv, uint32_t target);  // L7:0x003122b0
int32_t umb_313350_l7(uint32_t m, uint32_t pv, uint32_t out);  // L7:0x00313350
void umb_313420_l7(uint32_t m, uint32_t pv, uint32_t hp_ptr);  // L7:0x00313420
void umb_313920_l7(uint32_t pos, float r);  // L7:0x00313920
void umb_313988_l7(uint32_t m, uint32_t pv, uint32_t goal, uint32_t out);  // L7:0x00313988
uint32_t umb_313af0_l7(uint32_t m, uint32_t pv, uint32_t goal, uint32_t step_target, float reach);  // L7:0x00313af0
float umb_313d30_l7(uint32_t m, uint32_t pv, float dist, float near, float anim_scale);  // L7:0x00313d30
int32_t umb_313de8_l7(uint32_t m, uint32_t pv, uint32_t at, uint32_t done_anim, uint32_t fixed_target);  // L7:0x00313de8
void umb_313f60_l7(uint32_t m, uint32_t pv, uint32_t look_at);  // L7:0x00313f60
void umb_314058_l7(uint32_t m);  // L7:0x00314058
void umb_317c98_l7();  // L7:0x00317c98
float umb_29e6d8_l7(uint32_t out, uint32_t tie, uint32_t center, uint32_t p0, uint32_t p1, float radius);  // L7:0x0029e6d8
void umb_317cb0_l7(uint32_t m, uint32_t mouth);  // L7:0x00317cb0
float umb_317e60_l7(uint32_t m, uint32_t mouth);  // L7:0x00317e60
void umb_318e30_l7(uint32_t m);  // L7:0x00318e30
void umb_318e98_l7(uint32_t m);  // L7:0x00318e98
void umb_class1106_update_l7(uint32_t a0);  // L7:0x00314150
void umb_class1110_update_l7(uint32_t a0);  // L7:0x00319040 (umbris_mines.cpp)
void umb_319698_l7(uint32_t m, uint32_t parent_pv);  // L7:0x00319698
void umb_class1112_update_l7(uint32_t a0);  // L7:0x003196e0
void umb_class1113_update_l7(uint32_t a0);  // L7:0x00319f48 (umbris_props.cpp)
uint32_t umb_30b538_l7(uint32_t pos, uint32_t vel, uint32_t ignore, uint32_t flags);  // L7:0x0030b538 (umbris_sweep_turret.cpp)
void umb_class1126_update_l7(uint32_t a0);  // L7:0x0031a250
void umb_class1128_update_l7(uint32_t a0);  // L7:0x0031a9e8 (umbris_props.cpp)
void umb_class1133_update_l7(uint32_t a0);  // L7:0x0031b3f0
void umb_class1142_update_l7(uint32_t a0);  // L7:0x0031d2e0 (umbris_scene_fx.cpp)
void umb_class1552_update_l7(uint32_t a0);  // L7:0x0031eba8
void umb_class1686_update_l7(uint32_t a0);  // L7:0x0031f780 (umbris_props.cpp)
void umb_class1789_update_l7(uint32_t a0);  // L7:0x0031f900 (umbris_scene_fx.cpp)

void umb_cutscene_level_extras_l7();  // L7:0x00265bd8 (umbris_scene_fx.cpp: the scenes' level extras, cutscene.cpp's dispatch)

}  // namespace rac1

#endif
