// Ported overlay code: the music and the dialog streams (see music_streams.h).
#include <game/ovl/music_streams.h>

#include <game/ovl/ovl_reloc.h>
#include <game/lib/snd989.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t S = 0x001516d0;
constexpr uint32_t MUSIC = 0x0013a628;  // the level header's music sectors (one word a track)
constexpr uint32_t A = S + 0x34, B = S + 0x50, C = S + 0x6c;  // the stream slots

// orig: (helper) repeated in 0x00264c18 / 0x00264cf0 / 0x00264dd8 / 0x00264fa8 / 0x00265078 /
// 0x00265148 — the dialog slot taken (state 1, reply awaited) and the line's stream started
// paused (flags 0x21) in volume group 2
void dialog_start(uint32_t lsn, int32_t id, int32_t a, int32_t vol) {
    if (lsn == 0) return;
    if (lw(B) != 0) return;
    sh(B + 0xa, 1);
    sw(B + 0x14, 0xa);
    sw(B + 0x18, 0xbb80);
    sw(B, 0xffffffffu);
    sh(B + 4, static_cast<uint32_t>(id));
    sh(B + 8, static_cast<uint32_t>(a));
    sh(B + 6, static_cast<uint32_t>(vol));
    sh(B + 0x10, 0);
    snd_stream_play(lsn, 0, 0, 0, static_cast<int16_t>(vol), 0, 2, 0, 0x21, OVF(0x002661d8), B);
}
}  // namespace

// The streams' frame: the music slot's restart (flag 1, no next track) or its looped part
// queued (state 8); every 7 s a change of track (+0x22) faded over to (+0x23) or taken; a
// dialog line asked for (+0x1c) started (an old one stopped first); the music's sequence
// (+0x20: 2 fading out by the time left, 3 the next track preloaded, 4 continued once the
// other ends, 5 waiting for the second slot); the three slots' updates; then the stream
// system's disc read: its end awaited, or a failed one retried.
// orig: 0x00265aa8
void music_streams_frame() {
    if (lbu(S + 0xb) != 0) return;
    if ((lh(S + 0x40) & 0x8000) == 0 && (lh(S + 0x3e) & 0x8000) == 0) {
        bool check8 = true;
        if (lw(A) == 0) {
            const uint32_t f = lhu(S + 0x3c);
            if ((f & 1) != 0 && static_cast<int8_t>(lbu(S + 0x22)) == -1) {
                music_start(lh(S + 0x38), static_cast<int16_t>(f), lh(S + 0x3a));
                check8 = false;
            }
        }
        if (check8) {
            const int32_t st = lh(S + 0x3e);
            if (st != 9) {
                const uint32_t h = lwu(A);
                if (h != 0 && h != 0xffffffffu && st == 8) music_queue_next(lh(S + 0x38), lh(S + 0x3c), lh(S + 0x3a));
            }
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(S + 0x2c))) != 0) {
        const int32_t next = static_cast<int8_t>(lbu(S + 0x22));
        if (next != -1 && lh(S + 0x20) == 0) {
            if (lh(S + 0x38) == next) {
                sb(S + 0x22, 0xff);
            } else {
                const int32_t over = static_cast<int8_t>(lbu(S + 0x23));
                if (over == -1 || music2_start(next, over, lh(S + 0x3c), lh(S + 0x3a)) != 0) {
                    const int32_t t = frame_scale_i(7);
                    sw(S + 0x2c, static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(t) * 60.0f)));
                }
            }
        }
    }
    const int32_t line = lw(S + 0x1c);
    if (line >= 0) {
        if (lw(B) != 0) {
            if (!(static_cast<uint32_t>(lhu(B + 0xa)) - 6u < 2u)) sh(B + 0xa, 5);
        } else {
            dialog_play(line, 0, 0x400);
            sw(S + 0x1c, 0xffffffffu);
        }
    }
    const int32_t sa = lh(S + 0x3e);
    if (sa != 9 && lwu(A) != 0xffffffffu && (lh(S + 0x40) & 0x8000) == 0 && (sa & 0x8000) == 0 && (lh(S + 0x78) & 0x8000) == 0 &&
        (lh(S + 0x76) & 0x8000) == 0) {
        const int32_t sc = lh(S + 0x76);
        switch (lh(S + 0x20)) {
        case 2: {
            const int32_t total = lw(S + 0x28);
            const int32_t left = total - (lw(S + 0x24) - lw(S + 0x84));
            if (total == 0) TODO_PORT(0x00265d1c, "break 7: the fade's divide by zero")();
            const int32_t vol = ee_div(static_cast<int32_t>(static_cast<uint32_t>(lh(S + 0x3a)) * static_cast<uint32_t>(left)), total);
            bool done = vol <= 0;
            if (!done) {
                const bool other = (sc == 4 && lh(S + 0x7c) != 0) || lw(C) != 0;
                done = !other || lw(A) == 0;
            }
            if (done) {
                sh(S + 0x20, 3);
                sh(S + 0x3e, 5);
            } else if (lh(S + 0x3e) != 9) {
                const uint32_t h = lwu(A);
                sw(A, 0xffffffffu);
                snd_set_sound(h, 5, vol, 0, 0, 0, OVF(0x00266380), A);
            }
            break;
        }
        case 3:
            if (sa != 0) break;
            if (lh(S + 0x7c) == 0 && lw(C) != 0) break;
            music_preload(static_cast<int8_t>(lbu(S + 0x22)), lh(S + 0x3c), lh(S + 0x3a));
            sh(S + 0x20, 4);
            sb(S + 0x22, 0xff);
            break;
        case 4:
            if (sa != 3) break;
            if (!(lw(S + 0x84) < lw(S + 0x28)) && sc == 4) {
                if (lh(S + 0x7c) != 0) break;
                if (lw(C) != 0) break;
            }
            snd_cmd_2e(lwu(A));  // continue the preloaded track
            sh(S + 0x20, 5);
            sh(S + 0x3e, 8);
            break;
        case 5:
            if (sc != 4) {
                sh(S + 0x20, 0);
            } else if (lh(S + 0x7c) == 0) {
                sh(S + 0x20, 0);
            }
            break;
        default:
            break;
        }
    }
    stream_slot_update(A);
    stream_slot_update(C);
    stream_slot_update(B);
    if (lbu(S + 0xa) != 0) {
        if (snd_cd_status(1) != 0) return;
        sb(S + 0xa, 0);
        sh(S + 8, 0);
        return;
    }
    const int32_t r = lh(S + 8);
    if (r != 2) return;
    sh(S + 8, 0);
    stream_disc_read(lwu(S + 0x14), lwu(S + 0xc), lwu(S + 0x10));
    if (lh(S + 8) == 0) sh(S + 8, static_cast<uint32_t>(r));
}

