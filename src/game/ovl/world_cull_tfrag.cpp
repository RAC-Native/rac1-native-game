// The tfrag pass's culling and chain building (0x294368), written by hand from the routine: the same arithmetic, the same memory
// written in the same layout, the VU0 registers as a local copy (vu0_local.h) written back at the end. The transliteration
// (world_tfrag_cull_tr, world_cull.cpp) stays the reference: RAC1_WORLD_CULL_FAST (world_cull_fast.cpp) picks one or checks one
// against the other.
//
// What the routine does. The tfrag table (*(0x160f4c), *(0x160f50) entries of 0x40 bytes: a bounding sphere, the entry's data
// base, its LOD packets' offsets and sizes, a visibility group) comes into the scratchpad 32 entries at a time, double buffered
// (toSPR into 0x1000 / 0x1800 while the other half is tested). Each entry whose group is visible this frame (the group byte +0x3a
// against the group mask copied from 0x173d00 to SPR 0x3b00) has its sphere tested against the view (the camera's matrix in vf28
// .. vf31, the frustum's half-widths in vf23 .. vf27):
//  - outside: dropped;
//  - inside: drawn at a LOD picked from its view depth (vf1 as integers) against the distances at SPR 0x3f00 .. 0x3f08: a chain
//    record of 4 quadwords (LOD 14 / 16 / 20, near) or 3 (8 / 10, mid; 6, far or an entry flagged single-LOD), into the packet
//    buffer at SPR 0x0000 (double buffered by 0x800, flushed to the frame packet by fromSPR);
//  - on a plane: its 8 bounding-box corners (halfwords at its data + *(+0x38)) transformed and clip-tested; outside one plane:
//    dropped; inside the guard band: drawn as inside; else queued in the clipped list (SPR 0x3d00), drawn after the table with
//    LOD 2 (clipped by VU1).
// Along the way: the relight list (SPR 0x3000, halfword indices, entries with lights: byte +0x35 or halfword +0x36 != 0xffff) and
// the near entries' texture / data pairs (SPR 0x3400 and 0x3780, the latter copied to 0x1c5000 at the end).
// orig: (host)
#include <game/ovl/world_cull.h>
#include <game/ovl/world_cull_hand.h>

#include <game/ovl/ovl.h>

namespace rac1::world_hand {

namespace {

constexpr uint32_t SPR = 0x70000000u;
// the scratchpad's layout during the pass
constexpr uint32_t PACKETS = SPR + 0x0000;     // the packet buffer: two halves of 0x800
constexpr uint32_t ENTRIES = SPR + 0x1000;     // the table's entries: two halves of 0x800 (32 entries)
constexpr uint32_t RELIGHT = SPR + 0x3000;     // the relight list: halfword indices, to 0x33fe
constexpr uint32_t PAIRS = SPR + 0x3400;       // the near entries' pairs (texture address, size), to 0x3778; their twins at +0x380
constexpr uint32_t GROUPS = SPR + 0x3b00;      // the visibility groups' masks (0x80 bytes from 0x173d00)
constexpr uint32_t CORNERS = SPR + 0x3c00;     // the 8 corners of the entry being clip-tested, as floats
constexpr uint32_t CLIPPED = SPR + 0x3d00;     // the clipped entries' addresses, to 0x3efc
constexpr uint32_t LODS = SPR + 0x3f00;        // the LOD distances (a quadword from 0x160f70)
constexpr uint32_t TABLE = SPR + 0x3f10;       // the table's address
constexpr uint32_t PAIRS_FULL = SPR + 0x3778, RELIGHT_FULL = SPR + 0x33fe, CLIPPED_FULL = SPR + 0x3efc;

// the VU0 registers' roles during the pass
enum : int {
    CAM_POS = 28,                                    // the camera's position (x 1024: the tables' fixed point)
    VIEW0 = 29, VIEW1 = 30, VIEW2 = 31,             // the view rotation's rows
    CLIP0 = 18, CLIP1 = 19, CLIP2 = 20, CLIP3 = 21,  // the clip matrix (the view's 0x166d80 rows, translated)
    GUARD = 22,                                      // the guard band's scale (0x160f20)
};

struct Pass {
    vu0l::Vu u;
    Spr spr;
    uint32_t pkt = 0;    // the frame packet's cursor (RAM): where the packet buffer's bytes go
    uint32_t sp = 0;     // the packet buffer's cursor (scratchpad)
    uint32_t relight = RELIGHT, clipped = CLIPPED, pairs = PAIRS;
    Qw tpl[4];           // the chain records' templates (0x160a40 .. 0x160a70: two REF records, a CNT, the record's tail)

