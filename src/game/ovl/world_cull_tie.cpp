// The tie pass's culling and chain building (0x295fa0), written by hand from the routine: the same arithmetic, the same memory
// written in the same layout, the VU0 registers as a local copy (vu0_local.h) written back at the end. The transliteration
// (world_tie_cull_tr, world_cull.cpp) stays the reference: RAC1_WORLD_CULL_FAST (world_cull_fast.cpp) picks one or checks one
// against the other.
//
// What the routine does, class by class (the tie classes queued at 0x1c5800, *(0x160fbc) of them; a class record in RAM: its
// three LODs' packet lists at +0x0 / +0x4 / +0x8 with their counts at +0x20 .. +0x22, its LOD distances at +0x10, its LOD counters
// at +0x1c .. +0x1e, its flags at +0x24, its instances at *(+0x28), *(+0x26) of them, its bounding box's corners at +0xa0):
//  1. its instances come into the scratchpad (0x2000), 128 at a time (records of 0x20 bytes: a bounding sphere, the instance's
//     matrix address at +0x10, its visibility group at +0x18 / +0x19, its LOD at +0x14, its lights at +0x1b / +0x1e). Each is
//     tested against the view as the tfrag pass's entries are; a visible one keeps its view-space vector in its record (+0x0)
//     and the address of its 0x60 bytes in the frame packet (+0xc); one on a frustum plane has its box's 8 corners clip-tested,
//     and its record notes whether it fits the guard band (+0x8: 1) or must be clipped (0);
//  2. the visible ones again: the LOD (and the morph between two LODs) from the view depth against the class's distances, the
//     instance queued on its LOD's list (linked through +0x1c), its matrix rotated into the view (swaying first, for a class whose
//     flags ask: 0x296a4c) and written with its morph into the packet; one to be clipped is taken off its list and queued at SPR
//     0x3a00 instead (the clipped instances, drawn at the end with the clipping program);
//  3. the three lists' packets: for each of a LOD's packets its header (REFs of its data, the texture unpack, a FLUSH) then a
//     REF of each listed instance's matrix and morph.
// After the classes: the clipped instances' matrices (rotated by the clip matrix) written straight into the packet, and their
// packets with the clipping program. Along the way: the relight list (SPR 0x3600, halfword instance indices), the classes'
// nearest depths (SPR 0x3200). The packet buffer is SPR 0x0000, double buffered by 0x1000.
//
// Three of the original's registers carry their upper 64 bits from one use to the next and into the VU0 registers (a QMTC2 moves
// all 128): the instance test's pminw keeps the minimum of its upper words, a class's vf17 starts with v1's, the sway's sines
// take a0's. They are kept here as such (`hi` below), so the VU0 registers come out as the routine leaves them.
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
constexpr uint32_t PACKETS = SPR + 0x0000;     // the packet buffer: two halves of 0x1000
constexpr uint32_t RECORDS = SPR + 0x2000;     // the instances' records (128 of 0x20)
constexpr uint32_t GROUPS = SPR + 0x3000;      // the visibility groups' masks (0x80 bytes from 0x173d00)
constexpr uint32_t SINES = SPR + 0x3500;       // the sway's sine table (0x100 signed bytes from 0x1c3ef0)
constexpr uint32_t CLIPPED = SPR + 0x3a00;     // the clipped instances (0x10 each), to 0x3e00
constexpr uint32_t TEMPLATES = SPR + 0x3e00;   // the chain's templates (8 quadwords from 0x160a90)
constexpr uint32_t WIND = SPR + 0x3e80;        // the sway's parameters (2 quadwords from 0x1c3ff0) and its clock (+0x20)
constexpr uint32_t SAVED_VF12 = SPR + 0x3eb0;  // vf12 kept over a sway
constexpr uint32_t CLASSES_LEFT = SPR + 0x3f00, CLASS_NEXT = SPR + 0x3f04, CLIPPED_END = SPR + 0x3f08, BATCH_SRC = SPR + 0x3f0c;
constexpr uint32_t LIST_HEAD = SPR + 0x3f10, NEAREST_END = SPR + 0x3f14, RELIGHT_END = SPR + 0x3f18, TABLE = SPR + 0x3f1c;
constexpr uint32_t SWAY = SPR + 0x3f20, RESUME = SPR + 0x3f24, LOD_LIMIT = SPR + 0x3f28;
constexpr uint32_t RELIGHT_FULL = SPR + 0x39fe, CLIPPED_FULL = SPR + 0x3e00;

// the VU0 registers' roles during the pass
enum : int {
    CAM = 24,                            // the camera's position (x 1024 in w: the tables' fixed point)
    VIEW0 = 25, VIEW1 = 26, VIEW2 = 27,  // the view rotation's rows
    WORLD0 = 28,                         // the view matrix (rows 28 .. 31): the instances' matrices into the view
    CLIP0 = 12,                          // the clip matrix (rows 12 .. 15)
    CLASS_LODS = 16,                     // the class's LOD distances
    NEAREST = 17,                        // the class's nearest depth
};

inline uint32_t lo32(uint64_t v) { return static_cast<uint32_t>(v); }
inline uint32_t hi32(uint64_t v) { return static_cast<uint32_t>(v >> 32); }
inline uint32_t sign_word(uint32_t w) { return (w & 0x80000000u) != 0 ? 0xffffffffu : 0u; }
// pminw on two registers' upper doublewords (signed words)
inline uint64_t min_words(uint64_t a, uint64_t b) {
    const uint32_t l = static_cast<uint32_t>(std::min(static_cast<int32_t>(lo32(a)), static_cast<int32_t>(lo32(b))));
    const uint32_t h = static_cast<uint32_t>(std::min(static_cast<int32_t>(hi32(a)), static_cast<int32_t>(hi32(b))));
    return l | static_cast<uint64_t>(h) << 32;
}

