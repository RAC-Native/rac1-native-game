#!/usr/bin/env python3
"""WAD LZ for RAC1: a decoder that is RAC1's own (strict) and an encoder whose output it accepts.

RAC1's decoder, wad_decompress (orig 0x24d8d0, ported in src/game/ovl/level_setup.cpp), is stricter than the
lenient decoders of the tools (RAC2 port's rac2disc.py, src/formats/scene_wad): a stream that one of those accepts
can trap in the game. Its rules, which decode() below follows step by step:

- "WAD", then the stream's total size (u32 at +3, the 16-byte header included), the codes from +0x10.
- The codes are read through the scratchpad in 0x2000-byte halves (two loaded at the start, then one more each
  time a half ends). A half ends with an end-of-half code (0x10..0x17 with distance 0 and a count other than 1,
  e.g. `12 00 00`); the rest of that half is padding, and the codes go on at the start of the next one. A code never
  straddles two halves. The last half has no end code: the stream stops where the total size says.
- States: Top (a literal run or a match may come) and Next (only a match). A literal run (code < 0x10, 4..273
  bytes) leaves Next; a match whose last read byte has low bits k copies k (0..3) literals after it and leaves Next
  when k > 0, Top when k = 0. A code below 0x10 in Next traps. `11 0k 00` is a match of nothing (count 1, distance
  0): k literals, back to Top when k = 0.
- The first code may also be a run of (byte - 0x11) literals (byte > 0x11).

The encoder: greedy LZ77 (3-byte hash chains, distances up to 16384 through the 0x20 and 0x40 codes, lengths 3..288)
that tracks the decoder's state and the halves; no packet crosses a half's end, each half closes with `12 00 00`
and padding. encode(data, lz=False) writes literal runs only. Python's standard library only.
"""
import struct
import sys

HALF = 0x2000


class WadError(Exception):
    pass


def decode(blob, off=0):
    """RAC1's wad_decompress on blob[off:], step by step (the scratchpad halves included). Raises WadError where the
    game would trap or read outside its two halves. -> the decompressed bytes."""
    if blob[off:off + 3] != b"WAD":
        raise WadError("bad magic (teq)")
    size = struct.unpack_from("<I", blob, off + 3)[0]
    src = off + 0x10
    spr = bytearray(2 * HALF)
    state = {"madr": src, "half": 0}

    def dma():
        h, m = state["half"], state["madr"]
        chunk = blob[m:m + HALF]
        spr[h:h + HALF] = chunk + bytes(HALF - len(chunk))
        state["half"] ^= HALF

    dma()
    state["madr"] += HALF
    dma()
    state["madr"] += HALF
    size -= 0x10
    pos = 0  # the read position in the scratchpad (0 .. 0x4000)
    end = 1 << 31 if size > 0x1fff else pos + size
    out = bytearray()

    def rd(p):
        if not (0 <= p < 2 * HALF):
            raise WadError("read outside the scratchpad at %#x" % p)
        return spr[p]

    st = "First"
    while True:
        if st == "First":
            lit = rd(pos) - 0x11
            if lit > 0:
                pos += 1
                for _ in range(lit):
                    out.append(rd(pos))
                    pos += 1
                st = "Next"
                continue
            st = "Top"
            continue
        more = pos < end
        if not more:
            break
        v = rd(pos)
        pos += 1
        if st == "Top" and v < 0x10:
            if v == 0:
                v = rd(pos) + 0xf
                pos += 1
            for _ in range(v + 3):
                out.append(rd(pos))
                pos += 1
            st = "Next"
            continue
        copy = True
        if v >= 0x40:
            dist = 1 + ((v >> 2) & 7) + rd(pos) * 8
            pos += 1
            n = (v >> 5) - 1
        elif v >= 0x20:
            n = v & 0x1f
            if n == 0:
                n = rd(pos) + 0x1f
                pos += 1
            dist = 1 + (rd(pos) >> 2) + rd(pos + 1) * 64
            pos += 2
        elif v >= 0x10:
            far = (v & 8) << 11
            n = v & 7
            if n == 0:
                n = rd(pos) + 7
                pos += 1
            d = (rd(pos) >> 2) + rd(pos + 1) * 64
            pos += 2
            if far + d == 0:
                if n != 1:  # the end of a half
                    dma()
                    size -= HALF
                    state["madr"] += HALF
                    pos = state["half"]
                    end = 1 << 31 if size > 0x1fff else pos + size
                    st = "Top"
                    continue
                copy = False
            else:
                dist = far + d + 0x4000
        else:
            raise WadError("code %#x after literals (teq) at output %#x" % (v, len(out)))
        if copy:
            frm = len(out) - dist
            if frm < 0:
                raise WadError("match before the output's start at %#x" % len(out))
            for i in range(n + 2):
                out.append(out[frm + i])
        k = rd(pos - 2) & 3
        if k == 0:
            st = "Top"
            continue
        for i in range(3):
            rd(pos + i)  # (the decoder reads three bytes whatever k is)
        out += spr[pos:pos + k]
        pos += k
        st = "Next"
    return bytes(out)


