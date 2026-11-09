// Ported overlay code: the overlay's start-up tables (0x23d028 and its callees).
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libc.h>
#include <game/lib/snd989.h>
#include <game/port_stubs.h>
#include <host/level_slots.h>

namespace rac1 {

// orig: 0x00264bf8 — registers 0x266168 as 989snd's stream / CD callback (0x12ef28).
void sub_00264bf8() { snd_set_stream_callback(OVF(0x00266168u)); }

// orig: 0x002a07c8 — the GS memory start of this level (table 0x1c20b8; levels from 0x13 use row 0).
void sub_002a07c8() {
    const int32_t l = lw(0x0015ed84);
    sw(OV(0x001611cc), lwu(OV(0x001c20b8) + static_cast<uint32_t>(l < 0x13 ? l : 0) * 4));
}

// orig: 0x002a1128 — installs the DMA / VIF interrupt handlers (kernel calls 0x1187b0, 0x119160)
// once; the runtime has no interrupt controller.
void sub_002a1128() {
    if (lw(OV(0x001611d8)) != 0 || lw(OV(0x001611dc)) != 0) return;
    TODO_PORT(0x001187b0, "interrupt handlers (DMA channels 1 / 15)")();
}

// orig: 0x00268900 — the particle type function table at 0x1b1e80 (81 types, see particles_update).
void particle_table_init() {
    const uint32_t T[][2] = {
        {OV(0x001b1e80), OVF(0x00268d48)}, {OV(0x001b1e84), OVF(0x00268fe8)}, {OV(0x001b1e88), OVF(0x00269290)}, {OV(0x001b1e8c), OVF(0x002694f8)}, {OV(0x001b1e90), OVF(0x00269a70)},
        {OV(0x001b1e94), OVF(0x00269c70)}, {OV(0x001b1e98), OVF(0x00269d20)}, {OV(0x001b1e9c), OVF(0x0026a418)}, {OV(0x001b1ea0), OVF(0x0026a5b8)}, {OV(0x001b1ea4), OVF(0x0026a6f0)},
        {OV(0x001b1ea8), OVF(0x0026a880)}, {OV(0x001b1eac), OVF(0x0026ac78)}, {OV(0x001b1eb0), OVF(0x0026b500)}, {OV(0x001b1eb4), OVF(0x0026b910)}, {OV(0x001b1eb8), OVF(0x0026ba68)},
        {OV(0x001b1ebc), OVF(0x0026bca8)}, {OV(0x001b1ec0), OVF(0x0026c088)}, {OV(0x001b1ec4), OVF(0x0026c3c8)}, {OV(0x001b1ec8), OVF(0x0026c558)}, {OV(0x001b1ecc), OVF(0x0026c7f0)},
        {OV(0x001b1ed0), OVF(0x0026c860)}, {OV(0x001b1ed4), OVF(0x0026ccb0)}, {OV(0x001b1ed8), OVF(0x00267b10)}, {OV(0x001b1edc), OVF(0x0026d1b0)}, {OV(0x001b1ee0), OVF(0x0026d470)},
        {OV(0x001b1ee4), OVF(0x0026d700)}, {OV(0x001b1ee8), OVF(0x0026dbe0)}, {OV(0x001b1eec), OVF(0x0026de00)}, {OV(0x001b1ef0), OVF(0x0026e150)}, {OV(0x001b1ef4), OVF(0x0026e3a8)},
        {OV(0x001b1ef8), OVF(0x0026e670)}, {OV(0x001b1efc), OVF(0x0026eb88)}, {OV(0x001b1f00), OVF(0x0026ee48)}, {OV(0x001b1f04), OVF(0x0026f048)}, {OV(0x001b1f08), OVF(0x0026f280)},
        {OV(0x001b1f0c), OVF(0x0026f660)}, {OV(0x001b1f10), OVF(0x0026f898)}, {OV(0x001b1f14), OVF(0x0026fa00)}, {OV(0x001b1f18), OVF(0x0026fb00)}, {OV(0x001b1f1c), OVF(0x0026fc38)},
        {OV(0x001b1f20), OVF(0x0026ff10)}, {OV(0x001b1f24), OVF(0x00270710)}, {OV(0x001b1f28), OVF(0x00270948)}, {OV(0x001b1f2c), OVF(0x002710a0)}, {OV(0x001b1f30), OVF(0x00271450)},
        {OV(0x001b1f34), OVF(0x00271748)}, {OV(0x001b1f38), OVF(0x002719f0)}, {OV(0x001b1f3c), OVF(0x00271be8)}, {OV(0x001b1f40), OVF(0x00271c88)}, {OV(0x001b1f44), OVF(0x00271d20)},
        {OV(0x001b1f48), OVF(0x00271df8)}, {OV(0x001b1f4c), OVF(0x00271ec8)}, {OV(0x001b1f50), OVF(0x00271fc0)}, {OV(0x001b1f54), OVF(0x002721f0)}, {OV(0x001b1f58), OVF(0x00272438)},
        {OV(0x001b1f5c), OVF(0x002726d8)}, {OV(0x001b1f60), OVF(0x00272838)}, {OV(0x001b1f64), OVF(0x002728e8)}, {OV(0x001b1f68), OVF(0x00272bc0)}, {OV(0x001b1f6c), OVF(0x00273088)},
        {OV(0x001b1f70), OVF(0x00273298)}, {OV(0x001b1f74), OVF(0x00273448)}, {OV(0x001b1f78), OVF(0x00273800)}, {OV(0x001b1f7c), OVF(0x002738b0)}, {OV(0x001b1f80), OVF(0x00273a68)},
        {OV(0x001b1f84), OVF(0x00273ee0)}, {OV(0x001b1f88), OVF(0x00274308)}, {OV(0x001b1f8c), OVF(0x00274518)}, {OV(0x001b1f90), OVF(0x00274818)}, {OV(0x001b1f94), OVF(0x00274a70)},
        {OV(0x001b1f98), OVF(0x00274de0)}, {OV(0x001b1f9c), OVF(0x00274e48)}, {OV(0x001b1fa0), OVF(0x002752e0)}, {OV(0x001b1fa4), OVF(0x00275320)}, {OV(0x001b1fa8), OVF(0x00275510)},
        {OV(0x001b1fac), OVF(0x002756f0)}, {OV(0x001b1fb0), OVF(0x00275810)}, {OV(0x001b1fb4), OVF(0x00275860)}, {OV(0x001b1fb8), OVF(0x00275aa0)}, {OV(0x001b1fbc), OVF(0x00275d68)},
        {OV(0x001b1fc0), OVF(0x00275eb0)},
    };
    for (const auto& e : T) sw(e[0], e[1]);
}

// orig: 0x00262d38 — show the level's arrival message (table 0x1e9060, one id per level 0..18)
// for 0x49c frames.
void level_arrival_message(int32_t level) {
    int32_t l = level;
    if (!(-1 < l)) l = 0;
    if (!(l < 0x13)) l = 0x12;
    const int32_t frames = frame_scale_i(0x49c);
    sub_00263d40(lw(OV(0x001e9060) + static_cast<uint32_t>(l) * 4), frames);
}

// orig: 0x002607d0 — the first visit of level `l` (flag 0x13dd40[l]): it gets the next free
// slot of the visit order 0x13d510 (the count of levels seen) and, unless it is the current
// level, its arrival message.
void level_mark_visited(int32_t l) {
    const uint32_t flags = 0x0013dd40;
    if (lbu(flags + static_cast<uint32_t>(l)) != 0) return;
    int32_t n = 0;
    for (int32_t i = 0; i < 0x14; ++i)
        if (lbu(flags + static_cast<uint32_t>(i)) != 0) ++n;
    sb(flags + static_cast<uint32_t>(l), 1);
    sw(0x0013d510 + static_cast<uint32_t>(n) * 4, static_cast<uint32_t>(l));
    if (l != lw(0x0015ed84)) level_arrival_message(l);
}

// orig: 0x0023d3d8 — put the gadget of level `l` (table 0x1c3e98) in the first free quick-select
// slot (0x15edd0, 0xff = free) unless it is there already.
void quick_select_add_level_item(int32_t l) {
    const uint32_t item = lwu(OV(0x001c3e98) + static_cast<uint32_t>(l) * 4);
    if (item == 0) return;
    constexpr uint32_t Q = 0x0015edd0;
    for (uint32_t i = 0;; ++i) {
        const uint32_t b = lbu(Q + i);
        if ((b & 0x3f) == item) return;
        if (b == 0xff) {
            sb(Q + i, item);
            return;
        }
    }
}

// orig: 0x00260860 — give the player item `id`: owned flags 0x13d4c0 / 0x13d4e8, its message
// (table 0x1b0340, the upgraded name +0x25), the ammo maximum (0x1c40b0 rows / 0x13d428), a
// quick-select slot when it is a level gadget, and, by its kind (0x179ac0 rows), the current
// weapon / gadget slots (with `equip`) or the first free item slot 0x141ea0.
void item_give(int32_t id, int32_t equip) {
    const uint32_t owned = 0x0013d4c0 + static_cast<uint32_t>(id), seen = 0x0013d4e8 + static_cast<uint32_t>(id);
    const uint32_t msg = OV(0x001b0340) + static_cast<uint32_t>(lbu(owned) == 0 ? id : id + 0x25) * 4;
    sb(seen, 1);
    if (lw(msg) != -1) sub_00263d40(lw(msg), frame_scale_i(0x12c));
    if (lbu(owned) != 0) return;
    sb(seen, 1);
    sb(owned, 1);
    const uint32_t row = OV(0x001c40b0) + static_cast<uint32_t>(id) * 0x18;
    if (lhu(row + 8) != 0) {
        const uint32_t a = 0x0013d428 + static_cast<uint32_t>(id) * 4;
        int32_t v = lhu(row + 0x12);
        if (v < lw(a)) v = lw(a);
        sw(a, static_cast<uint32_t>(v));
    }
    for (int32_t i = 0; i < 0x14; ++i) {
        if (id == 0) continue;
        if (lw(OV(0x001c3e98) + static_cast<uint32_t>(i) * 4) != id) continue;
        constexpr uint32_t Q = 0x0015edd0;
        for (uint32_t k = 0;; ++k) {
            const uint32_t b = lbu(Q + k);
            if ((b & 0x3f) == static_cast<uint32_t>(id) || b == 0xff) {
                sb(Q + k, static_cast<uint32_t>(id) | 0x40u);
                break;
            }
        }
        break;
    }
    const uint32_t kind = OV(0x00179ac0) + static_cast<uint32_t>(id) * 0x4c + 8;
    if (equip != 0) {
        const int32_t k = lw(kind);
        if (k == 0) sw(0x00141408, static_cast<uint32_t>(id));
        else if (k == 3) sw(0x00141414, static_cast<uint32_t>(id));
        else if (k == 2) sw(0x00141410, static_cast<uint32_t>(id));
    }
    if (lw(kind) != 0) return;
    for (uint32_t i = 0; i < 8; ++i) {
        if (lw(0x00141ea0 + i * 4) == 0) {
            sw(0x00141ea0 + i * 4, static_cast<uint32_t>(id));
            return;
        }
    }
}

// orig: 0x0023d198 — the inventory at a level start: the wrench (item 10) for a fresh game,
// stale quick-select slots dropped, the level's gadget added; the debug levels from 0x13 get
// everything.
void inventory_level_start() {
    constexpr uint32_t Q = 0x0015edd0;
    if (!(lw(0x0015ed84) < 0x13)) {
        for (uint32_t i = 0; i < 0x25; ++i) {
            sb(0x0013d4c0 + i, 1);
            sb(0x0013d4e8 + i, 1);
        }
        sw(0x00141ea0, 0xf);
        sw(0x00141ebc, 0x13);
        static constexpr uint8_t qs[12] = {0x4a, 0x50, 0x4f, 0x54, 0x51, 0x4e, 0x4b, 0x52, 0x4d, 0x59, 0x58, 0x53};
        for (uint32_t i = 0; i < 12; ++i) sb(Q + i, qs[i]);
        static constexpr uint32_t slots[6] = {0xc, 0xd, 0xb, 0x11, 0xa, 0x10};
        for (uint32_t i = 0; i < 6; ++i) sw(0x00141ea4 + i * 4, slots[i]);
        return;
    }
    if (lbu(0x0013d4ca) == 0) {
        item_give(10, 1);
        sb(Q, 0x4a);
        sw(0x00141660, 10);
    }
    const uint32_t first = lwu(OV(0x001c3e98));
    for (uint32_t i = 0; i < 12; ++i) {
        const uint32_t b = lbu(Q + i);
        if (b == 0xff) continue;
        const uint32_t item = b & 0x3f;
        if (item == 10) continue;
        int32_t k = 0;
        if (first != item) {
            for (k = 1; k < 0x13; ++k)
                if (lwu(OV(0x001c3e98) + static_cast<uint32_t>(k) * 4) == item) break;
        }
        if (k == 0x13 || item == 0) sb(Q + i, 0xff);
    }
    if (!level_slots_keep_base_progress()) quick_select_add_level_item(lw(0x0015ed84));  // (enhancement: not the base level's gadget)
}

// orig: 0x0023d028 — the overlay's start-up: kernel / GS / interrupt set-up, the memory layout,
// the empty class and instance tables, the screen and projection, the menu table, the
// inventory, the visit of this level, and the start time 0x15ee24 (+180).
void overlay_init_tables() {
    sub_00264bf8();
    sub_002a07c8();
    memory_layout();
    const uint32_t top = lwu(0x0015ee8c);
    sw(0x0015ee78, top);
    sw(0x0015ee74, top);
    mem_fill32(OV(0x00173ec0), 0x87654321u, 0x10);
    mem_fill32(OV(0x00197bc0), 0xffffffffu, 0x800);
    mem_fill32(OV(0x0019a980), 0xffffffffu, 0xe00);
    mem_fill32(OV(0x0019a280), 0, 0xe0);
    mem_fill32(OV(0x001c5b00), 0xffffffffu, 0xc00);
    mem_fill32(OV(0x001c7700), 0xffffffffu, 0x800);
    mem_fill32(OV(0x001c6f00), 0, 0x80);
    mem_fill32(OV(0x001bc1b0), 0xffffffffu, 0x400);
    mem_fill32(OV(0x001bd3b0), 0xffffffffu, 0x400);
    mem_fill32(OV(0x001bcdb0), 0, 0x40);
    screen_setup();
    projection_setup();
    sub_002a07f8();
    particle_table_init();
    sub_002a1128();
    inventory_level_start();
    if (lw(0x0015ed84) != 0 && !level_slots_keep_base_progress()) level_mark_visited(lw(0x0015ed84));  // (enhancement: nor visited)
    sw(0x0015ee24, lwu(0x0015ee24) + 0xb4);
}

}  // namespace rac1