    // orig: (helper) a quadword of the packet buffer written, the buffer flushed when its half is full (the original tests
    // `sp & 0x700 == 0x700` after each record: a half's last 0x100 bytes)
    void flush_if_full() {
        if ((sp & 0x700u) == 0x700u) sp = flush_packets(sp, pkt, sp & 0x7ffu, 0x800u);
    }
    // orig: (helper) a fixed packet part copied from RAM (`n` quadwords)
    void copy_packet(uint32_t src, uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) spr.setq(sp + 16 * i, ram_q(src + 16 * i));
        sp += 16 * n;
        pkt += 16 * n;
    }
    // orig: (helper) the tfrag program's start: a CALL-type tag (0x30000000 | the program's address) to the VIF code at `vif`
    // (boot data, its first word's low half the address), then the 0x11 quadwords of the program's setup (0x1c2840)
    void program_start(uint32_t vif) {
        spr.setq(sp, Qw{});
        const uint32_t addr = lwu(vif);
        spr.set32(sp + 4, vif + 0x10);
        spr.set32(sp, 0x30000000u);
        pkt += 0x10;
        spr.set16(sp, addr);
        sp += 0x10;
        copy_packet(OV(0x001c2840u), 0x11);
    }

    // orig: (helper) an entry's relight index queued when it has lights (the list ends at RELIGHT_FULL)
    template <class Rd8, class Rd16>
    void queue_relight(Rd8 rd8, Rd16 rd16, uint32_t entry_addr) {
        if (rd8(0x35) == 0 && rd16(0x36) == 0xffffu) return;
        if (relight == RELIGHT_FULL) return;
        spr.set16(relight, (entry_addr - spr.u32(TABLE)) >> 6);
        relight += 2;
    }
    // orig: (helper) a near entry's texture / data pairs queued (the list ends at PAIRS_FULL)
    void queue_pairs(uint32_t e, uint32_t base) {
        if (pairs == PAIRS_FULL) return;
        spr.set32(pairs + 0x0, spr.u16(e + 0x2e) + base);
        spr.set32(pairs + 0x4, spr.u16(e + 0x2c));
        spr.set32(pairs + 0x380, spr.u16(e + 0x1c) + base);
        spr.set32(pairs + 0x384, spr.u8(e + 0x28));
        pairs += 8;
    }

