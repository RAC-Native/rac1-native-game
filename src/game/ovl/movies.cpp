// Ported overlay code: the story's picture cards and movies. The level exit
// 0x292e08 shows them between some levels (a new game's Veldin, Veldin to Novalis, ...); the
// boot module's copy of the same code shows the new game's ones.
// Their loops run one game frame per call (the host's frame), as the level exit's do.
#include <game/ovl/memory_card.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/movies.h>

#include <game/boot/level_read.h>
#include <game/lib/libcdvd.h>
#include <game/lib/libgraph.h>
#include <game/lib/movie_player.h>
#include <game/lib/snd989.h>
#include <game/math/vecmath.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>
#include <game/enhance/advanced.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001611c0u> PKT{};   // the frame's packet cursor
constexpr uint32_t SAVE = 0x0013d290;  // the save block (+0xd4 / +0xdc: the memory card's state)
// orig: (helper) a 32-bit result as the EE's 64-bit registers hold it (sign-extended)
int64_t sx(int32_t v) { return static_cast<int64_t>(v); }
}  // namespace

// The card file of the language (sectors from the table of contents 0x137b80 + 0x1388 + lang * 8)
// read 1 MB into the buffer *(0x173e14) and unpacked at its start; its images (a table of offsets
// from +0: image k at +*(k * 4), a 0x20-byte header, the palette 16 x 16 CT32, the texels PSMT8)
// uploaded to the GS from the texture start 0x15ee8c (0x15ee74 / 0x15ee78 the cursors): image 1
// (64 x 64), first + 2 and last + 2 (512 x 64). The upload addresses (cursor >> 8) make the three
// TEX0s (CLUT with each image's palette).
// orig: 0x00292690
void card_images_load(int32_t lang, int32_t first, int32_t last, uint32_t out0, uint32_t out1, uint32_t out2) {
    const uint32_t toc = 0x00137b80 + static_cast<uint32_t>(lang) * 8;
    cd_read_request(lwu(OV(0x00173e14)) + 0x100000u, lwu(toc + 0x1388), lwu(toc + 0x138c));
    cd_sync(0);
    // FlushCache(0)
    wad_decompress(lwu(OV(0x00173e14)) + 0x100000u, lwu(OV(0x00173e14)));
    // FlushCache(0)
    const uint32_t base = lwu(OV(0x00173e14));
    const uint32_t img_last = static_cast<uint32_t>(last) * 4 + base;
    const uint32_t img_first = static_cast<uint32_t>(first) * 4 + base;
    sw(0x0015ee78, lwu(0x0015ee8c));
    sw(0x0015ee74, lwu(0x0015ee8c));
    const uint32_t lp = ee_stack_alloc(0x60);
    uint32_t page[6];
    for (int32_t k = 0; k < 6; ++k) {
        const int32_t dbp = static_cast<int32_t>(lwu(0x0015ee74) << 8) >> 16;
        uint32_t bytes = 0, src = 0;
        switch (k) {
            case 0:
                gs_set_def_load_image(lp, dbp, 1, 0, 0, 0, 0x10, 0x10);
                bytes = 0x400;
                src = base + lwu(base + 4) + 0x20;
                break;
            case 1:
                gs_set_def_load_image(lp, dbp, 1, 0x13, 0, 0, 0x40, 0x40);
                bytes = 0x1000;
                src = base + lwu(base + 4) + 0x420;
                break;
            case 2:
                gs_set_def_load_image(lp, dbp, 1, 0, 0, 0, 0x10, 0x10);
                bytes = 0x400;
                src = base + lwu(img_first + 8) + 0x20;
                break;
            case 3:
                gs_set_def_load_image(lp, dbp, 8, 0x13, 0, 0, 0x200, 0x40);
                bytes = 0x8000;
                src = base + lwu(img_first + 8) + 0x420;
                break;
            case 4:
                gs_set_def_load_image(lp, dbp, 1, 0, 0, 0, 0x10, 0x10);
                bytes = 0x400;
                src = base + lwu(img_last + 8) + 0x20;
                break;
            default:
                gs_set_def_load_image(lp, dbp, 8, 0x13, 0, 0, 0x200, 0x40);
                bytes = 0x8000;
                src = base + lwu(img_last + 8) + 0x420;
                break;
        }
        // FlushCache(0)
        gs_exec_load_image(lp, src);
        gs_sync_path(0, 0);
        const uint32_t cur = lwu(0x0015ee74);
        page[k] = static_cast<uint32_t>(static_cast<int32_t>(cur) >> 8);
        sw(0x0015ee74, cur + bytes);
    }
    ee_stack_free(0x60);
    // TEX0: TBP0 the texels, TBW / PSM T8 / TW 512 or 64 / TH 64 / TCC / TFX DECAL ... (0x19304000
    // for 64 wide, 0x25320000 for 512), CBP the palette, CPSM CT32 / CSM / CLD (0x580000000),
    // and bit 63
    const uint64_t cld = 0xb000ull << 19, top = 0x8000000000000000ull;
    auto tex0 = [cld, top](uint32_t texels, uint32_t pal, uint64_t bits) {
        return static_cast<uint64_t>(sx(static_cast<int32_t>(texels))) | bits | (static_cast<uint64_t>(sx(static_cast<int32_t>(pal))) << 37) | cld | top;
    };
    sd(out0, tex0(page[1], page[0], 0x19304000ull));
    sd(out1, tex0(page[3], page[2], 0x25320000ull));
    sd(out2, tex0(page[5], page[4], 0x25320000ull));
}

