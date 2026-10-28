// Testing (the Wii U port's GamePad debug menu, ports/wiiu/game/debug_menu.cpp): parts of the game's frame turned off live, to see
// what each costs on the console. Host switches, not the game's: off, the game goes on without that part (enemies frozen, events
// that wait on them never coming) - for measuring only. All false: the game as it is.
#pragma once

namespace rac1 {
extern bool g_skip_mobys;      // the mobys' updates (0x2648e0), the sound emitters, the wind
extern bool g_skip_hero;       // the hero's frame (0x2070d0)
extern bool g_skip_particles;  // the particles' update (0x267c08), the spin timers
extern bool g_skip_camera;     // the camera (0x1ed0a8)
extern bool g_skip_sound;      // the game's sound frame (its requests to the IOP's 989snd)
extern bool g_skip_hud;        // the level's lights, the shadows' direction, the HUD, the help
extern bool g_skip_effects;    // the 3D effects' drawing and the draw lists' blocks
extern bool g_skip_shadows;    // the drop shadows (0x1fe6d8's shadows_draw)
extern bool g_skip_joints;     // the mobys' joints and skinning (the poses kept as they were)
// (the Wii U port: RAC1_DRAW_LAST_STEP=1, its default there) a host frame that runs several game frames to catch up shows the last
// one only: the others' drawing (0x1f8e30's render_frame - the moby pass, the joints, the world passes, the effects, the shadows)
// not run. The game's state may then differ from the PS2's (what the drawing writes that the update reads: the mobys' visibility)
extern bool g_draw_last_step_only;
}  // namespace rac1
