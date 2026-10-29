// orig: (host) The MRP's level of detail slider (see mrp_lod.h).
#include <game/render/mrp_lod.h>
#include <game/render/mrp_fx.h>

#include <game/debug_hooks.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/world_cull.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/moby_shadows.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_scene.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_world_scene.h>
#include <game/render/mrp_world_static.h>
#include <game/render/mrp_tfrag_gpu.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/ee_journal.h>
#include <runtime/vu/vu0.h>
#include <profiler/host_profiler.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1 {

// (ovl_1e_1f.cpp) the collision routines' contact registers vf25..vf29, which the port keeps outside the VU0 state: saved and
// put back around each shadow run as the VU0 registers are
void collide_vu_save(float out[15]);
void collide_vu_restore(const float in[15]);

int g_mrp_lod = 0;
bool g_mrp_lod_widen_off = false;
bool g_mrp_lod_frame_drawn = true;
bool g_mrp_wide_fill = true;
bool g_mrp_world_wide_external = false;
bool g_mrp_world_lod_external = false;
namespace {
float g_frame_widen = 1.0f;  // the widening measured at the frame's tfrag pass
float g_frame_widen_snap = 1.0f;  // (captured for a pipelined frame)
float g_frame_side[2] = {1.0f, 1.0f};  // fill_widen / fill_widen_y at the frame's tfrag pass (mrp_lod_side_widen)
}
// orig: (host)
float mrp_lod_frame_widen() { return g_world_mrp ? (g_draw_from_snapshot ? g_frame_widen_snap : g_frame_widen) : 1.0f; }
// orig: (host)
void mrp_lod_snapshot() { g_frame_widen_snap = g_frame_widen; }

namespace {
// the widening the shadow runs give the passes' frustum (1: none): the view's, when the sides are filled
float fill_widen() { return g_mrp_lod_widen_off || !g_mrp_wide_fill ? 1.0f : render_view_cull_widen(); }
// the same for the y tangent (the field of view's scale)
float fill_widen_y() { return g_mrp_lod_widen_off || !g_mrp_wide_fill ? 1.0f : render_view_cull_widen_y(); }
}  // namespace

// orig: (host) see mrp_lod.h
bool mrp_lod_world_shadows_needed(int kind, float* widen) {
    const bool world = kind >= 0 && kind <= 2;
    float w = world && g_mrp_world_wide_external ? 1.0f : fill_widen();
    const float s = kind == 3 || (world && g_mrp_world_lod_external) ? 1.0f : mrp_lod_scale(g_mrp_lod);
    if (widen != nullptr) *widen = w;
    return s != 1.0f || w > 1.0f;
}

// orig: (host)
float mrp_lod_scale(int v) {
    if (v <= 0) return 1.0f;
    if (v >= 100) return 0.0f;
    return std::pow(16.0f, static_cast<float>(v) / 100.0f);
}

