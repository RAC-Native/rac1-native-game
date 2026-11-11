// Ported overlay code: game mode 7, the credits sequence (shared across every level).
// Opened by 0x299a68 (at the end of level 18 or from pause menu screen 7), updated per frame by
// 0x299c48 (streaming 512x448 RGBA32 background slides from the disc with a 32-frame column
// dissolve, then exiting via 0x299c00), and drawn by 0x1f8f78 (background upload 0x1fc558 and
// timed text entries 0x1fcca0 / 0x1fcb28).

#include <game/ovl/credits_tr.h>
#include <game/lib/libc.h>
#include <game/ovl/arrival.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
inline int32_t cvt_w_i32(float f) {
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) {
        return static_cast<int32_t>((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu);
    }
    return static_cast<int32_t>(f);
}

void stream_credits_slide(uint32_t dst, int32_t slot) {
    const uint32_t entry = 0x00137b80u + static_cast<uint32_t>(slot << 3);
    if (lw(0x0015ed80u) != 0) {
        stream_disc_read(dst, lwu(entry + 0x1748u), lwu(entry + 0x174cu));
    } else {
        stream_disc_read(dst, lwu(entry + 0x16a8u), lwu(entry + 0x16acu));
    }
}
}  // namespace

// orig: 0x00299a68
void credits_start_tr(uint32_t) {
    stream_reads_wait(0);
    screen_fade(8);
    sw(OV(0x0015f5d8u), 1u);
    sw(OV(0x0015f5c4u), 7u);
    sw(OV(0x0015f3fcu), f2u(1.0f));
    mem_fill32(OV(0x0015f620u), 0u, 0x10);

    const uint32_t base = lwu(OV(0x001611ccu)) + 0xfff20000u;
    const uint32_t buf_b = lwu(OV(0x00173e08u)) + base;
    const uint32_t buf_a = lwu(OV(0x00173e04u)) + base;
    sw(OV(0x0015f62cu), buf_b);
    sw(OV(0x001611ccu), base);
    sw(OV(0x0015f628u), buf_a);

    stream_credits_slide(buf_a, lh(OV(0x0015f622u)));
    stream_reads_wait(1);

    const uint32_t next_u = lhu(OV(0x0015f622u)) + 1u;
    sh(OV(0x0015f622u), next_u);
    stream_credits_slide(lwu(OV(0x0015f62cu)), static_cast<int16_t>(next_u));
}

// orig: 0x00299c00
TrRet cr_299c00_tr(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    dma_wait(1u);
    screen_fade(0xc);
    disc_read_break();
    sw(OV(0x0015f5c4u), 0u);
    sw(OV(0x0015f5d8u), 1u);
    sw(OV(0x0015f3fcu), 0u);
    sub_002a07c8();
    return TrRet{1u, 0.0f};
}

