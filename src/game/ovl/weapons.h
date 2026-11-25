#ifndef RAC1_GAME_OVL_WEAPONS_H
#define RAC1_GAME_OVL_WEAPONS_H
// The weapons and gadgets ported here (the Bomb Glove / Pyrocitor / Blaster
// are declared in ovl.h). Addresses are Veldin's overlay addresses (the shared engine).
#include <game/math/vecmath.h>

#include <cstdint>

namespace rac1 {

// --- shared effect helpers (weapon_fx.cpp) ---
float angle_seek_limited(float cur, float target, float limit);              // orig: 0x00200510
void moby_list_remove(uint32_t m, uint32_t list);                            // orig: 0x0025f800
void hit_rec_init(uint32_t rec, uint32_t owner, uint32_t flags, float f);    // orig: 0x00259858
void spawn_particle_08(uint32_t pos, uint32_t vel, uint32_t color, uint32_t color2, int32_t life, float size);  // orig: 0x0026a4c8
void spawn_particle_08_v(const Vec4& pos, uint32_t vel, uint32_t color, uint32_t color2, int32_t life, float size);  // orig: 0x0026a4c8
void floor_light_add(uint32_t m, float scale);                               // orig: 0x00259fe8
void explosion_effect(uint32_t src, uint32_t a1_unused, uint32_t pos, int32_t n1, int32_t A, int32_t B, int32_t C, int32_t D,
                      int32_t E, int32_t F, uint32_t G, float radius, float f13, float f27, float f28, float f29, float S,
                      float f26);                                            // orig: 0x0025e450
// the same blast at the last hit's point (collision.h), read at each of the original's reads of its place (orig: 0x0025e450
// given 0x173e60)
void explosion_effect_at_hit(uint32_t src, int32_t n1, int32_t A, int32_t B, int32_t C, int32_t D, int32_t E, int32_t F, uint32_t G,
                             float radius, float f13, float f27, float f28, float f29, float S, float f26);  // orig: 0x0025e450

// --- Mine Glove (item 0x11, hand class 0xbe, mine class 0x4a) ---
void mine_throw_velocity_v(const Vec4& from, const Vec4& target, uint32_t out, float g, float lo, float range);  // orig: 0x002c1928
void mine_throw_velocity(uint32_t from, uint32_t target, uint32_t out, float g, float lo, float range);        // orig: 0x002c1928
uint32_t mine_spawn(uint32_t hand, uint32_t pos);                            // orig: 0x002a9ed0
void mine_throw_start(const Vec4& from, uint32_t mine, uint32_t vel);        // orig: 0x002aa008
void mine_glove_update(uint32_t m);                                          // orig: 0x002c1ad0
int32_t mine_predict(uint32_t pv, uint32_t mine, int32_t mode);              // orig: 0x002aa2a8
void mine_update(uint32_t m);                                                // orig: 0x002aa670
void mine_glove_fns_register();                                              // orig: (none)

// --- Taunter (item 0x0e, hand class 0xaf) ---
int32_t taunter_hit(uint32_t m, float range);                                // orig: 0x002b6bc8
void taunter_rings(uint32_t m);                                              // orig: 0x002b7398
void taunter_update(uint32_t m);                                             // orig: 0x002b6f10
void taunter_fns_register();                                                 // orig: (none)

// --- Walloper (item 0x12, hand class 0xb4) ---
void walloper_update(uint32_t m);                                            // orig: 0x002bb558
void walloper_fns_register();                                                // orig: (none)

// --- Glove of Doom (item 0x14, hand class 0xe5, agent class 0xe6) ---
void doom_throw_velocity(uint32_t from, uint32_t target, uint32_t out, float g, float lo, float range);  // orig: 0x002c7320
uint32_t doom_agent_spawn(uint32_t hand, uint32_t pos, uint32_t vel);        // orig: 0x002c8218
void doom_agent_launch(uint32_t from, uint32_t agent, uint32_t vel);         // orig: 0x002c82f0
void doom_aim(uint32_t hand, uint32_t pv, uint32_t from_pos);                // orig: 0x002c7548
void doom_glove_update(uint32_t m);                                          // orig: 0x002c7a58
void doom_glove_fns_register();                                              // orig: (none)

uint32_t doom_spawn_agent(uint32_t owner, uint32_t pos, uint32_t vel);       // orig: 0x002bfe08
void doom_agent_splash(uint32_t m);                                          // orig: 0x002c8078
void doom_agent_draw(uint32_t m);                                            // orig: 0x002c7df8
int32_t doom_agent_step(uint32_t pv, uint32_t m, int32_t mode);              // orig: 0x002c8440
void doom_agent_update(uint32_t m);                                          // orig: 0x002c89e8
void doom_agent_fns_register();                                              // orig: (none)
void spawn_particle_05(uint32_t pos, uint32_t r, uint32_t g, uint32_t b, int32_t life, float size, float f13);  // orig: 0x00269b70
uint32_t glow_particle_20(uint32_t m);                                       // orig: 0x0026ed28

// --- Trespasser (item 0x1a, class 0xbc) ---
void trespasser_pickup_update(uint32_t m);                                   // orig: 0x002c1778
void trespasser_update(uint32_t m);                                          // orig: 0x002c1718
void gadgets_fns_register();                                                 // orig: (none)

// --- draw callbacks (weapon_draw.cpp) ---
void taunter_rings_draw(uint32_t m);                                         // orig: 0x002b7568
void mine_marker_draw(uint32_t m);                                           // orig: 0x002a9c50
void walloper_ribbon(uint32_t seg, int32_t a1, int32_t a2, int32_t n, float w1, float w2, float w3);  // orig: 0x002bbbc8
void walloper_glow(uint32_t m);                                              // orig: 0x002bc1a0
void walloper_trail_draw(uint32_t m);                                        // orig: 0x002bbb00
void weapon_draw_fns_register();                                             // orig: (none)

// Registers the ports of this file group for the indirect calls (called from ovl_dispatch.cpp).
void weapons_fns_register();                                                 // orig: (none)

}  // namespace rac1
#endif
