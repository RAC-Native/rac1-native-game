// The MRP's modern lighting (enhancement, "Modern lighting"; experiment of the dynamic-lighting worktree):
// real shadows of the sun cast by the world (tfrag, tie, shrub: the foliage with its alpha cut-outs) and the mobys onto the world and
// the mobys, by cascaded shadow maps. Host code: none of it is the game's, and none of it changes the game's state (it only reads
// the frame the MRP draws). Off by default (the original's lighting, the reference); on, the original's drop shadows (mrp_shadows.h)
// are never drawn (the game still makes them): the mobys cast their own where there is a sun.
//
// How it plugs into the MRP (ADR 0010, "lighting / shadow modules"):
//  - every MRP vertex shader (world CPU / tfrag path, world GPU path, mobys) ends its world-projection branch with `sh_clip(clip)`:
//    the clip position as before; in the shadow pass (uShPass) the same vertex into the cascade's light clip space, the world point
//    taken back from the camera's clip coordinates (the MRP's one projection, world_view_proj: x / P00, y / P11, view z = -w, then the
//    view's inverse: no near / far cancellation). The game's culling decided what is drawn, so the casters are what the frame holds.
//  - every MRP fragment shader darkens its vertex colour by `sh_apply(c)` before the texture function: the sun's part of the lit
//    colour removed where the shadow map says the sun is hidden, on the faces turned to the sun (the faces turned away were already
//    darkened by the original's lighting).
#ifndef RAC1_GAME_RENDER_MRP_LIGHTING_H
#define RAC1_GAME_RENDER_MRP_LIGHTING_H

#include <cstdint>
#include <vector>

