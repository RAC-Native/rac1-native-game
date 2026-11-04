// The shrub pass's culling and chain building (0x286f10), written by hand from the routine: the same arithmetic, the same memory
// written in the same layout, the VU0 registers as a local copy (vu0_local.h) written back at the end. The transliteration
// (world_shrub_cull_tr, world_cull.cpp) stays the reference: RAC1_WORLD_CULL_FAST (world_cull_fast.cpp) picks one or checks one
// against the other.
//
// What the routine does. The shrub classes (*(0x16048c) of them, queued at 0x1bc030; a class record in RAM: its flags at +0x14,
// its instances' bounds at *(+0x18), *(+0x16) of them, its fade data at *(+0x1c), its LOD packets (8-byte entries: address, size)
// at +0x40, *(+0x28) of them) and their instances (the instance table *(0x16049c), 0x40 bytes each: a matrix):
//  1. class by class, the bounds come into the scratchpad (0x1000) 128 at a time and are tested as the ties' (no visibility
//     group): an instance outside is dropped; one on a frustum plane is queued for clipping (SPR 0x3d00); one inside has its
//     matrix (swayed when the class asks) rotated into the view and written into the packet, and is queued, by its fade (its
//     depth against the class's fade distances, when it has fade data), on the opaque list (SPR 0x2c00, per class), the faded
//     list (SPR 0x3700, with its colour) or the billboards' list (SPR 0x2000: index and fade). Each class's opaque instances are
//     then chained, LOD packet by LOD packet, 5 instances a group;
//  2. the faded instances, chained the same way;
//  3. the billboards (a second program): the fully faded ones first (fade bit 0x80), then the others, each a camera-facing quad
//     of 4 corners (SPR 0x3800) projected here;
//  4. the instances to clip (a third program): their matrices by the clip matrix, then chained.
// At the end the classes' and the three programs' reserved tags are made NEXT tags. Along the way: the relight list (SPR 0x3200),
// the classes' nearest depths and counts (SPR 0x3000 / 0x3100 / 0x3600 / 0x3680 / 0x3c80). The packet buffer is SPR 0x0000,
// double buffered by 0x800.
//
// As in the tie cull, some of the original's registers carry their upper 64 bits from one use to the next and into the VU0
// registers (QMTC2 moves all 128): they are kept here as such (`hi` below).
// orig: (host)
#include <game/ovl/world_cull.h>
#include <game/ovl/world_cull_hand.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>

namespace rac1::world_hand {

namespace {

constexpr uint32_t SPR = 0x70000000u;
// the scratchpad's layout during the pass
constexpr uint32_t PACKETS = SPR + 0x0000;      // the packet buffer: two halves of 0x800
constexpr uint32_t BOUNDS = SPR + 0x1000;       // the instances' bounds (128 of 0x20)
constexpr uint32_t BILLBOARDS = SPR + 0x2000;   // the billboards: (index, fade) halfword pairs, to 0x2c00
constexpr uint32_t OPAQUE = SPR + 0x2c00;       // the class's opaque instances (index << 20 | packet >> 4), to 0x3000
constexpr uint32_t CLASS_TAGS = SPR + 0x3000;   // per class: its reserved tag's patch (0x3000), its nearest depth (0x3100)
constexpr uint32_t RELIGHT = SPR + 0x3200;      // the relight list (halfword indices), to 0x35fe
constexpr uint32_t COUNTS_BB = SPR + 0x3600, COUNTS_FADED = SPR + 0x3680, COUNTS_CLIP = SPR + 0x3c80;  // per class (halfwords)
constexpr uint32_t FADED = SPR + 0x3700;        // the faded instances (as OPAQUE's, then colours in the packet), to 0x3b00
constexpr uint32_t CORNERS = SPR + 0x3800;      // phase 3: the billboard's corners (0x80 bytes from 0x1c2cb0)
constexpr uint32_t SINES = SPR + 0x3b00;        // the sway's sine table (0x100 signed bytes from 0x1c3ef0)
constexpr uint32_t WIND = SPR + 0x3c00;         // the sway's parameters (2 quadwords from 0x1c3ff0) and its clock (+0x20)
constexpr uint32_t CLIP_QUEUE = SPR + 0x3d00;   // the instances to clip (their bounds' RAM addresses), to 0x3f00
constexpr uint32_t TEMPLATES = SPR + 0x3f00;    // the chain's templates (6 quadwords from 0x160b20)
constexpr uint32_t CLASSES = SPR + 0x3f60, CLASS_NEXT = SPR + 0x3f64, OPAQUE_END = SPR + 0x3f68, BB_END = SPR + 0x3f6c;
constexpr uint32_t FADED_END = SPR + 0x3f70, CLIP_END = SPR + 0x3f74, BATCH_SRC = SPR + 0x3f78;
constexpr uint32_t TOGGLE_A = SPR + 0x3f7c, TOGGLE_B = SPR + 0x3f80, TOGGLE_C = SPR + 0x3f84;  // the chain's alternating fields
constexpr uint32_t COLOURS = SPR + 0x3f88, RELIGHT_END = SPR + 0x3f8c;
constexpr uint32_t TAG_OPAQUE = SPR + 0x3f90, TAG_BB = SPR + 0x3f94, TAG_CLIP = SPR + 0x3f98;  // the programs' reserved tags
constexpr uint32_t SWAY = SPR + 0x3f9c, RESUME = SPR + 0x3fa0, LOD_LIMIT = SPR + 0x3fa4;

enum : int {
    CAM = 24,                            // the camera's position (x 1024 in w)
    VIEW0 = 25, VIEW1 = 26, VIEW2 = 27,  // the view rotation's rows
    WORLD0 = 28,                         // the view matrix (rows 28 .. 31)
    NEAREST = 17,                        // the class's nearest depth
};

inline uint32_t lo32(uint64_t v) { return static_cast<uint32_t>(v); }
inline uint32_t hi32(uint64_t v) { return static_cast<uint32_t>(v >> 32); }
inline uint32_t sign_word(uint32_t w) { return (w & 0x80000000u) != 0 ? 0xffffffffu : 0u; }
inline int32_t s32(uint32_t v) { return static_cast<int32_t>(v); }
inline uint64_t min_words(uint64_t a, uint64_t b) {  // pminw on two doublewords (signed words)
    const uint32_t l = static_cast<uint32_t>(std::min(s32(lo32(a)), s32(lo32(b))));
    const uint32_t h = static_cast<uint32_t>(std::min(s32(hi32(a)), s32(hi32(b))));
    return l | static_cast<uint64_t>(h) << 32;
}

struct Pass {
    vu0l::Vu u;
    Spr spr;
    uint32_t pkt = 0, sp = 0;
    Qw tpl[6];  // TEMPLATES (t2 .. t7)
    // the original's registers' upper halves that reach the VU0 (at, v0, v1, a0; t7's for the test's pminw)
    uint64_t at_hi = 0, v0_hi = 0, v1_hi = 0, a0_hi = 0, t7_hi = 0;
    // the class
    uint32_t cls = 0, inst = 0;  // its record, the instance's matrix (RAM, 0x40 each)
    uint32_t class_index = 0;
    uint32_t n_bb = 0, n_clip = 0, n_faded = 0;
    uint32_t fade_data = 0;

