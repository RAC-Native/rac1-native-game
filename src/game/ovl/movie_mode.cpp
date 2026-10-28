// Game mode 1: a movie playing, see movie_mode.h.
#include <game/ovl/movie_mode.h>

#include <game/lib/libcdvd.h>
#include <game/lib/libgraph.h>
#include <game/lib/movie_player.h>
#include <game/lib/snd989.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/movies.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/veldin_mobys.h>
#include <game/port_stubs.h>
#include <runtime/kernel/bootstrap.h>

namespace rac1 {

namespace {
constexpr OvConst<0x00179100u> TK{};  // the talk state (talk.cpp): +8 the moby, +0xc its talk struct
bool g_running = false;               // the player's loop going (the host's split of 0x2999a8)
bool g_stop = false;                  // movie_mode_stop: the player stopped at the next frame (host)

// The movie of game mode 1 (0x161048 sector, 0x16104c size, 0x161050 sound track, 0x161054 / 0x161058
// the buffers movie_start kept): the cache flushed, the sounds, the music and the disc's sound
// stream stopped, the memory card's machine run until it is idle (no operation running or asked),
// then the movie player's entry on the buffers rounded up to 64 bytes. Its loop runs a frame at a time.
// orig: 0x002999a8 (up to the player's loop)
void movie_mode_begin() {
    FlushCache(0);
    sound_stop_all();
    music_stop_all();
    snd_cd_status(0);
    while (lw(0x0013d290 + 0xd4) >= 3 || lw(0x0013d290 + 0xdc) >= 0) card_machine();
    const uint32_t sector = lwu(OV(0x00161048));
    const uint32_t size = lwu(OV(0x0016104c));
    const uint32_t buf = (lwu(OV(0x00161054)) + 0x3fu) & 0xffffffc0u;
    const uint32_t buf2 = (lwu(OV(0x00161058)) + 0x3fu) & 0xffffffc0u;
    movie_stream_start(sector, size, buf, buf2, lw(OV(0x00161050)));
    g_running = true;
}
}  // namespace

// orig: 0x002999a8
void movie_mode_update() {
    if (!g_running) movie_mode_begin();
    if (g_stop) {  // (host: the debug level change's stop, as the player's skip test stops it)
        g_stop = false;
        movie_player_stop();
    }
    if (!movie_stream_frame()) return;
    g_running = false;
    cd_sync(0);
    movie_mode_end();
}

// orig: (host) see movie_mode.h
bool movie_mode_stop() {
    if (lw(OV(0x0015f5c4)) != 1) return false;
    g_stop = true;
    return true;
}

// orig: 0x002999a8
void movie_mode_run() {
    movie_mode_begin();
    while (!movie_stream_frame()) {
    }
    g_running = false;
    cd_sync(0);
    movie_mode_end();
}

// orig: 0x002997c8
void movie_mode_end() {
    sw(OV(0x0015f5d8), 1);
    TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    gs_sync_path(0, 0);
    TODO_PORT(0x00122e68, "libgraph: the vsync callback set back (sceGsSyncVCallback 0x12f1c8)")(0x0012f1c8u);
    const bool pal = lw(0x0015ed80) != 0;
    const int32_t big = pal ? 0xe : 0xd;
    const int32_t small = pal ? 0xe : 0x11;
    uint32_t src = lwu(OV(0x00161058));
    uint32_t vram = lwu(0x0015ee88);
    const uint32_t lp = ee_stack_alloc(0x70);
    for (int32_t n = big; n != 0; --n) {
        gs_set_def_store_image(lp, static_cast<int32_t>(vram << 8) >> 16, 2, 1, 0, 0, 0x80, 0x80);
        vram += 0x10000;
        FlushCache(0);
        gs_exec_store_image(lp, src);
        src += 0xc000;
        gs_sync_path(0, 0);
    }
    vram = lwu(0x0015ee78);
    for (int32_t n = small; n != 0; --n) {
        gs_set_def_store_image(lp, static_cast<int32_t>(vram << 8) >> 16, 1, 0, 0, 0, 0x40, 0x40);
        vram += 0x4000;
        FlushCache(0);
        gs_exec_store_image(lp, src);
        src += 0x4000;
        gs_sync_path(0, 0);
    }
    ee_stack_free(0x70);
    if (lw(0x0015eed8) == 2) sw(0x0015eed8, 0);
    screen_fade(4);
    music_start(lh(0x001516d0 + 0x38), 1, 0x400);
    sw(OV(0x0015f5c4), 0);
    const uint32_t m = lwu(TK + 8);
    if (m != 0) {
        const uint32_t t = lwu(TK + 0xc);
        sw(TK + 0xc, 0);
        sw(TK + 8, 0);
        talk_step(m, t, 1);
    }
    sb(0x0013e550 + 0x6b, lbu(0x0013e550 + 0x6b) | 0x10u);
}

// orig: 0x00299560
void movie_story_play(int32_t n) {
    if (n < 0) return;
    const uint32_t e = 0x00137b80 + static_cast<uint32_t>(n) * 8;
    uint32_t size, sector;
    if (lw(0x0015ed80) != 0) {
        size = lwu(e + 0x199c);
        sector = lwu(e + 0x1998);
    } else {
        size = lwu(e + 0x193c);
        sector = lwu(e + 0x1938);
    }
    movie_start(sector, size, lwu(0x0015ed88));
}

// orig: 0x002994f8
void movie_table2_play(int32_t n) {
    if (n < 0) return;
    const uint32_t e = 0x00137b80 + static_cast<uint32_t>(n) * 8;
    uint32_t size, sector;
    if (lw(0x0015ed80) != 0) {
        size = lwu(e + 0x1a54);
        sector = lwu(e + 0x1a50);
    } else {
        size = lwu(e + 0x1a2c);
        sector = lwu(e + 0x1a28);
    }
    movie_start(sector, size, 0);
}

}  // namespace rac1
