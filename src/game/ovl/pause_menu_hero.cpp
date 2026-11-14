// Ported overlay code: the hero panel of the Weapons / Gadgets pages (see pause_menu_pages.h): Ratchet (class 0) and Clank
// (0x259) as menu mobys, the equipped
// items attached to their joints (each a menu moby with its own update), the item 0x18's orbs
// circling, the hero's animation for the weapon in hand queued (0x1b9fc0) and streamed from the
// stash / the disc into the menu's two big buffers.
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};      // the menu's state
constexpr OvConst<0x00166dc0u> CAM{};     // the camera's position
constexpr OvConst<0x00179ac0u> ITEMS{};   // the item table (0x4c bytes per item)
constexpr OvConst<0x001b9fc0u> Q{};       // the animation requests (8 x 0x38 bytes)
constexpr OvConst<0x00160410u> QN{};      // their count
constexpr OvConst<0x001ba180u> PLAY{};    // the request playing
constexpr OvConst<0x00197300u> CLASSES{}; // the class table
constexpr OvConst<0x00197bc0u> SLOT_OF{}; // class -> its slot in the class table
constexpr OvConst<0x001b9ed0u> NODE_A{}; constexpr OvConst<0x001b9f10u> NODE_B{}; constexpr OvConst<0x001b9f50u> NODE_C{};  // attach nodes (0x40 bytes)
constexpr float PI_F = 3.14159274f;      // 0x40490fdb
// orig: (helper) the hero moby of a menu moby's panel (its data +0 the panel, +0x44 the hero)
uint32_t hero_of(uint32_t m) { return lwu(lwu(lwu(m + 0x78)) + 0x44); }
// orig: (helper) repeated in the attach updates: a joint's matrix (0x24f728) as the moby's
// position and orientation (normalized), its transform kept
void attach_to_joint(uint32_t m, uint32_t hero, uint32_t joint, bool normalize) {
    const uint32_t mat = ee_stack_alloc(0x40);
    moby_chain_world_one_v(hero, joint, M(mat));
    lq_sq(m + 0x10, mat + 0x30);
    if (normalize) {
        mat3_copy(M(m + 0xc0), M(mat));
        mat3_normalize_columns(m + 0xc0);
    }
    ee_stack_free(0x40);
}
// orig: (helper) repeated in 0x00281fb0: a menu moby in a panel slot replaced when its class
// changes (`cls` -1: none), with update `fn` and draw flags 4
uint32_t slot_moby(uint32_t w, uint32_t off, int32_t cls, uint32_t fn) {
    uint32_t m = lwu(w + off);
    const int32_t cur = m != 0 ? lh(m + 0xa6) : -1;
    if (cur == cls) return m;
    m = static_cast<uint32_t>(menu_moby_free(m));
    if (cls != -1) {
        m = menu_moby_spawn(static_cast<uint32_t>(cls));
        if (m != 0) {
            sw(lwu(m + 0x78), w);
            sw(m + 0x74, fn);
            sh(m + 0x34, 4);
        }
    }
    sw(w + off, m);
    return m;
}
}  // namespace

// orig: 0x002827a8 — an empty moby update (the hero's in the menu)
void menu_moby_nop_a(uint32_t) {}
// orig: 0x002827b0 — an empty moby update (the hero's animation extras)
void menu_moby_nop_b(uint32_t) {}

// The weapon in hand: on its animation's end set 1; held at the hero's hand joint (the item
// table +0xc of its class; +0x18 nonzero: oriented, else its pose encoded 0x2092c8 into
// 0x17c480); the attach node A (0x1b9ed0) reset.
// orig: 0x002827b8
void menu_weapon_moby_update(uint32_t m) {
    const uint32_t hero = hero_of(m);
    if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
    const int32_t cls = lh(m + 0xa6);
    uint32_t i = 0;
    if (lw(ITEMS + 0x10) != cls) {
        for (i = 1; i < 0x25; ++i)
            if (lw(ITEMS + i * 0x4c + 0x10) == cls) break;
    }
    const uint32_t it = ITEMS + i * 0x4c;
    const uint32_t mat = ee_stack_alloc(0x40);
    moby_chain_world_one_v(hero, lwu(it + 0xc), M(mat));
    lq_sq(m + 0x10, mat + 0x30);
    moby_update_transform(m);
    const bool plain = lwu(it + 0x18) == 0;
    mat3_copy(M(m + 0xc0), M(mat));
    ee_stack_free(0x40);
    if (!plain) mat3_normalize_columns(m + 0xc0);
    moby_update_transform_kept(m);
    bool node = false;
    if (lbu(NODE_A + 1) != 0) {
        node = true;
        sub_0024f608(hero, NODE_A);
    }
    if (plain) {
        moby_pose_encode(OV(0x0017a5c0), OV(0x0017c480), lwu(m + 0x24), 0, hero);
        sw(m + 0x6c, OV(0x0017c480));
        sw(m + 0x50, 0);
        sw(m + 0x68, OV(0x0017c480));
        sw(m + 0x54, 0);
        sw(m + 0x58, 0);
    }
    if (node) {
        sub_0024f590(hero, 0, NODE_A);
        sw(NODE_A + 0x28, 0);
        sw(NODE_A + 0x20, 0);
        sw(NODE_A + 0x24, 0);
    }
}