    uint64_t vf_hi(int r) const { return u.word(r, 2) | static_cast<uint64_t>(u.word(r, 3)) << 32; }
    uint64_t vf_lo(int r) const { return u.word(r, 0) | static_cast<uint64_t>(u.word(r, 1)) << 32; }

    void flush() { sp = flush_packets(sp, pkt, sp & 0x7ffu, 0x800u); }
    void flush_if_full() {
        if ((sp & 0x600u) == 0x600u) flush();
    }
    // orig: (helper) a fixed part copied from RAM into the packet buffer; the first three quadwords' upper halves go to v0, v1, a0
    void copy_packet(uint32_t src, uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) {
            const Qw q = ram_q(src + 16 * i);
            spr.setq(sp + 16 * i, q);
            if (i == 0) v0_hi = q.hi;
            if (i == 1) v1_hi = q.hi;
            if (i == 2) a0_hi = q.hi;
        }
        sp += 16 * n;
        pkt += 16 * n;
    }
    // orig: (helper) a reserved quadword (a tag made later), its packet address kept at `slot`
    void reserve_tag(uint32_t slot) {
        spr.set32(slot, pkt);
        pkt += 0x10;
        spr.setq(sp, Qw{});
        sp += 0x10;
    }
    void end_tag() {
        spr.setq(sp, Qw{});
        spr.set32(sp, 0x10000000u);
        spr.set32(sp + 8, 0x13000000u);
        sp += 0x10;
        pkt += 0x10;
    }
    void load_templates() {
        for (int i = 0; i < 6; ++i) tpl[i] = spr.q(TEMPLATES + 0x10 * static_cast<uint32_t>(i));
        t7_hi = tpl[5].hi;
    }

