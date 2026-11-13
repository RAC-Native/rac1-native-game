// Small helpers several levels carry copies of (task F5), each ported once (shared_little.cpp). Each declaration lists
// every level's address. Where the copies differ by a constant (a class, a state, a pvars offset, a level's table), it is
// a parameter or a wrapper per copy, named in the comment.
#ifndef RAC1_GAME_OVL_SHARED_LITTLE_H
#define RAC1_GAME_OVL_SHARED_LITTLE_H

#include <cstdint>

namespace rac1 {

// 1 when moby `m` is of class `cls` in state `state`. (Kerwan: 0x23e, 0xe; Pokitaru, Quartu, Drek's fleet: 0x49b, 2.)
uint32_t moby_is_in_state(uint32_t m, int32_t cls, uint32_t state);  // L3:0x002c6c60 L11:0x0030f270 L15:0x002d9570 L17:0x002d7778
// The same for a moby that may be 0 or gone (state 0xfe / 0xfd: 0). (Kerwan's two: 0x23d, 5 and 0x23e, 9.)
uint32_t moby_alive_in_state(uint32_t m, int32_t cls, uint32_t state);  // L3:0x002c5b78 L3:0x002c6d58
// Of the four mobys (0 for none), the one of the largest scale (+0x2c; the later one on a tie), or 0.
uint32_t moby_largest(uint32_t a, uint32_t b, uint32_t c, uint32_t d);  // L5:0x002899f8 L8:0x00269c70
// The moby shown (flags 0x41 off, 0x1000 on) with its class's default colour (+0x94).
void moby_show_default(uint32_t m);  // L14:0x002b3330 L15:0x00297118
// The moby resting (state 2, flags 0x41 off) with its sparkles' fields set up (helmet_init_fx).
void moby_rest_sparkles(uint32_t m);  // L6:0x002e6148 L7:0x00302208
// The level's camera entry `i` (0x15ef50's 32-byte entries): its record's +0x38 set, +0x39 cleared.
void cam_entry_mark(int32_t i);  // L10:0x002f5a50 L13:0x00316f20 L14:0x00314168 L15:0x002f78b8
// The same entry's record's +0x39 set while the active camera (0x166e00) is of type 19.
void cam_entry_mark_type19(int32_t i);  // L13:0x00316f48 L14:0x00314190
// The active camera's (0x166e00) angle block (+0x70): its +0x14 / +0x10 / +0x18 `a` / `b` / `c`, +0x54 / +0x50 / +0x58
// `d` / `e` / `f` (the copies' f12 .. f17).
void camera_angles_set(float a, float b, float c, float d, float e, float f);  // L2:0x002f8aa0 L6:0x00312b40 L17:0x002fe2c8
// Class `cls`'s animation set `set` (0..254, else 0): the time (+4, 16ths) of its last frame / of its first frame.
float class_anim_last_time(uint32_t cls, uint32_t set);   // L4:0x00241910 L15:0x0023e4c8 L17:0x00249618
float class_anim_first_time(uint32_t cls, uint32_t set);  // L4:0x002418b0 L17:0x002495b8
// A particle 0x33 (a 2-second spark of size 6300) at `pos` moving by `vel` (EE addresses); the particle, or 0.
uint32_t particle_33_spawn(uint32_t pos, uint32_t vel);  // L7:0x0029a2a0 L13:0x0027d8d0 L14:0x00279aa0
// A class 848 at `pos` (EE addresses: its pvars +0 from `from`), its +0xc `f`, +0x10 `a`, +0x14 `b`; the moby, or 0.
uint32_t class848_spawn(uint32_t from, uint32_t pos, uint32_t a, uint32_t b, float f);  // L3:0x002d5208 L7:0x00305a38 L15:0x002dee78
// A carry hook (`which`: carry_hook4 / 8) that puts the moby in state 0x14 while it holds (its state before kept at
// +0xbc) and back to it when it lets go. (Pokitaru's two: 4 and 8.)
int32_t carry_keep_state_hook(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which);  // L11:0x00316210 L11:0x00316270
// A carry hook (`which`: carry_hook0 / 4 / 8) that puts the moby in state 0xc while it holds; when it lets go, state 6 and
// its animation 7. (Drek's fleet's three: 0, 4, 8.)
int32_t carry_drop_hook(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which);  // L17:0x002efea8 L17:0x002eff30 L17:0x002effb8
// Kerwan's turning and wobbling objects (classes 816 and 1012): their angles from the level's rates.
void kerwan_class816_turn(uint32_t m);   // L3:0x002d3c40
void kerwan_class1012_turn(uint32_t m);  // L3:0x002dd0f0
// Blarg's class 1051 (the hydro pumps' switch): the hero idle, the pump's path (pvars +0xec) given its count (+0x114), the
// water's state 2. (Three copies.)
void blarg_class1051_switch(uint32_t m);  // L6:0x002fb4b0 L6:0x002fb870 L6:0x002fbc30
// The point `dist` along a path (`path`: a count and points of 16 bytes) of legs `leg` long into `out` (an EE vector): its
// leg into `index` (a word) and the distance into the leg into `frac` (a float); before the start or past the last
// point, that point (and 0).
void path_point_even(uint32_t path, uint32_t index, uint32_t frac, uint32_t out, float dist, float leg);  // L0:0x00262360 L10:0x002554e0
// A class 819 shot at `pos` flying by `vel` (EE addresses), its pvars' +0x10 `a` / +0x14 `b`, its life `frames`, its scale
// times `scale`, turned along its flight, its distance to the hero kept (+0x38); the moby, or 0. (Orxon's class 1229's,
// Quartu's class 491's.)
// The environment map's texture coordinates of `n` points (`points` and their `normals`: 12-byte triples) into `out`
// ((u, v) pairs): the view's reflection off each, in the map's 4 x 4 repeat. (The fourth argument is not read; Drek's
// fleet's five copies.)
void envmap_uvs4(int32_t n, uint32_t normals, uint32_t points, uint32_t unused, uint32_t out);  // L17:0x002f54d8 L17:0x002f57f0 L17:0x002f5b08 L17:0x002f5e20 L17:0x002f6450
// A path mover at the start of its path (pvars +0x68): the path's legs measured, its leg and distance 0 (+0x60 / +0x64), at
// the first point facing the second, its speeds (+0x70 .. +0x88) 0. (Oltanis's classes 908 and 921.)
void path_mover_start(uint32_t m);  // L14:0x002fc890 L14:0x002fd808
uint32_t class819_launch(uint32_t vel, uint32_t pos, uint32_t a, uint32_t b, int32_t frames, float scale);  // L10:0x002cc1b8 L15:0x002dd600

}  // namespace rac1

#endif
