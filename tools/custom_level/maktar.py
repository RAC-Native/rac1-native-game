#!/usr/bin/env python3
"""A custom level for RAC1 from RAC2's disc: the arena of Maktar Nebula (LEVEL2.WAD, chunk 1), in the slot of level 0.

LOCAL EXPERIMENT (feat/custom-levels-experiment). Writes, from the player's own discs, the files of a custom planet: the
game reads them for level 0 while the planet is on (the ship's list row, or RAC1_CUSTOM_LEVEL=<out>: runtime/kernel/disc.h,
src/host/custom_planet.h):

  <out>/data.wad      Veldin's data file with a new core: the arena's tfrags (chunk 1), its collision (types 1 and 63
                      made 31), the sky (Maktar's, or Veldin's with --sky veldin), the arena's tfrag textures (texels in
                      the core, palettes and the third mip in the VRAM image, added after Veldin's rows); Veldin's moby
                      classes, moby textures, hero sequences, gadgets, particle and effect banks kept; no occlusion, no
                      ties, no shrubs. The overlay, the sound bank and the HUD are Veldin's. Same size as Veldin's file.
  <out>/gameplay.bin  Veldin's gameplay with: Maktar's background / fog colours and fog range, death height 480, the
                      ship on the arena's floor with its take-off path (the only path), Maktar's seven directional light sets as RAC2 has them (the arena's tfrags name set 0, a faint
                      blue: the terrain is its painted colours), one light zone over the arena (the hero lit as RAC2 lights
                      the arena's mobys: set 6, a white key light from above, and the ambient 27 40 40), Veldin's generic
                      cameras (types 0, 4..7), no sound instances, no ties, no shrubs, the hero (class 0) at the
                      arena's centre and the wave challenge's enemies (copies of Veldin's class 749 and 1440 records,
                      hidden until their wave), no moby groups, volumes or grind lines, no occlusion. Veldin's help text, moby class list,
                      pvars (the hero's own, the cameras') and the two 0x4010 grids (empty on Veldin and on Maktar) kept.
                      Same size as Veldin's.
  <out>/ties.mrpc     the arena's ties (205 instances of 13 RAC2 classes, their 45 textures), drawn by the host
                      (src/formats/custom_ties.h; `rac1-native --setup --custom-level <out>` puts them in the MRP world).
                      With them, the arena's static mobys (RAC2's moby classes in their bind pose, rac2moby.py: the
                      monitors round the floor, the stands' crowd cards, the floor's fittings) as more such classes.
  <out>/waves.txt     the wave challenge: per wave its enemies' uids (src/game/enhance/custom_waves.cpp shows them).
  <out>/lights.txt    the modern lighting's file for the custom level (our choices): point lights at the arena's gates,
                      lamps and screens, emissive surfaces for its screens and neon.
  <out>/planet.txt    the planet's name on the ship's list, its death height and ship (our own settings file).
  <out>/manifest.txt  what was built (sizes, offsets): no game data.

Nothing here is game data; the outputs are, and stay local (never committed). The core is WAD-encoded by wadlz.py, whose
stream RAC1's strict decoder accepts; both outputs are decoded again with it before they are written.

Formats: src/formats/level_file.h (data file, asset index, texture records), level_gameplay.cpp (gameplay blocks),
level_collision.cpp (collision), src/game/ovl/level_setup.cpp / level_load.cpp (how the game reads them).
"""
import argparse
import math
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


# ---------------------------------------------------------------- the ties (host side: <out>/ties.mrpc)
# RAC2's tie classes are not RAC1's (FINDINGS "Ties"): they are not put in the data file. The arena's classes and instances
# go to <out>/ties.mrpc, our own container that `rac1-native --setup --custom-level <out>` turns into the MRP world's tie
# classes (world.mrpw) and tie textures (textures_tie.mrpt); the renderer draws them at their placements
# (src/formats/custom_ties.h: the layout). Nothing of it reaches EE memory.
#
# A class's packet as RAC2's tie program has it (the RAC2 port's src/formats/tie_class.cpp, docs/formats/level-core.md
# "Tie classes"): four vertex lists (singles, duals: 2 qwords each, s16 x y z slot | s t q slot2; morphing singles and
# duals: 3 qwords, s16 delta x y z slot | base x y z | s t q slot2), each up to the vertex whose slot is its end marker plus
# those the program's pipeline has loaded then (3 singles, 2 duals); strips at +0x2c (count, 0, GIF tag qword, flags), a
# strip's vertex i the one whose slot is tag + 1 + 3 i; the textures: the first ad-gif at the packet's start (+0x10 its
# source, 0x50 a texture), one more at each destination (+0x00..). Singles and duals become RAC1's dinky vertices, the
# morphing ones its fat ones (delta, base, s t q), in the lists' order.
MRPC_MAGIC = 0x4350524d  # "MRPC"
ARENA_BOX = ((400.0, 600.0), (500.0, 700.0), (450.0, 600.0))