    void setup();
    void classes();
    void records(uint32_t end);
    void inside(uint32_t index, uint32_t faded_flag);
    void matrix_into_view();
    void sway(uint32_t flag, uint32_t matrix);
    void chain(uint32_t list, uint32_t count, bool faded_layout);
    void billboards();
    void clipped();
    void finish();
};

// orig: 0x00286f10 (its start)
void Pass::setup() {
    for (uint32_t a = 0x3fa8; a < 0x4000; a += 8) spr.set64(SPR + a, 0);  // (the s registers' save: zeros here, the port's)
    inst = lwu(OV(0x0016049cu));
    spr.set32(CLASSES, lwu(OV(0x0016048cu)));
    spr.set32(CLASS_NEXT, OV(0x001bc030u));
    spr.set32(LOD_LIMIT, lwu(OV(0x001604a4u)));
    spr.set32(COLOURS, lwu(OV(0x001604a0u)));
    spr.set32(FADED_END, FADED);
    spr.set32(CLIP_END, CLIP_QUEUE);
    spr.set32(BB_END, BILLBOARDS);
    spr.set32(RELIGHT_END, RELIGHT);
    inst -= 0x40;
    for (uint32_t i = 0; i < 0x100; i += 0x10) spr.setq(SINES + i, ram_q(OV(0x001c3ef0u) + i));
    const Qw wind0 = ram_q(OV(0x001c3ff0u));
    spr.setq(WIND, wind0);
    spr.setq(WIND + 0x10, ram_q(OV(0x001c3ff0u) + 0x10));
    spr.set32(WIND + 0x20, lwu(OV(0x0015f3f8u)));
    spr.set32(RESUME, OVF(0x0028750cu));
    spr.set32(TOGGLE_A, 2);
    spr.set32(TOGGLE_B, 0xee);
    spr.set32(TOGGLE_C, 0x11);
    pkt = lwu(OV(0x001611c0u));
    sp = PACKETS;
    for (uint32_t a = BOUNDS; a < BILLBOARDS; a += 0x10) spr.setq(a, Qw{});

    const uint32_t view = OV(0x00166c80u);
    u.set_words(1, 0x44800000u, 0, lo32(wind0.hi), hi32(wind0.hi));  // (1024.0; v0's upper half: the wind's first quadword)
    ram_load_vf(u, CAM, view + 0x140);
    ram_load_vf(u, VIEW0, view + 0x0);
    ram_load_vf(u, VIEW1, view + 0x10);
    ram_load_vf(u, VIEW2, view + 0x20);
    for (int i = 0; i < 4; ++i) ram_load_vf(u, WORLD0 + i, view + 0x80 + 0x10 * static_cast<uint32_t>(i));
    u.mulbc(CAM, 0, u.lane(1, 0), vu0l::W);
    const uint32_t frustum = OV(0x0016ca40u);
    ram_load_vf(u, 18, frustum + 0x220);
    ram_load_vf(u, 19, frustum + 0xa0);
    ram_load_vf(u, 20, frustum + 0xb0);
    ram_load_vf(u, 21, frustum + 0x1d0);
    ram_load_vf(u, 22, frustum + 0x1e0);
    ram_load_vf(u, 23, frustum + 0x1f0);
    u.set_words(1, 0x3a800000u, 0, 0, 0);  // (1 / 1024)
    u.mulbc(19, 19, u.lane(1, 0), vu0l::XY);
    u.mulbc(23, 23, u.lane(1, 0), vu0l::XY);
    for (uint32_t i = 0; i < 6; ++i) {
        const Qw q = ram_q(OV(0x00160b20u) + 0x10 * i);
        if (i == 0) at_hi = q.hi;
        spr.setq(TEMPLATES + 0x10 * i, q);
    }
    // the opaque program's start (0x101750) and setup, a reserved tag, the first part
    spr.setq(sp, Qw{});
    const uint32_t addr = lwu(0x00101750u);
    spr.set32(sp + 4, 0x00101760u);
    spr.set32(sp, 0x30000000u);
    pkt += 0x10;
    spr.set16(sp, addr);
    sp += 0x10;
    copy_packet(OV(0x001c2ab0u), 5);
    reserve_tag(TAG_OPAQUE);
    copy_packet(OV(0x001c2b80u), 3);
}

// orig: (helper) 0x287618 .. 0x287794 -- the shrubs' sway (as the ties', 0x296a4c: the matrix vf1 .. vf3 sheared by two sines of
// the clock and the instance's address, within the wind's reach; `flag` bit 1 the stronger one), the scratchpad's WIND / SINES
void Pass::sway(uint32_t flag, uint32_t matrix) {
    spr.load_vf(u, 9, WIND + 0x10);
    spr.load_vf(u, 8, WIND);
    const uint32_t clock = spr.u32(WIND + 0x20);
    const uint32_t strong = flag >> 1;
    u.addbc(11, 0, u.lane(0, 3), vu0l::X);
    u.mul(12, 4, 4, vu0l::XYZ);
    u.addabc(12, u.lane(12, 1), vu0l::X);
    u.maddbc(12, 11, u.lane(12, 2), vu0l::X);
    u.subbc_flags(9, u.lane(12, 0), vu0l::Y);
    const bool beyond = (u.mac & 0x40u) != 0;
    u.mulbc(12, 12, u.lane(9, 2), vu0l::X);
    u.subbc(12, 0, u.lane(12, 0), vu0l::W);
    if (beyond) return;
    u.mulbc(5, 0, u.lane(0, 0), vu0l::XYZW);
    u.mulbc(6, 0, u.lane(0, 0), vu0l::XYZW);
    u.mr32(7, 0, vu0l::XYZW);
    u.addbc(5, 5, u.lane(0, 3), vu0l::X);
    u.addbc(6, 6, u.lane(0, 3), vu0l::Y);
    const uint32_t p0 = matrix * 67 + clock;                           // (m << 6) + (m << 1) + m
    const uint32_t p1 = matrix * 123 + clock + (strong != 0 ? clock : 0);  // (m << 7) - (m << 2) - m
    const auto sine = [&](int r, uint32_t i) {
        const uint32_t w = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(spr.u8(SINES + (i & 0xffu)))) << 5);
        u.set_words(r, w, sign_word(w), lo32(a0_hi), hi32(a0_hi));
        u.itof(r, r, vu0l::XYZW, 1.0f / 4096.0f);
    };
    sine(10, p0);
    sine(11, p0 >> 1);
    u.mul(13, 10, 11, vu0l::X);
    u.subbc(13, 0, u.lane(9, 0), vu0l::W);
    u.mulbc(13, 13, u.lane(9, 0), vu0l::X);
    u.addbc(13, 13, u.lane(13, 3), vu0l::X);
    sine(10, p1 + 0x40);
    sine(11, p1 >> 1);
    u.mulbc(13, 13, u.lane(8, 0), vu0l::X);
    u.mulabc(10, u.lane(8, 1), vu0l::X);
    u.maddbc(10, 13, u.lane(8, 2), vu0l::X);
    u.mulbc(10, 10, u.lane(12, 3), vu0l::X);
    u.mulabc(11, u.lane(8, 1), vu0l::X);
    u.maddbc(11, 13, u.lane(8, 3), vu0l::X);
    u.mulbc(11, 11, u.lane(12, 3), vu0l::X);
    if (strong == 0) {
        u.mulbc(10, 10, u.lane(9, 3), vu0l::X);
        u.mulbc(11, 11, u.lane(9, 3), vu0l::X);
    }
    u.addbc(7, 0, u.lane(10, 0), vu0l::X);
    u.addbc(7, 0, u.lane(11, 0), vu0l::Y);
    u.rotate(1, 5, 6, 7, 1, vu0l::XYZ);
    u.rotate(2, 5, 6, 7, 2, vu0l::XYZ);
    u.rotate(3, 5, 6, 7, 3, vu0l::XYZ);
}

// orig: (helper) 0x28750c .. 0x287564 / 0x288290 .. 0x2882e8 -- the matrix vf1 .. vf4 (rows scaled, translation relative to the
// camera) into the view: the rows by the view's rotation (+ vf31 x vf0.x: 0, the routine's form), the translation by the whole
void Pass::matrix_into_view() {
    u.transform(1, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 1, u.lane(0, 0));
    u.mulbc(4, 4, u.lane(CAM, 3), vu0l::XYZ);
    u.transform(2, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 2, u.lane(0, 0));
    u.transform(3, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 3, u.lane(0, 0));
    u.transform(4, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 4, u.lane(0, 3));
}

