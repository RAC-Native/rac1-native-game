// The MRP world's static data (ADR 0010): what the MRP world keeps on the GPU for the whole level instead of sending it
// every frame. Host code: none of it is the game's.
//
// Stage 1 - the lit colours. The tie / shrub instances' vertex colours are what the world's relighting routines wrote into the
// level's data (tie instance data +0x40, 64 RGBA8 each; shrub *(0x1604a0) + 0x60 x index, 24 RGBA8
// each); the passes' chains UNPACK them from there every frame (tie: STMOD 1 with the row 0x4b000000, VU1 0x346 / 0x386; shrub:
// V4_8 into the instance block), and they change only when an instance is relit (the lighting lists 0x1c7300 / 0x1bcfb0: a
// dirty flag set or a point light reaching it). The MRP keeps a copy of all of them resident: a
// mirror compared with EE memory after each update (a memcmp per instance, ~0.4 MB on Veldin), the instances that changed
// uploaded alone. The GPU path's instances then name their slot in that table instead of carrying their colours.
#ifndef RAC1_GAME_RENDER_MRP_WORLD_STATIC_H
#define RAC1_GAME_RENDER_MRP_WORLD_STATIC_H

#include <formats/world_mrp.h>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace rac1::mrpw {

// Where the level's lit colours are (EE addresses read on the game's thread: the tables' pointers and counts).
struct ColourSource {
    uint32_t tie_data = 0, tie_count = 0;      // *(0x160fc8): 0x1c0-byte instance data, *(0x160fcc) of them (colours at +0x40)
    uint32_t shrub_cols = 0, shrub_count = 0;  // *(0x1604a0): 0x60 bytes of colours per instance, *(0x160490) of them
    bool operator==(const ColourSource&) const = default;
};
// orig: (host) the colour tables' addresses and counts (game thread: overlay addresses)
ColourSource colour_source();

constexpr uint32_t kTieColours = 64, kShrubColours = 24;
constexpr uint32_t kTieDataStride = 0x1c0, kTieColourOffset = 0x40, kShrubColourStride = 0x60;

// The resident copy: one u32 (RGBA8 as the level's data holds it) per colour, the ties' first, then the shrubs'.
struct ResidentColours {
    ColourSource src;
    std::vector<uint32_t> mirror;
    // the ranges (first u32, count) whose words changed since the last upload, and their values (the upload's data, taken when
    // the change was seen: the draw of that frame sends them)
    std::vector<std::pair<uint32_t, uint32_t>> dirty;
    std::vector<uint32_t> dirty_words;
    bool all_dirty = true;  // (a new level: the whole table sent)
    uint64_t updates = 0, instances_changed = 0, words_uploaded = 0;
    uint32_t shrub_first() const { return src.tie_count * kTieColours; }
    // the slot (index of the first u32) of the tie instance whose colours start at EE address `addr` (-1: not one)
    int64_t tie_slot(uint32_t addr) const;
    // the slot of the shrub instance whose colours start at `addr`
    int64_t shrub_slot(uint32_t addr) const;
};
// orig: (host) after the game's update (its state frozen; any thread): the mirror compared with EE memory, the changed instances
// copied and queued for upload (a new source: everything)
void resident_colours_update(ResidentColours& rc, const ColourSource& src);
// orig: (host) the Wii U port (2026-10-28): an instance a skipped pass's relighting relit (overlay_entry.cpp world_relight; game
// thread), noted for resident_colours_update_touched - the Wii U compares the whole table only every RAC1_WIIU_COLOUR_EVERY frames
// (`shrub`: a shrub's index, else a tie's). Past 4096 notes between two updates the next one compares the whole table.
void resident_colours_touch(bool shrub, uint32_t index);
// orig: (host) the instances noted since the last call compared with EE memory alone (as resident_colours_update, the same source;
// a new source or too many notes: the whole table) and their changes queued; the notes cleared (`drop`: only cleared - the whole table
// was just compared)
void resident_colours_update_touched(ResidentColours& rc, const ColourSource& src, bool drop);
// orig: (host) the Wii U port (2026-10-28): what the host's culling drew in the last prepared frame, for that relighting - the passes
// relight only their visible entries (their lists come from the culling), so world_relight relights only these, a frame late. Kind 0
// tfrag (the EE table's index), 1 tie, 2 shrub. Cleared and marked by the Wii U's prepare; a kind never marked since the last clear
// reads as all visible (-1), as on the PC; `index` ~0u marks the kind's culling as run, nothing drawn by it.
void relight_visible_clear();
void relight_visible_mark(int kind, uint32_t index);
// -1: no marks for the kind, 0 not drawn, 1 drawn
int relight_visible(int kind, uint32_t index);

