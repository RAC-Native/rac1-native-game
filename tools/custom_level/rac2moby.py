"""RAC2 (Going Commando) moby classes as static meshes in their bind pose, for a custom level's host-drawn objects.

LOCAL EXPERIMENT (experiment/custom-level-maktar). Our own decoder, after the formats the two ports describe:
rac2-native-game docs/formats/level-core.md "Moby classes" (the class header, the packet table, the vertex data's header)
and rac1-native src/formats/moby_mrp.cpp decode_class (the VIF list: UVs by V2-16, the index list by V4-8, the texture
prims by V4-32; the index list as the VU1 program walks it; the vertex records, the vertex cache and its duplicates).
RAC2's vertex data is RAC1's but for its header (halfwords instead of words) and no colour table.

The bind pose: a vertex's record holds its bind-space position (s16, x the class scale / 1024); the skinning's matrices
are the joints' times their inverse bind, the identity at rest, so the bind pose is the records' positions. A class's
triangles come from its high LOD's packets: the index list's vertices make GS triangle strips (a negative index: the
vertex closes no triangle, ADC; an index 0: a texture switch, the strip restarted at the slot the prim names).
"""
import struct


def u8(b, o):
    return b[o]


def i8(b, o):
    return struct.unpack_from("<b", b, o)[0]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def i16(b, o):
    return struct.unpack_from("<h", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def parse_vif(v):
    """The VIF list's UNPACKs: (vn, vl, num, addr, data offset)."""
    out, i = [], 0
    while i + 4 <= len(v):
        code = u32(v, i)
        i += 4
        cmd = (code >> 24) & 0x7f
        if cmd == 0:
            continue
        if cmd < 0x60:
            raise ValueError("VIF code %08x" % code)
        vn, vl = (cmd >> 2) & 3, cmd & 3
        num = (code >> 16) & 0xff or 256
        bits = 16 if (vn == 3 and vl == 3) else (32 >> vl) * (vn + 1)
        n = ((num * bits + 31) // 32) * 4
        out.append((vn, vl, num, code & 0x3ff, i))
        i += n
    return out


class Packet:
    pass


def decode_class(core, o, remap):
    """The class at core offset `o` (its local texture remap: 16 bytes): its high LOD's packets, decoded. None: no geometry
    (another layout)."""
    table = i32(core, o)
    if table <= 0:
        return None
    high, low = core[o + 4], core[o + 5]
    cls = {"joints": core[o + 8], "scale": struct.unpack_from("<f", core, o + 0x24)[0],
           "sphere": struct.unpack_from("<4f", core, o + 0x30), "packets": []}
    cache = {}
    for k in range(high):
        e = o + table + 16 * k
        vif_o, vqwc, vd_o, dq, ntr = i32(core, e), u16(core, e + 4), i32(core, e + 8), core[e + 12], core[e + 15]
        vif = core[o + vif_o:o + vif_o + 16 * vqwc]
        vd = core[o + vd_o:o + vd_o + 16 * dq]
        p = Packet()
        p.uv, p.indices, p.prims, p.header = [], [], [], None
        for vn, vl, num, addr, at in parse_vif(vif):
            if vn == 1 and vl == 1:
                p.uv = [struct.unpack_from("<2h", vif, at + 4 * s) for s in range(num)]
            elif vn == 3 and vl == 2:
                p.header = struct.unpack_from("<4b", vif, at)
                p.indices = list(struct.unpack_from("<%db" % (num * 4 - 4), vif, at + 4))
            elif vn == 3 and vl == 0:
                for t in range(0, num - 3, 4):
                    q = at + 16 * t
                    regs = {}
                    for r in range(4):
                        data = struct.unpack_from("<Q", vif, q + 16 * r)[0]
                        reg = int(struct.unpack_from("<f", vif, q + 16 * r + 8)[0])
                        regs[r] = (reg, data)
                    local = u32(vif, q + 32)  # (TEX0's low word: the class's local texture)
                    secret = i8(vif, q + 0xc)
                    p.prims.append({"local": local, "texture": remap[local & 15] if local < 16 else -1, "next": secret,
                                    "clamp": regs[1][1] & 0xf})
            else:
                raise ValueError("UNPACK vn %d vl %d" % (vn, vl))
        # the vertex data: halfwords {matrix transfers, 2-way, 3-way, single, duplicates, slots, records' offset, tail}
        n_mt, n2, n3, n1, ndup, ntr2, vto, _tail = struct.unpack_from("<8H", vd, 0)
        n = n2 + n3 + n1
        if n + ndup != ntr2:
            raise ValueError("packet %d: %d records + %d duplicates != %d slots" % (k, n, ndup, ntr2))
        dup_at = 0x10 + ((2 * n_mt + 6) & ~7)
        dups = [u16(vd, dup_at + 2 * d) >> 7 for d in range(ndup)]
        pos, cidx = [], []
        for v in range(n):
            r = vto + 16 * v
            pos.append(struct.unpack_from("<3h", vd, r + 10))
            cidx.append(u16(vd, vto + 16 * (v + 7)) & 0x1ff if v + 7 < n else None)
        if n > 0:  # (the tail: the last records' cache indices, rac1-native 0x1ef184)
            ep = (len(vd) - vto) // 16 - n
            first = max(7 - n, 0)
            q = vto + 16 * n + 16 * first
            for k2 in range(first, ep):
                dest = n + k2 - 7
                if 0 <= dest < n and q + 2 <= len(vd):
                    cidx[dest] = u16(vd, q) & 0x1ff
                q += 16
            if ep > 0:
                last = vto + 16 * (n + ep - 1)
                for k2 in range(max(7 - n - ep, 0), 6):
                    dest = n + ep + k2 - 7
                    if 0 <= dest < n and last + 6 + 2 * k2 <= len(vd):
                        cidx[dest] = u16(vd, last + 4 + 2 * k2) & 0x1ff
        # slots 1..n the records, n+1.. the duplicates (through the cache of this LOD's packets drawn before)
        slots = [None] + list(pos)
        for v in range(n):
            if cidx[v] is not None:
                cache[cidx[v]] = pos[v]
        for d in dups:
            slots.append(cache.get(d & 0x1ff))
        p.slots = slots
        # the strip ops: (slot, adc, prim switched to or -1)
        ops, secret, prim = [], p.header[2] if p.header else 0, 0
        for ix in p.indices:
            if ix > 0:
                ops.append((ix, False, -1))
            elif ix < 0:
                ops.append((ix + 128, True, -1))
            else:
                if secret == 0:
                    break
                ops.append((secret, False, prim))
                secret = p.prims[prim]["next"] if prim < len(p.prims) else 0
                prim += 1
        p.ops = ops
        cls["packets"].append(p)
    return cls


def triangles(cls):
    """The class's triangles: per packet, [(prim index, ((x y z), (s t)) x 3)]; vertices a duplicate of no known vertex
    (another moby's cache) dropped with their triangles."""
    out = []
    for p in cls["packets"]:
        tris = []
        prim = 0
        q = []
        for slot, adc, sw in p.ops:
            if sw >= 0:
                prim = sw
                q = []
            q.append((slot, adc))
            if len(q) >= 3 and not adc:
                vs = q[-3:]
                ok = all(0 < s < len(p.slots) and p.slots[s] is not None for s, _ in vs)
                if ok:
                    tris.append((prim, tuple((p.slots[s], p.uv[s - 1] if s - 1 < len(p.uv) else (0, 0)) for s, _ in vs)))
        out.append((p, tris))
    return out