// orig: (helper) 0x287364 .. 0x2875c4 -- an instance inside the view (table index `index`): its fade (from its
// depth, vf3 / vf5 as 4096ths, against the class's fade data, `faded_flag` its record's +0x17) - a billboard (queued, nothing
// drawn here), opaque or faded (queued with its packet address) -, then its matrix into the packet
void Pass::inside(uint32_t index, uint32_t faded_flag) {
    // (v1 / v0 / a2 below: the original's doublewords, as it computes them)
    uint64_t v1 = vf_lo(3);
    v1_hi = vf_hi(3);
    if (fade_data != 0) {
        uint64_t a2 = 0x8000;
        bool queue_bb = true;
        v1 = 0x8000ull << 32;
        if (faded_flag != 0) {
            const uint64_t v0 = vf_lo(5);
            v0_hi = vf_hi(5);
            const uint64_t at = static_cast<uint64_t>(faded_flag) << 44;
            v1 = v1 + at;
            a2 = v0 - at;
            v1 = v1 - v0;
            const bool neg = static_cast<int64_t>(a2) < 0;
            a2 = static_cast<uint64_t>(static_cast<int64_t>(a2) >> 32);
            if (neg) queue_bb = false;
            else a2 = static_cast<uint32_t>(std::min(s32(lo32(a2)), 0x8000));
        }
        if (queue_bb) {  // 0x2873dc: on the billboards' list
            const uint32_t fade = lo32(a2) >> 8;
            const uint32_t at = spr.u32(BB_END);
            if (fade == 0x80) {  // (fully faded: the billboard's fade from vf3.y / 2, nothing drawn here)
                v1 = vf_lo(3);
                v1_hi = vf_hi(3);
                if (at == OPAQUE) return;
                v1 = static_cast<uint64_t>(static_cast<int64_t>(v1) >> 33);
                n_bb += 1;
                const uint32_t w = static_cast<uint32_t>(std::min(s32(lo32(v1)), 0x8000));
                v1_hi = min_words(v1_hi, v0_hi);
                spr.set16(at + 0x0, index);
                spr.set16(at + 0x2, w >> 8);
                spr.set32(BB_END, at + 4);
                return;
            }
            if (at == OPAQUE) return;  // 0x287424
            spr.set16(at + 0x2, fade);
            n_bb += 1;
            spr.set16(at + 0x0, index);
            spr.set32(BB_END, at + 4);
        }
    }
    // 0x28744c: opaque or faded
    u.mini_lane(NEAREST, 0, u.lane(2, 0), u.lane(NEAREST, 0));
    uint32_t entry = pkt >> 4;
    v1 = static_cast<uint64_t>(static_cast<int64_t>(v1) >> 32);
    spr.setq(sp + 0x40, Qw{});
    const uint32_t w0 = static_cast<uint32_t>(std::min(s32(lo32(v1)), 0x8000));
    const uint32_t w1 = static_cast<uint32_t>(std::min(s32(hi32(v1)), 0));
    v1 = w0 | static_cast<uint64_t>(w1) << 32;
    v1_hi = min_words(v1_hi, v0_hi);
    const uint32_t idx = index << 20;
    if (v1 == 0x8000) {  // opaque
        const uint32_t at = spr.u32(OPAQUE_END);
        entry |= idx;
        if (at == CLASS_TAGS) return;
        spr.set32(at, entry);
        spr.set32(OPAQUE_END, at + 4);
    } else {  // 0x28748c: faded, its colour (the instance's word at COLOURS + index x 0x60, its bytes as words) and fade
        entry |= idx;
        const uint32_t colour = lwu(spr.u32(COLOURS) + index * 0x60);
        const uint32_t fade = w0 >> 8;
        const uint32_t at = spr.u32(FADED_END);
        spr.setq(sp + 0x40, Qw{(colour & 0xffu) | static_cast<uint64_t>((colour >> 8) & 0xffu) << 32,
                               ((colour >> 16) & 0xffu) | static_cast<uint64_t>(colour >> 24) << 32});
        spr.set32(sp + 0x4c, fade);
        if (at == SINES) return;
        spr.set32(at, entry);
        spr.set32(FADED_END, at + 4);
        n_faded += 1;
    }
    // 0x2874dc: the matrix
    ram_load_vf(u, 1, inst + 0x0);
    ram_load_vf(u, 2, inst + 0x10);
    ram_load_vf(u, 3, inst + 0x20);
    u.mulbc(1, 1, u.lane(4, 3), vu0l::XYZ);
    u.mulbc(2, 2, u.lane(4, 3), vu0l::XYZ);
    const uint32_t flag = spr.u32(SWAY);
    u.mulbc(3, 3, u.lane(4, 3), vu0l::XYZ);
    sp += 0x50;
    u.sub(4, 4, CAM, vu0l::XYZ);
    if (flag != 0) sway(flag, inst);  // (it returns through RESUME: 0x28750c)
    matrix_into_view();
    pkt += 0x50;
    spr.store_vf(u, 1, sp - 0x50);
    spr.store_vf(u, 2, sp - 0x40);
    spr.store_vf(u, 3, sp - 0x30);
    spr.store_vf(u, 4, sp - 0x20);
    flush_if_full();
}