// The slot-2 item: at joint 4, its pose encoded (class 0x1b1 with 0x17a608 and 6, else 0x15f6a8).
// orig: 0x00282970
void menu_item2_moby_update(uint32_t m) {
    const uint32_t hero = hero_of(m);
    moby_anim_advance(m);
    moby_update_transform(m);
    attach_to_joint(m, hero, 4, true);
    moby_update_transform_kept(m);
    if (lh(m + 0xa6) == 0x1b1) moby_pose_encode(OV(0x0017a608), OV(0x0017c680), lwu(m + 0x24), 6, hero);
    else moby_pose_encode(OV(0x0015f6a8), OV(0x0017c680), lwu(m + 0x24), 0, hero);
    sw(m + 0x50, 0);
    sw(m + 0x6c, OV(0x0017c680));
    sw(m + 0x68, OV(0x0017c680));
    sw(m + 0x54, 0);
}

// The slot-1 items (two mobys: the panel's +0x5c at joint 3, the other at joint 2), their poses
// encoded; the attach nodes B / C reset.
// orig: 0x00282a60
void menu_item1_moby_update(uint32_t m) {
    const uint32_t d = lwu(m + 0x78);
    const uint32_t hero = lwu(lwu(d) + 0x44);
    moby_anim_advance(m);
    moby_update_transform(m);
    const bool second = m == lwu(lwu(d) + 0x5c);
    attach_to_joint(m, hero, second ? 3u : 2u, true);
    moby_update_transform_kept(m);
    bool nc = false, nb = false;
    if (lbu(NODE_C + 1) != 0) {
        nc = true;
        sub_0024f608(hero, NODE_C);
    }
    if (lbu(NODE_B + 1) != 0) {
        nb = true;
        sub_0024f608(hero, NODE_B);
    }
    const uint32_t out = second ? OV(0x0017c980u) : OV(0x0017c880u);
    moby_pose_encode(second ? OV(0x0017a668u) : OV(0x0017a650u), out, lwu(m + 0x24), 0, hero);
    sw(m + 0x6c, out);
    sw(m + 0x68, out);
    if (nc) {
        sub_0024f590(hero, 0x17, NODE_C);
        sw(NODE_C + 0x28, 0);
        sw(NODE_C + 0x20, 0);
        sw(NODE_C + 0x24, 0);
    }
    if (nb) {
        sub_0024f590(hero, 0x16, NODE_B);
        sw(NODE_B + 0x28, 0);
        sw(NODE_B + 0x20, 0);
        sw(NODE_B + 0x24, 0);
    }
    sw(m + 0x50, 0);
    sw(m + 0x54, 0);
}

// The slot-3 item / Clank: at joint 5; at an animation's end back to set 1 (class 0x25f in set 6
// first counts +0x20 down, then set 1 over 10 frames).
// orig: 0x00282c08
void menu_item3_moby_update(uint32_t m) {
    const uint32_t hero = hero_of(m);
    if ((lbu(m + 0x70) & 2) != 0) {
        if (lh(m + 0xa6) == 0x25f && lbu(m + 0x52) == 6) {
            const uint32_t v = lbu(m + 0x20) - 1u;
            sb(m + 0x20, v);
            if ((v & 0xff) == 0) {
                if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 10);
            } else if (lbu(m + 0x53) != 6) {
                moby_anim_set(m, 6, 0, 0);
            }
        } else if (lbu(m + 0x53) != 1) {
            moby_anim_set(m, 1, 0, 0);
        }
    }
    attach_to_joint(m, hero, 5, true);
    moby_update_transform_kept(m);
}

// The gadgets (classes 0x197 at joint 0x1e, others 0x1d).
// orig: 0x00282d38
void menu_gadget_moby_update(uint32_t m) {
    const uint32_t hero = hero_of(m);
    moby_anim_advance(m);
    moby_update_transform(m);
    attach_to_joint(m, hero, (lh(m + 0xa6) ^ 0x197) != 0 ? 0x1du : 0x1eu, true);
    moby_update_transform_kept(m);
}