// A stream slot's frame: state 5 stops it (6), a pause request (+0xc 0x8000) pauses it until its
// timer +0xe runs out (then +0xc = 4), a pause lifted continues it; a slot started (2) asks
// whether it plays, a playing one (4..7) its time left and whether it still plays (the handle
// -1 until the reply); a slot whose sound is gone is freed.
// orig: 0x002658c0
void stream_slot_update(uint32_t r) {
    const int32_t st = lh(r + 0xa);
    const uint32_t h = lwu(r);
    if (st == 9 || h == 0 || h == 0xffffffffu) {
        if (st == 7) {
            sh(r + 0xa, 0);
        } else if (lw(r) == 0) {
            sh(r + 0xa, 0);
        }
        return;
    }
    if (st == 5) {
        snd_stop_sound(h);
        sh(r + 0xa, 6);
    }
    if (lw(r) == 0) return;
    if ((lh(r + 0xc) & 0x8000) != 0) {
        if ((lh(r + 0xa) & 0x8000) == 0) {
            snd_stream_pause(lwu(r));
            sh(r + 0xa, lhu(r + 0xa) | 0x8000u);
        }
        if (timer_tick_i16(ee_ref<int16_t>(r + 0xe)) == 2) sh(r + 0xc, 4);
    } else if ((lh(r + 0xa) & 0x8000) != 0) {
        snd_cmd_2e(lwu(r));  // continue
        sh(r + 0xa, lhu(r + 0xa) ^ 0x8000u);
    }
    if ((lh(r + 0xa) & 0x8000) != 0) return;
    const uint32_t s = lhu(r + 0xa);
    const int32_t ss = static_cast<int16_t>(s);
    if (ss == 1 || ss == 8 || ss == 9) return;
    if (s - 2u < 2u) {
        if (lwu(r) == 0xffffffffu) return;
        if (ss != 2) return;
        snd_stream_started(lwu(r), OVF(0x002661a8), r);
        return;
    }
    const uint32_t hh = lwu(r);
    sw(r, 0xffffffffu);
    snd_stream_remaining(hh, OVF(0x002663d8), r);
    snd_sound_is_playing(hh, OVF(0x00266380), r);
}