// orig: (helper) 0x287234 .. 0x287614 -- a batch of bounds tested (as the ties': 0x2962d4)
void Pass::records(uint32_t end) {
    const uint32_t limit = spr.u32(LOD_LIMIT);
    for (uint32_t r = BOUNDS - 0x20;;) {
        const uint32_t lod_word = spr.u32(r + 0x30);
        r += 0x20;
        spr.load_vf(u, 1, r);
        if (r == end) return;
        const uint32_t lod = static_cast<uint32_t>(std::min(s32(lod_word), s32(limit)));
        v0_hi = min_words(v0_hi, t7_hi);
        u.sub(2, 1, CAM, vu0l::XYZ);
        u.addbc(3, 0, u.lane(1, 3), vu0l::X);
        u.set_words(4, 0, lod, lo32(v0_hi), hi32(v0_hi));
        u.subbc(3, 0, u.lane(1, 3), vu0l::Y);
        if (lod == 0) {  // (0x287600: the record not arrived - the DMA's wait: the same record again)
            r -= 0x20;
            continue;
        }
        u.rotate(2, VIEW0, VIEW1, VIEW2, 2, vu0l::XYZ);
        inst += 0x40;
        spr.set32(r + 0x10, 0);
        u.subbc(19, 4, u.lane(1, 3), vu0l::Y);
        u.mulbc(8, 22, u.lane(1, 3), vu0l::XY);
        u.addbc(1, 3, u.lane(2, 2), vu0l::XY);
        u.mulbc(5, 20, u.lane(2, 2), vu0l::XY);
        u.abs(2, 2, vu0l::XY);
        u.mulbc(7, 8, u.lane(22, 2), vu0l::XY);
        u.sub(3, 19, 1, vu0l::XY);
        u.mul(9, 5, 21, vu0l::XY);
        u.sub(6, 2, 8, vu0l::XY);
        u.add(8, 2, 7, vu0l::XY);
        u.sub(4, 1, 23, vu0l::XY);
        u.max_lane(1, 2, u.lane(0, 2), u.lane(2, 2));
        at_hi = vf_hi(3);
        u.sub(7, 5, 6, vu0l::XY);
        u.sub(8, 9, 8, vu0l::XY);
        a0_hi = vf_hi(4);
        if (u.negative(3, 1)) continue;  // beyond its LOD's distance
        u.mr32(1, 1, vu0l::XY);
        if (!u.negative(3, 0)) continue;  // behind the camera
        u.ftoi(3, 3, vu0l::XY, 4096.0f);
        const uint32_t index = spr.u16(r + 0x18);
        v0_hi = vf_hi(7), v1_hi = vf_hi(8);
        if (u.negative(7, 1) || u.negative(7, 0)) continue;  // beside the view
        // (the frustum planes' test: vf8 and the vf4 before its reload below)
        const bool plane_a = u.negative(8, 1) || u.negative(4, 1), plane_b = u.negative(8, 0) || !u.negative(4, 0);
        note_shrub(index);
        u.mulbc(2, 1, u.lane(CAM, 3), vu0l::X);
        ram_load_vf(u, 4, inst + 0x30);
        if (spr.u8(r + 0x1b) != 0 || spr.u16(r + 0x1e) != 0xffffu) {  // with lights: relit
            const uint32_t at = spr.u32(RELIGHT_END);
            if (at != SPR + 0x35fe) {
                spr.set16(at, index);
                spr.set32(RELIGHT_END, at + 2);
            }
        }
        bool on_plane = plane_a;
        if (!on_plane) {
            u.ftoi(5, 1, vu0l::Y, 4096.0f);
            on_plane = plane_b;
        }
        if (on_plane) {  // 0x2875c8: queued for clipping (its bounds' RAM address)
            const uint32_t at = spr.u32(CLIP_END);
            u.mini_lane(NEAREST, 0, u.lane(2, 0), u.lane(NEAREST, 0));
            const uint32_t src = spr.u32(BATCH_SRC);
            if (at == TEMPLATES) continue;
            n_clip += 1;
            spr.set32(at, src + (r & 0xfffu));
            spr.set32(CLIP_END, at + 4);
            continue;
        }
        inside(index, fade_data != 0 ? spr.u8(r + 0x17) : 0);
    }
}

// orig: (helper) 0x287848 .. 0x2879a8 (and 0x287aa0 / 0x28836c) -- a class's listed instances (`count` entries at `list`: index
// << 20 | packet address >> 4) chained, for each of its LOD packets: the packet's REF and unpack, then groups of up to 5 instances
// (a REF of the instance's matrix: its colours' address, and its colours' unpack, the two quadwords in the order `faded_layout`
// says), each group ended by the template's MSCAL; the alternating fields (TOGGLE_A .. C) flipped
void Pass::chain(uint32_t list, uint32_t count, bool faded_layout) {
    uint32_t a3 = spr.u32(TOGGLE_A), t0 = spr.u32(TOGGLE_B), t1 = spr.u32(TOGGLE_C);
    const uint32_t lods = cls + 0x40, end = lods + (static_cast<uint32_t>(lhu(cls + 0x28)) << 3);
    for (uint32_t p = lods; p != end;) {
        const uint32_t addr = lwu(p), size = lwu(p + 4);
        p += 8;
        spr.setq(sp, tpl[0]);
        spr.set32(sp + 4, addr);
        spr.set16(sp, size >> 4);
        pkt += 0x10;
        spr.set16(sp + 8, a3);
        sp += 0x10;
        spr.setq(sp, tpl[1]);
        pkt += 0x10;
        spr.set16(sp + 0xc, t1);
        sp += 0x10;
        uint32_t entry = list, left = count;
        for (;;) {
            const uint32_t n = static_cast<uint32_t>(std::min(s32(left), 5));
            a0_hi = min_words(0, at_hi);  // (pminw s7, at: their upper words)
            if (left == 0) break;
            uint32_t a1 = t0 + 1;
            spr.setq(sp, tpl[2]);
            left -= n;
            spr.set16(sp + 8, t0);
            pkt += 0x10;
            spr.set32(sp + 0xc, n);
            sp += 0x10;
            for (uint32_t k = 0; k < n; ++k) {
                const uint32_t e = spr.u32(entry);
                entry += 4;
                const uint32_t matrix_pkt = (e << 12) >> 8, index = e >> 20;
                const uint64_t colours = (static_cast<uint64_t>(index) << 37) + (static_cast<uint64_t>(index) << 38) + tpl[5].lo;
                const uint32_t addr1 = a1 + 4;
                const uint32_t ref = faded_layout ? 0x10u : 0x0u, unpack = faded_layout ? 0x0u : 0x10u;
                spr.setq(sp + ref, tpl[3]);
                spr.set16(sp + ref + 0xc, a1);
                spr.set32(sp + ref + 4, matrix_pkt);
                a1 += 0x1c;
                spr.setq(sp + unpack, tpl[5]);
                spr.set64(sp + unpack, colours);
                spr.set16(sp + unpack + 0xc, addr1 | 0x4000);
                pkt += 0x20;
                sp += 0x20;
            }
            spr.setq(sp, tpl[4]);
            sp += 0x10;
            t0 = 0x269 - t0;
            t1 = 0x26 - t1;
            pkt += 0x10;
            flush_if_full();
        }
        a3 = 0x7a - a3;
    }
    spr.set32(TOGGLE_A, a3);
    spr.set32(TOGGLE_B, t0);
    spr.set32(TOGGLE_C, t1);
}