namespace {

struct Stats {
    uint64_t runs[4] = {}, skipped_vu0 = 0, overflow[4] = {}, pages = 0, bytes[4] = {}, real_bytes[4] = {}, widened[4] = {};
    uint64_t verify_runs = 0, verify_bad = 0, selftest_bad[4] = {}, not_drawn = 0;
    double ms[4] = {};
} g_st;

// The distances "unlimited" stands for: the world's far plane is the renderer's (render_view.h kRenderFar, 4000 units); the
// passes compare view depths (x 1 for tie / shrub, x 1024 integers for tfrag) against these.
constexpr float kFar = 1.0e6f;
constexpr int32_t kFarInt = 0x3fffffff;

float scaled(float d, float s) { return s == 0.0f ? kFar : std::min(d * s, kFar); }
uint32_t scaled_bits(uint32_t bits, float s) { return f2u(scaled(u2f(bits), s)); }
int32_t scaled_int(int32_t d, float s) {
    if (s == 0.0f) return kFarInt;
    const double v = static_cast<double>(d) * static_cast<double>(s);
    return v > static_cast<double>(kFarInt) ? kFarInt : static_cast<int32_t>(v);
}

// The distances the passes read, scaled in the journaled memory (put back by the restore).
// tfrag 0x294368: the LOD bands, three integers at 0x160f70 (the depth x 1024 of the entry's sphere against them picks the
// chain's LOD code), the draw distance 0x160f80 (a float, vf23.y).
void patch_tfrag(float s) {
    for (uint32_t k = 0; k < 3; ++k) sw(OV(0x00160f70u) + 4 * k, static_cast<uint32_t>(scaled_int(lw(OV(0x00160f70u) + 4 * k), s)));
    swc1(OV(0x00160f80u), scaled(lwc1(OV(0x00160f80u)), s));
}
// tie 0x295fa0: per class record (the list 0x1c5800, *(0x160fbc) of them): +0x10 / +0x14 / +0x18 the LOD distances (near,
// mid, far: vf16), its instances' 0x20-byte bounds at +0x28 (+0x26 of them) with the draw distance at +0x14, taken as the
// minimum with the pass's maximum *(0x160fe0) (pminw: positive floats compared as integers).
void patch_tie(float s) {
    sw(OV(0x00160fe0u), scaled_bits(lwu(OV(0x00160fe0u)), s));
    const int32_t n = lw(OV(0x00160fbcu));
    for (int32_t c = 0; c < n && c < 4096; ++c) {
        const uint32_t r = lwu(OV(0x001c5800u) + 4 * static_cast<uint32_t>(c));
        if (r == 0 || r >= 0x02000000u) continue;
        for (uint32_t k = 0; k < 3; ++k) swc1(r + 0x10 + 4 * k, scaled(lwc1(r + 0x10 + 4 * k), s));
        const uint32_t b = lwu(r + 0x28), m = lhu(r + 0x26);
        if (b == 0 || b >= 0x02000000u) continue;
        for (uint32_t i = 0; i < m; ++i) sw(b + i * 0x20 + 0x14, scaled_bits(lwu(b + i * 0x20 + 0x14), s));
    }
}
// shrub 0x286f10: per class record (the list 0x1bc030, *(0x16048c) of them): its instances' bounds at +0x18 (+0x16 of them),
// the draw distance at +0x10, minimum with the pass's maximum *(0x1604a4); the fade near it follows (alpha from the distance
// left). a class whose record +0x1c is non-zero has a far variant (its packets at record +0x40, +0x28 of them,
// drawn from the scratchpad list 0x2c00): past the instance's switch distance (bounds byte +0x17, in units) the shrub
// crossfades over 8 units into it (the shrub's alpha 0x80 - (depth - d) x 16, the far variant's the reverse, 0x2873a0..
// 0x28748c), and is replaced by it beyond; byte 0 = always the far variant. Scaled with the slider (capped at 255 units); at
// max the far variant is off (+0x1c = 0: the shrub itself at any distance, opaque).
void patch_shrub(float s) {
    sw(OV(0x001604a4u), scaled_bits(lwu(OV(0x001604a4u)), s));
    const int32_t n = lw(OV(0x0016048cu));
    for (int32_t c = 0; c < n && c < 4096; ++c) {
        const uint32_t r = lwu(OV(0x001bc030u) + 4 * static_cast<uint32_t>(c));
        if (r == 0 || r >= 0x02000000u) continue;
        const uint32_t b = lwu(r + 0x18), m = lhu(r + 0x16);
        if (b == 0 || b >= 0x02000000u) continue;
        for (uint32_t i = 0; i < m; ++i) sw(b + i * 0x20 + 0x10, scaled_bits(lwu(b + i * 0x20 + 0x10), s));
        static const bool keep_fade = std::getenv("RAC1_MRP_LOD_KEEP_FADE") != nullptr;  // (testing: the switch distance not scaled)
        if (lwu(r + 0x1c) == 0 || keep_fade) continue;
        if (s == 0.0f) {
            sw(r + 0x1c, 0);
            continue;
        }
        for (uint32_t i = 0; i < m; ++i) {
            const double d = static_cast<double>(lbu(b + i * 0x20 + 0x17)) * static_cast<double>(s);
            sb(b + i * 0x20 + 0x17, static_cast<uint32_t>(d > 255.0 ? 255.0 : d));
        }
    }
}

// The widescreen part: the passes' frustum tests read the projection's x tangent 0x16caf0 (the sphere tests' slope:
// tfrag vf24, tie / shrub vf20, the moby draw loop, the particles vf26; the sky's focal length 0x16cc40 / tangent and its clusters'
// test, 0x28c3c0) and the radius factor sqrt(1 + tangent^2) that projection_setup 0x1f7980 derives from it, at 0x16caf8 (tfrag's
// vf24.z) and its copy 0x16cc20 (vf26 / vf27). During the update the projection is the game's own 4:3 one (the
// host's widened one is put back before each update: render_view_projection_undo), so the shadow run widens the tangent and runs projection_setup in the
// journal: every constant derived from the tangent follows (patching 0x16caf0 / 0x16cc20 only would leave tfrag the 4:3 radius
// factor 0x16caf8 - the ground at the sides missing in widescreen). The y tangent (the setup's host hook given the
// game's own) is widened by the field of view's scale only (the renderer's y tangent is the game's times it); the view's
// projections A (render_view_setup 0x1f6e48: 0x166d00 / 0x166d40, which the particles and the sky project with) are not
// rebuilt: what the pass projects stays in the game's 4:3 frame (the sky's shadow: widen_projection).
// The tie pass's second test (the eight corners of the instance's box through the clip matrix, 0x2963f4..) scales x by
// 0x160f90 x before VCLIPW: divided by the widening. (Its "needs clipping" flag follows: an instance inside the widened view is
// drawn by tie A; one across its edge by tie B, whose flagged triangles the MRP then draws whole, mrp_world_renderer.cpp.)
float g_sky_y_base = 0.0f;
float sky_y_base(float) { return g_sky_y_base; }
// The shadow run's patch of the projection, while its pass runs: the game's own code may set its projection up again in
// the middle of a pass (Kerwan's class 994 surface, L3:0x291218 / 0x291808: projection_setup + render_view_setup, then again with
// its fog put back; Aridia's ocean, Umbris, Pokitaru, the Vendor call them too). On the widened tangent that rebuilt the whole
// projection, the rows A 0x166cc0.. included, and everything the rest of the shadow's pass projected (the quad drawer's glows, the
// ships' canopy) came out in a projection narrowed by the widening, unlike the real pass's: drawn from the shadow's chain (the
// effects shadow's substitute), it slid sideways with the camera's turn and the window's aspect. The setup hooks below
// (MrpLodSetupScope, in projection_setup / render_view_setup) give the game's call the game's own tangent and put the shadow's
// patch back after it.
enum PatchType : int { kPatchNone = 0, kPatchCull = 1, kPatchWiden = 2 };
struct ShadowPatch {
    int type = kPatchNone;
    float w = 1.0f, wy = 1.0f;
    float t_game = 0.0f, t_patched = 0.0f;  // the tangent 0x16caf0: the game's, and the shadow's widened one
    float q_game = 0.0f, q_patched = 0.0f;  // (kPatchCull) the quad drawer's reject scale 0x160be4: the game's, the shadow's
} g_patch;
int g_patch_host = 0;  // the host's own patching calls the setups (not hooked)
void patch_clear() { g_patch = ShadowPatch{}; }
void widen_tangent(float w, float wy = 1.0f) {
    const float t = lwc1(OV(0x0016caf0u));
    float (*const hook)(float) = g_debug_fov_y_base;
    g_sky_y_base = (hook != nullptr ? hook(t) : t) * wy;
    g_debug_fov_y_base = &sky_y_base;
    swc1(OV(0x0016caf0u), t * w);
    projection_setup();
    g_debug_fov_y_base = hook;
}
// The frustum the passes test against: the tangent and what projection_setup derives from it, and the clip matrix 0x166d80
// (render_view_setup 0x1f6e48: the rotation times projection_setup's 0x16cb80 rows; tfrag's vf12..vf15, the tie pass's box test,
// with the 4:3 one tfrag dropped the ground at the sides) rebuilt; the projections A 0x166cc0..0x166d7f put back (the
// passes' own projected output - the particles - stays in the game's 4:3 frame). `wy`: the y tangent's widening (the field of
// view's scale, fill_widen_y): with the y tangent kept, a Field of View above x1.00 culled what lies in the bands above and below the
// 4:3 frame's height.
void patch_widen(float w, int kind, float wy) {
    static const bool old = std::getenv("RAC1_MRP_WIDEN_0338") != nullptr;  // (testing: patch only the tangent and 0x16cc20)
    if (old) {
        const float t = lwc1(OV(0x0016caf0u)) * w;
        swc1(OV(0x0016caf0u), t);
        swc1(OV(0x0016cc20u), std::sqrt(1.0f + t * t));
        if (kind == 1) swc1(OV(0x00160f90u), lwc1(OV(0x00160f90u)) / w);
        return;
    }
    ++g_patch_host;
    const float t = lwc1(OV(0x0016caf0u));
    widen_tangent(w, wy);
    uint8_t a[0xc0];
    std::memcpy(a, ee_ram_ptr(OV(0x00166cc0u)), sizeof a);
    render_view_setup();
    std::memcpy(ee_ram_ptr(OV(0x00166cc0u)), a, sizeof a);
    --g_patch_host;
    g_patch = ShadowPatch{kPatchWiden, w, wy, t, lwc1(OV(0x0016caf0u))};
}
// The 3D effects' shadow: the sphere test's constants only - the tangent 0x16caf0 and the radius factor sqrt(1 + t^2)
// at 0x16caf8 and its copy 0x16cc20 (as projection_setup computes it: measured 1.08235 for t = 0.414112) -, nothing else of the
// projection: with projection_setup's whole output widened (its clip matrices 0x16cb00..) effects inside the 4:3 frame changed too
// (Veldin's intro, frame 400: orange glows inside Ratchet's pod that neither the 4:3 image nor the game's own widened one shows).
// The frustum terms point_on_screen 0x2002e0 tests a point (and its radius) against - the draw lists' callbacks cull the cells of
// the levels' water, lava and sand-sea grids with it (Gaspar's lava, class 317's L9:0x2ef750 -> 0x2c1978: a cell drawn when
// point_on_screen(cell, 256) >= 0) -: projection_setup's copies at 0x16cd00 (the x / y tangents, then the radius factors
// sqrt(1 + t^2)), 0x16cd10 (the guard band's factors: 4, the screen's half size over its centre) and 0x16cd20 (the radius
// factors again, the guard test's radius scale). The sides' test (|x| - r x radius factor <= tangent x depth) widened by `w` / `wy`
// (the tangents and both radius factors); the guard band's test (|x| + r x radius factor x 0x16cd28 <= tangent x guard factor x
// depth: the point's projection inside the 4:3 projection's guard band, else 0: the callback's packets clipped) kept on the
// game's guard band, its factor divided by the widening (a cell past it is clipped, as the game clips one: the shadow's packets
// are projected with the game's own 4:3 projection). Before, the shadows widened only 0x16caf0 / 0x16caf8 / 0x16cc20, and those
// cells were culled at the 4:3 sides: the lava, water and sand sea missing at the far sides of a 32:9 view.
void patch_frustum_terms(float w, float wy) {
    const float s[2] = {w, wy};
    for (uint32_t k = 0; k < 2; ++k) {
        if (!(s[k] > 1.0f)) continue;
        const float t = lwc1(OV(0x0016cd00u) + 4 * k) * s[k];
        const float r = std::sqrt(1.0f + t * t);
        swc1(OV(0x0016cd00u) + 4 * k, t);
        swc1(OV(0x0016cd08u) + 4 * k, r);
        swc1(OV(0x0016cd20u) + 4 * k, r);
        swc1(OV(0x0016cd10u) + 4 * k, lwc1(OV(0x0016cd10u) + 4 * k) / s[k]);
    }
}
void patch_cull(float w, float wy) {
    const float t0 = lwc1(OV(0x0016caf0u));
    const float t = w > 1.0f ? t0 * w : t0;
    if (w > 1.0f) {
        const float r = std::sqrt(1.0f + t * t);
        swc1(OV(0x0016caf0u), t);
        swc1(OV(0x0016caf8u), r);
        swc1(OV(0x0016cc20u), r);
    }
    static const bool terms_off = std::getenv("RAC1_MRP_WIDEN_TERMS") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_TERMS")) == 0;  // (testing: before)
    // the quad drawer 0x1fd228 (the draw lists' effect quads: Rilgar's water, class 79's L5:0x2d7920; the glows, the decals) rejects a
    // quad whose four corners are all past one side of the guard band 0x16cbc0 (1 / 1024, 1 / 832: the GS's half extents) scaled by
    // 0x160be4 (1.5): +-683 pixels of the 4:3 frame's projection, which a 32:9 view's sides pass (+-883 at 5053 x 1371): the quads there
    // were rejected, the water cut at the far sides. That scale divided by the widening (x and y share it: the larger); the guard band's
    // own clipping (0x1fd89c) left as it is
    float q = lwc1(OV(0x00160be4u));
    if (g_patch.type == kPatchCull && q == g_patch.q_patched) q = g_patch.q_game;  // (a re-apply: the game's own)
    const float qs = std::max(w, wy), qp = !terms_off && qs > 1.0f ? q / qs : q;
    swc1(OV(0x00160be4u), qp);
    if (!terms_off) patch_frustum_terms(w, wy);
    g_patch = ShadowPatch{kPatchCull, w, wy, t0, t, q, qp};
}
// The sky (0x23d968) projects its shells itself with the frame's projection (render_view_setup 0x1f6e48: A 0x166d00 from
// projection_setup 0x1f7980's matrices), and the MRP draws it as the directions A sees its screen points in: a 4:3 A covers
// the 4:3 frustum only. Its shadow is run with the whole projection made from the widened tangent, the A it used kept for the
// scene. The y tangent is widened by the field of view's scale `wy` (the renderer's y tangent is the game's times it,
// render_view_projection): with the y tangent kept, a Field of View above x1.00 drew the sky's clusters of the 4:3 height only,
// the clear colour in bands above and below them (issue #18).
void widen_projection(float w, float wy) {
    widen_tangent(w, wy);
    render_view_setup();
}

// The shadow block running (mrp_lod_shadow_slot: -1 none) and the widening its draw lists' clip matrix gets
int g_slot = -1;
float g_slot_w = 1.0f, g_slot_wy = 1.0f;
// The draw lists' clip matrix 0x166d80 widened (mrp_lod_draw_list_widen): its x / y columns (the matrix is applied to a row vector,
// clip = (x y z 1) M: element [r][0] of each row makes clip x) divided by the widening, so that |x| <= w and |y| <= w hold over the
// widened view; `saved` the matrix as it was
struct ClipWiden {
    bool on = false;
    float saved[16] = {};
} g_clip;
void clip_widen_apply() {
    for (uint32_t i = 0; i < 16; ++i) g_clip.saved[i] = lwc1(OV(0x00166d80u) + 4 * i);
    for (uint32_t r = 0; r < 4; ++r) {
        swc1(OV(0x00166d80u) + 0x10 * r, g_clip.saved[4 * r] / g_slot_w);
        swc1(OV(0x00166d84u) + 0x10 * r, g_clip.saved[4 * r + 1] / g_slot_wy);
    }
}

}  // namespace

