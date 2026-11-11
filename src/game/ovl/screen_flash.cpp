// Screen flash overlay pass (0x001f9df8): draws full-screen and scanline-banded additive/alpha
// flash quads configured by the descriptor at *OV(0x0015f310).
#include <game/ovl/screen_flash.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/quad_draw.h>

namespace rac1 {

namespace {
constexpr uint32_t ADDR_SCREEN_W = 0x001518d0u;
constexpr uint32_t ADDR_SCREEN_H = 0x001518d2u;
constexpr OvConst<0x0015f310u> FLASH_CFG_PTR{};
constexpr uint64_t GS_ALPHA_MASK = 0x0000ff00000000ffull;
}  // namespace

// orig: 0x001f9df8
void screen_flash() {
    uint32_t cfg = lwu(FLASH_CFG_PTR);
    const uint64_t alpha0 = ld(cfg + 0x8);
    const int32_t scr_h = lh(ADDR_SCREEN_H);
    if (alpha0 != 0) {
        sub_002a0ba8(0x42u, alpha0 & GS_ALPHA_MASK);
        cfg = lwu(FLASH_CFG_PTR);
    }
    const uint32_t col0 = lwu(cfg + 0x4);
    if ((col0 & 0xff000000u) != 0) {
        quad_fill(0, scr_h, 0, lh(ADDR_SCREEN_W), static_cast<uint64_t>(col0));
    }

    if (scr_h <= 0) {
        return;
    }

    int32_t y = 0;
    cfg = lwu(FLASH_CFG_PTR);
    bool more = false;
    do {
        const uint64_t alpha1 = ld(cfg + 0x18);
        if (alpha1 != 0) {
            sub_002a0ba8(0x42u, alpha1 & GS_ALPHA_MASK);
        }
        cfg = lwu(FLASH_CFG_PTR);
        const uint32_t col1 = lwu(cfg + 0x14);
        if ((col1 & 0xff000000u) != 0) {
            int32_t y1 = y + lw(cfg + 0x10);
            if (!(y1 < scr_h - 1)) {
                y1 = scr_h - 1;
            }
            quad_fill(y, y1, 0, lh(ADDR_SCREEN_W), static_cast<uint64_t>(col1));
            cfg = lwu(FLASH_CFG_PTR);
        }
        y += lw(cfg + 0x10);

        const uint64_t alpha2 = ld(cfg + 0x28);
        if (alpha2 != 0) {
            sub_002a0ba8(0x42u, alpha2 & GS_ALPHA_MASK);
        }
        cfg = lwu(FLASH_CFG_PTR);
        const uint32_t col2 = lwu(cfg + 0x24);
        if ((col2 & 0xff000000u) != 0) {
            int32_t y1 = y + lw(cfg + 0x20);
            if (!(y1 < scr_h - 1)) {
                y1 = scr_h - 1;
            }
            quad_fill(y, y1, 0, lh(ADDR_SCREEN_W), static_cast<uint64_t>(col2));
        }
        cfg = lwu(FLASH_CFG_PTR);
        y += lw(cfg + 0x20);
        more = y < scr_h;
        cfg = lwu(FLASH_CFG_PTR);
    } while (more);
}

}  // namespace rac1