// orig: (helper) 0x287174 .. 0x287a00 -- the classes: their instances tested, drawn or queued, the opaque ones chained
void Pass::classes() {
    for (;;) {
        const uint32_t left = spr.u32(CLASSES), next = spr.u32(CLASS_NEXT);
        if (left == class_index) return;
        cls = lwu(next);
        spr.set32(CLASS_NEXT, next + 4);
        u.set_words(NEAREST, 0x50000000u, 0, lo32(v1_hi), hi32(v1_hi));  // (QMTC2 of v1: its upper half the last one's)
        n_bb = n_clip = n_faded = 0;
        const uint32_t flags = lhu(cls + 0x14);
        uint32_t src = lwu(cls + 0x18), n = lhu(cls + 0x16);
        sp += 0x10;  // (the class's tag, made at the end: a NEXT over its part of the chain)
        pkt += 0x10;
        const uint32_t start = pkt;
        bool drawn = false;
        if ((flags & 1) != 0) {  // not drawn: its instances skipped
            inst += n << 6;
        } else {
            fade_data = lwu(cls + 0x1c);
            spr.set32(SWAY, (flags & 6) >> 1);
            spr.set32(OPAQUE_END, OPAQUE);
            while (n != 0) {  // 0x2871e4: 128 at a time
                const uint32_t k = static_cast<uint32_t>(std::min(0x80, s32(n)));
                dma_to_spr(0x1000, src, k << 1);
                n -= k;
                spr.set32(BATCH_SRC, src);
                src += k << 5;
                records(BOUNDS + (k << 5));
            }
            // 0x287798
            a0_hi = 0;  // (PCPYUD a0, s3, zero)
            u.ftoi(NEAREST, NEAREST, vu0l::X, 1.0f);
            drawn = start != pkt;
        }
        const uint32_t i2 = SPR + class_index * 2, i4 = SPR + class_index * 4;
        if (!drawn) {  // 0x2879b4: nothing of it in the chain: its tag given back
            sp -= 0x10;
            pkt -= 0x10;
            sh(cls + 0x38, n_bb);
            sh(cls + 0x36, n_clip);
            sh(cls + 0x34, 0);
            spr.set16(i2 + 0x3600, n_bb);
            spr.set16(i2 + 0x3c80, n_clip);
            spr.set16(i2 + 0x3680, n_faded);
            ++class_index;
            spr.set32(i4 + 0x3000, 0);
            v0_hi = vf_hi(NEAREST);
            spr.set32(i4 + 0x3100, u.word(NEAREST, 0));
            continue;
        }
        const uint32_t tag = ((pkt - start) << 16) + (start >> 4);
        const uint32_t n_opaque = (spr.u32(OPAQUE_END) - OPAQUE) >> 2;
        spr.set16(i2 + 0x3600, n_bb);
        spr.set16(i2 + 0x3c80, n_clip);
        spr.set16(i2 + 0x3680, n_faded);
        ++class_index;
        v0_hi = vf_hi(NEAREST);
        spr.set32(i4 + 0x3000, tag);
        spr.set32(i4 + 0x3100, u.word(NEAREST, 0));
        sh(cls + 0x38, n_bb);
        sh(cls + 0x36, n_clip);
        sh(cls + 0x34, n_faded + n_opaque);
        if (n_opaque == 0) continue;
        load_templates();
        chain(OPAQUE, n_opaque, false);
    }
}