// The item 0x18's orbs (+0xbc the orb): circling the hero (by the frame counter 0x15f3f8),
// bobbing, rising in (0x1b9f90, speed 0x1b9fa8, seek 0x25b970 by $gp-0x7e90 / 0x15ed6c).
// orig: 0x00282dc8
void menu_orb_moby_update(uint32_t m) {
    const uint32_t hero = hero_of(m);
    moby_anim_advance(m);
    moby_update_transform(m);
    lq_sq(m + 0x10, hero + 0x10);
    mat3_copy(M(m + 0xc0), M(hero + 0xc0));
    mat3_normalize_columns(m + 0xc0);
    swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
    const uint32_t k0 = lbu(m + 0xbc);
    const int32_t place = k0 < 3 ? static_cast<int32_t>((k0 << 1) % 6u) : static_cast<int32_t>(((k0 << 1) + 1u) % 6u);
    const float two_pi = u2f(0x40c90fd0), pi = u2f(0x40490fd0);
    float d = int_to_float(0xc8);
    const float fp = static_cast<float>(place);
    const float a0 = fpu_sub(fpu_mul(fpu_div(static_cast<float>(ee_rem(lw(OV(0x0015f3f8)), 0xc8)), d), two_pi), pi);
    d = int_to_float(6);
    const float b = fpu_sub(fpu_div(fpu_mul(fp, two_pi), d), pi);
    d = int_to_float(0xaa);
    const float c0 = fpu_sub(fpu_mul(fpu_div(static_cast<float>(ee_rem(lw(OV(0x0015f3f8)), 0xaa)), d), two_pi), pi);
    d = int_to_float(6);
    const float e = fpu_div(fpu_mul(fp, u2f(0x41490fd0)), d);
    angle_wrap(e);  // its result unused by the original
    const float a = angle_add(a0, b);
    const float c = angle_add(c0, e);
    const uint32_t k = lbu(m + 0xbc);
    const uint32_t rise = OV(0x001b9f90) + k * 4;
    if (!(lwc1(rise) == 0.0f))
        seek_1d(rise, OV(0x001b9fa8) + k * 4, 0.0f, 1.0f, fpu_mul(lwc1(OV(0x00166c00u - 0x7e90)), 6.0f), fpu_mul(lwc1(0x0015ed6c), 6.0f));
    const uint32_t sp = ee_stack_alloc(0x50);  // the offset +0, a matrix +0x10, the angles +0x40
    swc1(sp, cos_vu(a));
    swc1(sp + 8, 0.0f);
    swc1(sp + 4, sin_vu(a));
    swc1(sp + 8, fpu_add(fpu_add(fpu_mul(sin_vu(c), 0.25f), 0.5f), lwc1(OV(0x001b9f90) + lbu(m + 0xbc) * 4)));
    mat3_transform(V(sp), V(sp), M(hero + 0xc0));
    vec3_add(V(m + 0x10), V(m + 0x10), V(sp));
    vec_clear(V(sp + 0x40));
    swc1(sp + 0x48, angle_add(a, u2f(0x3fc90fdb)));
    euler_to_mat3_vu(M(sp + 0x10), V(sp + 0x40));
    mat3_mul(M(m + 0xc0), M(sp + 0x10), M(m + 0xc0));
    ee_stack_free(0x50);
    moby_update_transform_kept(m);
}

// An animation request queued (at most 8): its sequence, kind, argument, the weapon, the
// classes / sets of up to three extra mobys, its stash entries (+0x30 first, +0x34 count).
// orig: 0x00284420
int32_t hero_anim_queue(uint32_t seq, uint32_t kind, uint32_t arg, uint32_t weapon, uint32_t cls, uint32_t e1, uint32_t s1,
                        uint32_t e2, uint32_t s2, uint32_t e3, uint32_t s3, uint32_t first, uint32_t count) {
    const int32_t n = lw(QN);
    if (!(n < 8)) return -1;
    const uint32_t e = Q + static_cast<uint32_t>(n) * 0x38;
    sw(QN, static_cast<uint32_t>(n + 1));
    sw(e + 4, seq);
    sw(e + 8, kind);
    sw(e + 0xc, arg);
    sw(e + 0x10, weapon);
    sw(e + 0x14, cls);
    sw(e + 0x18, e1);
    sw(e + 0x1c, s1);
    sw(e + 0x20, e2);
    sw(e + 0x34, count);
    sw(e + 0x24, s2);
    sw(e + 0x28, e3);
    sw(e + 0x2c, s3);
    sw(e, 0);
    sw(e + 0x30, first);
    return 0;
}

// The first request dropped (the others moved up).
// orig: 0x002844b8
int32_t hero_anim_dequeue() {
    for (uint32_t e = Q; e < OV(0x001b9f88) + 0x1c0; e += 0x38)
        for (uint32_t o = 0; o < 0x38; o += 8) sd(e + o, ld(e + 0x38 + o));
    sw(QN, static_cast<uint32_t>(lw(QN) - 1));
    return 0;
}

// The requests cleared (a read going stopped).
// orig: 0x00284560
int32_t hero_anim_clear() {
    if (lh(0x001516d8) != 0 && lbu(MS + 0xcb) != 0) {
        disc_read_break();
        sb(MS + 0xcb, 0);
    }
    sw(PLAY + 0xc, 0);
    sw(QN, 0);
    return 0;
}

// The extra sequences loaded for the request playing (0x1b9ad8 + i*8: class, set) unhooked.
// orig: 0x00284600
int32_t hero_anim_extra_free() {  // returns the menu state (its last $v0)
    for (int32_t i = lw(MS + 0xa8); i < lw(MS + 0xa8) + lw(MS + 0xac); ++i) {
        const uint32_t e = OV(0x001b9ad8) + static_cast<uint32_t>(i) * 8;
        const uint32_t c = lwu(CLASSES + static_cast<uint32_t>(lbu(SLOT_OF + lwu(e))) * 4);
        sw(c + lwu(e + 4) * 4 + 0x48, 0);
    }
    sw(MS + 0xac, 0);
    return static_cast<int32_t>(MS);
}

