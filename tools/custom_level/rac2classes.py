"""RAC2 (Going Commando) moby classes converted into RAC1 moby classes, for a custom level's core.

LOCAL EXPERIMENT (feat/custom-levels-experiment). Our own converter, after the formats the two ports describe (rac1-native
src/formats/moby_mrp.cpp decode_class, src/game/ovl/ovl_24_25.cpp moby_class_relocate; rac2-native-game
docs/formats/level-core.md "Moby classes"). Nothing here is game data: the classes are read from the player's RAC2 disc
and written to the custom level's own files (never committed).

Why a conversion is enough: RAC2's moby classes are RAC1's (the same 0x48-byte header, packet table, VIF lists,
sequences and frames, the same animation player), but for one thing: the vertex data's header. RAC1's is 8 words
{matrix transfers, 2-way, 3-way, single records, duplicates, slots, records' offset, colour table's offset} followed by
the transfers at +0x20, and a per-vertex RGBA table after the records; RAC2's is 8 halfwords (the same fields, the 8th
not a colour table) with the transfers at +0x10, and no colour table. So per packet: the header widened to words,
everything after it moved by 0x10 (the records' offset with it), a neutral colour table (0x80808080, 1.0) appended,
the packet's entry pointed at the new data (written at the class's end) with its new size.

The header's other differences (what RAC1 reads otherwise):
  +0x06       RAC2's metal packets (a shiny second layer drawn over the first), RAC1's shadow packets: 0, the metal
              packets left out (their look is lost: 3276's one)
  +0x10       the moby's collision (RAC1 copies it to moby +0x94): RAC1's layout (the same header {joints, primitives' bytes,
              triangles' bytes, vertices' bytes}, primitives, 8-byte vertices in 1024ths, 4-byte triangles), kept for the
              classes that take hits (keep_collision: the monitors), else 0 (RAC1 would test the others against weapons and
              the hero too)
  +0x2c       RAC1 copies the word to the moby pass's far distance: its low halfword kept (RAC2 has more in the upper)
  +0x46       RAC1's class kind (5 enemies, 9 crates ...): -1, none of RAC1's kinds
The textures: a class's 16 remap bytes index the level's moby texture table; the caller gives each RAC2 texture used a
new index in RAC1's table (the texels, palette and mips moved like the tfrags' textures).
"""
import struct

HEADER = 0x48


def u8(b, o):
    return b[o]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def class_ends(core, starts):
    """Each start's end: the next start (of any asset the caller knows of) after it."""
    s = sorted(set(starts) | {len(core)})
    return {a: s[s.index(a) + 1] for a in starts}


def widen_vertex_data(vd):
    """A packet's vertex data, RAC2's layout -> RAC1's (the module's doc). Returns the new bytes."""
    n_mt, n2, n3, n1, ndup, slots, records, _ = struct.unpack_from("<8H", vd, 0)
    n = n2 + n3 + n1
    if n + ndup != slots:
        raise ValueError("vertex data: %d records + %d duplicates != %d slots" % (n, ndup, slots))
    body = bytes(vd[0x10:])
    colours = 0x10 + len(vd)  # (the records and their tail end where RAC2's data ends; RAC1's colours follow them)
    out = bytearray(struct.pack("<8I", n_mt, n2, n3, n1, ndup, slots, records + 0x10, colours)) + body
    out += b"\x80\x80\x80\x80" * n
    out += bytes(align(len(out), 16) - len(out))
    return out


def convert_class(core, start, end, keep_collision=False):
    """RAC2's class at core [start, end) as a RAC1 class: (the bytes, the largest packet's vertex data in qwords). Its
    collision kept with keep_collision (it stays where it was in the class: the new vertex data goes after it)."""
    c = bytearray(core[start:end])
    table = i32(c, 0)
    largest = 0
    if table > 0:
        n = c[4] + c[5]  # (the two LODs; the metal packets after them are left out)
        for k in range(n):
            e = table + 16 * k
            vd_at, dq = i32(c, e + 8), c[e + 12]
            vd = widen_vertex_data(c[vd_at:vd_at + 16 * dq])
            if len(vd) // 16 > 255:
                raise ValueError("class at %#x packet %d: vertex data %d qwords" % (start, k, len(vd) // 16))
            at = align(len(c), 16)
            c += bytes(at - len(c)) + vd
            struct.pack_into("<i", c, e + 8, at)
            c[e + 12] = len(vd) // 16
            largest = max(largest, len(vd) // 16)
    c[6] = 0
    if not keep_collision:
        struct.pack_into("<i", c, 0x10, 0)
    struct.pack_into("<I", c, 0x2c, struct.unpack_from("<I", c, 0x2c)[0] & 0xffff)
    struct.pack_into("<h", c, 0x46, -1)
    c += bytes(align(len(c), 16) - len(c))
    # every pointer the relocation follows must stay inside the class
    for f in (0x00, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x28):
        if not 0 <= i32(c, f) < len(c):
            raise ValueError("class at %#x: header +%#x points outside it" % (start, f))
    for s in range(c[0xc]):
        if not 0 <= i32(c, HEADER + 4 * s) < len(c):
            raise ValueError("class at %#x: sequence %d outside it" % (start, s))
    return bytes(c), largest


def sound_lists(index, H):
    """RAC2's per-class sound lists (index +0x70: per class {offset, count} from +8, the list's halfwords one every 4
    bytes): class index -> (bank selector, [sound ids]). The leading halfword picks the bank (0xffff the level's, 1 the
    chunk's: inferred, PLANET_PLAN.md section 1)."""
    s6 = H[0x70 // 4]
    out = {}
    for k in range(H[6]):
        off, cnt = struct.unpack_from("<2h", index, s6 + 8 + 4 * k)
        if cnt <= 0:
            continue
        words = [u16(index, s6 + off + 4 * j) for j in range(cnt)]
        out[k] = (words[0], words[1:])
    return out
