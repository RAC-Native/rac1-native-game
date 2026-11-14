// Ported overlay code: widget functions for the end-of-game menu pages (menu state 0x21 and page
// state 0x23, shared across every level via Veldin addresses). Opened after defeating Drek and the
// credits on level 18 ("Challenge Mode" vs "Timewarp before defeating Drek", completion stats, and
// scrolling epilogue text).

#include <game/ovl/end_menu_tr.h>
#include <game/lib/libc.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/save_menus.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

// orig: 0x00283530
uint32_t end_menu_panel_update_tr(uint32_t w, uint32_t) {
    const int32_t st = lw(w + 0x34u);
    if (st == 0) {
        if (lh(0x001516d8u) != 0) return 0u;
        const uint32_t sectors = lwu(0x00139194u);
        const uint32_t lba = lwu(0x00139190u);
        const uint32_t off = 0x4f000u - (sectors << 11);
        if (stream_disc_read(lwu(w + 0x3cu) + off, lba, sectors) == 0) {
            sw(w + 0x34u, 3u);
            return 0u;
        }
        sw(w + 0x40u, off);
        sw(w + 0x34u, 1u);
        sb(OV(0x001b9dbbu), 1u);
        menu_buf_busy(lwu(w + 0x3cu));
        return 0u;
    }
    if (st == 1) {
        if (lh(0x001516d8u) != 0) return 0u;
        sb(OV(0x001b9dbbu), 0u);
        const uint32_t dst = lwu(w + 0x3cu);
        wad_decompress(dst + lwu(w + 0x40u), dst);
        const uint32_t cls_slot = OV(0x00197300u) + (static_cast<uint32_t>(lbu(OV(0x00198365u))) << 2);
        const uint32_t base = lwu(w + 0x3cu);
        for (int32_t i = 0; i < 3; ++i) {
            const uint32_t seq_off = lwu(base + static_cast<uint32_t>(i << 3));
            const uint32_t seq_idx = static_cast<uint32_t>(i + 1);
            const uint32_t cls = lwu(cls_slot);
            sw(cls + (seq_idx << 2) + 0x48u, lwu(w + 0x3cu) + seq_off);
            moby_seq_relocate(lwu(cls_slot), seq_idx);
        }
        const int32_t has_flag = lw(0x0015ee20u);
        sw(w + 0x34u, 2u);
        sb(lwu(w + 0x44u) + 0x20u, has_flag != 0 ? 4u : 0u);
        sb(lwu(w + 0x44u) + 0xbcu, 0u);
        return 0u;
    }
    if (st != 2) return 0u;

    const uint32_t m = lwu(w + 0x44u);
    const uint32_t mst = lbu(m + 0x20u);
    if (!(mst < 7u)) return 0u;

    // (the state's case by its index: the original's switch over the moby's state, compiled to the jump table 0x1e9350;
    // the disc's table is not read, so the states work on a build whose own code moved the case labels)
    switch (mst) {
        case 0: case 2: case 4: {  // (0x2836e0)
            int32_t lang = lw(0x0015ed88u) - 1;
            const uint32_t half_st = static_cast<uint32_t>(lbu(m + 0x20u)) >> 1;
            if (!(-1 < lang)) lang = 0;
            if (lbu(m + 0xbcu) == 0) {
                sw(0x001516ecu, static_cast<uint32_t>(static_cast<int32_t>(half_st) * 6 + lang + 0xea60));
            }
            uint32_t cnt = (lbu(m + 0xbcu) + 1u) & 0xffu;
            sb(m + 0xbcu, cnt);
            if (frame_scale_i(0x78) < static_cast<int32_t>(cnt)) {
                sb(m + 0xbcu, static_cast<uint32_t>(frame_scale_i(0x78)));
            }
            cnt = lbu(m + 0xbcu);
            if (static_cast<int32_t>(cnt) < frame_scale_i(0x78)) return 0u;
            if (lh(0x0015172au) != 3) return 0u;
            const uint32_t cur_mst = lbu(m + 0x20u);
            sb(m + 0xbcu, 0u);
            sb(m + 0x20u, cur_mst + 1u);
            moby_anim_set(m, half_st + 1u, 0, frame_scale_i(0x18));
            return 0u;
        }
        case 1: case 3: case 5: {  // (0x2837ac)
            if (lbu(m + 0xbcu) == 0 && lbu(m + 0x52u) == lbu(m + 0x53u)) {
                dialog_stream_stop();
                sb(m + 0xbcu, 1u);
            }
            if ((lbu(m + 0x70u) & 2u) == 0) return 0u;
            const uint32_t cur_mst = lbu(m + 0x20u);
            sb(m + 0x20u, cur_mst == 1u ? 2u : 6u);
            sb(m + 0xbcu, 0u);
            moby_anim_set(m, 0u, 0, frame_scale_i(0x18));
            return 0u;
        }
        case 6: {  // (0x283824)
            uint32_t cnt = (lbu(m + 0xbcu) + 1u) & 0xffu;
            sb(m + 0xbcu, cnt);
            if (frame_scale_i(0xf0) < static_cast<int32_t>(cnt)) {
                sb(m + 0xbcu, static_cast<uint32_t>(frame_scale_i(0xf0)));
            }
            cnt = lbu(m + 0xbcu);
            if (static_cast<int32_t>(cnt) < frame_scale_i(0xf0)) return 0u;
            sb(m + 0x20u, lw(0x0015ee20u) != 0 ? 4u : 0u);
            sb(m + 0xbcu, 0u);
            return 0u;
        }
    }
    return 0u;  // (not reached: every state below 7 has its case)
}