class _Writer:
    """The stream in halves: each packet is placed whole in the current half, which closes with `12 00 00`."""

    def __init__(self):
        self.halves = []
        self.cur = bytearray()
        self.state = "Top"
        self.room = HALF - 3 - 3  # the end code and the decoder's three-byte look past a packet

    def close_half(self):
        self.cur += b"\x12\x00\x00"
        self.cur += bytes(HALF - len(self.cur))
        self.halves.append(bytes(self.cur))
        self.cur = bytearray()
        self.state = "Top"

    def space(self):
        return self.room - len(self.cur)

    def packet(self, b, next_state):
        """A match or a no-op (valid in either state), or a literal run when the state is Top."""
        if len(b) > self.space():
            self.close_half()
        self.cur += b
        self.state = next_state

    def finish(self):
        body = b"".join(self.halves) + bytes(self.cur)
        return b"WAD" + struct.pack("<I", 0x10 + len(body)) + bytes(9) + body


def _run_code(c):
    return bytes([c - 3]) if c <= 18 else bytes([0, c - 18])


def _emit_literals(w, data, i, n):
    """n literals data[i:i+n] (any count), from whatever state the stream is in."""
    while n > 0:
        if n < 4:
            w.packet(bytes([0x11, n, 0]) + data[i:i + n], "Next")  # (the no-op match carries them)
            return
        if w.state == "Next":
            w.packet(b"\x11\x00\x00", "Top")
        c = min(n, 273, w.space() - 2)
        if c < n and n - c < 4:
            c = n - 4
        if c < 4:
            w.close_half()
            continue
        w.packet(_run_code(c) + data[i:i + c], "Next")
        i += c
        n -= c


def _match_code(dist, length, k):
    d = dist - 1
    if 3 <= length <= 8 and d < 2048:
        return bytes([((length - 1) << 5) | ((d & 7) << 2) | k, d >> 3])
    n = length - 2
    tail = bytes([((d & 0x3f) << 2) | k, d >> 6])
    if n <= 31:
        return bytes([0x20 | n]) + tail
    return bytes([0x20, n - 0x1f]) + tail


MAX_DIST = 16384
MAX_LEN = 288


def _matches(data, chain):
    """Greedy LZ77 over data: [(position, distance, length)], 3-byte hash chains."""
    n = len(data)
    heads = {}
    prev = {}
    out = []
    i = 0
    while i < n:
        best_len = best_dist = 0
        if i + 3 <= n:
            key = data[i:i + 3]
            cand = heads.get(key, -1)
            limit = min(MAX_LEN, n - i)
            tries = chain
            while cand >= 0 and tries > 0 and i - cand <= MAX_DIST:
                if best_len == 0 or data[cand + best_len] == data[i + best_len]:
                    L = 3
                    while L < limit and data[cand + L] == data[i + L]:
                        L += 1
                    if L > best_len:
                        best_len, best_dist = L, i - cand
                        if L == limit:
                            break
                cand = prev.get(cand, -1)
                tries -= 1
            prev[i] = heads.get(key, -1)
            heads[key] = i
        if best_len >= 3:
            out.append((i, best_dist, best_len))
            end = i + best_len
            for p in range(i + 1, min(end, n - 2)):
                key = data[p:p + 3]
                prev[p] = heads.get(key, -1)
                heads[key] = p
            i = end
        else:
            i += 1
        if len(prev) > 4 * MAX_DIST:  # (keep the chains' memory bounded)
            lo = i - MAX_DIST
            prev = {p: q for p, q in prev.items() if p >= lo}
    return out


def encode(data, lz=True, chain=8):
    """data -> a WAD stream RAC1's decoder accepts (decode(encode(x)) == x). lz=False: literal runs only."""
    data = bytes(data)
    n = len(data)
    ms = _matches(data, chain) if lz else []
    w = _Writer()
    cursor = 0
    for idx, (pos, dist, L) in enumerate(ms):
        if pos > cursor:
            _emit_literals(w, data, cursor, pos - cursor)
        after = pos + L
        nxt = ms[idx + 1][0] if idx + 1 < len(ms) else n
        k = nxt - after if nxt - after <= 3 else 0
        w.packet(_match_code(dist, L, k) + data[after:after + k], "Next" if k else "Top")
        cursor = after + k
    if n > cursor:
        _emit_literals(w, data, cursor, n - cursor)
    return w.finish()


def _selftest():
    import os
    import random
    rnd = random.Random(1)
    cases = [b"", b"a", b"ab", b"abc", b"abcd", bytes(100000), os.urandom(70000), b"abcabcabcabc" * 5000]
    for _ in range(40):
        parts = []
        for _ in range(rnd.randint(1, 60)):
            r = rnd.random()
            if r < 0.3:
                parts.append(os.urandom(rnd.randint(1, 600)))
            elif r < 0.6 and parts:
                src = b"".join(parts)
                a = rnd.randint(0, len(src) - 1)
                parts.append(src[a:a + rnd.randint(1, 400)])
            else:
                parts.append(bytes([rnd.randint(0, 255)]) * rnd.randint(1, 500))
        cases.append(b"".join(parts))
    for c in cases:
        for lz in (True, False):
            e = encode(c, lz=lz)
            d = decode(e)
            if d != c:
                raise SystemExit("round trip failed (lz=%s, %d bytes)" % (lz, len(c)))
    print("wadlz self-test: %d cases round-trip through RAC1's decoder" % len(cases))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "test":
        _selftest()
    elif len(sys.argv) > 2 and sys.argv[1] == "decode":
        sys.stdout.buffer.write(decode(open(sys.argv[2], "rb").read()))
    else:
        print("usage: wadlz.py test | decode FILE")
