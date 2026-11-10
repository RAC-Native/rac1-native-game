// The host renderer's switches (render_toggles.h), set by the debug menu. Host code, no
// original counterpart (moved out of the removed render_entry.cpp).
#include <cstdlib>
#include <game/render/render_toggles.h>

namespace rac1 {

bool g_render_tfrag = true;
bool g_render_tie = true;
bool g_render_shrub = true;
bool g_render_moby = std::getenv("RAC1_RENDER_MOBY") == nullptr || std::atoi(std::getenv("RAC1_RENDER_MOBY")) != 0;  // (testing: 0 the world alone, the debug menu's toggle)
bool g_render_sky = true;
bool g_render_fx = std::getenv("RAC1_RENDER_FX") == nullptr || std::atoi(std::getenv("RAC1_RENDER_FX")) != 0;  // (testing: 0 no effects, the debug menu's toggle)
bool g_draw_from_snapshot = false;
bool g_world_faithful = false;
bool g_world_mrp = false;
bool g_world_chain_forced = false;
bool g_title_scene_host = false;
bool g_moby_vu1_pass = true;  // (the game's moby pass runs, as the MRP and the faithful path need; false: the GLB drawer, RAC1_MOBY_VU1=0)
MobyRenderer g_moby_renderer = MobyRenderer::kFaithful;
bool g_render_gs2d = true;
bool g_render_wireframe = false;
int g_render_shade = std::getenv("RAC1_RENDER_SHADE") != nullptr ? std::atoi(std::getenv("RAC1_RENDER_SHADE")) : 0;  // (testing: 1 the vertex colours alone)
float g_render_aspect = 640.0f / 480.0f;
int g_view_mode = 0;
float g_view_frame_aspect = 4.0f / 3.0f;

} // namespace rac1