// orig: (host) see mrp_lod.h
int mrp_lod_shadow_slot() { return g_slot; }

// orig: (host) see mrp_lod.h
void mrp_lod_draw_list_widen(bool begin) {
    if (begin) {
        if (g_slot < 0 || g_clip.on || !(g_slot_w > 1.0f || g_slot_wy > 1.0f)) return;
        static const bool off = std::getenv("RAC1_MRP_WIDEN_LISTS") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_LISTS")) == 0;  // (testing)
        if (off) return;
        g_clip.on = true;
        clip_widen_apply();
    } else if (g_clip.on) {  // (the rest of the block - the HUD, the menus' 3D - sees the game's own; the journal puts it back too)
        for (uint32_t i = 0; i < 16; ++i) swc1(OV(0x00166d80u) + 4 * i, g_clip.saved[i]);
        g_clip.on = false;
    }
}

// orig: (host) see mrp_lod.h
MrpLodSetupScope::MrpLodSetupScope(bool view) : view_(view) {
    static const bool off = std::getenv("RAC1_MRP_SHADOW_SETUP") != nullptr && std::atoi(std::getenv("RAC1_MRP_SHADOW_SETUP")) == 0;  // (testing: before)
    on_ = !off && g_patch.type != kPatchNone && g_patch_host == 0;
    if (!on_ || view_) return;
    // projection_setup: the game's own tangent (unless the game wrote one of its own since: then that one is the game's)
    if (lwc1(OV(0x0016caf0u)) == g_patch.t_patched) swc1(OV(0x0016caf0u), g_patch.t_game);
}
MrpLodSetupScope::~MrpLodSetupScope() {
    if (!on_) return;
    ++g_patch_host;
    if (!view_) {  // the game's projection made: the shadow's patch put back on the tangent it used
        const float t = lwc1(OV(0x0016caf0u));
        if (g_patch.type == kPatchCull) {
            patch_cull(g_patch.w, g_patch.wy);
        } else {
            widen_tangent(g_patch.w, g_patch.wy);
            g_patch = ShadowPatch{kPatchWiden, g_patch.w, g_patch.wy, t, lwc1(OV(0x0016caf0u))};
        }
    } else if (g_patch.type == kPatchWiden) {
        // render_view_setup on patch_widen's widened projection: the clip matrix 0x166d80 the widened one (the passes' tests), the rows
        // A 0x166cc0..0x166d7f (what the pass projects with) made from the game's own projection, as patch_widen keeps them
        const float w = g_patch.w, wy = g_patch.wy;
        if (lwc1(OV(0x0016caf0u)) == g_patch.t_patched) swc1(OV(0x0016caf0u), g_patch.t_game);
        const float t = lwc1(OV(0x0016caf0u));
        projection_setup();
        render_view_setup();
        uint8_t a[0xc0];
        std::memcpy(a, ee_ram_ptr(OV(0x00166cc0u)), sizeof a);
        widen_tangent(w, wy);
        render_view_setup();
        std::memcpy(ee_ram_ptr(OV(0x00166cc0u)), a, sizeof a);
        g_patch = ShadowPatch{kPatchWiden, w, wy, t, lwc1(OV(0x0016caf0u))};
    }  // (kPatchCull: the projection stays the game's own, render_view_setup's output too)
    if (view_ && g_clip.on) clip_widen_apply();  // (a draw list's callback rebuilt the view: its clip matrix widened again)
    --g_patch_host;
}

// orig: (host) see mrp_lod.h
bool mrp_lod_reference() {
    static const bool on = std::getenv("RAC1_WIDEN_REF") != nullptr && std::atoi(std::getenv("RAC1_WIDEN_REF")) != 0;
    return on;
}
// orig: (host) see mrp_lod.h
void mrp_lod_frame_begin() {
    if (!mrp_lod_reference()) return;
    g_mrp_lod_widen_off = true;  // (the game's own culling widened: no shadow run)
    const float w = render_view_cull_widen(), t = lwc1(OV(0x0016caf0u));
    if (!(w > 1.0f)) return;
    widen_projection(w, render_view_cull_widen_y());
    render_view_note_write(lwc1(OV(0x0016caf0u)), t);
}

namespace {
void probe(int kind) {
    static int done = 0;
    if ((done & (1 << kind)) != 0) return;
    done |= 1 << kind;
    if (kind == 0) {
        std::printf("[lod] packet cursor %08x, limit %08x\n", lwu(OV(0x001611c0u)), lwu(OV(0x001611c8u)));
        std::printf("[lod] tfrag bands %d %d %d %d, draw distance %g\n", lw(OV(0x00160f70u)), lw(OV(0x00160f74u)), lw(OV(0x00160f78u)), lw(OV(0x00160f7cu)),
                    lwc1(OV(0x00160f80u)));
    } else if (kind == 1) {
        const int32_t n = lw(OV(0x00160fbcu));
        std::printf("[lod] tie max %g, %d classes\n", lwc1(OV(0x00160fe0u)), n);
        for (int32_t c = 0; c < n && c < 6; ++c) {
            const uint32_t r = lwu(OV(0x001c5800u) + 4 * static_cast<uint32_t>(c));
            const uint32_t b = lwu(r + 0x28);
            std::printf("[lod]  tie %08x lod %g %g %g %g, %u instances, first's distance %g\n", r, lwc1(r + 0x10), lwc1(r + 0x14), lwc1(r + 0x18), lwc1(r + 0x1c), lhu(r + 0x26),
                        lhu(r + 0x26) != 0 ? lwc1(b + 0x14) : 0.0f);
        }
    } else {
        const int32_t n = lw(OV(0x0016048cu));
        std::printf("[lod] shrub max %g, %d classes\n", lwc1(OV(0x001604a4u)), n);
        for (int32_t c = 0; c < n && c < 64; ++c) {
            const uint32_t r = lwu(OV(0x001bc030u) + 4 * static_cast<uint32_t>(c));
            const uint32_t b = lwu(r + 0x18);
            std::printf("[lod]  shrub %08x, %u instances, first's distance %g, far variant %08x, switch %u\n", r, lhu(r + 0x16), lhu(r + 0x16) != 0 ? lwc1(b + 0x10) : 0.0f,
                        lwu(r + 0x1c), lhu(r + 0x16) != 0 ? lbu(b + 0x17) : 0u);
        }
    }
}

std::vector<uint8_t> g_verify_ram, g_verify_spr;

uint64_t g_collide_vu_put_back = 0;  // (the shadow runs that changed the collision's contact registers: put back)
struct Vu0Save {
    float coll[15];  // the collision routines' contact registers vf25..vf29, kept by the port outside the VU0 state
    std::array<VUVector, 32> vf;
    std::array<int16_t, 16> vi;
    VUVector acc;
    float q, i, r, p;
    uint32_t macflag, statusflag, clipflag, mac_visible, status_visible, clip_visible, xtop;
    alignas(16) uint8_t mem[4096];
    std::array<uint64_t, 512> micro;
    VuMicroRun run;
    uint32_t cmsar0 = 0;
    bool full = false;
    explicit Vu0Save(bool save_mem_and_micro) : full(save_mem_and_micro) {
        const vu0::Vu0& v = vu0::global();
        const VUState& s = v.st;
        collide_vu_save(coll);
        vf = s.vf; vi = s.vi; acc = s.acc;
        q = s.q; i = s.i; r = s.r; p = s.p;
        macflag = s.macflag; statusflag = s.statusflag; clipflag = s.clipflag;
        mac_visible = s.mac_visible; status_visible = s.status_visible; clip_visible = s.clip_visible;
        xtop = s.xtop;
        if (full) {
            std::memcpy(mem, s.mem.data(), 4096);
            micro = v.micro;
            run = v.run;
            cmsar0 = v.cmsar0;
        }
    }
    void restore() const {
        vu0::Vu0& v = vu0::global();
        VUState& s = v.st;
        {
            float now[15];
            collide_vu_save(now);
            if (std::memcmp(now, coll, sizeof coll) != 0) ++g_collide_vu_put_back;
            collide_vu_restore(coll);
        }
        s.vf = vf; s.vi = vi; s.acc = acc;
        s.q = q; s.i = i; s.r = r; s.p = p;
        s.macflag = macflag; s.statusflag = statusflag; s.clipflag = clipflag;
        s.mac_visible = mac_visible; s.status_visible = status_visible; s.clip_visible = clip_visible;
        s.xtop = xtop;
        if (full) {
            std::memcpy(s.mem.data(), mem, 4096);
            v.micro = micro;
            v.run = run;
            v.cmsar0 = cmsar0;
        }
    }
};

}  // namespace