// A sprite into the frame's packets: the DMA tag (7 quadwords) and VIF DIRECT, the GIF tag
// 0x160910, then TEX0, the colour with Q 1.0, and the four corners (ST from the floats, XYZ in
// 1/16 pixels from the screen's origin 0x13e510 / 0x13e514, less half a pixel, Z 0xfffff0).
// orig: 0x00292500
void card_sprite(int32_t x, int32_t y, int32_t w, int32_t h, uint64_t colour, uint64_t tex0, float s0, float s1, float t0, float t1) {
    const int32_t ox = lw(0x0013e510), oy = lw(0x0013e514);
    const int64_t xa = sx((x << 4) + ox - 8);
    const int64_t xb = sx(((x + w) << 4) + ox - 8);
    const int64_t ya = sx((y << 4) + oy - 8);
    const int64_t yb = sx(((y + h) << 4) + oy - 8);
    const uint32_t p = lwu(PKT);
    sw(p, 0x10000007);
    sw(lwu(PKT) + 4, 0);
    sw(lwu(PKT) + 8, 0);
    sw(lwu(PKT) + 0xc, 0x50000007);
    const uint32_t t3 = lwu(PKT);
    sw(PKT, t3 + 0x10);
    lq_sq(t3 + 0x10, OV(0x00160910));
    const uint32_t g = t3 + 0x20;
    sw(PKT, g);
    const uint64_t z = 0xfffff000ull << 24;
    auto st = [](float s, float t) { return static_cast<uint64_t>(f2u(s)) | (static_cast<uint64_t>(f2u(t)) << 32); };
    sd(g, tex0);
    sd(g + 8, 0x54);
    sd(g + 0x10, (colour & 0xffffffffull) | (0xfe00ull << 46));
    sd(g + 0x18, st(s0, t0));
    sd(g + 0x20, static_cast<uint64_t>(xa) | static_cast<uint64_t>(ya << 16) | z);
    sd(g + 0x28, st(s1, t0));
    sd(g + 0x30, static_cast<uint64_t>(xb) | static_cast<uint64_t>(ya << 16) | z);
    sd(g + 0x38, st(s0, t1));
    sd(g + 0x40, static_cast<uint64_t>(xa) | static_cast<uint64_t>(yb << 16) | z);
    sd(g + 0x48, st(s1, t1));
    sd(g + 0x50, static_cast<uint64_t>(xb) | static_cast<uint64_t>(yb << 16) | z);
    sd(g + 0x58, 0);
    sw(PKT, lwu(PKT) + 0x60);
}

// ---- the picture card (0x2929f0), one frame a call ------------------------------------------

namespace {
struct Card {
    int32_t first = 0, last = 0, frames = 0, load = 0;  // a1 / s8, s4, s6
    int32_t i = 0;                                       // s1: the frame
    uint64_t tex[3] = {};                                // sp + 0x10 / 0x18 / 0x20
    bool running = false;
    bool load_done = false;
};
Card g_card;

// orig: (helper) the loop's test of 0x002929f0: frames left, and the memory card idle (its state
// +0xd4 < 3, no operation +0xdc)
bool card_more(const Card& c) {
    if (!(c.i < c.frames)) return false;
    if (!(lw(SAVE + 0xd4) < 3)) return false;
    return lw(SAVE + 0xdc) < 0;
}

// orig: (helper) the end of 0x002929f0: a fade of 2 frames
void card_end() {
    screen_fade(2);
    g_card.running = false;
}
}  // namespace

// The card's start: its images loaded (0x292690); with `load`, the next level's header read and
// its load's steps reset (0x15ee48 / 0x15ee4a); the frame's packets reset.
// orig: 0x002929f0 (up to its loop)
void card_start(int32_t lang, int32_t first, int32_t last, int32_t frames, int32_t load) {
    Card& c = g_card;
    c = Card{};
    c.first = first;
    c.last = last;
    c.frames = frames;
    c.load = load;
    const uint32_t out = ee_stack_alloc(0x20);
    card_images_load(lang, first, last, out, out + 8, out + 0x10);
    c.tex[0] = ld(out);
    c.tex[1] = ld(out + 8);
    c.tex[2] = ld(out + 0x10);
    ee_stack_free(0x20);
    if (load != 0) {
        level_header_read(lw(0x0015ed84));
        sh(0x0015ee48, 0);
        sh(0x0015ee4a, 0);
    }
    TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    sub_002a07f8();
    c.running = true;
    if (!(0 < frames) || !card_more(c)) card_end();
}