// The extra sequences (`count` from `first`) read from the stash (0x1b9e38) into the small
// buffers (M+0xb0..; with two, the second and third), unpacked and hooked into their classes.
// orig: 0x00284690
void hero_anim_extra_load(int32_t first, int32_t count) {
    sw(MS + 0xa8, static_cast<uint32_t>(first));
    sw(MS + 0xac, static_cast<uint32_t>(count));
    if (!(count > 0)) return;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t e = OV(0x001b9ad8) + static_cast<uint32_t>(first + i) * 8;
        const uint32_t slot = lbu(SLOT_OF + lwu(e));
        const int32_t skip = (count == 2 && i == 1) ? 1 : 0;
        const uint32_t set = lwu(e + 4);
        const uint32_t entry = lwu(OV(0x001b9e38) + static_cast<uint32_t>(first + i) * 4);
        const int32_t size = stash_size(entry) << 4;
        const uint32_t buf = lwu(OV(0x001b9da0) + static_cast<uint32_t>(i + skip) * 4);
        const uint32_t dst = buf + static_cast<uint32_t>(menu_buf_size(buf)) - static_cast<uint32_t>(size);
        stash_read(dst, entry, 0, -1, 0);
        wad_decompress(dst, buf);
        sw(lwu(CLASSES + slot * 4) + set * 4 + 0x48, buf);
        moby_seq_relocate(lwu(CLASSES + slot * 4), set);
    }
}

// At the level start (overlay entry): the hero panel's 20 extra sequences (the disc table
// 0x137b80 + 0xf8: sector, count) read into the level buffer *(0x173e08) and put in the IOP stash
// (0x293c58, dest 0x1602d0), their stash entries kept at M+0x148 (0x1b9e38, read by 0x284690). (The table's own address,
// not M + 0x148: the 1.02 build, the PAL disc 2.00, put 16 bytes into the menu's state before it, so there the entries
// sit at M+0x158, where hero_anim_extra_load reads them; with M + 0x148 the extras of the hero panel's animations came
// from the entries four places further on, #163)
// orig: 0x00276078
void menu_sequences_stash() {
    uint32_t id = OV(0x001b9e38);
    uint32_t e = 0x00137b80 + 0xf8;
    const uint32_t buf = lwu(OV(0x00173e08));
    for (int32_t k = 0x13; k >= 0; --k) {
        const uint32_t n = lwu(e + 4);
        const uint32_t qw = n << 7;
        if (qw != 0) {
            disc_read_wait(buf, lwu(e), n);
            sw(id, static_cast<uint32_t>(stash_put(buf, static_cast<int32_t>(qw), static_cast<int32_t>(qw), OV(0x001602d0))));
        }
        id += 4;
        e += 8;
    }
}

// The hero model's extras in a playable mode (2 / 6): its pickup classes' flag 0x800, its
// chains, its slot mobys, 0x234808, the held mobys' flag 1 cleared.
// orig: 0x00234a50
void hero_model_refresh(uint32_t m) {
    const int32_t mode = lw(OV(0x0015f5c4));
    if (mode != 2 && mode != 6) return;
    constexpr OvConst<0x0016c860u> L{};
    for (int32_t i = 0; i < lh(L + 0x44); ++i) {
        const uint32_t o = lwu(L + 0x178 + static_cast<uint32_t>(i) * 4);
        if (static_cast<uint32_t>(lhu(o + 0xa6) - 0x509u) < 2u) sh(o + 0x34, lhu(o + 0x34) | 0x800u);
    }
    hero_chain_world(m);
    hero_slot_mobys_update();
    sub_00234808();
    constexpr uint32_t H = 0x0013f350;
    for (uint32_t k : {0x11d0u, 0x11d4u, 0x1220u}) {
        const uint32_t o = lwu(H + k);
        if (o != 0) sh(o + 0x34, lhu(o + 0x34) & 0xfffeu);
    }
}

// The hero model set up: its node 0x17c340 (attach 0x18, scaled by 0x15ee14; mode 6 at most
// 1.33), on levels 2 / 9 / 11 / 13 the classes 0x1b1 / 0x50a / 0x509 scaled too; 0x234a50.
// orig: 0x00234b38
void hero_model_setup(uint32_t m) {
    float k = lwc1(OV(0x00166c00u - 0x7dec));
    if (lw(OV(0x0015f5c4)) == 6 && u2f(0x3faa3d71) < k) k = u2f(0x3faa3d71);
    constexpr OvConst<0x0017c340u> N{};
    if (lbu(N + 1) == 0) {
        sub_0024f590(m, 0x18, N);
        swc1(N + 0x28, k);
        swc1(N + 0x20, k);
        swc1(N + 0x24, k);
    }
    const int32_t lv = lw(OV(0x00166c00u - 0x7e7c));
    if (lv == 2 || lv == 9 || lv == 0xb || lv == 0xd) {
        constexpr OvConst<0x0016c860u> L{};
        for (int32_t i = 0; i < lh(L + 0x44); ++i) {
            const uint32_t o = lwu(L + 0x178 + static_cast<uint32_t>(i) * 4);
            if (o == 0) continue;
            const int32_t c = lh(o + 0xa6);
            if (c == 0x1b1 || c == 0x50a || c == 0x509) swc1(o + 0x2c, fpu_mul(lwc1(lwu(o + 0x24) + 0x24), k));
        }
    }
    hero_model_refresh(m);
}

