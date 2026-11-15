// See sky_levels.h and variants_sky.cpp (the table of the per-level dispatchers). The per-level dispatchers are hand-written from their listings (31 instructions each, the same shape on all eighteen levels
// checked by diffing the instruction streams); the helpers they call are the ports of sky_ee.cpp (Veldin's addresses, translated).
#include <game/ovl/sky_levels.h>

#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sky_ee.h>
#include <game/ovl/variants_sky.h>
#include <game/enhance/sections.h>
#include <host/level_slots.h>

namespace rac1 {

namespace {

// orig: (none) — enhancement: the level whose sky is the game's (0x16051c) when it is not the played level's own: a level mod's
// sky line (another level's sky copied into its asset data), or the current section's; -1 the level's own (always on the
// original's levels)
int sky_drawn_level() {
    if (!level_slots_on()) return -1;
    const int slot = level_slot_current();
    if (slot < 0) return -1;
    const int section = sections_sky_level();
    return section >= 0 ? section : level_slot_sky(slot);
}

// orig: (none) — enhancement: level `level`'s variant, the one its own dispatcher runs (Veldin's: 0x289330)
SkyTrFn sky_variant_of(int level) {
    if (level == 0) return ovl_289330_tr;
    const SkyLevel* s = sky_level(level);
    return s != nullptr ? s->variant : nullptr;
}

}  // namespace

// orig: 0x0023d968
// orig: L1:0x00252570 L2:0x00240048 L3:0x0022c6a8 L4:0x00230f48 L5:0x00267520 L6:0x0024d218 L7:0x00265b58 L8:0x00248148 L9:0x0025d648
// orig: L10:0x00230fd0 L11:0x00263908 L12:0x00256748 L13:0x002497d8 L14:0x00245940 L15:0x0022dad0 L16:0x00239c30 L17:0x00238c50 L18:0x00240840
void sky_render() {
    // enhancement: another level's sky (a level mod's or a section's sky line) drawn by that level's own variant, as on its
    // level. Each variant is made for its own sky: Veldin's makes 256 stars into the sky's sprite list (Umbris's, Rilgar's,
    // Gemlik's, Quartu's have room for 128: the rest overwrote their first shell; Kerwan's and most others have no list: they
    // went to EE address 0), and turns its own shells. The dispatchers' init and tail are the same on every build.
    if (const int from = sky_drawn_level(); from >= 0 && from != static_cast<int32_t>(lw(0x0015ed84))) {
        if (const SkyTrFn v = sky_variant_of(from); v != nullptr) {
            ovl_28b680_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            v(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            ovl_28b710_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            sub_002a0ba8(0x47, 0x5360b);
            sub_002a0ba8(0x4e, 0x01000000ull | static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(OV(0x0015ee88))) >> 13)));
            return;
        }
    }
    const SkyLevel* s = sky_level(ovl_reloc_level());
    if (s == nullptr) {  // Veldin's build (and every level played on its image): the twenty-way dispatcher
        sky_render_tr(0);
        return;
    }
    // the level's own build: jal init (0x28b680); level == N ? variant : default (0x2892d0); jal tail (0x28b710); the two 0x2a0ba8 calls
    ovl_28b680_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (lw(0x0015ed84) == static_cast<uint32_t>(s->level)) s->variant(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    else ovl_2892d0_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    ovl_28b710_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(0x47, 0x5360b);
    // (or a1, v0, a1 in the delay slot: the sign-extended arithmetic shift of the word, ORed with 0x01000000)
    sub_002a0ba8(0x4e, 0x01000000ull | static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(OV(0x0015ee88))) >> 13)));
}

}  // namespace rac1
