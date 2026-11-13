// The sound emitters: the parts their updates share (sound.cpp's sphere and box emitters, Veldin's types 0 / 2) and the
// levels' own emitter types 1 / 3 / 4 / 5 / 6 with the requests they make (sound_emitter_types.cpp: one port for every
// level's copy). An emitter's parameters are +8 of its 0x90-byte record.
#ifndef RAC1_GAME_OVL_SOUND_EMITTER_H
#define RAC1_GAME_OVL_SOUND_EMITTER_H

#include <cstdint>

namespace rac1 {

// the emitter's next start: `lo` + rand(hi - lo) seconds (params +4 / +8) into its timer +0xc
void emitter_rearm(uint32_t prm);                 // orig: (helper) repeated in 0x002ed950 / 0x002edad8
// whether the emitter's sound still plays in its slot (params +0x10)
bool emitter_playing(uint32_t em, uint32_t prm);  // orig: (helper) repeated in 0x002ed950 / 0x002edad8

// ---- the levels' own types (sound_emitter_types.cpp; the addresses of every level's copy are there)
uint32_t sound_slot_volume_set(int32_t slot, uint32_t volume);  // L1:0x002a1968 ...  a sound slot's volume; returns 1
void reverb_request(uint32_t owner, uint32_t type, uint32_t depth, uint32_t delay, uint32_t feedback);  // L1:0x002a1a90 ...  the reverb
void music_track_request(int32_t track, uint32_t next);         // L1:0x0027a248 ...  the music track
void emitter_fade_box_update(uint32_t em);    // type 1 (L1:0x00319928 ...)  a box whose sound fades out from its core
void emitter_reverb_box_update(uint32_t em);  // type 3 (L1:0x00319f18 ...)  a box that sets the reverb
void emitter_open_bed_update(uint32_t em);    // type 4 (L8:0x00316fa8, L14:0x00316e90)  an ambience bed by the open ground
void emitter_camera_bed_update(uint32_t em);  // type 5 (L1:0x0031a078 ...)  a sound that follows the camera
void emitter_music_box_update(uint32_t em);   // type 6 (L1:0x0031a128 ...)  a box that changes the music track

}  // namespace rac1

#endif
