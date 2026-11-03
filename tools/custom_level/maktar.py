#!/usr/bin/env python3
"""A custom level for RAC1 from RAC2's disc: the arena of Maktar Nebula (LEVEL2.WAD, chunk 1), in the slot of level 0.

LOCAL EXPERIMENT (experiment/custom-level-maktar). Writes, from the player's own discs, the two files the game reads
for level 0 when RAC1_CUSTOM_LEVEL=<out> is set (runtime/kernel/disc.cpp):

  <out>/data.wad      Veldin's data file with a new core: the arena's tfrags (chunk 1), its collision (types 1 and 63
                      made 31), the sky (Maktar's, or Veldin's with --sky veldin), the arena's tfrag textures (texels in
                      the core, palettes and the third mip in the VRAM image, added after Veldin's rows); Veldin's moby
                      classes, moby textures, hero sequences, gadgets, particle and effect banks kept; no occlusion, no
                      ties, no shrubs. The overlay, the sound bank and the HUD are Veldin's. Same size as Veldin's file.
  <out>/gameplay.bin  Veldin's gameplay with: Maktar's background / fog colours and fog range, death height 480, no
                      ship, Maktar's directional lights, no cameras, no sound instances, no ties, no shrubs, only the
                      hero (class 0) at the arena's centre, no volumes, paths or grind lines, no occlusion. Veldin's help
                      text, moby class list, pvars (the hero's own) and the two 0x4010 grids kept. Same size as Veldin's.
  <out>/manifest.txt  what was built (sizes, offsets): no game data.

Nothing here is game data; the outputs are, and stay local (never committed). The core is WAD-encoded by wadlz.py, whose
stream RAC1's strict decoder accepts; both outputs are decoded again with it before they are written.

Formats: src/formats/level_file.h (data file, asset index, texture records), level_gameplay.cpp (gameplay blocks),
level_collision.cpp (collision), src/game/ovl/level_setup.cpp / level_load.cpp (how the game reads them).
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wadlz  # noqa: E402

SECTOR = 2048


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def align(v, a):
    return (v + a - 1) & ~(a - 1)


# ---------------------------------------------------------------- RAC2 (Going Commando) source
class Rac2Level:
    """LEVEL<n>.WAD of RAC2's disc: the data file's index, core and VRAM image, the gameplay, a chunk's tfrags and
    collision (RAC2 port's tools/disc/rac2disc.py reads the disc; its lenient decoder is fine for RAC2's own streams)."""

    def __init__(self, iso_path, rac2_tools, level=2, chunk=1):
        sys.path.insert(0, rac2_tools)
        import rac2disc
        iso = rac2disc.Iso(iso_path)
        lba, _ = iso.files["G/LEVEL%d.WAD" % level]
        h = rac2disc.level_header(iso.read(lba, 0x800))
        dsec, dn = h["data"]
        data = iso.read(lba + dsec, dn * SECTOR)
        dh = rac2disc.data_header(data)
        o, n = dh["core_index"]
        self.index = data[o:o + n]
        o, n = dh["gs_ram"]
        self.gs_ram = data[o:o + n]
        o, n = dh["core_data"]
        self.core = rac2disc.wad_decompress(data[o:o + n])
        gsec, gn = h["gameplay"]
        self.gameplay = rac2disc.wad_decompress(iso.read(lba + gsec, gn * SECTOR))
        csec, cn = h["chunk%d" % chunk]
        raw = iso.read(lba + csec, cn * SECTOR)
        t_off, c_off = struct.unpack_from("<2I", raw)
        self.tfrag = bytearray(rac2disc.wad_decompress(raw, t_off))
        self.coll = bytearray(rac2disc.wad_decompress(raw, c_off))
        self.H = struct.unpack_from("<48i", self.index)


# ---------------------------------------------------------------- RAC1 (Veldin) source
class Rac1Level:
    FIELDS = ["overlay", "sound_bank", "asset_index", "vram_image", "hud_index", "hud0", "hud1", "hud2", "hud3", "hud4",
              "asset_data"]

    def __init__(self, data_wad, gameplay):
        self.wad = open(data_wad, "rb").read()
        self.parts = {n: struct.unpack_from("<2i", self.wad, 8 * i) for i, n in enumerate(self.FIELDS)}
        o, n = self.parts["asset_index"]
        self.index = self.wad[o:o + n]
        o, n = self.parts["vram_image"]
        self.vram = self.wad[o:o + n]
        o, _ = self.parts["asset_data"]
        self.core = wadlz.decode(self.wad, o)
        self.H = struct.unpack_from("<48i", self.index)
        self.gameplay_file = open(gameplay, "rb").read()
        self.gameplay = wadlz.decode(self.gameplay_file)

    def part(self, name):
        o, n = self.parts[name]
        return self.wad[o:o + n]


# ---------------------------------------------------------------- pieces
def tex_record(idx, H, k, table):
    """(texels, w, h, mips, palette, mip1, mip2) of record k of the table (count field H[table], offset H[table+1])."""
    return struct.unpack_from("<i6h", idx, H[table + 1] + 16 * k)


def vram_rows(idx, H):
    return [struct.unpack_from("<4I", idx, H[1] + 16 * k) for k in range(H[0])]


def row_bytes(fmt, wh):
    if fmt == 0x13:
        return max(0x100, (wh & 0xffff) * (wh >> 16))
    return 0x200 if fmt == 2 else 0x400


def tfrag_textures(tf):
    """The tfrag block's texture indices, in first-use order, and the packets' offsets."""
    table, n = struct.unpack_from("<2I", tf)
    order, packets = [], []
    for i in range(n):
        e = table + 0x40 * i
        data = table + u32(tf, e + 0x10)
        tex = struct.unpack_from("<H", tf, e + 0x1c)[0]
        for k in range(tf[e + 0x28]):
            p = data + tex + 0x50 * k
            packets.append(p)
            t = u32(tf, p)
            if t not in order:
                order.append(t)
    return order, packets


def collision_retype(c, remap):
    """Face types of the collision block changed by `remap` (in place); -> counts of the types before."""
    counts = {}
    g = u32(c, 0)
    z0, zn = struct.unpack_from("<hH", c, g)
    for zi in range(zn):
        zrel = struct.unpack_from("<H", c, g + 4 + zi * 2)[0]
        if not zrel:
            continue
        zrow = g + zrel * 4
        _, yn = struct.unpack_from("<hH", c, zrow)
        for yi in range(yn):
            yrel = u32(c, zrow + 4 + yi * 4)
            if not yrel:
                continue
            yrow = g + yrel
            _, xn = struct.unpack_from("<hH", c, yrow)
            for xi in range(xn):
                cr = u32(c, yrow + 4 + xi * 4) >> 8
                if not cr:
                    continue
                cell = g + cr
                faces, points, _ = struct.unpack_from("<HBB", c, cell)
                f = cell + 4 + points * 4
                for k in range(faces):
                    t = c[f + 4 * k + 3]
                    counts[t] = counts.get(t, 0) + 1
                    if t in remap:
                        c[f + 4 * k + 3] = remap[t]
    return counts


class Core:
    """The new asset data, part by part; `remap` takes an offset of Veldin's core to the new one."""

    def __init__(self):
        self.buf = bytearray()
        self.segs = []  # (old start, old end, new start)

    def put(self, b, a=0x100):
        at = align(len(self.buf), a)
        self.buf += bytes(at - len(self.buf))
        self.buf += b
        return at

    def keep(self, old, lo, hi):
        """Veldin's core bytes [lo, hi), placed at the same offset modulo 0x1000."""
        at = align(len(self.buf), 0x1000) + (lo & 0xfff)
        if at < len(self.buf):
            at += 0x1000
        self.buf += bytes(at - len(self.buf))
        self.buf += old[lo:hi]
        self.segs.append((lo, hi, at))
        return at

    def remap(self, off):
        for lo, hi, at in self.segs:
            if lo <= off < hi or (off == hi and hi == self.segs[-1][1]):
                return at + off - lo
        raise ValueError("core offset %#x is in no part kept" % off)