// The shadow chain of the pass being run (kept until the real pass has run: its end, the self-test's comparison).
struct Pending {
    bool on = false;
    int kind = 0;
    uint32_t cur = 0;
    std::vector<uint8_t> bytes;
    std::vector<std::pair<uint64_t, uint32_t>> sky_tex;
    std::array<float, 16> sky_a{};
} g_pending;

// orig: (host) see mrp_lod.h
void mrp_lod_world_shadow(int kind, void (*pass)(), uint32_t cur) {
    RAC1_PROF_ZONE(kGameLodOther);
    static const bool selftest = std::getenv("RAC1_MRP_LOD_SELFTEST") != nullptr;  // (testing: the shadow run with nothing scaled)
    static const bool verify = std::getenv("RAC1_MRP_LOD_VERIFY") != nullptr;      // (testing: the whole memory compared after)
    static const bool probe_env = std::getenv("RAC1_MRP_LOD_PROBE") != nullptr;
    g_pending.on = false;
    if (kind < 0 || kind > 3 || !g_world_mrp) return;
    // the whole world made by the MRP's host code - the tfrags on the GPU with the tfrag pass's decisions (mrp_tfrag_gpu.h),
    // the ties / shrubs by mrp_world_static.h's -: the world is culled by something else than the game's passes, no world
    // shadow run for the LOD slider nor the widening (the sky's, the mobys', the effects' stay)
    if (kind == 0) g_mrp_world_wide_external = g_mrp_world_lod_external = !selftest && mrp_world_static_lod() && mrp_world_tfrag_lod();
    const float s = kind == 3 || (kind <= 2 && g_mrp_world_lod_external) ? 1.0f : mrp_lod_scale(g_mrp_lod);  // (the sky: no distances, the widening only)
    // (the GS replica's narrowing of the game's projected 3D follows the view, whether the sides are filled or not)
    if (kind == 0) g_frame_widen = g_mrp_lod_widen_off ? 1.0f : render_view_cull_widen();
    if (kind == 0) {  // (the widening the frame's shadow runs give the view: the GS replica's side tests, mrp_lod_side_widen)
        g_frame_side[0] = fill_widen();
        g_frame_side[1] = fill_widen_y();
    }
    float widen = 1.0f;
    const bool needed = mrp_lod_world_shadows_needed(kind, &widen);
    if (kind <= 2) {  // (the passes whose decisions the MRP's host code makes from the pass's inputs: no shadow run)
        const float s_req = mrp_lod_scale(g_mrp_lod), w_req = fill_widen();  // (what the slider / the widening ask, whoever makes it)
        const bool asked = s_req != 1.0f || w_req > 1.0f;
        if (kind == 0) {
            const bool host = asked && !selftest && mrp_world_tfrag_lod();
            if (w_req > 1.0f && mrpw::tfrag_gpu_enabled() && !vu0::running()) {  // (the widened view's values, as the shadow run's patch_widen makes them, read in a journal)
                const Vu0Save vu0_saved(false);
                const auto hw_saved = eer::hwregs();
                ee_journal_begin();
                patch_widen(w_req, 0, fill_widen_y());
                mrpw::tfrag_note_pass(host, s_req, w_req);
                patch_clear();
                g_st.pages += ee_journal_restore();
                vu0_saved.restore();
                eer::hwregs() = hw_saved;
            } else {
                mrpw::tfrag_note_pass(host && w_req <= 1.0f, s_req, 1.0f);  // (a VU0 program running: the shadow run instead, below)
            }
            if (host) return;
        } else {
            const bool host = asked && !selftest && mrp_world_static_lod();
            mrpw::static_note_pass(kind, host, s_req, w_req, fill_widen_y());
            if (host) return;
        }
    }
    if (!needed && !selftest) return;
    if (!g_mrp_lod_frame_drawn) {  // (a game frame not drawn: nothing to make)
        ++g_st.not_drawn;
        return;
    }
    if (probe_env && kind < 3) probe(kind);
    if (vu0::running()) {  // (a VU0 program the pass would have to finish first: not run twice)
        ++g_st.skipped_vu0;
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    if (verify) {
        g_verify_ram.assign(ee_get_ram_base(), ee_get_ram_base() + 0x04000000u);
        g_verify_spr.assign(ee_get_scratchpad_base(), ee_get_scratchpad_base() + 0x4000u);
    }
    // the host state the pass changes besides EE memory: the VU0 registers (macro mode), the DMA registers its scratchpad
    // transfers go through, the visible lists the passes note
    const Vu0Save vu0_saved(true);
    const auto hw_saved = eer::hwregs();
    const WorldVisible vis_saved = world_visible();
    ee_journal_begin();
    if (!selftest) {
        if (kind == 0) patch_tfrag(s);
        else if (kind == 1) patch_tie(s);
        else if (kind == 2) patch_shrub(s);
        if (widen > 1.0f) {
            if (kind == 3) widen_projection(widen, fill_widen_y());
            else patch_widen(widen, kind, fill_widen_y());
            ++g_st.widened[kind];
        }
    }
    // the shadow chain where the real one will be (the passes pack chain addresses as 20-bit quadword indices: it stays in the
    // frame packet buffer: the two buffers of *(0x1611cc) bytes at *(0x173e04) / *(0x173e08), memory_layout 0x23d520)
    const uint32_t buf_b = lwu(OV(0x00173e08u)), limit = (cur >= buf_b ? buf_b : lwu(OV(0x00173e04u))) + lwu(OV(0x001611ccu));
    pass();
    patch_clear();
    const uint32_t end = lwu(OV(0x001611c0u));
    const bool ok = end > cur && end <= limit;
    g_pending.sky_tex.clear();
    if (ok) {
        g_pending.bytes.resize(end - cur);
        std::memcpy(g_pending.bytes.data(), ee_ram_ptr(cur), g_pending.bytes.size());
        if (kind == 3) {  // (the TEX0 the shadow's sky drawers cached and the A it projected with: put back below)
            mrpw::sky_texture_entries(g_pending.sky_tex);
            for (uint32_t k = 0; k < 16; ++k) g_pending.sky_a[k] = lwc1(OV(0x00166d00u) + 4 * k);
        }
    }
    g_st.pages += ee_journal_restore();
    vu0_saved.restore();
    eer::hwregs() = hw_saved;
    world_visible_restore(vis_saved);
    if (verify) {
        ++g_st.verify_runs;
        const bool same = std::memcmp(g_verify_ram.data(), ee_get_ram_base(), g_verify_ram.size()) == 0 &&
                          std::memcmp(g_verify_spr.data(), ee_get_scratchpad_base(), g_verify_spr.size()) == 0;
        if (!same) {
            ++g_st.verify_bad;
            if (g_st.verify_bad <= 5) std::fprintf(stderr, "[lod] VERIFY: the memory differs after the shadow run of pass %d\n", kind);
        }
    }
    g_st.ms[kind] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) {
        if (g_st.overflow[kind] < 3) std::fprintf(stderr, "[lod] pass %d: shadow chain %08x..%08x past the buffer's limit %08x\n", kind, cur, end, limit);
        ++g_st.overflow[kind];
        return;
    }
    g_pending.on = true;
    g_pending.kind = kind;
    g_pending.cur = cur;
}

