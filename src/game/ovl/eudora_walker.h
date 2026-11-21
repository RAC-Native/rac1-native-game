// Eudora's legged walkers: the engine that walks a two-legged moby on its animations (class 217, eudora_biped.cpp, and
// class 563, the Woodcut Bot, still generated). The walker's record (the class's pvars +0x1a0 / +0x160):
//   +0x00 .. +0x5f  six points (the joints of the six-joint list; +0x10 / +0x30 the two feet)
//   +0x60 the jump's goal, +0x6c the heading to it
//   +0x70 .. +0xa0  the 13 gaits (pointers): 0 walk, 1 / 2 a step up, 3 a step on a slope, 4 / 5 a step down, 6 the
//                   foot down after a step down, 7 run, 8 (unused here), 9 / 11 the take-off on either foot, 10 / 12 the
//                   landing; a gait is +0 its animation set, +4 its stride, +8 its speed, +0xc its first frame, +0x10
//                   its length, +0x14 the key it starts from (an integer), +0x18 / +0x1c the feet's passing frames,
//                   +0x28 .. +0x4c the frames each foot is lifted and planted
//   +0xb0 / +0xb3 the animation sets of the idle and of the landing; +0xb4 / +0xb5 two more joints of the list
//   +0xb6 the feet on the ground (bits), +0xb7 the state (0 stands, 1 .. 11 walks, 12 stops, 13 runs, 14 / 15 jumps,
//         16 .. 18 lands, 19 / 20 the foot down after it), +0xb8 the slope's pitch (+0xbc its seek)
//   +0xc0 the speed, +0xc4 its greatest change a frame, +0xc8 the heading's seek, +0xcc / +0xd0 / +0xd4 its rates
//   +0xd8 the vertical speed, +0xdc the gravity, +0xe0 the step's height, +0xe4 the floor ahead, +0xe8 the body's lift
//   +0xec a counter, +0xee flags (1 a jump asked, 2 jumping, 4 in the air, 8 stepping down)
//   +0xf0 and +0x1a0 (+0xb0 each) the two legs: +0xf0 .. +0xf3 joints, +0xf8 the leg's heading, +0xfc / +0x104 its
//         two bends (+0x100 / +0x108 their seeks, +0x10c / +0x110 their limits), +0x121 / +0x161 a joint's own rotation
//         off (+0x130 / +0x170 its quaternions); +0x1a0 / +0x1a1 two more joints of the list
// Header comments carry the levels' addresses; the `orig:` lines are on the definitions (eudora_walker.cpp). The four
// Veldin functions (every level has them; only Eudora's code calls them) are declared with their Veldin address: the
// code generation glues each level's copy to them.
#ifndef RAC1_GAME_OVL_EUDORA_WALKER_H
#define RAC1_GAME_OVL_EUDORA_WALKER_H

#include <cstdint>

namespace rac1 {

// The key of an animation sequence (+0x10 its key count, +0x1c its keys: +4 a key's time in 1/16ths of a frame) nearest
// the frame `time` among the two around it; -1 past the last key.
int32_t anim_seq_key_at(uint32_t seq, float time);  // orig: 0x0024ed18 (L4:0x00241708)
// Which of a gait's foot ranges hold the frame `time` (bits 1 / 4 planted, 2 / 8 lifted); with none, 0x10 / 0x20 when
// `time` is off the feet's passing frames (+0x18 / +0x1c) by more than a frame.
uint32_t gait_phase_flags(uint32_t gait, float time);  // orig: 0x002a13f0 (L4:0x00292688)
// The two legs' bends and their joints' own rotations back to none.
void walker_legs_reset(uint32_t rec);  // orig: 0x002a1538 (L4:0x00292cb8)
// The frames until the walker in the air lands on the height of its goal (+0x68): 100000 when it never does.
float walker_fall_time(uint32_t m, uint32_t rec);  // orig: 0x002a15c8 (L4:0x00293080)

// Eudora's own: the walker's feet flags at the frame `time` (the gait playing, else +0xb6 as it is).
uint32_t walker_phase_flags_l4(uint32_t m, uint32_t rec, float time);  // L4:0x002927d0
// The floor's height under the point `pos` (up to 2.5 below it, from 0.75 above): 0 with none.
float walker_floor_below_l4(uint32_t pos);  // L4:0x002922d0
// The body kept on the ground under the feet: its pitch, the vertical speed, the points moved; 2 on a slope too steep.
uint32_t walker_ground_follow_l4(uint32_t m, uint32_t rec);  // L4:0x00292828
// The legs' bends turned toward the feet's places (`flags`: the feet's ground bits of this frame).
void walker_legs_aim_l4(uint32_t m, uint32_t rec, uint32_t flags);  // L4:0x00292d48
// The gaits' state machine (`landed`: the feet put down this frame).
void walker_gait_update_l4(uint32_t m, uint32_t rec, uint32_t landed, float speed, float yaw, float accel);  // L4:0x00293128
// The walker's frame: heading, legs, speed toward `speed` by at most `accel`, the body on the ground, the gaits, the
// collision; bit 1 when pushed out of something, bit 2 when it barely moved or stands on a slope too steep.
uint32_t walker_update_l4(uint32_t m, uint32_t rec, float speed, float yaw, float accel);  // L4:0x00295778
// The walker set up on the moby's animation as it is; its feet flags.
uint32_t walker_init_l4(uint32_t m, uint32_t rec);  // L4:0x00295de8

}  // namespace rac1

#endif