    // orig: (helper) an entry's chain record: the near form (4 quadwords: two REFs of its LOD's packets, the CNT with the
    // texture's unpack, the tail with the LOD code) and the mid / far form (3: one REF). `rd8` / `rd16` read the entry
    // (scratchpad or RAM); `base` its data.
    template <class Rd8, class Rd16>
    void record_near(Rd8 rd8, Rd16 rd16, uint32_t base, uint32_t lod) {
        const uint32_t a = rd16(0x16), n0 = rd8(0x20), c = rd16(0x1e), t = rd8(0x26), cnt = rd8(0x2a);
        spr.setq(sp + 0x00, tpl[0]);
        spr.set8(sp + 0x00, n0);
        spr.set32(sp + 0x04, a + base);
        const uint32_t b = rd16(0x1a), n1 = rd8(0x23);
        spr.setq(sp + 0x10, tpl[0]);
        spr.set8(sp + 0x10, n1);
        spr.set32(sp + 0x14, b + base);
        spr.setq(sp + 0x20, tpl[1]);
        spr.set8(sp + 0x20, t >> 2);
        spr.set32(sp + 0x24, c + base);
        spr.set8(sp + 0x2e, (t >> 2) << 2);
        spr.set8(sp + 0x2c, cnt);
        spr.setq(sp + 0x30, tpl[3]);
        pkt += 0x40;
        spr.set8(sp + 0x3c, lod);
        sp += 0x40;
    }
    template <class Rd8, class Rd16>
    void record_short(Rd8 rd8, Rd16 rd16, uint32_t base, uint32_t lod, uint32_t a_off, uint32_t n_off, uint32_t t_off, const Qw& unpack) {
        const uint32_t a = rd16(a_off), n = rd8(n_off), c = rd16(0x1e), t = rd8(t_off), cnt = rd8(0x2a);
        spr.setq(sp + 0x00, tpl[0]);
        spr.set8(sp + 0x00, n);
        spr.set32(sp + 0x04, a + base);
        spr.setq(sp + 0x10, unpack);
        spr.set8(sp + 0x10, t >> 2);
        spr.set32(sp + 0x14, c + base);
        spr.set8(sp + 0x1e, (t >> 2) << 2);
        spr.set8(sp + 0x1c, cnt);
        spr.setq(sp + 0x20, tpl[3]);
        pkt += 0x30;
        spr.set8(sp + 0x2c, lod);
        sp += 0x30;
    }

    // orig: (helper) an entry drawn: its relight index, its pairs when near enough, its LOD picked from its view depth (vf1 as
    // integers) and its record (0x29468c .. 0x294920)
    void draw(uint32_t e, uint32_t entry_addr, uint32_t base, uint32_t single_lod_depth);