// orig: (host) see mrp_lod.h
void mrp_lod_world_after(int kind, uint32_t cur, uint32_t real_end) {
    static const bool selftest = std::getenv("RAC1_MRP_LOD_SELFTEST") != nullptr;
    if (!g_pending.on || g_pending.kind != kind || g_pending.cur != cur) return;
    g_pending.on = false;
    std::vector<uint8_t>& bytes = g_pending.bytes;
    static const bool diff = std::getenv("RAC1_MRP_LOD_DIFF") != nullptr;  // (testing: how many words the shadow changed)
    if (diff && g_st.runs[kind] % 100 == 0) {
        uint32_t nd = 0;
        for (uint32_t o = 0; o + 4 <= bytes.size() && cur + o < real_end; o += 4) {
            uint32_t a;
            std::memcpy(&a, bytes.data() + o, 4);
            nd += a != lwu(cur + o) ? 1u : 0u;
        }
        std::printf("[lod] diff pass %d: shadow %zu bytes, real %u, words differing %u\n", kind, bytes.size(), real_end - cur, nd);
    }
    if (selftest) {  // (testing: the shadow chain against the real one, word by word)
        uint32_t bad = 0;
        if (bytes.size() != real_end - cur) ++bad;
        for (uint32_t o = 0; o + 4 <= bytes.size() && cur + o < real_end; o += 4) {
            uint32_t a;
            std::memcpy(&a, bytes.data() + o, 4);
            const uint32_t b = lwu(cur + o);
            if (a == b) continue;
            if (bad++ < 4 && g_st.runs[kind] < 3) std::fprintf(stderr, "[lod] selftest pass %d +%x: shadow %08x real %08x\n", kind, o, a, b);
        }
        if (bad != 0) ++g_st.selftest_bad[kind];
    }
    ++g_st.runs[kind];
    g_st.bytes[kind] += bytes.size();
    g_st.real_bytes[kind] += real_end - cur;
    mrpw::scene_note_shadow(kind, cur, real_end, cur, std::move(bytes), std::move(g_pending.sky_tex), kind == 3 ? &g_pending.sky_a : nullptr);
    bytes.clear();
    g_pending.sky_tex.clear();
}

// The 3D effects' shadow (see mrp_lod.h).
namespace {
struct EffectsShadow {
    bool pending = false, ready = false;
    std::vector<std::pair<uint32_t, uint8_t>> drawn;  // the mobys' +0x31 as the moby pass's shadow left them (this frame)
    bool drawn_valid = false;
    uint32_t cur = 0, end = 0;
    std::vector<uint8_t> bytes;
    uint64_t runs = 0, overflow = 0, bytes_total = 0, real_total = 0, same = 0;  // (same: the shadow's chain the real block's byte for byte)
    // the frames the shadow was not run because nothing in it could differ from the real block (mrp_lod_effects_shadow); with
    // RAC1_MRP_EFFECTS_SKIP=2 (testing) run anyway: `predicted` of them, `missed` those whose chain then differed from the real block's
    uint64_t skipped = 0, predicted = 0, missed = 0, need_glows = 0, need_particles = 0, need_lists = 0;
    bool predicted_now = false;
    double ms = 0;
    bool running = false;  // the shadow's block runs
    float widen = 1.0f;
} g_ps;
}  // namespace

// orig: (host) see mrp_lod.h
bool mrp_lod_in_effects_shadow() { return g_ps.running; }
// orig: (host) see mrp_lod.h
float mrp_lod_effects_widen() { return g_ps.running ? g_ps.widen : 1.0f; }

// orig: (host) see mrp_lod.h
void mrp_lod_effects_shadow(void (*pass)(), uint32_t cur) {
    RAC1_PROF_ZONE(kGameLodOther);
    g_ps.pending = false;
    if (g_mrp_lod_frame_drawn) g_ps.ready = false;  // (a drawn frame: its own effects only)
    const bool drawn_valid = g_ps.drawn_valid;
    g_ps.drawn_valid = false;  // (this frame's moby shadow's: used once)
    if (!g_world_mrp || g_mrp_lod_widen_off || !g_mrp_lod_frame_drawn) return;
    static const bool off = std::getenv("RAC1_MRP_WIDEN_EFFECTS") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_EFFECTS")) == 0;  // (testing)
    const float widen = fill_widen();
    if (off || widen <= 1.0f || vu0::running()) return;
    static const bool no_drawn = std::getenv("RAC1_MRP_WIDEN_GLOWS") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_GLOWS")) == 0;  // (testing)
    static const bool selftest = std::getenv("RAC1_MRP_EFFECTS_SELFTEST") != nullptr;  // (testing: the shadow with nothing changed)
    // what the shadow changes is the particles' frustum tests (patch_cull, mrp_fx_particles_patch) and the mobys' +0x31 (the
    // lights' glows 0x2df3d8 read it); the rest of the block's 3D (the quad drawer's glows / canopy / decals, the sparkles, the prim lists)
    // is clipped by the game at its guard band and reaches the side bands from the real block, drawn by the MRP in the world projection.
    // When no moby's +0x31 differs and no particle could pass the widened tests that fails the game's own, the shadow would make the real
    // block's chain: not run (RAC1_MRP_EFFECTS_SKIP=0 always run, 2 run and check the prediction)
    static const int skip_mode = std::getenv("RAC1_MRP_EFFECTS_SKIP") != nullptr ? std::atoi(std::getenv("RAC1_MRP_EFFECTS_SKIP")) : 1;
    bool glows = false;
    if (drawn_valid && !no_drawn && !selftest) {
        // the lights' glows 0x2df3d8 queued in the block's draw lists (0x16d880 / 0x16da80): their (group +0x21, class +0xa6); a moby whose
        // +0x31 the widened moby pass changed matters only in one of them (light_group_glows_draw reads it for that group and class alone)
        std::vector<std::pair<uint32_t, uint32_t>> lights;
        const std::array<std::array<uint32_t, 3>, 2> lists{{{OV(0x0015f424u), OV(0x0016d880u), OV(0x0016d980u)}, {OV(0x0015f428u), OV(0x0016da80u), OV(0x0016db80u)}}};
        for (const auto& L : lists)
            for (int32_t i = 0; i < lw(L[0]) && i < 0x40; ++i)
                if (lwu(L[1] + 4 * static_cast<uint32_t>(i)) == OVF(0x002df3d8u)) {
                    const uint32_t m = lwu(L[2] + 4 * static_cast<uint32_t>(i));
                    lights.emplace_back(lbu(m + 0x21), lhu(m + 0xa6));
                }
        for (const auto& [m, d] : g_ps.drawn) {
            if (lbu(m + 0x31) == d) continue;
            const std::pair<uint32_t, uint32_t> key(lbu(m + 0x21), lhu(m + 0xa6));
            if (std::find(lights.begin(), lights.end(), key) != lights.end()) {
                glows = true;
                break;
            }
        }
    }
    const bool particles = !selftest && mrp_fx_particles_widen_needed(widen);
    // the draw lists' callbacks cull against the clip matrix (mrp_lod_draw_list_widen) or the sphere test: one queued may draw
    // more in the widened view (Novalis's water tiles L1:0x2b91c8, the levels' water grids); nothing tells which without running them
    static const bool no_lists = std::getenv("RAC1_MRP_WIDEN_LISTS") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_LISTS")) == 0;  // (testing)
    const bool lists = !selftest && !no_lists && (lw(OV(0x0015f424u)) > 0 || lw(OV(0x0015f428u)) > 0);
    g_ps.need_glows += glows ? 1u : 0u;
    g_ps.need_particles += particles ? 1u : 0u;
    g_ps.need_lists += lists ? 1u : 0u;
    g_ps.predicted_now = !glows && !particles && !lists && !selftest;
    if (g_ps.predicted_now && skip_mode == 1) {
        ++g_ps.skipped;
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const Vu0Save vu0_saved(true);  // (the block's VU0 macro / micro work)
    const auto hw_saved = eer::hwregs();
    ee_journal_begin();
    if (!selftest) patch_cull(widen, fill_widen_y());  // (the particles' sphere test: vf26 / vf27; point_on_screen's terms)
    if (drawn_valid && !no_drawn && !selftest)
        for (const auto& [m, d] : g_ps.drawn) sb(m + 0x31, d);  // (the mobys drawn in the widened view: their glows)
    const uint32_t buf_b = lwu(OV(0x00173e08u)), limit = (cur >= buf_b ? buf_b : lwu(OV(0x00173e04u))) + lwu(OV(0x001611ccu));
    g_ps.running = true;
    g_ps.widen = selftest ? 1.0f : widen;
    g_slot = kMrpLodSlotEffects;
    g_slot_w = g_ps.widen;
    g_slot_wy = selftest ? 1.0f : fill_widen_y();
    pass();
    mrp_lod_draw_list_widen(false);
    g_slot = -1;
    patch_clear();
    g_ps.running = false;
    const uint32_t end = lwu(OV(0x001611c0u));
    const bool ok = end > cur && end <= limit;
    if (ok) {
        g_ps.bytes.resize(end - cur);
        std::memcpy(g_ps.bytes.data(), ee_ram_ptr(cur), g_ps.bytes.size());
    }
    g_st.pages += ee_journal_restore();
    vu0_saved.restore();
    eer::hwregs() = hw_saved;
    g_ps.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) {
        if (end != cur && g_ps.overflow++ < 3) std::fprintf(stderr, "[lod] effects: shadow chain %08x..%08x past the buffer's limit %08x\n", cur, end, limit);
        return;
    }
    g_ps.pending = true;
    g_ps.cur = cur;
}