// One frame of the card: the static packets, the frame's GS start, the alpha ramp (in over 32
// frames, out over the last 16: 0x80 at most), the background's scroll (frame % 600 / 600), the
// card (one: the background, then the card at y - 0x20; two: at y - 0x2e, then from frame 0x41 the
// second over them fading in until 0x60, at y); the frame's end and, while `load`, a step of the
// next level's load (the card kept 20 frames past the current one until it is in). True at the end.
// orig: 0x002929f0 (one pass of its loop, then its end)
bool card_frame() {
    Card& c = g_card;
    if (!c.running) return true;
    render_static_packets();
    int32_t alpha = 0x80;
    sub_00201040();
    sub_002a0ba8(1, 0x80000000ull);
    sub_002a0ba8(8, 0);
    sw(lwu(PKT), 0x30000014);
    if (!(0x1f < c.i)) alpha = c.i * 4;
    sw(lwu(PKT) + 4, 0x0013cdd0);
    sw(lwu(PKT) + 8, 0);
    sw(lwu(PKT) + 0xc, 0x50000014);
    sw(PKT, lwu(PKT) + 0x10);
    if (c.frames - 0x10 < c.i) alpha = (c.frames - c.i) << 3;
    const float scroll = fpu_mul(int_to_float(ee_rem(c.i, 0x258)), u2f(0x3ada740e));
    const float t0 = fpu_add(scroll, 0.0f), t1 = fpu_add(scroll, u2f(0x3ecccccd));
    const uint64_t grey = 0x80808080ull;
    if (c.first == c.last) {
        const int32_t y = lw(0x0013e500 + 0xc) - 0x20;
        card_sprite(0, y, 0x200, 0x40, static_cast<uint64_t>(sx(static_cast<int32_t>((static_cast<uint32_t>(alpha) << 24) | 0x808080u))), c.tex[0], 0.0f, 4.0f, t0, t1);
        glyph_sprite(0, lw(0x0013e500 + 0xc) - 0x20, 0x200, 0x40, 0, 0, 0x200, 0x40, grey, c.tex[1]);
    } else {
        card_sprite(0, lw(0x0013e500 + 0xc) - 0x2e, 0x200, 0x40, static_cast<uint64_t>(sx(static_cast<int32_t>((static_cast<uint32_t>(alpha) << 24) | 0x808080u))), c.tex[0], 0.0f, 4.0f, t0, t1);
        glyph_sprite(0, lw(0x0013e500 + 0xc) - 0x2e, 0x200, 0x40, 0, 0, 0x200, 0x40, grey, c.tex[1]);
        if (!(c.i < 0x41)) {
            if (c.i < 0x60) alpha = (c.i - 0x40) << 2;
            card_sprite(0, lw(0x0013e500 + 0xc), 0x200, 0x40, static_cast<uint64_t>(sx(static_cast<int32_t>((static_cast<uint32_t>(alpha) << 24) | 0x808080u))), c.tex[0], 0.0f, 4.0f, t0, t1);
            glyph_sprite(0, lw(0x0013e500 + 0xc), 0x200, 0x40, 0, 0, 0x200, 0x40, grey, c.tex[2]);
        }
    }
    card_machine();
    card_ui_machine();
    dma_wait(1);
    TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    TODO_PORT(0x001f8540, "the GS display registers")();
    TODO_PORT(0x002a08c8, "DMA: packet kick")();
    sub_002a0858();
    if (c.load != 0) {
        if (level_load_step() != 0) {
            c.load = 0;
            c.load_done = true;
        } else if (c.frames < c.i + 0x14) {
            c.frames = c.i + 0x14;
        }
    }
    ++c.i;
    if (card_more(c)) return false;
    card_end();
    return true;
}

bool card_load_done() { return g_card.load_done; }

// ---- the movies (0x292420) ------------------------------------------------------------------

namespace {
// orig: (helper) the end of 0x00292420, after the player
void movie_end() {
    TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    gs_sync_path(0, 0);
    TODO_PORT(0x00122e68, "libgraph: the vsync callback set back (sceGsSyncVCallback 0x12f1c8)")(0x0012f1c8u);
    screen_fade(4);
    sb(0x0013e550 + 0x6b, lbu(0x0013e550 + 0x6b) | 0x10);
}
bool g_movie_running = false;
}  // namespace