struct Pass {
    vu0l::Vu u;
    Spr spr;
    uint32_t pkt = 0;     // the frame packet's cursor (RAM)
    uint32_t sp = 0;      // the packet buffer's cursor (scratchpad)
    uint32_t parity = 0;  // the instances' alternating ITOP halves (0 / 0x40)
    Qw tpl[8];            // the chain's templates (TEMPLATES)
    // the original's registers' upper halves that reach the VU0 (see the file's comment)
    uint64_t v0_hi = 0, v1_hi = 0, a0_hi = 0, t3_hi = 0;

    // the class and its batch
    uint32_t cls = 0;
    uint32_t list[3] = {};   // the LODs' lists (record addresses, linked through +0x1c)
    uint32_t count[3] = {};  // the class's LOD counters (+0x1c, +0x1d, +0x1e)
    uint32_t used = 0;       // the batch's bytes of the frame packet (0x60 an instance)

    uint64_t vf_hi(int r) const { return u.word(r, 2) | static_cast<uint64_t>(u.word(r, 3)) << 32; }

    // orig: (helper) the packet buffer's half flushed to the frame packet (fromSPR), the other half taken
    void flush() { sp = flush_packets(sp, pkt, sp & 0xfffu, 0x1000u); }
    // orig: (helper) a fixed part copied from RAM into the packet buffer (`n` quadwords): returns the last three quadwords
    // read (the original reads them into v0, v1, a0 ... in order: their upper halves stay in those registers)
    void copy_packet(uint32_t src, uint32_t n, uint64_t* hi_out) {
        for (uint32_t i = 0; i < n; ++i) {
            const Qw q = ram_q(src + 16 * i);
            spr.setq(sp + 16 * i, q);
            if (i < 3) hi_out[i] = q.hi;
        }
        sp += 16 * n;
        pkt += 16 * n;
    }
    // orig: (helper) a VU1 program's start (a CALL-type tag to the VIF code at `vif`, its first word's low half the address)
    void program_call(uint32_t vif) {
        spr.setq(sp, Qw{});
        const uint32_t addr = lwu(vif);
        spr.set32(sp + 4, vif + 0x10);
        spr.set32(sp, 0x30000000u);
        pkt += 0x10;
        spr.set16(sp, addr);
        sp += 0x10;
    }
    // orig: (helper) the chain's end: an END tag with the VIF code FLUSHA (0x13000000)
    void end_tag() {
        spr.setq(sp, Qw{});
        spr.set32(sp, 0x10000000u);
        spr.set32(sp + 8, 0x13000000u);
        sp += 0x10;
        pkt += 0x10;
    }

    // orig: (helper) 0x296670 -- an instance with lights queued for the relighting (its index in the table)
    void relight(uint32_t rec) {
        const uint32_t index = ((spr.u32(BATCH_SRC) + (rec & 0xfffu)) - spr.u32(TABLE)) >> 5;
        const uint32_t at = spr.u32(RELIGHT_END);
        if (at == RELIGHT_FULL) return;
        spr.set16(at, index);
        spr.set32(RELIGHT_END, at + 2);
    }

    void setup();
    void classes();
    void batch(uint32_t src, uint32_t n);
    bool test(uint32_t rec);
    bool clip_test(uint32_t rec);
    void lod_and_matrix(uint32_t rec, uint32_t matrix);
    void matrix_into_view(uint32_t matrix);
    void clipped_record(uint32_t rec, uint32_t matrix, uint32_t packets, uint32_t n);
    void sway(uint32_t flag, uint32_t matrix);
    uint32_t packet_header(uint32_t p, uint32_t data, uint32_t base, bool extra);
    void instance_ref(uint32_t matrix_pkt, uint32_t matrix);
    void lists();
    void clipped();
};

// orig: 0x00295fa0 (its start) -- the scratchpad's state, the VU0 registers, the templates, the program's start
void Pass::setup() {
    for (uint32_t a = 0x3fa8; a < 0x4000; a += 8) spr.set64(SPR + a, 0);  // (the s registers' save: zeros here, the port's)
    spr.set32(CLASSES_LEFT, lwu(OV(0x00160fbcu)));
    spr.set32(CLASS_NEXT, OV(0x001c5800u));
    spr.set32(CLIPPED_END, CLIPPED);
    spr.set32(NEAREST_END, SPR + 0x3200);
    spr.set32(RELIGHT_END, SPR + 0x3600);
    spr.set32(TABLE, lwu(OV(0x00160fc0u)));
    spr.set32(LOD_LIMIT, lwu(OV(0x00160fe0u)));
    pkt = lwu(OV(0x001611c0u));
    sp = PACKETS;
    for (uint32_t a = RECORDS; a < GROUPS; a += 0x10) spr.setq(a, Qw{});
    for (uint32_t i = 0; i < 0x80; i += 0x10) spr.setq(GROUPS + i, ram_q(OV(0x00173d00u) + i));
    for (uint32_t i = 0; i < 0x100; i += 0x10) spr.setq(SINES + i, ram_q(OV(0x001c3ef0u) + i));
    const Qw wind0 = ram_q(OV(0x001c3ff0u));
    spr.setq(WIND, wind0);
    spr.setq(WIND + 0x10, ram_q(OV(0x001c3ff0u) + 0x10));
    spr.set32(WIND + 0x20, lwu(OV(0x0015f3f8u)));
    spr.set32(RESUME, OVF(0x002968b8u));

    const uint32_t view = OV(0x00166c80u);
    u.set_words(1, 0x44800000u, 0, lo32(wind0.hi), hi32(wind0.hi));  // (1024.0; v0's upper half: the wind's first quadword)
    ram_load_vf(u, CAM, view + 0x140);
    ram_load_vf(u, VIEW0, view + 0x0);
    ram_load_vf(u, VIEW1, view + 0x10);
    ram_load_vf(u, VIEW2, view + 0x20);
    for (int i = 0; i < 4; ++i) ram_load_vf(u, WORLD0 + i, view + 0x80 + 0x10 * static_cast<uint32_t>(i));
    for (int i = 0; i < 4; ++i) ram_load_vf(u, CLIP0 + i, view + 0x100 + 0x10 * static_cast<uint32_t>(i));
    u.mulbc(CAM, 0, u.lane(1, 0), vu0l::W);
    const uint32_t frustum = OV(0x0016ca40u);
    ram_load_vf(u, 18, frustum + 0x220);
    ram_load_vf(u, 19, frustum + 0xa0);
    ram_load_vf(u, 20, frustum + 0xb0);
    ram_load_vf(u, 22, frustum + 0x1e0);
    ram_load_vf(u, 23, frustum + 0x1f0);
    u.set_words(1, 0x3a800000u, 0, 0, 0);  // (1 / 1024)
    u.mulbc(19, 19, u.lane(1, 0), vu0l::XY);
    u.mulbc(23, 23, u.lane(1, 0), vu0l::XY);
    ram_load_vf(u, 21, OV(0x00160f90u));
    for (uint32_t i = 0; i < 8; ++i) spr.setq(TEMPLATES + 0x10 * i, ram_q(OV(0x00160a90u) + 0x10 * i));

    program_call(0x00108e10u);
    uint64_t hi[3];
    copy_packet(OV(0x001c29c0u), 6, hi);
    v0_hi = hi[0], v1_hi = hi[1], a0_hi = hi[2];
}