// orig: 0x002838b0
uint32_t end_menu_panel_draw_tr(uint32_t w, uint32_t) {
    uint32_t p = w + 0x44u;
    for (int32_t i = 0x17; i >= 0; --i, p += 4u) {
        const uint32_t m = lwu(p);
        if (m != 0) {
            moby_draw_list(m, 1);
        }
    }
    return 4u;
}

// orig: 0x002833d0
uint32_t end_menu_panel_focus_in_tr(uint32_t w, uint32_t) {
    const uint32_t m = static_cast<uint32_t>(menu_moby_spawn(0x7a5));
    sw(w + 0x34u, 0u);
    if (m != 0) {
        sw(w + 0x44u, m);
        sh(m + 0x34u, 0u);
        const uint32_t pv = lwu(m + 0x78u);
        sw(m + 0x10u, f2u(fpu_add(u2f(lwu(OV(0x00166dc0u))), u2f(0x400ccccdu))));
        sw(m + 0x14u, f2u(fpu_add(u2f(lwu(OV(0x00166dc4u))), 0.0f)));
        sw(m + 0x48u, 0x40490fdbu);
        sw(m + 0x74u, OVF(0x00283900u));
        sw(m + 0x18u, f2u(fpu_add(u2f(lwu(OV(0x00166dc8u))), u2f(0xbfcccccdu))));
        sw(pv + 0x8u, 0u);
        sw(pv + 0x0u, w);
        sw(pv + 0x4u, 0u);
    } else {
        sw(w + 0x34u, 3u);
    }
    const uint32_t buf = static_cast<uint32_t>(menu_buf_alloc(1));
    sw(w + 0x3cu, buf);
    if (buf == 0) {
        sw(w + 0x34u, 3u);
    }
    return 0u;
}

// orig: 0x002834a8
uint32_t end_menu_panel_focus_out_tr(uint32_t w, uint32_t) {
    uint32_t p = w + 0x44u;
    for (int32_t i = 0x17; i >= 0; --i, p += 4u) {
        sw(p, static_cast<uint32_t>(menu_moby_free(lwu(p))));
    }
    sw(w + 0x3cu, static_cast<uint32_t>(menu_buf_free(lwu(w + 0x3cu))));
    const uint32_t st = lhu(0x0015172au) - 6u;
    if (!(st < 2u)) {
        sh(0x0015172au, 5u);
    }
    return 0u;
}