// The hero panel's focus in: the big buffers, the requests cleared, no class loaded (M+0x120 /
// 0x16000c / M+0x11c -1), the animation buffers (two big, three small), the nodes free; Ratchet
// (class 0) in front of the camera (x + 4, z - 0.6, facing it), his node, Clank (0x259).
// orig: 0x00281d10
int32_t menu_hero_focus_in(uint32_t w) {
    menu_vram_layout(1);
    hero_anim_clear();
    sw(MS + 0x120, 0xffffffffu);
    sw(OV(0x0016000c), 0xffffffffu);
    sw(MS + 0x11c, 0xffffffffu);
    sw(MS + 0xa0, menu_buf_alloc(1));
    sw(MS + 0xa4, menu_buf_alloc(1));
    sb(MS + 0xc9, 0xff);
    sb(MS + 0xc8, 0xff);
    sb(MS + 0xca, 0);
    for (uint32_t k = 0; k < 3; ++k) sw(MS + 0xb0 + k * 4, menu_buf_alloc(0));
    sb(NODE_A + 1, 0);
    sw(MS + 0x1c, 0xffffffffu);
    sb(NODE_B + 1, 0);
    sb(NODE_C + 1, 0);
    const uint32_t m = menu_moby_spawn(0);
    for (int32_t k = 0x17; k >= 0; --k) sb(w + 0xa4 + static_cast<uint32_t>(k), 0);
    if (m != 0) {
        sw(w + 0x44, m);
        sh(m + 0x34, 0);
        const uint32_t d = lwu(m + 0x78);
        swc1(m + 0x10, fpu_add(lwc1(CAM), 4.0f));
        swc1(m + 0x14, lwc1(CAM + 4));
        swc1(m + 0x48, PI_F);
        sw(m + 0x74, OVF(0x002827a8));
        swc1(m + 0x18, fpu_sub(lwc1(CAM + 8), u2f(0x3f19999a)));
        sw(d + 8, 0);
        sw(d, w);
        sw(d + 4, 0);
        sw(MS + 0xc0, 0xffffffffu);
        mem_fill32(OV(0x0017c340), 0, 0x40);
        hero_model_setup(m);
    }
    const uint32_t c = menu_moby_spawn(0x259);
    if (c != 0) {
        sw(lwu(c + 0x78), w);
        sw(c + 0x74, OVF(0x00282c08));
        sh(c + 0x34, 4);
    }
    sw(w + 0x48, c);
    return 0;
}

// The hero panel's focus out: its 24 mobys freed, the buffers given back, the extras unhooked.
// orig: 0x00281f00
int32_t menu_hero_focus_out(uint32_t w) {
    for (uint32_t k = 0; k < 0x18; ++k) sw(w + 0x44 + k * 4, static_cast<uint32_t>(menu_moby_free(lwu(w + 0x44 + k * 4))));
    sw(MS + 0xa0, static_cast<uint32_t>(menu_buf_free(lwu(MS + 0xa0))));
    sw(MS + 0xa4, static_cast<uint32_t>(menu_buf_free(lwu(MS + 0xa4))));
    sb(MS + 0xc9, 0xff);
    sb(MS + 0xc8, 0xff);
    sb(MS + 0xca, 0);
    hero_anim_extra_free();
    for (uint32_t k = 0; k < 3; ++k) sw(MS + 0xb0 + k * 4, static_cast<uint32_t>(menu_buf_free(lwu(MS + 0xb0 + k * 4))));
    return 0;
}