// orig: (helper) 0x2962d4 .. 0x2965f8 -- an instance's record tested against the view (see the tfrag cull's test): visible, its
// view vector kept in the record and its packet bytes reserved. Returns false when the instance is dropped.
bool Pass::test(uint32_t rec) {
    const uint32_t group = spr.u8(rec + 0x19), mask = spr.u8(rec + 0x18);
    u.sub(2, 1, CAM, vu0l::XYZ);
    u.addbc(3, 0, u.lane(1, 3), vu0l::X);
    u.subbc(3, 0, u.lane(1, 3), vu0l::Y);
    u.rotate(2, VIEW0, VIEW1, VIEW2, 2, vu0l::XYZ);
    spr.set64(rec + 0x0, 0);  // (the record's state cleared)
    spr.set32(rec + 0x14, 0);
    u.subbc(19, 4, u.lane(1, 3), vu0l::Y);
    u.mulbc(8, 22, u.lane(1, 3), vu0l::XY);
    u.addbc(1, 3, u.lane(2, 2), vu0l::XY);
    u.mulbc(5, 20, u.lane(2, 2), vu0l::XY);
    if ((mask & spr.u8(GROUPS + group)) == 0) return false;
    u.abs(2, 2, vu0l::XY);
    u.sub(3, 19, 1, vu0l::XY);
    u.sub(6, 2, 8, vu0l::XY);
    u.add(8, 2, 8, vu0l::XY);
    u.sub(4, 1, 23, vu0l::XY);
    u.max_lane(1, 2, u.lane(0, 2), u.lane(2, 2));
    u.sub(7, 5, 6, vu0l::XY);
    u.sub(8, 5, 8, vu0l::XY);
    a0_hi = vf_hi(4);  // (QMFC2 a0, vf4)
    if (u.negative(3, 1)) return false;  // beyond its LOD's distance
    u.mr32(1, 1, vu0l::XY);
    if (!u.negative(3, 0)) return false;  // behind the camera
    v0_hi = vf_hi(7), v1_hi = vf_hi(8);   // (QMFC2 v0, vf7; v1, vf8)
    if (u.negative(7, 1) || u.negative(7, 0)) return false;  // beside the view
    const bool on_plane = u.negative(8, 1) || u.negative(4, 1) || u.negative(8, 0) || !u.negative(4, 0);
    if (on_plane) return clip_test(rec);
    spr.store_vf(u, 1, rec);
    spr.set32(rec + 0xc, pkt + used + 0x10);
    used += 0x60;
    return true;
}