namespace rac1 {

// orig: (host) 0 the original's lighting (default), 1 the modern shadows (RAC1_SHADOWS=0|1, the Graphics page, the debug menu)
extern int g_modern_shadows;
// orig: (host) the quality: the cascades' resolution 0 1024, 1 2048 (default), 2 4096 (RAC1_SHADOW_RES)
extern int g_shadow_quality;
// orig: (host) the shadows' strength, percent of the sun's part removed (RAC1_SHADOW_STRENGTH, default 55)
extern int g_shadow_strength;
// orig: (host) testing: 1 draws the cascades' index as a tint, 2 the visibility alone (RAC1_SHADOW_DEBUG)
extern int g_shadow_debug;
// orig: (host) 1 (default) the game's 8 dynamic point lights (0x1802c0) lit per pixel while the modern lighting is on
// (RAC1_POINTS=0: as the game, per vertex)
extern int g_modern_points;
// orig: (host) 1 (default) the automatic emitters - lamps / glowing panels found in the ties' textures, our art choice - lit
// per pixel with the game's point lights (RAC1_AUTO_LIGHTS=0: none)
extern int g_auto_lights;
// orig: (host) how far from the camera the mobys cast into the sun's shadows, units (0: as far as the cascades reach;
// RAC1_SHADOW_MOBY_DIST; Options > Graphics > Shadow Distance)
extern float g_shadow_moby_dist;
// orig: (host) the ambient occlusion's intensity, percent of each mode's own strength (RAC1_AO_INTENSITY, default 75;
// Options > Graphics > AO Intensity)
extern int g_ao_intensity;
// orig: (host) the mobys' pixels marked (stencil bit 0) while the MRP draws them, so that the ambient occlusion is gentler on the
// characters and objects than on the world (RAC1_AO_MOBY): begin before the MRP's mobys, end after them. Nothing unless the modern
// lighting draws this frame with the occlusion on and the framebuffer has a stencil
void mrp_lighting_ao_moby_mark(bool begin);
// orig: (host) ambient occlusion on the 3D (screen space, from the depth; 0 off, the default; RAC1_AO; Options > Graphics)
extern int g_ao;
// orig: (host) the sun's rays (light shafts, screen space): 0 off (default), 1 Low, 2 Medium, 3 High (RAC1_SUN_RAYS; Options > Graphics > Sun Rays)
extern int g_sun_rays;
// orig: (host) the ambient occlusion over this frame's 3D (after the MRP world and mobys, before the effects and the 2D;
// the framebuffer they were drawn into bound): nothing unless the modern lighting drew this frame and g_ao is on
void mrp_lighting_ao_pass();

// ---- (step 3) the lights placed by hand: our own art choices, per level (data/lights/level<N>.txt; RAC1_LIGHTS_DIR) ----
// Lit per pixel as the automatic emitters (Secondary Lights: All), before them; their shadows as the emitters' (the mobys' alone).
struct UserLight {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float col[3] = {1.0f, 0.8f, 0.55f};
    float intensity = 1.0f;  // the colour's scale
    float radius = 8.0f;     // units
    float flicker = 0.0f;    // 0 steady .. 1 a flame's
    bool on = true;
};
// orig: (host) the current level's lights (loaded at its first frame with the lighting on; -1 none yet)
std::vector<UserLight>& mrp_user_lights();
int mrp_user_lights_level();
// orig: (host) the level's file: written (true when it was), read again (the edits since the last save dropped)
bool mrp_user_lights_save();
void mrp_user_lights_reload();
const char* mrp_user_lights_path();
// orig: (host) the debug menu's selected light (-1 none): it pulses, so that it is found
extern int g_user_light_selected;
// orig: (host) the level's lights per object class ("emit <tie|shrub|moby> <class> <ox oy oz> <r g b> <intensity> <radius> [flicker]" in its
// file; our data): every instance of that tie class, or every live moby of that class, lights as a point light at the offset turned
// with it (the instance's rows / the moby's matrix; world units at the object's scale), through the same per-pixel path as the
// lights placed by hand (within reach of the camera, the nearest first, at most the shaders' count; Secondary Lights: All)
struct ClassLight {
    bool moby = false;
    bool shrub = false;  // (a shrub class: its instances as a tie's)
    int o_class = 0;
    float off[3] = {0.0f, 0.0f, 0.0f};
    float col[3] = {0.85f, 0.92f, 1.0f};
    float intensity = 1.0f, radius = 8.0f, flicker = 0.0f;
    float down = 0.0f;  // (the share of the light aimed straight down, as a ceiling fixture's: 0 every way alike, 1 a downward Lambert lobe)
    bool on = true;
};
std::vector<ClassLight>& mrp_user_class_lights();
// orig: (host) the objects at a world point (the debug menu's crosshair: the depth there): the tie instance whose model's box holds
// it (the smallest), the nearest live moby within 6 units; each with its class and the point in its own frame (an "emit" offset)
struct LightPick {
    bool tie = false, moby = false, shrub = false;
    int tie_class = 0, moby_class = 0, shrub_class = 0;
    float tie_off[3] = {0.0f, 0.0f, 0.0f}, moby_off[3] = {0.0f, 0.0f, 0.0f}, shrub_off[3] = {0.0f, 0.0f, 0.0f};
    float moby_dist = 0.0f;
};
LightPick mrp_lighting_pick(const float p[3]);
// orig: (host) the world point under a point of the 3D's view (u, v: 0..1 from the bottom left; the debug menu's crosshair): asked
// for the next frames, the last one read (false: the sky or nothing yet)
bool mrp_lighting_screen_point(float u, float v, float out[3]);
// orig: (host) the debug menu's selected class (-1 none): its lights pulse
extern int g_class_light_selected;
// orig: (host) the level's own sun ("sun <azimuth> <elevation>" in its file, degrees; azimuth from +x toward +y, the direction
// toward the sun): the game's key light's direction replaced where the game has a sun, its colour and weight kept, and its own
// direction still the one a shadow takes the key light's term off with. Our data, not the game's: where the key light's direction
// leaves the places the player walks in the permanent shade of the level's walls (Novalis's start). True when set
bool mrp_user_sun(float* az, float* el);
void mrp_user_sun_set(bool on, float az, float el);
// orig: (host) the rotation (column-major 3x3) taking the game's key light's direction to the sun's, true when the modern lighting is
// on and the level's file turns the sun (the mobys' light records are turned with it: mrp_moby_renderer.cpp)
bool mrp_lighting_sun_turn(float r[9]);
// orig: (host) the level's interiors (its file: "interior x0 y0 z0 x1 y1 z1" boxes, "interior_set <n>" light sets; our data): where the
// camera is in one of the boxes, or the hero is lit with one of the sets, the sun is off and the picture is the original's lighting
// (faded over about 0.3 s): caves, closed rooms, ships' holds. Edited by the debug menu's Sun tab, saved with the level's file
struct SunInterior {
    float lo[3] = {0.0f, 0.0f, 0.0f}, hi[3] = {0.0f, 0.0f, 0.0f};
};
std::vector<SunInterior>& mrp_user_interiors();
std::vector<int>& mrp_user_interior_sets();
// orig: (host) how much the camera is inside an interior (0 .. 1, faded), and the hero's light set now
float mrp_lighting_interior(int* hero_set);
// orig: (host) the game's key light's direction as the shadows last used it (azimuth, elevation, degrees)
void mrp_lighting_game_sun(float* az, float* el);
// orig: (host) the tie classes whose automatic emitters the level's file turns off ("noauto <class>"), and on again
bool mrp_user_lights_class_off(int o_class);
void mrp_user_lights_class_set_off(int o_class, bool off);
// orig: (host) the level's tie classes found glowing (mrp_world_renderer.cpp): the class, its glowing share, the hue, the
// lights it gives (0: none - a warm material, or turned off), whether the heuristic took it for a material
struct EmitterClass {
    int o_class = 0;
    float share = 0.0f, hue = 0.0f;
    int lights = 0;
    bool material = false;
};
std::vector<EmitterClass> mrp_world_emitter_classes();
// orig: (host) the level's tie instances (their class, position and rows - a model unit / 1024 is a world unit at scale 1), made
// once per level from its tables (mrp_world_renderer.cpp)
struct TieSpot {
    int o_class = 0;
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float rows[3][3] = {};
    float lo[3] = {0.0f, 0.0f, 0.0f}, hi[3] = {0.0f, 0.0f, 0.0f};  // (the class's model's box, in the rows' units: model / 1024)
    bool shrub = false;                    // (a shrub instance: no box, its bounding sphere)
    float sphere[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // (world centre, radius)
};
const std::vector<TieSpot>& mrp_world_tie_instances();
// orig: (host) the automatic emitters made again (a class turned on or off)
void mrp_world_emitters_rebuild();
// orig: (host) the automatic emitters within `reach` of `cam` (their sphere's edge), the nearest first, at most `max`: position +
// radius, colour (w -1); returns the count (mrp_world_renderer.cpp)
int mrp_world_emitters_near(const float cam[3], float reach, int max, float* pos_r, float* col);
// orig: (host) the point lights' shadows ("dynamic shadows"): 0 off (default), 1 on - cube shadow maps for the nearest
// few lit per pixel (RAC1_POINT_SHADOWS)
extern int g_point_shadows;
// orig: (host) true when the world's resident lit colours must not take the game's relighting now: the modern point
// lights on and one of the game's lights active (its per-vertex term would count twice); read on the scene's worker (EE memory)
bool mrp_lighting_points_freeze();

// orig: (host) true when this frame gets the modern shadows: on, the MRP world and mobys drawn in the world's projection
bool mrp_lighting_active();
// orig: (host) at the start of a host frame's draw: no shadows until the frame's shadow pass ran
void mrp_lighting_frame_begin();
// orig: (host) the frame's shadow maps: the cascades fitted to the view, the world's and the mobys' casters drawn into them (call
// after mrp_world_finish_frame and mrp_moby_prepare, before mrp_world_draw; restores the bound framebuffer and viewport)
void mrp_lighting_shadow_pass();

// orig: (host) what the world's casters outside the view are looked for with (mrp_world_static.h ShadowCull): the sun, the cascades'
// far distances, how far toward the sun (from the camera: the shadow pass cuts every caster beyond that plane - uShCut -, wherever the
// camera looks); false while the lighting is off (or before its first frame: the sun not known yet)
bool mrp_lighting_cull_params(float sun[3], float splits[3], float* back, float* tan_x = nullptr, float* tan_y = nullptr);
// orig: (host) the point lights whose shadows are drawn, by slot (position, radius; radius 0: none) - read on the scene's
// worker (EE memory), the casters near them sorted into their cubes' views
void mrp_lighting_point_casters(float pts[8][4]);
// orig: (host) how much larger than the view's slices the casters outside the view are looked for (the cached cascades'
// boxes: 1 when the cache is off)
float mrp_lighting_caster_grow();
// orig: (host) whether the next frame's casters outside the view are wanted (read on the scene's worker): a cached cascade
// near its box's edge or drawn without them, or the cache off; the game's point lights' shadows want them besides (their slots)
bool mrp_lighting_casters_wanted();
// orig: (host) whether this frame's casters outside the view were made (mrp_world_renderer.cpp)
bool mrp_world_casters_built();

// The renderers' side. Uniform locations of the lighting in a program (the vertex and fragment snippets below).
struct ShadowLocs {
    int pass = -1, on = -1, inv_view = -1, proj = -1, light = -1, cut = -1;
    int map = -1, casc = -1, cam = -1, sun = -1, param = -1, texel = -1, debug = -1;
    int pt_n = -1, pt_pos = -1, pt_col = -1;  // (the game's point lights per pixel)
    int pt_down = -1;                        // (each light's share aimed down, four to a vector)
    int pt_map = -1, pt_near = -1;           // (their shadows: the cube maps, the near plane)
    int pt_tiles = -1, pt_vp = -1;           // (the screen tiles' lights, the view's world -> clip)
    int opaque = -1;                         // (the shadow pass: the batch is opaque - no blending -: its depth alone)
    int part = -1;                           // (the cascades' squares' share of their layer)
    int sun_k = -1;                          // (the sun's shadows' weight)
    int key = -1, key_dir = -1;              // (the key light's colour and back factor, its direction: the world's shadows)
    int strength = -1;                       // (the shadows' strength)
};
// orig: (host) the locations of a linked program
ShadowLocs mrp_lighting_locs(unsigned prog);
// orig: (host) the receivers' uniforms for the frame's draw (the program bound): the lighting on or off, the shadow map on its unit
void mrp_lighting_bind_receiver(const ShadowLocs& l, bool sky = false);
// orig: (host) the shadow pass's uniforms (the program bound): the cascade's light view-projection
void mrp_lighting_bind_caster(const ShadowLocs& l, const float* light_vp);
// orig: (host) the uniforms back to "no lighting, no pass" (the program bound): the MRP's original image
void mrp_lighting_bind_off(const ShadowLocs& l);
// orig: (host) the Vulkan descriptor set layout (set = 1) and bindings for the modern lighting
uint64_t mrp_lighting_vk_set_layout();
void mrp_lighting_vk_bind_receiver(uint64_t pipe_layout, bool sky = false, bool lit_variant = true);
void mrp_lighting_vk_bind_caster(uint64_t pipe_layout, const float* light_vp, bool opaque = false);
void mrp_lighting_vk_bind_off(uint64_t pipe_layout);
// orig: (host) Vulkan: whether the MRP mobys drawn now mark their pixels in the stencil (bit 0) for the ambient occlusion
bool mrp_lighting_vk_ao_marking();

// The draws the shadow pass asks of the renderers (mrp_world_renderer.cpp, mrp_moby_renderer.cpp): the frame's casters with the
// light's view-projection `light_vp` into the bound depth buffer (depth only, alpha-tested cut-outs)
// (`view` the shadow view - 0..2 a cascade, 3 + i point light slot i's cube: the world draws the casters sorted into it;
// `sphere` (x, y, z, r) the mobys near a point light alone)
void mrp_world_draw_casters(const float* light_vp, int view);
// (`within` - x, y, z, distance - the parts within that distance of that point alone: the shadows' distance)
void mrp_moby_draw_casters(const float* light_vp, const float* sphere = nullptr, const float* within = nullptr);
// the distance from a point to the nearest moby part the frame draws (a large value when none)
float mrp_moby_nearest_part(const float p[3]);
// the light's clip-space rectangle (x0, y0, x1, y1) holding every moby part a caster pass draws (false: none)
bool mrp_moby_caster_rect(const float* light_vp, float rect[4], const float* within = nullptr);

}  // namespace rac1

// The GLSL snippets (spliced into the MRP's shaders after their #version line and declarations).
#define RAC1_SHADOW_VS                                                                                  \
    "uniform int uShPass;\n"                                                                            \
    "uniform int uShOn;\n"                                                                              \
    "uniform mat4 uShInvView;\n"                                                                        \
    "uniform vec2 uShProj;\n"                                                                           \
    "uniform mat4 uShLight;\n"                                                                          \
    "uniform vec4 uShCut;\n"                                                                            \
    "uniform int uPtN;\n"                                                                               \
    "smooth out vec3 vShWorld;\n"                                                                       \
    "vec4 sh_clip(vec4 clip) {\n"                                                                       \
    "    vShWorld = vec3(0.0);\n"                                                                       \
    "    gl_ClipDistance[0] = 1.0;\n"                                                                   \
    "    if (uShPass == 0 && uShOn == 0) return clip;\n"                                                \
    "    vec4 wp = uShInvView * vec4(clip.x / uShProj.x, clip.y / uShProj.y, -clip.w, 1.0);\n"          \
    "    vShWorld = wp.xyz;\n"                                                                          \
    "    if (uShPass != 0) {\n"                                                                         \
    "        gl_ClipDistance[0] = uShCut.w - dot(wp.xyz, uShCut.xyz);\n"                                \
    "        return uShLight * vec4(wp.xyz, 1.0);\n"                                                    \
    "    }\n"                                                                                           \
    "    return clip;\n"                                                                                \
    "}\n"

#define RAC1_SHADOW_FS                                                                                  \
    "uniform int uShPass;\n"                                                                            \
    "uniform int uShOn;\n"                                                                              \
    "uniform sampler2DArrayShadow uShMap;\n"                                                            \
    "uniform mat4 uShCasc[3];\n"                                                                        \
    "uniform vec3 uShCam;\n"                                                                            \
    "uniform vec3 uShSun;\n"                                                                            \
    "uniform vec4 uShParam;\n" /* x strength, y count, z the cascades' blend band, w the PCF radius */ \
    "uniform vec3 uShTexel;\n" /* the cascades' texel in world units */                                \
    "#if defined(SH_POINTS) || defined(SH_SPLIT)\n" /* (the lighting's variants only) */     \
    "uniform float uShSunK;\n"           /* the sun's shadows' weight (the zone's light; faded) */      \
    "uniform vec4 uShKey;\n"             /* the key light's colour (x 128 / 255) and back factor; w < 0: none */ \
    "uniform vec3 uShKeyDir;\n"          /* toward the key light as the game lights with it (the sun may be turned) */ \
    "uniform float uShStrength;\n"       /* the shadows' strength: the share of the key light a shadow hides (Options) */ \
    "float sh_vis = 1.0;\n"              /* the sun's visibility (the strength applied), the zone's weight not applied */ \
    "#endif\n"                                                                                          \
    "uniform int uShDebug;\n"                                                                           \
    "uniform int uPtN;\n"                                                                               \
    "uniform vec4 uPtPos[96];\n"                                                                         \
    "uniform vec4 uPtCol[96];\n"                                                                         \
    "uniform vec4 uPtDown[24];\n" /* per light (four to a vector) the share aimed down: a ceiling fixture's lobe */ \
    "uniform samplerCubeArrayShadow uPtMap;\n"                                                          \
    "uniform float uPtNear;\n"                                                                          \
    "uniform usampler2D uPtTiles;\n" /* per screen tile the lights reaching it (bits 0..63) */ \
    "uniform mat4 uPtVP;\n"           /* the view's world -> clip, the tiles' frame */                       \
    "uniform int uShOpaque;\n"                                                                          \
    "uniform vec3 uShPart;\n"                                                                           \
    "smooth in vec3 vShWorld;\n"                                                                        \
    "float sh_tap(int k, vec3 p) {\n"                                                                   \
    "    vec4 q = uShCasc[k] * vec4(p, 1.0);\n"                                                         \
    "    float r = uShParam.w / (float(textureSize(uShMap, 0).x) * uShPart[k]);\n"                      \
    "    float s = 0.0;\n"                                                                              \
    "    for (int y = -1; y <= 1; ++y)\n"                                                               \
    "        for (int x = -1; x <= 1; ++x)\n"                                                           \
    "            s += texture(uShMap, vec4((q.xy + vec2(x, y) * r) * uShPart[k], float(k), q.z));\n"    \
    "    return s / 9.0;\n"                                                                             \
    "}\n"                                                                                               \
    "bool sh_inside(int k, vec3 p, float m) {\n"                                                        \
    "    vec4 q = uShCasc[k] * vec4(p, 1.0);\n"                                                         \
    "    return all(greaterThan(q.xy, vec2(m))) && all(lessThan(q.xy, vec2(1.0 - m))) && q.z < 1.0;\n" \
    "}\n"                                                                                               \
    "vec4 sh_state;\n" /* the visibility, the facing, the cascade, 1 when computed */                  \
    "vec3 sh_n = vec3(0.0, 0.0, 1.0);\n"                                                                \
    "void sh_prepare() {\n" /* (in uniform control flow: the derivatives) */                           \
    "    sh_state = vec4(1.0, 0.0, -1.0, 0.0);\n"                                                       \
    "    if (uShPass != 0 || uShOn == 0) return;\n"                                                     \
    "    vec3 dx = dFdx(vShWorld), dy = dFdy(vShWorld);\n"                                              \
    "    vec3 n = cross(dx, dy);\n"                                                                     \
    "    float nl = length(n);\n"                                                                       \
    "    n = nl > 0.0 ? n / nl : vec3(0.0, 0.0, 1.0);\n"                                                \
    "    if (dot(n, uShCam - vShWorld) < 0.0) n = -n;\n"                                                \
    "    sh_n = n;\n"                                                                                   \
    "    float ndl = dot(n, uShSun);\n"                                                                 \
    "    float facing = smoothstep(0.0, 0.25, ndl);\n"                                                  \
    "#ifdef SH_POINTS\n"                                                                                \
    "    if (facing <= 0.0 && uShDebug == 0) {\n" /* back-facing world surfaces receive no sun shadow */ \
    "        sh_state = vec4(1.0, 0.0, -1.0, 1.0);\n"                                                   \
    "        return;\n"                                                                                 \
    "    }\n"                                                                                           \
    "#endif\n"                                                                                          \
    "    int count = int(uShParam.y);\n"                                                                \
    "    float vis = 1.0;\n"                                                                            \
    "    int casc = -1;\n"                                                                              \
    "    for (int k = 0; k < count; ++k) {\n"                                                           \
    "        vec3 p = vShWorld + n * (uShTexel[k] * 1.5) + uShSun * (uShTexel[k] * 1.0);\n"             \
    "        if (!sh_inside(k, p, 0.02)) continue;\n"                                                   \
    "        vis = sh_tap(k, p);\n"                                                                     \
    "        casc = k;\n"                                                                               \
    "        if (k + 1 < count && !sh_inside(k, p, 0.02 + uShParam.z)) {\n" /* the band: blend with the next */ \
    "            vec3 p2 = vShWorld + n * (uShTexel[k + 1] * 1.5) + uShSun * uShTexel[k + 1];\n"           \
    "            vec4 q = uShCasc[k] * vec4(p, 1.0);\n"                                                  \
    "            vec2 e = min(q.xy - 0.02, 0.98 - q.xy);\n"                                              \
    "            float t = clamp(min(e.x, e.y) / uShParam.z, 0.0, 1.0);\n"                               \
    "            vis = mix(sh_tap(k + 1, p2), vis, t);\n"                                                \
    "        }\n"                                                                                       \
    "        break;\n"                                                                                  \
    "    }\n"                                                                                           \
    "#if defined(SH_POINTS) || defined(SH_SPLIT)\n" /* one strength whatever blocks the sun, near or far: lit or not */ \
    "    vis = 1.0 - (1.0 - vis) * uShStrength;\n"                                                    \
    "    sh_vis = vis;\n"                                                                              \
    "    vis = 1.0 - (1.0 - vis) * uShSunK;\n"                                                         \
    "#endif\n"                                                                                          \
    "    sh_state = vec4(vis, facing, float(casc), 1.0);\n"                                             \
    "}\n"                                                                                               \
    "vec3 sh_points(vec3 c) {\n"                                                                        \
    "    if (uShOn == 0 || uPtN <= 0) return uShDebug == 3 && uShOn != 0 ? vec3(0.0) : c;\n"            \
    "    vec3 add = vec3(0.0);\n"                                                                       \
    "    vec4 cp = uPtVP * vec4(vShWorld, 1.0);\n" /* this pixel's tile: its lights alone */              \
    "    ivec2 ts = textureSize(uPtTiles, 0);\n"                                                         \
    "    vec2 t01 = cp.w > 0.0 ? cp.xy / cp.w * 0.5 + 0.5 : vec2(0.5);\n"                                 \
    "    uvec4 m = texelFetch(uPtTiles, clamp(ivec2(floor(t01 * vec2(ts))), ivec2(0), ts - 1), 0);\n"  \
    "    while ((m.x | m.y | m.z) != 0u) {\n"                                                           \
    "        int i;\n"                                                                                    \
    "        if (m.x != 0u) { i = findLSB(m.x); m.x &= m.x - 1u; }\n"                                   \
    "        else if (m.y != 0u) { i = 32 + findLSB(m.y); m.y &= m.y - 1u; }\n"                         \
    "        else { i = 64 + findLSB(m.z); m.z &= m.z - 1u; }\n"                                         \
    "        if (i >= uPtN) break;\n"                                                                     \
    "        vec3 L = uPtPos[i].xyz - vShWorld;\n"                                                      \
    "        float r = uPtPos[i].w, d2 = dot(L, L);\n"                                                  \
    "        if (d2 >= r * r || d2 <= 0.0) continue;\n"                                                 \
    "        float inv_d = inversesqrt(d2), d = d2 * inv_d;\n"                                          \
    "        float ndl_p = dot(sh_n, L) * inv_d;\n"                                                     \
    "        if (ndl_p <= 0.0) continue;\n"                                                             \
    "        float att = 1.0 - d / r;\n"                                                                \
    /* aimed down (a ceiling fixture's tube or bulb): a Lambert lobe below the light, the falloff squared - a pool under it */ \
    "        att = mix(att, att * att * max(L.z * inv_d, 0.0), uPtDown[i >> 2][i & 3]);\n"              \
    "        float sv = 1.0;\n"                                                                         \
    "        if (uPtCol[i].w >= 0.0) {\n" /* the cube (the integer part) and its shadow's weight (0.05..0.95: fading in or out) */ \
    "            float layer = floor(uPtCol[i].w), kw = clamp((fract(uPtCol[i].w) - 0.05) / 0.9, 0.0, 1.0);\n" \
    /* (off the surface by 0.04 + 3 % of the distance - some 8 of the cube's texels -: a moby lying flat on the floor - the round */ \
    /* plate under Novalis's yellow press - put the whole floor around it in its shadow with 0.04 alone) */ \
    "            vec3 v = vShWorld + sh_n * (0.04 + 0.03 * d) - uPtPos[i].xyz;\n"                       \
    "            float f = r, nn = uPtNear;\n"                                                          \
    "            vec3 a = abs(v);\n"                                                                    \
    "            float z = max(max(a.x, a.y), a.z);\n"                                                  \
    "            float ref = 0.5 * ((f + nn) / (f - nn) - (2.0 * f * nn) / ((f - nn) * z)) + 0.5 - 0.0002;\n" \
    "            float o = 0.015 * z;\n"                                                                \
    "            sv = 0.25 * (texture(uPtMap, vec4(v + vec3(o, o, 0.0), layer), ref)\n"           \
    "                 + texture(uPtMap, vec4(v + vec3(-o, 0.0, o), layer), ref)\n"                \
    "                 + texture(uPtMap, vec4(v + vec3(0.0, -o, -o), layer), ref)\n"               \
    "                 + texture(uPtMap, vec4(v + vec3(o, -o, o), layer), ref));\n"                \
    "            sv = mix(1.0, sv, kw);\n"                                                              \
    "        }\n"                                                                                       \
    "        add += uPtCol[i].rgb * (ndl_p * att * 0.5 * sv);\n"                                        \
    "    }\n"                                                                                           \
    "    if (uShDebug == 3) return add;\n"                                                              \
    "    return c + add;\n"                                                                             \
    "}\n"                                                                                               \
    "vec3 sh_apply_split(vec3 full, vec3 nokey) {\n" /* the colour lit with / without the key light: mixed by the visibility */ \
    "    if (sh_state.w == 0.0) return full;\n"                                                        \
    "    if (uShDebug == 2) return vec3(sh_state.x);\n"                                                \
    "    return mix(nokey, full, sh_state.x);\n"                                                       \
    "}\n"                                                                                              \
    "vec3 sh_apply(vec3 c) {\n"                                                                         \
    "    if (sh_state.w == 0.0) return c;\n"                                                            \
    "    if (uShDebug == 2) return vec3(sh_state.x * max(sh_state.y, 0.15));\n"                         \
    "    float k = 1.0 - uShParam.x * (1.0 - sh_state.x) * sh_state.y;\n"                               \
    "    vec3 o = c * k;\n"                                                                             \
    "#ifdef SH_POINTS\n" /* the world relit by our sun: the game's key light term (base + colour max(n.l, k n.l)) taken off, */ \
    "    if (uShKey.w >= 0.0) {\n" /* the same light's term from the sun's direction, times its visibility, put back */ \
    "        float ndg = dot(sh_n, uShKeyDir), nds = dot(sh_n, uShSun);\n"                             \
    "        vec3 kg = uShKey.rgb * max(ndg, uShKey.w * ndg), ks = uShKey.rgb * max(nds, uShKey.w * nds);\n" \
    "        o = max(c + uShSunK * (ks * sh_vis - kg), vec3(0.0));\n"                                  \
    "    }\n"                                                                                          \
    "#endif\n"                                                                                         \
    "    if (uShDebug == 1) {\n"                                                                        \
    "        int ci = int(sh_state.z);\n"                                                               \
    "        vec3 tint = ci == 0 ? vec3(1.0, 0.6, 0.6) : ci == 1 ? vec3(0.6, 1.0, 0.6) : ci == 2 ? vec3(0.6, 0.6, 1.0) : vec3(1.0);\n" \
    "        o *= tint;\n"                                                                              \
    "    }\n"                                                                                           \
    "    return o;\n"                                                                                   \
    "}\n"

#endif  // RAC1_GAME_RENDER_MRP_LIGHTING_H