// orig: 0x00299c48
void credits_update_tr(uint32_t) {
    stream_reads_wait(0);
    const float fade = fpu_sub(u2f(lwu(OV(0x0015f3fcu))), u2f(0x3d4ccccdu));
    sw(OV(0x0015f3fcu), f2u(fade));
    if (fade < 0.0f) {
        sw(OV(0x0015f3fcu), 0u);
    }

    const int32_t state = lh(OV(0x0015f620u));
    const bool mode_41 = (lbu(0x0013d1d2u) == 0x41u);
    if (state == 0) {
        const int32_t page = lh(OV(0x0015f622u));
        const int32_t timer = lh(OV(0x0015f626u));
        const int32_t end_frame = lw(OV(0x00173bf0u) + static_cast<uint32_t>(page << 2)) - 0x24;
        if (!(timer < end_frame)) {
            if (lh(0x001516d8u) != 0) {
                stream_reads_wait(1);
            }
            const int32_t cur_page = lh(OV(0x0015f622u));
            if ((!mode_41 && cur_page == 0x27) || !(cur_page < 43)) {
                cr_299c00_tr(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
                return;
            }
            sh(OV(0x0015f624u), 0xffffu);
            sh(OV(0x0015f620u), 1u);
        }
    } else if (state == 1) {
        dma_wait(1u);
        const int32_t step = lh(OV(0x0015f624u));
        if (step < 32) {
            const uint32_t col_off = static_cast<uint32_t>(step << 2);
            uint32_t src = lwu(OV(0x0015f62cu)) + col_off;
            uint32_t dst = lwu(OV(0x0015f628u)) + col_off;
            int32_t count = 0;
            do {
                const uint32_t px = lwu(src);
                count += 0x20;
                src += 0x80u;
                sw(dst, px);
                dst += 0x80u;
            } while (!(0x37fff < count));
        } else if (!(step < 36)) {
            if (!mode_41) {
                if (lh(OV(0x0015f622u)) == 8) {
                    sh(OV(0x0015f622u), 0x11u);
                    sh(OV(0x0015f626u), 0x1590u);
                }
            } else {
                if (lh(OV(0x0015f622u)) == 0x11) {
                    sh(OV(0x0015f622u), 0x1bu);
                    sh(OV(0x0015f626u), 0x2238u);
                }
                if (lh(OV(0x0015f622u)) == 0x25) {
                    sh(OV(0x0015f622u), 0x27u);
                    sh(OV(0x0015f626u), 0x32b4u);
                }
            }
            const uint32_t next_u = lhu(OV(0x0015f622u)) + 1u;
            sh(OV(0x0015f620u), 0u);
            sh(OV(0x0015f624u), 0xffffu);
            sh(OV(0x0015f622u), next_u);
            const int32_t slot = ee_rem(static_cast<int32_t>(static_cast<int16_t>(next_u)), 0x14);
            stream_credits_slide(lwu(OV(0x0015f62cu)), slot);
        }
    }

    sh(OV(0x0015f624u), lhu(OV(0x0015f624u)) + 1u);
    sh(OV(0x0015f626u), lhu(OV(0x0015f626u)) + 1u);
    if ((lwu(0x0013cae4u) & 0x800u) != 0) {
        cr_299c00_tr(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    sound_frame();
}

// orig: 0x001fc558
TrRet cr_1fc558_tr(uint32_t src, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    uint32_t tbp = static_cast<uint32_t>(lw(0x0015ee84u) >> 8);
    int32_t rows_left = lw(0x0013e504u);
    uint32_t last_cursor = 0;
    do {
        const uint32_t p = lwu(OV(0x001611c0u));
        const uint64_t rows_u64 = static_cast<uint64_t>(static_cast<int64_t>(rows_left));
        const uint32_t chunk_h = (rows_u64 < 129ull) ? static_cast<uint32_t>(rows_left) : 0x80u;
        rows_left -= 0x80;

        sw(p + 0x0, 0x10000006u);
        sw(p + 0x4, 0u);
        sw(p + 0x8, 0u);
        sw(p + 0xc, 0x50000006u);

        const uint32_t q = p + 0x10u;
        const uint64_t qwc = static_cast<uint64_t>(chunk_h) << 7;
        sd(q + 0x00, 0x4000000000000001ull);
        sd(q + 0x08, 0x0eeeeeeeull);
        sd(q + 0x10, (static_cast<uint64_t>(tbp) << 32) | 0x0800000000000000ull);
        sd(q + 0x18, 0x50ull);
        sd(q + 0x20, 0ull);
        sd(q + 0x28, 0x51ull);
        sd(q + 0x30, (static_cast<uint64_t>(chunk_h) << 32) | 0x200ull);
        sd(q + 0x38, 0x52ull);
        sd(q + 0x40, 0ull);
        sd(q + 0x48, 0x53ull);
        sd(q + 0x50, qwc | 0x0800000000008000ull);
        sd(q + 0x58, 0ull);

        tbp += chunk_h << 3;
        const uint32_t p2 = p + 0x70u;
        sw(p2 + 0x0, static_cast<uint32_t>(qwc) | 0x30000000u);
        sw(p2 + 0x4, src);
        sw(p2 + 0x8, 0u);
        sw(p2 + 0xc, static_cast<uint32_t>(qwc) | 0x50000000u);
        src += chunk_h << 11;
        last_cursor = p2 + 0x10u;
        sw(OV(0x001611c0u), last_cursor);
    } while (rows_left > 0);

    packet_ref_1c2f00();
    return TrRet{last_cursor, 0.0f};
}

// orig: 0x001fcb28
TrRet cr_1fcb28_tr(uint32_t a0_in, uint32_t a1_in, uint32_t a2_in, uint32_t str, uint32_t, uint32_t, uint32_t, uint32_t, float box_scale, float text_scale, float, float) {
    const int32_t x_in = static_cast<int32_t>(a0_in);
    const int32_t y = static_cast<int32_t>(a1_in);
    const int32_t align = static_cast<int32_t>(a2_in);

    const int32_t raw_w = text_width_small(str, -1);
    const int32_t w = cvt_w_i32(fpu_mul(static_cast<float>(raw_w), box_scale));

    int32_t left = x_in;
    int32_t right = x_in;
    if (align == 0) {
        const int32_t half = w >> 1;
        right = x_in + half;
        left = x_in - half;
    } else if (align > 0) {
        if (align == 1) {
            left = x_in - w;
        }
    } else if (align == -1) {
        right = x_in + w;
    }

    const int32_t box_w = (right - left) + 0x40;
    const int32_t box_x = left - 0x20;
    const int32_t box_y = y - 0xe;
    const uint32_t box_alpha = static_cast<uint32_t>(float_to_int(fpu_mul(box_scale, 112.0f)));
    hud_box(box_x, box_y, box_w, 0x1c, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(box_alpha))));

    text_colour_codes_off();
    const uint32_t text_alpha = static_cast<uint32_t>(cvt_w_i32(fpu_mul(text_scale, 128.0f))) << 24;
    const uint64_t shadow_col = static_cast<uint64_t>(text_alpha);
    const uint64_t fg_col = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(text_alpha + 0x00e0e0e0u)));
    text_small(left + 1, y - 7, shadow_col, str, -1);
    text_small(left, y - 8, fg_col, str, -1);
    sub_001fb028();
    return TrRet{0u, 0.0f};
}