    enum class Clip { Out, Inside, Clipped };
    void setup();
    void table();
    Clip clip_test(uint32_t e, uint32_t base);
    void finish();
};

// orig: 0x00294368 (its start) -- the scratchpad's save area, the first DMA, the groups, the VU0 registers, the templates and the
// program's start
void Pass::setup() {
    for (uint32_t a = 0x3fa8; a < 0x4000; a += 8) spr.set64(SPR + a, 0);  // (the s registers' save: zeros here, the port's)
    const uint32_t table = lwu(OV(0x00160f4cu));
    spr.set32(TABLE, table);
    dma_to_spr(0x1000, table, 0x80);
    pkt = lwu(OV(0x001611c0u));
    sp = PACKETS;
    for (uint32_t i = 0; i < 0x80; i += 0x10) spr.setq(GROUPS + i, ram_q(OV(0x00173d00u) + i));

    // the view: the camera position x 1024, its rotation, the clip matrix translated to the camera, the frustum's widths
    const uint32_t view = OV(0x00166c80u);
    u.set_words(1, 0x44800000u, 0, 0, 0);  // (1024.0)
    ram_load_vf(u, CAM_POS, view + 0x140);
    ram_load_vf(u, VIEW0, view + 0x0);
    ram_load_vf(u, VIEW1, view + 0x10);
    ram_load_vf(u, VIEW2, view + 0x20);
    u.mulbc(CAM_POS, CAM_POS, u.lane(1, 0), vu0l::XYZW);
    ram_load_vf(u, 12, view + 0x100);
    ram_load_vf(u, 13, view + 0x110);
    ram_load_vf(u, 14, view + 0x120);
    ram_load_vf(u, 15, view + 0x130);
    u.sub(1, 0, CAM_POS, vu0l::XYZ);
    u.mulbc(CLIP0, 12, u.lane(0, 3), vu0l::XYZW);
    u.mulbc(CLIP1, 13, u.lane(0, 3), vu0l::XYZW);
    u.mulbc(CLIP2, 14, u.lane(0, 3), vu0l::XYZW);
    u.transform(CLIP3, 12, 13, 14, 15, 1, u.lane(0, 3));
    const uint32_t frustum = OV(0x0016ca40u);
    ram_load_vf(u, 23, frustum + 0xa0);
    ram_load_vf(u, 24, frustum + 0xb0);
    ram_load_vf(u, 25, frustum + 0x1d0);
    ram_load_vf(u, 26, frustum + 0x1e0);
    ram_load_vf(u, 27, frustum + 0x1f0);
    ram_load_vf(u, GUARD, OV(0x00160f20u));
    const uint32_t near_limit = lwu(OV(0x00160f80u));  // (QMTC2 of a sign-extended word: y its sign)
    u.set_words(1, near_limit, (near_limit & 0x80000000u) != 0 ? 0xffffffffu : 0u, 0, 0);
    u.addbc(23, 0, u.lane(1, 0), vu0l::Y);

    spr.setq(LODS, ram_q(OV(0x00160f70u)));
    for (int i = 0; i < 4; ++i) tpl[i] = ram_q(OV(0x00160a40u) + 0x10 * static_cast<uint32_t>(i));
    program_start(0x00103560u);
}

// orig: (helper) 0x29496c .. 0x294b5c -- an entry on a plane: its 8 corners (halfwords x 64 at RAM base + *(+0x38)) into SPR
// CORNERS as floats, transformed by the clip matrix and clip-tested as they are and scaled by the guard band. Returns whether
// it is outside one plane, inside the guard band, or to be clipped.
Pass::Clip Pass::clip_test(uint32_t e, uint32_t base) {
    const uint32_t pts = spr.u16(e + 0x38) + base;
    for (uint32_t k = 0; k < 8; ++k) {  // (pextlh / psraw 10 / qmtc2 / vitof0.xyz: x, y, z as h x 64; w the integer's bits)
        const uint64_t h = ld(pts + 8 * k);
        uint32_t w[4];
        for (int i = 0; i < 4; ++i) w[i] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(h >> (16 * i))) * 64);
        const int r = 4 + static_cast<int>(k & 1);
        u.set_words(r, w[0], w[1], w[2], w[3]);
        u.itof(r, r, vu0l::XYZ, 1.0f);
        spr.store_vf(u, r, CORNERS + 0x10 * k);
    }
    for (int i = 0; i < 4; ++i) spr.load_vf(u, 5 + i, CORNERS + 0x10 * static_cast<uint32_t>(i));
    const float one = u.lane(0, 3);
    u.transform(5, CLIP0, CLIP1, CLIP2, CLIP3, 5, one);
    u.transform(6, CLIP0, CLIP1, CLIP2, CLIP3, 6, one);
    u.clipw(5);
    u.transform(7, CLIP0, CLIP1, CLIP2, CLIP3, 7, one);
    u.clipw(6);
    u.transform(8, CLIP0, CLIP1, CLIP2, CLIP3, 8, one);
    u.clipw(7);
    u.mul(5, 5, GUARD, vu0l::XY);
    u.mul(6, 6, GUARD, vu0l::XY);
    u.clipw(8);
    u.mul(7, 7, GUARD, vu0l::XY);
    u.mul(8, 8, GUARD, vu0l::XY);
    spr.load_vf(u, 10, CORNERS + 0x40);
    spr.load_vf(u, 11, CORNERS + 0x50);
    const uint32_t first = u.clip;  // corners 1-4 against the frustum
    u.clipw(5);
    u.clipw(6);
    u.clipw(7);
    u.clipw(8);
    u.transform(5, CLIP0, CLIP1, CLIP2, CLIP3, 10, one);
    const uint32_t first_guard = u.clip;  // corners 1-4 against the guard band
    spr.load_vf(u, 7, CORNERS + 0x60);
    spr.load_vf(u, 8, CORNERS + 0x70);
    u.transform(6, CLIP0, CLIP1, CLIP2, CLIP3, 11, one);
    u.clipw(5);
    u.transform(7, CLIP0, CLIP1, CLIP2, CLIP3, 7, one);
    u.clipw(6);
    u.transform(8, CLIP0, CLIP1, CLIP2, CLIP3, 8, one);
    u.clipw(7);
    u.mul(5, 5, GUARD, vu0l::XY);
    u.mul(6, 6, GUARD, vu0l::XY);
    u.clipw(8);
    u.mul(7, 7, GUARD, vu0l::XY);
    u.mul(8, 8, GUARD, vu0l::XY);
    const uint32_t second = u.clip;  // corners 5-8 against the frustum
    u.clipw(5);
    u.clipw(6);
    u.clipw(7);
    u.clipw(8);
    const uint32_t second_guard = u.clip;
    // outside one plane: a plane's bit in all eight corners' judgements
    const uint32_t out = first & (first >> 6) & (first >> 12) & (first >> 18) & second & (second >> 6) & (second >> 12) & (second >> 18);
    if ((out & 0x3f) != 0) return Clip::Out;
    return (first_guard | second_guard) != 0 ? Clip::Clipped : Clip::Inside;
}

