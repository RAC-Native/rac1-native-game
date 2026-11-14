// Ported overlay code: the gameplay data loader of the level setup (0x241940).
#include <game/enhance/corrected_texts.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/world_lighting.h>
#include <host/level_slots.h>
#include <host/mod_code.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/enhance/session.h>
#include <game/level/level_registry.h>
#include <game/enhance/sections.h>
#include <array>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/port_stubs.h>
#include <runtime/kernel/disc.h>

namespace rac1 {

namespace {
// orig: (helper) the loader's 64-byte alignment of its allocation pointer
uint32_t align64(uint32_t a) { return (a + 0x3f) & 0xffffffc0u; }
// orig: (helper) the bit index word of the LOD / visibility masks: ((v >> 3) << 8) | 1 << (v & 7)
uint32_t mask_bit(int32_t v) { return static_cast<uint32_t>((v >> 3) << 8) | (1u << (static_cast<uint32_t>(v) & 7)); }
// orig: (helper) one pvar block (index `i` of the table `tab`, data `data`) moved to `s8`
uint32_t pvar_move(uint32_t slot, uint32_t tab, uint32_t data, uint32_t s8) {
    const int32_t i = lw(slot);
    if (i < 0) {
        sw(slot, 0);
        return s8;
    }
    const uint32_t t = static_cast<uint32_t>(i) * 8 + tab;
    const int32_t size = lw(t + 4);
    sw(slot, s8);
    mem_copy128(s8, data + lwu(t), size);
    sw(t, s8);
    return s8 + static_cast<uint32_t>(size);
}
// orig: (helper) the index of `id` among the n (8-byte, id at +4) records at `p`, n if absent
int32_t find_id(uint32_t p, int32_t n, int32_t id) {
    int32_t k = 0;
    if (n <= 0) return 0;
    if (lw(p + 4) == id) return 0;
    uint32_t a = p + 4;
    k = 1;
    for (;;) {
        if (!(k < n)) return k;
        a += 8;
        if (lw(a) == id) return k;
        ++k;
    }
}
// orig: (helper) the four quadwords of an instance matrix
void copy_mat(uint32_t d, uint32_t s) {
    for (uint32_t o = 0; o < 0x40; o += 0x10) lq_sq(d + o, s + o);
}
}  // namespace

// A disc read that waits until done: `n` sectors from `lba` to `dst` (the request recorded at
// 0x1516dc..e4, the busy flag 0x1516d8 clear again when it returns). Returns the byte count.
// The host's ISO read stands in for sceCdRead and the IOP's completion.
// orig: 0x00266040 (host: the read is served from the ISO)
int32_t disc_read_wait(uint32_t dst, uint32_t lba, uint32_t n) {
    if (lh(0x001516d8) != 0 || n == 0) return 0;
    sw(0x001516dc, lba);
    sw(0x001516e0, n);
    sw(0x001516e4, dst);
    if (!disc_read_ee(dst, lba, n)) debug_trap();
    sh(0x001516d8, 0);
    return static_cast<int32_t>(n << 11);
}

// The level's gameplay data: (level != 0: already in memory, else read from the disc to
// *(0x173e18)) decompressed to *(0x173e04) (H); the hero / camera / effect state cleared; the
// frame rate, fog, background colour and lights; the tie and shrub instances (bounds, matrices,
// colours) with their static lighting; the cuboids, the 0x90-byte records, the mobys (spawned
// by their spawn rules and the level's kill masks), their pvars and the pvar pointers; the
// paths, the grind lines, the light grid, the ship, the LOD / visibility masks; then the
// effects reset, hero_init, the hero reset and the cameras. Allocates from *(0x173e18) upwards
// and returns the end.
// orig: 0x00241940
uint32_t gameplay_load(int32_t level) {
    level_slots_level_start(lw(0x0015ed84));  // enhancement: a level mod's level records in or out (src/host/level_slots.h)
    mod_code_level_load();                     // enhancement: the mods' code: loaded, the level before ended (src/host/mod_code.h)
    const uint32_t H = lwu(OV(0x00173e04));
    uint32_t s8 = lwu(OV(0x00173e18));
    if (level == 0) {
        if (lw(0x0015ed80) != 0) {
            disc_read_wait(s8, lwu(0x0013a4f8), lwu(0x0013a4fc));
        } else {
            disc_read_wait(s8, lwu(0x0013a4f0), lwu(0x0013a4f4));
        }
        sw(0x0015ee50, s8);
    }
    // 0x118a80 (sceGsSyncPath) before and after: the GS, no memory effect
    wad_decompress(lwu(0x0015ee50), H);
    mem_fill32(0x0013f350, 0, 0x2310);
    mem_fill32(OV(0x00166c80), 0, 0x3a0);
    mem_fill32(OV(0x0018eb40), 0, 0x180);
    libc_srand(0x4d2);
    const float sc = lwc1(0x0015ed60);
    if (lw(0x0015ed80) != 0) {
        if (sc == 1.0f) frame_rate_set(1);
    } else if (!(sc == 1.0f)) {
        frame_rate_set(0);
    }
    sw(OV(0x00173e48), OV(0x00173ec0));
    sw(OV(0x00173e44), OV(0x00173f00));
    sw(OV(0x00173e54), 0);
    sw(OV(0x00173e4c), 0);
    sw(OV(0x00173e50), 0);
    mem_clear128(OV(0x0019b7e0), 0x10000);
    mem_clear128(OV(0x001ab7e0), 0x60);
    // the level's settings
    constexpr OvConst<0x0016ca40u> S{};
    const uint32_t st = H + lwu(H);
    sw(S + 0x23c, lwu(st));
    sw(S + 0x240, lwu(st + 4));
    sw(S + 0x244, lwu(st + 8));
    sb(OV(0x0015f444), lbu(st + 0xc));
    sb(OV(0x0015f445), lbu(st + 0x10));
    sb(OV(0x0015f446), lbu(st + 0x14));
    const uint32_t ship = st + 0x2c;  // the ship's position and yaw
    sw(OV(0x0015f448), lwu(st + 0x18));
    sw(OV(0x0015f44c), lwu(st + 0x1c));
    sw(OV(0x0015f450), lwu(st + 0x20));
    sw(OV(0x0015f454), lwu(st + 0x24));
    sw(OV(0x0015f638), lwu(st + 0x28));
    sw(0x0013e060, lwu(st + 0x3c));
    sw(0x0013e064, lwu(st + 0x40));
    sw(0x0013e068, lwu(st + 0x44));
    fog_setup();
    sw(OV(0x00160f80), 0x48fa0000u);
    sw(OV(0x00160fe0), 0x44340000u);
    sw(OV(0x001604a4), 0x43fa0000u);
    sw(OV(0x0015fff0), 500);
    sw(OV(0x0016017c), NOT_ADDR(0x1f4000));
    bg_color_set(lwu(S + 0x23c), lwu(S + 0x240), lwu(S + 0x244));
    vif0_send(0x00100ae0);  // the VU0 micro programs
    mem_clear128(OV(0x001802c0), 0x100);
    mem_clear128(OV(0x001804c0), 0x180);
    mem_clear128(OV(0x0017fec0), 0x400);
    {  // the directional lights, at most 12
        uint32_t p = H + lwu(H + 4);
        int32_t n = lw(p);
        p += 0x10;
        if (!(n < 0xc)) {
            debug_printf_stub();
            n = 0xc;
        }
        if (n != 0) mem_copy(OV(0x0017fec0), p, n << 6);
    }
    // the light grid
    if (lw(H + 0x80) == 0) {
        sw(OV(0x0015fbc4), 0);
    } else {
        uint32_t p = H + lwu(H + 0x80);
        const int32_t c = lw(p);
        p += 0x10;
        sw(OV(0x0015fbc4), static_cast<uint32_t>(c));
        if (c != 0) {
            mem_copy(OV(0x0017ecc0), p, c << 4);
            const int32_t c2 = lw(OV(0x0015fbc4));
            mem_copy(OV(0x0017eec0), p + static_cast<uint32_t>(c2) * 16, c2 << 7);
            if (lw(OV(0x0015fbc4)) > 0) {
                uint32_t e = OV(0x0017eec0);
                for (int32_t k = lw(OV(0x0015fbc4)); k != 0; --k, e += 0x80) {
                    if (lw(e + 0x48) < 0) sw(e + 0x48, 0xb);
                    if (lw(e + 0x4c) < 0) sw(e + 0x4c, 0xb);
                }
            }
        }
    }
    // the moby LOD entries: no light set yet, then their static lighting in batches of 0x3ff
    if (lw(OV(0x00160f50)) > 0) {
        uint32_t a = lwu(OV(0x00160f4c)) + 0x36;
        for (int32_t k = lw(OV(0x00160f50)); k != 0; --k, a += 0x40) sh(a, 0xffff);
    }
    if (lw(OV(0x00160f50)) > 0) {
        int32_t s0 = 0;
        do {
            uint32_t v = 0x70003000u;
            int32_t a = 0;
            if (s0 < lw(OV(0x00160f50))) {
                sh(v, static_cast<uint32_t>(s0));
                for (;;) {
                    ++a;
                    v += 2;
                    const bool room = a < 0x3ff;
                    ++s0;
                    if (!room || !(s0 < lw(OV(0x00160f50)))) break;
                    sh(v, static_cast<uint32_t>(s0));
                }
            }
            sh(v, 0xffff);
            world_tfrag_lighting(0x70003000u);  // the tfrag entries' static lighting
        } while (s0 < lw(OV(0x00160f50)));
    }
    // the level's shared block (indexed by 0x15ed88)
    if (lw(H + 0x10) != 0) {
        const uint32_t b = H + lwu(H + 0x10 + lwu(0x0015ed88) * 4);
        sw(OV(0x0017943c), lwu(b));
        sw(OV(0x0015f660), s8);
        const uint32_t size = ((lwu(b + 4) + 3) & 0xfffffffcu) - 8;
        mem_copy(s8, b + 8, static_cast<int32_t>(size));
        if (lw(OV(0x0017943c)) > 0) {
            const uint32_t base = lwu(OV(0x0015f660));
            uint32_t e = base;
            for (int32_t i = 0; i < lw(OV(0x0017943c)); ++i, e += 0x10) sw(e, lwu(e) + base - 8);
        }
        corrected_texts_patch(lwu(OV(0x0015f660)), lw(OV(0x0017943c)), lw(0x0015ed88));  // enhancement (off: nothing)
        s8 = align64(s8 + size);
    }
    fixes102_level_load();  // enhancement: 1.02 Fixes' Glove of Doom ring cleared (off: nothing)
    {  // the tie instances: 0x20-byte bounds at 0x160fc0, 0x1c0-byte data at 0x160fc8
        uint32_t s4 = H + lwu(H + 0x34);
        const uint32_t nt = lwu(s4);
        sw(OV(0x00160fc0), s8);
        s4 += 0x10;
        sw(OV(0x00160fcc), nt);
        s8 += nt << 5;
        if (nt != 0) mem_fill32(lwu(OV(0x00160fc0)), 0, static_cast<int32_t>(nt << 5));
        const uint32_t nt2 = lwu(OV(0x00160fcc));
        s8 = align64(s8);
        sw(OV(0x00160fc8), s8);
        s8 += nt2 * 0x1c0;
        if (nt2 != 0) mem_fill32(lwu(OV(0x00160fc8)), 0, static_cast<int32_t>(nt2 * 0x1c0));
        uint32_t last = 0xffffffffu;
        sw(OV(0x00160fc4), lwu(OV(0x00160fc0)) + lwu(OV(0x00160fcc)) * 32);
        if (lw(OV(0x00160fcc)) > 0) {
            int32_t s7 = 0;
            do {
                const uint32_t cls = lwu(s4);
                const uint32_t s6 = static_cast<uint32_t>(s7) * 0x1c0;
                const uint32_t s3 = lwu(OV(0x00160fc0)) + static_cast<uint32_t>(s7) * 32;
                const uint32_t ti = lbu(cls + OV(0x001c5b00));
                sb(s3 + 0x1a, ti);
                const uint32_t s5 = lwu(OV(0x00160fc8)) + s6;
                if ((ti & 0xff) != last) {
                    sw(lwu(OV(0x001c5800) + ti * 4) + 0x28, s3);
                    sh(lwu(OV(0x001c5800) + lbu(s3 + 0x1a) * 4u) + 0x26, 0);
                    last = lbu(s3 + 0x1a);
                }
                const auto hdr = [s3] { return lwu(OV(0x001c5800) + lbu(s3 + 0x1a) * 4u); };
                sh(hdr() + 0x26, lhu(hdr() + 0x26) + 1u);
                sw(s3 + 0x10, s5);
                const float d = static_cast<float>(lw(s4 + 4));
                sh(s3 + 0x1e, 0xffff);
                sb(s3 + 0x1b, 0);
                swc1(s3 + 0x14, d);
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
                swc1(lwu(OV(0x00160fc8)) + s6 + 0xc, fpu_div(1.0f, vec3_length(V(lwu(OV(0x00160fc8)) + s6))));
                swc1(lwu(OV(0x00160fc8)) + s6 + 0x1c, fpu_div(1.0f, vec3_length(V(lwu(OV(0x00160fc8)) + s6 + 0x10))));
                swc1(lwu(OV(0x00160fc8)) + s6 + 0x2c, fpu_div(1.0f, vec3_length(V(lwu(OV(0x00160fc8)) + s6 + 0x20))));
                mem_copy(s5 + 0x140, s4, 0x80);
                s4 += 0x80;
                sh(s3 + 0x1c, lhu(s4));
                s4 += 0x10;
            } while (s7 < lw(OV(0x00160fcc)));
        }
        uint32_t v = 0x70000000u;
        for (int32_t i = 0; i < lw(OV(0x00160fcc)); ++i, v += 2) sh(v, static_cast<uint32_t>(i));
        sh(v, 0xffff);
        world_tie_lighting(0x70000000u);
    }
    {  // the shrub instances: bounds at 0x160494, data, 0x40 each, at 0x16049c, colours, 0x60 each, at 0x1604a0
        uint32_t s6 = H + lwu(H + 0x3c);
        const uint32_t ns = lwu(s6);
        sw(OV(0x00160494), s8);
        s6 += 0x10;
        sw(OV(0x00160490), ns);
        s8 += ns << 5;
        if (ns != 0) mem_fill32(lwu(OV(0x00160494)), 0, static_cast<int32_t>(ns << 5));
        s8 = align64(s8);
        const uint32_t ns2 = lwu(OV(0x00160490));
        sw(OV(0x0016049c), s8);
        s8 += ns2 << 6;
        if (ns2 != 0) mem_fill32(lwu(OV(0x0016049c)), 0, static_cast<int32_t>(ns2 << 6));
        const uint32_t ns3 = lwu(OV(0x00160490));
        sw(OV(0x001604a0), s8);
        s8 = align64(s8 + ns3 * 0x60);
        if (ns3 != 0) mem_fill32(lwu(OV(0x001604a0)), 0, static_cast<int32_t>(ns3 * 0x60));
        uint32_t last = 0xffffffffu;
        sw(OV(0x00160b74), lwu(OV(0x001604a0)));
        sw(OV(0x00160498), lwu(OV(0x00160494)) + lwu(OV(0x00160490)) * 32);
        if (lw(OV(0x00160490)) > 0) {
            int32_t i = 0;
            do {
                const uint32_t s5 = lwu(OV(0x00160494)) + static_cast<uint32_t>(i) * 32;
                const uint32_t s7 = lwu(OV(0x0016049c)) + static_cast<uint32_t>(i) * 64;
                const uint32_t si = lbu(lwu(s6) + OV(0x001bc1b0));
                sb(s5 + 0x1a, si);
                if (si != last) {
                    last = si;
                    sw(lwu(OV(0x001bc030) + si * 4) + 0x18, s5);
                    sh(lwu(OV(0x001bc030) + si * 4) + 0x16, 0);
                }
                const uint32_t s3 = OV(0x001bc030) + si * 4;
                sh(lwu(s3) + 0x16, lhu(lwu(s3) + 0x16) + 1u);
                sh(s5 + 0x18, static_cast<uint32_t>(i));
                const float d = lwc1(s6 + 4);
                swc1(s5 + 0x10, d);
                if (d < 16.0f) swc1(s5 + 0x10, 16.0f);
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
                ++i;
                s6 += 0x50;
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
            } while (i < lw(OV(0x00160490)));
        }
        uint32_t v = 0x70000000u;
        for (int32_t k = 0; k < lw(OV(0x00160490)); ++k, v += 2) sh(v, static_cast<uint32_t>(k));
        sh(v, 0xffff);
        world_shrub_lighting(0x70000000u);
        for (int32_t k = 0; k < lw(OV(0x00160490)); ++k) {
            uint32_t a = lwu(OV(0x001604a0)) + static_cast<uint32_t>(k) * 0x60;
            int32_t r = 0, g = 0, bl = 0;
            for (int32_t n = 0; n < 24; ++n, a += 4) {
                r += lbu(a);
                g += lbu(a + 1);
                bl += lbu(a + 2);
            }
            const uint32_t e = static_cast<uint32_t>(k) * 64 + lwu(OV(0x0016049c));
            sw(e + 0x1c, static_cast<uint32_t>(bl / 24) << 16 | static_cast<uint32_t>(g / 24) << 8 | static_cast<uint32_t>(r / 24));
        }
    }
    // the cameras (0x20 each) at 0x15ef50
    if (lw(H + 8) != 0) {
        uint32_t p = H + lwu(H + 8);
        const uint32_t c = lwu(p);
        sw(OV(0x0015ef50), s8);
        p += 0x10;
        sw(OV(0x0015ef54), c);
        s8 += c << 5;
        if (c != 0) mem_fill32(lwu(OV(0x0015ef50)), 0, static_cast<int32_t>(c << 5));
        s8 = align64(s8);
        if (lw(OV(0x0015ef54)) > 0) {
            uint32_t e = lwu(OV(0x0015ef50));
            for (int32_t k = lw(OV(0x0015ef54)); k != 0; --k, e += 0x20) {
                sw(e + 0xc, lwu(p));
                for (uint32_t o = 0; o < 0xc; o += 4) sw(e + o, lwu(p + 4 + o));
                for (uint32_t o = 0; o < 0xc; o += 4) sw(e + 0x10 + o, lwu(p + 0x10 + o));
                sw(e + 0x1c, lwu(p + 0x1c));
                p += 0x20;
            }
        }
    }
    // the 0x90-byte records at 0x13f2e4 (count 0x13f2e0)
    if (lw(H + 0xc) != 0) {
        uint32_t p = H + lwu(H + 0xc);
        const uint32_t c = lwu(p);
        p += 0x10;
        sw(0x0013f2e0, c);
        if (c == 0) {
            sw(0x0013f2e4, 0);
        } else {
            sw(0x0013f2e4, s8);
            const uint32_t size = c * 0x90;
            const uint32_t a = s8;
            s8 = align64(s8 + size);
            mem_fill32(a, 0, static_cast<int32_t>(size));
            for (int32_t k = 0, off = 0; k < lw(0x0013f2e0); ++k, off += 0x90) {
                const uint32_t e = lwu(0x0013f2e4) + static_cast<uint32_t>(off);
                mem_copy(e, p, 0x90);
                p += 0x90;
                sub_0028df58(e);
            }
        }
    }
    // the mobys
    uint32_t spawned = 0;
    {
        const uint32_t mp = H + lwu(H + 0x44);
        const int32_t n1 = lw(mp), n2 = lw(mp + 4);
        uint32_t s1 = mp + 0x10;
        sw(OV(0x0015ffd8), s8);
        mem_fill32(s8, 0, (n1 + n2) << 8);
        if (n1 > 0) {
            uint32_t map = OV(0x001ac780);  // the spawn index of each moby uid
            for (int32_t left = n1; left != 0; --left, map += 2) {
                const uint32_t rec_end = s1 + static_cast<uint32_t>((lw(s1) >> 2) << 2);
                s1 += 4;
                const uint32_t wl = lwu(s1);
                s1 += 4;
                const uint32_t flags = lwu(s1);
                s1 += 4;
                const int32_t uid = lw(s1);
                s1 += 4;
                const int32_t s6 = lw(s1);
                s1 += 4;
                const int32_t s7 = lw(s1);
                s1 += 4;
                int32_t s4 = s6, s5 = s6;
                uint32_t a2 = 0xfe;
                bool skip = false;
                const auto bit = [uid](uint32_t base) { return (lw(base + static_cast<uint32_t>(uid >> 5) * 4) >> (uid & 0x1f)) & 1; };
                const auto half_up = [](int32_t v) { return (v + static_cast<int32_t>(static_cast<uint32_t>(v) >> 31)) >> 1; };
                if (flags != 0) {
                    a2 = (flags & 0x10) != 0 ? static_cast<uint32_t>(sub_00284d90(lwu(0x0015ed84), uid)) & 0xff : 0xff;
                    const uint32_t lvbits = 0x0014c190 + (lwu(0x0015ed84) << 8);
                    if (lbu(OV(0x001bb684) + static_cast<uint32_t>(uid)) != 0) {
                        skip = true;
                    } else if ((flags & 3) != 0) {
                        if (lbu(OV(0x0015fc88) + wl) == 0xff) {
                            if ((flags & 2) == 0) {
                                skip = true;
                            } else {
                                s4 = s7;
                                if (bit(OV(0x001ba4d0)) != 0) {
                                    s5 = half_up(s4 + 1);
                                } else {
                                    s5 = s4;
                                }
                            }
                        } else if ((flags & 1) == 0) {
                            skip = true;
                        } else if (bit(lvbits) != 0) {
                            s5 = half_up(s6 + 1);
                        }
                    } else if ((flags & 8) != 0) {
                        skip = bit(OV(0x001ba4d0)) != 0;
                    } else if ((flags & 0xc) == 4) {
                        skip = bit(lvbits) != 0;
                    }
                }
                if (skip) {
                    sh(map, 0xffff);
                } else {
                    sh(map, spawned);
                    const uint32_t m = lwu(OV(0x0015ffd8)) + spawned * 0x100;
                    moby_init(m, g_session_classes ? session_placed_class(lwu(s1)) : lwu(s1));  // (a session class id: testing)
                    s1 += 4;
                    sh(m + 0xb2, static_cast<uint32_t>(uid));
                    sb(m + 0xb1, a2);
                    sh(m + 0xb4, static_cast<uint32_t>(s5));
                    sb(m + 0xb0, wl);
                    sh(m + 0xb6, static_cast<uint32_t>(s4));
                    if (lw(m + 0x24) != 0) swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * lwc1(s1));
                    s1 += 4;
                    sh(m + 0x32, lhu(s1));
                    s1 += 4;
                    sb(m + 0x30, lbu(s1));
                    s1 += 0xc;
                    for (uint32_t o = 0; o < 0xc; o += 4) sw(m + 0x10 + o, lwu(s1 + o));
                    s1 += 0xc;
                    for (uint32_t o = 0; o < 0xc; o += 4) sw(m + 0x40 + o, lwu(s1 + o));
                    s1 += 0xc;
                    sb(m + 0x21, lbu(s1));
                    s1 += 4;
                    const int32_t ground = lw(s1);
                    s1 += 4;
                    const float above = lwc1(s1);
                    s1 += 4;
                    if (ground != 0) {
                        const float z = floor_probe(0.5f, m + 0x10, 0);
                        if (0.0f < z) swc1(m + 0x18, z + above);
                    }
                    s1 += 4;
                    sh(m + 0x36, 0x7f80);
                    sw(m + 0x78, lwu(s1));
                    s1 += 4;
                    if (lw(s1) == 0) sh(m + 0x36, 0);
                    s1 += 4;
                    if (lw(m + 0x24) != 0) sh(lwu(m + 0x24) + 0x44, lhu(lwu(m + 0x24) + 0x44) | lhu(s1));
                    sh(m + 0x34, lhu(m + 0x34) | lhu(s1));
                    s1 += 4;
                    if (lw(m + 0x24) != 0) moby_update_transform(m);
                    const uint32_t r = lwu(s1), g = lwu(s1 + 4), b = lwu(s1 + 8), lt = lwu(s1 + 0xc);
                    s1 += 0x10;
                    const uint32_t col = (b << 16) + (g << 8) + r;
                    sw(m + 0x80, col);
                    moby_set_color_packed(m, static_cast<int64_t>(static_cast<int32_t>(col)), 0, 0, 0);
                    sw(m + 0x38, lt);
                    if (lw(s1) != -1) level_slot_set(m, lw(s1));
                    ++spawned;
                }
                s1 = rec_end;
            }
        }
        const uint32_t end = lwu(OV(0x0015ffd8)) + spawned * 0x100;
        sw(OV(0x0015ffdc), end);
        sb(end + 0x20, 0xff);
        const uint32_t all = (spawned + static_cast<uint32_t>(n2)) << 8;
        s8 += all;
        sw(OV(0x0015ffe8), s8);
        s8 = align64(s8 + (static_cast<uint32_t>(n2) << 7));
        sw(OV(0x0015ffe0), lwu(OV(0x0015ffd8)) + all - 0x100);
    }
    // the pvars: each owner's block moved out of the file data
    if (lw(H + 0x58) != 0) {
        const uint32_t data = H + lwu(H + 0x58), tab = H + lwu(H + 0x54);
        for (int32_t k = 0; k < lw(OV(0x0015ef54)); ++k) s8 = pvar_move(lwu(OV(0x0015ef50)) + static_cast<uint32_t>(k) * 32 + 0x1c, tab, data, s8);
        for (int32_t k = 0; k < lw(0x0013f2e0); ++k) s8 = pvar_move(lwu(0x0013f2e4) + static_cast<uint32_t>(k) * 0x90 + 8, tab, data, s8);
        for (uint32_t m = lwu(OV(0x0015ffd8)); m != lwu(OV(0x0015ffdc)); m += 0x100) s8 = pvar_move(m + 0x78, tab, data, s8);
        s8 = align64(s8);
    }
    // pvar words holding a moby uid -> its spawn index
    if (lw(H + 0x50) != 0) {
        uint32_t a = H + lwu(H + 0x50);
        const uint32_t tab = H + lwu(H + 0x54);
        for (int32_t i = lw(a); i >= 0; a += 8, i = lw(a)) {
            const int32_t pv = lw(tab + static_cast<uint32_t>(i) * 8);
            if (pv < static_cast<int32_t>(H)) continue;
            const uint32_t w = static_cast<uint32_t>(pv) + lwu(a + 4);
            const int32_t v = lw(w);
            if (v >= 0) sw(w, static_cast<uint32_t>(static_cast<int32_t>(lh(OV(0x001ac780) + static_cast<uint32_t>(v) * 2))));
        }
    }
    // pvar words holding an offset into their own block
    if (lw(H + 0x5c) != 0) {
        uint32_t a = H + lwu(H + 0x5c);
        const uint32_t tab = H + lwu(H + 0x54);
        for (int32_t i = lw(a); i >= 0; a += 8, i = lw(a)) {
            const int32_t pv = lw(tab + static_cast<uint32_t>(i) * 8);
            if (pv < static_cast<int32_t>(H)) continue;
            const uint32_t w = static_cast<uint32_t>(pv) + lwu(a + 4);
            sw(w, lwu(w) + static_cast<uint32_t>(pv));
        }
    }
    // the moby groups (lists of moby uids -> spawn indices, the last one flagged) at 0x1ab840
    if (lw(H + 0x48) != 0) {
        uint32_t a = H + lwu(H + 0x48);
        const uint32_t base = s8;
        const int32_t n = lw(a);
        a += 4;
        const int32_t size = lw(a);
        uint32_t s0 = a + 0xc;
        s8 = align64(s8 + static_cast<uint32_t>(size));
        if (size != 0) mem_copy128(base, s0 + static_cast<uint32_t>(n) * 4, size);
        mem_fill32(OV(0x001ab840), 0, 0x1c0);
        for (int32_t g = 0; g < n; ++g, s0 += 4) {
            const int32_t off = lw(s0);
            if (off < 0) continue;
            const uint32_t slot = OV(0x001ab840) + static_cast<uint32_t>(g) * 4;
            uint32_t rd = base + static_cast<uint32_t>(off);
            sw(slot, rd);
            sw(OV(0x0015fff4), static_cast<uint32_t>(g + 1));
            uint32_t wr = rd;
            for (;;) {
                const uint32_t id = lhu(rd) & 0x7fffu;
                const bool last = (lh(rd) & 0x8000) != 0;
                const int32_t v = lh(OV(0x001ac780) + id * 2);
                if (v >= 0) {
                    sh(wr, static_cast<uint32_t>(v));
                    wr += 2;
                }
                if (!last) {
                    rd += 2;
                    continue;
                }
                if (!(lw(slot) < static_cast<int32_t>(wr))) {
                    sw(slot, 0);
                } else {
                    sh(wr - 2, lhu(wr - 2) | 0x8000u);
                }
                break;
            }
        }
    }
    // pointers into the level's shared pvar data
    if (lw(H + 0x4c) != 0) {
        uint32_t p = H + lwu(H + 0x4c);
        const uint32_t base = s8;
        const int32_t size = lw(p);
        const int32_t n = lw(p + 4);
        p += 0x10;
        s8 = align64(s8 + static_cast<uint32_t>(size));
        if (size != 0) mem_copy128(base, p, size);
        p += static_cast<uint32_t>(size);
        const uint32_t tab = H + lwu(H + 0x54);
        for (int32_t k = n; k > 0; --k, p += 8) {
            const uint32_t w = lwu(p);
            const int32_t pv = lw(tab + (w & 0xffff) * 8);
            if (pv < static_cast<int32_t>(H)) continue;
            sw(static_cast<uint32_t>(pv) + static_cast<uint32_t>(static_cast<int32_t>(w) >> 16), base + lwu(p + 4));
        }
    }
    if (__builtin_expect(g_section_equiv, 0)) section_equiv_native(H);  // (testing, RAC1_SECTION_EQUIV: src/game/enhance/sections.h)
    // the paths (0x160104 count, 0x160108 data, pointers at 0x1b04b0)
    if (lw(H + 0x70) != 0) {
        uint32_t p = H + lwu(H + 0x70);
        sw(OV(0x00160104), lwu(p));
        const uint32_t src = p + lwu(p + 4);
        const int32_t size = lw(p + 8);
        sw(OV(0x00160108), s8);
        p += 0x10;
        s8 = align64(s8 + static_cast<uint32_t>(size));
        if (size != 0) mem_copy128(lwu(OV(0x00160108)), src, size);
        if (lw(OV(0x00160104)) > 0) {
            const uint32_t d = lwu(OV(0x00160108));
            uint32_t o = OV(0x001b04b0);
            for (int32_t k = lw(OV(0x00160104)); k != 0; --k, p += 4, o += 4) sw(o, d + lwu(p));
        }
    }
    // the grind lines (0x15f710 count, 0x20 each at 0x15f70c)
    if (lw(H + 0x74) != 0) {
        uint32_t p = H + lwu(H + 0x74);
        const int32_t n = lw(p);
        sw(OV(0x0015f710), static_cast<uint32_t>(n));
        const uint32_t src = p + lwu(p + 4);
        const int32_t size = lw(p + 8);
        sw(OV(0x0015f70c), s8);
        s8 = align64(s8 + (static_cast<uint32_t>(n) << 5));
        p += 0x10;
        if (n > 0) {
            uint32_t d = lwu(OV(0x0015f70c));
            for (int32_t k = n; k != 0; --k, d += 0x20) {
                lq_sq(d, p);
                p += 0x10;
                sw(d + 0x14, lwu(p));
                p += 0x10;
            }
        }
        const uint32_t pts = s8;
        s8 = align64(s8 + static_cast<uint32_t>(size));
        if (size != 0) mem_copy128(pts, src, size);
        if (lw(OV(0x0015f710)) > 0) {
            uint32_t d = lwu(OV(0x0015f70c)) + 0x10;
            for (int32_t k = lw(OV(0x0015f710)); k != 0; --k, p += 4, d += 0x20) sw(d, pts + lwu(p));
        }
    }
    // 0x90-byte records at 0x1600e4 (count 0x1600e8), read 0x80 apart
    if (lw(H + 0x6c) != 0) {
        uint32_t p = H + lwu(H + 0x6c);
        const uint32_t n = lwu(p);
        sw(OV(0x001600e4), s8);
        sw(OV(0x001600e8), n);
        p += 0x10;
        s8 = align64(s8 + n * 0x90);
        if (n != 0) mem_fill32(lwu(OV(0x001600e4)), 0, static_cast<int32_t>(n * 0x90));
        for (int32_t k = 0; k < lw(OV(0x001600e8)); ++k, p += 0x80) mem_copy(lwu(OV(0x001600e4)) + static_cast<uint32_t>(k) * 0x90, p, 0x90);
    }
    // three lists of 0x80-byte records
    for (const auto& l : {std::array<uint32_t, 3>{0x60, OV(0x001600ec), OV(0x001600f0)}, std::array<uint32_t, 3>{0x64, OV(0x001600f4), OV(0x001600f8)},
                          std::array<uint32_t, 3>{0x68, OV(0x001600fc), OV(0x00160100)}}) {
        if (lw(H + l[0]) == 0) continue;
        uint32_t p = H + lwu(H + l[0]);
        const uint32_t n = lwu(p);
        sw(l[1], s8);
        p += 0x10;
        sw(l[2], n);
        s8 = align64(s8 + (n << 7));
        if (n != 0) mem_fill32(lwu(l[1]), 0, static_cast<int32_t>(n << 7));
        for (int32_t k = 0; k < lw(l[2]); ++k, p += 0x80) mem_copy(lwu(l[1]) + (static_cast<uint32_t>(k) << 7), p, 0x80);
    }
    // two blocks of (size + 0x4000) bytes
    for (const auto& l : {std::array<uint32_t, 2>{0x84, OV(0x0015ef80)}, std::array<uint32_t, 2>{0x78, OV(0x0015fbc0)}}) {
        if (lw(H + l[0]) == 0) continue;
        const uint32_t p = H + lwu(H + l[0]);
        const uint32_t size = lwu(p) + 0x4000;
        sw(l[1], s8);
        mem_copy(s8, p + 0x10, static_cast<int32_t>(size));
        s8 = align64(s8 + size);
    }
    if (lw(H + 0x7c) != 0) {
        const uint32_t p = H + lwu(H + 0x7c);
        const uint32_t n = lwu(p);
        sw(OV(0x0015fb80), s8);
        sw(OV(0x0015fb50), n);
        s8 = align64(s8 + (n << 5));
        if (n != 0) mem_copy(lwu(OV(0x0015fb80)), p + 0x10, static_cast<int32_t>(n << 5));
    }
    if (lw(H + 0x88) != 0) {
        const uint32_t p = H + lwu(H + 0x88);
        const uint32_t n = lwu(p);
        sw(OV(0x00160040), s8);
        sw(OV(0x00160014), n);
        s8 = align64(s8 + n * 0x30);
        if (n != 0) mem_copy(lwu(OV(0x00160040)), p + 0x10, static_cast<int32_t>(n * 0x30));
    }
    sw(OV(0x0016016c), s8);
    sw(OV(0x00160174), 0xffffffffu);
    s8 += 0x20000;
    sw(OV(0x00160170), 0);
    sw(OV(0x00160178), 0);
    mem_clear128(OV(0x001b1880), 0x200);
    // the ship
    sw(0x0013e030, 0);
    if (0.0f < lwc1(ship)) {
        if (lw(0x0015ed84) == 0xd && lh(0x0013e056) == 2 && lw(OV(0x00160540)) != 0) {
            for (uint32_t m = lwu(OV(0x0015ffd8)); lbu(m + 0x20) != 0xff; m += 0x100) {
                if (lh(m + 0xa6) == lw(OV(0x00160548) + static_cast<uint32_t>(lh(0x0013e056)) * 4)) {
                    ship_init(m);
                    moby_set_anim(m, 1, 0);
                    break;
                }
            }
        }
        if (lw(0x0013e030) == 0) {
            const uint32_t m = moby_spawn(lwu(OV(0x00160548) + static_cast<uint32_t>(lh(0x0013e056)) * 4));
            const uint32_t f = lhu(m + 0x34) & 0xfffdu;
            swc1(m + 0x10, lwc1(ship));
            swc1(m + 0x14, lwc1(ship + 4));
            swc1(m + 0x18, lwc1(ship + 8));
            sw(m + 0x74, OVF(0x0028e180));
            sh(m + 0x32, 0xff);
            swc1(m + 0x48, lwc1(ship + 0xc));
            sb(m + 0x30, 0x10);
            sh(m + 0x34, f);
            moby_set_anim(m, 1, 0);
            moby_update_transform(m);
            sw(0x0013e030, m);
            level_slot_set(m, 0);
        }
    }
    // the LOD / visibility masks: from the file when they match this build of the level
    if (lw(OV(0x0015f600)) != 0 && lw(H + 0x8c) != 0) {
        const uint32_t s5 = H + lwu(H + 0x8c);
        const int32_t nl = lw(s5), ntie = lw(s5 + 4), nm = lw(s5 + 8);
        uint32_t a1 = s5 + 0x10;
        bool lod_bad = true, tie_bad = true;
        const uint32_t s4 = static_cast<uint32_t>(nl) << 3;
        if (lw(OV(0x00160f50)) == nl) {
            lod_bad = false;
            if (nl > 0) {
                uint32_t a = lwu(OV(0x00160f4c)) + 0x3d;
                for (int32_t k = nl; k != 0; --k, a += 0x40, a1 += 8)
                    if ((lbu(a) ^ lwu(a1 + 4)) != 0) lod_bad = true;
            }
        } else {
            a1 += s4;
        }
        const uint32_t t0 = lwu(OV(0x00160fc4)), a3t = lwu(OV(0x00160fc0));
        if (static_cast<int32_t>(t0 - a3t) >> 5 == ntie) {
            tie_bad = false;
            if (ntie > 0) {
                uint32_t a = a3t + 0x18;
                for (int32_t k = ntie; k != 0; --k, a1 += 8, a += 0x20)
                    if ((lhu(a) ^ lhu(a1 + 4)) != 0) tie_bad = true;
            }
        } else {
            a1 += static_cast<uint32_t>(ntie) << 3;
        }
        bool missing = false;
        int32_t nmiss = 0;
        for (uint32_t m = lwu(OV(0x0015ffd8)); lbu(m + 0x20) != 0xff; m += 0x100) {
            if (lhu(m + 0x36) == 0x7f80) continue;
            const int32_t k = find_id(a1, nm, lh(m + 0xb2));
            if (k == nm) {
                sh(m + 0x36, 0x7f80);
                missing = true;
                ++nmiss;
            } else {
                sh(m + 0x36, mask_bit(lw(a1 + static_cast<uint32_t>(k) * 8)));
            }
        }
        if (missing) debug_printf_stub();
        if (lod_bad) {
            debug_printf_stub();
            uint32_t a = lwu(OV(0x00160f4c)) + 0x3a;
            for (int32_t k = lw(OV(0x00160f50)); k > 0; --k, a += 0x40) sh(a, 0x7f80);
        } else if (nl > 0) {
            uint32_t a = lwu(OV(0x00160f4c)) + 0x3a, r = s5 + 0x10;
            for (int32_t k = nl; k != 0; --k, a += 0x40, r += 8) sh(a, mask_bit(lw(r)));
        }
        const uint32_t ties = s5 + s4 + 0x10;
        if (tie_bad) {
            for (uint32_t t = lwu(OV(0x00160fc0)); t != lwu(OV(0x00160fc4)); t += 0x20) {
                const int32_t k = find_id(ties, ntie, lhu(t + 0x18));
                if (k == ntie) {
                    sh(t + 0x18, 0x7f80);
                } else {
                    sh(t + 0x18, mask_bit(lw(ties + static_cast<uint32_t>(k) * 8)));
                }
            }
            debug_printf_stub();
        } else if (ntie > 0) {
            uint32_t a = lwu(OV(0x00160fc0)) + 0x18, r = ties;
            for (int32_t k = ntie; k != 0; --k, a += 0x20, r += 8) sh(a, mask_bit(lw(r)));
        }
        sw(OV(0x0016c074), 2);
    } else {
        debug_printf_stub();
        for (uint32_t m = lwu(OV(0x0015ffd8)); m != lwu(OV(0x0015ffe0)); m += 0x100) sh(m + 0x36, 0x7f80);
        for (uint32_t t = lwu(OV(0x00160fc0)); t != lwu(OV(0x00160fc4)); t += 0x20) sh(t + 0x18, 0x7f80);
        uint32_t a = lwu(OV(0x00160f4c)) + 0x3a;
        for (int32_t k = lw(OV(0x00160f50)); k > 0; --k, a += 0x40) sh(a, 0x7f80);
        sw(OV(0x0016c074), 0);
    }
    sub_001f83d0();
    hero_init();
    hero_level_reset();
    camera_init();
    camera_update();
    sub_00266d48();
    sub_001fb028();
    const uint32_t shipm = lwu(0x0013e030);
    sh(OV(0x001b0770), 0);
    sh(OV(0x001b07b0), 0);
    sh(OV(0x001b0830), 0);
    sw(OV(0x0015f5cc), 0);
    sw(OV(0x0015f63c), 0);
    if (shipm != 0) sd(shipm + 0x38, ld(lwu(0x0013f350 + 0x2080) + 0x38));
    const int32_t lv = lw(0x0015ed84);
    const bool alt = (lv < 9) ? (!(lv < 5) || lv == 2) : lv == 10;
    sw(OV(0x0015f318), alt ? OV(0x0016c7b0u) : OV(0x0016c780u));
    sw(OV(0x0015f310), lwu(OV(0x0015f318)));
    mod_code_level_loaded();  // enhancement: a level mod's spawn pools (src/host/mod_code.h; nothing on the game's levels)
    lvd::registry_level_loaded(H);  // (stage 5) the native level registry from the file just laid out (src/game/level/level_registry.h)
    return s8;
}

}  // namespace rac1