// orig: (helper) 0x296404 .. 0x2965f8 -- an instance on a plane: its class's box (8 corners at +0xa0) by its matrix and the clip
// matrix, clip-tested as they are and scaled by the guard band (vf21). Outside one plane: dropped; else kept, with whether it
// fits the guard band.
bool Pass::clip_test(uint32_t rec) {
    const uint32_t m = spr.u32(rec + 0x10);
    u.mulbc(9, 1, u.lane(0, 3), vu0l::XYZW);
    ram_load_vf(u, 4, m + 0x30);
    ram_load_vf(u, 1, m + 0x0);
    ram_load_vf(u, 2, m + 0x10);
    ram_load_vf(u, 3, m + 0x20);
    ram_load_vf(u, 5, cls + 0xa0);
    ram_load_vf(u, 6, cls + 0xb0);
    u.mulbc(1, 1, u.lane(4, 3), vu0l::XYZ);  // (the matrix's rows scaled by its w)
    u.mulbc(2, 2, u.lane(4, 3), vu0l::XYZ);
    u.mulbc(3, 3, u.lane(4, 3), vu0l::XYZ);
    u.sub(4, 4, CAM, vu0l::XYZ);
    u.rotate(1, CLIP0, CLIP0 + 1, CLIP0 + 2, 1, vu0l::XYZW);
    u.mulbc(4, 4, u.lane(CAM, 3), vu0l::XYZ);
    u.rotate(2, CLIP0, CLIP0 + 1, CLIP0 + 2, 2, vu0l::XYZW);
    ram_load_vf(u, 7, cls + 0xc0);
    u.rotate(3, CLIP0, CLIP0 + 1, CLIP0 + 2, 3, vu0l::XYZW);
    ram_load_vf(u, 8, cls + 0xd0);
    const float one = u.lane(0, 3);
    u.transform(4, CLIP0, CLIP0 + 1, CLIP0 + 2, CLIP0 + 3, 4, one);
    // the corners: vf1 .. vf4 now the instance's clip matrix
    u.transform(5, 1, 2, 3, 4, 5, one);
    u.transform(6, 1, 2, 3, 4, 6, one);
    u.clipw(5);
    u.transform(7, 1, 2, 3, 4, 7, one);
    u.clipw(6);
    u.transform(8, 1, 2, 3, 4, 8, one);
    u.clipw(7);
    u.mul(5, 5, 21, vu0l::XY);
    u.mul(6, 6, 21, vu0l::XY);
    u.clipw(8);
    u.mul(7, 7, 21, vu0l::XY);
    u.mul(8, 8, 21, vu0l::XY);
    ram_load_vf(u, 10, cls + 0xe0);
    ram_load_vf(u, 11, cls + 0xf0);
    const uint32_t first = u.clip;
    u.clipw(5);
    u.clipw(6);
    u.clipw(7);
    u.clipw(8);
    u.transform(5, 1, 2, 3, 4, 10, one);
    const uint32_t first_guard = u.clip;
    ram_load_vf(u, 7, cls + 0x100);
    ram_load_vf(u, 8, cls + 0x110);
    u.transform(6, 1, 2, 3, 4, 11, one);
    u.clipw(5);
    u.transform(7, 1, 2, 3, 4, 7, one);
    u.clipw(6);
    u.transform(8, 1, 2, 3, 4, 8, one);
    u.clipw(7);
    u.mul(5, 5, 21, vu0l::XY);
    u.mul(6, 6, 21, vu0l::XY);
    u.clipw(8);
    u.mul(7, 7, 21, vu0l::XY);
    u.mul(8, 8, 21, vu0l::XY);
    const uint32_t second = u.clip;
    u.clipw(5);
    u.clipw(6);
    u.clipw(7);
    u.clipw(8);
    const uint32_t second_guard = u.clip;
    const uint32_t out = first & (first >> 6) & (first >> 12) & (first >> 18) & second & (second >> 6) & (second >> 12) & (second >> 18);
    if ((out & 0x3f) != 0) return false;
    spr.store_vf(u, 9, rec);
    const uint32_t at = pkt + used + 0x10;
    spr.set32(rec + 0x8, (first_guard | second_guard) == 0 ? 1u : 0u);  // (fits the guard band: drawn without clipping)
    used += 0x60;
    spr.set32(rec + 0xc, at);
    return true;
}

// orig: (helper) 0x296a4c .. 0x296bd8 -- a swaying class's matrix (vf1 .. vf3, the translation vf4 relative to the camera): within
// the wind's reach (wind.y against the squared distance), the matrix sheared by two sines of the clock and the instance's address
// (the table at SINES), weaker with distance; `flag` bit 1: the stronger sway. vf12 kept.
void Pass::sway(uint32_t flag, uint32_t matrix) {
    spr.load_vf(u, 9, WIND + 0x10);
    spr.load_vf(u, 8, WIND);
    spr.store_vf(u, 12, SAVED_VF12);
    u.addbc(11, 0, u.lane(0, 3), vu0l::X);
    const uint32_t clock = spr.u32(WIND + 0x20);
    const uint32_t strong = flag >> 1;
    u.mul(12, 4, 4, vu0l::XYZ);
    u.addabc(12, u.lane(12, 1), vu0l::X);
    u.maddbc(12, 11, u.lane(12, 2), vu0l::X);  // the squared distance
    u.subbc_flags(9, u.lane(12, 0), vu0l::Y);
    const bool beyond = (u.mac & 0x40u) != 0;  // (the MAC flag's y sign: wind.y - d² < 0)
    u.mulbc(12, 12, u.lane(9, 2), vu0l::X);
    u.subbc(12, 0, u.lane(12, 0), vu0l::W);    // the falloff
    if (!beyond) {
        u.mulbc(5, 0, u.lane(0, 0), vu0l::XYZW);
        const uint32_t k = (matrix >> 9) & 0x1ffu;
        u.mulbc(6, 0, u.lane(0, 0), vu0l::XYZW);
        u.mr32(7, 0, vu0l::XYZW);
        u.addbc(5, 5, u.lane(0, 3), vu0l::X);
        u.addbc(6, 6, u.lane(0, 3), vu0l::Y);
        const uint32_t p0 = k * 67 + clock;                        // (k << 6) + (k << 1) + k
        const uint32_t p1 = k * 123 + clock + (strong != 0 ? clock : 0);  // (k << 7) - (k << 2) - k
        // a sine (a signed byte of the table, << 5) into a register through QMTC2 (its upper half: a0's), then to floats / 4096
        const auto sine = [&](int r, uint32_t i) {
            const uint32_t w = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(spr.u8(SINES + (i & 0xffu)))) << 5);
            u.set_words(r, w, sign_word(w), lo32(a0_hi), hi32(a0_hi));
            u.itof(r, r, vu0l::XYZW, 1.0f / 4096.0f);
        };
        sine(10, p0);
        sine(11, p0 >> 1);
        u.mul(7, 10, 11, vu0l::X);
        u.subbc(7, 0, u.lane(9, 0), vu0l::W);
        u.mulbc(7, 7, u.lane(9, 0), vu0l::X);
        u.addbc(7, 7, u.lane(7, 3), vu0l::X);
        sine(10, p1 + 0x40);
        sine(11, p1 >> 1);
        u.mulbc(7, 7, u.lane(8, 0), vu0l::X);
        u.mulabc(10, u.lane(8, 1), vu0l::X);
        u.maddbc(10, 7, u.lane(8, 2), vu0l::X);
        u.mulbc(10, 10, u.lane(12, 3), vu0l::X);
        u.mulabc(11, u.lane(8, 1), vu0l::X);
        u.maddbc(11, 7, u.lane(8, 3), vu0l::X);
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
    spr.load_vf(u, 12, SAVED_VF12);
}

