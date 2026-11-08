// Ported overlay code: the per-frame moby update pass (0x2648e0 / 0x264848) and the visible
// queue it walks (0x250768).
#include <game/debug_hooks.h>
#include <game/ovl/ovl_reloc.h>
#include <host/mod_code.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

#include <cmath>
#include <cstring>

namespace rac1 {

bool (*g_debug_moby_frozen)(uint32_t moby) = nullptr;

namespace {
constexpr uint32_t SPR = 0x70000000u;

// Appends a moby to the chain of its class byte +0x22 (scratchpad 0x70000000 + class * 8: head,
// tail; the tail's +0x28 links the next), and keeps its index in the list at `ids` when it has
// flag 0x1000.
// orig: (helper) repeated in 0x00250768
void queue_add(uint32_t m, uint32_t& ids) {
    const uint32_t slot = SPR + lbu(m + 0x22) * 8u;
    ids += (lhu(m + 0x34) & 0x1000u) != 0 ? 2u : 0u;
    const uint32_t tail = lwu(slot + 4);
    sw(slot + 4, m);
    if (tail == 0) {
        sw(slot, m);
    } else {
        sw(tail + 0x28, m);
    }
}
}  // namespace

// The frame's visible mobys, chained by class: the moby list *(0x15ffd8) to the state 0xff is
// walked; a moby with state >= 0, not hidden (flags +0x34 bit 2) and within its draw distance
// +0x30 of the camera 0x166dc0 (unless +0x31 is set or the distance is 0xff) marks its group
// +0x21 (scratchpad 0x70000c80); without a group (< 0) it is queued at once. Then every moby of
// each marked group (lists 0x1ab840[group], halfword ids, the last with bit 15) with state >= 0.
// The ids of the queued mobys with flag 0x1000 go to 0x1aba00 (addresses, 0-terminated); the
// class chains are linked in class order through +0x28. Returns the first moby (0 if none).
// orig: 0x00250768
uint32_t moby_visible_queue() {
    for (uint32_t a = SPR; a < SPR + 0x700; a += 4) sw(a, 0);
    for (uint32_t a = SPR + 0x800; a < SPR + 0xc00; a += 4) sw(a, 0);
    for (uint32_t a = SPR + 0xc80; a < SPR + 0xd00; a += 4) sw(a, 0);
    const float cx = lwc1(OV(0x00166dc0)), cy = lwc1(OV(0x00166dc4)), cz = lwc1(OV(0x00166dc8));
    uint32_t ids = SPR + 0x800;
    const uint32_t base = lwu(OV(0x0015ffd8));
    uint32_t idx = 0xffffffffu;
    for (uint32_t m = base;; m += 0x100) {
        ++idx;
        const int8_t state = static_cast<int8_t>(lbu(m + 0x20));
        if (state < 0) {
            if (state == -1) break;
            continue;
        }
        const uint32_t dist = lbu(m + 0x30);
        const uint32_t flags = lhu(m + 0x34);
        if ((flags & 2u) != 0) continue;
        if (lbu(m + 0x31) == 0 && dist != 0xff) {  // VU0: d = dist^2 - dx^2 - dy^2 - dz^2 (ACC.x)
            const float dx = lwc1(m + 0x10) - cx, dy = lwc1(m + 0x14) - cy, dz = lwc1(m + 0x18) - cz;
            const float fd = static_cast<float>(dist);
            float acc = fd * fd;
            acc = acc - dx * dx;
            acc = acc - dy * dy;
            acc = acc - dz * dz;
            if (std::signbit(acc)) continue;
        }
        const int8_t group = static_cast<int8_t>(lbu(m + 0x21));
        sb(SPR + 0xc80 + static_cast<uint32_t>(static_cast<int32_t>(group)), 1);
        if (!(group < 0)) continue;
        sh(ids, idx);
        queue_add(m, ids);
    }
    for (uint32_t g = 0; g < 0x70; ++g) {
        if (lbu(SPR + 0xc80 + g) == 0) continue;
        uint32_t p = lwu(OV(0x001ab840) + g * 4);
        for (;;) {
            const int16_t e = lh(p);
            p += 2;
            const uint32_t id = static_cast<uint32_t>(e) & 0x7fffu;
            sh(ids, id);
            const uint32_t m = id * 0x100 + base;
            if (!(static_cast<int8_t>(lbu(m + 0x20)) < 0)) queue_add(m, ids);
            if (e < 0) break;
        }
    }
    uint32_t out = OV(0x001aba00);
    for (uint32_t a = SPR + 0x800; a != ids; a += 2, out += 4) sw(out, lhu(a) * 0x100 + base);
    sw(out, 0);
    uint32_t first = 0, last = 0;
    for (uint32_t c = SPR; c != SPR + 0x700; c += 8) {
        const uint32_t head = lwu(c);
        if (head == 0) continue;
        const uint32_t tail = lwu(c + 4);
        if (first == 0) {
            first = head;
        } else {
            sw(last + 0x28, head);
        }
        last = tail;
        sw(tail + 0x28, 0);
    }
    return first;
}

// The update of the visible mobys: the animation step (unless flag 0x40), the class update
// function +0x74, the transform (unless flag 4), in queue order (+0x28).
// orig: 0x00264848
void moby_update_visible() {
    const uint32_t first = moby_visible_queue();
    sw(OV(0x0015ffe4), first);
    for (uint32_t m = first; m != 0; m = lwu(m + 0x28)) {
        if (static_cast<int8_t>(lbu(m + 0x20)) < 0) continue;
        if (g_debug_moby_frozen != nullptr && g_debug_moby_frozen(m)) continue;  // host debug menu (not in the original)
        if ((lhu(m + 0x34) & 0x40u) == 0) moby_anim_advance(m);
        // (enhancement: a level mod's placed object a plugin updates instead, src/host/mod_code.h; else as the original)
        if (g_mod_code_objects && mod_code_object_update(m)) {
        } else if (const uint32_t fn = lwu(m + 0x74); fn != 0) {
            ovl_call_moby_fn(fn, m);
        }
        if ((lhu(m + 0x34) & 4u) == 0) moby_update_transform(m);
    }
}

// The moby updates of the frame; with L1 + L2 + R2 and R1 held (0x13cae0) the same pass is
// timed per class into VU0 memory and printed (a developer profile; the timing is the host's
// to skip).
// orig: 0x002648e0
void moby_updates() {
    if ((ld(0x0013cae0) & 0x800000007ull) == 0x800000007ull) TODO_PORT(0x002648e0, "moby update profile (VU0 memory, RCNT0)")();
    moby_update_visible();
}

}  // namespace rac1
