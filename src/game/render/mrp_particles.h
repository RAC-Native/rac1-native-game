// The MRP's ambient particles (enhancement, Options > Graphics > Ambient Particles; docs/particles.md): our own dust, ash, embers,
// fireflies, motes in the sunlight, rain and snow, purely visual. Host code: none of it is the game's, and none of it changes the
// game's state (it reads the camera, the hero's light set and the level's file, and writes nothing the game reads).
//
// What a level has comes from its file (data/lights/levelNN.txt, the light editor's: "particles <kind> <density> ..." lines, see
// mrp_lighting.h ParticleSpec). Each line is a camera-relative volume: a box around the camera, its particles at fixed places in
// the world that wrap around it as the camera moves (a particle leaving the box on one side comes back on the other), so the
// volume always holds the same count at no CPU cost. The GPU makes every particle from its index (no buffers): its place, its
// motion (the kind's fall or rise, the wind, a drift), its size and its life, drawn after the MRP's world and mobys (before the
// effects and the 2D), soft against the scene's depth, lit by the zone's light (the hero's light set: dim at night), not drawn
// inside the level's interior boxes. Rain draws streaks along its motion (the camera's own taken off) and splashes on the ground
// (a screen pass over the scene's upward surfaces); snow drifting flakes.
//
// Where a level's rain or snow line replaces the game's own (Batalia's rain, Hoven's snow), the game's effect is not drawn while
// ours is on (mrp_particles_hide_game: the host's drawing only - the game still makes it, its state the same).
#ifndef RAC1_GAME_RENDER_MRP_PARTICLES_H
#define RAC1_GAME_RENDER_MRP_PARTICLES_H

#include <cstdint>

namespace rac1 {

// orig: (host) the option: 0 Off (the default: nothing drawn, nothing read), 1 Low, 2 High (RAC1_PARTICLES; Options > Graphics >
// Ambient Particles; the debug menu's Sun tab)
extern int g_ambient_particles;

// orig: (host) the frame's ambient particles into the bound framebuffer (the 3D's: the render scale's buffer when scaled), after the
// MRP's world, mobys and the ambient occlusion; `game_frame` the game frame shown (their time: still while the game is). Nothing
// unless the option is on, the MRP drew this frame's world and the level's file has particle lines
void mrp_particles_draw(uint64_t game_frame);

// orig: (host) whether the game's own rain / snow is not to be drawn this frame (a rain or snow line of the level's file replacing it
// while the option is on; RAC1_PARTICLES_GAME=1 keeps it, for comparing): read by the GS replica's draw of the frame's primitives
bool mrp_particles_hide_game();
// orig: (host) whether one of the frame's 3D effect primitives (mrp_fx.h: its producer `kind`, its GS TEX0 and primitive type) is the
// game's rain / snow that ours replace now: not drawn (mrp_fx_draw; the game made it as ever)
bool mrp_particles_hide_fx(uint8_t kind, uint64_t tex0, int prim_type);

// orig: (host) testing / the debug menu: 1 (RAC1_PARTICLES_GAME=1) the game's own rain / snow drawn even where ours replace it
extern int g_particles_show_game;
// orig: (host) the debug menu's Enhancements tab: the kinds drawn (bit 1 << ParticleKind; all by default), live, not saved
extern uint32_t g_particles_kinds;

// orig: (host) the debug menu's numbers: the particles drawn last frame, the pass's CPU and GPU time (ms, averaged over a second;
// GPU 0 until measured)
void mrp_particles_stats(int* count, float* cpu_ms, float* gpu_ms);

// orig: (host) the game's particle draw (particles_draw.cpp, 0x267c98): at its start, and the GS texture (TEX0) each particle
// texture got in it - what tells the game's rain and snow from its other particles (their textures this frame). Nothing unless
// the option is on; no game state
void mrp_particles_note_begin();
void mrp_particles_note_tex(uint32_t tex, uint64_t tex0);
// orig: (host) with the frame's chain (mrp_fx_chain_ranges, the game's thread after its update): the textures of the live particles
// of the game's rain (types 0 and 1, Batalia) and snow (type 0x49, Hoven), kept for that frame's drawing (mrp_particles_hide_fx)
void mrp_particles_fx_capture();

}  // namespace rac1

#endif