// orig: (helper) 0x287c04 .. 0x28800c -- the billboards: a second program; the fully faded ones (fade bit 0x80) then the others
// (bits 0x7f), each instance a quad facing the camera (the class's corners at its fade data, scaled; SPR CORNERS), projected here
void Pass::billboards() {
    end_tag();
    flush();
    reserve_tag(TAG_BB);
    copy_packet(OV(0x001c2c10u), 4);
    const uint32_t view = OV(0x00166c80u);
    for (int i = 0; i < 4; ++i) ram_load_vf(u, WORLD0 + i, view + 0x40 + 0x10 * static_cast<uint32_t>(i));
    const uint32_t frustum = OV(0x0016ca40u);
    ram_load_vf(u, 21, frustum + 0x220);
    ram_load_vf(u, 22, frustum + 0x210);
    ram_load_vf(u, 23, frustum + 0x1a0);
    const uint32_t table = lwu(OV(0x0016049cu));
    for (uint32_t i = 0; i < 0x80; i += 0x10) spr.setq(CORNERS + i, ram_q(OV(0x001c2cb0u) + i));
    Qw t[4];
    for (uint32_t i = 0; i < 4; ++i) t[i] = ram_q(OV(0x00160b80u) + 0x10 * i);
    for (uint32_t mask : {0x80u, 0x7fu}) {
        if (mask == 0x7f) copy_packet(OV(0x001c2be0u), 3);
        uint32_t e = BILLBOARDS;
        for (uint32_t c = 0; c < spr.u32(CLASSES); ++c) {
            uint32_t n = spr.u16(COUNTS_BB + 2 * c);
            if (n == 0) continue;
            const uint32_t klass = lwu(OV(0x001bc030u) + 4 * c);
            uint32_t data = lwu(klass + 0x1c);
            ram_load_vf(u, 16, data);
            while (n != 0) {
                const uint32_t index = spr.u16(e), fade = spr.u16(e + 2);
                e += 4;
                --n;
                if ((fade & mask) == 0) continue;
                const uint32_t m = (index << 6) + table;
                const uint32_t colour = lwu(m + 0x1c), size = lwu(m + 0x2c);
                if (data != 0) {  // the class's quad part, once
                    spr.setq(sp + 0x00, t[0]);
                    spr.setq(sp + 0x10, t[1]);
                    spr.setq(sp + 0x20, ram_q(data + 0x10));
                    spr.setq(sp + 0x30, ram_q(data + 0x20));
                    spr.setq(sp + 0x40, ram_q(data + 0x30));
                    data = 0;
                    sp += 0x50;
                    pkt += 0x50;
                }
                ram_load_vf(u, 1, m + 0x30);
                // the colour's bytes (the fade in its top byte) as words; the size's halfwords as words (QMTC2)
                const uint32_t c32 = colour | (fade << 24);
                const Qw cw{(c32 & 0xffu) | static_cast<uint64_t>((c32 >> 8) & 0xffu) << 32, ((c32 >> 16) & 0xffu) | static_cast<uint64_t>(c32 >> 24) << 32};
                const uint32_t size_hi = (size & 0x80000000u) != 0 ? 0xffffu : 0u;
                spr.setq(sp, t[2]);
                spr.setq(sp + 0x10, t[3]);
                u.set_words(3, size & 0xffffu, size >> 16, size_hi, size_hi);
                u.sub(1, 1, CAM, vu0l::XYZ);
                sp += 0x20;
                u.itof(3, 3, vu0l::XY, 1.0f / 4096.0f);
                pkt += 0x20;
                u.addbc(4, 0, u.lane(0, 3), vu0l::XYZ);
                u.mul(2, 1, 1, vu0l::XYZ);
                u.addabc(2, u.lane(2, 1), vu0l::X);
                u.maddbc(2, 4, u.lane(2, 2), vu0l::X);
                u.q = vu0m::vrsqrt(u.lane(0, 3), u.lane(2, 0));
                u.mulbc(15, 16, u.lane(3, 0), vu0l::Y);
                u.mulbc(4, 0, u.lane(0, 0), vu0l::XYZW);
                u.mulbc(5, 0, u.lane(0, 0), vu0l::XYZW);
                u.mr32(6, 0, vu0l::XYZW);
                u.vf[7] = u.vf[0];
                u.addbc(4, 4, u.lane(0, 3), vu0l::X);
                u.addbc(5, 5, u.lane(0, 3), vu0l::Y);
                u.mulbc(15, 16, u.lane(3, 1), vu0l::Z | vu0l::W);
                u.mulbc(2, 1, u.q, vu0l::XYZ);
                u.mulbc(15, 15, u.lane(1, 3), vu0l::YZ | vu0l::W);
                u.add(4, 0, 2, vu0l::XY);
                u.addbc(5, 0, u.lane(2, 1), vu0l::X);
                u.subbc(5, 0, u.lane(2, 0), vu0l::Y);
                u.mulbc(7, 1, u.lane(CAM, 3), vu0l::XYZ);
                u.addbc(7, 0, u.lane(0, 0), vu0l::W);
                u.transform(4, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 4, u.lane(0, 0));
                u.transform(5, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 5, u.lane(0, 0));
                u.transform(6, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 6, u.lane(0, 0));
                u.transform(7, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 7, u.lane(7, 3));
                for (uint32_t k = 0; k < 4; ++k) {  // 0x287ec4: the 4 corners projected
                    spr.load_vf(u, 1, CORNERS + 0x10 * k);
                    spr.load_vf(u, 2, CORNERS + 0x40 + 0x10 * k);
                    u.mul(1, 1, 15, vu0l::YZ);
                    u.addbc(1, 1, u.lane(15, 3), vu0l::Z);
                    u.transform(1, 4, 5, 6, 7, 1, u.lane(0, 3));
                    u.div(u.lane(22, 0), u.lane(1, 3));
                    u.add(1, 1, 23, vu0l::W);
                    u.max_lane(1, 3, u.lane(1, 3), u.lane(21, 3));
                    u.mini_lane(1, 3, u.lane(1, 3), u.lane(21, 2));
                    u.mulbc(1, 1, u.q, vu0l::XYZ);
                    u.mulbc(2, 2, u.q, vu0l::XYZ);
                    u.add(1, 1, 23, vu0l::XYZ);
                    u.ftoi(1, 1, vu0l::XYZW, 16.0f);
                    spr.setq(sp + 0x10, cw);
                    spr.store_vf(u, 2, sp);
                    pkt += 0x30;
                    spr.store_vf(u, 1, sp + 0x20);
                    sp += 0x30;
                }
                flush_if_full();
            }
        }
    }
}

