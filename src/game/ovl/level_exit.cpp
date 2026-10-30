// Ported overlay code: the level exit 0x292e08 (after the overlay's main loop is left for another
// level: the level's sounds and bank released, the next level made current, the loading screen's
// scene 0x291d78 set up, the level's header read and its files and sound bank loaded 0x243db0
// while the ship flies, then back to the base ELF, whose overlay load starts the new level) and
// its callees. The function's loops (the loading screen's frames, the load's wait) run one pass a
// host frame: level_exit_start / level_exit_frame / level_exit_finish.

#include <game/ovl/memory_card.h>
#include <game/ovl/ovl_reloc.h>
#include <game/boot/level_read.h>
#include <game/input/pad_build.h>
#include <game/lib/libc.h>
#include <game/lib/libcdvd.h>
#include <game/lib/libgraph.h>
#include <game/lib/libvu0.h>
#include <game/lib/snd989.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/level_change.h>
#include <game/ovl/movies.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>

#include <functional>
#include <memory>
#include <vector>

namespace rac1 {

// ---- the next level's load ------------------------------------------------------------------

// A disc read started: `n` sectors from `lsn` to `buf`, with the read mode 0x151700 (its spindle
// byte *(0x15ed58)); the read's frame count 0x15eebc and error flag 0x15eec0 cleared. 1.
// orig: 0x00265f40
int32_t cd_read_request(uint32_t buf, uint32_t lsn, uint32_t n) {
    const uint32_t mode = ee_stack_alloc(0x10);
    sw(mode, lwu(0x00151700));
    sw(0x0015eebc, 0);
    sb(mode + 1, lbu(0x0015ed58));
    sw(0x0015eec0, 0);
    cd_read(lsn, n, buf, mode);
    ee_stack_free(0x10);
    return 1;
}

// Quitting to the title: the boot module read back from the disc (the table of contents' +0x4f8 /
// +0x4fc: its sector and sector count; the disc's frontbin) below 0x1ff8000, its size rounded up
// to 4 KiB with room for a 0x60-byte header before it; the header's address is the overlay
// header 0x15ee4c and its first word 0x60 (the offset of the module's sections from it), the
// read started behind it. The base ELF's overlay load (0x12d8f8) copies the module's sections
// once the overlay's main has returned. 1.
// orig: 0x00243d38
int32_t front_read_back() {
    constexpr uint32_t TOC = 0x00137b80;
    const uint32_t h = (0x01ff7ff0u - (((lwu(TOC + 0x4fc) << 11) + 0x1057u) & 0xfffff000u)) & 0xfffffff0u;
    sw(0x0015ee4c, h);
    sw(h, 0x60);
    const uint32_t hdr = lwu(0x0015ee4c);
    cd_read_request(hdr + lwu(hdr), lwu(TOC + 0x4f8), lwu(TOC + 0x4fc));
    return 1;
}

// One step of the next level's load (step 0x15ee48), when no read is in progress (a read that
// takes over 0x2d0 frames in spindle mode 1, or that failed, is stopped and its step done again):
// 0 the level data, 1 the armor, 2 the gameplay data read below 0x1ff8000 (the same layout as at
// the start, 0x204428); 3 every sound stopped (once 0x15ee4a is clear); 4 / 5 the previous
// level's bank unloaded; 6 the level's bank loaded (reply 0x28e0f0 into 0x15ed5c); 7 its wait.
// 1 once the bank is in.
// orig: 0x00243db0
int32_t level_load_step() {
    const uint32_t armor = static_cast<uint32_t>(lh(0x0013e056) + 1);
    if (cd_sync(1) != 0) {
        const int32_t t = lw(0x0015eebc) + 1;
        const int32_t spin = lw(0x0015ed58);
        sw(0x0015eebc, static_cast<uint32_t>(t));
        if (spin != 1 || t < 0x2d1) return 0;
        sw(0x0015eec0, static_cast<uint32_t>(spin));
        const uint32_t back = lhu(0x0015ee48) - 1u;
        sw(0x0015ed58, 0);
        if ((back & 0xffff) < 3) sh(0x0015ee48, back);
        cd_break();
        return 0;
    }
    if (cd_get_error() != 0 && lw(0x0015eec0) == 0) {
        const uint32_t back = lhu(0x0015ee48) - 1u;
        sw(0x0015eec0, 1);
        sw(0x0015ed58, 0);
        if ((back & 0xffff) < 3) sh(0x0015ee48, back);
    }
    const int32_t step = lh(0x0015ee48);
    auto next = [] { sh(0x0015ee48, lhu(0x0015ee48) + 1u); };
    constexpr uint32_t TOC = 0x00137b80;
    switch (static_cast<uint32_t>(step)) {
    case 0: {
        const uint32_t gn = lwu(lw(0x0015ed80) != 0 ? TOC + 0x297c : TOC + 0x2974);
        const uint32_t gp = 0x01ff8000u - ((gn * 0x800 + 0xfff) & 0xfffff000u);
        const uint32_t ar = gp - ((lwu(TOC + armor * 8 + 0x12cc) * 0x800 + 0xfff) & 0xfffff000u);
        const uint32_t dn = lwu(TOC + 0x296c);
        const uint32_t dt = ar - ((dn * 0x800 + 0xfff) & 0xfffff000u);
        sw(0x0015ee54, ar);
        sw(0x0015ee50, gp);
        sw(0x0015ee4c, dt);
        cd_read_request(dt, lwu(TOC + 0x2968), dn);
        next();
        return 0;
    }
    case 1:
        cd_read_request(lwu(0x0015ee54), lwu(TOC + armor * 8 + 0x12c8), lwu(TOC + armor * 8 + 0x12cc));
        next();
        return 0;
    case 2:
        if (lw(0x0015ed80) != 0) cd_read_request(lwu(0x0015ee50), lwu(TOC + 0x2978), lwu(TOC + 0x297c));
        else cd_read_request(lwu(0x0015ee50), lwu(TOC + 0x2970), lwu(TOC + 0x2974));
        next();
        return 0;
    case 3:
        if (lh(0x0015ee4a) != 0) return 0;
        snd_cmd_18();
        if (lw(0x0015ed5c) != 0) next();
        else sh(0x0015ee48, 6);
        return 0;
    case 4:
        if (snd_flush() != 0) return 0;
        snd_cmd_06(lwu(0x0015ed5c));
        sw(0x0015ed5c, 0);
        next();
        return 0;
    case 5:
        if (snd_flush() != 0) return 0;
        snd_cmd_08();
        next();
        return 0;
    case 6: {
        if (snd_flush() != 0) return 0;
        const uint32_t lvl = lwu(0x0015ee4c);
        sw(0x0015ed5c, 0xffffffffu);
        snd_bank_load_from_ee_cb(lwu(lvl + 8) + lvl, OVF(0x0028e0f0), 0x0015ed5c);
        next();
        return 0;
    }
    case 7:
        if (snd_flush() != 0) return 0;
        if (lw(0x0015ed5c) == -1) return 0;
        snd_cmd_08();
        return 1;
    default:
        return 0;
    }
}

// The pad read for the frame (the controller packet into the pad state 0x13c940).
// orig: 0x00267898
void pad_update() { pad_read_packet(0x0013c940, g_host_pad_packet, 6); }

// The level's DMA interrupt handlers removed (0x1611d8 / 0x1611dc cleared). The DMA controller's
// status (channel 1's interrupt acknowledged) and the kernel calls are the hardware's.
// orig: 0x002a11b8
void dma_handlers_remove() {
    TODO_PORT(0x001187d0, "kernel: RemoveDmacHandler (channels 1 / 15), DisableDmac 0x1190f8")(lwu(OV(0x001611d8)), lwu(OV(0x001611dc)));
    sw(OV(0x001611d8), 0);
    sw(OV(0x001611dc), 0);
}

// ---- the loading screen -----------------------------------------------------------------------

// An image (header at `img`: +8 / +0xc width / height, +0x10 the GS pixel format, +0x14 the
// palette's, +0x1c the mip levels; the palette (8 / 4-bit formats) then the levels from +0x20)
// uploaded to GS memory at *(0x15ee74), moved past it (0x100 at least a level); `out` gets its
// TEX0 (+0), TEX1 (+8) and MIPTBP1 (+0x10) words. -1.
// orig: 0x0023f1d8
int32_t tex_image_upload(uint32_t img, uint32_t out) {
    int32_t L[21] = {};  // the original's stack block (0x54 bytes cleared)
    const int32_t psm = lw(img + 0x10);
    if (psm >= 0x13 && psm < 0x15) {
        L[0] = static_cast<int32_t>(img + 0x20);
        if (lw(img + 0x10) == 0x14) L[5] = lw(img + 0x14) != 0 ? 0x20 : 0x40;
        else L[5] = lw(img + 0x14) != 0 ? 0x200 : 0x400;
    }
    L[19] = highest_bit_index(lw(img + 8));
    L[20] = highest_bit_index(lw(img + 0xc));
    L[1] = static_cast<int32_t>(img + static_cast<uint32_t>(L[5] + 0x20));
    const int32_t fmt = lw(img + 0x10);
    const int32_t w = lw(img + 8), h = lw(img + 0xc);
    if (fmt == 2) L[6] = w * h * 2;
    else if (fmt == 0) L[6] = w * h * 4;
    else if (fmt == 0x13) L[6] = w * h;
    else if (fmt == 0x14) L[6] = (w * h) >> 1;
    const int32_t f2 = lw(img + 0x10);
    if (static_cast<uint32_t>(f2 - 0x13) < 2) {
        const int32_t vram = lw(0x0015ee74);
        L[10] = vram >> 8;
        const uint32_t lp = ee_stack_alloc(0x60);
        if (f2 == 0x14) {
            sw(0x0015ee74, static_cast<uint32_t>(vram + 0x100));
            gs_set_def_load_image(lp, static_cast<int16_t>(L[10]), 1, lh(img + 0x14), 0, 0, 8, 2);
        } else {
            sw(0x0015ee74, static_cast<uint32_t>(vram + L[5]));
            gs_set_def_load_image(lp, static_cast<int16_t>(L[10]), 1, lh(img + 0x14), 0, 0, 0x10, 0x10);
        }
        gs_exec_load_image(lp, static_cast<uint32_t>(L[0]));  // (after FlushCache)
        gs_sync_path(0, 0);
        ee_stack_free(0x60);
    }
    for (int32_t k = 0; k < lw(img + 0x1c) - 1; ++k) {
        L[7 + k] = L[6 + k] >> 2;
        L[2 + k] = L[1 + k] + L[6 + k];
    }
    for (int32_t k = 0; k < lw(img + 0x1c); ++k) {
        L[15 + k] = lw(img + 8) >> (k + 6);
        if (L[15 + k] <= 0) L[15 + k] = 1;
        L[11 + k] = lw(0x0015ee74) >> 8;
        const uint32_t lp = ee_stack_alloc(0x60);
        gs_set_def_load_image(lp, static_cast<int16_t>(L[11 + k]), static_cast<int16_t>(L[15 + k]), lh(img + 0x10), 0, 0,
                              static_cast<int16_t>(lw(img + 8) >> k), static_cast<int16_t>(lw(img + 0xc) >> k));
        gs_exec_load_image(lp, static_cast<uint32_t>(L[1 + k]));  // (after FlushCache)
        gs_sync_path(0, 0);
        ee_stack_free(0x60);
        int32_t size = L[6] >> (k * 2);
        if (!(0xff < size)) size = 0x100;
        sw(0x0015ee74, static_cast<uint32_t>(lw(0x0015ee74) + size));
    }
    auto x = [](int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); };
    sd(out, x(L[11]) | x(L[15]) << 14 | x(lw(img + 0x10)) << 20 | x(L[19]) << 26 | x(L[20]) << 30 | (x(L[10]) << 37 | 0x400000000ull) |
                x(lw(img + 0x14)) << 51 | 0x8000000000000000ull);
    sd(out + 8, x(lw(img + 0x1c) - 1) << 2 | 0xffa0000000e0ull);
    sd(out + 0x10, x(L[12]) | x(L[16]) << 14 | x(L[13]) << 20 | x(L[17]) << 34 | x(L[14]) << 40 | x(L[18]) << 54);
    return -1;
}