// orig: (helper) 0x296888 .. 0x29691c -- a visible instance's matrix (RAM `matrix`) scaled, swayed when its class asks, into the
// view and written with its morph (already at sp + 0x40 / + 0x50) as its 0x60 bytes of the packet
void Pass::matrix_into_view(uint32_t matrix) {
    ram_load_vf(u, 4, matrix + 0x30);
    ram_load_vf(u, 1, matrix + 0x0);
    ram_load_vf(u, 2, matrix + 0x10);
    ram_load_vf(u, 3, matrix + 0x20);
    u.mulbc(1, 1, u.lane(4, 3), vu0l::XYZ);
    const uint32_t flag = spr.u32(SWAY);
    u.mulbc(2, 2, u.lane(4, 3), vu0l::XYZ);
    sp += 0x60;
    u.mulbc(3, 3, u.lane(4, 3), vu0l::XYZ);
    u.sub(4, 4, CAM, vu0l::XYZ);
    if (flag != 0) sway(flag, matrix);  // (it returns through RESUME: 0x2968b8)
    u.rotate(1, WORLD0, WORLD0 + 1, WORLD0 + 2, 1, vu0l::XYZW);
    u.mulbc(4, 4, u.lane(CAM, 3), vu0l::XYZ);
    pkt += 0x60;
    u.rotate(2, WORLD0, WORLD0 + 1, WORLD0 + 2, 2, vu0l::XYZW);
    u.rotate(3, WORLD0, WORLD0 + 1, WORLD0 + 2, 3, vu0l::XYZW);
    u.transform(4, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 4, u.lane(0, 3));
    spr.store_vf(u, 1, sp - 0x60);
    spr.store_vf(u, 2, sp - 0x50);
    spr.store_vf(u, 3, sp - 0x40);
    spr.store_vf(u, 4, sp - 0x30);
    if ((sp & 0xe00u) == 0xe00u) flush();
}

// orig: (helper) 0x2969ec .. 0x296a48 -- an instance to be clipped: its packet bytes kept (its matrix comes later), a record of it
// queued at CLIPPED (its packet address, its matrix with the sway flag, its LOD's packets, their count and the class's data base)
void Pass::clipped_record(uint32_t rec, uint32_t matrix, uint32_t packets, uint32_t n) {
    uint32_t at = spr.u32(CLIPPED_END);
    if (at != CLIPPED_FULL) {
        spr.set32(at + 0x0, spr.u32(rec + 0xc));
        spr.set32(at + 0x4, spr.u32(SWAY) | matrix);
        spr.set32(at + 0x8, packets);
        spr.set32(at + 0xc, (n << 24) | (lwu(cls + 0x2c) >> 4));
        at += 0x10;
    }
    spr.set32(CLIPPED_END, at);
    if ((sp & 0xe00u) == 0xe00u) flush();
}

