// The EE side of the world's render passes: see world_cull.cpp
#ifndef RAC1_GAME_OVL_WORLD_CULL_H
#define RAC1_GAME_OVL_WORLD_CULL_H

#include <cstdint>
#include <vector>

namespace rac1 {

// orig: (host) what the passes kept visible, as they decided it (the original leaves it only in the chain it writes).
// The indices are those of the lighting lists: the entries of the level's tables.
struct WorldVisible {
    struct Tfrag {
        uint16_t index;  // the 0x40-byte entry (table *(0x160f4c))
        uint8_t lod;     // the chain's LOD code: 14 / 16 / 20 near, 8 / 10 mid, 6 far or single, 2 drawn with clipping
    };
    std::vector<Tfrag> tfrag;
    std::vector<uint16_t> tie;    // the 0x20-byte bounds of the instances (table *(0x160fc0))
    std::vector<uint16_t> shrub;  // the 0x20-byte bounds of the instances (table *(0x160494))
};
const WorldVisible& world_visible();
void world_visible_restore(const WorldVisible& v);  // orig: (host)

// The passes of render_frame (flags 2 / 4 / 8): culling, chain building, the lists for the lighting and the end routines.
void world_tfrag_pass();   // orig: 0x00294240
void world_tie_pass();     // orig: 0x00295c80
void world_shrub_pass();   // orig: 0x00286e60
// The end routines that follow the lighting calls (they patch the chain's VIF words from the texture tables).
void world_tfrag_end();    // orig: 0x002941a0
void world_tie_end();      // orig: 0x00295b38
void world_shrub_end();    // orig: 0x00286d58
// render_frame's first call after the view setup: the moby animation lerp slots.
void moby_anim_lerp_slots_validate();  // orig: 0x0024f6e0

// orig: (host) overlay_entry.cpp: a world pass run for render_frame (its chain kept for the faithful / MRP world and noted for the
// MRP, else taken back); `kind` 0 tfrag, 1 tie, 2 shrub, -1 the sky. Also the boot module's title draw.
void world_pass(void (*pass)(), int kind);
// orig: (host) overlay_entry.cpp: whether the sky pass runs (its chain drawn: the faithful / MRP world, the Sky toggle)
bool sky_wanted();

// The passes' parts.
void world_tfrag_cull();   // orig: 0x00294368
void world_tfrag_cull_tr();  // orig: (host) its transliteration (world_cull.cpp), the hand-written cull's reference
void world_tfrag_visible_textures();  // orig: 0x00294f90
uint32_t world_tfrag_texture_packets(uint32_t a0);  // orig: 0x00295100
void world_tfrag_packets();  // orig: 0x00294058
void world_tie_cull();     // orig: 0x00295fa0
void world_tie_cull_tr();  // orig: (host) its transliteration
uint32_t world_tie_texture_packets(uint32_t a0);  // orig: 0x00297478
void world_tie_packets();  // orig: 0x002959f8
void world_shrub_cull();   // orig: 0x00286f10
void world_shrub_cull_tr();  // orig: (host) its transliteration
uint32_t world_shrub_texture_packets(uint32_t a0);  // orig: 0x00288658
void world_shrub_packets();  // orig: 0x00286c18
void spr_to_ram(uint32_t dst, uint32_t spr, uint32_t qwc);  // orig: 0x001ff180
void packet_vif_unpack(uint32_t addr, uint32_t src, uint32_t qwc);  // orig: 0x002a0ab0
// orig: (host) the notes of what the culls keep visible (world_visible())
void note_tfrag(uint32_t entry, uint32_t lod);
void note_tie(uint32_t index);
void note_shrub(uint32_t index);

}  // namespace rac1

#endif