// The loading screen's scene (the ship's flight to the next planet: game mode 6, step 4): the
// hero gone, the level memory laid out again from 0x100000, the class tables cleared, the
// transition's data (table of contents +0x13b8 / +0x13bc) read 4 MB above *(0x173e14) and
// decompressed there (the screen faded out meanwhile): its GS textures, its moby textures and
// classes, paths and texture pages; the next planet's pictures (its name in the language
// *(0x15ed88)) uploaded, their registers to 0x160640 / 0x160648; the moby list, the particles;
// the flight's variant (random; 4 / 2 leaving for Veldin or a first Novalis), its scene blocks;
// its sound bank loaded, waited for.
// orig: 0x00291d78
void loading_screen_setup() {
    const uint32_t G = 0x0013e030, CORE = OV(0x00173e00), SC = OV(0x0016c860);
    sh(G + 0x24, 0xffff);
    sw(G + 0x20, 4);
    swc1(OV(0x0015f3fc), 1.0f);
    sw(0x001413d0, 0);
    sw(OV(0x001611cc), 0x00100000);
    sw(OV(0x0015f5d8), 0);
    sw(OV(0x0015f400), 0);
    memory_layout();
    const uint32_t vram = lwu(0x0015ee8c);
    sw(0x0015ee78, vram);
    sw(0x0015ee74, vram);
    mem_fill32(OV(0x00173ec0), 0x87654321u, 0x10);
    mem_fill32(OV(0x00197bc0), 0xffffffffu, 0x800);
    mem_fill32(OV(0x0019a980), 0xffffffffu, 0xe00);
    mem_fill32(OV(0x0019a280), 0, 0xe0);
    screen_setup();
    projection_setup();
    sub_002a07f8();
    cd_read_request(lwu(CORE + 0x14) + 0x00400000u, lwu(0x00137b80 + 0x13b8), lwu(0x00137b80 + 0x13bc));
    screen_fade(frame_scale_i(0xc));
    cd_sync(0);
    const int32_t size = wad_decompress(lwu(CORE + 0x14) + 0x00400000u, lwu(CORE + 0x14));  // (FlushCache around it)
    const uint32_t s4 = lwu(CORE + 0x14);
    gs_ram_upload(lwu(s4) + s4, lw(s4 + 8), lwu(s4 + 0xc) + s4);
    const uint32_t tex = lwu(0x0015ee8c);
    auto sx8 = [](uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v) >> 8)); };
    const uint32_t s5 = s4 + lwu(s4 + 4);
    sd(OV(0x001827c0), sx8(tex + lwu(s4 + 0x40)) | 0x1d308000u | (sx8(tex + lwu(s4 + 0x44)) << 37 | 0x5c0000000ull) | 0x8000000000000000ull);
    const uint32_t s6 = s5 + lwu(s4 + 0x30);
    sd(OV(0x001827d0), 0x40000400004000ull);
    sd(OV(0x001827c8), 0xffa0000000e0ull);
    const uint32_t mtex = s4 + lwu(s4 + 0x1c);
    sw(OV(0x0015ffc8), lwu(s4 + 0x18));
    if (lw(s4 + 0x18) > 0) {
        uint32_t e = mtex, t = OV(0x00199a80);
        int32_t i = 0;
        do {
            const int32_t b = highest_bit_index(static_cast<int32_t>(lhu(e + 4)));
            ++i;
            sw(t, s6 + lwu(e) + (static_cast<uint32_t>(b) << 28));
            e += 0x10;
            t += 4;
        } while (i < lw(OV(0x0015ffc8)));
    }
    mem_copy128(OV(0x001aebc0), mtex, lw(OV(0x0015ffc8)) << 4);
    const uint32_t lists = s4 + 0x50;
    sub_0023f848(s5 + lwu(s4 + 0x48));
    uint32_t e = s4 + lwu(s4 + 0x14);
    sw(OV(0x0015ffc0), 0);
    if (lw(s4 + 0x10) > 0) {
        int32_t i = 0;
        do {
            const uint32_t off = lwu(e);
            ++i;
            moby_class_register(off == 0 ? 0u : s5 + off, s4 + lwu(s4 + 0x1c), e + 0x10, lwu(e + 4));
            e += 0x20;
        } while (i < lw(s4 + 0x10));
    }
    sw(OV(0x0015f420), s5 + lwu(s4 + 0x38));
    sub_0023f768(s4 + lwu(s4 + 0x2c), lw(s4 + 0x28));
    sub_0023f630(s4 + lwu(s4 + 0x3c), static_cast<int32_t>(s5 + lwu(s4 + 0x34)), s4 + lwu(s4 + 0x24), lw(s4 + 0x20));
    const uint32_t pics = s5 + lwu(s4 + 0x4c);
    int32_t lang = lw(0x0015ed88) - 1;
    if (!(-1 < lang)) lang = 0;
    const uint32_t regs = ee_stack_alloc(0x20);
    tex_image_upload(pics + lwu(lwu(0x0015ed84) * 4 + pics + 4), regs);
    sd(OV(0x00160640), ld(regs));
    tex_image_upload(pics + lwu((static_cast<uint32_t>(lang * 0x13) + lwu(0x0015ed84)) * 4 + pics + 0x50), regs);
    const uint32_t top = lwu(0x0015ee74) + 0x2000;
    sw(0x0015ee78, top);
    const uint32_t list = lwu(CORE + 0x14) + static_cast<uint32_t>(size);
    sw(CORE + 0x18, list);
    sw(0x0015ee74, top);
    sd(OV(0x00160648), ld(regs));
    ee_stack_free(0x20);
    mem_clear128(OV(0x001802c0), 0x100);
    mem_clear128(OV(0x001804c0), 0x180);
    mem_clear128(OV(0x0017fec0), 0x400);
    mem_copy128(OV(0x0017fec0), OV(0x001bd840), 0x40);
    sw(OV(0x0015ffd8), list);
    mem_fill32(list, 0, 0x4000);
    sw(OV(0x0015ffdc), lwu(OV(0x0015ffd8)));
    sb(lwu(OV(0x0015ffd8)) + 0x20, 0xff);
    sw(OV(0x0015ffe8), list + 0x4000);
    sw(OV(0x0016016c), list + 0x6000);
    sw(OV(0x00160174), 0xffffffffu);
    sw(OV(0x0015ffe0), lwu(OV(0x0015ffd8)) + 0x3f00);
    sw(OV(0x00160170), 0);
    sw(OV(0x00160178), 0);
    mem_clear128(OV(0x001b1880), 0x200);
    sub_001f83d0();
    sw(OV(0x0015fff0), 0x1f4);
    sw(OV(0x0016017c), NOT_ADDR(0x001f4000));
    const int32_t r = libc_rand();
    sw(G + 0x5c, 0);
    sw(G + 0x58, static_cast<uint32_t>((r >> 16) & 3));
    sw(G + 0x50, 0);
    sw(G + 0x54, 0);
    lq_sq(OV(0x001605b0), OV(0x001605a0));
    const int32_t lv = lw(0x0015ed84);
    if (lv == 0 || (lv == 1 && lbu(0x0013dd43) == 0)) {
        sw(G + 0x5c, 2);
        sw(G + 0x58, 4);
    }
    mem_fill32(SC, 0, 0x1c0);
    mem_fill32(OV(0x0017c340), 0, 0x40);
    mem_fill32(OV(0x0017c380), 0, 0x40);
    const uint32_t base = lwu(OV(0x001611cc)) + 0xfffa0000u;
    sw(SC + 0x5c, lwu(CORE + 8) + base);
    sw(SC + 0x58, lwu(CORE + 4) + base);
    const uint32_t row = lists + (lwu(G + 0x58) << 2);
    sw(OV(0x001611cc), base);
    uint32_t blk = s5 + lwu(row);
    if (lw(blk + 4) != 0) {
        uint32_t o = SC + 0x60;
        for (int32_t t = 1;; ++t) {
            sw(o, s5 + lwu(row) + lwu(blk) + 0x800);
            blk += 8;
            if (!(t < 0x46)) break;
            o += 4;
            if (lw(blk + 4) == 0) break;
        }
    }
    scene_block_load(0);
    const uint32_t bank = s5 + lwu(s4 + 0x64);
    sw(0x0015ed5c, 0xffffffffu);
    snd_bank_load_from_ee_cb(bank, OVF(0x0028e0f0), 0x0015ed5c);
    do {
        TODO_PORT(0x00122298, "kernel: vsync wait")(0);  // (after FlushCache)
        snd_flush();
    } while (lw(0x0015ed5c) == -1);
    snd_cmd_08();
}

