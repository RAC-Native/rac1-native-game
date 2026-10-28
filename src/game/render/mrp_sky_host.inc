// The host sky (2026-10-28, ports/wiiu/MRP_LITE.md "The host sky"; declared in mrp_world_wiiu.inc / mrp_world_sky.inc): the level's sky
// drawn without the game's sky pass. Included by mrp_world_scene.cpp inside rac1::mrpw (its SkyGif, sky_capture_copy). Host code but
// the game's own routines it calls (the variants' state: 0x288ec0 / 0x289108; the stars' data: 0x28bd58).
//
// What the pass (0x23d968: 0x28b680, the level's variant, 0x28b710, two GS writes) does a frame, read from sky_ee.cpp:
//  - 0x28b680: the chain's first tag reserved, the VRAM pointer 0x15ee74 from 0x15ee78, the upload count 0x15f418 0, the texture entries'
//    TEX0 cache (asset + 0x10, 16 bytes each: +0 TEX0, +8 / +0xa the texels' / palette's offsets / 16, +0xc / +0xe log2 width / height) 0;
//  - the variant (Veldin 0x289330, Novalis 0x289420, Kerwan 0x2899a8, ...): asset + 4 cleared, each shell drawn (0x28b848 i: i below the
//    shell count asset + 6) with the matrix at 0x1bd7e0 the variant sets before it - identity, or 0x1ffa90's turn about z by an angle
//    from the frame counter 0x15f5cc -, GS writes between them (Veldin: ALPHA_1 0x8000000044 before shell 2), the stars (0x288ec0 their
//    start when asset + 8 is 0 - 256 of them, libc_rand -, 0x289108 their movement a frame - libc_rand -, 0x28bd58 their data);
//  - a shell (header asset + 0x20 + 4 i: +0 its cluster count, +4 not 0 for RGBA without texture, then per cluster 32 bytes: a sphere
//    the view culls it by - 0x28c680 -, its data's address, vertex / triangle counts, the vertices' (8 bytes: x, y, z, A), the S / T or
//    RGBA (4 bytes a vertex) and the triangles' (3 vertex bytes, a texture entry byte) offsets, its size): a REF of a 7-quadword GS
//    set-up packet (0x13d160 textured, 0x13d0f0 RGBA), the scratchpad's double buffer flipped (0x1604d0, once more a cluster); a visible
//    cluster DMAed to the scratchpad, its vertices turned by 0x166d80 x the shell's matrix and projected (0x28c14c: the screen scale
//    0x16cbd0 / 4, the offset 0x16cbe0, FTOI4), a DIRECT of its triangles but those outside one plane of the view (0x28c3c0: ST,
//    RGBAQ 0x808080 + A << 24 with Q 1, XYZ2 Z 0, the entry's TEX0 at each change - 0x15ee74's VRAM given at first use, its upload
//    noted at 0x16cd80 -; 0x28c298: RGBAQ the asset's word, XYZ2); an RGBA shell then writes TEST_1 0x3180b and ZBUF_1 with ZMSK;
//  - 0x28b710: the texture uploads and 0x1c2f00's REF chained before the draws, 0x15ee74 put back; then TEST_1 0x5360b, ZBUF_1.
// So the picture is the shells' triangles in the variant's order and matrices, at infinity, as the GS draws them, and the stars.

// (Inside rac1::mrpw: the including file includes game/math/vecmath.h, game/ovl/ovl_common.h, game/ovl/ovl_reloc.h, game/ovl/sky_ee.h,
// game/ovl/variants_sky.h, runtime/kernel/ee_memory.h, runtime/vu/vu0.h before its namespace.)