// orig: 0x0027fed8
uint32_t end_menu_choice_update_tr(uint32_t, uint32_t) {
    const uint32_t pressed = lwu(0x0013cae4u);
    if ((pressed & 0x20u) != 0) {
        const int32_t card = lw(0x0015eeb0u);
        if (card == 1 || card == 0x10) {
            sw(OV(0x001b9cf8u), OV(0x001b8fc0u));
        } else {
            challenge_game_image(-1);
            level_exit_to(0);
            sh(0x0013e05au, 1u);
        }
        return 0u;
    }
    if ((pressed & 0x40u) == 0) return 0u;

    const uint8_t saved_byte = lbu(0x0013d425u);
    uint8_t saved_4[4];
    for (uint32_t i = 0; i < 4; ++i) {
        saved_4[i] = lbu(0x0014bf08u + i);
    }
    save_blocks_read(lwu(OV(0x001b9cf0u) + 0xe0u));
    sb(0x0013d425u, saved_byte);
    for (uint32_t i = 0; i < 4; ++i) {
        sb(0x0014bf08u + i, saved_4[i]);
    }
    sw(0x0015eea0u, 1u);
    level_change_request(0, -1);
    sub_00284e00();
    screen_fade(frame_scale_i(0x10));
    sb(0x00141401u, 1u);
    return 0xffffffffu;
}

// orig: 0x002806e0
uint32_t end_menu_choice_draw_tr(uint32_t w, uint32_t) {
    texture_block_begin(0);
    const int32_t row_h = ee_div(lw(w + 0x24u), 5);
    const uint32_t buf = ee_stack_alloc(0x50);

    auto measure_stat = [buf](int32_t txt_id, int32_t count, uint64_t total) -> int32_t {
        const uint32_t fmt = static_cast<uint32_t>(sub_00203ad8(txt_id));
        const std::vector<uint64_t> args = {static_cast<uint64_t>(static_cast<int64_t>(count)), total};
        libc_sprintf(buf, fmt, args);
        return text_width_small(buf, -1);
    };

    int32_t max_w = 0;
    if (const int32_t w1 = measure_stat(TXT(0x522f), bytes_14bec0_count(), 0x28ull); 0 < w1) max_w = w1;
    if (const int32_t w2 = measure_stat(TXT(0x5230), gold_bolts_count(), 0xaull); max_w < w2) max_w = w2;
    if (const int32_t w3 = measure_stat(TXT(0x5231), skill_points_count(), 0x1eull); max_w < w3) max_w = w3;

    int32_t fx_id = 1;
    uint32_t font = OV(0x001c3150u);
    if (lw(w + 0x20u) < max_w + 0x18) {
        fx_id = 2;
        font = OV(0x001c34f0u);
    }

    int32_t y = row_h - 8;
    text_colour_codes_off();
    const int32_t cx = lw(w + 0x20u) >> 1;
    const uint32_t hdr = static_cast<uint32_t>(sub_00203ad8(TXT(0x522e)));
    text_centered_small(cx, y, 0x80ffa888ull, hdr, -1);
    y += row_h;

    auto draw_stat_row = [buf, fx_id, font](int32_t row_y, int32_t txt_id, int32_t count_check, int32_t (*count_fn)(), uint64_t total) {
        mission_box(0xb, row_y + 9, count_check == static_cast<int32_t>(total) ? 1 : 0);
        const uint32_t fmt = static_cast<uint32_t>(sub_00203ad8(txt_id));
        const int32_t count_val = count_fn();
        const std::vector<uint64_t> args = {static_cast<uint64_t>(static_cast<int64_t>(count_val)), total};
        libc_sprintf(buf, fmt, args);
        const uint64_t tex = static_cast<uint64_t>(effect_tex0(fx_id));
        text_draw(0x14, row_y, 0x80ffa888ull, buf, -1, tex, font);
    };

    draw_stat_row(y, TXT(0x522f), bytes_14bec0_count(), &bytes_14bec0_count, 0x28ull);
    y += row_h;
    draw_stat_row(y, TXT(0x5230), gold_bolts_count(), &gold_bolts_count, 0xaull);
    y += row_h;
    draw_stat_row(y, TXT(0x5231), skill_points_count(), &skill_points_count, 0x1eull);

    sub_001fb028();
    texture_block_end();
    ee_stack_free(0x50);
    return 2u;
}