// The music track `track` started (slot state 1, flags, volume) in volume group 1, unless the
// slot is busy or the level has no such track; its handle to 0x266340.
// orig: 0x00265418
void music_start(int32_t track, int32_t flags, int32_t vol) {
    if (lw(A) != 0) return;
    const uint32_t lsn = lwu(MUSIC + static_cast<uint32_t>(track) * 4);
    if (lsn == 0) return;
    sh(A + 0xa, 1);
    sh(A + 4, static_cast<uint32_t>(track));
    sw(A + 0x14, 0xa);
    sw(A + 0x18, 0xbb80);
    sw(A, 0xffffffffu);
    sh(A + 8, static_cast<uint32_t>(flags));
    sh(A + 6, static_cast<uint32_t>(vol));
    sh(A + 0x10, 0);
    snd_stream_play(lsn, 0, 0, 0, static_cast<int16_t>(vol), 0, 1, 0, 0x20, OVF(0x00266340), A);
}

// The same, the stream started paused (flags 0x21; continued by the sequence), reply 0x266238.
// orig: 0x00265340
void music_preload(int32_t track, int32_t flags, int32_t vol) {
    if (lw(A) != 0) return;
    const uint32_t lsn = lwu(MUSIC + static_cast<uint32_t>(track) * 4);
    if (lsn == 0) return;
    sh(A + 0xa, 1);
    sh(A + 4, static_cast<uint32_t>(track));
    sw(A + 0x14, 0xa);
    sw(A + 0x18, 0xbb80);
    sw(A, 0xffffffffu);
    sh(A + 8, static_cast<uint32_t>(flags));
    sh(A + 6, static_cast<uint32_t>(vol));
    sh(A + 0x10, 0);
    snd_stream_play(lsn, 0, 0, 0, static_cast<int16_t>(vol), 0, 1, 0, 0x21, OVF(0x00266238), A);
}

// The track's next part (the table's next entry) queued after the playing music stream, looped
// when the slot's flag 1 is set (stream flags 0x24); the slot waits in state 9 (reply 0x2662e8).
// orig: 0x002654f0
void music_queue_next(int32_t track, int32_t flags, int32_t vol) {
    if (lh(A + 0xa) == 9) return;
    const uint32_t h = lwu(A);
    if (h == 0 || h == 0xffffffffu) return;
    const uint32_t lsn = lwu(MUSIC + static_cast<uint32_t>(track + 1) * 4);
    if (lsn == 0) return;
    sh(A + 4, static_cast<uint32_t>(track));
    sh(A + 8, static_cast<uint32_t>(flags));
    sh(A + 0xa, 9);
    sw(A + 0x14, 0xa);
    sw(A + 0x18, 0xbb80);
    sh(A + 6, static_cast<uint32_t>(vol));
    sh(A + 0x10, 0);
    snd_stream_play(lsn, 0, 0, 0, static_cast<int16_t>(vol), 0, 1, h, (flags & 1) != 0 ? 0x24u : 0x20u, OVF(0x002662e8), A);
}

// The second music slot's track `track` started (state 1, +0x10 set: the sequence follows it),
// reply 0x266298. 1 when started.
// orig: 0x002655d8
int32_t music2_start(int32_t a, int32_t track, int32_t b, int32_t vol) {
    if (lw(C) != 0) return 0;
    const uint32_t lsn = lwu(MUSIC + static_cast<uint32_t>(track) * 4);
    if (lsn == 0) return 0;
    sh(C + 8, static_cast<uint32_t>(b));
    sw(C + 0x14, 0xa);
    sw(C + 0x18, 0xbb80);
    sw(C, 0xffffffffu);
    sh(C + 4, static_cast<uint32_t>(a));
    sh(C + 0x10, 1);
    sh(C + 0xa, 1);
    sh(C + 6, static_cast<uint32_t>(vol));
    snd_stream_play(lsn, 0, 0, 0, static_cast<int16_t>(vol), 0, 1, 0, 0x20, OVF(0x00266298), C);
    return 1;
}

// A dialog line by its id: the ranges from 10000 up have their tables in the level header
// (0x137b80..) or the ELF (30000..: 0x11c178 by language), below the level's own table
// 0x13a664 (0x250 bytes a line, by language 0x15ed88).
// orig: 0x00265148
void dialog_play(int32_t id, int32_t a, int32_t vol) {
    if (0xea5f < id) {
        dialog_play_60000(id, a, vol);
    } else if (0xc34f < id) {
        dialog_play_50000(id, a, vol);
    } else if (0x9c3f < id) {
        dialog_play_40000(id, a, vol);
    } else if (!(id < 0x7530)) {
        dialog_play_30000(id, a, vol);
    } else if (!(id < 0x4e20)) {
        dialog_play_20000(id, a, vol);
    } else if (!(id < 0x2710)) {
        dialog_play_10000(id, a, vol);
    } else {
        const uint32_t p = static_cast<uint32_t>(id) * 0x250u + lwu(0x0015ed88) * 4 + 0x0013a664;
        dialog_start(lwu(p), id, a, vol);
    }
}