namespace {

// the switch: RAC1_WIIU_SKY_HOST (the Wii U: on unless 0; elsewhere off unless 1)
bool host_on_default() {
    const char* e = std::getenv("RAC1_WIIU_SKY_HOST");
#if defined(__WIIU__)
    return e == nullptr || e[0] == 0 || std::atoi(e) != 0;
#else
    return e != nullptr && std::atoi(e) != 0;
#endif
}
bool g_host_on = host_on_default();

// a frame's ops, as the variant draws: a shell (its matrix as 0x1bd7e0 holds it then; an RGBA shell's ZBUF_1 after it in `value`), the
// stars, a GS register write
struct HostOp {
    uint8_t kind = 0, shell = 0, reg = 0;  // kind: 0 shell, 1 stars, 2 write
    uint64_t value = 0;
    std::array<float, 16> m{};
};
struct HostOps {
    bool fresh = false;
    int level = -1;
    uint32_t asset = 0;
    std::vector<HostOp> ops;
};
HostOps g_ops;
uint64_t g_ops_frames = 0, g_streams = 0;
SkyHostModel g_model;
bool g_model_ok = false;
uint64_t g_model_serial = 0;
uint32_t g_model_bad = 0;  // (an asset the model could not be made from: the pass runs there)

constexpr uint32_t kMatrix = 0x001bd7e0u;  // the shells' matrix (OV)
constexpr uint32_t kCounter = 0x0015f5ccu;  // the frame counter (OV)

const float kPi = u2f(0x40490fdbu);

// orig: (host) 0x28b848's draw of shell `i` noted with the matrix now at 0x1bd7e0 (nothing for i past the count or a shell without
// clusters); the flips of the scratchpad's buffer it makes counted
void op_shell(HostOps& o, uint32_t asset, int i, uint32_t& flips) {
    if (!(i < lh(asset + 6))) return;
    const uint32_t hdr = lwu(asset + 0x20 + 4 * static_cast<uint32_t>(i));
    const int32_t n = lw(hdr);
    if (n == 0) return;
    flips += 1 + static_cast<uint32_t>(n > 0 ? n : 0);
    HostOp op;
    op.kind = 0;
    op.shell = static_cast<uint8_t>(i);
    const Mat4& m = M(OV(kMatrix));
    for (int c = 0; c < 4; ++c) {
        op.m[static_cast<size_t>(4 * c + 0)] = m.r[c].x;
        op.m[static_cast<size_t>(4 * c + 1)] = m.r[c].y;
        op.m[static_cast<size_t>(4 * c + 2)] = m.r[c].z;
        op.m[static_cast<size_t>(4 * c + 3)] = m.r[c].w;
    }
    // (0x28bae0's ZBUF_1 after an RGBA shell: 0x15ee88 >> 13, PSM 1, ZMSK)
    op.value = 0x01000000ull | (1ull << 32) | static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(0x0015ee88u)) >> 13));
    o.ops.push_back(op);
}
void op_write(HostOps& o, uint8_t reg, uint64_t v) {
    HostOp op;
    op.kind = 2;
    op.reg = reg;
    op.value = v;
    o.ops.push_back(op);
}
// orig: (host) 0x1ffa90 into 0x1bd7e0: the turn about z by `z` (x, y: the variant's stack words)
void turn_z(float x, float y, float z) {
    Vec4 a{};
    a.x = x;
    a.y = y;
    a.z = z;
    euler_to_mat4_fpu(M(OV(kMatrix)), a);
}
// orig: (host) the variants' angle: the counter's bits as an integer, x k - pi
float angle(uint32_t bits, uint32_t k) { return fpu_sub(fpu_mul(static_cast<float>(static_cast<int32_t>(bits)), u2f(k)), kPi); }

// The level variants written for the host: each as the variant runs - its ops noted, its game state (`st` true: in place of the pass;
// false - the PC's check, the pass having run - the ops alone, nothing written). 0x1bd7e0 is the game's (the pass's matrices are written
// there either way: with `st` false the values the pass left are written again).

// orig: 0x00289330 (Veldin: level 0)
void variant_veldin(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    op_shell(o, asset, 0, flips);
    op_shell(o, asset, 1, flips);
    if (st) {
        if (lh(asset + 8) == 0) ovl_288ec0_tr(0xf4, 0xc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        ovl_289108_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    HostOp stars;
    stars.kind = 1;
    o.ops.push_back(stars);
    op_write(o, 0x42, (0x8000ull << 24) | 0x44);
    op_shell(o, asset, 2, flips);
    turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x7fff, 0x39490fdbu));
    op_shell(o, asset, 3, flips);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 4, flips);
}

// orig: 0x00289420 (Novalis: level 1)
void variant_novalis(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    for (int i = 0; i < lh(asset + 6); ++i) {
        if (i == 3) turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x1ffff, 0x38490fdbu));
        else if (i == 2) turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x3ffff, 0x37c90fdbu));
        else if (i == 4) turn_z(0.0f, 0.0f, angle(lhu(OV(kCounter)), 0x38c90fdbu));
        else mat4_identity(M(OV(kMatrix)));
        op_shell(o, asset, i, flips);
    }
}

