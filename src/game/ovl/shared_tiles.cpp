// The tile grids of Umbris (level 7), Pokitaru (11), Hoven (12) and Gemlik Base (13): a level buffer of tile records (0x1190
// bytes: +0 / +4 the tile's place, +8 its height, +0xc .. +0x13 its eight neighbours' indexes, 0xff none), 36 / 20 / 17 / 22 of
// them; and Pokitaru's and Hoven's Raritanium pickup (class 37). Each family's copies are the same code, relinked, apart
// from the tile count and the pvar of a tile's index (+4 on Pokitaru, +0xc on Hoven). Level words: the tile buffer, the
// pickup's message (per level below); the group lists 0x1ab840, the moby list 0x15ffd8, the message buffer 0x179118 and its
// timer 0x15f640 go through OV(); the Raritanium flag 0x13d389 and the hero 0x13f3d0 are the base ELF's. Header
// shared_tiles.h.
#include <game/ovl/shared_tiles.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
constexpr uint32_t kTile = 0x1190;

uint32_t tiles() {
    if (on_level(UMBRIS)) return L7_ADDR(0x001dce40);
    if (on_level(POKITARU)) return L11_ADDR(0x001da880);
    if (on_level(GEMLIK)) return L13_ADDR(0x001d9bc0);
    return L12_ADDR(0x001cbf40);
}
int32_t tile_count() {
    if (on_level(UMBRIS)) return 36;
    if (on_level(POKITARU)) return 20;
    if (on_level(GEMLIK)) return 22;
    return 17;
}
}  // namespace

// orig: L7:0x0030cd48 L11:0x0030e730 L12:0x002bf770 L13:0x0030a488
void tile_links(int32_t i) {
    const uint32_t base = tiles();
    const uint32_t me = base + static_cast<uint32_t>(i) * kTile;
    for (uint32_t k = 0xc; k <= 0x13; ++k) sb(me + k, 0xff);
    const float close = 0.1f, step = 16.0f;
    auto near = [close](float v) { return abs_f(v) < close; };
    for (int32_t j = 0; j < tile_count(); ++j) {
        if (j == i) continue;
        const uint32_t other = base + static_cast<uint32_t>(j) * kTile;
        const float dx = fpu_sub(lwc1(me), lwc1(other));
        const float dy = fpu_sub(lwc1(me + 4), lwc1(other + 4));
        const uint32_t n = static_cast<uint32_t>(j);
        if (near(dx)) {
            if (near(fpu_sub(dy, step))) sb(me + 0xc, n);
            if (near(fpu_add(dy, step))) sb(me + 0xe, n);
        } else if (near(dy)) {
            if (near(fpu_sub(dx, step))) sb(me + 0xd, n);
            if (near(fpu_add(dx, step))) sb(me + 0xf, n);
        } else if (near(fpu_add(dx, step))) {
            if (near(fpu_sub(dy, step))) sb(me + 0x11, n);
            if (near(fpu_add(dy, step))) sb(me + 0x13, n);
        } else if (near(fpu_sub(dx, step))) {
            if (near(fpu_sub(dy, step))) sb(me + 0x10, n);
            if (near(fpu_add(dy, step))) sb(me + 0x12, n);
        }
    }
}

// orig: L11:0x0030e690 L12:0x002bf6d0
void tile_group_height(uint32_t m) {
    const uint32_t index_at = on_level(POKITARU) ? 4u : 0xcu;
    const uint32_t base = tiles();
    const uint32_t list = lwu(OV(0x0015ffd8));
    for (uint32_t e = lwu(OV(0x001ab840) + (lbu(m + 0x21) << 2));; e += 2) {  // (the group's halves, the last with bit 15)
        const uint32_t member = list + ((lhu(e) & 0x7fffu) << 8);
        swc1(member + 0x18, lwc1(m + 0x18));
        const int32_t tile = lw(lwu(member + 0x78) + index_at);
        swc1(base + static_cast<uint32_t>(tile * static_cast<int32_t>(kTile)) + 8, lwc1(m + 0x18));
        if (lh(e) < 0) break;
    }
}

// The pickup: Ratchet within 2 of it gets the Raritanium (its flag), the message "Raritanium Acquired" (English, built into
// the level's code) shown for three seconds, and the pickup is gone.
// orig: L11:0x002cb790 L12:0x002bf9b8
void raritanium_pickup_update(uint32_t m) {
    if (!(vec3_distance(V(m + 0x10), V(0x0013f3d0)) < 2.0f)) return;
    sb(0x0013d389, 1);
    libc_sprintf(OV(0x00179118), on_level(POKITARU) ? L11_ADDR(0x0021a900) : L12_ADDR(0x0020bce0), {});  // (the message: no format)
    sw(OV(0x0015f640), static_cast<uint32_t>(frame_scale_i(0xb4)));
    moby_retire(m);
}

}  // namespace rac1