def build(a):
    log = []
    say = lambda s: (log.append(s), print(s))  # noqa: E731
    r2 = Rac2Level(a.rac2_iso, a.rac2_tools)
    v = Rac1Level(os.path.join(a.rac1_level, "data.wad"), os.path.join(a.rac1_level, "gameplay_ntsc.bin"))
    VH, MH = v.H, r2.H
    say("veldin: core %#x bytes, index %#x, vram %#x (%d rows), gameplay %#x" % (len(v.core), len(v.index), len(v.vram), VH[0],
                                                                           len(v.gameplay)))
    say("maktar: core %#x, index %#x, gs ram %#x, gameplay %#x, chunk tfrag %#x, collision %#x" % (
        len(r2.core), len(r2.index), len(r2.gs_ram), len(r2.gameplay), len(r2.tfrag), len(r2.coll)))

    # --- the arena's tfrags: texture indices 0..n-1 over the textures they use
    tf = r2.tfrag
    order, packets = tfrag_textures(tf)
    newidx = {t: k for k, t in enumerate(order)}
    for p in packets:
        struct.pack_into("<I", tf, p, newidx[u32(tf, p)])
    say("tfrag header words %s (the last one, RAC2's tfrag total, set to 0)" % list(struct.unpack_from("<2IfI", tf)))
    struct.pack_into("<I", tf, 0xc, 0)
    say("tfrags %d, textures used %d: RAC2 tfrag textures %s" % (u32(tf, 4), len(order), order))

    # --- collision
    coll = r2.coll
    counts = collision_retype(coll, {1: 31, 63: 31})
    say("collision %#x bytes, face types before %s (1, 63 -> 31), hero groups at %#x" % (len(coll), sorted(counts.items()), u32(coll, 4)))

    # --- sky
    if a.sky == "maktar":
        sky = r2.core[MH[4]:MH[5]]
    else:
        sky = v.core[VH[4]:VH[5]]
    say("sky: %s, %#x bytes" % (a.sky, len(sky)))

    # --- the arena's texture records, texels (core) and VRAM pages (palette, third mip)
    m_rows = vram_rows(r2.index, MH)
    by_addr = {}
    for fmt, wh, addr, off in m_rows:
        by_addr[addr] = (fmt, wh, off)
    v_rows = vram_rows(v.index, VH)
    vram_total = 0
    for fmt, wh, addr, off in v_rows:
        vram_total = addr + row_bytes(fmt, wh)
    if vram_total != len(v.vram):
        say("note: Veldin's VRAM rows end at %#x, its image is %#x bytes" % (vram_total, len(v.vram)))
    new_vram = bytearray(v.vram) + bytes(max(0, vram_total - len(v.vram)))
    new_rows = list(v_rows)
    page_map = {}

    def vram_page(rac2_page, what):
        if rac2_page in page_map:
            return page_map[rac2_page]
        addr = rac2_page * 0x100
        if addr not in by_addr:
            raise SystemExit("RAC2 VRAM page %#x (%s) is in no upload row" % (rac2_page, what))
        fmt, wh, off = by_addr[addr]
        n = row_bytes(fmt, wh)
        at = len(new_vram)
        new_vram.extend(r2.gs_ram[off:off + n])
        new_vram.extend(bytes(at + n - len(new_vram)))
        new_rows.append((fmt, wh, at, at))
        page_map[rac2_page] = at >> 8
        return at >> 8

    arena_tex = []  # (record words, ee bytes)
    for t in order:
        texels, w, h, mips, pal, m1, m2 = tex_record(r2.index, MH, t, 12)
        ee = w * h + ((w // 2) * (h // 2) if mips >= 2 else 0)
        px = MH[24] + texels
        rec = [w, h, mips, vram_page(pal, "palette"), 0, 0]
        if mips >= 3:
            rec[4] = vram_page(m1, "mip 2")
        if mips >= 4 and m2 > 0:
            rec[5] = vram_page(m2, "mip 3")
        arena_tex.append((rec, r2.core[px:px + ee]))
        say("  texture %d (RAC2 %d): %dx%d mips %d, RAC2 pages pal %d mip %d/%d -> pal %d mip %d/%d" % (
            len(arena_tex) - 1, t, w, h, mips, pal, m1, m2, rec[3], rec[4], rec[5]))
    say("VRAM image %#x -> %#x bytes, rows %d -> %d" % (len(v.vram), len(new_vram), len(v_rows), len(new_rows)))

    # --- the core
    core = Core()
    o_tfrag = core.put(bytes(tf))
    o_sky = core.put(sky)
    o_coll = core.put(bytes(coll))
    mtex = [list(tex_record(v.index, VH, k, 14)) for k in range(VH[14])]
    m_lo = min(r[0] for r in mtex)
    m_hi = max(r[0] + r[1] * r[2] + ((r[1] // 2) * (r[2] // 2) if r[3] >= 2 else 0) for r in mtex)
    o_texel = core.put(v.core[VH[24] + m_lo:VH[24] + m_hi])
    tex_off = []
    for rec, ee in arena_tex:
        tex_off.append(core.put(ee) - o_texel)
    core.put(bytes(0x400))  # (the exporter reads two mip levels whatever the count)
    # Veldin's parts after its texel block: particle and effect banks, the +0x74 / +0xa0 blocks, the moby classes
    # [particle_block, first tie class); the hero's sequences and the gadgets [+0x7c, end). Ties and shrubs dropped.
    tie_lo = min(i32(v.index, VH[9] + 32 * k) for k in range(VH[8]))
    shrub_hi = VH[31]
    core.keep(v.core, VH[25], tie_lo)
    core.keep(v.core, shrub_hi, len(v.core))
    say("core: tfrag %#x, sky %#x, collision %#x, texel block %#x (moby texels %#x..%#x of Veldin's), kept %s, %#x bytes" % (
        o_tfrag, o_sky, o_coll, o_texel, m_lo, m_hi, [(hex(lo), hex(hi), hex(at)) for lo, hi, at in core.segs], len(core.buf)))

    # --- the index: Veldin's, re-pointed; the new tables after it
    idx = bytearray(v.index)

    def put_index(b):
        at = align(len(idx), 0x10)
        idx.extend(bytes(at - len(idx)))
        idx.extend(b)
        return at

    def set_h(field, value):
        struct.pack_into("<i", idx, field, value)

    rows_at = put_index(b"".join(struct.pack("<4I", *r) for r in new_rows))
    set_h(0x00, len(new_rows))
    set_h(0x04, rows_at)
    set_h(0x08, o_tfrag)
    set_h(0x0c, 0)  # no occlusion
    set_h(0x10, o_sky)
    set_h(0x14, o_coll)
    for k in range(VH[6]):  # moby classes
        r = VH[7] + 32 * k
        d = i32(idx, r)
        if d > 0:
            struct.pack_into("<i", idx, r, core.remap(d))
    set_h(0x20, 0)  # no tie classes
    set_h(0x28, 0)  # no shrub classes
    tt_at = put_index(b"".join(struct.pack("<i6h", tex_off[k], *rec) for k, (rec, _) in enumerate(arena_tex)))
    set_h(0x30, len(arena_tex))
    set_h(0x34, tt_at)
    for k in range(VH[14]):  # moby textures: texels from the new texel block
        struct.pack_into("<i", idx, VH[15] + 16 * k, mtex[k][0] - m_lo)
    set_h(0x40, 0)
    set_h(0x48, 0)
    set_h(0x60, o_texel)
    for f in (0x64, 0x68, 0x74, 0x7c, 0xa0, 0xa4, 0xa8):
        val = i32(idx, f)
        if val > 0:
            set_h(f, core.remap(val))
    for k in range(256):  # hero sequences
        p = VH[30] + 4 * k
        s = i32(idx, p)
        if s > 0:
            struct.pack_into("<i", idx, p, core.remap(s))
    for k in range(VH[32]):  # gadgets
        p = VH[33] + 16 * k
        d = i32(idx, p)
        if d > 0:
            struct.pack_into("<i", idx, p, core.remap(d))

    packed = wadlz.encode(bytes(core.buf), lz=not a.literal_only)
    if wadlz.decode(packed) != bytes(core.buf):
        raise SystemExit("the core does not round-trip through RAC1's decoder")
    set_h(0x88, len(packed))
    set_h(0x8c, len(core.buf))
    say("core packed %#x -> %#x (%.1f %%), round trip through RAC1's decoder ok" % (len(core.buf), len(packed),
                                                                                   100.0 * len(packed) / len(core.buf)))

    # --- the data file: Veldin's overlay, sound bank and HUD; the new index, VRAM image and asset data (at the end)
    size = len(v.wad)
    out = bytearray(size)
    parts = {}
    at = 0x80
    for name, b in (("overlay", v.part("overlay")), ("sound_bank", v.part("sound_bank")), ("asset_index", bytes(idx)),
                    ("vram_image", bytes(new_vram)), ("hud_index", v.part("hud_index")), ("hud0", v.part("hud0")),
                    ("hud1", v.part("hud1")), ("hud2", v.part("hud2")), ("hud3", v.part("hud3")), ("hud4", v.part("hud4"))):
        at = align(at, 0x40)
        out[at:at + len(b)] = b
        parts[name] = (at, len(b))
        at += len(b)
    asset_at = (size - len(packed)) & ~0x3f
    if asset_at < align(at, 0x40):
        raise SystemExit("the data file does not fit in Veldin's %#x bytes (asset data %#x, other parts end at %#x)" % (
            size, len(packed), at))
    out[asset_at:asset_at + len(packed)] = packed
    parts["asset_data"] = (asset_at, len(packed))
    for i, n in enumerate(Rac1Level.FIELDS):
        struct.pack_into("<2i", out, 8 * i, *parts[n])
    say("data.wad %#x bytes: %s; free between the HUD and the asset data %#x" % (size, {n: (hex(o), hex(s)) for n, (o, s) in parts.items()},
                                                                                 asset_at - at))

    # --- gameplay
    g = v.gameplay
    G = list(struct.unpack_from("<36I", g))
    offs = sorted(set([x for x in G if x] + [len(g)]))

    def block(field):
        o = G[field // 4]
        return g[o:offs[offs.index(o) + 1]] if o else None

    M = struct.unpack_from("<40I", r2.gameplay)
    blocks = {}
    st = bytearray(block(0x00))
    ms = r2.gameplay[M[0]:M[0] + 0x80]
    st[0x00:0x20] = ms[0x00:0x20]  # background and fog colours, fog near / far (same layout to +0x20)
    struct.pack_into("<f", st, 0x28, a.death_height)
    struct.pack_into("<4f", st, 0x2c, 0.0, 0.0, 0.0, 0.0)  # no ship (its x <= 0)
    blocks[0x00] = bytes(st)
    nl = u32(r2.gameplay, M[1])
    blocks[0x04] = r2.gameplay[M[1]:M[1] + 0x10 + 0x40 * nl]
    empty = bytes(0x10)
    for f in (0x08, 0x0c, 0x30, 0x34, 0x38, 0x3c, 0x60, 0x64, 0x68, 0x6c, 0x80, 0x88):
        blocks[f] = empty
    blocks[0x70] = struct.pack("<4I", 0, 0x10, 0, 0)  # paths
    blocks[0x74] = struct.pack("<4I", 0, 0x10, 0, 0)  # grind lines
    # the hero only: Veldin's class 0 record, at the arena's centre
    mo = G[0x44 // 4]
    n1, n2 = struct.unpack_from("<2i", g, mo)
    p = mo + 0x10
    hero = None
    for _ in range(n1):
        sz = i32(g, p)
        if i32(g, p + 0x18) == 0:
            hero = bytearray(g[p:p + sz])
            break
        p += sz
    struct.pack_into("<3f", hero, 0x30, *a.hero)
    struct.pack_into("<3f", hero, 0x3c, 0.0, 0.0, a.hero_yaw)
    blocks[0x44] = struct.pack("<2i8x", 1, n2) + bytes(hero)
    for f in range(0, 0x90, 4):
        if f not in blocks:
            blocks[f] = block(f)
    order_f = sorted(range(0, 0x90, 4), key=lambda f: (G[f // 4] == 0, G[f // 4], f))
    ng = bytearray(0xa0)
    for f in order_f:
        b = blocks[f]
        if b is None or (f == 0x8c):
            struct.pack_into("<I", ng, f, 0)
            continue
        at = align(len(ng), 0x10)
        ng.extend(bytes(at - len(ng)))
        ng.extend(b)
        struct.pack_into("<I", ng, f, at)
    gp = wadlz.encode(bytes(ng))
    if wadlz.decode(gp) != bytes(ng):
        raise SystemExit("the gameplay does not round-trip through RAC1's decoder")
    gsize = len(v.gameplay_file)
    if len(gp) > gsize:
        raise SystemExit("the gameplay (%#x packed) does not fit in Veldin's %#x" % (len(gp), gsize))
    say("gameplay %#x -> packed %#x (Veldin's file %#x); lights %d, hero at %s yaw %.2f, death height %.0f" % (
        len(ng), len(gp), gsize, nl, a.hero, a.hero_yaw, a.death_height))

    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "data.wad"), "wb").write(bytes(out))
    open(os.path.join(a.out, "gameplay.bin"), "wb").write(gp + bytes(gsize - len(gp)))
    if a.dump_core:
        open(os.path.join(a.out, "core.bin"), "wb").write(bytes(core.buf))
        open(os.path.join(a.out, "index.bin"), "wb").write(bytes(idx))
        open(os.path.join(a.out, "gameplay_unpacked.bin"), "wb").write(bytes(ng))
    open(os.path.join(a.out, "manifest.txt"), "w").write("\n".join(log) + "\n")
    print("wrote", a.out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rac2-iso", default="/home/daino/progetti/rac2-native-game/assets/iso/Ratchet Clank - Going Commando.iso")
    ap.add_argument("--rac2-tools", default="/home/daino/progetti/rac2-native-game/tools/disc",
                    help="the RAC2 port's tools/disc (rac2disc.py)")
    ap.add_argument("--rac1-level", default="assets/disc/levels/level00", help="Veldin's extracted files")
    ap.add_argument("--out", required=True)
    ap.add_argument("--sky", choices=("maktar", "veldin"), default="maktar")
    ap.add_argument("--hero", type=float, nargs=3, default=(500.0, 600.0, 502.0))
    ap.add_argument("--hero-yaw", type=float, default=1.5708)
    ap.add_argument("--death-height", type=float, default=480.0)
    ap.add_argument("--literal-only", action="store_true", help="WAD-encode with literal runs only")
    ap.add_argument("--dump-core", action="store_true", help="also write the unpacked core, index and gameplay")
    build(ap.parse_args())


if __name__ == "__main__":
    main()
