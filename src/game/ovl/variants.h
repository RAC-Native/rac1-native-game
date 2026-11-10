// The engine variants of the levels' own overlays (ADR 0008 decision point 4).
//
// A level's overlay is built from the same source as Veldin's with other features compiled in. Where
// the code is the same (relocations, registers and scheduling masked) the Veldin port runs on the
// level (translated); where it differs (a "variant": no `fn` line in the generated map
// assets/generated/overlay_map/levelN.txt) the level has its own port, `// orig: LN:0x00xxxxxx` at the
// level's own address, and the Veldin function dispatches to it while that level's overlay is the
// translation target (`on_level(N)`; identity on Veldin and on a level played on Veldin's image).
//
// One table per level (variants_l1.cpp: Novalis, variants_l2.cpp: Aridia): Veldin function -> the
// level's own function. They are merged into the translation tables when the level is set, so the
// per-level oracle's "equivalent", ovl_xlate_code and ovl_code_to_veldin know them.
//
// A level may share a port with another level when the two builds have the same code there (checked
// by diffing the instruction streams, proved by the per-level oracle): the dispatch line then names
// both, `if (on_novalis() || on_aridia()) return xxx_l1(...)`, and the port's annotation lists both
// addresses (`// orig: L1:0x00226f10 L2:0x00216068`).
#ifndef RAC1_GAME_OVL_VARIANTS_H
#define RAC1_GAME_OVL_VARIANTS_H

#include <cstddef>
#include <cstdint>

#include <game/ovl/ovl_reloc.h>

namespace rac1 {

struct VariantRow {
    uint32_t veldin;  // the Veldin function this one replaces
    uint32_t level;   // its address in the level's overlay
    uint32_t size;    // bytes
};

// orig: (host) the current translation target is level `n`'s overlay
inline bool on_level(int n) { return ovl_reloc_level() == n; }

// The open engine's hero contexts (src/game/enhance/hero_modes.h, docs/open-engine.md "Hero contexts"): in a session (a level
// mod of format 3 on another level's overlay) the level whose own hero code and camera code run Ratchet, as on that level
// (Kalebo III's board, Quartu's Giant Clank): its hero controller's dispatch lines and its camera types and hooks take
// level `g_hero_level`'s branch while the session's overlay stays loaded; its addresses go through its context. -1 (always,
// outside a session): none, the overlay's own
// (HERO_GENERIC: the generic hero code, Veldin's ports, on a level mod whose base level has a hero build of its own: Kalebo
// III's lava)
extern int g_hero_level;
constexpr int HERO_GENERIC = -2;
// orig: (host) the hero code of level `n` runs: its overlay is loaded (unless the generic hero code was asked for), or it is
// the session's hero context
inline bool on_hero_level(int n) { return g_hero_level == HERO_GENERIC ? false : ovl_reloc_level() == n || g_hero_level == n; }
// The open engine's camera level (stage 10): in a session, the level whose world is the current section's (a `world <level>`
// section), whose camera code runs the cameras there (its camera types, its camera hooks and callbacks: a boss's camera on its
// planet's world) as the hero context's does. -1 (always, outside a session): none
extern int g_camera_level;
// orig: (host) the camera code of level `n` runs (its camera types and hooks): its overlay is loaded, or it is the hero context
// or the camera level
inline bool on_camera_level(int n) { return ovl_reloc_level() == n || g_hero_level == n || g_camera_level == n; }

// orig: (host) the variants of a level: Veldin function -> the level's own function (the per-level
// oracle's equivalents, the code-pointer dispatch); null / 0 for a level with none
const VariantRow* ovl_variants(int level, size_t* count);

// the per-level tables
const VariantRow* ovl_variants_l1(size_t* count);  // Novalis (variants_l1.cpp)
const VariantRow* ovl_variants_l2(size_t* count);  // Aridia (variants_l2.cpp)
const VariantRow* ovl_variants_l3(size_t* count);  // Kerwan (variants_l3.cpp)
const VariantRow* ovl_variants_l4(size_t* count);  // Eudora (variants_l4.cpp)
const VariantRow* ovl_variants_l5(size_t* count);  // Rilgar (variants_l5.cpp)
const VariantRow* ovl_variants_l6(size_t* count);  // Blarg's Nebula G34 (variants_l6.cpp)
const VariantRow* ovl_variants_l7(size_t* count);  // Umbris (variants_l7.cpp)
const VariantRow* ovl_variants_l9(size_t* count);  // Gaspar (variants_l9.cpp)
const VariantRow* ovl_variants_l8(size_t* count);  // Batalia (variants_l8.cpp)
const VariantRow* ovl_variants_l10(size_t* count);  // Orxon (variants_l10.cpp)
const VariantRow* ovl_variants_l11(size_t* count);  // Pokitaru (variants_l11.cpp)
const VariantRow* ovl_variants_l12(size_t* count);  // Hoven (variants_l12.cpp)
const VariantRow* ovl_variants_l13(size_t* count);  // Gemlik Base (variants_l13.cpp)
const VariantRow* ovl_variants_l14(size_t* count);  // Oltanis (variants_l14.cpp)
const VariantRow* ovl_variants_l15(size_t* count);  // Quartu (variants_l15.cpp)
const VariantRow* ovl_variants_l16(size_t* count);  // Kalebo III (variants_l16.cpp)
const VariantRow* ovl_variants_l17(size_t* count);  // Drek's fleet (variants_l17.cpp)
const VariantRow* ovl_variants_l18(size_t* count);  // Veldin, the finale (variants_l18.cpp)

}  // namespace rac1

#endif