// The hero's animation: a request done (its set ended, +8 kind 0 / 1 / 2 / 3, or its weapon no
// longer equipped) starts the next: the hero's set (0x2579c8), the weapon's (slot 2: the
// second item's; else the weapon's), its extra mobys spawned at the hero (0x2827b0); the next
// request's sequence read from the disc (0x137b80 + 0x18 / 0x1c) into the free big buffer and
// unpacked when read (M+0xcb, M+0xbc).
// orig: 0x00283cb8
int32_t hero_anim_update(uint32_t hero, uint32_t weapon, uint32_t item2, uint32_t o1, uint32_t o2, uint32_t o3) {
    bool ended = false, last = false;
    if (lw(QN) == 0) return 0;
    if ((lbu(hero + 0x70) & 2) != 0) {
        ended = true;
        const int32_t c = lw(PLAY + 0xc);
        if (c == 0) {
            last = true;
        } else {
            sw(PLAY + 0xc, static_cast<uint32_t>(c - 1));
            if (c - 1 == 0) last = true;
        }
    }
    int32_t n;
    bool skip_read = false;
    if (lbu(MS + 0xcb) != 0) {
        if (lh(0x001516d8) == 0) {
            const uint32_t i = lbu(MS + 0xcb) - 1u;
            const uint32_t pb = MS + 0xa0 + i * 4;
            const uint32_t buf = lwu(pb);
            wad_decompress(buf + lwu(MS + 0xbc), buf);
            sw(MS + 0xbc, 0);
            menu_buf_ready(buf);
            sb(MS + i + 0xc8, lbu(Q + 4));
            const uint32_t seq = lwu(Q + 4);
            sw(Q, 2);
            sw(lwu(CLASSES) + seq * 4 + 0x48, lwu(pb));
            moby_seq_relocate(lwu(CLASSES), lwu(Q + 4));
            sb(MS + 0xcb, 0);
        }
        if (lbu(MS + 0xcb) != 0) skip_read = true;
    }
    n = lw(QN);
    if (!skip_read && lh(0x001516d8) == 0) {
        if (!(n > 0)) return 0;
        if (lw(Q) == 0) {
            uint32_t seq = lwu(Q + 4);
            const uint32_t cur = lbu(MS + 0xca);
            const uint32_t have = MS + 0xc8;
            if (lbu(have + cur) == seq || lbu(have + (cur < 1 ? 1u : 0u)) == seq) {
                sw(Q, 2);
            } else if (seq < lwu(MS + 0xcc)) {
                sw(Q, 3);
            } else {
                seq = (seq - lwu(MS + 0xcc)) << 3;
                const uint32_t e = 0x00137b80 + seq;
                const uint32_t buf = lwu(MS + 0xa0 + cur * 4);
                const uint32_t bytes = lwu(e + 0x1c) << 11;
                const uint32_t dst = buf + static_cast<uint32_t>(menu_buf_size(buf)) - bytes;
                sw(MS + 0xbc, dst - buf);
                if (stream_disc_read(dst, lwu(e + 0x18), lwu(e + 0x1c)) == 0) debug_trap();
                menu_buf_busy(buf);
                const uint32_t c2 = lbu(MS + 0xca);
                sw(Q, 1);
                sb(MS + 0xcb, c2 + 1u);
                sb(have + (c2 & 0xff), 0xff);
            }
        }
        n = lw(QN);
    }
    if (!(n > 0)) return 0;
    bool start = false;
    if (static_cast<uint32_t>(lw(Q) - 2) < 2u) {
        const uint32_t wpn = lwu(Q + 0x10);
        if (wpn != 0 && lwu(MS + 0x30) != wpn && lwu(MS + 0x38) != wpn && lwu(MS + 0x34) != wpn) {
            hero_anim_dequeue();
            return 0;
        }
        const int32_t kind = lw(Q + 8);
        if (kind == 0) start = true;
        else if (kind == 1 && ended) start = true;
        else if (lw(Q + 8) == 2 && last) start = true;
        else if (lw(Q + 8) == 3) {
            hero_anim_dequeue();
            start = false;
        }
    }
    if (!start) return 0;
    for (uint32_t o = 0; o < 0x38; o += 8) sd(PLAY + o, ld(Q + o));
    hero_anim_dequeue();
    hero_anim_extra_free();
    hero_anim_extra_load(lw(PLAY + 0x30), lw(PLAY + 0x34));
    moby_anim_set_flags(hero, lwu(PLAY + 4), 0, 10, 5);
    const uint32_t played = lwu(PLAY + 4);
    if (played == lbu(MS + 0xc8)) sb(MS + 0xca, 1);
    else if (played == lbu(MS + 0xc9)) sb(MS + 0xca, 0);
    if (lw(ITEMS + lwu(PLAY + 0x10) * 0x4c + 8) == 2) {
        if (item2 != 0) moby_anim_set_flags(item2, lwu(PLAY + 0x14), 0, 10, 5);
        if (weapon != 0) moby_set_anim(weapon, 1, 0);
    } else if (weapon != 0) {
        moby_anim_set_flags(weapon, lwu(PLAY + 0x14), 0, 10, 5);
    }
    sw(o1, static_cast<uint32_t>(menu_moby_free(lwu(o1))));
    sw(o2, static_cast<uint32_t>(menu_moby_free(lwu(o2))));
    sw(o3, static_cast<uint32_t>(menu_moby_free(lwu(o3))));
    const uint32_t outs[3] = {o1, o2, o3};
    for (uint32_t k = 0; k < 3; ++k) {
        const int32_t cls = lw(PLAY + 0x18 + k * 8);
        if (cls == -1) continue;
        const uint32_t m = menu_moby_spawn(static_cast<uint32_t>(cls));
        sw(outs[k], m);
        if (m == 0) {
            if (k == 2) return 0;
            continue;
        }
        const uint32_t set = lwu(PLAY + 0x1c + k * 8);
        moby_set_anim(m, set, 0);
        moby_anim_set(lwu(outs[k]), set, 0, 10);
        const uint32_t x = lwu(outs[k]);
        sh(x + 0x34, 0);
        lq_sq(x + 0x10, hero + 0x10);
        lq_sq(x + 0x40, hero + 0x40);
        if (lh(x + 0xa6) == 0x4a) swc1(x + 0x2c, fpu_mul(lwc1(x + 0x2c), 3.0f));
        moby_update_transform(lwu(outs[k]));
        sw(lwu(outs[k]) + 0x74, OVF(0x002827b0));
    }
    return 0;
}