// orig: 0x001fcca0
TrRet cr_1fcca0_tr(uint32_t entry, uint32_t a1_in, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    const int32_t frame = static_cast<int32_t>(a1_in);
    const int32_t start = lh(entry + 0x8u);
    const int32_t end = start + lh(entry + 0xau);
    if (frame < start || end < frame) {
        return TrRet{0u, 0.0f};
    }

    float box_scale = 1.0f;
    if (frame < start + 0x10) {
        box_scale = fpu_mul(static_cast<float>(frame - start), 0.0625f);
    }
    if (end - 8 < frame) {
        box_scale = fpu_mul(static_cast<float>(end - frame), 0.125f);
    }

    float text_scale = 1.0f;
    if (frame < start + 0x20) {
        text_scale = fpu_mul(static_cast<float>((frame - 0x10) - start), 0.0625f);
        if (text_scale < 0.0f) text_scale = 0.0f;
    }
    if (end - 0x10 < frame) {
        text_scale = fpu_mul(static_cast<float>(end - (frame + 8)), 0.125f);
        if (text_scale < 0.0f) text_scale = 0.0f;
    }

    uint32_t src = static_cast<uint32_t>(sub_00203ad8(lw(entry + 0x0u)));
    const uint32_t buf = ee_stack_alloc(0x100);
    if (lh(entry + 0xeu) == 1) {
        uint32_t dst = buf;
        uint8_t c = lbu(src);
        if (c != 0) {
            for (;;) {
                if (c != 1u) {
                    sb(dst, c);
                    ++src;
                    ++dst;
                    c = lbu(src);
                    if (c == 0) break;
                } else {
                    sb(dst, 0x20u);
                    ++dst;
                    sb(dst, 0x2du);
                    ++dst;
                    sb(dst, 0x20u);
                    ++dst;
                    if (lbu(src) == 1u) {
                        do {
                            ++src;
                        } while (lbu(src) == 1u);
                    }
                    c = lbu(src);
                    if (c == 0) {
                        dst -= 3u;
                        break;
                    }
                }
            }
        }
        sb(dst, 0u);
        cr_1fcb28_tr(static_cast<uint32_t>(lh(entry + 0x4u)),
                     static_cast<uint32_t>(lh(entry + 0x6u)),
                     static_cast<uint32_t>(lh(entry + 0xcu)),
                     buf, 0, 0, 0, 0, box_scale, text_scale, 0.0f, 0.0f);
    } else {
        uint32_t dst = buf;
        int32_t y = lh(entry + 0x6u);
        uint8_t c = lbu(src);
        if (c != 0) {
            for (;;) {
                if (c != 1u) {
                    sb(dst, c);
                    ++src;
                    ++dst;
                    c = lbu(src);
                    if (c == 0) break;
                } else {
                    sb(dst, 0u);
                    cr_1fcb28_tr(static_cast<uint32_t>(lh(entry + 0x4u)),
                                 static_cast<uint32_t>(y),
                                 static_cast<uint32_t>(lh(entry + 0xcu)),
                                 buf, 0, 0, 0, 0, box_scale, text_scale, 0.0f, 0.0f);
                    y += 0x1a;
                    dst = buf;
                    if (lbu(src) == 1u) {
                        do {
                            ++src;
                        } while (lbu(src) == 1u);
                    }
                    c = lbu(src);
                    if (c == 0) break;
                }
            }
        }
        if (buf < dst) {
            sb(dst, 0u);
            cr_1fcb28_tr(static_cast<uint32_t>(lh(entry + 0x4u)),
                         static_cast<uint32_t>(y),
                         static_cast<uint32_t>(lh(entry + 0xcu)),
                         buf, 0, 0, 0, 0, box_scale, text_scale, 0.0f, 0.0f);
        }
    }
    ee_stack_free(0x100);
    return TrRet{0u, 0.0f};
}

// orig: 0x001f8f78
void credits_draw_tr(uint32_t) {
    if (lw(OV(0x0015f5d8u)) != 0) return;

    cr_1fc558_tr(lwu(OV(0x0015f628u)), 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    float fade = u2f(lwu(OV(0x0015f3fcu)));
    if (0.0f < fade) {
        if (1.0f < fade) {
            sw(OV(0x0015f3fcu), f2u(1.0f));
            fade = 1.0f;
        }
        const int32_t alpha = float_to_int(fpu_mul(fade, 128.0f));
        sub_001fa050(0, 0, 0, static_cast<int64_t>(alpha));
    }

    render_static_packets();
    texture_block_begin(0);
    uint32_t entry = OV(0x00173080u);
    for (int32_t i = 0xb6; i >= 0; --i, entry += 0x10u) {
        cr_1fcca0_tr(entry, static_cast<uint32_t>(lh(OV(0x0015f626u))), 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    texture_block_end();
}

}  // namespace rac1