// orig: 0x00280038
uint32_t end_menu_page23_update_tr(uint32_t w, uint32_t) {
    const uint64_t pad64 = ld(0x0013cae0u);
    const uint32_t ms = OV(0x001b9cf0u);
    const int32_t cheat_en = lw(OV(0x001b9dccu));
    const bool shoulders = ((pad64 & 0xfull) == 0xfull);

    if (cheat_en == 0 || (shoulders && (lwu(0x0013cae0u) & 0x10u) != 0)) {
        if ((lwu(0x0013c940u + 0x1c4u) & 0xd00u) != 0 && lw(ms + 0x124u) == 0) {
            return 1u;
        }
        if ((lwu(0x0013c940u + 0x1c4u) & 0x10u) != 0) {
            const uint32_t parent = lwu(lwu(ms + 0x4u) + 0x38u);
            if (parent != 0) {
                sw(ms + 0x8u, parent);
            } else if (lw(ms + 0x124u) == 0) {
                return 0xffffffffu;
            }
        }
    }

    const uint32_t cur_t = lwu(w + 0x40u);
    sw(w + 0x40u, shoulders ? (cur_t - 2u) : (cur_t - 1u));
    if (lw(w + 0x40u) < 0) {
        sw(w + 0x40u, 0u);
    }

    const uint32_t step = lwu(w + 0x50u);
    if (!(step < 20u)) return 0u;

    // (the step's case by its index: the original's switch over the step, compiled to the jump table 0x1e92b0; the
    // disc's table is not read, so the step works on a build whose own code moved the case labels: PAL 2.00)
    switch (step) {
        case 0:  // (0x280140)
            sw(w + 0x50u, step + 1u);
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sw(w + 0x3cu, 0u);
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x50a9)));
            sw(w + 0x34u, OV(0x001b9198u));
            return 0u;
        case 1: case 5: case 8: case 12: case 16:  // (0x280174)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(w + 0x54u, 0u);
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            return 0u;
        case 2: case 6: case 9: case 13: case 17: {  // (0x280194)
            const uint32_t scr = lwu(w + 0x3cu);
            sw(w + 0x3cu, shoulders ? (scr + 0x14u) : (scr + 0xau));
            if (lw(w + 0x54u) == 0) return 0u;
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            return 0u;
        }
        case 4:  // (0x2801d4)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(w + 0x50u, 5u);
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sw(w + 0x3cu, 0u);
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x50d6)));
            sw(w + 0x34u, OV(0x001b9248u));
            return 0u;
        case 3:  // (0x280214)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x50d4)));
            sw(w + 0x34u, static_cast<uint32_t>(TXT(0x50d5)));
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xf0)));
            sw(w + 0x3cu, 0u);
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            return 0u;
        case 7:  // (0x28023c)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x5106)));
            sw(w + 0x34u, OV(0x001b9308u));
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xf0)));
            sw(w + 0x3cu, 0u);
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            return 0u;
        case 10:  // (0x280268)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x5136)));
            sw(w + 0x34u, static_cast<uint32_t>(TXT(0x5137)));
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xf0)));
            sw(w + 0x3cu, 0u);
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            return 0u;
        case 11:  // (0x280290)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sw(w + 0x3cu, 0u);
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x5138)));
            sw(w + 0x50u, 0xcu);
            sw(w + 0x34u, OV(0x001b9398u));
            return 0u;
        case 14:  // (0x2802c8)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(OV(0x001b6bf4u), 0u);
            sw(w + 0x34u, static_cast<uint32_t>(TXT(0x5143)));
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xf0)));
            sw(w + 0x3cu, 0u);
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            return 0u;
        case 15:  // (0x280308)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sw(w + 0x3cu, 0u);
            sw(OV(0x001b6bf4u), static_cast<uint32_t>(TXT(0x5144)));
            sw(w + 0x50u, 0x10u);
            sw(w + 0x34u, OV(0x001b93b0u));
            return 0u;
        case 18:  // (0x280348)
            if (lw(w + 0x40u) != 0) return 0u;
            sw(OV(0x001b6bf4u), 0u);
            sw(w + 0x34u, static_cast<uint32_t>(TXT(0x5175)));
            sw(w + 0x50u, lwu(w + 0x50u) + 1u);
            sw(w + 0x40u, static_cast<uint32_t>(frame_scale_i(0x12c)));
            return 0u;
        case 19:  // (0x280380)
            if (lw(w + 0x40u) != 0) return 0u;
            if (lw(OV(0x001b9cf0u) + 0xdcu) != 0) return 1u;
            return 0u;
        default:
            return 0u;
    }
}

