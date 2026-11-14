// The level's shadow meshes: static shadows baked into the level data (a core list of 32-byte
// records 0x15f43c, count 0x15f438, mesh table 0x15f440; set up by 0x23f120), each a PSMT8
// shadow texture draped over the ground by a mesh of triangle strips, faded with the distance
// and by the nearby point lights (0x1802c0). Drawn in the render frame between the shrubs and the
// mobys by the VU1 program the boot ELF's 0x10e810 uploads (see shadows_vu1.h).
#ifndef RAC1_GAME_OVL_SHADOWS_H
#define RAC1_GAME_OVL_SHADOWS_H

#include <cstdint>

#include <game/math/vecmath.h>

namespace rac1 {

// The whole pass: GS state (CLAMP, TEX1, TEST, FBA), the records culled and their textures
// queued for upload, the VU1 set-up packet, the per-mesh packets. `s6` is the caller's $s6 (a
// stale register 0x1fe6d8 reads): the overlay's main loop holds 0x150000 there (0x245c88).
// orig: 0x001fca48
void shadows_draw(uint32_t s6);
// Per record: culled (point_on_screen), its alpha by the view depth (+0x14), its texture given a
// GS address and queued for upload (0x16cd80), its TEX0 (+0x18); the lights DMA'd to SPR+0x2400.
// orig: 0x001fe6d8
void shadows_prepare(uint32_t s6);
// The VU1 program's packet: its upload (when not the loaded one, 0x15f5e0 != 7), the two view
// matrices (scaled by 1024, about the camera; Z biased by 0x15f308), GIF tag and constants, the
// start-up MSCALF 0, then the fixed GS packet 0x13cf10.
// orig: 0x001fc740
void shadows_vu1_setup();
// The per-mesh packets: the lights fading each record, then per strip batch a VIF packet (GIF
// tag, ST / RGBA / XYZ unpacks, MSCALF 0xc, or 0xe when the record needs clipping).
// orig: 0x001fe08c
void shadows_packets();
// A REF of the fixed GS packet 0x13cf10 (11 quadwords, DIRECT).
// orig: 0x002a0eb8
void packet_ref_13cf10();  // orig: 0x002a0eb8
// A matrix of diagonal (s, s, s) and row 3 vf0 (VU0).
// orig: 0x001ffa18
void mat4_scale_diag(Mat4& d, float s);

}  // namespace rac1

#endif