// Movie `k`: its sector and size from the table of contents (NTSC +0x1938, PAL +0x1998), the HUD
// flag 8 set, the player run in the buffer *(0x173e14) (64-aligned) and 3 MB past it, with the
// game's language as its sound track.
// orig: 0x00292420 (up to the player's loop)
void movie_start(int32_t k) {
    const uint32_t row = 0x00137b80 + static_cast<uint32_t>(k) * 8;
    const bool pal = lw(0x0015ed80) != 0;
    const uint32_t size = lwu(row + (pal ? 0x199c : 0x193c));
    const uint32_t lsn = lwu(row + (pal ? 0x1998 : 0x1938));
    const uint32_t buf = lwu(OV(0x00173e14));
    sb(0x0013e550 + 0x6b, lbu(0x0013e550 + 0x6b) | 8);
    movie_stream_start(lsn, size, (buf + 0x3f) & ~0x3fu, (buf + 0x30003f) & ~0x3fu, lw(0x0015ed88));
    g_movie_running = true;
}

// One frame of the movie; its end (0x292420's) when the player is done. True at the end.
// orig: 0x00292420 (the player's loop, then its end)
bool movie_frame() {
    if (!g_movie_running) return true;
    if (!movie_stream_frame()) return false;
    g_movie_running = false;
    movie_end();
    return true;
}

// ---- the player's entry (0x2edd28) --------------------------------------------------------

namespace {
struct Stream {
    bool running = false;   // the player's loop going
    bool opened = false;    // 0x2ee130 succeeded
    uint32_t buf2 = 0;
};
Stream g_stream;

// orig: (helper) the end of 0x002edd28: the player closed (0x2ee3d8), its buffer pointers cleared
void stream_end() {
    movie_player_close();
    sw(OV(0x00161348), 0);
    sw(OV(0x0016134c), 0);
    g_stream.running = false;
}
}  // namespace

// The player's entry: the buffers kept (0x161348, 0x16134c), the thread's priority raised, the
// player opened (0x2ee130: the stream, its decoder and sound, the display); when it opened, its
// loop (0x2eddd0) on the buffers +0xd9048 (its block), +0, +0xd9040; then closed (0x2ee3d8).
// orig: 0x002edd28 (up to the player's loop)
void movie_stream_start(uint32_t lsn, uint32_t size, uint32_t buf, uint32_t buf2, int32_t lang) {
    sw(OV(0x00161348), buf);
    sw(OV(0x0016134c), buf2);
    TODO_PORT(0x001188a0, "kernel: ChangeThreadPriority(GetThreadId(), 1)")();
    g_stream = Stream{};
    g_stream.buf2 = buf2;
    g_stream.opened = movie_player_open(lsn, size, lang);
    g_stream.running = true;
    if (g_stream.opened) movie_player_loop_start(buf2 + 0xd9048, buf2, buf2 + 0xd9040);
}

// One frame of the player's loop; the end when it is over. True at the end (0 returned).
// orig: 0x002edd28 (the player's loop, then its end)
bool movie_stream_frame() {
    if (!g_stream.running) return true;
    if (g_stream.opened && !movie_player_loop_frame()) return false;
    stream_end();
    return true;
}

// The skip test of the player's loop: the pad read (0x267898); with 0x15eed8 -1 none; 2: any button
// pressed; then, with 0x15eea0, 0x15ee20 or 0x15eed8 set or on level 0 or below, Start pressed;
// and in every case L2 + R2 + L1 + R1 held with Start pressed. The player's stop flag
// (`player` + 0xa8 = 1, 0x2f05e0) set when it skips. The PC's Skip Cutscenes = Always (advanced.h): Start alone
// skips every movie, the boot's intro (-1) too.
// orig: 0x002eddd0 (its skip test)
bool movie_skip_test(uint32_t player) {
    constexpr uint32_t PAD = 0x0013c940;
    pad_update();
    const int32_t mode = lw(0x0015eed8);
    const bool always = advanced_skip_cutscenes();  // (the PC's Always: the condition passes, the skip is the game's)
    if (mode == -1 && !always) return false;
    bool skip = false;
    if (mode == 2 && lw(PAD + 0x1a4) != 0) {
        skip = true;
    } else {
        const bool start_skips = lw(0x0015eea0) != 0 || lw(0x0015ee20) != 0 || lw(0x0015eed8) != 0 || !(0 < lw(0x0015ed84)) || always;
        if (start_skips && (lw(PAD + 0x1a4) & 0x800) != 0) {
            skip = true;
        } else {
            const uint64_t mask = (0x8000ull << 28) | 0xf;
            skip = ((ld(PAD + 0x1a0) & mask) ^ mask) == 0;
        }
    }
    if (skip) sw(player + 0xa8, 1);  // 0x2f05e0
    return skip;
}

}  // namespace rac1