// ---- Stage 2: the ties and shrubs at LOD > 0 / widened, without the passes' shadow runs ---------------------------------------
//
// With the LOD slider off 0 or the view wider than the game's 4:3 frustum, the host used to run the tie (0x295c80) and shrub
// (0x286e60) passes a second time on a journal of EE memory with their distances scaled / their frustum widened, and the
// walker decoded that shadow chain. Here the same per-instance decisions are made by host code from the level's own tables
// (the passes' work per instance is a handful of float operations8 has the listing's formulas):
// the frustum's sphere tests with the widened tangent, the draw distance (scaled), the zones (ties), the LOD and its morph
// factors (ties), the fade and the far variant's crossfade (shrubs), the fog (ties), the instance's rows (its matrix times the
// pass's projection A, the classes' wind sway from the frame counter). The real passes keep running as the game's (their
// chain decoded as usual but its tie / shrub draws not drawn); the walker draws these instances instead, at the end of the
// pass's range, with the GPU path's packet templates (every packet of every LOD is a template already).

// What the passes read at their start (the view, the projection's tangents and constants, the fog, the zones, the wind),
// taken by static_note_pass on the game's thread before each real tie / shrub pass.
struct StaticSnapshot {
    bool valid = false;
    bool on[2] = {};                            // (the pass's instances made here: 0 tie, 1 shrub)
    float scale = 1.0f, widen = 1.0f;           // the slider's distance scale (0 = unlimited), the frustum's widening
    float widen_y = 1.0f;                       // the frustum's vertical widening (the field of view's scale)
    std::array<std::array<float, 4>, 3> rot{};  // 0x166c80: the camera's rotation (vf25..vf27)
    std::array<std::array<float, 4>, 4> a{};    // 0x166d00: the projection A (vf28..vf31)
    std::array<float, 4> cam{};                 // 0x166dc0 (vf24)
    std::array<float, 4> fog{}, near{}, tan{}, rad{};  // 0x16cc60 (vf18), 0x16cae0 (vf19), 0x16caf0 (vf20), 0x16cc20 (vf22)
    std::array<float, 4> c23{}, box{};          // 0x16cc30 (vf23), 0x160f90 (the tie's box test: vf21)
    std::array<std::array<float, 4>, 4> clip{}; // 0x166d80 (the tie's box test: vf12..vf15)
    uint32_t frame = 0;                         // 0x15f3f8 (the wind's clock)
    std::array<uint8_t, 0x80> zone{};           // 0x173d00 (the zones' mask, ties)
    std::array<int8_t, 0x100> sine{};           // 0x1c3ef0 (the wind's table)
    std::array<float, 8> wind{};                // 0x1c3ff0 (vf8, vf9 of the wind)
    uint32_t tie_max = 0, shrub_max = 0;        // *(0x160fe0), *(0x1604a4): the draw distances' maximum (float bits)
    uint32_t shrub_data = 0;                    // *(0x16049c): the shrubs' 0x40-byte instance data
    // the shrub far variants' quads (world_render.md 18.4, 0x287c04..0x288010): the matrix 0x166cc0 (vf28..vf31), vf22 0x16cc50,
    // vf23 0x16cbe0 (vf21 is `fog`), the stream's templates (the header 0x1c2c10, the fading ones' header 0x1c2be0, the class /
    // instance transfers 0x160b80..0x160bb0) and the unit corners with their ST 0x1c2cb0 (8 qwords)
    std::array<std::array<float, 4>, 4> vm{};
    std::array<float, 4> vf22{}, vf23{};
    std::array<std::array<uint32_t, 4>, 4> vhdr{}, vtpl{};
    std::array<std::array<uint32_t, 4>, 3> vhdr2{};
    std::array<std::array<float, 4>, 8> vcorner{};
};
// orig: (host) before the real tie (kind 1) / shrub (kind 2) pass, every game frame (mrp_lod_world_shadow): the snapshot taken;
// `handle`: the pass's instances made by the host this frame (its shadow run not needed); `scale` / `widen` / `widen_y` as the shadow's
void static_note_pass(int kind, bool handle, float scale, float widen, float widen_y = 1.0f);