// orig: (host) see mrp_lod.h
void mrp_lod_effects_after(uint32_t cur, uint32_t real_end) {
    if (!g_ps.pending || g_ps.cur != cur) return;
    g_ps.pending = false;
    g_ps.ready = true;
    g_ps.end = real_end;
    ++g_ps.runs;
    g_ps.bytes_total += g_ps.bytes.size();
    const bool same = real_end - cur == g_ps.bytes.size() && std::memcmp(ee_ram_ptr(cur), g_ps.bytes.data(), g_ps.bytes.size()) == 0;
    g_ps.same += same ? 1u : 0u;
    if (g_ps.predicted_now) {  // (RAC1_MRP_EFFECTS_SKIP=2: the prediction checked)
        ++g_ps.predicted;
        if (!same) {
            ++g_ps.missed;
            if (g_ps.missed <= 3) std::fprintf(stderr, "[lod] effects: predicted the real block's chain, the shadow's differs (%zu / %u bytes)\n", g_ps.bytes.size(), real_end - cur);
        }
    }
    g_ps.real_total += real_end - cur;
}

// The draw lists' shadows (see mrp_lod.h mrp_lod_list_shadow)
namespace {
struct ListShadow {
    bool pending = false, ready = false;
    uint32_t cur = 0, end = 0;
    std::vector<uint8_t> bytes;
    uint64_t runs = 0, overflow = 0, same = 0, bytes_total = 0, real_total = 0;
    double ms = 0;
} g_ls[2];
}  // namespace

// orig: (host) see mrp_lod.h
void mrp_lod_list_shadow(int list, void (*block)(), uint32_t cur) {
    RAC1_PROF_ZONE(kGameLodOther);
    if (list < 0 || list > 1) return;
    ListShadow& L = g_ls[list];
    L.pending = false;
    if (g_mrp_lod_frame_drawn) L.ready = false;  // (a drawn frame: its own chain only)
    if (!g_world_mrp || g_mrp_lod_widen_off || !g_mrp_lod_frame_drawn) return;
    static const bool off = std::getenv("RAC1_MRP_WIDEN_LISTS") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_LISTS")) == 0;  // (testing)
    const float widen = fill_widen(), widen_y = fill_widen_y();
    if (off || !(widen > 1.0f || widen_y > 1.0f) || vu0::running()) return;
    const auto t0 = std::chrono::steady_clock::now();
    const Vu0Save vu0_saved(true);  // (the callbacks' VU0 macro / micro work)
    const auto hw_saved = eer::hwregs();
    ee_journal_begin();
    patch_cull(widen, widen_y);  // (the sphere tests: vf26 / vf27; point_on_screen's terms)
    const uint32_t buf_b = lwu(OV(0x00173e08u)), limit = (cur >= buf_b ? buf_b : lwu(OV(0x00173e04u))) + lwu(OV(0x001611ccu));
    g_slot = list;
    g_slot_w = widen;
    g_slot_wy = widen_y;
    block();
    mrp_lod_draw_list_widen(false);
    g_slot = -1;
    patch_clear();
    const uint32_t end = lwu(OV(0x001611c0u));
    const bool ok = end > cur && end <= limit;
    if (ok) {
        L.bytes.resize(end - cur);
        std::memcpy(L.bytes.data(), ee_ram_ptr(cur), L.bytes.size());
    }
    g_st.pages += ee_journal_restore();
    vu0_saved.restore();
    eer::hwregs() = hw_saved;
    L.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) {
        if (end != cur && L.overflow++ < 3) std::fprintf(stderr, "[lod] list %d: shadow chain %08x..%08x past the buffer's limit %08x\n", list, cur, end, limit);
        return;
    }
    L.pending = true;
    L.cur = cur;
}

// orig: (host) see mrp_lod.h
void mrp_lod_list_after(int list, uint32_t cur, uint32_t real_end) {
    if (list < 0 || list > 1) return;
    ListShadow& L = g_ls[list];
    if (!L.pending || L.cur != cur) return;
    L.pending = false;
    L.ready = true;
    L.end = real_end;
    ++L.runs;
    L.bytes_total += L.bytes.size();
    L.real_total += real_end - cur;
    L.same += real_end - cur == L.bytes.size() && std::memcmp(ee_ram_ptr(cur), L.bytes.data(), L.bytes.size()) == 0 ? 1u : 0u;
}

// orig: (host) see mrp_lod.h
void mrp_lod_side_widen(float out[2]) {
    static const bool off = std::getenv("RAC1_MRP_WIDEN_SIDES") != nullptr && std::atoi(std::getenv("RAC1_MRP_WIDEN_SIDES")) == 0;  // (testing: before)
    const bool on = !off && g_world_mrp;  // (measured at the frame's tfrag pass: by the chain's time the tangent may hold the host's widened value)
    out[0] = on ? std::max(g_frame_side[0], 1.0f) : 1.0f;
    out[1] = on ? std::max(g_frame_side[1], 1.0f) : 1.0f;
}

// orig: (host) see mrp_lod.h
void mrp_lod_substitutes(std::vector<MrpLodSubstitute>& out) {
    out.clear();
    for (int k = 0; k < 2; ++k) {
        ListShadow& L = g_ls[k];
        if (!L.ready) continue;
        L.ready = false;
        out.push_back(MrpLodSubstitute{k, L.cur, L.end, std::move(L.bytes)});
        L.bytes.clear();
    }
    if (g_ps.ready) {
        g_ps.ready = false;
        out.push_back(MrpLodSubstitute{kMrpLodSlotEffects, g_ps.cur, g_ps.end, std::move(g_ps.bytes)});
        g_ps.bytes.clear();
    }
}

