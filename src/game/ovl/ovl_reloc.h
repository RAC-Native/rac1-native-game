// The ports' overlay addresses on the played level's own overlay (ADR 0008). The ports are written
// against Veldin's overlay (level 0); every other level ships the same engine relinked at other
// addresses. An overlay literal of a port goes through OV (data:
// sections 0..5, 0x15ef00..0x1ea300) or OVF (code): Veldin's address on Veldin, the level's own
// address elsewhere, from the maps tools/overlay_match generates from the ISO
// (assets/generated/overlay_map/levelN.txt). Each use site caches its translation until the
// level changes: one compare on the hot path.
#ifndef RAC1_GAME_OVL_OVL_RELOC_H
#define RAC1_GAME_OVL_OVL_RELOC_H

#include <cstdint>

namespace rac1 {

extern uint32_t g_ovl_gen;  // bumped by every change of the translation
// The boot module's image (SCUS_971.99's own engine build: the boot, the title, its menu) as a translation target, like a
// level's overlay (assets/generated/overlay_map/
// front_reloc.txt, tools/overlay_match/front_match.py).
constexpr int OVL_FRONT = 100;
// No overlay: the ports' addresses as they are (the boot module's loader before the overlay load). On
// the NTSC-U disc level 0 is the same; on another region's disc level 0 is its own Veldin, translated.
constexpr int OVL_NONE = -1;
// orig: (host) the translation's level: 0 (or a level played on Veldin's image) is the identity;
// OVL_FRONT the boot module
void ovl_reloc_set_level(int level);
int ovl_reloc_level();
// orig: (host) the translation is the identity (NTSC-U Veldin's image, or OVL_NONE)
bool ovl_reloc_identity();
// orig: (host) Veldin's data / code address on the current level's overlay
// (data: an address inside an engine object - the extents of tools/overlay_match/extents.py, levelN.ext - moves with
// it; an address in no engine object of the level is kept, and logged once)
uint32_t ovl_xlate_data(uint32_t veldin);
uint32_t ovl_xlate_code(uint32_t veldin);
// orig: (host) an address of the current level's overlay data as Veldin's: inside an engine object (its extent), the
// same place in Veldin's; 0 for the level's own data (what a cross-level context copies: docs/open-engine.md)
uint32_t ovl_data_to_veldin(uint32_t level_addr);
// orig: (host) the current level's engine objects have their extents (levelN.ext; Veldin's own image: always)
bool ovl_has_extents();
// orig: (host) Veldin's function `veldin` (or an address inside it) on the current level's overlay,
// 0 when the level has no equivalent (the per-level oracle skips those functions; nothing logged)
uint32_t ovl_code_lookup(uint32_t veldin);
// orig: (host) a code address of the current level's overlay as Veldin's (0: none) -- the
// dispatch of the level's code pointers to the ports
uint32_t ovl_code_to_veldin(uint32_t level_addr);
// orig: (host) RAC1_PERF_CODE_MEMO's account at the end of a run (ovl_reloc.cpp)
void ovl_code_memo_report();

// orig: (host) one use site's cached translation
template <uint32_t A>
inline uint32_t ov_site() {
    static uint32_t gen = 0xffffffffu, v = 0;
    if (gen != g_ovl_gen) {
        v = ovl_xlate_data(A);
        gen = g_ovl_gen;
    }
    return v;
}
template <uint32_t A>
inline uint32_t ovf_site() {
    static uint32_t gen = 0xffffffffu, v = 0;
    if (gen != g_ovl_gen) {
        v = ovl_xlate_code(A);
        gen = g_ovl_gen;
    }
    return v;
}
// The region layer: the level-only ports (`L<n>_ADDR`, `// orig: L<n>:`) and the boot
// module's (`FR`, `// orig: front:`) carry NTSC-U (SCUS-97199) addresses of that level's own overlay /
// of the boot module. On another region's disc (the PAL release, SCES-50916) the same code and data
// sit elsewhere: the region table's runs say where (data/regions/<serial>.txt, written for the
// runtime by tools/overlay_match/region_maps.py apply as assets/generated/overlay_map/
// region_levelN.txt / region_boot.txt). On the NTSC-U disc both are the identity.
// orig: (host) the NTSC-U address `a` of unit `unit` (a level 0..18, or OVL_FRONT: the boot module)
// on the disc's build (kept, and logged once, when the region changed that code)
uint32_t region_xlate(int unit, uint32_t a);
// orig: (host) a code address of the disc's build of unit `unit` as NTSC-U's (kept when none): the
// key the level-only dispatchers' `case` labels compare against
uint32_t region_to_ntsc(int unit, uint32_t p);
// orig: (host) the disc is another region's (the identity otherwise; cached)
bool region_other();
// The open engine's contexts (docs/open-engine.md, src/game/enhance/context.h): in a custom-level session, level N's own
// code runs while another level's overlay (the session's base: Veldin's) is loaded. A site of level N then translates
// through N's context: N's engine data to the session's (N -> Veldin through N's map), N's own data to a relocated copy of
// N's data sections in the session heap, N's own code to a tag (below). Without a context for N, or with N's own overlay
// loaded, a site is region_xlate's, as on every level of the original.
// The bit of each level that has a context (0: none, the original's case: one test)
extern uint32_t g_ctx_levels;
// Bumped by every change of a site's translation: an overlay loaded (the level whose overlay is loaded is native), a
// context created or dropped
extern uint32_t g_region_gen;
// orig: (host) the context's translation of level `unit`'s address `a`, set by the contexts (context.cpp)
extern uint32_t (*g_ctx_xlate)(int unit, uint32_t a);
// orig: (host) one site's translation: through level `unit`'s context in a session (`unit` not the loaded overlay's),
// region_xlate otherwise
uint32_t region_site_xlate(int unit, uint32_t a);
// A tag: a code address of level A's own code handed out by A's context (a site naming it, a word of A's copied data:
// an update function, a callback, a jump table's entry). Bit 30 set: outside the EE memory, its mirrors and every
// session's RAM (512 MB at most); never a Veldin address. `A << 22 | (a - 0x1d0000)`: every level's code lies in
// 0x1d0000..0x5d0000. The dispatchers (ovl_dispatch.cpp) and lvl_key untag it; the address is NTSC-U level A's.
constexpr uint32_t CTX_TAG = 0x40000000u;
constexpr uint32_t CTX_CODE_LO = 0x001d0000u;
inline bool ctx_is_tag(uint32_t p) { return (p & 0xc0000000u) == CTX_TAG; }
inline uint32_t ctx_tag(int level, uint32_t a) { return CTX_TAG | static_cast<uint32_t>(level) << 22 | (a - CTX_CODE_LO); }
inline int ctx_tag_level(uint32_t p) { return static_cast<int>((p >> 22) & 0xffu); }
inline uint32_t ctx_tag_addr(uint32_t p) { return (p & 0x003fffffu) + CTX_CODE_LO; }
// orig: (host) one use site's translation: cached until the translation changes (the region's is fixed for the process;
// a context's lasts while it lives)
template <int N, uint32_t A>
inline uint32_t region_site() {
    static uint32_t gen = 0xffffffffu, v = 0;
    if (gen != g_region_gen) {
        v = region_site_xlate(N, A);
        gen = g_region_gen;
    }
    return v;
}
// A `lui` half of a level-only address that the transliteration keeps apart from its low
// half (tools/codegen/reloc_level.py folds every use it can follow into the full address): the NTSC-U value,
// never translated (a page is not an address: the addresses around it move by different amounts on
// another region's disc). RAC1_PAGE_POISON=1 moves it 16 MB away: a diagnostic for the oracle (a use left unfolded
// fails; so does a page that is a value: a mask passed to a call, a colour built by lui + ori).
extern uint32_t g_page_poison;
inline uint32_t lvl_page(uint32_t a) { return a ^ g_page_poison; }
// orig: (host) a code pointer of the current level's overlay as NTSC-U's: a `switch` over
// `case L<n>_CASE(...)` labels
// (a context's tag: its NTSC-U address)
inline uint32_t lvl_key(uint32_t p) {
    if (g_ctx_levels != 0 && ctx_is_tag(p)) return ctx_tag_addr(p);
    return region_other() ? region_to_ntsc(ovl_reloc_level(), p) : p;
}

// A named level-only address (a namespace-scope constant of a level's port): converts on use.
template <int N, uint32_t A>
struct LvlConst {
    operator uint32_t() const { return region_site<N, A>(); }
};

// A named boot module address (a namespace-scope constant of a `front:` port).
template <uint32_t A>
using FrConst = LvlConst<OVL_FRONT, A>;

// A named overlay address (a namespace-scope constant of a port): converts on use.
template <uint32_t A>
struct OvConst {
    operator uint32_t() const { return ov_site<A>(); }
};

// orig: (host) a text id the North American build's code names (a literal of a hand port: sub_00203ad8's, a HUD
// message's...), as the disc's text tables number it. The 1.02 build (the PAL disc 2.00, whose core moved) added five
// credits lines to every table (ids 20695 and 20743..20746): the ids from 20695 one higher, from 20742 five higher. Ids
// read from the disc's data (the menus' rows, the items' tables) and the 1.02 stand-ins' own are the disc's already: never
// through here. The identity on the North American disc and the PAL disc 1.00
int32_t region_text_id(int32_t id);

}  // namespace rac1

#define OV(a) (::rac1::ov_site<(a)>())
#define OVF(a) (::rac1::ovf_site<(a)>())
// A literal in the overlay's address range that is a value, not an address (a colour, a flag
// mask, a GS memory address: the original builds it with lui + ori): never translated.
#define NOT_ADDR(a) (a)
// An address of level n's own overlay (NTSC-U), on the disc's build: the variants_l<n>.h
// macros L<n>_ADDR. A `case` label cannot be a lookup: it is L<n>_CASE (the NTSC-U address), and its
// switch compares lvl_key(pointer).
#define LVL_ADDR(n, a) (::rac1::region_site<(n), (a)>())
// An address of the boot module's own image (NTSC-U `front:`), on the disc's build.
#define FR(a) (::rac1::region_site<::rac1::OVL_FRONT, (a)>())
// A text id the North American code names, on the disc's build (region_text_id).
#define TXT(id) (::rac1::region_text_id(id))
// A level-only address's `lui` half (see lvl_page): the variants_l<n>.h macros L<n>_PAGE.
#define LVL_PAGE(a) (::rac1::lvl_page(a))

#endif