// orig: 0x002899a8 (Kerwan: level 3)
void variant_kerwan(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    turn_z(0.0f, 0.0f, angle(lhu(OV(kCounter)), 0x38c90fdbu));
    op_shell(o, asset, 1, flips);
    turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x1ffff, 0x38490fdbu));
    op_shell(o, asset, 2, flips);
}

// orig: 0x00289a90 (level 4)
void variant_289a90(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    for (int i = 0; i < lh(asset + 6); ++i) {
        if (i == 1) turn_z(0.0f, 0.0f, angle(lhu(OV(kCounter)), 0x38c90fdbu));
        else if (i == 2) turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x1ffff, 0x38490fdbu));
        else mat4_identity(M(OV(kMatrix)));
        op_shell(o, asset, i, flips);
    }
}

// the counter modulo `m` (as the variants' div's remainder), x k - pi
float angle_mod(int32_t m, uint32_t k) { return angle(static_cast<uint32_t>(ee_rem(lw(OV(kCounter)), m)), k); }

// orig: 0x0028a2f0 (level 8)
void variant_28a2f0(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    turn_z(0.0f, 0.0f, angle_mod(40000, 0x3924b5beu));
    op_shell(o, asset, 1, flips);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 2, flips);
}

// orig: 0x0028a998 (level 14: asset + 4 not cleared)
void variant_28a998(HostOps& o, uint32_t asset, uint32_t& flips) {
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    turn_z(0.0f, 0.0f, angle_mod(50000, 0x3903c498u));
    op_shell(o, asset, 1, flips);
    turn_z(0.0f, 0.0f, angle_mod(100000, 0x3883c498u));
    op_shell(o, asset, 2, flips);
}

// orig: 0x0028b080 (level 18)
void variant_28b080(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    for (int i = 0; i < 4; ++i) op_shell(o, asset, i, flips);
    turn_z(0.0f, 0.0f, angle_mod(40000, 0x3924b5beu));
    op_shell(o, asset, 4, flips);
    turn_z(0.0f, 0.0f, angle_mod(60000, 0x38db9cfeu));
    op_shell(o, asset, 5, flips);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 6, flips);
}

// orig: 0x0028a3a8 (level 9: its turn about y by 0.3 too)
void variant_28a3a8(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    op_shell(o, asset, 1, flips);
    op_shell(o, asset, 2, flips);
    turn_z(0.0f, u2f(0x3e99999au), angle_mod(25000, 0x3983c498u));
    op_shell(o, asset, 3, flips);
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 4, flips);
}

// orig: 0x0028a480 (level 10: asset + 4 not cleared)
void variant_28a480(HostOps& o, uint32_t asset, uint32_t& flips) {
    for (int i = 0; i < lh(asset + 6); ++i) {
        const uint32_t fc = lwu(OV(kCounter));
        if (i == 0) turn_z(0.0f, 0.0f, angle((fc << 1) & 0x1ffff, 0x38490fdbu));
        else if (i == 1) turn_z(0.0f, 0.0f, angle(((fc << 2) + fc) & 0x1ffff, 0x38490fdbu));
        else if (i == 2) turn_z(0.0f, 0.0f, angle((fc * 10u) & 0x3ffff, 0x37c90fdbu));
        else if (i == 3) turn_z(0.0f, 0.0f, angle((fc * 10u) & 0x1ffff, 0x38490fdbu));
        else mat4_identity(M(OV(kMatrix)));
        op_shell(o, asset, i, flips);
    }
}

// orig: 0x0028a620 / 0x0028a7a0 / 0x0028ab08 (the same code three times)
void variant_28a620(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    if (st) sh(asset + 4, 0);
    for (int i = 0; i < lh(asset + 6); ++i) {
        if (i == 2) turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x1ffff, 0x38490fdbu));
        else if (i == 1) turn_z(0.0f, 0.0f, angle(lwu(OV(kCounter)) & 0x3ffff, 0x37c90fdbu));
        else if (i == 3) turn_z(0.0f, 0.0f, angle(lhu(OV(kCounter)), 0x38c90fdbu));
        else mat4_identity(M(OV(kMatrix)));
        op_shell(o, asset, i, flips);
    }
}