namespace {
// orig: (helper) part of 0x00281fb0: the weapon in hand (equipment slot 0)
void hero_panel_weapon(uint32_t w, uint32_t& last) {
    uint32_t m = lwu(w + 0x54);
    const int32_t cur = m != 0 ? lh(m + 0xa6) : -1;
    const uint32_t item = lwu(MS + 0x30);
    const int32_t cls = lw(ITEMS + item * 0x4c + 0x10);
    const bool loaded = cls == lw(MS + 0x120);
    if (cur != cls && loaded) {
        menu_moby_free(m);
        if (lbu(NODE_A + 1) != 0) sub_0024f608(lwu(w + 0x44), NODE_A);
        const uint32_t held = lwu(0x00140408);
        if (held != 0 && item != held) hero_slot_close(0, 0);
        sw(OV(0x00160010), lw(MS + 0x118) == 0 ? 1u : 0u);
        item_class_load(static_cast<uint32_t>(cls), -1);
        const uint32_t b = lwu(OV(0x00160010));
        sw(MS + 0x144, b);
        sw(MS + 0x11c, static_cast<uint32_t>(cls));
        sw(MS + 0x118, b);
        sw(MS + 0x140, static_cast<uint32_t>(cls));
        sb(lwu(CLASSES + static_cast<uint32_t>(lbu(SLOT_OF + static_cast<uint32_t>(cls))) * 4) + 0xd, 0);
        m = menu_moby_spawn(static_cast<uint32_t>(cls));
        if (m != 0) {
            last = item;
            if (lbu(0x0013e520 + last) != 0) sub_00207330(m, lwu(w + 0x44));
            sw(lwu(m + 0x78), w);
            sw(m + 0x74, OVF(0x002827b8));
            sh(m + 0x34, 4);
            if (last == 0x12) {
                sub_0024f590(lwu(w + 0x44), 0, NODE_A);
                sw(NODE_A + 0x28, 0);
                sw(NODE_A + 0x20, 0);
                sw(NODE_A + 0x24, 0);
            }
        }
        sw(w + 0x54, m);
    }
}

// orig: (helper) part of 0x00281fb0: slot 2 (+0x50)
void hero_panel_slot2(uint32_t w, uint32_t& last) {
    uint32_t m = lwu(w + 0x50);
    const int32_t cur = m != 0 ? lh(m + 0xa6) : -1;
    const int32_t cls = lw(ITEMS + lwu(MS + 0x38) * 0x4c + 0x10);
    if (cur != cls) {
        m = static_cast<uint32_t>(menu_moby_free(m));
        if (cls != -1) {
            m = menu_moby_spawn(static_cast<uint32_t>(cls));
            if (m != 0) {
                last = lwu(MS + 0x38);
                sw(lwu(m + 0x78), w);
                sw(m + 0x74, OVF(0x00282970));
                sh(m + 0x34, 4);
            }
        }
        sw(w + 0x50, m);
    }
}

// orig: (helper) part of 0x00281fb0: slot 1 (+0x58 and +0x5c)
void hero_panel_slot1(uint32_t w, uint32_t& last) {
    uint32_t m = lwu(w + 0x58);
    const int32_t cur = m != 0 ? lh(m + 0xa6) : -1;
    const int32_t cls = lw(ITEMS + lwu(MS + 0x34) * 0x4c + 0x10);
    if (cur != cls) {
        m = static_cast<uint32_t>(menu_moby_free(m));
        if (lbu(NODE_B + 1) != 0) sub_0024f608(lwu(w + 0x44), NODE_B);
        if (lbu(NODE_C + 1) != 0) sub_0024f608(lwu(w + 0x44), NODE_C);
        if (cls == -1) {
            sw(w + 0x58, m);
        } else {
            m = menu_moby_spawn(static_cast<uint32_t>(cls));
            if (m != 0) {
                last = lwu(MS + 0x34);
                sw(lwu(m + 0x78), w);
                sw(m + 0x74, OVF(0x00282a60));
                sh(m + 0x34, 4);
                sub_0024f590(lwu(w + 0x44), 0x16, NODE_B);
                sub_0024f590(lwu(w + 0x44), 0x17, NODE_C);
                const float f = u2f(0x3c23d70a);
                swc1(NODE_C + 0x28, f);
                swc1(NODE_B + 0x28, f);
                swc1(NODE_B + 0x20, f);
                swc1(NODE_B + 0x24, f);
                swc1(NODE_C + 0x20, f);
                swc1(NODE_C + 0x24, f);
            }
            sw(w + 0x58, m);
        }
        m = static_cast<uint32_t>(menu_moby_free(lwu(w + 0x5c)));
        if (cls != -1) {
            m = menu_moby_spawn(static_cast<uint32_t>(cls));
            if (m != 0) {
                sw(lwu(m + 0x78), w);
                sw(m + 0x74, OVF(0x00282a60));
                sh(m + 0x34, 4);
            }
        }
        sw(w + 0x5c, m);
    }
}

// orig: (helper) part of 0x00281fb0: slot 3 (+0x4c)
void hero_panel_slot3(uint32_t w) {
    uint32_t m = lwu(w + 0x4c);
    const int32_t cur = m != 0 ? lh(m + 0xa6) : -1;
    const int32_t cls = lw(ITEMS + lwu(MS + 0x3c) * 0x4c + 0x10);
    if (cur != cls) {
        m = static_cast<uint32_t>(menu_moby_free(m));
        if (cls != -1) {
            m = menu_moby_spawn(static_cast<uint32_t>(cls));
            if (m != 0) {
                sw(lwu(m + 0x78), w);
                sw(m + 0x74, OVF(0x00282c08));
                sh(m + 0x34, 4);
                if (lh(m + 0xa6) == 0x25f) {
                    if (lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, 10);
                    sb(m + 0x20, 8);
                }
            }
        }
        sw(w + 0x4c, m);
    }
}
}  // namespace