// The flight's camera from the scene's frame record (*(0x16c8b4) + frame * 0x20: the position,
// the angles x / y / z +0x10.., the field of view +0x1c to 0x16caf0): the camera position
// 0x166dc0 and its rows 0x166fd0.. (the cross product when *(0x15edb4)). Returns the record's
// flag +0xc.
// orig: 0x0028e540
uint32_t flight_camera() {
    const uint32_t SC = OV(0x0016c860), CAM = OV(0x00166dc0);
    const uint32_t fr = lwu(SC + 0x54) + (lwu(SC + 0x38) << 5);
    const uint32_t s1 = fr + 0x10;
    const uint32_t flag = lbu(fr + 0xc);
    swc1(OV(0x0016caf0), lwc1(s1 + 0xc));
    projection_setup();
    lq_sq(CAM, fr);
    Mat4 r;
    vu0_unit_matrix(r);
    vu0_rot_matrix_x(r, r, lwc1(fr + 0x10));
    vu0_rot_matrix_y(r, r, lwc1(s1 + 4));
    vu0_rot_matrix_z(r, r, lwc1(s1 + 8));
    const uint32_t V0 = CAM - 0x140;
    const uint8_t cross = lbu(0x0015edb4);
    swc1(V0 + 0x378, r.r[1].z);
    swc1(V0 + 0x350, -r.r[2].x);
    swc1(V0 + 0x360, -r.r[0].x);
    swc1(V0 + 0x370, r.r[1].x);
    swc1(V0 + 0x354, -r.r[2].y);
    swc1(V0 + 0x364, -r.r[0].y);
    swc1(V0 + 0x374, r.r[1].y);
    swc1(V0 + 0x358, -r.r[2].z);
    swc1(V0 + 0x368, -r.r[0].z);
    if (cross != 0) vec3_cross_ba(V(CAM + 0x220), V(CAM + 0x230), V(CAM + 0x210));
    return flag;
}