// the stars as the variants with stars run them: their start when asset + 8 is 0 (0x288ec0 `a`, `b`: their count a + b), their movement
// (0x289108; both with libc_rand: the game's state), their data (0x28bd58: the op, made by sky_host_stream), ALPHA_1 0x8000000044
void stars(HostOps& o, uint32_t asset, bool st, uint32_t a, uint32_t b) {
    if (st) {
        if (lh(asset + 8) == 0) ovl_288ec0_tr(a, b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        ovl_289108_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    HostOp op;
    op.kind = 1;
    o.ops.push_back(op);
    op_write(o, 0x42, (0x8000ull << 24) | 0x44);
}

// orig: 0x00289bd8 (level 5)
void variant_289bd8(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    stars(o, asset, st, 0x78, 8);
    turn_z(0.0f, 0.0f, angle_mod(50000, 0x3903c498u));
    op_shell(o, asset, 1, flips);
}

// orig: 0x0028a208 (level 7)
void variant_28a208(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    stars(o, asset, st, 0x78, 8);
    op_shell(o, asset, 1, flips);
    turn_z(0.0f, 0.0f, angle_mod(50000, 0x3903c498u));
    op_shell(o, asset, 2, flips);
}

// orig: 0x0028a920 (level 13)
void variant_28a920(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    op_shell(o, asset, 1, flips);
    stars(o, asset, st, 0x78, 8);
    op_shell(o, asset, 2, flips);
}

// orig: 0x0028aa98 (level 15)
void variant_28aa98(HostOps& o, uint32_t asset, bool st, uint32_t& flips) {
    mat4_identity(M(OV(kMatrix)));
    op_shell(o, asset, 0, flips);
    stars(o, asset, st, 0x78, 8);
    op_shell(o, asset, 1, flips);
}

// orig: (host) the variant 0x23d968 runs for `level` (its table 0x1e8580's case, by address), or a level's own build's; 0x2892d0 (the
// default: every shell, identity) for a level past the table or another level's build; 0: unknown
uint32_t variant_of(int level) {
    static const uint32_t kByCase[20] = {0x289330, 0x289420, 0x2895a0, 0x2899a8, 0x289a90, 0x289bd8, 0x289cb8, 0x28a208, 0x28a2f0, 0x28a3a8,
                                         0x28a480, 0x28a620, 0x28a7a0, 0x28a920, 0x28a998, 0x28aa98, 0x28ab08, 0x28ac88, 0x28b080, 0x28b1a0};
    if (const SkyLevel* s = sky_level(ovl_reloc_level())) {
        if (s->level != level) return 0x2892d0;
        static const std::pair<SkyTrFn, uint32_t> kFns[] = {{ovl_289330_tr, 0x289330}, {ovl_289420_tr, 0x289420}, {ovl_2899a8_tr, 0x2899a8}, {ovl_289a90_tr, 0x289a90},
                                                            {ovl_28a2f0_tr, 0x28a2f0}, {ovl_28a998_tr, 0x28a998}, {ovl_28b080_tr, 0x28b080}, {ovl_289bd8_tr, 0x289bd8},
                                                            {ovl_28a208_tr, 0x28a208}, {ovl_28a920_tr, 0x28a920}, {ovl_28aa98_tr, 0x28aa98}, {ovl_28a3a8_tr, 0x28a3a8},
                                                            {ovl_28a480_tr, 0x28a480}, {ovl_28a620_tr, 0x28a620}, {ovl_28a7a0_tr, 0x28a7a0}, {ovl_28ab08_tr, 0x28ab08}};
        for (const auto& [f, a] : kFns)
            if (f == s->variant) return a;
        return 0;
    }
    if (level < 0 || level >= 20) return 0x2892d0;
    const uint32_t c = ovl_code_to_veldin(lwu(OV(0x001e8580u) + 4 * static_cast<uint32_t>(level)));
    if (c < 0x23d9a4 || c > 0x23dad4 || (c - 0x23d9a4) % 16 != 0) return 0;
    return kByCase[(c - 0x23d9a4) / 16];
}
// the variants written for the host
bool variant_written(uint32_t v) {
    return v == 0x289330 || v == 0x289420 || v == 0x2899a8 || v == 0x289a90 || v == 0x28a2f0 || v == 0x28a998 || v == 0x28b080 || v == 0x289bd8 ||
           v == 0x28a208 || v == 0x28a920 || v == 0x28aa98 || v == 0x28a3a8 || v == 0x28a480 || v == 0x28a620 || v == 0x28a7a0 || v == 0x28ab08;
}

// orig: (host) the pass's ops of this frame (and its game state, `st`): 0x28b680's, the variant's, 0x28b710's, the dispatcher's writes
bool ops_run(HostOps& o, bool st) {
    const int level = lw(0x0015ed84u);
    const uint32_t variant = variant_of(level);
    if (!variant_written(variant)) return false;
    const uint32_t asset = lwu(OV(0x0016051cu));
    if (asset == 0 || asset == g_model_bad || ee_ram_ptr_read(asset) == nullptr) return false;
    o.ops.clear();
    o.level = level;
    o.asset = asset;
    if (st) {  // 0x28b680 (but its chain tag)
        sw(0x0015ee74u, lwu(0x0015ee78u));
        sw(OV(0x0015f418u), 0);
        const uint32_t e = lwu(asset + 0x10);
        for (int32_t k = 0; k < lh(asset + 0xc); ++k) sd(e + 16 * static_cast<uint32_t>(k), 0);
    }
    uint32_t flips = 0;
    switch (variant) {
    case 0x289330: variant_veldin(o, asset, st, flips); break;
    case 0x289420: variant_novalis(o, asset, st, flips); break;
    case 0x2899a8: variant_kerwan(o, asset, st, flips); break;
    case 0x289a90: variant_289a90(o, asset, st, flips); break;
    case 0x28a2f0: variant_28a2f0(o, asset, st, flips); break;
    case 0x28a998: variant_28a998(o, asset, flips); break;
    case 0x28b080: variant_28b080(o, asset, st, flips); break;
    case 0x289bd8: variant_289bd8(o, asset, st, flips); break;
    case 0x28a208: variant_28a208(o, asset, st, flips); break;
    case 0x28a920: variant_28a920(o, asset, st, flips); break;
    case 0x28aa98: variant_28aa98(o, asset, st, flips); break;
    case 0x28a3a8: variant_28a3a8(o, asset, st, flips); break;
    case 0x28a480: variant_28a480(o, asset, flips); break;
    case 0x28a620: case 0x28a7a0: case 0x28ab08: variant_28a620(o, asset, st, flips); break;
    default: break;
    }
    if (st) {
        if ((flips & 1) != 0) sw(OV(0x001604d0u), 1u - lwu(OV(0x001604d0u)));  // (the scratchpad's buffers: the clusters' flips)
        sw(0x0015ee74u, lwu(0x0015ee78u));  // 0x28b710 (but its chain)
    }
    op_write(o, 0x47, 0x5360b);
    op_write(o, 0x4e, 0x01000000ull | static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(0x0015ee88u)) >> 13)));
    o.fresh = true;
    return true;
}