// orig: 0x00264c18
void dialog_play_60000(int32_t id, int32_t a, int32_t vol) {
    dialog_start(lwu(0x00137b80 + static_cast<uint32_t>(id - 60000) * 8 + 0x1618), id, a, vol);
}
// orig: 0x00264cf0
void dialog_play_50000(int32_t id, int32_t a, int32_t vol) {
    dialog_start(lwu(0x00137b80 + lwu(0x0015ed88) * 4 + static_cast<uint32_t>(id - 50000) * 0x18 + 0xf00), id, a, vol);
}
// orig: 0x00264dd8
void dialog_play_40000(int32_t id, int32_t a, int32_t vol) {
    dialog_start(lwu(0x00137b80 + static_cast<uint32_t>(id - 40000) * 8 + 0x13c0), id, a, vol);
}

// The 30000.. lines (the ELF's table by language, 0x258 bytes a language); their volume 0
// unless the option 0x15ee1c is on.
// orig: 0x00264eb0
void dialog_play_30000(int32_t id, int32_t a, int32_t vol) {
    const uint32_t lsn = lwu(static_cast<uint32_t>(id) * 4 + lwu(0x0015ed88) * 0x258 + 0x0011c178);
    if (lsn == 0) return;
    if (lw(B) != 0) return;
    sw(B, 0xffffffffu);
    sh(B + 0xa, 1);
    sh(B + 4, static_cast<uint32_t>(id));
    sh(B + 8, static_cast<uint32_t>(a));
    sw(B + 0x14, 0xa);
    sw(B + 0x18, 0xbb80);
    sh(B + 0x10, 0);
    sh(B + 6, static_cast<uint32_t>(vol));
    const int32_t v = lbu(0x0015ee1c) != 0 ? static_cast<int16_t>(vol) : 0;
    snd_stream_play(lsn, 0, 0, 0, v, 0, 2, 0, 0x21, OVF(0x002661d8), B);
}
// orig: 0x00264fa8
void dialog_play_20000(int32_t id, int32_t a, int32_t vol) {
    dialog_start(lwu(0x00137b80 + static_cast<uint32_t>(id - 20000) * 8 + 0x2988), id, a, vol);
}
// orig: 0x00265078
void dialog_play_10000(int32_t id, int32_t a, int32_t vol) {
    dialog_start(lwu(0x00137b80 + static_cast<uint32_t>(id - 10000) * 8 + 0x1a0), id, a, vol);
}

// A disc read through the stream system (`n` sectors from `lsn` to `buf`) unless one is going:
// recorded for a retry; its byte count, 0 when it could not start (a message, the debug trap).
// orig: 0x00265fa0
int32_t stream_disc_read(uint32_t buf, uint32_t lsn, uint32_t n) {
    if (lh(S + 8) != 0 || n == 0) return 0;
    if (snd_stream_cd_read(lsn, n, buf, S + 0x30) != 0) {
        sw(S + 0x14, buf);
        sh(S + 8, 1);
        sw(S + 0xc, lsn);
        sw(S + 0x10, n);
        return static_cast<int32_t>(n << 11);
    }
    debug_printf_stub();  // 0x1e90f8
    debug_trap();
    return 0;
}

// Every stream stopped (the level restarts): the replies awaited, 989snd's streams stopped and
// its reads finished, the slots cleared, the next track (if any) made the current one.
// orig: 0x002656c0
void music_stop_all() {
    while (lwu(A) == 0xffffffffu) snd_flush();
    while (lwu(C) == 0xffffffffu) snd_flush();
    while (lwu(B) == 0xffffffffu) snd_flush();
    snd_stream_stop_all();
    while (snd_flush() != 0) {
    }
    snd_stream_reads_wait(1);
    const int32_t next = static_cast<int8_t>(lbu(S + 0x22));
    sh(A + 0xa, 0);
    sh(A + 8, 0);
    sw(A, 0);
    if (next != -1) sh(A + 4, static_cast<uint32_t>(next));
    sb(S + 0x23, 0xff);
    sh(B + 0xa, 0);
    sh(B + 8, 0);
    sw(B, 0);
    sh(C + 0xa, 0);
    sh(C + 8, 0);
    sw(C, 0);
    sh(S + 0x20, 0);
    sb(S + 0x22, 0xff);
}