void Pass::draw(uint32_t e, uint32_t entry_addr, uint32_t base, uint32_t single_lod_depth) {
    const auto rd8 = [&](uint32_t o) { return static_cast<uint32_t>(spr.u8(e + o)); };
    const auto rd16 = [&](uint32_t o) { return static_cast<uint32_t>(spr.u16(e + o)); };
    queue_relight(rd8, rd16, entry_addr);
    const int32_t dy = static_cast<int32_t>(u.word(1, 1)), dx = static_cast<int32_t>(u.word(1, 0));
    const uint32_t far0 = spr.u32(LODS + 0x0), far1 = spr.u32(LODS + 0x4), far2 = spr.u32(LODS + 0x8);
    const bool single = spr.u8(e + 0x27) != 0;
    if (static_cast<int32_t>(single_lod_depth - static_cast<uint32_t>(dy)) >= 0) queue_pairs(e, base);
    // (the distances against the depth's y, then its x: the integers' wrapping differences, as the original's)
    const auto le0 = [](uint32_t lim, int32_t d) { return static_cast<int32_t>(lim - static_cast<uint32_t>(d)) <= 0; };
    uint32_t lod;
    if (single || le0(far0, dy)) lod = 6;
    else if (le0(far0, dx)) lod = 8;
    else if (le0(far1, dy)) lod = 10;
    else if (le0(far1, dx)) lod = 14;
    else if (le0(far2, dy) || le0(far2, dx)) lod = 16;
    else lod = 20;
    note_tfrag(entry_addr, lod);
    if (lod >= 14) record_near(rd8, rd16, base, lod);
    else if (lod >= 8) record_short(rd8, rd16, base, lod, 0x16, 0x22, 0x25, tpl[2]);
    else record_short(rd8, rd16, base, lod, 0x14, 0x21, 0x24, tpl[1]);
    flush_if_full();
}