// orig: 0x002803b0
uint32_t end_menu_page23_draw_tr(uint32_t w, uint32_t) {
    sub_002a0ba8(0x47u, 0x30000ull);
    sub_002a0ba8(0x42u, (0x8000ull << 24) | 0x44ull);
    texture_block_begin(0);

    const uint32_t box = ee_stack_alloc(0x20);
    widget_text_box(box, w);
    const uint32_t wy = lhu(w + 0x1cu);
    const uint32_t wh = lhu(w + 0x24u);
    const uint32_t wx = lhu(w + 0x18u);
    const uint32_t ww16 = lhu(w + 0x20u);
    const int32_t ww32 = lw(w + 0x20u);
    const uint32_t step = lwu(w + 0x50u);

    sh(box + 0x12u, 9u);
    sh(box + 0x06u, wx + ww16);
    sh(box + 0x00u, wy + 4u);
    sh(box + 0x02u, wy + wh - 4u);
    sh(box + 0x08u, wx + static_cast<uint32_t>(ww32 >> 1));
    sh(box + 0x04u, wx);
    sh(box + 0x14u, 0u);
    sh(box + 0x16u, 0u);

    if (step < 20u) {
        // (the step's case by its index: the original's switch over the step, compiled to the jump table 0x1e9300; the
        // disc's table is not read, so the steps work on a build whose own code moved the case labels)
        switch (step) {
            case 1: case 2: case 3: case 5: case 6: case 7: case 8: case 9: case 10:
            case 12: case 13: case 14: case 16: case 17: case 18: {  // (0x280478)
                int32_t y = lw(w + 0x1cu) - ((lw(w + 0x3cu) >> 4) - 4);
                uint32_t idx = 0;
                if (lw(lwu(w + 0x34u)) != -1) {
                    for (;;) {
                        sh(box + 0x0au, static_cast<uint32_t>(y));
                        sh(box + 0x16u, static_cast<uint32_t>(lbu(w + 0x3cu) >> 4));
                        const int32_t tid = lw(lwu(w + 0x34u) + (idx << 2));
                        ++idx;
                        text_box_small(box, 0x80ffa888ull, static_cast<uint32_t>(sub_00203ad8(tid)), -1);
                        y += lh(box + 0x0eu);
                        const int32_t next_tid = lw(lwu(w + 0x34u) + (idx << 2));
                        y += 0xa;
                        if (next_tid == -1) break;
                    }
                }
                if (y + 0x18 < lw(w + 0x1cu) + lw(w + 0x24u)) {
                    sw(w + 0x54u, 1u);
                }
                break;
            }
            case 4: case 11: case 15: case 19:  // (0x280538)
                sh(box + 0x0au, lhu(w + 0x1cu) + 0x20u);
                text_box_small(box, 0x80ffa888ull, static_cast<uint32_t>(sub_00203ad8(lw(w + 0x34u))), -1);
                break;
            case 0:  // (0x280568)
                break;
        }
    }

    texture_block_end();
    ee_stack_free(0x20);
    return 2u;
}

// orig: 0x00280020
uint32_t end_menu_page23_focus_in_tr(uint32_t w, uint32_t) {
    sw(w + 0x3cu, 0u);
    sw(w + 0x40u, 0u);
    sw(w + 0x50u, 0u);
    return 0u;
}

// orig: (host) see end_menu_tr.h
bool end_menu_widget_call(uint32_t veldin, uint32_t w, int32_t a1, int32_t& r) {
    const uint32_t a = static_cast<uint32_t>(a1);
    switch (veldin) {
        case 0x00283530: r = static_cast<int32_t>(end_menu_panel_update_tr(w, a)); return true;
        case 0x002838b0: r = static_cast<int32_t>(end_menu_panel_draw_tr(w, a)); return true;
        case 0x002833d0: r = static_cast<int32_t>(end_menu_panel_focus_in_tr(w, a)); return true;
        case 0x002834a8: r = static_cast<int32_t>(end_menu_panel_focus_out_tr(w, a)); return true;
        case 0x0027fed8: r = static_cast<int32_t>(end_menu_choice_update_tr(w, a)); return true;
        case 0x002806e0: r = static_cast<int32_t>(end_menu_choice_draw_tr(w, a)); return true;
        case 0x00280038: r = static_cast<int32_t>(end_menu_page23_update_tr(w, a)); return true;
        case 0x002803b0: r = static_cast<int32_t>(end_menu_page23_draw_tr(w, a)); return true;
        case 0x00280020: r = static_cast<int32_t>(end_menu_page23_focus_in_tr(w, a)); return true;
        default: return false;
    }
}

}  // namespace rac1