// The hero panel's update: the mobys of what is equipped (the weapon in hand, slots 0..3, the
// gadgets of items 0x21..0x23, the item 0x18's orbs) replaced when it changes (the weapon's class
// loaded into the hero's item buffers 0x245770); a new weapon queues its animations (the table
// 0x1b93e8, 0x30 per weapon); the panel's timers counted down; the animation (0x283cb8).
// orig: 0x00281fb0
int32_t menu_hero_update(uint32_t w) {
    uint32_t last = 0;
    hero_panel_weapon(w, last);
    hero_panel_slot2(w, last);
    hero_panel_slot1(w, last);
    hero_panel_slot3(w);
    // the gadgets of items 0x23 / 0x21 / 0x22
    slot_moby(w, 0x6c, lbu(0x0013d4c0 + 0x23) != 0 ? 0x197 : -1, OVF(0x00282d38));
    slot_moby(w, 0x70, lbu(0x0013d4c0 + 0x21) != 0 ? 0x266 : -1, OVF(0x00282d38));
    slot_moby(w, 0x74, lbu(0x0013d4c0 + 0x22) != 0 ? 0x26a : -1, OVF(0x00282d38));
    // the item 0x18's orbs (0x141347 of them, 0x141346 already risen)
    for (uint32_t k = 0; k < 8; ++k) {
        const uint32_t off = 0x78 + k * 4;
        uint32_t m = lwu(w + off);
        const int32_t cur = m != 0 ? lh(m + 0xa6) : -1;
        const int32_t cls = static_cast<int32_t>(k) < static_cast<int32_t>(lbu(0x00141347)) ? 0x1df : -1;
        if (cur == cls) continue;
        m = static_cast<uint32_t>(menu_moby_free(m));
        if (cls != -1) {
            m = menu_moby_spawn(0x1df);
            const float rise = static_cast<int32_t>(k) < static_cast<int32_t>(lbu(0x00141346)) ? 0.0f : 3.0f;
            swc1(OV(0x001b9f90) + k * 4, rise);
            sw(OV(0x001b9fa8) + k * 4, 0);
            if (m != 0) {
                sw(lwu(m + 0x78), w);
                sh(m + 0x34, 4);
                sb(m + 0xbc, k);
                sw(m + 0x74, OVF(0x00282dc8));
            }
        }
        sw(w + off, m);
    }
    // a new weapon: its animations queued
    if (last != lwu(MS + 0x1c) && static_cast<int32_t>(last) > 0 && static_cast<int32_t>(last) < 0x24) {
        sw(MS + 0x1c, last);
        hero_anim_clear();
        const uint32_t e = OV(0x001b93e8) + last * 0x30;
        const uint32_t base = lwu(MS + 0xcc);
        hero_anim_queue(lwu(e + 8) + base, 0, lwu(e + 0xc), last, lwu(e + 0x10), lwu(e + 0x18), lwu(e + 0x1c), lwu(e + 0x20), lwu(e + 0x24),
                        lwu(e + 0x28), lwu(e + 0x2c), lwu(e), lwu(e + 4));
        if (lwu(e + 0xc) != 0)
            hero_anim_queue(lwu(e + 0x14) + lwu(MS + 0xcc), 2, 0, last, 1, 0xffffffffu, 0, 0xffffffffu, 0, 0xffffffffu, 0, 0, 0);
    }
    for (uint32_t p = w + 0xa4; p < w + 0xbc; ++p)
        if (lbu(p) != 0) sb(p, lbu(p) - 1u);
    hero_anim_update(lwu(w + 0x44), lwu(w + 0x54), lwu(w + 0x50), w + 0x60, w + 0x64, w + 0x68);
    return 0;
}

}  // namespace rac1