// The level's tie / shrub classes as the passes find them in EE memory (the class records listed at 0x1c5800 / 0x1bc030) matched
// to the MRP world asset's classes, each packet with its texture prims' A+D qwords as VU1 memory receives them (taken from EE
// memory: the level load rewrote TEX0 / CLAMP).
struct StaticPacket {
    const TiePacket* tie = nullptr;
    const ShrubPacket* shrub = nullptr;
    uint32_t tex_first = 0, ntex = 0;  // StaticLevel::tex
};
struct StaticTieClass {
    uint32_t rec = 0;
    std::array<std::vector<StaticPacket>, 3> lod;
};
struct StaticShrubClass {
    uint32_t rec = 0;
    std::vector<StaticPacket> packets;
};
struct StaticLevel {
    bool ok = false;
    std::vector<StaticTieClass> ties;
    std::vector<StaticShrubClass> shrubs;
    std::vector<std::array<uint32_t, 4>> tex;
    uint32_t tie_unmatched = 0, shrub_unmatched = 0;
};
// orig: (host) at the level's load (game thread: EE memory, overlay addresses): `w` the level's MRP world asset
bool static_level_build(StaticLevel& L, const World& w);

// A frame's instance (as the walker's SceneInst: the rows, the tie morph block, F, the shrub alpha; `slot` the resident colours')
// and the draws in the passes' order.
struct StaticInst {
    float rows[4][4] = {};
    float morph[4] = {};
    uint32_t slot = 0, fog = 0, alpha = 0x80;
    uint16_t mask = 0;  // (the casters' frame) the shadow views it is drawn into: bits 0..2 the cascades, 3 + i the point light slot i
};
struct StaticDraw {
    const StaticPacket* pk = nullptr;
    uint32_t inst = 0;
};
struct StaticFrame {
    bool on[2] = {};
    std::vector<StaticInst> insts;
    std::vector<StaticDraw> draws[2];  // ties, shrubs
    uint32_t tested[2] = {}, visible[2] = {};
    // the shrub far variants' quads as the shrub pass's GIF stream (0x287c04..0x288010): the DIRECT transfers' data alone (their DMA
    // tags / VIF codes left out), in the pass's order; empty when the host does not make the shrubs
    std::vector<std::array<uint32_t, 4>> variant_gif;
    uint32_t variants = 0;  // (the quads in it)
};
// orig: (host) after the update (its state frozen; any thread): the frame's instances from the snapshot (`check`: made at the
// game's own distances and frustum whatever the switches, for RAC1_MRPW_STATIC_CHECK)
void static_frame_build(const StaticLevel& L, const ResidentColours& rc, StaticFrame& f, bool check);

// ---- the modern lighting's casters: the ties / shrubs whose shadow can fall into the view ----------------------------
//
// The frame's own draws are what the camera sees; a tie or shrub outside the view (behind the camera, above it) can still cast into
// it. These are the instances near the cascades' volumes (mrp_lighting.cpp: per cascade the bounding sphere of its view slice, swept
// `back` units toward the sun): a capsule test per instance sphere, every one whatever the camera's frustum, zones and draw
// distances, at the LOD its camera distance gives. Drawn only into the shadow maps.
struct ShadowCull {
    bool on = false;
    float sun[3] = {0.0f, 0.0f, 1.0f};  // toward the sun
    float splits[3] = {};               // the cascades' far distances
    float back = 300.0f;                // how far toward the sun the casters are looked for
    float pts[8][4] = {};               // the point lights with a shadow, by slot: position, radius (0: none)
    float tan_x = 0.0f, tan_y = 0.0f;   // the drawn view's half-angle tangents (the renderer's projection: widened, its field of view
                                        // scaled); 0: the passes' own (the snapshot's)
    float grow = 1.0f;                  // the slices' spheres scaled (the cached cascades' boxes are larger)
    bool cascades = true;               // the cascades' casters too (false: the point lights' alone)
};
// The volume the casters are looked for in: per cascade the bounding sphere of its view slice (10 % larger), swept toward the sun.
struct CasterVolume {
    bool ok = false;
    float cen[3][3] = {}, rad[3] = {}, sun[3] = {0.0f, 0.0f, 1.0f}, back[3] = {};  // (per cascade: to the cut plane, kBack from the camera)
    float pts[8][4] = {};  // the point lights' spheres by slot (radius 0: none)
    bool casc = true;      // the cascades' capsules looked at (false: the point lights' spheres alone)
    // orig: (host) true when a sphere (x, y, z, r) reaches the volume
    bool reaches(const float sph[4]) const;
    // orig: (host) the shadow views a sphere reaches (bit k: cascade k's capsule; bit 3 + i: point light slot i's sphere)
    uint16_t views(const float sph[4]) const;
};
// orig: (host) the volume of this frame's view (the static snapshot's camera, rotation, tangents)
CasterVolume caster_volume(const ShadowCull& c);
// orig: (host) the casters' frame (the same thread / inputs as static_frame_build); `f.draws` [0] ties, [1] shrubs
void static_shadow_build(const StaticLevel& L, const ResidentColours& rc, StaticFrame& f, const ShadowCull& c);

}  // namespace rac1::mrpw

#endif