// orig: (host) the level's shells decoded from EE memory, as 0x28b8a0 / 0x28bae0 read them (their clusters' data in their order, every
// triangle); false: not a sky asset this reads (counts or addresses out of range)
bool model_make(SkyHostModel& m, int level, uint32_t asset) {
    m = SkyHostModel{};
    m.level = level;
    m.asset = asset;
    const int32_t nshell = lh(asset + 6), ntex = lh(asset + 0xc);
    const uint32_t entries = lwu(asset + 0x10);
    if (nshell <= 0 || nshell > 32 || ntex < 0 || ntex > 1024) return false;
    auto readable = [](uint32_t a, uint32_t n) { return n == 0 || (ee_ram_ptr_read(a) != nullptr && ee_ram_ptr_read(a + n - 1) != nullptr); };
    if (!readable(entries, 16u * static_cast<uint32_t>(ntex)) || !readable(asset + 0x20, 4u * static_cast<uint32_t>(nshell))) return false;
    // a TEX0 per entry, as 0x28c3c0 makes it but its TBP / CBP (unique to the entry) and CLD (0: the game's are 4)
    for (int32_t k = 0; k < ntex; ++k) {
        const uint32_t e = entries + 16 * static_cast<uint32_t>(k);
        const int32_t tw = lh(e + 0xc), th = lh(e + 0xe);
        const uint64_t tbw = 1ull << std::max(tw - 6, 0);
        m.tex0.push_back(((0x2000ull + static_cast<uint64_t>(k)) & 0x3fff) + ((tbw & 0x3f) << 14) + (0x13ull << 20) + ((static_cast<uint64_t>(tw) & 0xf) << 26) +
                         ((static_cast<uint64_t>(th) & 0xf) << 30) + (1ull << 34) + (((0x3000ull + static_cast<uint64_t>(k)) & 0x3fff) << 37));
    }
    for (int32_t i = 0; i < nshell; ++i) {
        SkyHostShell s;
        const uint32_t hdr = lwu(asset + 0x20 + 4 * static_cast<uint32_t>(i));
        if (!readable(hdr, 0x10)) return false;
        const int32_t n = lw(hdr);
        s.colours = lw(hdr + 4) != 0;
        s.clusters = n > 0 ? static_cast<uint32_t>(n) : 0;
        s.first = static_cast<uint32_t>(m.corners.size());
        if (s.clusters > 4096 || !readable(hdr + 0x10, 0x20 * s.clusters)) return false;
        for (uint32_t c = 0; c < s.clusters; ++c) {
            const uint32_t rec = hdr + 0x20 * c;  // (its sphere at +0x10, its record at +0x20)
            const uint32_t data = lwu(rec + 0x20);
            const int32_t nv = lh(rec + 0x24), nt = lh(rec + 0x26);
            const uint32_t vo = data + static_cast<uint32_t>(lh(rec + 0x28)), ao = data + static_cast<uint32_t>(lh(rec + 0x2a)),
                           io = data + static_cast<uint32_t>(lh(rec + 0x2c));
            if (nv < 0 || nv > 256 || nt < 0 || nt > 4096 || !readable(vo, 8u * static_cast<uint32_t>(nv)) || !readable(ao, 4u * static_cast<uint32_t>(nv)) ||
                !readable(io, 4u * static_cast<uint32_t>(nt)))
                return false;
            for (int32_t t = 0; t < nt; ++t) {
                const uint32_t ti = io + 4 * static_cast<uint32_t>(t);
                const int32_t tex = s.colours ? -1 : static_cast<int32_t>(lbu(ti + 3));
                if (tex >= ntex) return false;
                if (s.runs.empty() || s.runs.back().tex != tex) s.runs.push_back(SkyHostRun{static_cast<uint32_t>(m.corners.size()), 0, tex});
                for (int k = 0; k < 3; ++k) {
                    const uint32_t v = lbu(ti + static_cast<uint32_t>(k));
                    if (static_cast<int32_t>(v) >= nv) return false;
                    const uint32_t va = vo + 8 * v, aa = ao + 4 * v;
                    SkyHostCorner h;
                    h.p[0] = static_cast<float>(lh(va));
                    h.p[1] = static_cast<float>(lh(va + 2));
                    h.p[2] = static_cast<float>(lh(va + 4));
                    if (s.colours) {  // (0x28c298: RGBAQ the word sign-extended - Q its sign)
                        h.rgba = lwu(aa);
                        h.q = (h.rgba & 0x80000000u) != 0 ? 0xffffffffu : 0u;
                    } else {  // (0x28c3c0: ST the halfwords' ITOF12, RGBAQ 0x808080 + A << 24, Q 1)
                        h.st[0] = static_cast<float>(lhu(aa)) * (1.0f / 4096.0f);
                        h.st[1] = static_cast<float>(lhu(aa + 2)) * (1.0f / 4096.0f);
                        h.rgba = 0x808080u | (static_cast<uint32_t>(lbu(va + 6)) << 24);
                        h.q = 0x3f800000u;
                    }
                    m.corners.push_back(h);
                }
                s.runs.back().count += 3;
            }
        }
        s.count = static_cast<uint32_t>(m.corners.size()) - s.first;
        m.shells.push_back(std::move(s));
    }
    m.serial = ++g_model_serial;
    return true;
}