// The ship's flight between planets (game mode 6, step 4: the loading screen; the title screen's
// too): the fade-in 0x15f3fc, the frame counters; on its first frame (but leaving for Veldin or a
// first Novalis) the music held and the flight's sound played. At the end of a flight segment
// (frame *(0x16c894) at the scene's count): its mobys retired; segments go on while the level's
// files are not read (step 0x15ee48 below 3), at most two, each with a new variant (random) and
// its scene blocks; after them 0x15ee4a cleared (the sound bank's load may start) and the frame
// cut short (0x15f5d8: the loading screen ends). Else the scene's block streamed every 0x60
// frames, the camera, the field of view, the drift of variant 4, the scene's mobys (two passes:
// placed between their frame positions, their two joints' world positions kept in the trail
// 0x13e0f0 / 0x13e2f0) and in the last 0x38 frames the fade of their colour.
// orig: 0x0028f8c0
void flight_sequence_update() {
    const uint32_t SC = OV(0x0016c860), G = 0x0013e030;
    draw_lists_reset();
    const float fade = lwc1(OV(0x0015f3fc)) - 0.25f;
    const int32_t frame = lw(SC + 0x38) + 1, total = lw(SC + 0x34) + 1;
    sw(SC + 0x38, static_cast<uint32_t>(frame));
    sw(SC + 0x34, static_cast<uint32_t>(total));
    swc1(OV(0x0015f3fc), fade);
    if (fade < 0.0f) swc1(OV(0x0015f3fc), 0.0f);
    if (total == 1) {
        const int32_t lv = lw(0x0015ed84);
        if (lv != 0 && !(lv == 1 && lbu(0x0013dd43) == 0)) {
            snd_hold();
            snd_play_sound(lwu(0x0015ed5c), lwu(G + 0x58), 0x400, 0, 0, 0, 0, 0x0013e5c0);
            snd_release();
            snd_flush();
        }
    }
    if (!(lw(SC + 0x34) < lh(SC + 0x40))) {
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
            const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
            if (m == 0) continue;
            const uint32_t cls = lwu(m + 0x24);
            sb(cls + 0xc, lbu(cls + 0xc) - 1u);
            const uint32_t c2 = lwu(m + 0x24);
            sw(c2 + static_cast<uint32_t>(lbu(c2 + 0xc)) * 4 + 0x48, 0);
            moby_retire(m);
        }
        const int32_t lv = lw(0x0015ed84);
        if (lv != 0 && !(lv == 1 && lbu(0x0013dd43) == 0) && lh(0x0015ee48) < 3) sw(G + 0x5c, 0);
        const int32_t seg = lw(G + 0x5c);
        if (!(seg < 2)) {
            if (lh(0x0015ee4a) != 0) sh(0x0015ee4a, 0);
            sw(OV(0x0015f5d8), 1);
            return;
        }
        if (seg != 0) {
            sw(G + 0x58, 4);
        } else {
            const int32_t r = libc_rand() >> 16;
            sw(G + 0x58, (lwu(G + 0x58) + static_cast<uint32_t>(ee_rem(r, 3) + 1)) & 3);
        }
        sw(G + 0x50, 0);
        sw(G + 0x54, 0);
        sw(G + 0x5c, lwu(G + 0x5c) + 1);
        lq_sq(OV(0x001605b0), OV(0x001605a0));
        mem_fill32(SC, 0, 0x1c0);
        constexpr OvConst<0x00173e00u> CORE{};
        const uint32_t heap = lwu(OV(0x001611cc));
        const uint32_t data = lwu(CORE + 0x14);
        sw(SC + 0x58, lwu(CORE + 4) + heap);
        sw(SC + 0x5c, lwu(CORE + 8) + heap);
        const uint32_t row = data + 0x50 + (lwu(G + 0x58) << 2);
        const uint32_t s5 = data + lwu(data + 4);
        uint32_t blk = s5 + lwu(row);
        if (lw(blk + 4) != 0) {
            uint32_t o = SC + 0x60;
            for (int32_t t = 1;; ++t) {
                sw(o, s5 + lwu(row) + lwu(blk) + 0x800);
                blk += 8;
                if (!(t < 0x46)) break;
                o += 4;
                if (lw(blk + 4) == 0) break;
            }
        }
        scene_block_load(0);
    } else if (!(lw(SC + 0x38) < 0x60)) {
        const int32_t blk = lw(SC + 0x3c) + 1;
        sw(SC + 0x3c, static_cast<uint32_t>(blk));
        scene_block_load(blk);
    }
    flight_camera();
    const float fov = lwc1(OV(0x001bdc48) + (lwu(G + 0x58) << 2));
    if (lwc1(OV(0x0016caf0)) < fov) swc1(OV(0x0016caf0), fov);
    projection_setup();
    float f20 = 1.0f;
    if (lw(G + 0x58) == 4) {
        const int32_t n = lh(SC + 0x40);
        f20 = fpu_div(static_cast<float>(n - lw(SC + 0x34)), static_cast<float>(n));
        Vec4 d;
        vec3_scale(d, V(OV(0x001605c0)), f20);
        vec3_add(V(OV(0x001605b0)), V(OV(0x001605b0)), d);
    }
    const uint32_t drift = (lwu(G + 0x58) << 4) + OV(0x001bdbe0);
    const int32_t lead = frame_scale_i(0x78);
    vec3_scale(V(OV(0x00160520)), V(drift), fpu_mul(fpu_mul(static_cast<float>(lw(SC + 0x34) - lead), 20.0f), f20));
    swc1(OV(0x001604c4), lwc1(OV(0x001bdc30) + (lwu(G + 0x58) << 2)));
    for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
        const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
        for (int32_t k = 0; k < 2; ++k) {
            const uint32_t cls = lwu(m + 0x24);
            const int32_t fr = lw(SC + 0x38);
            const int32_t half = fr >> 1;
            const uint32_t seq = lwu(cls + (static_cast<uint32_t>(lbu(cls + 0xc)) - 1u) * 4 + 0x48) + 0x1c;
            const uint32_t next_rec = lwu(seq + static_cast<uint32_t>(half + 1) * 4) + 0x10;
            const uint32_t rec = lwu(seq + static_cast<uint32_t>(half) * 4) + 0x10;
            const float t = fpu_add(fpu_mul(int_to_float(fr & 1), 0.5f), fpu_mul(static_cast<float>(k), 0.25f));
            const uint32_t pos = lwu(m + 0x78);
            swc1(m + 0x54, t);
            Vec4 a, b;
            vec3_scale(a, V(pos + static_cast<uint32_t>(half) * 16), fpu_sub(1.0f, t));
            vec3_scale(b, V(pos + static_cast<uint32_t>(half + 1) * 16), lwc1(m + 0x54));
            vec3_add(V(m + 0x10), a, b);
            sb(m + 0x52, 2);
            sb(m + 0x53, 2);
            sb(m + 0x50, 0);
            sb(m + 0x51, 0);
            sub_0024ebc0(m);
            mem_copy128(lwu(m + 0x68) + 0x10, rec, 0x20);
            mem_copy128(lwu(m + 0x6c) + 0x10, next_rec, 0x20);
            sb(m + 0x72, 0xff);
            sb(m + 0x71, 0xff);
            sh(m + 0x32, 0x1ff);
            moby_update_transform(m);
            const uint32_t c2 = lwu(m + 0x24);
            sb(m + 0x52, lbu(c2 + 0xc) - 1u);
            sb(m + 0x53, lbu(c2 + 0xc) - 1u);
            Mat4 j1, j2;
            moby_chain_world_one_v(m, 1, j1);
            moby_chain_world_one_v(m, 2, j2);
            sw(G + 0x50, (lwu(G + 0x50) + 1) & 0x1f);
            if (lw(G + 0x54) < 0x20) sw(G + 0x54, lwu(G + 0x54) + 1);
            const uint32_t slot = lwu(G + 0x50) << 4;
            *reinterpret_cast<Vec4*>(ee_ram_ptr(slot + G + 0xc0)) = j1.r[3];
            *reinterpret_cast<Vec4*>(ee_ram_ptr(slot + G + 0x2c0)) = j2.r[3];
            if (lw(G + 0x58) != 4) continue;
            const int32_t lv = lw(0x0015ed84);
            if (lv == 0 || (lv == 1 && lbu(0x0013dd43) == 0)) {
                sh(m + 0x34, lhu(m + 0x34) | 1u);
                sw(G + 0x54, 0);
            }
            const int32_t n = lh(SC + 0x40);
            const int32_t at = lw(SC + 0x34);
            if (n - 0x38 < at) {
                swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), fpu_mul(static_cast<float>(n - at), u2f(0x3c924925u))));
                const uint32_t a2 = static_cast<uint32_t>(lh(SC + 0x40) - lw(SC + 0x34)) << 24;
                for (uint32_t c = OV(0x001bdb30); c < OV(0x001bdb30) + 0x18; c += 8) sw(c, (lwu(c) & 0x00ffffffu) | a2);
            } else {
                for (uint32_t c = OV(0x001bdb30); c < OV(0x001bdb30) + 0x18; c += 8) sw(c, (lwu(c) & 0x00ffffffu) | 0x38000000u);
            }
        }
    }
    frame_counters_advance();
}

