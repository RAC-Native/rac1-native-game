#!/usr/bin/env python3
"""The night arena's textures: PNGs of our own, made here (no game data).

    python3 make_textures.py [textures/]

Each is a power of two (64 or 128), palettised to at most 256 colours, as the level format wants
(docs/mods.md, "Level mods": map_Kd PNG, at most 256 x 256, at most 256 colours). The tile sizes the
build script maps them with are in build_arena.py's TILE table.
"""
import math
import os
import random
import sys

from PIL import Image


def clamp(v):
    return max(0, min(255, int(v)))


def make(path, w, h, pixel, seed=1):
    random.seed(seed)
    img = Image.new("RGB", (w, h))
    img.putdata([tuple(clamp(c) for c in pixel(x, y)) for y in range(h) for x in range(w)])
    img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(path, optimize=True)
    print("wrote", path)


def noise(a):
    return random.uniform(-a, a)


def floor(x, y):
    # 4 plates of 2 units (64 px) per tile: seams, a bevel, rivets at the plates' corners
    px, py = x % 64, y % 64
    base = [50, 55, 63]
    k = 1.0 + noise(0.05)
    if px < 2 or py < 2:
        k = 0.45  # the seam
    elif px < 4 or py < 4:
        k *= 1.25  # the lit bevel
    elif px > 60 or py > 60:
        k *= 0.8  # the shaded bevel
    for cx, cy in ((8, 8), (56, 8), (8, 56), (56, 56)):
        d = math.hypot(px - cx, py - cy)
        if d < 2.5:
            k = 1.45 - d * 0.15
    # a faint brushed grain, a different direction per plate
    if ((x // 64) + (y // 64)) % 2:
        k *= 1.0 + 0.04 * math.sin(y * 1.7)
    else:
        k *= 1.0 + 0.04 * math.sin(x * 1.7)
    return [c * k for c in base]


def wall(x, y):
    # vertical ribs every 32 px, a panel line across the middle, streaks running down
    base = [58, 64, 76]
    k = 1.0 + noise(0.04)
    rx = x % 32
    if rx < 3:
        k = 1.35
    elif rx < 5:
        k = 0.6
    if 62 <= y <= 65:
        k = 0.55 if y in (62, 63) else 1.2
    k *= 0.9 + 0.1 * math.sin(x * 0.37 + math.sin(x * 0.11) * 3)
    return [c * k for c in base]


def concrete(x, y):
    base = [104, 102, 98]
    k = 1.0 + noise(0.07)
    if random.random() < 0.03:
        k *= 0.7
    k *= 1.0 + 0.05 * math.sin(x * 0.2 + y * 0.13)
    return [c * k for c in base]


def grating(x, y):
    # bars 3 px wide every 8 px, holes dark
    gx, gy = x % 8, y % 8
    if gx < 3 or gy < 3:
        k = 1.0 + noise(0.05) + (0.2 if gx == 0 or gy == 0 else 0)
        return [92 * k, 96 * k, 100 * k]
    return [14, 16, 20]


def hazard(x, y):
    yellow = (x + y) % 32 < 16
    k = 1.0 + noise(0.06)
    return [230 * k, 176 * k, 28 * k] if yellow else [28 * k, 28 * k, 30 * k]


def pad(x, y):
    # a landing pad seen from above, the texture once across it: a ring, a cross of lines, chevrons
    cx, cy = x - 63.5, y - 63.5
    d = math.hypot(cx, cy)
    base = [44, 50, 62]
    k = 1.0 + noise(0.04)
    if 46 < d < 52:
        return [210, 214, 220]
    if (abs(cx) < 1.5 or abs(cy) < 1.5) and d < 40:
        return [190, 196, 204]
    if x < 3 or y < 3 or x > 124 or y > 124:
        return [230, 176, 28]
    return [c * k for c in base]


def ground(x, y):
    base = [34, 37, 40]
    k = 1.0 + noise(0.12)
    k *= 1.0 + 0.12 * math.sin(x * 0.19) * math.cos(y * 0.23)
    return [c * k for c in base]


def wrap_d(ax, ay, bx, by, size):
    dx = abs(ax - bx)
    dy = abs(ay - by)
    return math.hypot(min(dx, size - dx), min(dy, size - dy))


LAVA_CELLS = []


def lava(x, y):
    # crust plates (a tiling Voronoi) parted by molten cracks: the cracks bright yellow-orange, the plates dark red,
    # warmer toward their edges (the lava material makes the bright texels glow)
    if not LAVA_CELLS:
        random.seed(11)
        LAVA_CELLS.extend((random.uniform(0, 128), random.uniform(0, 128)) for _ in range(14))
    d = sorted(wrap_d(x, y, cx, cy, 128) for cx, cy in LAVA_CELLS)
    edge = d[1] - d[0]
    k = noise(0.06)
    if edge < 2.5:
        return [255, 215 - edge * 30, 90 - edge * 30]
    if edge < 6:
        t = (edge - 2.5) / 3.5
        return [255 - 120 * t, 120 - 90 * t, 20 - 15 * t]
    t = min(1.0, (edge - 6) / 10)
    return [(110 - 60 * t) * (1 + k), (28 - 16 * t) * (1 + k), 8 * (1 + k)]


def rock(x, y):
    # brown-grey rock: strata along v, a coarse grain, tiling (whole periods over 128)
    p = 2 * math.pi / 128
    strata = 0.12 * math.sin(y * p * 6 + 1.7 * math.sin(x * p * 2)) + 0.06 * math.sin(y * p * 17)
    grain = 0.08 * math.sin(x * p * 9 + y * p * 4) * math.cos(y * p * 7)
    k = 1.0 + strata + grain + noise(0.08)
    return [96 * k, 80 * k, 68 * k]


def floor_dark(x, y):
    # big diamond-tread plates, darker: the border by the walls, the hub's and the towers' tops
    px, py = x % 64, y % 64
    k = 1.0 + noise(0.05)
    if px < 2 or py < 2:
        k = 0.5
    elif ((px // 8) + (py // 8)) % 2 == 0 and (px % 8 in (3, 4)) != (py % 8 in (3, 4)):
        k *= 1.3  # the tread's raised bars, turned each square
    return [40 * k, 42 * k, 46 * k]


def tech(x, y):
    # a 4-unit panel (64 px): dark, a cyan line along its edges, a small light in its middle (the bright texels glow:
    # an emissive threshold)
    e = min(x, y, 63 - x, 63 - y)
    d = math.hypot(x - 31.5, y - 31.5)
    if e < 2:
        return [60, 230, 255] if e < 1 else [30, 140, 170]
    if d < 3:
        return [180, 250, 255]
    k = 1.0 + noise(0.05)
    if e < 4:
        k *= 0.7
    return [34 * k, 40 * k, 48 * k]


def dropzone(x, y):
    # a 16 x 16 drop zone, once across it (128 px): a hazard border, a dark grated middle, an orange ring and arrows
    e = min(x, y, 127 - x, 127 - y)
    if e < 8:
        return [230, 176, 28] if ((x + y) // 8) % 2 == 0 else [28, 28, 30]
    d = math.hypot(x - 63.5, y - 63.5)
    if 40 < d < 45:
        return [240, 110, 20]
    if (x % 8 < 2 or y % 8 < 2) and d < 40:
        return [70, 74, 80]
    if d < 40:
        return [20, 22, 26]
    return [48, 52, 58]


def vent(x, y):
    # a 4-unit flame vent (64 px): a frame, slots over a glowing orange depth
    e = min(x, y, 63 - x, 63 - y)
    if e < 6:
        return [230, 176, 28] if ((x + y) // 6) % 2 == 0 else [30, 30, 32]
    if e < 9:
        return [60, 62, 66]
    if (x // 5) % 2 == 0:
        return [44, 46, 50]
    d = math.hypot(x - 31.5, y - 31.5)
    t = min(1.0, d / 26.0)
    return [255, 200 - 120 * t, 60 - 50 * t]


def steel(x, y):
    # brushed steel for the grates' bars: streaks along u, a little rust in the corners of the tile
    p = 2 * math.pi / 64
    k = 1.0 + 0.10 * math.sin(y * p * 23 + 0.7 * math.sin(x * p * 2)) + 0.05 * math.sin(y * p * 57) + noise(0.04)
    return [120 * k, 124 * k, 130 * k]


def vent_glow(x, y):
    # the bed of embers at the bottom of a vent: bright cracks (a tiling Voronoi) over dark red coals
    if not VENT_CELLS:
        random.seed(21)
        VENT_CELLS.extend((random.uniform(0, 64), random.uniform(0, 64)) for _ in range(10))
    d = sorted(wrap_d(x, y, cx, cy, 64) for cx, cy in VENT_CELLS)
    e = d[1] - d[0]
    k = 1.0 + noise(0.08)
    if e < 1.5:
        return [255, 240, 150]
    if e < 4:
        t = (e - 1.5) / 2.5
        return [255, 200 - 110 * t, 60 - 50 * t]
    t = min(1.0, (e - 4) / 8)
    return [(200 - 110 * t) * k, (60 - 45 * t) * k, 10 * k]


VENT_CELLS = []


def flame_rgba(w, h, core, seed):
    """A tongue of fire (a sprite: transparent round it): a teardrop, its edge wavy, white-yellow in the middle,
    orange then red outward. RGBA, at most 128 colours (the alpha only 0 or 255)."""
    random.seed(seed)
    img = Image.new("RGBA", (w, h))
    px = []
    for y in range(h):
        v = 1.0 - y / (h - 1.0)  # 0 at the bottom, 1 at the tip
        for x in range(w):
            u = (x + 0.5) / w * 2 - 1  # -1 .. 1 across
            wave = 0.10 * math.sin(v * 9 + 1.3) + 0.06 * math.sin(v * 21 + 0.4)
            # the width: round at the bottom, narrowing to a point
            width = (math.sin(min(1.0, v * 3.2) * math.pi / 2) ** 0.6) * (1.0 - v) ** 0.8 * (0.95 if core else 1.0)
            d = abs(u - wave * v) / max(width, 1e-3)
            # the upper half parted into licks: a notch running up from the edge
            lick = 0.5 + 0.5 * math.sin(u * 7.0 + v * 5.0 + 0.8)
            if not core and v > 0.45 and d > 0.55 and lick < (v - 0.45) * 1.6:
                d = 2.0
            if d > 1.0 or v > 0.97:
                px.append((0, 0, 0, 0))
                continue
            t = min(1.0, d * 0.85 + v * 0.3)  # the heat: the middle hottest, the whole height of it
            if core:
                t *= 0.45
            if t < 0.3:
                c = (255, 250, 210)
            elif t < 0.55:
                c = (255, 220 - 120 * (t - 0.3) / 0.25, 90 - 70 * (t - 0.3) / 0.25)
            else:
                c = (255 - 60 * (t - 0.55) / 0.45, 100 - 70 * (t - 0.55) / 0.45, 20)
            n = noise(0.05)
            px.append(tuple(clamp(ch * (1 + n)) for ch in c) + (255,))
    img.putdata(px)
    rgb = img.convert("RGB").quantize(colors=127, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")
    out = Image.new("RGBA", (w, h))
    out.putdata([c + (a,) for c, a in zip(rgb.getdata(), img.getchannel("A").getdata())])
    return out


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "textures")
    os.makedirs(out, exist_ok=True)
    make(os.path.join(out, "floor.png"), 128, 128, floor, 1)
    make(os.path.join(out, "wall.png"), 128, 128, wall, 2)
    make(os.path.join(out, "concrete.png"), 64, 64, concrete, 3)
    make(os.path.join(out, "grating.png"), 64, 64, grating, 4)
    make(os.path.join(out, "hazard.png"), 64, 64, hazard, 5)
    make(os.path.join(out, "pad.png"), 128, 128, pad, 6)
    make(os.path.join(out, "lava.png"), 128, 128, lava, 8)
    make(os.path.join(out, "rock.png"), 128, 128, rock, 9)
    make(os.path.join(out, "floor_dark.png"), 128, 128, floor_dark, 10)
    make(os.path.join(out, "tech.png"), 64, 64, tech, 11)
    make(os.path.join(out, "dropzone.png"), 128, 128, dropzone, 12)
    make(os.path.join(out, "vent.png"), 64, 64, vent, 13)
    make(os.path.join(out, "steel.png"), 64, 64, steel, 14)
    make(os.path.join(out, "vent_glow.png"), 64, 64, vent_glow, 15)
    for name, w, h, core, seed in (("flame.png", 64, 128, False, 16), ("flame_core.png", 32, 64, True, 17)):
        flame_rgba(w, h, core, seed).save(os.path.join(out, name))
        print("wrote", os.path.join(out, name))


if __name__ == "__main__":
    main()
