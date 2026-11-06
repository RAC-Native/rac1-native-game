// The moby drop shadows (ADR 0007): the mobys the draw loop deferred for their shadow on the
// ground, the shadows drawn into a buffer and laid over the frame, chained between the first
// mobys' chain and the deferred ones' (0x160000).
#ifndef RAC1_GAME_OVL_MOBY_SHADOWS_H
#define RAC1_GAME_OVL_MOBY_SHADOWS_H

#include <cstdint>

namespace rac1 {

// orig: (host) the observer of the drop shadows' passes (the MRP draws them natively from the data the original
// computes). All calls are optional; the game code does not depend on them.
struct DropShadowObserver {
    // drop_shadows_draw starts: the buffer cleared to the colour word `clear_rgba` (alpha byte = the frame's marking alpha)
    void (*begin)(uint32_t clear_rgba) = nullptr;
    // one tube (a shadow entry's volume) before shadow_quads 0x285d30: `count` point pairs at the scratchpad 0x70000000 (0x20 bytes each:
    // the near and the far point, world space), `clip` the group's flag (the third argument of shadow_quads)
    void (*tube_points)(uint32_t count, uint32_t clip) = nullptr;
    // shadow_layer_send 0x285468 (the layer a0 of shadow_packets, the colour word its faces carry: the alpha byte marks or unmarks);
    // the scratchpad then holds shadow_quads' results
    void (*layer)(uint32_t layer, uint32_t rgba_word) = nullptr;
    // the darkening: ALPHA_1 value and the sprites' RGBA (shadow_buffer_apply 0x2856a0)
    void (*apply)(uint64_t alpha_reg, uint32_t rgba) = nullptr;
    // moby_shadows 0x24fc38: the DMA tags of the shadows are in [first, last] (tag addresses)
    void (*chain)(uint32_t first, uint32_t last) = nullptr;
};
extern DropShadowObserver g_drop_shadow_observer;

// orig: 0x0024fc38
void moby_shadows();
// orig: 0x00285870
void drop_shadows_draw(uint32_t list);
// orig: 0x002854f8
void shadow_buffer_clear();
// orig: 0x002856a0
void shadow_buffer_apply(uint32_t rgba);
// orig: 0x002852e0
void shadow_light_axes(uint32_t dir, float len);
// orig: 0x00285410
uint32_t shadow_entry_draw(uint32_t e);
// orig: 0x00285468
void shadow_layer_send(uint32_t layer, uint32_t which, uint32_t n);
// The VU0 parts (written by hand in moby_shadows_hand.cpp; transliterated in moby_shadows_asm.cpp).
// orig: 0x00285a68
void shadow_points();
// orig: 0x00286848
void shadow_points_clip(uint32_t a0_in, uint32_t a1_in);
// orig: 0x002860a8
void shadow_point_draw(uint32_t a0_in);
// orig: 0x002861f8
void shadow_segment_draw(uint32_t a0_in);
// orig: 0x002868c0
void shadow_packets(uint32_t a0_in, uint32_t a1_in);
// orig: 0x00285d30
void shadow_quads(uint32_t a0_in, uint32_t a1_in, uint32_t a2_in);
// The routines above (moby_shadows_fast.cpp) run the hand-written forms (moby_shadows_hand.cpp) or these, their
// transliterations (moby_shadows_asm.cpp), the reference: RAC1_DROP_SHADOWS_FAST.
void shadow_points_tr();                                              // orig: (host) 0x285a68 transliterated
void shadow_points_clip_tr(uint32_t a0_in, uint32_t a1_in);           // orig: (host) 0x286848 transliterated
void shadow_point_draw_tr(uint32_t a0_in);                            // orig: (host) 0x2860a8 transliterated
void shadow_segment_draw_tr(uint32_t a0_in);                          // orig: (host) 0x2861f8 transliterated
void shadow_packets_tr(uint32_t a0_in, uint32_t a1_in);               // orig: (host) 0x2868c0 transliterated
void shadow_quads_tr(uint32_t a0_in, uint32_t a1_in, uint32_t a2_in);  // orig: (host) 0x285d30 transliterated

}  // namespace rac1

#endif