// The stream system's disc read finished (989snd's stream callback, `reply` 1): its error marks
// it for a retry (2).
// orig: 0x00266168
void stream_read_done(int32_t reply, uint64_t data) {
    (void)data;
    if (reply != 1) return;
    sh(S + 8, 0);
    if (snd_cd_error() != 0) sh(S + 8, 2);
}

// orig: 0x002661a8
void stream_started_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0 || reply == 0) return;
    if (lh(r + 0xa) == 2) sh(r + 0xa, 3);
}

// The dialog's handle; none: started again.
// orig: 0x002661d8
void dialog_play_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    sw(r, static_cast<uint32_t>(reply));
    if (reply != 0) {
        if (lh(r + 0xa) == 1) sh(r + 0xa, 2);
        return;
    }
    dialog_play(lh(S + 0x54), lh(S + 0x58), lh(S + 0x56));
}

// orig: 0x00266238
void music_preload_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    sw(r, static_cast<uint32_t>(reply));
    if (reply != 0) {
        if (lh(r + 0xa) == 1) sh(r + 0xa, 2);
        return;
    }
    music_preload(lh(S + 0x38), lh(S + 0x3c), lh(S + 0x3a));
}

// orig: 0x00266298
void music2_play_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    sw(r, static_cast<uint32_t>(reply));
    if (reply == 0) {
        sh(r + 0xa, 0);
        return;
    }
    const int32_t st = lh(r + 0xa);
    if (st != 1) return;
    const int32_t f = lh(r + 0x10);
    sh(r + 0xa, 4);
    if (f == 0) return;
    sh(S + 0x20, static_cast<uint32_t>(st));
}

// The queued part's reply (1, or an error < 0 kept as the handle): the slot plays (4).
// orig: 0x002662e8
void music_queue_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    if (reply < 0) sw(r, static_cast<uint32_t>(reply));
    if (reply == 0) {
        sh(r + 0xa, 0);
        return;
    }
    if (lh(r + 0xa) != 9) return;
    const int32_t f = lh(r + 0x10);
    sh(r + 0xa, 4);
    if (f == 0) return;
    sh(S + 0x20, 1);
}

// orig: 0x00266340
void music_play_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    sw(r, static_cast<uint32_t>(reply));
    if (reply == 0) {
        sh(r + 0xa, 0);
        return;
    }
    if (lh(r + 0xa) == 1) sh(r + 0xa, 8);
}

// Still playing: the handle back (the slot awaited it, else a message); ended: state 7.
// orig: 0x00266380
void stream_playing_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    if (lwu(r) != 0xffffffffu) {
        debug_printf_stub();  // 0x160128
        return;
    }
    sw(r, static_cast<uint32_t>(reply));
    if (reply == 0) sh(r + 0xa, 7);
}

// The time left (+0x18); a slot followed by the sequence (+0x10) waiting (1) starts its fade
// (2) over that time (+0x24, a quarter of it +0x28).
// orig: 0x002663d8
void stream_remaining_reply(int32_t reply, uint64_t data) {
    const uint32_t r = static_cast<uint32_t>(data);
    if (r == 0) return;
    const int32_t f = lh(r + 0x10);
    sw(r + 0x18, static_cast<uint32_t>(reply));
    if (f == 0) return;
    if (lh(S + 0x20) != 1) return;
    if (reply == 0) return;
    sh(S + 0x20, 2);
    sw(S + 0x24, lwu(r + 0x18));
    const int32_t t = lw(r + 0x18);
    sw(S + 0x28, static_cast<uint32_t>((t < 0 ? t + 3 : t) >> 2));
}

// orig: (none) — the replies' callbacks by address
void music_streams_callbacks_register() {
    snd_register_callback(0x00266168, stream_read_done);
    snd_register_callback(0x002661a8, stream_started_reply);
    snd_register_callback(0x002661d8, dialog_play_reply);
    snd_register_callback(0x00266238, music_preload_reply);
    snd_register_callback(0x00266298, music2_play_reply);
    snd_register_callback(0x002662e8, music_queue_reply);
    snd_register_callback(0x00266340, music_play_reply);
    snd_register_callback(0x00266380, stream_playing_reply);
    snd_register_callback(0x002663d8, stream_remaining_reply);
}

}  // namespace rac1