// orig: (helper) 0x296624 .. 0x2969e8 -- a visible instance (its view vector in vf1): its relighting, its LOD from its depth
// against the class's distances (vf16) - the near LOD, a morph from it to the middle one, a morph from the middle one to the far
// one, or the far one -, its nearest depth, its list, its morph's quadwords, then its matrix or its clipped record
void Pass::lod_and_matrix(uint32_t rec, uint32_t matrix) {
    note_tie(((spr.u32(BATCH_SRC) + (rec & 0xfffu)) - spr.u32(TABLE)) >> 5);
    u.subbc(2, CLASS_LODS, u.lane(1, 1), vu0l::XYZ);  // the distances' margins
    const uint32_t lights = spr.u8(rec + 0x1b), light_set = spr.u16(rec + 0x1e);
    u.mulbc(3, 1, u.lane(CAM, 3), vu0l::X);
    u.addav(0, 18, vu0l::Y);
    u.maddbc(4, 1, u.lane(18, 0), vu0l::Y);
    if (lights != 0 || light_set != 0xffffu) relight(rec);
    v0_hi = vf_hi(2);  // (QMFC2 v0, vf2; v1 = PROT3W v0)
    u.mini_lane(NEAREST, 0, u.lane(3, 0), u.lane(NEAREST, 0));
    v1_hi = u.word(2, 0) | static_cast<uint64_t>(u.word(2, 3)) << 32;
    const auto morph_quads = [&]() {  // (the 256-scaled morph: vf5 = 256 in every lane)
        u.set_words(5, 0x43800000u, 0x43800000u, 0x43800000u, 0x43800000u);
    };
    const Qw plain{0, static_cast<uint64_t>(0x43800000u)};  // (0, 0, 256.0, 0: PROT3W of 256.0)
    if (!u.negative(2, 0)) {  // 0x296810: the near LOD
        u.mini_lane(4, 1, u.lane(4, 1), u.lane(18, 2));
        spr.set32(rec + 0x1c, list[0]);
        list[0] = rec;
        u.add(3, 0, 0, vu0l::XYZ);
        u.max_lane(3, 3, u.lane(18, 3), u.lane(4, 1));
        const uint32_t inside = spr.u32(rec + 0x8);
        count[0] += 1;
        spr.setq(sp + 0x40, plain);
        if (inside == 0) {  // 0x296974
            list[0] = spr.u32(rec + 0x1c);
            const uint32_t packets = lwu(cls + 0x0);
            sp += 0x60;
            const uint32_t n = lbu(cls + 0x20);
            pkt += 0x60;
            spr.store_vf(u, 3, sp - 0x10);
            return clipped_record(rec, matrix, packets, n);
        }
        spr.store_vf(u, 3, sp + 0x50);
    } else if (u.negative(2, 2)) {  // 0x296850: the far LOD
        u.mini_lane(4, 1, u.lane(4, 1), u.lane(18, 2));
        spr.set32(rec + 0x1c, list[2]);
        list[2] = rec;
        u.add(3, 0, 0, vu0l::XYZ);
        u.max_lane(3, 3, u.lane(18, 3), u.lane(4, 1));
        const uint32_t inside = spr.u32(rec + 0x8);
        spr.setq(sp + 0x40, plain);
        count[2] += 1;
        spr.store_vf(u, 3, sp + 0x50);
        if (inside == 0) {  // 0x2969d4
            list[2] = spr.u32(rec + 0x1c);
            const uint32_t packets = lwu(cls + 0x8);
            sp += 0x60;
            const uint32_t n = lbu(cls + 0x22);
            pkt += 0x60;
            return clipped_record(rec, matrix, packets, n);
        }
    } else {  // 0x296778 / the fall-through: a morph, near to middle (y >= 0) or middle to far
        const int lod = u.negative(2, 1) ? 1 : 0;
        const int a = lod, b = lod + 1;  // (the lanes of the two distances)
        u.subbc(3, CLASS_LODS, u.lane(CLASS_LODS, a), b == 1 ? vu0l::Y : vu0l::Z);
        count[lod] += 1;
        u.sub(3, 0, 2, a == 0 ? vu0l::X : vu0l::Y);
        u.mini_lane(4, 1, u.lane(4, 1), u.lane(18, 2));
        spr.set32(rec + 0x1c, list[lod]);
        list[lod] = rec;
        u.div(u.lane(3, a), u.lane(3, b));  // the morph's weight
        u.max_lane(4, 3, u.lane(18, 3), u.lane(4, 1));
        morph_quads();
        u.add(3, 0, 0, vu0l::Y);
        u.mulbc(3, 5, u.q, vu0l::W);
        u.addbc(3, 0, u.q, vu0l::X);
        u.add(4, 0, 0, vu0l::XYZ);
        const uint32_t inside = spr.u32(rec + 0x8);
        u.subbc(3, 5, u.lane(3, 3), vu0l::Z);
        spr.store_vf(u, 4, sp + 0x50);
        if (inside == 0) {  // 0x296994 / 0x2969b4
            list[lod] = spr.u32(rec + 0x1c);
            const uint32_t packets = lwu(cls + 4 * static_cast<uint32_t>(lod));
            sp += 0x60;
            const uint32_t n = lbu(cls + 0x20 + static_cast<uint32_t>(lod));
            pkt += 0x60;
            spr.store_vf(u, 3, sp - 0x20);
            return clipped_record(rec, matrix, packets, n);
        }
        spr.store_vf(u, 3, sp + 0x40);
    }
    matrix_into_view(matrix);
}

// orig: (helper) 0x296c94 .. 0x296dcc / 0x297148 .. 0x2972bc -- a LOD packet's header in the chain: a REF of its first quadword,
// a REF of each of its parts (offsets at data + 0x10, from the class's data base), its texture's unpack, its second unpack (the
// clipping program's: `extra`), its colours, a FLUSH, its two vertex unpacks. Returns the next packet's data.
uint32_t Pass::packet_header(uint32_t p, uint32_t data, uint32_t base, bool extra) {
    uint32_t n = lbu(p + 0x4);
    uint32_t parts = data + 0x10;
    spr.setq(sp, tpl[0]);
    pkt += 0x10;
    spr.set32(sp + 4, data);
    sp += 0x10;
    uint32_t addr = 1;  // (the unpacks' VU1 addresses: 5 quadwords a part)
    do {
        const uint32_t off = lwu(parts);
        parts += 4;
        spr.setq(sp, tpl[0]);
        spr.set8(sp + 0x0, 5);
        spr.set8(sp + 0xe, 5);
        pkt += 0x10;
        spr.set16(sp + 0xc, addr);
        addr += 5;
        spr.set32(sp + 4, off + base);
        --n;
        sp += 0x10;
    } while (n != 0);
    const uint32_t tex = lbu(p + 0x7);
    spr.setq(sp, tpl[1]);
    addr += 0x4000;
    spr.set32(sp + 4, data + 0x20);
    spr.set8(sp, tex);
    spr.set8(sp + 0xe, tex << 2);
    pkt += 0x10;
    spr.set16(sp + 0xc, addr);
    sp += 0x10;
    if (extra) {
        const uint32_t at = lbu(p + 0xc), q = lbu(p + 0xd);
        pkt += 0x10;
        spr.setq(sp, tpl[7]);
        spr.set8(sp, q);
        spr.set8(sp + 0xe, q << 2);
        spr.set32(sp + 4, (at << 4) + data);
        sp += 0x10;
    }
    const uint32_t at = lbu(p + 0x8), q = lbu(p + 0x9);
    pkt += 0x10;
    spr.setq(sp, tpl[2]);
    spr.set8(sp, q);
    spr.set8(sp + 0xe, q << 1);
    spr.set32(sp + 4, (at << 4) + data);
    sp += 0x10;
    spr.setq(sp, Qw{});
    spr.set32(sp, 0x10000000u);
    pkt += 0x10;
    spr.set32(sp + 0xc, 0x14000006u);
    sp += 0x10;
    const uint32_t verts = lbu(p + 0xb);
    pkt += 0x20;
    const uint32_t vat = (lbu(p + 0xa) << 4) + data;
    const uint32_t half = (verts + 3) >> 2;
    spr.setq(sp, tpl[3]);
    spr.setq(sp + 0x10, tpl[3]);
    spr.set8(sp + 0xe, verts);
    spr.set8(sp + 0x1e, verts);
    spr.set8(sp, half);
    spr.set8(sp + 0x10, half);
    spr.set32(sp + 4, vat);
    spr.set32(sp + 0x14, vat + (half << 4));
    spr.set16(sp + 0x1c, 0x40f8);
    sp += 0x20;
    return lwu(p + 0x10);
}