// ---- the level exit ---------------------------------------------------------------------------

namespace {
// The level exit's register s0 across its loops (the next level's files and bank loaded).
int32_t g_exit_loaded = 0;
// The level exit's code between its start and its loops, run one game frame at a time: the story's
// cards and movies (each runs over many frames) and the code between them, in the original's
// order; a step returns true when it is over. Then the loops (g_exit_loops) or the end.
std::vector<std::function<bool()>> g_exit_steps;
size_t g_exit_step = 0;
bool g_exit_loops = false;  // the loading screen's loops follow the steps
bool g_exit_done = false;

// orig: (helper) repeated in 0x00292e08: a part of the exit's code, run once, as a step
void step(std::function<void()> f) {
    g_exit_steps.push_back([f] {
        f();
        return true;
    });
}
// orig: (helper) repeated in 0x00292e08: a picture card (0x2929f0) as a step
void card(int32_t lang, int32_t first, int32_t last, int32_t frames, int32_t load) {
    auto started = std::make_shared<bool>(false);
    g_exit_steps.push_back([=] {
        if (!*started) {
            *started = true;
            card_start(lang, first, last, frames, load);
        }
        return card_frame();
    });
}
// orig: (helper) repeated in 0x00292e08: a movie of the story (0x292420) as a step
void movie(int32_t k) {
    auto started = std::make_shared<bool>(false);
    g_exit_steps.push_back([=] {
        if (!*started) {
            *started = true;
            movie_start(k);
        }
        return movie_frame();
    });
}
// orig: (helper) repeated in 0x00292e08
void fade(int32_t frames) { screen_fade(frames); }

// orig: (helper) the steps run in order until one needs more frames (a step may add steps)
bool steps_run() {
    while (g_exit_step < g_exit_steps.size()) {
        const std::function<bool()> f = g_exit_steps[g_exit_step];
        if (!f()) return false;
        ++g_exit_step;
    }
    return true;
}

// The rest of the exit when no movie loads the next level: the level left marked done
// (0x13dd58[level] 2, with its conditions for Umbris / Oltanis), the next level made current, the
// loading screen set up and the level's header read; then the loops.
// orig: (helper) a part of 0x00292e08
void exit_to_loading_screen() {
    const int32_t cur = lw(0x0015ed84);
    if (static_cast<uint32_t>(cur) < 0x13) {
        bool done = true;
        if (cur == 7) done = lbu(0x0013dd48) != 0;
        if (cur == 0xe) done = lbu(0x0013dd4f) != 0;
        if (done) sb(0x0013dd58 + static_cast<uint32_t>(cur), 2);
    }
    sh(0x0015ee4a, 1);
    g_exit_loaded = 0;
    sw(0x0015ed84, lwu(OV(0x0015f5c0)));
    sh(0x0015ee48, 0);
    loading_screen_setup();
    level_header_read(lw(0x0015ed84));
    g_exit_loops = true;
}
}  // namespace

