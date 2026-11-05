// orig: (host) see palette_blend.h
#include <game/ovl/palette_blend.h>

#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/vu0_macro.h>
#include <game/ovl/vu0_regs.h>

#include <cstdlib>

namespace rac1 {

namespace {
bool enabled() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_PALETTE_BLEND_FAST");
#if defined(__WIIU__)
        return e == nullptr || std::atoi(e) != 0;
#else
        return e != nullptr && std::atoi(e) != 0;
#endif
    }();
    return on;
}
// RAC1_PALETTE_BLEND_DIRECT (default 1; 0 for A/B): the texels read and written through one host pointer each, without the
// clamps, when they cannot change anything (direct_ok)
bool direct_on() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_PALETTE_BLEND_DIRECT");
        return e == nullptr || std::atoi(e) != 0;
    }();
    return on;
}
// a product the sum and the conversion can take as it is: 0, or 2^-40 <= |x| < 2^30 (two of them: a sum below 2^31, never a
// denormal - both multiples of 2^-63 -, so the VU clamp is the identity and VFTOI0 the C conversion)
bool safe(float x) {
    const uint32_t e = (f2u(x) >> 23) & 0xffu;
    return (f2u(x) & 0x7fffffffu) == 0 || (e >= 127 - 40 && e < 127 + 30);
}
// the three streams in RAM (the PS2's 32 MB), word aligned, the texels written not over the indices (the loop reads a group's
// indices before writing it: an overlap would need that order)
bool direct_ok(uint32_t a0, uint32_t a1, uint32_t a3, uint32_t groups) {
    constexpr uint32_t kTop = 0x02000000u;
    const uint32_t nw = groups * 16, ni = groups * 4;
    if (((a0 | a1 | a3) & 3u) != 0 || groups > (kTop >> 4)) return false;
    if (a0 == 0 || a1 == 0 || a3 == 0 || a0 > kTop - nw || a1 > kTop - ni || a3 > kTop - ni) return false;
    const auto apart = [](uint32_t x, uint32_t nx, uint32_t y, uint32_t ny) { return x + nx <= y || y + ny <= x; };
    return apart(a0, nw, a1, ni) && apart(a0, nw, a3, ni);
}
}  // namespace

bool palette_blend_loop(uint32_t a0, uint32_t a1, uint32_t a3, uint32_t t1, uint32_t* v0) {
    const uint32_t groups = t1 >> 2;
    if (!enabled() || groups == 0) return false;
    eer::vmacro();
    vu0m::Vf* vf = vu0m::vf();
    // the two palettes' products (VMULbc: the operands and the product clamped by the VU rules), once per entry: the texels'
    // indices pick them (a byte: 256 entries each)
    const float wa = vf[31].v[3], wb = vf[31].v[0];
    const float* pa = static_cast<const float*>(ee_ram_ptr_read(0x70000000u));
    const float* pb = static_cast<const float*>(ee_ram_ptr_read(0x70002000u));
    static float ma[256][4], mb[256][4];
    const float ca = vuc(wa), cb = vuc(wb);
    for (int i = 0; i < 256; ++i)
        for (int l = 0; l < 4; ++l) {
            ma[i][l] = vuc(vuc(pa[i * 4 + l]) * ca);
            mb[i][l] = vuc(vuc(pb[i * 4 + l]) * cb);
        }
    uint32_t w[4] = {};
    int ia[4] = {}, ib[4] = {};
    float s[4][4] = {};
    bool direct = direct_on() && direct_ok(a0, a1, a3, groups);
    for (int i = 0; i < 256 && direct; ++i)
        for (int l = 0; l < 4; ++l) direct = direct && safe(ma[i][l]) && safe(mb[i][l]);
    if (direct) {
        // (host-order words: the index byte of the PS2's address a1 + t is the word's bits 8t..8t+7, the texel's lane l its
        // byte l, on either host)
        const uint32_t* xa = static_cast<const uint32_t*>(ee_ram_ptr_read(a1));
        const uint32_t* xb = static_cast<const uint32_t*>(ee_ram_ptr_read(a3));
        uint32_t* out = static_cast<uint32_t*>(ee_ram_ptr_write(a0, groups * 16));
        for (uint32_t g = 0; g < groups; ++g) {
            const uint32_t wa4 = xa[g], wb4 = xb[g];
            for (int t = 0; t < 4; ++t) {
                const float* pa4 = ma[(wa4 >> (8 * t)) & 0xffu];
                const float* pb4 = mb[(wb4 >> (8 * t)) & 0xffu];
                uint32_t word = 0;
                for (int l = 0; l < 4; ++l)
                    word |= (static_cast<uint32_t>(static_cast<int32_t>(pa4[l] + pb4[l])) & 0xffu) << (8 * l);
                out[g * 4 + static_cast<uint32_t>(t)] = word;
            }
        }
        // the last group again for the registers below (its indices still there: not overwritten)
        const uint32_t wa4 = xa[groups - 1], wb4 = xb[groups - 1];
        for (int t = 0; t < 4; ++t) {
            ia[t] = static_cast<int>((wa4 >> (8 * t)) & 0xffu);
            ib[t] = static_cast<int>((wb4 >> (8 * t)) & 0xffu);
            for (int l = 0; l < 4; ++l) s[t][l] = vuc(ma[ia[t]][l] + mb[ib[t]][l]);
        }
        w[0] = out[(groups - 1) * 4];
    }
    for (uint32_t g = 0; g < groups && !direct; ++g) {
        // (a group's eight indices read before its four words are written, as the loop does)
        for (int t = 0; t < 4; ++t) {
            ia[t] = static_cast<int>(lbu(a1 + static_cast<uint32_t>(t)));
            ib[t] = static_cast<int>(lbu(a3 + static_cast<uint32_t>(t)));
        }
        for (int t = 0; t < 4; ++t) {
            uint32_t word = 0;
            for (int l = 0; l < 4; ++l) {
                s[t][l] = vuc(ma[ia[t]][l] + mb[ib[t]][l]);
                word |= (vu0m::ftoi(s[t][l], 1.0f) & 0xffu) << (8 * l);
            }
            w[t] = word;
        }
        for (int t = 0; t < 4; ++t) sw(a0 + static_cast<uint32_t>(t) * 4, w[t]);
        a0 += 0x10;
        a1 += 4;
        a3 += 4;
    }
    // the VU0 registers as the loop leaves them: vf2 / vf4 / vf6 / vf8 the last group's integers, vf3 / vf5 / vf7 / vf9 its
    // second palette's products
    for (int t = 0; t < 4; ++t)
        for (int l = 0; l < 4; ++l) {
            vf[2 + 2 * t].v[l] = vu0m::bits(vu0m::ftoi(s[t][l], 1.0f));
            vf[3 + 2 * t].v[l] = mb[ib[t]][l];
        }
    *v0 = w[0];
    return true;
}

}  // namespace rac1