// orig: (helper) 0x296dd4 .. 0x296e30 / 0x2972c0 .. 0x29731c -- an instance in the chain: a REF of its matrix and morph (its 0x60
// bytes at `matrix_pkt`), a REF of its colours (`colours`), the MSCAL; the double buffer's halves alternated (ITOP)
void Pass::instance_ref(uint32_t matrix_pkt, uint32_t colours) {
    pkt += 0x30;
    spr.setq(sp, tpl[4]);
    spr.set32(sp + 4, matrix_pkt);
    spr.setq(sp + 0x10, tpl[5]);
    spr.set32(sp + 0x14, colours);
    spr.set16(sp + 0x1c, parity + 0x4346);
    const bool full = (sp & 0xe00u) == 0xe00u;
    spr.setq(sp + 0x20, tpl[6]);
    sp += 0x30;
    parity ^= 0x40;
    if (full) flush();
}

// orig: (helper) 0x296bdc .. 0x296e94 -- the batch's lists: the class's counters back, then each LOD's packets with its instances
void Pass::lists() {
    sb(cls + 0x1c, count[0]);
    sb(cls + 0x1d, count[1]);
    sh(cls + 0x1e, count[2]);
    for (int i = 0; i < 7; ++i) tpl[i] = spr.q(TEMPLATES + 0x10 * static_cast<uint32_t>(i));
    t3_hi = tpl[3].hi;
    for (uint32_t k = 0; k < 3; ++k) {
        if (list[k] == 0) continue;
        uint32_t p = lwu(cls + 4 * k);
        const uint32_t end = p + (static_cast<uint32_t>(lbu(cls + 0x20 + k)) << 4);
        uint32_t data = lwu(p);
        spr.set32(LIST_HEAD, list[k]);
        while (p != end) {
            data = packet_header(p, data, lwu(cls + 0x2c), false);
            p += 0x10;
            for (uint32_t r = spr.u32(LIST_HEAD); r != 0;) {
                const uint32_t matrix_pkt = spr.u32(r + 0xc), colours = spr.u32(r + 0x10) + 0x40;
                r = spr.u32(r + 0x1c);
                instance_ref(matrix_pkt, colours);
            }
        }
    }
}

// orig: (helper) 0x296264 .. 0x296c88 -- a class's instances, 128 at a time
void Pass::batch(uint32_t src, uint32_t left) {
    while (left != 0) {
        const uint32_t limit = spr.u32(LOD_LIMIT);
        const uint32_t n = static_cast<uint32_t>(std::min(0x80, static_cast<int32_t>(left)));
        dma_to_spr(0x2000, src, n << 1);
        left -= n;
        spr.set32(BATCH_SRC, src);
        src += n << 5;
        const uint32_t end = RECORDS + (n << 5);
        list[0] = list[1] = list[2] = 0;
        used = 0;
        count[0] = lbu(cls + 0x1c);
        count[1] = lbu(cls + 0x1d);
        count[2] = lhu(cls + 0x1e);
        // 1. the test (0x2962d4)
        for (uint32_t r = RECORDS - 0x20;;) {
            const uint32_t lod_word = spr.u32(r + 0x34);
            r += 0x20;
            spr.load_vf(u, 1, r);  // (the sphere: loaded before the end test, the end's too)
            if (r == end) break;
            // vf4 = (0, the instance's LOD distance (its +0x14, limited), v0's upper words): QMTC2 of PMINW's result << 32
            const uint32_t lod = static_cast<uint32_t>(std::min(static_cast<int32_t>(lod_word), static_cast<int32_t>(limit)));
            v0_hi = min_words(v0_hi, t3_hi);
            u.set_words(4, 0, lod, lo32(v0_hi), hi32(v0_hi));
            if (lod == 0) {  // (0x2963f4: the record not arrived - the DMA's wait: the same record again)
                u.sub(2, 1, CAM, vu0l::XYZ);
                u.addbc(3, 0, u.lane(1, 3), vu0l::X);
                u.subbc(3, 0, u.lane(1, 3), vu0l::Y);
                r -= 0x20;
                continue;
            }
            test(r);
        }
        if (used == 0) continue;
        // 2. the visible ones (0x2965fc): a NEXT over their matrices, then each
        pkt += 0x10;
        spr.setq(sp, Qw{});
        spr.set32(sp, 0x20000000u);
        spr.set32(sp + 4, pkt + used);
        sp += 0x10;
        for (uint32_t r = RECORDS - 0x20;;) {
            const uint64_t state = static_cast<uint64_t>(spr.u32(r + 0x20)) | static_cast<uint64_t>(spr.u32(r + 0x24)) << 32;
            r += 0x20;
            spr.load_vf(u, 1, r);
            if (r == end) break;
            const uint32_t matrix = spr.u32(r + 0x10);
            if (state == 0) continue;
            lod_and_matrix(r, matrix);
        }
        // 3. the lists' packets (0x296bdc)
        lists();
    }
}

