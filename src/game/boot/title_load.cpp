// The title's scene loaded: the boot module's own code (SCUS_971.99, its own image,
// see front_image.h), ported at its own addresses (`// orig: front:0x...`); the engine code it
// shares with the levels is the ports' (src/game/ovl), relocated onto the boot module
// (ovl_reloc_set_level(OVL_FRONT)). The addresses below are the boot module's. See

#include <game/boot/title.h>
#include <game/enhance/corrected_texts.h>

#include <game/lib/libgraph.h>
#include <game/math/vecmath.h>
#include <game/ovl/world_lighting.h>
#include <game/ovl/arrival.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/lib/snd989.h>
#include <game/port_stubs.h>

#include <game/ovl/ovl_reloc.h>
namespace rac1 {

namespace {
constexpr uint32_t TOC = 0x00137b80;  // the disc's table of contents (base ELF)
constexpr FrConst<0x001940c0u> LAYOUT{};  // the level memory map (the overlays' 0x173e00)
constexpr FrConst<0x0018cb20u> SCENE{};   // the scene (cutscene) state (the overlays' 0x16c860)

// orig: (helper) the loader's 64-byte alignment of its allocation pointer
uint32_t align64(uint32_t a) { return (a + 0x3f) & 0xffffffc0u; }
// orig: (helper) the four quadwords of an instance matrix (lq / sq)
void copy_mat(uint32_t d, uint32_t s) {
    for (uint32_t o = 0; o < 0x40; o += 0x10) lq_sq(d + o, s + o);
}
}  // namespace

// The texts of language `lang` (the title's language block read at 0x15ef60, its table at
// 0x15ef64): their count to 0x1996fc, their base to 0x15f6a0 (the overlays' 0x17943c /
// 0x15f660).
// orig: front:0x001eaf50
void title_texts(int32_t lang) {
    const uint32_t row = lwu(FR(0x0015ef64)) + static_cast<uint32_t>(lang) * 4;
    const uint32_t b = lwu(FR(0x0015ef60)) + lwu(row);
    sw(FR(0x001996fc), lwu(b));
    sw(FR(0x0015f6a0), b + 8);
}

// The scene's instances (the title's gameplay data, cut down from the levels' 0x241940): the
// hero / camera state cleared, the frame rate, the level's settings (background colour, fog,
// light), the directional lights, the tfrag entries' lighting, the tie and shrub instances
// with their static lighting, then an empty moby list (the scene spawns its mobys), the
// particles' block. Allocates from *(0x1940d8) upwards; returns the end.
// orig: front:0x001e9b10
uint32_t title_instances(uint32_t p) {
    uint32_t s8 = lwu(LAYOUT + 0x18);
    mem_fill32(0x0013f350, 0, 0x2310);
    mem_fill32(FR(0x00186f40), 0, 0x3a0);
    mem_fill32(FR(0x001aaa40), 0, 0x180);
    const float sc = lwc1(0x0015ed60);
    if (lw(0x0015ed80) != 0) {
        if (sc == 1.0f) frame_rate_set(1);
    } else if (!(sc == 1.0f)) {
        frame_rate_set(0);
    }
    sb(FR(0x0016034c), lbu(0x0015ed80));
    sw(FR(0x00194108), FR(0x00194180));
    sw(FR(0x00194104), FR(0x001941c0));
    sw(FR(0x00194114), 0);
    sw(FR(0x0019410c), 0);
    sw(FR(0x00194110), 0);
    mem_clear128(FR(0x001b76e0), 0x10000);
    mem_clear128(FR(0x001c76e0), 0x60);
    // the settings
    const uint32_t S = FR(0x0018cd00u);
    const uint32_t st = p + lwu(p);
    sw(S + 0x23c, lwu(st));
    sw(S + 0x240, lwu(st + 4));
    sw(S + 0x244, lwu(st + 8));
    sb(FR(0x0015f484), lbu(st + 0xc));
    sb(FR(0x0015f485), lbu(st + 0x10));
    sb(FR(0x0015f486), lbu(st + 0x14));
    sw(FR(0x0015f488), lwu(st + 0x18));  // (lwc1 / swc1: the bits)
    sw(FR(0x0015f48c), lwu(st + 0x1c));
    sw(FR(0x0015f490), lwu(st + 0x20));
    sw(FR(0x0015f494), lwu(st + 0x24));
    fog_setup();
    sw(FR(0x00160ec0), 0x48fa0000u);
    sw(FR(0x00160f70), 0x44340000u);
    sw(FR(0x001603e4), 0x43fa0000u);
    sw(FR(0x0015ff30), 500);
    sw(FR(0x001600bc), FR(0x001f4000u));
    bg_color_set(lwu(S + 0x23c), lwu(S + 0x240), lwu(S + 0x244));
    vif0_send(0x00100ae0);  // the VU0 micro programs
    mem_clear128(FR(0x0019c1c0), 0x100);
    mem_clear128(FR(0x0019c3c0), 0x180);
    mem_clear128(FR(0x0019bdc0), 0x400);
    {  // the directional lights, at most 12
        uint32_t q = p + lwu(p + 4);
        int32_t n = lw(q);
        q += 0x10;
        if (!(n < 0xc)) {
            debug_printf_stub();
            n = 0xc;
        }
        if (n != 0) mem_copy(FR(0x0019bdc0), q, n << 6);
    }
    // the tfrag entries (0x160e8c / 0x160e90: the overlays' 0x160f4c / 0x160f50): no light set yet, then their static
    // lighting in batches of 0x3ff
    if (lw(FR(0x00160e90)) > 0) {
        uint32_t a = lwu(FR(0x00160e8c)) + 0x36;
        for (int32_t k = lw(FR(0x00160e90)); k != 0; --k, a += 0x40) sh(a, 0xffff);
    }
    if (lw(FR(0x00160e90)) > 0) {
        int32_t s0 = 0;
        do {
            uint32_t v = 0x70003000u;
            int32_t a = 0;
            if (s0 < lw(FR(0x00160e90))) {
                sh(v, static_cast<uint32_t>(s0));
                for (;;) {
                    ++a;
                    v += 2;
                    const bool room = a < 0x3ff;
                    ++s0;
                    if (!room || !(s0 < lw(FR(0x00160e90)))) break;
                    sh(v, static_cast<uint32_t>(s0));
                }
            }
            sh(v, 0xffff);
            world_tfrag_lighting(0x70003000u);  // (front: 0x234f98)
        } while (s0 < lw(FR(0x00160e90)));
    }
    {  // the tie instances: 0x20-byte bounds at 0x160f50, 0x1c0-byte data at 0x160f58
        uint32_t s4 = p + lwu(p + 0x34);
        const uint32_t nt = lwu(s4);
        s4 += 0x10;
        sw(FR(0x00160f5c), nt);
        sw(FR(0x00160f50), s8);
        s8 += nt << 5;
        if (nt != 0) mem_fill32(lwu(FR(0x00160f50)), 0, static_cast<int32_t>(nt << 5));
        const uint32_t nt2 = lwu(FR(0x00160f5c));
        const uint32_t d = align64(s8);
        sw(FR(0x00160f58), d);
        s8 = d + nt2 * 0x1c0;
        if (nt2 != 0) mem_fill32(lwu(FR(0x00160f58)), 0, static_cast<int32_t>(nt2 * 0x1c0));
        uint32_t last = 0xffffffffu;
        sw(FR(0x00160f54), lwu(FR(0x00160f50)) + lwu(FR(0x00160f5c)) * 32);
        if (lw(FR(0x00160f5c)) > 0) {
            const uint32_t HDR = FR(0x001e1700u);  // the tie classes' headers by index
            int32_t s7 = 0;
            do {
                const uint32_t cls = lwu(s4);
                const uint32_t s6 = static_cast<uint32_t>(s7) * 0x1c0;
                const uint32_t s3 = lwu(FR(0x00160f50)) + static_cast<uint32_t>(s7) * 32;
                const uint32_t ti = lbu(cls + FR(0x001e1a00));
                sb(s3 + 0x1a, ti);
                const uint32_t s5 = lwu(FR(0x00160f58)) + s6;
                if ((ti & 0xff) != last) {
                    sw(lwu(HDR + ti * 4) + 0x28, s3);
                    sh(lwu(HDR + lbu(s3 + 0x1a) * 4u) + 0x26, 0);
                    last = lbu(s3 + 0x1a);
                }
                const auto hdr = [s3, HDR] { return lwu(HDR + lbu(s3 + 0x1a) * 4u); };
                sh(hdr() + 0x26, lhu(hdr() + 0x26) + 1u);
                sw(s3 + 0x10, s5);
                const float dist = static_cast<float>(lw(s4 + 4));
                sh(s3 + 0x1e, 0xffff);
                sb(s3 + 0x1b, 0);
                swc1(s3 + 0x14, dist);
                sh(s3 + 0x1c, 0);
                const uint32_t w0c = lwu(s4 + 0xc);
                copy_mat(s5, s4 + 0x10);
                sh(s3 + 0x18, w0c);
                s4 += 0x50;
                ++s7;
                swc1(s5 + 0x3c, lwc1(hdr() + 0x40));
                mat3_transform(V(s3), V(hdr() + 0x30), M(s5));
                float m = max_f(vec3_length(V(s5)), vec3_length(V(s5 + 0x10)));
                m = max_f(m, vec3_length(V(s5 + 0x20)));
                swc1(s3 + 0xc, lwc1(hdr() + 0x3c) * m);
                vec4_scale(V(s3), V(s3), lwc1(s5 + 0x3c));
                vec3_add(V(s3), V(s3), V(s5 + 0x30));
                swc1(lwu(FR(0x00160f58)) + s6 + 0xc, fpu_div(1.0f, vec3_length(V(lwu(FR(0x00160f58)) + s6))));
                swc1(lwu(FR(0x00160f58)) + s6 + 0x1c, fpu_div(1.0f, vec3_length(V(lwu(FR(0x00160f58)) + s6 + 0x10))));
                swc1(lwu(FR(0x00160f58)) + s6 + 0x2c, fpu_div(1.0f, vec3_length(V(lwu(FR(0x00160f58)) + s6 + 0x20))));
                mem_copy(s5 + 0x140, s4, 0x80);
                s4 += 0x80;
                sh(s3 + 0x1c, lhu(s4));
                s4 += 0x10;
            } while (s7 < lw(FR(0x00160f5c)));
        }
        uint32_t v = 0x70000000u;
        for (int32_t i = 0; i < lw(FR(0x00160f5c)); ++i, v += 2) sh(v, static_cast<uint32_t>(i));
        sh(v, 0xffff);
        world_tie_lighting(0x70000000u);  // (front: 0x237370)
    }
    uint32_t moby_end;  // the moby block's end (the stack slot +0x10)
    {  // the shrub instances: bounds at 0x1603d4, data (0x40 each) at 0x1603dc, colours (0x60 each) at 0x1603e0
        uint32_t s6 = p + lwu(p + 0x3c);
        const uint32_t ns = lwu(s6);
        s6 += 0x10;
        sw(FR(0x001603d0), ns);
        sw(FR(0x001603d4), s8);
        s8 += ns << 5;
        if (ns != 0) mem_fill32(lwu(FR(0x001603d4)), 0, static_cast<int32_t>(ns << 5));
        const uint32_t ns2 = lwu(FR(0x001603d0));
        const uint32_t d = align64(s8);
        sw(FR(0x001603dc), d);
        s8 = d + (ns2 << 6);
        if (ns2 != 0) mem_fill32(lwu(FR(0x001603dc)), 0, static_cast<int32_t>(ns2 << 6));
        const uint32_t ns3 = lwu(FR(0x001603d0));
        sw(FR(0x001603e0), s8);
        s8 = align64(s8 + ns3 * 0x60);
        if (ns3 != 0) mem_fill32(lwu(FR(0x001603e0)), 0, static_cast<int32_t>(ns3 * 0x60));
        uint32_t last = 0xffffffffu;
        sw(FR(0x00160ab4), lwu(FR(0x001603e0)));
        sw(FR(0x001603d8), lwu(FR(0x001603d4)) + lwu(FR(0x001603d0)) * 32);
        moby_end = s8 + 0x4000;
        const uint32_t HDR = FR(0x001d7f30u);  // the shrub classes' headers by index
        if (lw(FR(0x001603d0)) > 0) {
            int32_t i = 0;
            do {
                const uint32_t s5 = lwu(FR(0x001603d4)) + static_cast<uint32_t>(i) * 32;
                const uint32_t s7 = lwu(FR(0x001603dc)) + static_cast<uint32_t>(i) * 64;
                const uint32_t si = lbu(lwu(s6) + FR(0x001d80b0));
                sb(s5 + 0x1a, si);
                if (si != last) {
                    last = si;
                    sw(lwu(HDR + si * 4) + 0x18, s5);
                    sh(lwu(HDR + si * 4) + 0x16, 0);
                }
                const uint32_t s3 = HDR + si * 4;
                sh(lwu(s3) + 0x16, lhu(lwu(s3) + 0x16) + 1u);
                sh(s5 + 0x18, static_cast<uint32_t>(i));
                const float dist = lwc1(s6 + 4);
                swc1(s5 + 0x10, dist);
                if (dist < 16.0f) swc1(s5 + 0x10, 16.0f);
                sb(s5 + 0x1b, 0);
                sh(s5 + 0x1e, 0xffff);
                const uint32_t bb = lwu(lwu(s3) + 0x1c);
                if (bb != 0) {
                    const int32_t r = float_to_int(lwc1(bb));
                    sb(s5 + 0x17, static_cast<uint32_t>(r));
                    const float f = int_to_float(r & 0xff) + 24.0f;
                    if (lwc1(s5 + 0x10) < f) swc1(s5 + 0x10, f);
                }
                copy_mat(s7, s6 + 0x10);
                s6 += 0x50;
                ++i;
                swc1(s7 + 0x3c, lwc1(lwu(s3) + 0x20));
                float a = vec3_length(V(s7));
                a = (a + vec3_length(V(s7 + 0x10))) * 0.5f;
                const float b = vec3_length(V(s7 + 0x20));
                int32_t r0 = float_to_int(a * 4096.0f);
                int32_t r1 = float_to_int(b * 4096.0f);
                if (0x10000 < r1) r1 = 0x10000;
                if (0x10000 < r0) r0 = 0x10000;
                sw(s7 + 0x2c, static_cast<uint32_t>(r0) | static_cast<uint32_t>(r1) << 16);
                const uint32_t c0 = lwu(s6), c1 = lwu(s6 + 4), c2 = lwu(s6 + 8);
                s6 += 0x10;
                sw(s7 + 0xc, c2 << 16 | (c1 << 8 | 0x80000000u) | c0);
                sh(s5 + 0x1c, lhu(s6));
                s6 += 0x10;
                mat3_transform(V(s5), V(lwu(s3)), M(s7));
                float m = max_f(vec3_length(V(s7)), vec3_length(V(s7 + 0x10)));
                m = max_f(m, vec3_length(V(s7 + 0x20)));
                swc1(s5 + 0xc, lwc1(lwu(s3) + 0xc) * m);
                vec4_scale(V(s5), V(s5), lwc1(s7 + 0x3c));
                vec3_add(V(s5), V(s5), V(s7 + 0x30));
            } while (i < lw(FR(0x001603d0)));
        }
        uint32_t v = 0x70000000u;
        for (int32_t k = 0; k < lw(FR(0x001603d0)); ++k, v += 2) sh(v, static_cast<uint32_t>(k));
        sh(v, 0xffff);
        world_shrub_lighting(0x70000000u);  // (front: 0x22a5e0, the overlays' 0x288908: the same code)
        for (int32_t k = 0; k < lw(FR(0x001603d0)); ++k) {
            uint32_t a = lwu(FR(0x001603e0)) + static_cast<uint32_t>(k) * 0x60;
            int32_t r = 0, g = 0, bl = 0;
            for (int32_t n = 0; n < 24; ++n, a += 4) {
                r += lbu(a);
                g += lbu(a + 1);
                bl += lbu(a + 2);
            }
            const uint32_t e = static_cast<uint32_t>(k) * 64 + lwu(FR(0x001603dc));
            sw(e + 0x1c, static_cast<uint32_t>(bl / 24) << 16 | static_cast<uint32_t>(g / 24) << 8 | static_cast<uint32_t>(r / 24));
        }
    }
    // the moby list (empty: the scene spawns its mobys), 0x40 slots of 0x100; the particles
    mem_fill32(s8, 0, 0x4000);
    sw(FR(0x0015ff18), s8);
    s8 = moby_end;
    sw(FR(0x0015ff1c), lwu(FR(0x0015ff18)));
    sb(lwu(FR(0x0015ff18)) + 0x20, 0xff);
    sw(FR(0x0015ff28), s8);
    sw(FR(0x001600ac), s8 + 0x2000);
    sw(FR(0x0015ff20), lwu(FR(0x0015ff18)) + 0x3f00);
    s8 = s8 + 0x2000 + 0x20000;
    sw(FR(0x001600b4), 0xffffffffu);
    sw(FR(0x001600b0), 0);
    sw(FR(0x001600b8), 0);
    mem_clear128(FR(0x001cd780), 0x200);
    for (uint32_t m = lwu(FR(0x0015ff18)); m != lwu(FR(0x0015ff20)); m += 0x100) sh(m + 0x36, 0x7f80);
    for (uint32_t t = lwu(FR(0x00160f50)); t != lwu(FR(0x00160f54)); t += 0x20) sh(t + 0x18, 0x7f80);
    if (lw(FR(0x00160e90)) > 0) {
        uint32_t a = lwu(FR(0x00160e8c)) + 0x3a;
        for (int32_t k = lw(FR(0x00160e90)); k != 0; --k, a += 0x40) sh(a, 0x7f80);
    }
    sw(FR(0x0018c318) + 0x1c, 0);
    sub_001f83d0();
    sub_00266d48();
    sub_001fb028();
    sw(FR(0x0015f60c), 0);
    swc1(FR(0x0015f43c), 1.0f);
    return s8;
}

// The title's level: the memory map and its tables, the screen and projection, its data
// (table of contents +0x14e8: 2100 sectors) read into the top of memory and decompressed, the
// textures to the GS, the logo's TEX0 (0x19e6c0), the class texture tables, the moby / tie /
// shrub classes, the sky, the logo image (256 x 128) to the GS, the instances (0x1e9b10), the
// scene's blocks (the cutscene the title plays: Ratchet and Clank's animated scene) and its first
// block, the language block (+0x1528) and the texts, the sounds of class 0x472.
// orig: front:0x001ea830
void title_load() {
    sw(FR(0x0015ef58), 0);
    sub_002a07c8();
    memory_layout();
    sw(FR(0x00160f0c), 0x00100000u);
    sw(0x0015ee74, 0x002c0000u);
    sw(0x0015ee8c, 0x002c0000u);
    sw(0x0015ee78, 0x002c0000u);
    mem_fill32(FR(0x00194180), 0x87654321u, 0x10);
    mem_fill32(FR(0x001b3ac0), 0xffffffffu, 0x800);
    mem_fill32(FR(0x001b6880), 0xffffffffu, 0xe00);
    mem_fill32(FR(0x001b6180), 0, 0xe0);
    screen_setup();
    projection_setup();
    sub_002a07f8();
    stream_disc_read(lwu(LAYOUT + 0x14) + 0x01000000u, lwu(TOC + 0x14e8), lwu(TOC + 0x14ec));
    stream_reads_wait(1);
    // (FlushCache around the decompression: no memory effect)
    const int32_t size = wad_decompress(lwu(LAYOUT + 0x14) + 0x01000000u, lwu(LAYOUT + 0x14));
    const uint32_t H = lwu(LAYOUT + 0x14);
    gs_ram_upload(H + lwu(H), lw(H + 8), H + lwu(H + 0xc));
    {  // the logo's TEX0 (and its two companion words)
        const uint32_t t = lwu(0x0015ee8c);
        const uint64_t tbp = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t + lwu(H + 0x70)) >> 8));
        const uint64_t cbp = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t + lwu(H + 0x74)) >> 8));
        sd(FR(0x0019e6c0), tbp | 0x1d308000ull | (cbp << 37 | 0x5c0000000ull) | 0x8000000000000000ull);
        sd(FR(0x0019e6d0), 0x0040000400004000ull);
        sd(FR(0x0019e6c8), 0x0000ffa0000000e0ull);
    }
    const uint32_t s5 = H + lwu(H + 4);
    const uint32_t s4 = s5 + lwu(H + 0x60);
    // the texture tables: 16-byte records (offset, size at +4), each the texture's address and
    // its size's log2 in the top bits
    const struct { uint32_t n, list, count, table; } tabs[4] = {
        {0x30, 0x34, FR(0x00160e94), FR(0x001e0900)}, {0x38, 0x3c, FR(0x0015ff08), FR(0x001b5980)}, {0x40, 0x44, FR(0x00160f64), FR(0x001e2600)}, {0x48, 0x4c, FR(0x001603ec), FR(0x001d84b0)}};
    for (const auto& t : tabs) {
        const int32_t n = lw(H + t.n);
        uint32_t r = H + lwu(H + t.list);
        sw(t.count, static_cast<uint32_t>(n));
        if (n <= 0) continue;
        uint32_t dst = t.table;
        for (int32_t i = 0; i < lw(t.count); ++i, r += 0x10, dst += 4) {
            const int32_t l = highest_bit_index(lhu(r + 4));
            sw(dst, s4 + lwu(r) + (static_cast<uint32_t>(l) << 28));
        }
    }
    moby_lod_set(s5 + lwu(H + 0x10), H + lwu(H + 0x34));  // (the tfrag block and its texture table: 0x241458 sets up the tfrags)
    sub_0023f848(s5 + lwu(H + 0x14));
    sw(FR(0x0015ff00), 0);
    sw(FR(0x00160f4c), 0);
    sw(FR(0x001603cc), 0);
    {  // the classes: mobys (0x20 each), ties (0x20), shrubs (0x30)
        uint32_t r = H + lwu(H + 0x1c);
        for (int32_t i = 0; i < lw(H + 0x18); ++i, r += 0x20) {
            const uint32_t off = lwu(r);
            moby_class_register(off != 0 ? s5 + off : 0, H + lwu(H + 0x3c), r + 0x10, lwu(r + 4));
        }
        r = H + lwu(H + 0x24);
        for (int32_t i = 0; i < lw(H + 0x20); ++i, r += 0x20) tie_class_register(s5 + lwu(r), H + lwu(H + 0x44), r + 0x10, lwu(r + 4));
        r = H + lwu(H + 0x2c);
        for (int32_t i = 0; i < lw(H + 0x28); ++i, r += 0x30) shrub_class_register(s5 + lwu(r), H + lwu(H + 0x4c), r + 0x10, r + 0x20, lwu(r + 4));
    }
    sw(FR(0x0015f460), s5 + lwu(H + 0x68));
    sub_0023f768(H + lwu(H + 0x5c), lw(H + 0x58));
    sub_0023f630(H + lwu(H + 0x6c), static_cast<int32_t>(s5 + lwu(H + 0x64)), H + lwu(H + 0x54), lw(H + 0x50));
    {  // the logo's image (256 x 128, PSMCT32) to the GS at the texture top
        const uint32_t lp = ee_stack_alloc(0x60);
        gs_set_def_load_image(lp, static_cast<int16_t>(static_cast<uint32_t>(lw(0x0015ee74)) >> 8), 4, 0, 0, 0, 0x100, 0x80);
        gs_exec_load_image(lp, s5 + lwu(H + 0x84));  // (after FlushCache)
        gs_sync_path(0, 0);
        ee_stack_free(0x60);
    }
    sw(LAYOUT + 0x18, lwu(LAYOUT + 0x14) + static_cast<uint32_t>(size));
    {  // (the logo's TEX0 for the frame's sprite)
        const uint32_t t = lwu(0x0015ee74);
        const uint64_t tex0 = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t) >> 8)) | 0x20010000ull | 0x5c0000000ull;
        sw(0x0015ee78, t + 0x20000);
        sw(0x0015ee74, t + 0x20000);
        sd(FR(0x0015ef48), tex0);
    }
    sw(LAYOUT + 0x1c, title_instances(s5 + lwu(H + 0x7c)));
    mem_fill32(SCENE, 0, 0x1c0);
    mem_fill32(FR(0x00186310), 0, 0x40);
    {  // (the scene's two buffers)
        const uint32_t a = lwu(LAYOUT + 0x1c);
        sw(SCENE + 0x58, a);
        sw(SCENE + 0x5c, a + 0x40000);
        sw(LAYOUT + 0x1c, a + 0x80000);
    }
    {  // the scene's blocks (offset, size): up to 0x46, each at its offset + 0x800
        uint32_t e = s5 + lwu(H + 0x80);
        if (lw(e + 4) != 0) {
            uint32_t dst = SCENE + 0x60;
            int32_t k = 0;
            for (;;) {
                const uint32_t off = lwu(e);
                ++k;
                e += 8;
                sw(dst, s5 + lwu(H + 0x80) + off + 0x800);
                if (!(k < 0x46)) break;
                dst += 4;
                if (lw(e + 4) == 0) break;
            }
        }
    }
    scene_block_load(0);
    {  // the language block: the texts of each of the 8 languages relocated
        const uint32_t buf = lwu(LAYOUT + 0x1c);
        sw(FR(0x0015ef60), buf);
        disc_read_wait(buf, lwu(TOC + 0x1528), lwu(TOC + 0x152c));
        const uint32_t base = lwu(FR(0x0015ef60));
        sw(FR(0x0015ef64), base);
        sw(LAYOUT + 0x1c, base + (lwu(TOC + 0x152c) << 11));
        for (int32_t lang = 0; lang < 8; ++lang) {
            title_texts(lang);
            if (lw(FR(0x001996d0) + 0x2c) > 0) {
                const uint32_t b = lwu(FR(0x0015f6a0));
                uint32_t e = b;
                for (int32_t i = 0; i < lw(FR(0x001996d0) + 0x2c); ++i, e += 0x10) sw(e, lwu(e) + (b - 8));
            }
            corrected_texts_title_patch(lwu(FR(0x0015f6a0)), lw(FR(0x001996d0) + 0x2c), lang);  // enhancement (off: nothing)
        }
        title_texts(0);
    }
    {  // class 0x472's sounds: the title's sound table 0x1861e0, 5 of them
        const uint32_t row = static_cast<uint32_t>(lbu(FR(0x001b3ac0) + 0x472)) * 4 + FR(0x001b3200);
        sw(lwu(row) + 0x28, FR(0x001861e0));
        sb(lwu(row) + 0xd, 5);
    }
}

}  // namespace rac1