// orig: (helper) 0x294520 .. 0x294bf4 -- the table, 32 entries at a time
void Pass::table() {
    const uint32_t table = spr.u32(TABLE);
    uint32_t count = lwu(OV(0x00160f50u));
    uint32_t next = table + 0x800;      // the next batch's source
    uint32_t half = 0x800;               // the batch's half of ENTRIES
    uint32_t entry_addr = table - 0x40;  // the entry's address in RAM
    while (count != 0) {
        half ^= 0x800;
        uint32_t e = ENTRIES + half;
        const int32_t n = std::min(0x20, static_cast<int32_t>(count));  // (pminw: signed)
        count -= static_cast<uint32_t>(n);
        const uint32_t end = e + (static_cast<uint32_t>(n) << 6);
        if (count != 0) {  // the next batch into the other half
            dma_to_spr(e ^ 0x800, next, 0x80);
            next += 0x800;
        }
        for (;;) {
            spr.load_vf(u, 1, e);  // (the sphere: loaded before the end test, the end's too)
            if (e == end) break;
            const uint32_t s = e;
            e += 0x40;
            entry_addr += 0x40;
            // the sphere in view space: vf2 = rotated (centre - camera), vf3 = (r, -r)
            u.sub(2, 1, CAM_POS, vu0l::XYZ);
            u.addbc(3, 0, u.lane(1, 3), vu0l::X);
            u.subbc(3, 0, u.lane(1, 3), vu0l::Y);
            const uint32_t single_lod_depth = spr.u16(s + 0x3e);
            const uint32_t base = spr.u32(s + 0x10);
            u.rotate(2, VIEW0, VIEW1, VIEW2, 2, vu0l::XYZ);
            const uint32_t gmask = spr.u8(s + 0x3a) & spr.u8(GROUPS + spr.u8(s + 0x3b));
            u.mulbc(8, 26, u.lane(1, 3), vu0l::XY);
            u.addbc(1, 3, u.lane(2, 2), vu0l::XY);
            u.mulbc(5, 24, u.lane(2, 2), vu0l::XY);
            if (gmask == 0) continue;  // its group not visible
            // the frustum's sides at the sphere's depth (vf5 = the half-widths there, vf8 = r scaled): vf7 / vf8 the margins
            // of the sphere's far / near side; vf3 / vf4 the depth range against the near / far planes
            u.abs(2, 2, vu0l::XY);
            u.sub(3, 23, 1, vu0l::XY);
            u.sub(6, 2, 8, vu0l::XY);
            u.add(8, 2, 8, vu0l::XY);
            u.sub(4, 1, 27, vu0l::XY);
            u.sub(7, 5, 6, vu0l::XY);
            u.sub(8, 5, 8, vu0l::XY);
            u.ftoi(1, 1, vu0l::XY, 1.0f);
            if (u.negative(3, 1) || !u.negative(3, 0)) continue;  // beyond the near plane / the far one
            if (u.negative(7, 1) || u.negative(7, 0)) continue;   // beside the view
            const bool on_plane = u.negative(8, 1) || u.negative(4, 1) || u.negative(8, 0) || !u.negative(4, 0);
            if (on_plane) {
                const Clip c = clip_test(s, base);
                if (c == Clip::Out) continue;
                if (c == Clip::Clipped) {  // 0x294b4c: queued for the clipped pass
                    queue_relight([&](uint32_t o) { return static_cast<uint32_t>(spr.u8(s + o)); },
                                  [&](uint32_t o) { return static_cast<uint32_t>(spr.u16(s + o)); }, entry_addr);
                    spr.set32(clipped, entry_addr);  // (written even when the list is full)
                    if (clipped == CLIPPED_FULL) continue;
                    clipped += 4;
                    if (static_cast<int32_t>(single_lod_depth - u.word(1, 1)) < 0) continue;
                    queue_pairs(s, base);
                    continue;
                }
            }
            draw(s, entry_addr, base, single_lod_depth);
        }
    }
}

// orig: (helper) 0x294bf8 .. 0x294f4c -- the table's end: the program's end packet, the lists' ends, the pairs' twins out to 0x1c5000,
// then the clipped entries (LOD 2, their records from RAM) as a second program
void Pass::finish() {
    copy_packet(OV(0x001c2980u), 4);
    sp = flush_packets(sp, pkt, sp & 0x7ffu, 0x800u);
    spr.set32(pairs, 0);
    spr.set32(pairs + 0x380, 0);
    spr.set16(relight, 0xffff);
    dma_from_spr(OV(0x001c5000u), 0x3780, 0x38);
    if (clipped != CLIPPED) {
        program_start(0x00107010u);
        for (uint32_t c = CLIPPED; c != clipped; c += 4) {
            const uint32_t entry = spr.u32(c);
            note_tfrag(entry, 2);
            const uint32_t base = lwu(entry + 0x10);
            record_near([&](uint32_t o) { return static_cast<uint32_t>(lbu(entry + o)); },
                        [&](uint32_t o) { return static_cast<uint32_t>(lhu(entry + o)); }, base, 2);
            flush_if_full();
        }
        copy_packet(OV(0x001c2980u), 4);
        sp = flush_packets(sp, pkt, sp & 0x7ffu, 0x800u);
    }
    sw(OV(0x001611c0u), pkt);
}

}  // namespace

// orig: 0x00294368
bool tfrag_cull() {
    if (!vu0l::usable()) return false;
    Pass p;
    p.u.load();
    p.setup();
    p.table();
    p.finish();
    p.u.store();
    return true;
}

}  // namespace rac1::world_hand