namespace {
// The moby draw loop 0x2559c0 (moby_draw.cpp): per moby of the list (*(0x15ffd8) on, 0x100 bytes each, ended by a moby whose
// state byte +0x20 is 0xff) the draw distance +0x32 (a halfword, the minimum with the loop's *(0x15fff0), << 10 against the view
// depth x 1024) and the LOD distance +0x72 (a byte << 10 against the depth clamped to 0x30000: past it the far model); the
// sides' test reads the tangent 0x16caf0 / 0x16cc20 as the world passes.
// The moby pass's packets go to the frame's packet buffer up to its limit (0x1611c8 = the skinning's work area 0x15f5fc
// less 0x10000, set by the pass's start 0x24fe50); a moby past it is not drawn (0x2566e0) and the pass ends. The widened / LOD view
// draws two to five times the 4:3 game's mobys and filled it (Novalis at LOD 100: the last mobys of the list, those near the camera
// included, missing). The shadow run's packets go to a host region instead (EE 0x03c00000.., above the game's 32 MB, free since the
// legacy GS ring was removed: runtime/level/host_layout.h): the cursor 0x1611c0 moved there, the work area (its 0x2000 bytes copied)
// to 0x03ee0000, so the limit is 0x03ed0000 (2.8 MB; at LOD High in a 32:9 view Rilgar, Gemlik and Kalebo's landing reach it).
// The skinning records go to a host region too: the draw loop writes one per moby drawn to the ring 0x15f5f8 (the pass's start sets
// the ring's cursor 0x15ffd4 to it), which the game sizes for its own frame (0x15f578 = 0x1e000 bytes at the top of the packet
// buffer, ending where the buffer ends: sub_002a0858) and never bounds. With the shadow's two to five times the mobys it ran past the
// buffer's end into what lies above it (Kalebo's landing: the ship scene's streamed animation frames at 0x40c000.., a moby's
// keyframe header then read back as garbage -> a pose's scratchpad address 0x700c130c, the pointer fault). The shadow's ring is
// 0x03f40000..0x04000000 (0xc0000 bytes, above the host's texture tables; the landing used 0x2c140). Inside the journal: put back
// after the run. RAC1_MRP_LOD_ROOM=0: off (testing).
constexpr uint32_t kShadowPackets = 0x03c00000u, kShadowWork = 0x03ee0000u, kShadowRing = 0x03f40000u;
void shadow_packet_room() {
    static const bool off = [] {
        const char* v = std::getenv("RAC1_MRP_LOD_ROOM");
        return v != nullptr && std::atoi(v) == 0;
    }();
    if (off) return;
    const uint32_t work = lwu(OV(0x0015f5fcu));
    std::memcpy(ee_ram_ptr_write(kShadowWork, 0x2000), ee_ram_ptr_read(work), 0x2000);
    sw(OV(0x0015f5fcu), kShadowWork);
    sw(OV(0x0015f5f8u), kShadowRing);
    sw(OV(0x001611c0u), kShadowPackets);
}

void patch_moby(float s) {
    auto dist = [s](int32_t d, int32_t cap) {
        if (s == 0.0f) return cap;
        const double v = static_cast<double>(d) * static_cast<double>(s);
        return v > static_cast<double>(cap) ? cap : static_cast<int32_t>(v);
    };
    sw(OV(0x0015fff0u), static_cast<uint32_t>(dist(lw(OV(0x0015fff0u)), 0x7fff)));
    uint32_t m = lwu(OV(0x0015ffd8u));
    for (int guard = 0; guard < 4096 && m != 0 && m < 0x02000000u && lbu(m + 0x20) != 0xff; ++guard, m += 0x100) {
        const int32_t d = lh(m + 0x32);
        // a moby whose class is not loaded (+0x24 = 0: no model; e.g. class 805 on level 9) keeps the game's distances: drawn,
        // the loop would walk a null class's packets (lbu(0 + 4) = 0 -> the parts loop never meets its end) past the scratchpad's end; the
        // game never has it within its draw distance
        if (lwu(m + 0x24) == 0) continue;
        if (d > 0) sh(m + 0x32, static_cast<uint32_t>(dist(d, 0x7fff)));
        sb(m + 0x72, static_cast<uint32_t>(dist(lbu(m + 0x72), 0xff)));
    }
}
// ((2026-10-29) the Wii U's real moby pass, widen_real) patch_moby's writes with their own undo log, not through the EE journal: the
// journal saved every page the patch touched (a 4 KB copy each), then diffed them byte by byte into runs (two vectors allocated per run)
// - ~10% of the game thread on Novalis for a few hundred halfwords and bytes. Each field written is logged with its value before and
// after; patch_moby_undo puts a field back where it still holds the patch's value (the game's own writes since kept), as the journal's
// runs were put back.
struct MobyFieldUndo {
    uint32_t addr;
    uint8_t size;  // 1, 2 or 4
    uint32_t before, after;
};
std::vector<MobyFieldUndo> g_moby_undo;
void patch_moby_logged(float s) {
    g_moby_undo.clear();
    auto dist = [s](int32_t d, int32_t cap) {
        if (s == 0.0f) return cap;
        const double v = static_cast<double>(d) * static_cast<double>(s);
        return v > static_cast<double>(cap) ? cap : static_cast<int32_t>(v);
    };
    auto put = [](uint32_t addr, uint8_t size, uint32_t before, uint32_t after) {
        if (before == after) return;
        if (size == 4) sw(addr, after);
        else if (size == 2) sh(addr, after);
        else sb(addr, after);
        g_moby_undo.push_back({addr, size, before, after});
    };
    {
        const uint32_t before = lwu(OV(0x0015fff0u));
        put(OV(0x0015fff0u), 4, before, static_cast<uint32_t>(dist(static_cast<int32_t>(before), 0x7fff)));
    }
    uint32_t m = lwu(OV(0x0015ffd8u));
    for (int guard = 0; guard < 4096 && m != 0 && m < 0x02000000u && lbu(m + 0x20) != 0xff; ++guard, m += 0x100) {
        if (lwu(m + 0x24) == 0) continue;  // (patch_moby's rule: a class not loaded keeps the game's distances)
        const int32_t d = lh(m + 0x32);
        if (d > 0) put(m + 0x32, 2, static_cast<uint32_t>(lhu(m + 0x32)), static_cast<uint32_t>(dist(d, 0x7fff)) & 0xffffu);
        const uint32_t b = lbu(m + 0x72);
        put(m + 0x72, 1, b, static_cast<uint32_t>(dist(static_cast<int32_t>(b), 0xff)) & 0xffu);
    }
}
void patch_moby_undo() {
    for (auto it = g_moby_undo.rbegin(); it != g_moby_undo.rend(); ++it) {
        const uint32_t now = it->size == 4 ? lwu(it->addr) : it->size == 2 ? lhu(it->addr) : lbu(it->addr);
        if (now != it->after) continue;
        if (it->size == 4) sw(it->addr, it->before);
        else if (it->size == 2) sh(it->addr, it->before);
        else sb(it->addr, it->before);
    }
    g_moby_undo.clear();
}
struct MobyStats {
    uint64_t runs = 0, widened = 0, mobys_real = 0, mobys_shadow = 0, faded = 0;
    double ms = 0;
} g_mst;
}  // namespace

