// The movie player module's stand-in (see movie_player.h): the host's movie hardware
// (runtime/ipu/movie_hw) plays the stream, a picture each 1 / fps second of game time (the game
// runs at 60 frames a second, NTSC); the game's skip test runs each frame as in the original loop.
#include <game/lib/movie_player.h>

#include <game/lib/snd989.h>
#include <game/ovl/movies.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>
#include <runtime/iop/snd989_stream.h>
#include <runtime/ipu/movie_hw.h>

#include <cstdio>
#include <vector>

namespace rac1 {

namespace {
struct Player {
    uint32_t block = 0;   // the player's block (its stop flag at +0xa8)
    uint64_t frame = 0;   // game frames since the start
    uint64_t shown = 0;   // pictures shown
    bool stopping = false;
    std::vector<uint8_t> picture;
};
Player g_player;
}  // namespace

// orig: (host) stands in for 0x002ee130
bool movie_player_open(uint32_t lsn, uint32_t size, int32_t lang) {
    if (!movie_hw_open(lsn, size, lang)) {
        if (movie_hw_enabled()) std::printf("Can't Open movie\n");  // (the original's debug message, 0x1e9700)
        return false;
    }
    return true;
}

// orig: (host) stands in for 0x002eddd0 (its start)
void movie_player_loop_start(uint32_t player, uint32_t buf, uint32_t aux) {
    (void)buf;
    (void)aux;
    // FlushCache(0), FlushCache(2), the vsync wait
    g_player = Player{};
    g_player.block = player;
    sw(player + 0xa8, 0);
    float gl = 1.0f, gr = 1.0f;
    snd989_movie_gain(gl, gr);  // (the sound's voices, 0x3b / 0x3e of the IRX: orig_main's vsync mixes the track with these)
    std::printf("[movie] sound voices' gain %.4f / %.4f of the track (989snd group 5)\n", static_cast<double>(gl), static_cast<double>(gr));
}

// orig: (host) stands in for 0x002eddd0 (one pass of its loop, then its end)
bool movie_player_loop_frame() {
    Player& p = g_player;
    if (!p.stopping && movie_skip_test(p.block)) p.stopping = true;
    if (!p.stopping) {
        ++p.frame;
        const double fps = movie_hw_fps() > 0.0 ? movie_hw_fps() : 30.0;
        const auto want = static_cast<uint64_t>(static_cast<double>(p.frame) * fps / 60.0) + 1;
        bool decoded = false;
        while (p.shown < want) {
            if (!movie_hw_next_picture(p.picture, p.shown + 1 >= want)) {
                p.stopping = true;
                break;
            }
            ++p.shown;
            decoded = true;
        }
        if (decoded && !p.picture.empty()) movie_hw_show_swap(p.picture, movie_hw_width(), movie_hw_height());
        if (!p.stopping) return false;
    }
    std::printf("[movie] end after %llu frames, %llu pictures%s\n", static_cast<unsigned long long>(p.frame), static_cast<unsigned long long>(p.shown),
                lw(p.block + 0xa8) == 1 ? " (skipped)" : "");
    // the loop's end: the sound stopped (0x2ee680), the master volume of group 5 back, the
    // sound commands flushed
    snd_set_master_volume(5, lw(0x0013e550 + 0x5c));
    snd_flush();
    return true;
}

// orig: (host) the debug menu's skip
void movie_player_stop() {
    if (g_player.block != 0) sw(g_player.block + 0xa8, 1);
    g_player.stopping = true;
}

// orig: (host) stands in for 0x002ee3d8
void movie_player_close() {
    movie_hw_close();
    movie_hw_show_clear();
    g_player = Player{};
}

}  // namespace rac1