// orig: (helper) 0x288010 .. 0x2884cc -- the instances to clip: a third program; class by class a NEXT over their matrices (by the
// clip matrix), then their chain
void Pass::clipped() {
    reserve_tag(TAG_CLIP);
    if ((sp & 0x7ffu) != 0) flush();
    copy_packet(OV(0x001c2b80u), 3);
    if (spr.u32(CLIP_END) == CLIP_QUEUE) return;
    spr.setq(sp, Qw{});
    const uint32_t addr = lwu(0x001022d0u);
    spr.set32(sp + 4, 0x001022e0u);
    spr.set32(sp, 0x30000000u);
    pkt += 0x10;
    spr.set16(sp, addr);
    sp += 0x10;
    copy_packet(OV(0x001c2b00u), 8);
    for (int i = 0; i < 4; ++i) ram_load_vf(u, WORLD0 + i, OV(0x00166c80u) + 0xc0 + 0x10 * static_cast<uint32_t>(i));
    spr.set32(RESUME, OVF(0x00288290u));
    spr.set32(TOGGLE_A, 2);
    spr.set32(TOGGLE_B, 0xee);
    spr.set32(TOGGLE_C, 0x11);
    load_templates();
    const uint32_t table = lwu(OV(0x0016049cu));
    u.set_words(16, table, sign_word(table), lo32(at_hi), hi32(at_hi));  // (QMTC2 of at: its upper half the last one's)
    uint32_t e = CLIP_QUEUE;
    for (uint32_t c = 0; c < spr.u32(CLASSES); ++c) {
        uint32_t n = spr.u16(COUNTS_CLIP + 2 * c);
        if (n == 0) continue;
        cls = lwu(OV(0x001bc030u) + 4 * c);
        spr.setq(sp, Qw{});
        spr.set32(sp, 0x20000000u);
        pkt += 0x10;
        spr.set32(sp + 4, n * 0x50 + pkt);
        sp += 0x10;
        spr.set32(SWAY, (lhu(cls + 0x14) & 6) >> 1);
        const uint32_t first = e;
        while (n != 0) {  // 0x288228
            const uint32_t bounds = spr.u32(e);
            e += 4;
            --n;
            const uint32_t index = lhu(bounds + 0x18);
            a0_hi = vf_hi(16);
            const uint32_t m = (index << 6) + u.word(16, 0);
            ram_load_vf(u, 4, m + 0x30);
            ram_load_vf(u, 1, m + 0x0);
            ram_load_vf(u, 2, m + 0x10);
            ram_load_vf(u, 3, m + 0x20);
            spr.set32(e - 4, (index << 20) | (pkt >> 4));
            u.sub(4, 4, CAM, vu0l::XYZ);
            u.mulbc(1, 1, u.lane(4, 3), vu0l::XYZ);
            const uint32_t flag = spr.u32(SWAY);
            u.mulbc(2, 2, u.lane(4, 3), vu0l::XYZ);
            u.mulbc(3, 3, u.lane(4, 3), vu0l::XYZ);
            if (flag != 0) sway(flag, m);  // (it returns through RESUME: 0x288290)
            matrix_into_view();
            spr.store_vf(u, 1, sp + 0x00);
            spr.store_vf(u, 2, sp + 0x10);
            spr.store_vf(u, 3, sp + 0x20);
            spr.store_vf(u, 4, sp + 0x30);
            spr.setq(sp + 0x40, Qw{});
            sp += 0x50;
            pkt += 0x50;
            flush_if_full();
        }
        chain(first, spr.u16(COUNTS_CLIP + 2 * c), false);
    }
    end_tag();
}

// orig: (helper) 0x2884f0 .. 0x288654 -- the chain's last part out, the relight list's end, the classes' tags and the three
// programs' tags made NEXT tags (each to the next part)
void Pass::finish() {
    const uint32_t used = sp & 0x7ffu;
    if (used != 0) dma_from_spr(pkt - used, sp - used, used >> 4);
    spr.set16(spr.u32(RELIGHT_END), 0xffff);
    const uint32_t classes = spr.u32(CLASSES);
    for (uint32_t c = 0; c < classes; ++c) {
        const uint32_t w = spr.u32(CLASS_TAGS + 4 * c);
        if (w == 0) continue;
        const uint32_t size = (w >> 16) & 0xfff0u, addr = (w << 12) >> 8;
        ram_setq(addr - 0x10, Qw{});
        sw(addr - 0x10, 0x20000000u);
        sw(addr - 0xc, addr + size);
    }
    const uint32_t a = spr.u32(TAG_OPAQUE), b = spr.u32(TAG_BB), c = spr.u32(TAG_CLIP);
    sw(a, 0x20000000u);
    sw(b, 0x20000000u);
    sw(c, 0x20000000u);
    sw(a + 4, b + 0x10);
    sw(b + 4, c + 0x10);
    sw(c + 4, a + 0x10);
    sw(OV(0x001611c0u), pkt);
}

}  // namespace

// orig: 0x00286f10
bool shrub_cull() {
    if (!vu0l::usable()) return false;
    Pass p;
    p.u.load();
    p.setup();
    p.classes();
    // 0x287a04: the opaque program's part end, then the faded instances, class by class
    p.copy_packet(OV(0x001c2bb0u), 3);
    p.load_templates();
    {
        uint32_t list = FADED;
        for (uint32_t c = 0; c < p.spr.u32(CLASSES); ++c) {
            const uint32_t n = p.spr.u16(COUNTS_FADED + 2 * c);
            if (n == 0) continue;
            p.cls = lwu(OV(0x001bc030u) + 4 * c);
            p.chain(list, n, true);
            list += n << 2;
        }
    }
    p.billboards();
    p.clipped();
    p.finish();
    p.u.store();
    return true;
}

}  // namespace rac1::world_hand
