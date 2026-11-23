// Game mode 7 credits sequence (0x299a68 start, 0x299c48 update, 0x1f8f78 draw; implemented in credits.cpp).
#ifndef RAC1_GAME_OVL_CREDITS_TR_H
#define RAC1_GAME_OVL_CREDITS_TR_H

#include <cstdint>

#include <game/ovl/novalis_mobys2.h>

namespace rac1 {

void credits_start_tr(uint32_t a0);  // orig: 0x00299a68
uint32_t cr_299c00_tr();  // 0x00299c00
void credits_update_tr(uint32_t a0);  // 0x00299c48
uint32_t cr_1fc558_tr(uint32_t src);  // 0x001fc558
void cr_1fcb28_tr(int32_t x, int32_t y, int32_t align, uint32_t str, float box_scale, float text_scale);  // 0x001fcb28
void cr_1fcca0_tr(uint32_t entry, int32_t frame);  // 0x001fcca0
void credits_draw_tr(uint32_t a0);  // 0x001f8f78

}  // namespace rac1

#endif
