// orig: (host) see palette_blend.h
#include <game/ovl/palette_blend.h>

#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/vu0_macro.h>
#include <game/ovl/vu0_regs.h>

#include <cstdlib>

namespace rac1 {

namespace {
// orig: (host) RAC1_PERF_PALETTE_BLEND=0: the loop as transliterated (A/B)
bool enabled() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_PERF_PALETTE_BLEND");
        return e == nullptr || std::atoi(e) != 0;
    }();
    return on;
}
}  // namespace

// orig: (host) see palette_blend.h
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
    thread_local float ma[256][4], mb[256][4];
    const float ca = vuc(wa), cb = vuc(wb);
    for (int i = 0; i < 256; ++i)
        for (int l = 0; l < 4; ++l) {
            ma[i][l] = vuc(vuc(pa[i * 4 + l]) * ca);
            mb[i][l] = vuc(vuc(pb[i * 4 + l]) * cb);
        }
    uint32_t w[4] = {};
    int ia[4] = {}, ib[4] = {};
    float s[4][4] = {};
    for (uint32_t g = 0; g < groups; ++g) {
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