// The level exit up to its loops: the next level's armor (0x13e056: 1 / 2 after the story's
// points 0x13dd48 / 0x13dd4e or from level 8 / 14 on), the level's sounds, music and bank
// released, the fog of the hero data cleared, the background black. Leaving the game (next level
// *(0x15f5c0) < 0, the pause menu's Quit): the card's save waited for, the boot module read back
// from the disc (0x243d38), done (LevelExit::Quit: the host starts the boot module read back, the
// title). The story's cards and movies between some levels (Veldin's start, Veldin to Novalis, to
// Eudora, from Umbris, to Oltanis Orbit, from Oltanis, to Kalebo III): on a new game's
// Veldin and the first Veldin -> Novalis the last card loads the next level (LevelExit::Movie);
// else after them the level left is marked done, the next level made current, the loading screen
// set up and the level's header read (LevelExit::Load when there was no card or movie). The cards
// and movies and the code after them run in level_exit_frame, a game frame per call.
// orig: 0x00292e08 (up to its loops)
LevelExit level_exit_start() {
    int32_t lang = lw(0x0015ed88) - 1;
    if (!(-1 < lang)) lang = 0;
    sw(OV(0x00173e50), lwu(OV(0x00173e50)) | 0x80000000u);
    sw(OV(0x0015f5c4), 6);
    sh(0x0013e056, 0);
    if (lbu(0x0013dd48) != 0 || !(lw(OV(0x0015f5c0)) < 8)) sh(0x0013e056, 1);
    if (lbu(0x0013dd4e) != 0 || !(lw(OV(0x0015f5c0)) < 0xe)) sh(0x0013e056, 2);
    snd_cmd_ef68(2, 0, 0, 0, 0);  // (after FlushCache)
    snd_release();
    snd_flush();
    sound_stop_all();
    music_stop_all();
    sb(0x001516db, 1);
    if (lw(OV(0x0015f5f4)) != 0) {
        snd_cmd_06(lwu(lwu(OV(0x0015f5f4)) + 0x1c));
        snd_cmd_08();
        debug_printf_stub();  // "unloaded sound bank %#01x" (0x1e9428), *(*(0x15f5f4) + 0x1c)
    }
    sw(0x0015ed5c, 0);
    snd_set_stream_callback(0);
    snd_cd_status(0);
    constexpr OvConst<0x0016ca40u> H{};
    sw(H + 0x238, 0x10);
    swc1(H + 0x22c, 128.0f);
    swc1(H + 0x21c, 524288.0f);
    swc1(H + 0x228, 255.0f);
    sw(H + 0x230, 0);
    sw(H + 0x234, 0);
    sw(H + 0x218, 0);
    bg_color_set(0, 0, 0);
    g_exit_steps.clear();
    g_exit_step = 0;
    g_exit_loops = false;
    g_exit_done = false;
    if (lw(OV(0x0015f5c0)) < 0) {
        constexpr uint32_t SAVE = 0x0013d290;
        if (!(lw(SAVE + 0xd4) < 3 && lw(SAVE + 0xdc) < 0))  // the wait for the card (0x24b690, 0x24ac20 each pass)
            TODO_PORT(0x0024b690, "the memory card's state machine: the level exit's wait for the card skipped")();
        fade(frame_scale_i(6));
        sw(0x0015ed84, lwu(OV(0x0015f5c0)));
        front_read_back();
        cd_sync(0);
        sb(0x001516db, 0);
        dma_handlers_remove();
        return LevelExit::Quit;
    }
    const int32_t next = lw(OV(0x0015f5c0));
    const int32_t cur = lw(0x0015ed84);
    if (next == 0 && lbu(0x0013dd58) == 0) {
        fade(frame_scale_i(6));
        card(lang, 0, 1, frame_scale_i(0xf0), 0);
        movie(0);
        card(lang, 2, 2, frame_scale_i(0xb4), 0);
        movie(1);
        step([] {
            sw(0x0015ed84, lwu(OV(0x0015f5c0)));
        });
        card(lang, 3, 4, frame_scale_i(0xf0), 1);
        movie(2);
        return LevelExit::Movie;
    }
    if (cur == 0 && next == 1 && lbu(0x0013dd59) == 0) {
        fade(frame_scale_i(6));
        card(lang, 5, 6, frame_scale_i(0xf0), 0);
        movie(3);
        movie(4);
        card(lang, 7, 7, frame_scale_i(0xb4), 0);
        movie(5);
        step([] {
            sb(0x0013dd58 + lwu(0x0015ed84), 2);
            sw(0x0015ed84, lwu(OV(0x0015f5c0)));
        });
        card(lang, 8, 8, frame_scale_i(0xf0), 1);
        return LevelExit::Movie;
    }
    if (lw(OV(0x0015f5c0)) == 4 && lbu(0x0013dd5c) == 0) {
        fade(frame_scale_i(0xc));
        card(lang, 9, 10, frame_scale_i(0xf0), 0);
        movie(6);
    }
    step([lang] {
        if (lw(0x0015ed84) == 7 && lbu(0x0013dd5f) != 2 && lbu(0x0013dd48) != 0) {
            fade(frame_scale_i(0xc));
            card(lang, 0xb, 0xb, frame_scale_i(0xf0), 0);
            movie(7);
        }
        step([lang] {
            if (lw(OV(0x0015f5c0)) == 0xd && lbu(0x0013dd65) == 0) {
                fade(frame_scale_i(0xc));
                card(lang, 0xc, 0xd, frame_scale_i(0xf0), 0);
                movie(8);
            }
            step([lang] {
                if (lw(0x0015ed84) == 0xe && lbu(0x0013dd66) != 2 && lbu(0x0013dd4f) != 0) {
                    fade(frame_scale_i(0xc));
                    card(lang, 0xe, 0xe, frame_scale_i(0xf0), 0);
                    movie(9);
                }
                step([lang] {
                    if (lw(OV(0x0015f5c0)) == 0x10 && lbu(0x0013dd68) == 0) {
                        fade(frame_scale_i(0xc));
                        card(lang, 0xf, 0x10, frame_scale_i(0xf0), 0);
                        movie(0xa);
                    }
                    step(exit_to_loading_screen);
                });
            });
        });
    });
    // with no card or movie on the way the loading screen is set up at once
    return steps_run() && g_exit_loops ? LevelExit::Load : LevelExit::Movie;
}