// orig: (host) see mrp_lod.h
void mrp_lod_moby_shadow(void (*pass)()) {
    RAC1_PROF_ZONE(kGameLodMobys);
    static const bool verify = std::getenv("RAC1_MRP_LOD_VERIFY") != nullptr;
    if (!mrp_moby_ready() || vu0::running()) return;
    const float s = mrp_lod_scale(g_mrp_lod);
    const float widen = fill_widen();
    if (s == 1.0f && widen <= 1.0f) return;
    if (!g_mrp_lod_frame_drawn) return;  // (a game frame not drawn)
    const auto t0 = std::chrono::steady_clock::now();
    if (verify) {
        g_verify_ram.assign(ee_get_ram_base(), ee_get_ram_base() + 0x04000000u);
        g_verify_spr.assign(ee_get_scratchpad_base(), ee_get_scratchpad_base() + 0x4000u);
    }
    // the host state the pass changes besides EE memory: the whole VU0 (the pass uploads its micro programs, the drop shadows run
    // them), the DMA registers, the pass's host notes and timers; the drop shadows' observer off (the MRP's are the real pass's)
    const Vu0Save vu0_saved(true);
    const auto hw_saved = eer::hwregs();
    const uint32_t up_first = g_moby_upload_chain_first, up_last = g_moby_upload_chain_last;
    const double t_skin = g_prof_moby_skin_ms, t_fill = g_prof_moby_fill_ms, t_tex = g_prof_moby_tex_ms, t_slots = g_prof_moby_slots_ms;
    const uint64_t n_bad = g_moby_mrp_skip_next_bad, n_ok = g_moby_mrp_skip_next_ok;
    const DropShadowObserver shadows_obs = g_drop_shadow_observer;
    g_drop_shadow_observer = DropShadowObserver{};
    ee_journal_begin();
    shadow_packet_room();
    patch_moby(s);
    if (widen > 1.0f) {
        patch_widen(widen, 0, fill_widen_y());
        ++g_mst.widened;
    }
    mrp_scene_shadow_begin();
    g_moby_pass_shadow_run = true;  // (its drop shadows not made)
    pass();
    patch_clear();
    g_moby_pass_shadow_run = false;
    mrp_scene_shadow_end();  // (the chains decoded while the shadow's memory is there)
    {  // the mobys' +0x31 (drawn this frame) the shadow left: the effects' shadow gives them to the glows
        g_ps.drawn.clear();
        uint32_t m = lwu(OV(0x0015ffd8u));
        for (int guard = 0; guard < 4096 && m != 0 && m < 0x02000000u && lbu(m + 0x20) != 0xff; ++guard, m += 0x100)
            g_ps.drawn.emplace_back(m, static_cast<uint8_t>(lbu(m + 0x31)));
        g_ps.drawn_valid = true;
    }
    g_st.pages += ee_journal_restore();
    vu0_saved.restore();
    eer::hwregs() = hw_saved;
    g_moby_upload_chain_first = up_first;
    g_moby_upload_chain_last = up_last;
    g_prof_moby_skin_ms = t_skin;
    g_prof_moby_fill_ms = t_fill;
    g_prof_moby_tex_ms = t_tex;
    g_prof_moby_slots_ms = t_slots;
    g_moby_mrp_skip_next_bad = n_bad;
    g_moby_mrp_skip_next_ok = n_ok;
    g_drop_shadow_observer = shadows_obs;
    if (verify) {
        ++g_st.verify_runs;
        const bool same = std::memcmp(g_verify_ram.data(), ee_get_ram_base(), g_verify_ram.size()) == 0 &&
                          std::memcmp(g_verify_spr.data(), ee_get_scratchpad_base(), g_verify_spr.size()) == 0;
        if (!same) {
            ++g_st.verify_bad;
            if (g_st.verify_bad <= 5) std::fprintf(stderr, "[lod] VERIFY: the memory differs after the moby pass's shadow run\n");
        }
    }
    ++g_mst.runs;
    g_mst.mobys_shadow += mrp_scene_draw().mobys.size();
    for (const MrpMoby& m : mrp_scene_draw().mobys) g_mst.faded += m.tint[3] < m.alpha_base ? 1u : 0u;
    g_mst.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

namespace {
// the A (0x166d00) the last real sky pass widened by mrp_lod_sky_widen_real projected with (mrp_lod_sky_real_a)
std::array<float, 16> g_real_sky_a{};
bool g_real_sky_a_on = false;
// orig: (host) the real pass `pass` with the frustum widened by `w` (patch_widen: the rows A kept) or, `projection`, the whole projection
// (widen_projection, A widened too - its A copied to `a_out` after the pass); the patch's own writes journaled and put back after the pass
// where it left them, the VU0 registers the setups leave kept as they were
void widen_real(void (*pass)(), float w, float wy, float lod_s, bool projection, float* a_out) {
    const Vu0Save vu0_saved(false);
    std::vector<EeByteRun> runs;
    const bool lod = !projection && lod_s != 1.0f;
    if (lod) patch_moby_logged(lod_s);  // (its own undo log: patch_moby_undo below)
    const bool widen = w > 1.0f || wy > 1.0f;
    if (widen) {
        ee_journal_begin();
        if (projection) widen_projection(w, wy);
        else patch_widen(w, 0, wy);
        ee_journal_commit(runs);
    }
    vu0_saved.restore();
    pass();  // (patch_widen's g_patch set: a setup the pass calls gets the game's own tangent, then the patch again - MrpLodSetupScope)
    patch_clear();
    if (a_out != nullptr)
        for (uint32_t k = 0; k < 16; ++k) a_out[k] = lwc1(OV(0x00166d00u) + 4 * k);
    // the game's bytes put back where they still hold the patch's (the game's own writes since kept), as render_view_projection_undo
    for (auto it = runs.rbegin(); it != runs.rend(); ++it) {
        uint8_t* p = static_cast<uint8_t*>(ee_ram_ptr(it->addr));
        if (p == nullptr || std::memcmp(p, it->after.data(), it->after.size()) != 0) continue;
        std::memcpy(p, it->before.data(), it->before.size());
    }
    if (lod) patch_moby_undo();
}
}  // namespace

// orig: (host) see mrp_lod.h
void mrp_lod_moby_widen_real(void (*pass)(), float w) {
    const float wy = fill_widen_y();
    const float s = mrp_lod_scale(g_mrp_lod);
    if (!(w > 1.0f || wy > 1.0f || s != 1.0f) || g_patch.type != kPatchNone || ee_journal_active()) {
        pass();
        return;
    }
    widen_real(pass, w > 1.0f ? w * wy : wy, wy, s, false, nullptr);
}

// orig: (host) see mrp_lod.h
void mrp_lod_sky_widen_real(void (*pass)(), float w) {
    const float wy = fill_widen_y();
    if (!(w > 1.0f || wy > 1.0f) || g_patch.type != kPatchNone || ee_journal_active()) {
        g_real_sky_a_on = false;
        pass();
        return;
    }
    widen_real(pass, w > 1.0f ? w * wy : wy, wy, 1.0f, true, g_real_sky_a.data());
    g_real_sky_a_on = true;
}

// orig: (host) see mrp_lod.h
const float* mrp_lod_sky_real_a() { return g_real_sky_a_on ? g_real_sky_a.data() : nullptr; }

// orig: (host)
void mrp_lod_report() {
    mrp_scene_lod_drop_report();
    if (g_mst.runs != 0)
        std::printf("[lod] mobys: %llu shadow runs (%llu widened), %.3f ms each, %.1f mobys drawn a frame, %llu drawn with an alpha below their +0x23 "
                    "(the distance fade 0x255e48, or the moby's own)\n",
                    static_cast<unsigned long long>(g_mst.runs), static_cast<unsigned long long>(g_mst.widened), g_mst.ms / static_cast<double>(g_mst.runs),
                    static_cast<double>(g_mst.mobys_shadow) / static_cast<double>(g_mst.runs), static_cast<unsigned long long>(g_mst.faded));
    if (g_ps.runs + g_ps.skipped != 0) {
        const double n = static_cast<double>(std::max<uint64_t>(1, g_ps.runs));
        std::printf("[lod] effects: %llu shadow runs (%llu with the real block's chain byte for byte), %.3f ms each, chain %.0f bytes (the real pass's %.0f); "
                    "%llu not run (nothing to widen); needed for glows (+0x31) %llu, particles %llu, draw lists %llu; predicted %llu, missed %llu\n",
                    static_cast<unsigned long long>(g_ps.runs), static_cast<unsigned long long>(g_ps.same), g_ps.ms / n, static_cast<double>(g_ps.bytes_total) / n,
                    static_cast<double>(g_ps.real_total) / n, static_cast<unsigned long long>(g_ps.skipped), static_cast<unsigned long long>(g_ps.need_glows),
                    static_cast<unsigned long long>(g_ps.need_particles), static_cast<unsigned long long>(g_ps.need_lists), static_cast<unsigned long long>(g_ps.predicted),
                    static_cast<unsigned long long>(g_ps.missed));
    }
    for (int k = 0; k < 2; ++k) {
        const ListShadow& L = g_ls[k];
        if (L.runs + L.overflow == 0) continue;
        const double n = static_cast<double>(std::max<uint64_t>(1, L.runs));
        std::printf("[lod] draw list %s: %llu shadow runs (%llu with the real block's chain byte for byte, %llu overflowed), %.3f ms each, chain %.0f bytes (the real block's %.0f)\n",
                    k == 0 ? "0x16dc80" : "0x16de80", static_cast<unsigned long long>(L.runs), static_cast<unsigned long long>(L.same), static_cast<unsigned long long>(L.overflow), L.ms / n,
                    static_cast<double>(L.bytes_total) / n, static_cast<double>(L.real_total) / n);
    }
    if (g_collide_vu_put_back != 0)
        std::printf("[lod] the collision's contact registers vf25..vf29 changed by %llu shadow runs: put back\n", static_cast<unsigned long long>(g_collide_vu_put_back));
    if (g_st.runs[0] + g_st.runs[1] + g_st.runs[2] + g_st.runs[3] + g_st.skipped_vu0 == 0) return;
    static const char* names[4] = {"tfrag", "tie", "shrub", "sky"};
    for (int k = 0; k < 4; ++k) {
        if (g_st.runs[k] + g_st.overflow[k] == 0) continue;
        const double n = static_cast<double>(std::max<uint64_t>(1, g_st.runs[k]));
        std::printf("[lod] %s: %llu shadow runs (%llu overflowed, %llu widened), %.3f ms each, chain %.0f bytes (the real pass's %.0f)\n", names[k],
                    static_cast<unsigned long long>(g_st.runs[k]), static_cast<unsigned long long>(g_st.overflow[k]), static_cast<unsigned long long>(g_st.widened[k]),
                    g_st.ms[k] / n, static_cast<double>(g_st.bytes[k]) / n, static_cast<double>(g_st.real_bytes[k]) / n);
    }
    std::printf("[lod] selftest: chains differing %llu / %llu / %llu\n", static_cast<unsigned long long>(g_st.selftest_bad[0]), static_cast<unsigned long long>(g_st.selftest_bad[1]), static_cast<unsigned long long>(g_st.selftest_bad[2]));
    std::printf("[lod] pages journaled %llu, skipped (VU0 busy) %llu (game frames not drawn %llu), verified %llu (memory differing %llu)\n", static_cast<unsigned long long>(g_st.pages),
                static_cast<unsigned long long>(g_st.skipped_vu0), static_cast<unsigned long long>(g_st.not_drawn), static_cast<unsigned long long>(g_st.verify_runs), static_cast<unsigned long long>(g_st.verify_bad));
}

}  // namespace rac1