// ---- the stream ----
void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) { std::memcpy(b.data() + at, &v, 4); }
// a DIRECT of `qwc` quadwords of EE memory at `addr` (a REF's data), as the capture copies it (host-order words)
void stream_direct(SkyStream& s, uint32_t addr, uint32_t qwc) {
    const size_t at = s.bytes.size();
    s.bytes.resize(at + 16 + 16 * static_cast<size_t>(qwc), 0);
    put32(s.bytes, at + 12, 0x50000000u | qwc);
    for (uint32_t k = 0; k < 4 * qwc; ++k) put32(s.bytes, at + 16 + 4 * k, lwu(addr + 4 * k));
    s.tags.push_back({static_cast<uint32_t>(at), static_cast<uint32_t>(s.bytes.size() - at)});
}
// a GS register write as 0x2a0ba8 makes it (a DIRECT of an A+D GIF tag and its data)
void stream_write(SkyStream& s, uint32_t reg, uint64_t v) {
    const size_t at = s.bytes.size();
    s.bytes.resize(at + 48, 0);
    put32(s.bytes, at + 12, 0x50000002u);
    put32(s.bytes, at + 16, 0x8001u);
    put32(s.bytes, at + 20, 0x10000000u);
    put32(s.bytes, at + 24, 0xeu);
    put32(s.bytes, at + 32, static_cast<uint32_t>(v));
    put32(s.bytes, at + 36, static_cast<uint32_t>(v >> 32));
    put32(s.bytes, at + 40, reg);
    s.tags.push_back({static_cast<uint32_t>(at), static_cast<uint32_t>(s.bytes.size() - at)});
}