// One pass of the level exit after its start: the cards' / movies' frames and the code between
// them; then its loops: while the loading screen runs (*(0x15f5d8) clear, until its flight ends)
// its frame (the packets, the pad, the ship's sequence and its drawing, the frame's end) with a
// load step; then the load's steps alone; then the wait for the card's save. True when done (its
// end, level_exit_finish, done).
// orig: 0x00292e08 (one pass of its loops)
bool level_exit_frame() {
    if (g_exit_done) return true;
    if (!steps_run()) return false;
    if (!g_exit_loops) {  // the movie path: the last card loaded the next level
        level_exit_finish();
        g_exit_done = true;
        return true;
    }
    if (lw(OV(0x0015f5d8)) == 0) {
        TODO_PORT(0x002a08c8, "DMA: packet kick")();
        sub_002a0858();
        sub_00201040();
        packet_call_151b60();
        packet_call_display();
        pad_update();
        ship_sequence_update();
        ship_sequence_draw();
        card_machine();
        card_ui_machine();
        dma_wait(1);
        TODO_PORT(0x00122298, "kernel: vsync wait")(0);
        sw(OV(0x0015f3f8), lwu(OV(0x0015f3f8)) + 1);
        autosave_tick();
        if (g_exit_loaded == 0) g_exit_loaded = level_load_step();
        return false;
    }
    if (g_exit_loaded == 0) {
        TODO_PORT(0x00122298, "kernel: vsync wait")(0);  // (after FlushCache)
        card_machine();
        card_ui_machine();
        autosave_tick();
        g_exit_loaded = level_load_step();
        return false;
    }
    constexpr uint32_t SAVE = 0x0013d290;
    if (lw(SAVE + 0xd4) != 2 || lw(SAVE + 0xdc) >= 0)  // the wait for the card's save to end
        TODO_PORT(0x0024b690, "the memory card's state machine: the level exit's wait for an idle card skipped")();
    level_exit_finish();
    g_exit_done = true;
    return true;
}

// Whether the next level's files are in EE memory at the level exit's end: on the loading-screen
// path always; on the movie path when its last card's load finished.
// orig: (helper) a part of 0x00292e08
bool level_exit_files_read() { return g_exit_loops || card_load_done(); }

// Whether the exit's cards / movies are running (no 3D drawn then: the frames are the card's 2D
// packets or the movie's pictures).
// orig: (helper) a part of 0x00292e08
bool level_exit_in_story() { return !g_exit_done && g_exit_step < g_exit_steps.size(); }

// Whether the loading screen's loops run (loading_screen_setup done, the exit not over).
// orig: (helper) a part of 0x00292e08
bool level_exit_loading_screen() { return g_exit_loops && !g_exit_done; }

// The level exit's end: the disc idle, the sound flag 0x1516db clear, the DMA handlers removed.
// orig: 0x00292e08 (its end)
void level_exit_finish() {
    cd_sync(0);
    sb(0x001516db, 0);
    dma_handlers_remove();
}

}  // namespace rac1
