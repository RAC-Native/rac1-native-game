"""The arena, built in Blender: run inside Blender (Scripting tab, or exec(open(...).read())).

It (re)makes four collections and leaves the rest of the scene alone:

  arena_visual     the meshes drawn (meshes.obj): textured, vertex coloured, faces split so that a
                   texture repeats at most MAXREP times across a triangle (the format allows 8)
  arena_collision  the simplified collision (collision.obj), shown as wireframe; col_lava is surface type 3
                   (the game's lava: Ratchet burns and bounces; Kalebo III's hero code has no lava yet, the plugin
                   kills him at the third bounce or when he stays in it)
  arena_markers    empties: hero, ship, spawn_<kind>_<name>, trigger_<name> (cubes), obj_<class>,
                   flame_<name> (the flame vents: the plugin's)
  arena_lights     light_sun (Veldin's key light's direction: the sun Veldin's sky shows) and light_<name>
                   point lights

1 Blender unit = 1 game unit, z up, made round (0, 0, 0): export_arena.py moves it (the offset). A material's
"rac1_material" property is its line in lights.txt ("metal 0.5 0.7", "lava 1.5 0.2", "emissive 2 0.4").
The textures come from make_textures.py (textures/). Everything here is our own, no game data.

Layout (x east, y north): the arena floor 96 x 96 at z 0 inside a wall 6 high, standing in a lava lake ringed by
mountains; gates N, E, W (alcoves behind them: the ground enemies' spawns; a drop zone in front of each: the
dropships'); a hub 16 x 16, 3 high, with four ramps and a glowing ring round it; a tower 6 high in each corner with
two ramps; L walls, low walls and pillars for cover; four flame vents; to the south a corridor to the ship's plaza;
floodlight masts in the lava outside the corners.
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

# this script's folder (textures/ beside it, make_textures.py's): its file, or the open .blend's folder when it is run
# without one (exec in Blender's GUI)
HERE = os.path.dirname(os.path.abspath(globals().get("__file__") or bpy.data.filepath))
TEX = os.path.join(HERE, "textures")

# material -> (texture or None, Kd, tile in units, glows in the viewport, lights.txt's material line or None)
MATS = {
    "floor":      ("floor.png",      (1, 1, 1),          4.0, False, "metal 0.5 0.7"),
    "floor_dark": ("floor_dark.png", (1, 1, 1),          4.0, False, "metal 0.4 0.9"),
    "tech":       ("tech.png",       (1, 1, 1),          4.0, True,  "metal 0.4 0.8 glow 2 0.45"),
    "dropzone":   ("dropzone.png",   (1, 1, 1),         16.0, False, "metal 0.45 0.7"),
    "wall":       ("wall.png",       (1, 1, 1),          4.0, False, "metal 0.5 0.7"),
    "concrete":   ("concrete.png",   (1, 1, 1),          2.0, False, None),
    "grating":    ("grating.png",    (1, 1, 1),          2.0, False, "metal 0.4 0.9"),
    "hazard":     ("hazard.png",     (1, 1, 1),          1.0, False, "metal 0.6 0.4"),
    "pad":        ("pad.png",        (1, 1, 1),         10.0, False, "metal 0.25 1"),
    "steel":      ("steel.png",      (1, 1, 1),          2.0, False, "metal 0.3 1"),
    "vent_glow":  ("vent_glow.png",  (1, 1, 1),          4.0, True,  "lava 2.2 0.35"),
    "lava":       ("lava.png",       (1, 1, 1),          8.0, True,  "lava 1.5 0.2"),
    "rock":       ("rock.png",       (1, 1, 1),         16.0, False, None),
    "lamp":       (None,             (1.0, 0.95, 0.8),   4.0, True,  "emissive 2"),
    "lamp_red":   (None,             (1.0, 0.35, 0.2),   4.0, True,  "emissive 2"),
    "lamp_green": (None,             (0.35, 1.0, 0.55),  4.0, True,  "emissive 2"),
    "trim":       (None,             (0.2, 0.85, 1.0),   4.0, True,  "emissive 2"),
}
MAXREP = 6        # texture repeats across a triangle, at most (the format's limit: 8)
VIS_CELL = 24     # the visual faces' largest cell, in units
COL_CELL = 16     # the collision's

H = 48.0          # half the arena's side
FOOT = -2.0       # the outer walls' foot, under the lava
LAVA_Z = -1.5
CRATER = (0.0, -12.0)  # the mountains' ring's centre
KEY_LIGHT = (-64.8, 19.9)  # Veldin's key light (azimuth, elevation): the sun its sky shows (mrp_lighting.cpp's game_sun)


# ---------------------------------------------------------------- geometry

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def lerp3(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


class Geo:
    """Polygons (counter-clockwise seen from their front) split into cells, with world-projected UVs."""

    def __init__(self, cell, uv=True):
        self.cell, self.uv, self.faces = cell, uv, []  # faces: (mat, [p...], [uv...] or None)

    def poly(self, mat, pts, origin=(0.0, 0.0, 0.0)):
        cell = self.cell
        if self.uv:
            cell = min(cell, MATS[mat][2] * MAXREP)
        for piece in self._split(pts, cell):
            self.faces.append((mat, piece, self._uvs(mat, pts, piece, origin) if self.uv else None))

    def tri_facing(self, mat, a, b, c, front):
        """A triangle turned so that its normal goes along `front`."""
        if dot(cross(sub(b, a), sub(c, a)), front) < 0:
            b, c = c, b
        self.poly(mat, [a, b, c])

    def _split(self, pts, cell):
        if len(pts) == 4:
            a, b, c, d = pts
            n = max(1, math.ceil(max(dist(a, b), dist(d, c)) / cell - 1e-6))
            m = max(1, math.ceil(max(dist(a, d), dist(b, c)) / cell - 1e-6))

            def at(u, v):
                return lerp3(lerp3(a, b, u), lerp3(d, c, u), v)

            return [[at(i / n, j / m), at((i + 1) / n, j / m), at((i + 1) / n, (j + 1) / m), at(i / n, (j + 1) / m)]
                    for i in range(n) for j in range(m)]
        a, b, c = pts
        if max(dist(a, b), dist(b, c), dist(c, a)) <= cell:
            return [[a, b, c]]
        ab, bc, ca = lerp3(a, b, 0.5), lerp3(b, c, 0.5), lerp3(c, a, 0.5)
        out = []
        for t in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)):
            out += self._split(list(t), cell)
        return out

    @staticmethod
    def _uvs(mat, whole, piece, origin):
        # projected along the whole face's main axis, so a face's cells line up
        nx, ny, nz = (abs(c) for c in cross(sub(whole[1], whole[0]), sub(whole[-1], whole[0])))
        tile = MATS[mat][2]
        out = []
        for p in piece:
            q = sub(p, origin)
            if nz >= nx and nz >= ny:
                u, v = q[0], q[1]
            elif nx >= ny:
                u, v = q[1], q[2]
            else:
                u, v = q[0], q[2]
            out.append((u / tile, v / tile))
        return out


def rot(p, deg):
    r = math.radians(deg)
    c, s = math.cos(r), math.sin(r)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2])


def box(v, c, mat, x0, y0, z0, x1, y1, z1, top=None, skip=(), tf=None, mats=None):
    """A box's faces, outward (no bottom). v / c: the visual and collision Geo (either may be None); mats: per side."""
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    sides = {
        "top": [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
        "-y": [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
        "+y": [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)],
        "-x": [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)],
        "+x": [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
    }
    for name, pts in sides.items():
        if name in skip:
            continue
        if tf:
            pts = [tf(p) for p in pts]
        m = (mats or {}).get(name) or (top if name == "top" and top else mat)
        if v:
            v.poly(m, pts)
        if c:
            c.poly("type31", pts)


def ramp(v, c, mtop, mside, cx, cy, w, length, zl, zh, d):
    """A wedge rising toward d ('n', 's', 'e', 'w'), from the floor (zl) to zh, w wide."""
    ang = {"n": 0, "w": 90, "s": 180, "e": -90}[d]
    hw, hl = w / 2.0, length / 2.0

    def tf(p):
        q = rot(p, ang)
        return (q[0] + cx, q[1] + cy, q[2])

    faces = [
        (mtop, [(-hw, -hl, zl), (hw, -hl, zl), (hw, hl, zh), (-hw, hl, zh)]),
        (mside, [(-hw, -hl, zl), (-hw, hl, zh), (-hw, hl, zl)]),
        (mside, [(hw, -hl, zl), (hw, hl, zl), (hw, hl, zh)]),
    ]
    for m, pts in faces:
        pts = [tf(p) for p in pts]
        if v:
            v.poly(m, pts)
        if c:
            c.poly("type31", pts)


def quad_floor(v, c, mat, x0, y0, x1, y1, z=0.0, origin=(0.0, 0.0, 0.0)):
    pts = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
    if v:
        v.poly(mat, pts, origin)
    if c:
        c.poly("type31", pts)


def vent(v, x0, y0, size=4.0, depth=-0.9, rim=0.35):
    """A flame vent in a size x size hole of the floor, its corner at (x0, y0)."""
    x1, y1 = x0 + size, y0 + size
    a0, b0, a1, b1 = x0 + rim, y0 + rim, x1 - rim, y1 - rim
    for bx in ((x0, y0, x1, b0), (x0, b1, x1, y1), (x0, b0, a0, b1), (a1, b0, x1, b1)):
        box(v, None, "steel", bx[0], bx[1], 0.0, bx[2], bx[3], 0.08, top="hazard")
    v.poly("wall", [(a1, b0, depth), (a0, b0, depth), (a0, b0, 0.0), (a1, b0, 0.0)])  # the pit's walls, facing in
    v.poly("wall", [(a0, b1, depth), (a1, b1, depth), (a1, b1, 0.0), (a0, b1, 0.0)])
    v.poly("wall", [(a0, b0, depth), (a0, b1, depth), (a0, b1, 0.0), (a0, b0, 0.0)])
    v.poly("wall", [(a1, b1, depth), (a1, b0, depth), (a1, b0, 0.0), (a1, b1, 0.0)])
    v.poly("vent_glow", [(a0, b0, depth), (a1, b0, depth), (a1, b1, depth), (a0, b1, depth)], origin=(a0, b0, 0.0))
    n = 7
    for i in range(n):  # the bars, along y, their tops flush with the floor
        x = a0 + (a1 - a0) * (i + 0.5) / n
        box(v, None, "steel", x - 0.08, b0, -0.14, x + 0.08, b1, 0.0)
    for y in (b0 + 0.9, b1 - 0.9):  # two cross bars under them
        box(v, None, "steel", a0, y - 0.07, -0.32, a1, y + 0.07, -0.14)


def smooth_noise(theta, k):
    """0..1, periodic in theta, a different shape per k."""
    a = 0.5 + 0.28 * math.sin((2 + k) * theta + 1.3 * k) + 0.16 * math.sin((5 + 2 * k) * theta + 0.7 + k)
    a += 0.08 * math.sin((11 + k) * theta + 2.1 * k)
    return max(0.0, min(1.0, a))


# ---------------------------------------------------------------- the layout

DROP_ZONES = {"n": (-8, 26), "e": (26, -8), "w": (-42, -8)}  # their lower-left corners, 16 x 16
VENTS = {"ne": (14, 14), "nw": (-14, 14), "se": (14, -14), "sw": (-14, -14)}  # centres of 4 x 4 cells


def floor_cell(cx, cy):
    """(material, uv origin) of the arena floor's 4 x 4 cell centred at (cx, cy)."""
    for x0, y0 in DROP_ZONES.values():
        if x0 < cx < x0 + 16 and y0 < cy < y0 + 16:
            return "dropzone", (x0, y0, 0.0)
    for vx, vy in VENTS.values():
        if abs(cx - vx) < 2 and abs(cy - vy) < 2:
            return None, None  # (a hole: the vent's own geometry)
    m = max(abs(cx), abs(cy))
    if 20 < m < 24:
        return "tech", (0.0, 0.0, 0.0)
    if m > H - 4:
        return "floor_dark", (0.0, 0.0, 0.0)
    return "floor", (0.0, 0.0, 0.0)


def layout():
    V = {n: Geo(VIS_CELL) for n in ("lava", "arena_floor", "walls", "hub", "towers", "covers", "south_area",
                                     "masts", "lamps", "trim", "mountains", "vents")}
    C = {n: Geo(COL_CELL, uv=False) for n in ("col_floor", "col_walls", "col_hub", "col_towers", "col_covers",
                                              "col_south", "col_lava")}
    lights = []  # (name, pos, colour, intensity, radius)

    # the arena's floor in 4 x 4 cells: plates, a darker band by the walls, a glowing ring round the hub, the drop
    # zones and the flame vents
    for i in range(int(-H), int(H), 4):
        for j in range(int(-H), int(H), 4):
            mat, origin = floor_cell(i + 2, j + 2)
            if mat:
                quad_floor(V["arena_floor"], None, mat, i, j, i + 4, j + 4, origin=origin)
    quad_floor(None, C["col_floor"], None, -H, -H, H, H)
    for ang in (0, -90, 90):  # the N, E, W alcoves' floors (made for N, turned)
        a = [rot(p, ang) for p in ((-6, H, 0), (6, H + 14, 0))]
        quad_floor(V["arena_floor"], C["col_floor"], "floor_dark", min(a[0][0], a[1][0]), min(a[0][1], a[1][1]),
                   max(a[0][0], a[1][0]), max(a[0][1], a[1][1]))
    quad_floor(V["south_area"], C["col_south"], "grating", -5, -64, 5, -H)
    quad_floor(V["south_area"], C["col_south"], "floor", -12, -88, 12, -64)

    # the lava lake: under everything, to the mountains' foot and beyond; its collision is surface type 3 (lava)
    quad_floor(V["lava"], C["col_lava"], "lava", -104, -116, 104, 92, z=LAVA_Z)

    # the perimeter wall (6 high, 2 thick, outside the floor, its foot in the lava), gaps for the gates and the
    # corridor, the lintels over them, the alcoves' walls
    w, cw = V["walls"], C["col_walls"]
    W2 = H + 2
    for seg in ((-W2, -4), (4, W2)):
        box(w, cw, "wall", seg[0], H, FOOT, seg[1], W2, 6)
    for seg in ((-W2, -5), (5, W2)):
        box(w, cw, "wall", seg[0], -W2, FOOT, seg[1], -H, 6)
    for seg in ((-H, -4), (4, H)):
        box(w, cw, "wall", H, seg[0], FOOT, W2, seg[1], 6)
        box(w, cw, "wall", -W2, seg[0], FOOT, -H, seg[1], 6)
    for ang in (0, -90, 90):
        t = lambda p, a=ang: rot(p, a)
        box(w, cw, "wall", -4, H, 4.5, 4, W2, 6, tf=t, skip=("-x", "+x"))
        box(w, cw, "wall", -8, W2, FOOT, -6, W2 + 14, 6, tf=t)
        box(w, cw, "wall", 6, W2, FOOT, 8, W2 + 14, 6, tf=t)
        box(w, cw, "wall", -6, W2 + 12, FOOT, 6, W2 + 14, 6, tf=t, skip=("-x", "+x"))
    box(w, cw, "wall", -5, -W2, 4.5, 5, -H, 6, skip=("-x", "+x"))

    # the trim: a cyan strip along the wall's inner face, under its top
    for ang, segs in ((0, ((-36, -4), (4, 36))), (-90, ((-36, -4), (4, 36))), (90, ((-36, -4), (4, 36))),
                      (180, ((-36, -5), (5, 36)))):
        for a, b in segs:
            pts = [(a, H - 0.03, 5.35), (b, H - 0.03, 5.35), (b, H - 0.03, 5.65), (a, H - 0.03, 5.65)]
            V["trim"].poly("trim", [rot(p, ang) for p in pts])  # facing the arena (-y before turning)

    # the wall's relief: pilasters on its inner face between the towers, a coping along its top (the low sun catches
    # their edges)
    for ang in (0, -90, 90, 180):
        t = lambda p, a=ang: rot(p, a)
        for x in (-15, -9, 9, 15):  # (clear of the towers' ramps along the walls, 18 .. 36)
            box(w, cw, "wall", x - 0.6, H - 0.45, 0, x + 0.6, H, 6.25, top="floor_dark", tf=t, skip=("+y",))
    for x0, y0, x1, y1 in ((-W2 - 0.15, H - 0.15, W2 + 0.15, W2 + 0.15), (-W2 - 0.15, -W2 - 0.15, W2 + 0.15, -H + 0.15),
                           (H - 0.15, -H + 0.15, W2 + 0.15, H - 0.15), (-W2 - 0.15, -H + 0.15, -H + 0.15, H - 0.15)):
        box(w, None, "floor_dark", x0, y0, 6.0, x1, y1, 6.3)

    # the flame vents: a pit 0.9 deep in a raised frame, steel bars over it, a bed of embers at its bottom; the embers'
    # glow, flickering, on the frame, the bars and the floor round it
    for name, (vx, vy) in VENTS.items():
        vent(V["vents"], vx - 2, vy - 2)
        lights.append(("vent_" + name, (vx, vy, 0.5), (1.0, 0.45, 0.12), 2.5, 9.0, 0.6))

    # the hub: 16 x 16, 3 high, a ramp on each side, lamp posts on its corners
    h, ch = V["hub"], C["col_hub"]
    box(h, ch, "wall", -8, -8, 0, 8, 8, 3, top="floor_dark")
    for cx, cy, d in ((0, 13, "s"), (0, -13, "n"), (13, 0, "w"), (-13, 0, "e")):
        ramp(h, ch, "grating", "concrete", cx, cy, 6, 10, 0, 3, d)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = 7 * sx, 7 * sy
            box(h, ch, "wall", x - 0.2, y - 0.2, 3, x + 0.2, y + 0.2, 6.5)
            box(V["lamps"], None, "lamp", x - 0.4, y - 0.4, 6.5, x + 0.4, y + 0.4, 6.9)
            lights.append(("hub_%s%s" % ("n" if sy > 0 else "s", "e" if sx > 0 else "w"), (x, y, 6.2),
                           (1.0, 0.75, 0.4), 1.4, 14.0))

    # the corner towers: 12 x 12, 6 high, a ramp along each wall, a parapet on their inner sides
    t, ct = V["towers"], C["col_towers"]
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(t, ct, "wall", 36 * sx, 36 * sy, 0, H * sx, H * sy, 6, top="floor_dark",
                skip=("+x" if sx > 0 else "-x", "+y" if sy > 0 else "-y"))
            ramp(t, ct, "grating", "concrete", 27 * sx, 45 * sy, 6, 18, 0, 6, "e" if sx > 0 else "w")
            ramp(t, ct, "grating", "concrete", 45 * sx, 27 * sy, 6, 18, 0, 6, "n" if sy > 0 else "s")
            box(t, ct, "concrete", 36 * sx, 36 * sy, 6, 36.5 * sx, 42 * sy, 7, top="hazard")
            box(t, ct, "concrete", 36 * sx, 36 * sy, 6, 42 * sx, 36.5 * sy, 7, top="hazard")

    # cover: L walls (2.2 high) opening toward the centre, low walls (1.2), pillars (5)
    cv, cc = V["covers"], C["col_covers"]
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(cv, cc, "concrete", 13 * sx, 19 * sy, 0, 20 * sx, 20 * sy, 2.2)
            box(cv, cc, "concrete", 19 * sx, 13 * sy, 0, 20 * sx, 19 * sy, 2.2)
            box(cv, cc, "wall", 27.5 * sx, 27.5 * sy, 0, 30.5 * sx, 30.5 * sy, 5, top="concrete")
            box(cv, cc, "concrete", 9 * sx, 26.5 * sy, 0, 15 * sx, 27.5 * sy, 1.2, top="hazard")
            box(cv, cc, "concrete", 26.5 * sx, 9 * sy, 0, 27.5 * sx, 15 * sy, 1.2, top="hazard")

    # the corridor (walls 4 high) and the ship's plaza (walls 3 high), the pad
    s, cs = V["south_area"], C["col_south"]
    for x0, y0, x1, y1, top, skip in ((-7, -62, -5, -50, 4, ()), (5, -62, 7, -50, 4, ()),
                                      (-14, -90, -12, -62, 3, ()), (12, -90, 14, -62, 3, ()),
                                      (-12, -90, 12, -88, 3, ("-x", "+x")), (-12, -64, -5, -62, 3, ("-x",)),
                                      (5, -64, 12, -62, 3, ("+x",))):
        box(s, cs, "wall", x0, y0, FOOT, x1, y1, top, skip=skip)
    box(s, cs, "hazard", -6, -84, 0, 6, -72, 0.2, skip=("top",))
    s.poly("pad", [(-5, -83, 0.2), (5, -83, 0.2), (5, -73, 0.2), (-5, -73, 0.2)], origin=(-5, -83, 0))
    for x0, y0, x1, y1 in ((-6, -84, 6, -83), (-6, -73, 6, -72), (-6, -83, -5, -73), (5, -83, 6, -73)):
        s.poly("hazard", [(x0, y0, 0.2), (x1, y0, 0.2), (x1, y1, 0.2), (x0, y1, 0.2)])
    quad_floor(None, cs, None, -6, -84, 6, -72, z=0.2)
    box(None, cs, None, -6, -84, 0, 6, -72, 0.2, skip=("top",))

    # lamps on the wall's top (cool), over the gates (red: where the enemies come from), over the corridor
    # (green: the way to the ship), in the corridor and on the plaza's walls
    for ang, side in ((0, "n"), (-90, "e"), (90, "w"), (180, "s")):
        for tx in (-24, 0, 24):
            kind = "lamp"
            if tx == 0:
                kind = "lamp_green" if side == "s" else "lamp_red"
            box(V["lamps"], None, kind, tx - 0.6, H, 6.3, tx + 0.6, H + 0.6, 6.8, tf=lambda p, a=ang: rot(p, a))
            pos = rot((tx, H - 0.8, 7.2), ang)
            if kind == "lamp":
                lights.append(("wall_%s_%s" % (side, "a" if tx < 0 else "b"), pos, (0.75, 0.85, 1.0), 1.6, 24.0))
            elif kind == "lamp_red":
                lights.append(("gate_%s" % side, pos, (1.0, 0.4, 0.25), 1.4, 18.0))
            else:
                lights.append(("exit", pos, (0.4, 1.0, 0.55), 1.4, 18.0))
    box(V["lamps"], None, "lamp", -6.6, -56.6, 4.0, -5.4, -55.4, 4.5)
    lights.append(("corridor", (-5.0, -56.0, 5.0), (0.75, 0.85, 1.0), 1.2, 14.0))
    for sx in (-1, 1):
        box(V["lamps"], None, "lamp", 13 * sx - 0.6, -78.6, 3.0, 13 * sx + 0.6, -77.4, 3.5)
        lights.append(("pad_%s" % ("e" if sx > 0 else "w"), (11.6 * sx, -78.0, 4.2), (0.75, 0.85, 1.0), 1.6, 20.0))

    # the floodlight masts in the lava outside the corners
    for sx in (-1, 1):
        for sy in (-1, 1):
            mx, my = 56 * sx, 56 * sy
            top = 36.0
            box(V["masts"], None, "wall", mx - 0.8, my - 0.8, LAVA_Z - 1, mx + 0.8, my + 0.8, top - 3.5)
            ang = math.degrees(math.atan2(-my, -mx))
            tf = lambda p, a=ang, x=mx, y=my: (lambda q: (q[0] + x, q[1] + y, q[2]))(rot(p, a))
            box(V["masts"], None, "wall", -0.6, -3.5, top - 3.5, 0.6, 3.5, top, tf=tf, mats={"+x": "lamp"})
            lights.append(("mast_%s%s" % ("n" if sy > 0 else "s", "e" if sx > 0 else "w"), tf((1.4, 0.0, top - 1.75)),
                           (0.82, 0.9, 1.0), 2.5, 55.0))
    # the lava's glow on the walls' outer faces
    for name, pos in (("lava_ne", (60, 26, 1.0)), ("lava_nw", (-60, 26, 1.0)), ("lava_se", (60, -26, 1.0)),
                      ("lava_sw", (-60, -26, 1.0)), ("lava_pe", (20, -76, 1.0)), ("lava_pw", (-20, -76, 1.0))):
        lights.append((name, pos, (1.0, 0.42, 0.12), 1.6, 18.0))

    # the mountains: a near ring rising out of the lava, a farther, higher one behind it (only their faces turned
    # to the arena: nothing sees their backs)
    mtn = V["mountains"]
    cx0, cy0 = CRATER

    def ring(n, rows, k0):
        pts = []
        for j in range(n):
            th = 2 * math.pi * j / n
            row = []
            for r, zf, kk in rows:
                rr = r + 6.0 * (smooth_noise(th, k0 + kk + 7) - 0.5)
                row.append((cx0 + rr * math.cos(th), cy0 + rr * math.sin(th), zf(th)))
            pts.append(row)
        for j in range(n):
            a, b = pts[j], pts[(j + 1) % n]
            for i in range(len(rows) - 1):
                mid = lerp3(lerp3(a[i], b[i], 0.5), lerp3(a[i + 1], b[i + 1], 0.5), 0.5)
                front = (cx0 - mid[0], cy0 - mid[1], 0.6 * math.hypot(cx0 - mid[0], cy0 - mid[1]))
                mtn.tri_facing("rock", a[i], b[i], b[i + 1], front)
                mtn.tri_facing("rock", a[i], b[i + 1], a[i + 1], front)

    ring(48, [(92, lambda th: LAVA_Z - 1.5, 0),
              (104, lambda th: 4 + 10 * smooth_noise(th, 1), 1),
              (118, lambda th: 20 + 26 * smooth_noise(th, 2), 2)], 0)
    ring(36, [(135, lambda th: 5.0, 0),
              (158, lambda th: 40 + 35 * smooth_noise(th, 3), 1)], 3)
    return V, C, lights


# ---------------------------------------------------------------- markers

def face_centre(x, y):
    return math.degrees(math.atan2(-y, -x))


MARKERS = [
    # (name, position, yaw in degrees: the way the marker's x axis points)
    ("hero", (0, -67, 0.5), 90),
    ("ship", (0, -78, 0.2), 90),
    ("spawn_gate_n", (0, 56, 0), -90),
    ("spawn_gate_e", (56, 0, 0), 180),
    ("spawn_gate_w", (-56, 0, 0), 0),
    ("spawn_drop_n", (0, 34, 0), -90),
    ("spawn_drop_e", (34, 0, 0), 180),
    ("spawn_drop_w", (-34, 0, 0), 0),
    ("spawn_ship_n", (0, 34, 18), -90),
    ("spawn_ship_e", (34, 0, 18), 180),
    ("spawn_ship_w", (-34, 0, 18), 0),
    ("spawn_air_ne", (26, 42, 13), face_centre(26, 42)),
    ("spawn_air_nw", (-26, 42, 13), face_centre(-26, 42)),
    ("spawn_air_e", (44, -20, 13), face_centre(44, -20)),
    ("spawn_air_w", (-44, -20, 13), face_centre(-44, -20)),
    ("spawn_hub", (0, 0, 3), -90),
    ("spawn_info_lava", (60, 40, LAVA_Z), 0),  # (not a spawn: the lava's surface, for the plugin)
]
TRIGGERS = [
    ("trigger_arena", (-H, -H, -1), (H, H, 20)),
    ("trigger_pad", (-12, -88, -1), (12, -64, 8)),
    ("trigger_center", (-8, -8, 2.5), (8, 8, 9)),  # the hub's top: Triangle opens the arena's menu
]
# (the hub's nanotech and ammo crates are the plugin's: four places it fills during the challenges)
OBJS = ([("obj_511", (42 * sx, 42 * sy, 6.5)) for sx in (-1, 1) for sy in (-1, 1)] +
        [("obj_500", p) for p in ((12, -24.5, 0.5), (-12, -24.5, 0.5), (24.5, 12, 0.5), (24.5, -12, 0.5),
                                  (-24.5, 12, 0.5), (-24.5, -12, 0.5))])
FLAMES = [("flame_" + n, (x, y, -0.7)) for n, (x, y) in VENTS.items()]


# ---------------------------------------------------------------- Blender

def walkable(p):
    """Inside the arena's floors (the main square, the alcoves, the corridor, the plaza)."""
    x, y = p[0], p[1]
    e = 0.6
    if abs(x) <= H + e and abs(y) <= H + e:
        return True
    for ang in (0, -90, 90):
        q = rot((x, y, 0), -ang)
        if abs(q[0]) <= 6 + e and H - e <= q[1] <= H + 14 + e:
            return True
    if abs(x) <= 5 + e and -64 - e <= y <= -H + e:
        return True
    return abs(x) <= 12 + e and -88 - e <= y <= -64 + e


GLOW = (1.0, 0.5, 0.3)


def colour(name, p):
    """The vertex colour: darker at the foot of walls and cover, the lava's glow low on the outer faces and the
    mountains' foot (1/16 steps)."""
    if name in ("lamps", "trim", "lava"):
        c = (1.0, 1.0, 1.0)
    elif name == "vents":
        t = max(0.0, min(1.0, -p[2] / 0.9))
        c = lerp3((1.0, 1.0, 1.0), (1.0, 0.55, 0.3), t)
    elif name == "mountains":
        # the lava's glow at the foot; above it each vertex a little lighter or darker (four steps, by a hash of where
        # it is): the faces' shading breaks the texture's repeat
        t = round(max(0.0, min(1.0, (p[2] - LAVA_Z) / 16.0)) * 4) / 4.0
        hsh = math.sin(round(p[0], 1) * 12.9898 + round(p[1], 1) * 78.233) * 43758.5453
        k = 0.62 + 0.13 * int((hsh - math.floor(hsh)) * 4)
        c = lerp3(GLOW, (k, k * 0.97, k * 0.95), t)
    elif not walkable(p) or name == "masts":
        t = max(0.0, min(1.0, (p[2] - LAVA_Z) / 4.0))
        c = lerp3(GLOW, (1.0, 1.0, 1.0), t)
    elif name == "arena_floor":
        d = H - max(abs(p[0]), abs(p[1]))
        k = 0.6 + 0.4 * min(1.0, max(0.0, d) / 3.0) if d >= -0.01 else 0.6
        c = (k, k, k)
    else:
        k = 0.55 + 0.45 * min(1.0, max(0.0, p[2]) / 2.5)
        c = (k, k, k)
    return tuple(round(v * 16) / 16.0 for v in c) + (1.0,)


def collection(name, parent):
    old = bpy.data.collections.get(name)
    if old:
        for ob in list(old.all_objects):
            bpy.data.objects.remove(ob, do_unlink=True)
        bpy.data.collections.remove(old)
    col = bpy.data.collections.new(name)
    parent.children.link(col)
    return col


def material(name):
    tex, kd, tile, glow, line = MATS[name]
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat["rac1_tex"] = tex or ""
    mat["rac1_kd"] = list(kd)
    if line:
        mat["rac1_material"] = line
    elif "rac1_material" in mat:
        del mat["rac1_material"]
    try:
        mat.use_nodes = True  # (always on in newer Blenders)
    except (AttributeError, TypeError):
        pass
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs[0], out.inputs[0])
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    mul = nt.nodes.new("ShaderNodeVectorMath")
    mul.operation = "MULTIPLY"
    nt.links.new(vc.outputs[0], mul.inputs[0])
    if tex:
        img = bpy.data.images.load(os.path.join(TEX, tex), check_existing=True)
        img.reload()
        it = nt.nodes.new("ShaderNodeTexImage")
        it.image = img
        it.interpolation = "Closest"
        nt.links.new(it.outputs[0], mul.inputs[1])
        if glow:
            nt.links.new(it.outputs[0], bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = 1.5
    else:
        mul.inputs[1].default_value = kd
        if glow:
            bsdf.inputs["Emission Color"].default_value = tuple(kd) + (1.0,)
            bsdf.inputs["Emission Strength"].default_value = 4.0
    nt.links.new(mul.outputs[0], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    mat.diffuse_color = tuple(kd) + (1.0,)
    return mat


def mesh_object(name, geo, col, collision):
    verts, faces, uvs, mids = [], [], [], []
    names = []
    for mat, pts, uv in geo.faces:
        if mat not in names:
            names.append(mat)
        faces.append(list(range(len(verts), len(verts) + len(pts))))
        verts += pts
        uvs.append(uv)
        mids.append(names.index(mat))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    if not collision:
        layer = me.uv_layers.new(name="UVMap")
        for poly, uv in zip(me.polygons, uvs):
            poly.material_index = mids[poly.index]
            for li, t in zip(poly.loop_indices, uv):
                layer.data[li].uv = t
        for m in names:
            me.materials.append(material(m))
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bm.to_mesh(me)
    bm.free()
    if not collision:
        ca = me.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="POINT")
        for v in me.vertices:
            ca.data[v.index].color = colour(name, v.co)
    me.update()
    ob = bpy.data.objects.new(name, me)
    col.objects.link(ob)
    if collision:
        ob.display_type = "WIRE"
        ob["rac1_collision_type"] = 3 if name == "col_lava" else 31
    return ob


def empty(col, name, pos, display, size=1.0, scale=None, yaw=0.0):
    ob = bpy.data.objects.new(name, None)
    ob.empty_display_type = display
    ob.empty_display_size = size
    ob.location = pos
    ob.rotation_euler = (0.0, 0.0, math.radians(yaw))
    if scale:
        ob.scale = scale
    col.objects.link(ob)
    return ob


def light(col, name, pos, colour_, intensity, radius, flicker=0.0):
    ld = bpy.data.lights.new(name, type="POINT")
    ld.color = colour_
    ld.energy = intensity * radius * 40.0  # (the viewport's only; the export reads the properties)
    ld.use_custom_distance = True
    ld.cutoff_distance = radius
    ob = bpy.data.objects.new(name, ld)
    ob.location = pos
    ob["intensity"] = intensity
    ob["radius"] = radius
    ob["flicker"] = flicker
    col.objects.link(ob)
    return ob


def main():
    scene = bpy.context.scene
    for n in ("Cube", "Light"):  # the default scene's
        ob = bpy.data.objects.get(n)
        if ob:
            bpy.data.objects.remove(ob, do_unlink=True)
    root = scene.collection
    cvis = collection("arena_visual", root)
    ccol = collection("arena_collision", root)
    cmark = collection("arena_markers", root)
    clig = collection("arena_lights", root)
    V, C, lights = layout()
    for name, g in V.items():
        mesh_object(name, g, cvis, False)
    for name, g in C.items():
        mesh_object(name, g, ccol, True)
    for name, pos, yaw in MARKERS:
        empty(cmark, name, pos, "ARROWS", 2.0 if name in ("hero", "ship") else 1.5, yaw=yaw)
    for name, lo, hi in TRIGGERS:
        c = tuple((a + b) / 2 for a, b in zip(lo, hi))
        s = tuple((b - a) / 2 for a, b in zip(lo, hi))
        empty(cmark, name, c, "CUBE", 1.0, scale=s)
    for name, pos in OBJS:
        empty(cmark, name, pos, "CUBE", 0.5)
    for name, pos in FLAMES:
        empty(cmark, name, pos, "CONE", 1.5)
    for name, pos, colour_, inten, radius, *flicker in lights:
        light(clig, "light_" + name, pos, colour_, inten, radius, flicker[0] if flicker else 0.0)
    # the sun: Veldin's key light's direction, the sun its sky shows (lights.txt's sun line: the base is Kalebo III)
    az, el = (math.radians(a) for a in KEY_LIGHT)
    toward = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
    sd = bpy.data.lights.new("light_sun", type="SUN")
    sd.color = (1.0, 0.85, 0.65)
    sd.energy = 3.0
    sun = bpy.data.objects.new("light_sun", sd)
    sun.location = toward * 120.0
    sun.rotation_euler = toward.to_track_quat("Z", "Y").to_euler()
    clig.objects.link(sun)
    world = scene.world or bpy.data.worlds.new("World")
    scene.world = world
    world.color = (0.25, 0.16, 0.1)
    if world.node_tree:
        bg = next((n for n in world.node_tree.nodes if n.type == "BACKGROUND"), None)
        if bg:
            bg.inputs[0].default_value = (0.25, 0.16, 0.1, 1.0)
            bg.inputs[1].default_value = 1.0
    ccol.hide_viewport = False
    for data in (bpy.data.meshes, bpy.data.lights, bpy.data.materials):
        for d in list(data):
            if d.users == 0:
                data.remove(d)
    print("arena: %d visual meshes (%d faces), %d collision meshes (%d faces), %d markers, %d lights" % (
        len(V), sum(len(g.faces) for g in V.values()), len(C), sum(len(g.faces) for g in C.values()),
        len(MARKERS) + len(TRIGGERS) + len(OBJS) + len(FLAMES), len(lights) + 1))


main()