// the stars' data of the frame (0x28bd58 run by stars_run, its EE writes put back): its range's tags, its entries' TEX0s, the A it
// projected with
SkyStream g_stars;
std::array<float, 16> g_stars_a{};
bool g_stars_ok = false;
uint64_t g_stars_runs = 0;
double g_stars_ms = 0.0;
// orig: (host) 0x28bd58 run as the variant runs it, then everything it wrote put back (the chain's bytes, the stars' visibility bytes,
// the entries' TEX0s, the uploads' list, the VU0's registers): its chain range copied first (main thread, the game's update done)
void stars_run() {
    vu0::Vu0& v0 = vu0::global();
    const VUState vs = v0.st;
    const auto vrun = v0.run;
    ee_journal_begin();
    const uint32_t asset = lwu(OV(0x0016051cu));
    const uint32_t e = lwu(asset + 0x10);
    for (int32_t k = 0; k < lh(asset + 0xc); ++k) sd(e + 16 * static_cast<uint32_t>(k), 0);  // (0x28b680's: the stars' TEX0s given now)
    const uint32_t first = lwu(OV(0x001611c0u));
    ovl_28bd58_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    const uint32_t end = lwu(OV(0x001611c0u));
    g_stars_ok = end > first ? sky_capture_copy(first, end, scene_addrs(), g_stars) : false;
    if (end <= first) {
        g_stars.tags.clear();
        g_stars.bytes.clear();
        g_stars.sky_tex.clear();
        g_stars_ok = true;  // (no star drawn)
    }
    for (uint32_t k = 0; k < 16; ++k) g_stars_a[k] = lwc1(OV(0x00166d00u) + 4 * k);  // (the A it projected with: widened, in a widened run)
    ee_journal_restore();
    v0.st = vs;
    v0.run = vrun;
}

}  // namespace

// orig: (host) see mrp_world_sky.inc
bool sky_host_supported(int level) { return variant_written(variant_of(level)); }
void sky_host_set(bool on) { g_host_on = on; }
bool sky_host_on() { return g_host_on; }

// orig: (host) see mrp_world_sky.inc
bool sky_host_frame() {
    if (!g_host_on) return false;
#if defined(__WIIU__)
    if (!mrp_world_wiiu_sky_host_wanted()) return false;  // (the GX2 world not up: the pass runs, as on the PC)
#endif
    if (!ops_run(g_ops, true)) return false;
    ++g_ops_frames;
    return true;
}

// orig: (host) see mrp_world_wiiu.inc
bool sky_host_pending() { return g_ops.fresh; }
void sky_host_forget() { g_ops.fresh = false; }

// orig: (host) see mrp_world_wiiu.inc
const SkyHostModel* sky_host_model() { return g_model_ok ? &g_model : nullptr; }