def rac2_tie_packet(b, table, entry):
    e = table + entry * 16
    d = table + i32(b, e)
    vert_ofs, vert_size = b[e + 8], b[e + 9]
    nstrips = b[d + 0x23]
    marker = b[d + 0x24:d + 0x28]
    size, in_flight = (16, 16, 24, 24), (3, 2, 0, 0)
    v0 = d + vert_ofs * 16
    v1 = v0 + vert_size * 16
    v = v0
    count = [0, 0, 0, 0]
    for l in range(4):
        if l > 0 and marker[l] == 0:
            continue
        n = 0
        while True:
            if v + size[l] > v1:
                raise SystemExit("a RAC2 tie packet's vertex list does not end")
            slot = struct.unpack_from("<h", b, v + 6)[0]
            v += size[l]
            n += 1
            if slot == marker[l]:
                break
        n += in_flight[l]
        v += in_flight[l] * size[l]
        count[l] = n
    dinky, fat, slots_d, slots_f = [], [], [], []
    v = v0
    for l in range(4):
        for _ in range(count[l]):
            if l < 2:
                x, y, z, s0 = struct.unpack_from("<4h", b, v)
                s, t, q, s1 = struct.unpack_from("<4h", b, v + 8)
                dinky.append((x, y, z, 0, s, t, q, 0))
                slots_d.append((s0, s1))
            else:
                dx, dy, dz, s0 = struct.unpack_from("<4h", b, v)
                bx, by, bz, _ = struct.unpack_from("<4h", b, v + 8)
                s, t, q, s1 = struct.unpack_from("<4h", b, v + 16)
                fat.append((dx, dy, dz, 0, bx, by, bz, 0, s, t, q, 0))
                slots_f.append((s0, s1))
            v += size[l]
    # GS packet slot -> vertex (dinky 0.., fat after them); a slot several vertices write is the last's in the lists' order
    at = {}
    for i, (s0, s1) in enumerate(slots_d + slots_f):
        at[s0] = i
        if s1 != 0 and s1 != s0:
            at[s1] = i
    dest = struct.unpack_from("<4i", b, d)
    src = struct.unpack_from("<4i", b, d + 0x10)
    strips = []
    for k in range(nstrips):
        r = d + 0x2c + 4 * k
        nv, tag = b[r], b[r + 2]
        tex = 0
        for j in range(3):
            if dest[j] != 0 and dest[j] < tag:
                tex = j + 1
        try:
            vidx = [at[tag + 1 + 3 * i] for i in range(nv)]
        except KeyError:
            raise SystemExit("a RAC2 tie strip's vertex has no vertex")
        strips.append((src[tex] // 0x50, vidx))
    return dinky, fat, strips


def tie_colour(g, at, n):
    """An instance's flat colour (RGB, 0x80 = 1.0): the mean of the non-zero RGB 5:5:5 words of its colour list (RAC2
    keeps one list per instance; how its words map to the vertices is not worked out: the 4 bytes before them skipped)."""
    words = [struct.unpack_from("<H", g, at + 2 * k)[0] for k in range(2, n)]
    words = [w for w in words if w & 0x7fff]
    if not words:
        return (0x80, 0x80, 0x80)
    return tuple(sum(((w >> (5 * c)) & 31) << 3 for w in words) / len(words) for c in range(3))


# the arena's mobys drawn as static objects (step 4b): the classes RAC2 places in the arena that stand still there (the
# monitors on posts round the floor 4697, the stands' crowd cards 3734, the floor's fittings 2563 and 3276), in their
# bind pose (rac2moby.py). Not the six jointed arms 2581 at the floor's edge (in their bind pose they stand up as big pods
# across the walkway: RAC2 poses them by their animation), nor the crates (501, 511), nor the challenges' mobys, which wait
# in a block above the arena (z ~520: 2506, 2850, 3131, 3134 ...); 805, 2905, 2953 and 3950 have no mesh in the level.
STATIC_MOBYS = (4697, 3734, 2563, 3276)
MOBY_COLOUR = (0x60, 0x66, 0x6a, 0x80)  # (their flat colour: RAC2 lights them with set 6 and the ambient 27 40 40; ours a grey)


def euler_rows(rx, ry, rz, s):
    """A moby's rotation (x, then y, then z) as the three rows the tie path takes (the axes' images), times its scale."""
    import math
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    m = [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx],
         [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx],
         [-sy, cy * sx, cy * cx]]
    return [[m[0][c] * s, m[1][c] * s, m[2][c] * s] for c in range(3)]


def build_mobys(r2, say, texture, classes):
    """Appends the static mobys' classes to `classes` (as the ties' entries); returns their placements (class index among
    the ones appended, rows, position, colour)."""
    import rac2moby
    MH, idx, core, g = r2.H, r2.index, r2.core, r2.gameplay
    G = struct.unpack_from("<40I", g)
    recs = {}
    for k in range(MH[6]):
        r = MH[7] + 32 * k
        recs[i32(idx, r + 4)] = (i32(idx, r), idx[r + 16:r + 32])
    # the placements (gameplay +0x4c: {count, ...}, then records of their own size: +0x28 the class, +0x2c the scale,
    # +0x40 the position, +0x4c the rotation)
    mo = G[0x4c // 4]
    p = mo + 0x10
    places = []
    for _ in range(i32(g, mo)):
        sz = i32(g, p)
        cid = i32(g, p + 0x28)
        pos = struct.unpack_from("<3f", g, p + 0x40)
        if cid in STATIC_MOBYS and all(ARENA_BOX[j][0] <= pos[j] <= ARENA_BOX[j][1] for j in range(3)):
            places.append((cid, struct.unpack_from("<f", g, p + 0x2c)[0], pos, struct.unpack_from("<3f", g, p + 0x4c)))
        p += sz
    order, insts, ntris = [], [], 0
    for cid in STATIC_MOBYS:
        if not any(pl[0] == cid for pl in places):
            continue
        o, remap = recs[cid]
        cls = rac2moby.decode_class(core, o, remap)
        prim_of, prims, packets = {}, [], []
        for k, (pk, tris) in enumerate(rac2moby.triangles(cls)):
            used = sorted({s for _, vs in tris for s in range(3)})  # (unused: the slots are the vertices)
            del used
            dinky, strips = [], []
            vid = {}
            for prim, vs in tris:
                pr = pk.prims[prim] if prim < len(pk.prims) else None
                if pr is None or pr["texture"] < 0:
                    continue
                key = (pr["texture"], pr["clamp"])
                if key not in prim_of:
                    prims.append((texture(pr["texture"], 14), pr["clamp"], 0x44))
                    prim_of[key] = len(prims) - 1
                idxs = []
                for (x, y, z), (s, t) in vs:
                    kv = (x, y, z, s, t)
                    if kv not in vid:
                        vid[kv] = len(dinky)
                        dinky.append((x, y, z, 0, s, t, 4096, 0))
                    idxs.append(vid[kv])
                strips.append((prim_of[key], idxs))
                ntris += 1
            if strips:
                strips.sort(key=lambda st: st[0])  # (one texture change per texture)
                packets.append((0, len(packets), dinky, [], strips))
        sph = list(cls["sphere"])
        classes.append((cid, cls["scale"], sph, prims, packets))
        order.append(cid)
    for cid, s, pos, rot in places:
        insts.append((order.index(cid), euler_rows(rot[0], rot[1], rot[2], s), list(pos), MOBY_COLOUR))
    say("mobys drawn static (bind pose): classes %s, %d placements, %d triangles" % (order, len(insts), ntris))
    return insts


# the modern lighting's file for the arena (<out>/lights.txt, the level's file while the custom level is on: mrp_lighting.cpp
# user_path): point lights at the arena's lamps and screens, emissive surfaces for the screens and neon (our choices, after
# RAC2's look of Maktar: a dark blue night, cyan neon, warm lamps). Per tie class: (instances: a range of the class's
# placements in their order, None all; the light's colour, intensity, radius; how far toward the arena's centre and up)
ARENA_LIGHTS = [
    (2718, slice(0, 6), (0.40, 0.85, 1.00), 2.2, 16.0, 3.0, 0.0, "the six round cyan gates in the outer wall"),
    (2720, None, (0.45, 1.00, 0.35), 1.0, 5.0, 1.0, 0.0, "the green lamps on the pillars"),
    (2721, None, (1.00, 0.55, 0.20), 0.9, 8.0, 1.5, 0.0, "the orange M screens above the stands"),
    (4154, slice(0, 1), (0.60, 0.90, 1.00), 2.0, 18.0, 0.0, -3.0, "the jumbotron's screens over the centre"),
]
# (class, its texture (the class's local index, as ties.mrpc lists them), strength): the screens and neon keep the
# original's brightness in shade
ARENA_EMISSIVE = [
    ("tie", 2721, 0, 1.6), ("tie", 4154, 0, 1.6), ("tie", 2718, 8, 1.6), ("tie", 2718, 9, 1.4), ("tie", 2701, 5, 1.6),
    ("tie", 2718, 0, 1.4), ("tie", 2720, 2, 1.4), ("tie", 2701, 7, 1.4), ("tie", 2723, 3, 1.4),
    ("tfrag", 0, 8, 1.4), ("tfrag", 0, 5, 1.3),
]


def write_lights(out, insts, say):
    """<out>/lights.txt from the tie placements (k, class, matrix)."""
    cx, cy = 500.0, 600.0
    lines = ["# rac1-native: the modern lighting of the custom level Maktar's arena (tools/custom_level/maktar.py; our choices)",
             "# light x y z  r g b  intensity radius flicker on"]
    n = 0
    for cid, which, rgb, inten, rad, inward, up, what in ARENA_LIGHTS:
        mine = [m for _, c, m in insts if c == cid]
        mine = mine[which] if which is not None else mine
        lines.append("# %s (tie class %d, %d)" % (what, cid, len(mine)))
        for m in mine:
            x, y, z = m[12], m[13], m[14]
            d = ((cx - x) ** 2 + (cy - y) ** 2) ** 0.5
            if d > 1e-3:
                x, y = x + (cx - x) / d * inward, y + (cy - y) / d * inward
            lines.append("light %.3f %.3f %.3f  %.3f %.3f %.3f  %.3f %.3f 0.000 1" % (x, y, z + up, *rgb, inten, rad))
            n += 1
    lines.append("# emissive surfaces: the screens, the gates, the lamps' glass, the neon")
    for kind, cid, tex, strength in ARENA_EMISSIVE:
        lines.append("emissive %s %d tex %d  %.2f 0.00" % (kind, cid, tex, strength))
    open(os.path.join(out, "lights.txt"), "w").write("\n".join(lines) + "\n")
    say("lights.txt: %d point lights, %d emissive surfaces" % (n, len(ARENA_EMISSIVE)))


def build_ties(r2, out, say, colour_scale, colour_min, mobys=True):
    MH = r2.H
    core, idx, g = r2.core, r2.index, r2.gameplay
    G = struct.unpack_from("<40I", g)
    # the instances in the arena's box (gameplay +0x34: count, then 0x60 each: +0 the class, +0x10 the matrix's rows)
    ti = G[0x34 // 4]
    insts = []
    for k in range(i32(g, ti)):
        r = ti + 0x10 + 0x60 * k
        m = struct.unpack_from("<16f", g, r + 0x10)
        if all(ARENA_BOX[j][0] <= m[12 + j] <= ARENA_BOX[j][1] for j in range(3)):
            insts.append((k, i32(g, r), m))
    # their colour lists (gameplay +0x94: {s16 instance, u16 words} and the words, until a negative instance)
    cols = {}
    p, end = G[0x94 // 4], G[0x98 // 4]
    while p + 4 <= end:
        inst, cnt = struct.unpack_from("<hH", g, p)
        if inst < 0:
            break
        cols[inst] = (p + 4, cnt)
        p += 4 + 2 * cnt
    # the classes (index +0x20: 32-byte records {core offset, id, 0, 0, the local textures' remap})
    recs = {}
    for k in range(MH[8]):
        r = MH[9] + 32 * k
        recs[i32(idx, r + 4)] = (i32(idx, r), idx[r + 16:r + 32])
    ids = []
    for _, c, _ in insts:
        if c not in ids:
            ids.append(c)
    gs_rows = {addr: off for _, _, addr, off in vram_rows(idx, MH)}
    tex_of, textures = {}, []

    def texture(t, table=16):  # (a level texture of the table: 16 the ties', 14 the mobys')
        if (table, t) not in tex_of:
            texels, w, h, mips, pal, m1, _ = tex_record(idx, MH, t, table)
            px = MH[24] + texels
            levels = [core[px:px + w * h]]
            if mips >= 2:
                levels.append(core[px + w * h:px + w * h + (w // 2) * (h // 2)])
            if mips >= 3 and m1 > 0 and m1 * 0x100 in gs_rows:
                off = gs_rows[m1 * 0x100]
                levels.append(r2.gs_ram[off:off + (w // 4) * (h // 4)])
            off = gs_rows[pal * 0x100]
            textures.append((w, h, r2.gs_ram[off:off + 0x400], levels))
            tex_of[(table, t)] = len(textures) - 1
        return tex_of[(table, t)]

    classes = []
    nverts = npk = 0
    for cid in ids:
        o, remap = recs[cid]
        scale = struct.unpack_from("<f", core, o + 0x40)[0]
        sphere = struct.unpack_from("<4f", core, o + 0x50)
        adgif = o + i32(core, o + 0x1c)
        prims = []  # per local texture: (our texture, CLAMP's wrap bits, ALPHA_1)
        for lt in range(16):
            if remap[lt] == 0xff:
                break
            regs = {}
            for q in range(5):
                dv, rg = struct.unpack_from("<QQ", core, adgif + 0x50 * lt + 16 * q)
                regs[rg & 0xff] = dv
            prims.append((texture(remap[lt]), regs.get(0x08, 0) & 0xf, regs.get(0x42, 0x44)))
        packets = []
        for lod in range(3):
            table = o + i32(core, o + 4 * lod)
            for k in range(core[o + 0x0c + lod]):
                dinky, fat, strips = rac2_tie_packet(core, table, k)
                packets.append((lod, k, dinky, fat, strips))
                nverts += len(dinky) + len(fat)
                npk += 1
        classes.append((cid, scale, sphere, prims, packets))
    tie_classes = len(classes)
    moby_insts = build_mobys(r2, say, texture, classes) if mobys else []
    say("ties: %d instances of %d classes (%s), %d packets (3 LODs), %d vertices, %d tie textures" % (
        len(insts), len(ids), ", ".join(str(c) for c in ids), npk, nverts, len(textures)))

    b = bytearray(struct.pack("<2I", MRPC_MAGIC, 1))
    b += struct.pack("<I", len(textures))
    for w, h, pal, levels in textures:
        b += struct.pack("<2I", w, h) + pal + struct.pack("<I", len(levels))
        for lv in levels:
            b += struct.pack("<I", len(lv)) + lv
    b += struct.pack("<I", len(classes))
    for cid, scale, sphere, prims, packets in classes:
        b += struct.pack("<if4fI", cid, scale, *sphere, len(prims))
        for t, wrap, alpha in prims:
            b += struct.pack("<IIQ", t, wrap, alpha)
        b += struct.pack("<I", len(packets))
        for lod, k, dinky, fat, strips in packets:
            b += struct.pack("<BBH2I", lod, k, 0, len(dinky), len(fat))
            for v in dinky:
                b += struct.pack("<8h", *v)
            for v in fat:
                b += struct.pack("<12h", *v)
            b += struct.pack("<I", len(strips))
            for lt, vidx in strips:
                b += struct.pack("<2I", lt, len(vidx)) + struct.pack("<%dH" % len(vidx), *vidx)
                if len(vidx) & 1:
                    b += bytes(2)
    b += struct.pack("<I", len(insts) + len(moby_insts))
    shade = []
    for k, c, m in insts:
        at, cnt = cols.get(k, (0, 0))
        rgb = tie_colour(g, at, cnt) if cnt > 2 else (0x80, 0x80, 0x80)
        rgb = [x * colour_scale for x in rgb]
        grey = sum(rgb) / 3.0
        if 0 < grey < colour_min:  # (the lists of the dark ones are mostly their shaded sides: a flat colour that dark reads as a hole)
            rgb = [x * colour_min / grey for x in rgb]
        rgba = tuple(min(255, int(round(x))) for x in rgb) + (0x80,)
        shade.append(sum(rgba[:3]) / 3.0)
        b += struct.pack("<I12f4B", ids.index(c), m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10], m[12], m[13], m[14], *rgba)
    for ci, rows, pos, rgba in moby_insts:
        b += struct.pack("<I12f4B", tie_classes + ci, *(rows[0] + rows[1] + rows[2] + pos), *rgba)
    say("ties: instance colours (each one's list's mean, x %.2f, at least grey %.0f): grey %.0f..%.0f, mean %.0f (0x80 = 1.0)" % (
        colour_scale, colour_min, min(shade), max(shade), sum(shade) / len(shade)))
    open(os.path.join(out, "ties.mrpc"), "wb").write(bytes(b))
    write_lights(out, insts, say)


# step 4c: RAC1 enemies in the arena, in waves. Veldin's own enemies (their classes, code and textures are in Veldin's data
# file and overlay, which the custom level keeps): class 749 (16 on Veldin) and 1440 (9). Each one placed is a copy of one of
# Veldin's records of its class with that record's pvars (its own block: no two share one), its kill-save uid, its light set
# 6 and the arena's ambient; flags 0 (always spawned), no group, hidden (+0x60 bits 0 and 1, OR'd into the moby's +0x34
# flags: not drawn, not updated) until our wave hook (src/game/custom_waves.cpp, <out>/waves.txt) shows its wave. Not the two whose pvars name
# another moby (749 pvar 189, 1440 pvar 231).
WAVE_SKIP_PVARS = (189, 231)


def wave_enemies(g, mo, spec, say):
    """(the enemies' records, per wave their uids) from `spec`: waves separated by ';', each `<class>x<count>` by ','."""
    import math
    n1 = i32(g, mo)
    p = mo + 0x10
    pool = {}
    for _ in range(n1):
        sz = i32(g, p)
        cid = i32(g, p + 0x18)
        if cid in (749, 1440) and i32(g, p + 0x58) not in WAVE_SKIP_PVARS:
            pool.setdefault(cid, []).append(bytes(g[p:p + sz]))
        p += sz
    recs, waves = [], []
    for wi, wave in enumerate(x for x in spec.split(";") if x.strip()):
        group = []
        for part in wave.split(","):
            cid, n = (int(v) for v in part.strip().split("x"))
            for _ in range(n):
                if not pool.get(cid):
                    raise SystemExit("--waves: no more of Veldin's class %d records to copy" % cid)
                group.append((cid, bytearray(pool[cid].pop(0))))
        uids = []
        for k, (cid, r) in enumerate(group):
            ang = 2 * math.pi * (k + 0.5 * wi) / len(group)
            rad = 13.0 if cid == 749 else 17.0
            x, y = 500.0 + rad * math.cos(ang), 600.0 + rad * math.sin(ang)
            struct.pack_into("<i", r, 0x08, 0)  # flags: always spawned
            struct.pack_into("<3f", r, 0x30, x, y, 500.0)
            struct.pack_into("<3f", r, 0x3c, 0.0, 0.0, math.atan2(600.0 - y, 500.0 - x))  # (facing the centre)
            r[0x48] = 0xff  # no group
            struct.pack_into("<i", r, 0x60, i32(r, 0x60) | 3)  # hidden (+0x34 bit 1: not updated, bit 0: not drawn)
            struct.pack_into("<4i", r, 0x64, 27, 40, 40, 6)  # the arena mobys' ambient and light set
            uids.append(i32(r, 0x0c))
            recs.append(bytes(r))
        waves.append(uids)
    say("waves: %s (uids %s)" % (spec, waves))
    return recs, waves


def flight_path(ship, n=12):
    """The paths block with one path, the ship's flight after its take-off (arrival.cpp's step 3 follows it; its last six
    points fade the screen out): from above the ship up and ahead along its heading, out through the dome. The block: count,
    the data's offset and size, then the paths' offsets in the data; a path: its point count, 12 bytes, then 16-byte points
    (x y z, the turn the game computes)."""
    x, y, z, yaw = ship
    dx, dy = math.cos(yaw), math.sin(yaw)
    pts = []
    for k in range(n):
        ahead = 6.0 * k * k / 4.0 + 4.0 * k
        pts.append(struct.pack("<4f", x + dx * ahead, y + dy * ahead, z + 6.0 + 9.0 * k, 0.0))
    path = struct.pack("<4i", n, 0, 0, 0) + b"".join(pts)
    head = struct.pack("<4I", 1, 0x20, len(path), 0) + struct.pack("<4I", 0, 0, 0, 0)
    return head + path


def write_planet(out, a):
    """planet.txt: our own small settings file of the custom planet (src/host/custom_planet.h reads it)."""
    with open(os.path.join(out, "planet.txt"), "w") as f:
        f.write("# A custom planet (rac1-native, local experiment): see src/host/custom_planet.h\n"
                "name %s\nname.it %s\nregion %s\nregion.it %s\ndeath_height %.1f\nship %.2f %.2f %.2f %.4f\n" % (
                    a.name, a.name_it, a.region, a.region_it, a.death_height, *a.ship)
                + "# its sounds and music, read from Ratchet & Clank 2's disc (a data disc: setup), custom_planet.h\n"
                  "rac2_level 2\n"
                  "# LEVEL2.WAD's sound banks (header offsets): the level's, the arena chunk's\n"
                  "rac2_banks 0x18 0x50\n"
                  "# AUDIO2.WAD's entries (left channels): rat2_l02_arena_a then rat2_l02_arena_b looped\n"
                  "music 4 6\n")


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
    # the ship (the custom planet's way out: planet.txt's "ship" line puts it there again, src/host/custom_planet.h) and its
    # take-off: +0x3c the path the flight follows (ours, below), +0x40 / +0x44 the two level cameras the view blends between
    # (none: the take-off scene's last view stays)
    struct.pack_into("<4f", st, 0x2c, *a.ship)
    struct.pack_into("<3i", st, 0x3c, 0, -1, -1)
    blocks[0x00] = bytes(st)
    nl = u32(r2.gameplay, M[1])
    # Maktar's directional light sets, as RAC2 has them. The arena's tfrags name their sets per vertex (mostly set 0, on Maktar
    # a faint blue, 0.04 0.12 0.13: the terrain is its painted base colour, as in RAC2); its mobys all name set 6 (a white key
    # light from above, 45 degrees, a blue-grey fill from below) and the ambient 27 40 40. `--light-set0 <n>` swaps set n into
    # slot 0 (step 3's look, set 6: the terrain lit by the key light too); 0, the default, keeps RAC2's sets.
    lights = [r2.gameplay[M[1] + 0x10 + 0x40 * k:M[1] + 0x50 + 0x40 * k] for k in range(nl)]
    if 0 < a.light_set0 < nl:
        lights[0], lights[a.light_set0] = lights[a.light_set0], lights[0]
    blocks[0x04] = r2.gameplay[M[1]:M[1] + 0x10] + b"".join(lights)
    # the light zones (+0x80; level_load.cpp "the light grid", sub_00256de0 / moby_update_light): one zone over the arena, so the
    # hero (whose light the zone sets each frame) is lit as RAC2 lights the arena's mobys: sets A = B = `--hero-light-set`, the
    # ambient `--hero-ambient`. A zone: a sphere {x y z, radius^2} (tested in x y), then 0x80 bytes: the box's matrix (world ->
    # -1..1, rows as the VU0 code multiplies them), +0x40 / +0x44 the ambients (RGBA bytes) blended across the box's x, +0x48 /
    # +0x4c the light sets, +0x50 flags (1 the light, 2 the fog), +0x54.. the fog (not used: flag 2 off)
    cx, cy, cz = a.hero[0], a.hero[1], 520.0
    half = 90.0
    amb = a.hero_ambient[0] | (a.hero_ambient[1] << 8) | (a.hero_ambient[2] << 16)
    zone = struct.pack("<16f", 1.0 / half, 0, 0, 0, 0, 1.0 / half, 0, 0, 0, 0, 1.0 / half, 0, -cx / half, -cy / half, -cz / half, 1.0)
    zone += struct.pack("<IIiiI", amb, amb, a.hero_light_set, a.hero_light_set, 1)
    zone += bytes(0x80 - len(zone))
    blocks[0x80] = struct.pack("<4i", 1, 0, 0, 0) + struct.pack("<4f", cx, cy, cz, half * half) + zone
    empty = bytes(0x10)
    for f in (0x08, 0x0c, 0x30, 0x34, 0x38, 0x3c, 0x60, 0x64, 0x68, 0x6c, 0x80, 0x88):
        blocks[f] = empty
    # the cameras: Veldin's generic ones (the follow camera, type 0, and types 4..7, which no place holds: zero position; their
    # pvars, +0x1c, Veldin's kept), not its scripted ones (types 23 / 17, whose boxes are Veldin's cuboids, dropped)
    cb = block(0x08)
    cams = [cb[0x10 + 0x20 * k:0x30 + 0x20 * k] for k in range(i32(cb, 0))]
    cams = [c for c in cams if i32(c, 0) in (0, 4, 5, 6, 7) and c[4:16] == bytes(12)]
    blocks[0x08] = struct.pack("<4I", len(cams), 0, 0, 0) + b"".join(cams)
    blocks[0x70] = flight_path(a.ship)  # paths: the ship's take-off
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
    enemies, waves = wave_enemies(g, mo, a.waves, say)
    # their pvars: a class-1440 enemy with a path (pvars +0x1b8 >= 0) flies in along it, and retires itself when the path
    # is not there (the custom level has none): its path made -1, it then starts on the spot (e1440_update's state 3)
    pdata = bytearray(block(0x58))
    ptab = G[0x54 // 4]
    for r in enemies:
        if i32(r, 0x18) == 1440:
            struct.pack_into("<i", pdata, i32(g, ptab + 8 * i32(r, 0x58)) + 0x1b8, -1)
    blocks[0x58] = bytes(pdata)
    blocks[0x44] = struct.pack("<2i8x", 1 + len(enemies), n2) + bytes(hero) + b"".join(enemies)
    blocks[0x48] = struct.pack("<4i", 0, 0, 0, 0)  # (no moby groups: Veldin's name its own records)
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
    say("gameplay %#x -> packed %#x (Veldin's file %#x); lights %d (set 0 Maktar's %d), hero at %s yaw %.2f, death height %.0f, cameras %s" % (
        len(ng), len(gp), gsize, nl, a.light_set0, a.hero, a.hero_yaw, a.death_height, [i32(c, 0) for c in cams]))

    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "data.wad"), "wb").write(bytes(out))
    with open(os.path.join(a.out, "waves.txt"), "w") as f:
        f.write("# rac1-native: the custom level's wave challenge (tools/custom_level/maktar.py; src/game/custom_waves.h)\n"
                "# wave <the uids of its enemies (moby +0xb2), hidden until the wave starts>\n")
        for w in waves:
            f.write("wave %s\n" % " ".join(str(u) for u in w))
    open(os.path.join(a.out, "gameplay.bin"), "wb").write(gp + bytes(gsize - len(gp)))
    write_planet(a.out, a)
    if a.dump_core:
        open(os.path.join(a.out, "core.bin"), "wb").write(bytes(core.buf))
        open(os.path.join(a.out, "index.bin"), "wb").write(bytes(idx))
        open(os.path.join(a.out, "gameplay_unpacked.bin"), "wb").write(bytes(ng))
    if not a.no_ties:
        build_ties(r2, a.out, say, a.tie_colour_scale, a.tie_colour_min, mobys=not a.no_mobys)
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
    ap.add_argument("--ship", type=float, nargs=4, default=(500.0, 578.0, 500.5, -1.5708), help="the ship: x y z yaw (x <= 0: none)")
    ap.add_argument("--name", default="Maktar", help="the planet's row on the ship's list")
    ap.add_argument("--name-it", default="Maktar")
    ap.add_argument("--region", default="Battle Arena", help="the line under the name")
    ap.add_argument("--region-it", default="Arena")
    ap.add_argument("--light-set0", type=int, default=0, help="Maktar's light set swapped into set 0 (the terrain's; 0: RAC2's sets as they are)")
    ap.add_argument("--hero-light-set", type=int, default=6, help="the arena's light zone's set: the hero's light (RAC2's arena mobys: 6)")
    ap.add_argument("--hero-ambient", type=int, nargs=3, default=(27, 40, 40), help="the light zone's ambient (RAC2's arena mobys: 27 40 40)")
    ap.add_argument("--waves", default="749x2;749x3,1440x1;749x4,1440x2",
                    help="the wave challenge's enemies: waves by ';', each <class>x<count> by ',' (Veldin's 749 and 1440); '' none")
    ap.add_argument("--no-ties", action="store_true", help="no ties.mrpc (the arena without its ties)")
    ap.add_argument("--no-mobys", action="store_true", help="the arena's static mobys not drawn (ties.mrpc without them)")
    ap.add_argument("--tie-colour-scale", type=float, default=1.25, help="the ties' instance colours scaled")
    ap.add_argument("--tie-colour-min", type=float, default=80.0, help="the ties' instance colours brightened to at least this grey")
    ap.add_argument("--literal-only", action="store_true", help="WAD-encode with literal runs only")
    ap.add_argument("--dump-core", action="store_true", help="also write the unpacked core, index and gameplay")
    build(ap.parse_args())


if __name__ == "__main__":
    main()
