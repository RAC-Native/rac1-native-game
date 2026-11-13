#ifndef RAC1_GAME_OVL_ENEMY_749_H
#define RAC1_GAME_OVL_ENEMY_749_H
// The class-749 enemy of Veldin (update 0x2d4610) and the helpers its chain needs.
#include <game/math/vecmath.h>

#include <cstdint>

namespace rac1 {

void debug_stub_1f6c28();                                                     // orig: 0x001f6c28
float vec3_length_sq_vu(const Vec4& a);                                       // orig: 0x001ff430
int32_t vec3_clamp_length_vu(Vec4& d, const Vec4& a, float max_len);          // orig: 0x001ff598
float angle_wrap_mod(float a);                                                // orig: 0x0025e310
int32_t class_hook_4(uint32_t m, uint32_t a1);                                           // orig: 0x002da218
uint32_t e749_hook_10(uint32_t m);                                            // orig: 0x002d5800
uint32_t class_hook_none(uint32_t m);                                         // orig: 0x002db888
uint32_t class_hook_10(uint32_t m);                                           // orig: 0x002db890
int32_t moby_chain_contact(uint32_t m, uint32_t id, int32_t b, uint32_t a, uint32_t c, uint32_t flags, float radius, float speed);  // orig: 0x002599e8
int32_t path_crossed_v(uint32_t path, const Vec4& a, const Vec4& b);          // orig: 0x00261968
int32_t path_crossed(uint32_t path, uint32_t a, uint32_t b);                  // orig: 0x00261968
int32_t path_push_out_2d_v(uint32_t path, const Vec4& pos, Vec4& out, float r);  // orig: 0x00261d78
int32_t path_push_out_2d(uint32_t path, uint32_t pos, uint32_t out, float r);    // orig: 0x00261d78
uint32_t walker_move_v(uint32_t m, Vec4& d, uint32_t flags, float up, float r, float drop);   // orig: 0x00258830
uint32_t walker_move(uint32_t m, uint32_t d, uint32_t flags, float up, float r, float drop);  // orig: 0x00258830
uint32_t walker_move_fx(uint32_t m, uint32_t d, int32_t r, uint32_t flags, float up, float drop);  // orig: 0x00258ad0
void moby_list_hit_v(uint32_t m, const Vec4& pos, uint32_t list, int32_t n, uint32_t skip, uint32_t b, uint32_t a, uint32_t c, float f, float speed, float z);  // orig: 0x0025a9f8
void moby_list_hit(uint32_t m, uint32_t pos, uint32_t list, int32_t n, uint32_t skip, uint32_t b, uint32_t a, uint32_t c, float f, float speed, float z);  // orig: 0x0025a9f8
void spawn_particle_12(uint32_t pos, uint32_t mat);                           // orig: 0x0026c3d0
void moby_33e_wake(uint32_t m);                                                   // orig: 0x002d5830
uint32_t debris_7a_spawn_v(const Vec4& pos, const Vec4& vel, uint32_t life, uint32_t keep, uint32_t flag);  // orig: 0x002af450
uint32_t debris_7a_spawn(uint32_t pos, uint32_t vel, uint32_t life, uint32_t keep, uint32_t flag);  // orig: 0x002af450
uint32_t fx_4a8_spawn_v(uint32_t v, const Vec4& pos, const Vec4& vel, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t a, float scale);  // orig: 0x002df808
uint32_t fx_4a8_spawn(uint32_t v, uint32_t pos, uint32_t vel, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t a, float scale);  // orig: 0x002df808
float e749_chase(uint32_t m, uint32_t target, float cur);                     // orig: 0x002d54c8
void e749_hit_update(uint32_t m);                                             // orig: 0x002d5160
void class_hook_retire(uint32_t m);                                           // orig: 0x002db868
void explosion_burst(uint32_t m);                                             // orig: 0x002dbf28
void explode_and_reset(uint32_t m, uint32_t r, uint32_t burst);               // orig: 0x002dc938
void carry_hook_c(uint32_t m);                                                // orig: (helper) carry_update's call of the class hook +0xc
int32_t carry_update(uint32_t m, uint32_t p);                                 // orig: 0x002dc9f0
void e749_update(uint32_t m);                                                  // orig: 0x002d4610

}  // namespace rac1

#endif