// orig: (host) see mrp_world_wiiu.inc
bool sky_host_stream(SkyStream& s, float widen) {
    s.ok = false;
    s.bytes.clear();
    s.tags.clear();
    s.shells.clear();
    s.sky_tex.clear();
    s.outside = 0;
    if (!g_ops.fresh) return false;
    g_ops.fresh = false;
    const uint32_t asset = lwu(OV(0x0016051cu));
    if (asset != g_ops.asset || lw(0x0015ed84u) != g_ops.level) return false;
    if (!g_model_ok || g_model.level != g_ops.level || g_model.asset != asset) {
        g_model_ok = model_make(g_model, g_ops.level, asset);
        std::printf("[sky-host] level %d: the sky asset at %08x %s: %zu shells, %zu triangles, %zu texture entries\n", g_ops.level, asset,
                    g_model_ok ? "decoded" : "NOT decoded (the pass's capture instead)", g_model.shells.size(), g_model.corners.size() / 3, g_model.tex0.size());
        if (!g_model_ok) {
            g_model_bad = asset;
            return false;
        }
        g_model_bad = 0;
    }
    ++g_streams;
    std::array<float, 16> a{};
    for (uint32_t k = 0; k < 16; ++k) a[k] = lwc1(OV(0x00166d00u) + 4 * k);
    stream_direct(s, OV(0x001c2f00u), 3);  // (0x28b710's REF 0x1c2f00: chained after the uploads, before the draws)
    std::vector<int32_t> used;
    for (const HostOp& op : g_ops.ops) {
        if (op.kind == 0) {
            if (op.shell >= g_model.shells.size()) continue;
            const SkyHostShell& sh = g_model.shells[op.shell];
            if (sh.clusters == 0) continue;
            stream_direct(s, sh.colours ? 0x0013d0f0u : 0x0013d160u, 7);  // (its set-up packet)
            s.shells.push_back(SkyHostMark{static_cast<uint32_t>(s.tags.size()), op.shell, op.m});
            for (const SkyHostRun& r : sh.runs)
                if (r.tex >= 0 && std::find(used.begin(), used.end(), r.tex) == used.end()) used.push_back(r.tex);
            if (sh.colours) {
                stream_write(s, 0x47, 0x3180b);
                stream_write(s, 0x4e, op.value);
            }
        } else if (op.kind == 1) {
            const auto t0 = std::chrono::steady_clock::now();
            if (widen > 1.0f) mrp_lod_sky_widen_real(stars_run, widen);
            else stars_run();
            g_stars_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            ++g_stars_runs;
            if (!g_stars_ok) continue;
            a = g_stars_a;
            for (const auto& [at, len] : g_stars.tags) {
                const size_t to = s.bytes.size();
                s.bytes.insert(s.bytes.end(), g_stars.bytes.begin() + at, g_stars.bytes.begin() + at + len);
                s.tags.push_back({static_cast<uint32_t>(to), len});
            }
            s.sky_tex.insert(s.sky_tex.end(), g_stars.sky_tex.begin(), g_stars.sky_tex.end());
            s.outside += g_stars.outside;
        } else {
            stream_write(s, op.reg, op.value);
        }
    }
    const uint32_t entries = lwu(asset + 0x10);
    for (const int32_t k : used) s.sky_tex.emplace_back(g_model.tex0[static_cast<size_t>(k)], entries + 16 * static_cast<uint32_t>(k));
    s.sky_asset = asset;
    s.sky_a = a;
    s.ok = true;
    if (g_streams % 600 == 1)
        std::printf("[sky-host] %llu streams (%llu game frames' state), level %d: %zu tags, %zu shells; the stars' data %.3f ms a run (%llu runs)\n",
                    static_cast<unsigned long long>(g_streams), static_cast<unsigned long long>(g_ops_frames), g_ops.level, s.tags.size(), s.shells.size(),
                    g_stars_runs != 0 ? g_stars_ms / static_cast<double>(g_stars_runs) : 0.0, static_cast<unsigned long long>(g_stars_runs));
    return true;
}

// orig: (host) the ops of this frame after the pass ran (the PC's check): nothing of the game's state written but what the pass left there
bool sky_host_ops_after_pass() { return ops_run(g_ops, false); }

// (testing, the check) render_frame ran the pass (noted before it): the check compares that frame's sky alone
bool g_pass_noted = false;
void sky_host_note_pass() { g_pass_noted = true; }