// orig: (helper) 0x2961fc .. 0x296ebc -- the classes
void Pass::classes() {
    for (;;) {
        const uint32_t left = spr.u32(CLASSES_LEFT), next = spr.u32(CLASS_NEXT);
        if (left == 0) return;
        cls = lwu(next);
        spr.set32(CLASSES_LEFT, left - 1);
        spr.set32(CLASS_NEXT, next + 4);
        const uint32_t flags = lhu(cls + 0x24);
        u.set_words(NEAREST, 0x50000000u, 0, lo32(v1_hi), hi32(v1_hi));  // (QMTC2 of v1: its upper half the last one's)
        const uint32_t src = lwu(cls + 0x28), n = lhu(cls + 0x26);
        ram_load_vf(u, CLASS_LODS, cls + 0x10);
        if ((flags & 9) == 0) {  // (bits 0 / 3: the class not drawn)
            sw(cls + 0x1c, 0);
            spr.set32(SWAY, (flags & 6) >> 1);
            batch(src, n);
        }
        // 0x296e98: the class's nearest depth into its list
        u.ftoi(NEAREST, NEAREST, vu0l::X, 1.0f);
        const uint32_t at = spr.u32(NEAREST_END);
        spr.set32(NEAREST_END, at + 4);
        spr.set32(at, u.word(NEAREST, 0));
    }
}

// orig: (helper) 0x296ec0 .. 0x297470 -- the chain's end; the clipped instances: their matrices by the clip matrix straight into
// the frame packet, then their packets with the clipping program, the instances of one LOD's packets together
void Pass::clipped() {
    end_tag();
    const uint32_t used_half = sp & 0xfffu;
    if (used_half != 0) sp = flush_packets(sp, pkt, used_half, 0x1000u);
    const uint32_t end = spr.u32(CLIPPED_END);
    parity = 0;
    if (end != CLIPPED) {
        program_call(0x0010ad10u);
        uint64_t hi[3];
        copy_packet(OV(0x001c2a20u), 9, hi);
        v0_hi = hi[0], v1_hi = hi[1], a0_hi = hi[2];
        for (int i = 0; i < 4; ++i) ram_load_vf(u, WORLD0 + i, OV(0x00166c80u) + 0xc0 + 0x10 * static_cast<uint32_t>(i));
        for (int i = 0; i < 8; ++i) tpl[i] = spr.q(TEMPLATES + 0x10 * static_cast<uint32_t>(i));
        t3_hi = tpl[3].hi;
        spr.set32(RESUME, OVF(0x00297098u));
        for (uint32_t r = CLIPPED; r != end;) {  // 0x297058
            const uint32_t word = spr.u32(r + 0x4);
            const uint32_t flag = word & 0xfu, matrix = word & ~0xfu;
            ram_load_vf(u, 4, matrix + 0x30);
            ram_load_vf(u, 1, matrix + 0x0);
            ram_load_vf(u, 2, matrix + 0x10);
            ram_load_vf(u, 3, matrix + 0x20);
            u.mulbc(1, 1, u.lane(4, 3), vu0l::XYZ);
            const uint32_t dst = spr.u32(r + 0x0);
            u.mulbc(2, 2, u.lane(4, 3), vu0l::XYZ);
            r += 0x10;
            u.mulbc(3, 3, u.lane(4, 3), vu0l::XYZ);
            u.sub(4, 4, CAM, vu0l::XYZ);
            if (flag != 0) sway(flag, matrix);  // (it returns through RESUME: 0x297098)
            u.rotate(1, WORLD0, WORLD0 + 1, WORLD0 + 2, 1, vu0l::XYZW);
            u.mulbc(4, 4, u.lane(CAM, 3), vu0l::XYZ);
            u.rotate(2, WORLD0, WORLD0 + 1, WORLD0 + 2, 2, vu0l::XYZW);
            u.rotate(3, WORLD0, WORLD0 + 1, WORLD0 + 2, 3, vu0l::XYZW);
            u.transform(4, WORLD0, WORLD0 + 1, WORLD0 + 2, WORLD0 + 3, 4, u.lane(0, 3));
            ram_store_vf(u, 1, dst + 0x00);
            ram_store_vf(u, 2, dst + 0x10);
            ram_store_vf(u, 3, dst + 0x20);
            ram_store_vf(u, 4, dst + 0x30);
        }
        for (uint32_t r = CLIPPED; r != end;) {  // 0x2970f8: a run of records with the same LOD packets
            const uint32_t word = spr.u32(r + 0xc);
            uint32_t p = spr.u32(r + 0x8);
            const uint32_t base = (word << 8) >> 4;
            const uint32_t stop = p + ((word >> 24) << 4);
            uint32_t data = lwu(p);
            const uint32_t first = r;
            uint32_t n = 0;
            for (;;) {
                const uint32_t next_packets = spr.u32(r + 0x18);
                r += 0x10;
                ++n;
                if (r == end || next_packets != p) break;
            }
            while (p != stop) {
                data = packet_header(p, data, base, true);
                p += 0x10;
                uint32_t q = first;
                int32_t k = static_cast<int32_t>(n);
                do {
                    const uint32_t colours = ((spr.u32(q + 0x4) >> 4) << 4) + 0x40;
                    --k;
                    const uint32_t matrix_pkt = spr.u32(q + 0x0);
                    q += 0x10;
                    instance_ref(matrix_pkt, colours);
                } while (k > 0);
            }
        }
        end_tag();
        flush_packets(sp, pkt, sp & 0xfffu, 0x1000u);
    }
    spr.set16(spr.u32(RELIGHT_END), 0xffff);
    sw(OV(0x001611c0u), pkt);
}

}  // namespace

// orig: 0x00295fa0
bool tie_cull() {
    if (!vu0l::usable()) return false;
    Pass p;
    p.u.load();
    p.setup();
    p.classes();
    p.clipped();
    p.u.store();
    return true;
}

}  // namespace rac1::world_hand
