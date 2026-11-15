"""Builds the arena's grind rails and their station into the open scene (after build_arena.py; run it again freely).

In Blender (the GUI):   p = "<this folder>/build_rails.py"; exec(open(p).read(), {"__file__": p})
Headless (after build_arena.py):   blender -b --python build_arena.py --python build_rails.py ...

  arena_rails      a curve object per rail, rail_<name> (a Bezier: edit its points in Blender). export_arena.py writes
                   its points (level.txt's rail line: the composer puts it in the game's rail table) and its look (a
                   tube on pylons: the class rail_<name>); the plugin turns a rail on, and shows it, during its challenge
  rail_station     the start's platform (arena_visual, arena_collision: col_rail_station); spawn_rail_course (a
                   marker: where a rail challenge puts Ratchet)
  the course       "Grind Scarponi: Discesa": its centre rail (rail_lane_c, a Bezier) and two lanes 3 units to its sides
                   for its first part (rail_lane_l, rail_lane_r: made from the centre's line, so run this again after
                   editing it); on the lanes, our mines (jump them: markers mine_<n>, the class mine) and Kalebo III's
                   own rail mines (class 933: break them with the wrench or change lane; markers spawn_kmine_<n>, the
                   plugin starts them with base_spawn), two of them closing two lanes at a time

The station is by one of Kalebo III's own rail spots, (-61, 76, 29) in the scene's coordinates ((115, 264, 133) in the
game's: the offset 176 188 104): its hero code (hero_rail_land_check 0x20cf58) took a rail only within 12 units of them
while the plugin made the rails. With level.txt's rail lines (the SDK's ABI 11) it takes one anywhere: the station could
move.
"""
import math
import os

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(globals().get("__file__") or bpy.data.filepath))
_src = open(os.path.join(HERE, "build_arena.py")).read().rsplit("\nmain()", 1)[0]
B = {"__name__": "build_arena_lib", "__file__": os.path.join(HERE, "build_arena.py")}
exec(_src, B)  # (its helpers: Geo, box, mesh_object, empty, light, collection)

SPOT = (-61.0, 76.0, 29.0)  # Kalebo III's rail spot, the scene's coordinates (the game's (115, 264, 133))
TOP = 24.0                  # the station's floor
PLAT = (-63.0, 72.0, -57.0, 85.0)  # its platform, x0 y0 x1 y1 (the course's lanes from its north part)
# the course's centre rail: east along the north wall's outside, round the north-east corner, south along the east
# wall's outside, over it and down into the arena, ending 8 over the east drop zone
COURSE = [(-56.5, 80.0, 24.5), (-40.0, 79.0, 22.0), (-15.0, 77.0, 19.0), (10.0, 77.0, 17.0), (35.0, 76.0, 15.5),
          (55.0, 72.0, 14.5), (66.0, 60.0, 13.5), (69.0, 45.0, 12.5), (69.0, 20.0, 11.5), (67.0, 5.0, 11.0),
          (58.0, -2.0, 10.0), (44.0, -2.0, 8.5), (37.0, 0.0, 8.0)]
LANE = 3.0                  # the lanes' distance from the centre (a side jump finds a rail 2..4.5 away)
LANE_END = {"l": 125.0, "r": 150.0}  # where each side lane ends, along the centre (units from its start)
# (along the centre, the lane: l, c, r, the kind: mine ours, to jump; kmine Kalebo III's, to break or pass on another
# lane): two of them make a gate (one lane free)
OBSTACLES = [(28, "c", "mine"),
             (40, "l", "kmine"), (40, "c", "kmine"), (52, "r", "kmine"),       # the right lane free
             (64, "r", "kmine"), (64, "l", "kmine"), (74, "c", "kmine"),       # the centre free
             (88, "c", "kmine"), (88, "r", "kmine"), (98, "l", "kmine"),       # the left free
             (110, "l", "mine"),                                               # (the left lane ends at 125)
             (135, "r", "kmine"),                                              # (the right one at 150)
             (162, "c", "kmine"), (176, "c", "mine"), (190, "c", "kmine"), (204, "c", "mine"), (214, "c", "kmine")]
# the escape ("La grande fuga": the lava floods the arena): from the hub's top (3 high), west over the arena's wall, south
# along its outside over the lava, round the south-west corner and up through the portal (the marker portal_fuga, on it
# PORTAL_BACK before its end: Ratchet goes through it grinding, the section's own rail goes on beyond), west of the ship's
# plaza
ESCAPE = [(5.0, 0.0, 3.6), (-4.0, 0.0, 3.8), (-14.0, -1.0, 5.5), (-26.0, -4.0, 8.5), (-38.0, -8.0, 11.0),
          (-50.0, -12.0, 12.5), (-62.0, -16.0, 12.0), (-72.0, -26.0, 11.0), (-76.0, -40.0, 10.0), (-72.0, -54.0, 9.5),
          (-62.0, -66.0, 9.5), (-48.0, -72.0, 10.5), (-34.0, -74.0, 12.5), (-24.0, -73.5, 13.5)]
ESCAPE_OBSTACLES = [(30, "mine"), (44, "kmine"), (58, "kmine"), (72, "mine"), (86, "kmine"), (100, "kmine"),
                    (114, "mine"), (128, "kmine"), (142, "kmine")]  # (along it; mine_e<n>, spawn_kmine_e<n>)
PORTAL_BACK = 6.0  # the portal's ring on the escape, this far before its end (the plugin's switch as he goes through)
# the way back (rail_ritorno): from a portal's ring high outside the north wall (clear of the corners' floodlight masts and
# of the course's lanes), over the wall and down into the arena's middle, its end over the hub's top (Ratchet flies off it
# there); a portal from any world brings him onto it, grinding (the plugin's hero_rail_attach at main's entry rientro, its start)
RITORNO = [(-16.0, 66.0, 20.0), (-12.0, 56.0, 17.5), (-8.5, 46.0, 14.5), (-6.0, 34.0, 11.0), (-4.0, 22.0, 8.5), (-2.0, 12.0, 6.5),
           (-1.0, 4.0, 5.2)]


def remove(name):
    ob = bpy.data.objects.get(name)
    if ob:
        bpy.data.objects.remove(ob, do_unlink=True)


def rail_curve(col, name, pts):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.resolution_u = 16
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(pts) - 1)
    for bp, p in zip(sp.bezier_points, pts):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    cu.bevel_depth = 0.12  # (the viewport's look; the export makes its own tube)
    cu.bevel_resolution = 2
    ob = bpy.data.objects.new(name, cu)
    col.objects.link(ob)
    return ob


def bezier_line(pts, n=33):
    """The centre's line as the curve makes it (auto handles), dense: its points, their distances along it."""
    cu = bpy.data.curves.new("_tmp", "CURVE")
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(pts) - 1)
    for bp, p in zip(sp.bezier_points, pts):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    from mathutils.geometry import interpolate_bezier
    bp = sp.bezier_points
    line = []
    for i in range(len(bp) - 1):
        seg = interpolate_bezier(bp[i].co, bp[i].handle_right, bp[i + 1].handle_left, bp[i + 1].co, n)
        line += [p.copy() for p in (seg if not line else seg[1:])]
    bpy.data.curves.remove(cu)
    dist = [0.0]
    for i in range(1, len(line)):
        dist.append(dist[-1] + (line[i] - line[i - 1]).length)
    return line, dist


def along(line, dist, s):
    """The point at s along the line, and its horizontal side (to the right of the way it goes)."""
    i = max(0, min(len(line) - 2, next((k for k in range(len(dist) - 1) if dist[k + 1] >= s), len(dist) - 2)))
    t = (s - dist[i]) / max(1e-6, dist[i + 1] - dist[i])
    p = line[i].lerp(line[i + 1], max(0.0, min(1.0, t)))
    d = (line[i + 1] - line[i])
    side = d.cross(Vector((0, 0, 1)))
    side.z = 0.0
    return p, side.normalized()


def poly_curve(col, name, pts):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    sp = cu.splines.new("POLY")
    sp.points.add(len(pts) - 1)
    for q, p in zip(sp.points, pts):
        q.co = (p[0], p[1], p[2], 1.0)
    cu.bevel_depth = 0.12
    cu.bevel_resolution = 2
    ob = bpy.data.objects.new(name, cu)
    col.objects.link(ob)
    return ob


def course(crail, cmark):
    """The course: its centre rail, the side lanes, the obstacles' markers."""
    c = rail_curve(crail, "rail_lane_c", COURSE)
    c["beam"] = LANE + 0.45      # its pylons' cross bars under all three lanes...
    c["beam_until"] = LANE_END["r"]  # ...as far as the lanes go
    line, dist = bezier_line(COURSE)
    for k, sign in (("l", -1.0), ("r", 1.0)):
        pts, s = [], 0.0
        while s <= LANE_END[k]:
            p, side = along(line, dist, s)
            pts.append(p + side * (sign * LANE))
            s += 1.5
        ob = poly_curve(crail, "rail_lane_" + k, pts)
        ob["pylons"] = 0  # (the centre's cross bars hold it)
        ob["end_stop"] = 1
    count = {}
    for s, lane, kind in OBSTACLES:
        p, side = along(line, dist, s)
        p = p + side * ({"l": -LANE, "c": 0.0, "r": LANE}[lane])
        d = along(line, dist, s + 1.0)[0] - along(line, dist, s - 1.0)[0]
        count[kind] = count.get(kind, 0) + 1
        name = ("spawn_kmine_%d" if kind == "kmine" else "mine_%d") % count[kind]
        B["empty"](cmark, name, tuple(p), "SPHERE", 0.6, yaw=math.degrees(math.atan2(d.y, d.x)))
    return dist[-1]


def escape(crail, cmark):
    """The escape's rail, its obstacles' markers (mine_e<n>, spawn_kmine_e<n>: the plugin keeps them apart from the
    course's) and the portal's marker past its end."""
    rail_curve(crail, "rail_escape", ESCAPE)
    line, dist = bezier_line(ESCAPE)
    count = {}
    for s, kind in ESCAPE_OBSTACLES:
        p = along(line, dist, s)[0]
        d = along(line, dist, s + 1.0)[0] - along(line, dist, s - 1.0)[0]
        count[kind] = count.get(kind, 0) + 1
        name = ("spawn_kmine_e%d" if kind == "kmine" else "mine_e%d") % count[kind]
        B["empty"](cmark, name, tuple(p), "SPHERE", 0.6, yaw=math.degrees(math.atan2(d.y, d.x)))
    p = along(line, dist, dist[-1] - PORTAL_BACK)[0]
    d = along(line, dist, dist[-1] - PORTAL_BACK + 1.0)[0] - along(line, dist, dist[-1] - PORTAL_BACK - 1.0)[0]
    B["empty"](cmark, "portal_fuga", (p.x, p.y, p.z + 0.9), "CIRCLE", 3.0, yaw=math.degrees(math.atan2(d.y, d.x)))
    return dist[-1]


def main():
    scene = bpy.context.scene
    cvis = bpy.data.collections["arena_visual"]
    ccol = bpy.data.collections["arena_collision"]
    cmark = bpy.data.collections["arena_markers"]
    clig = bpy.data.collections["arena_lights"]
    for n in ("rail_station", "col_rail_station", "spawn_rail_start", "spawn_rail_course", "light_rail_station", "rail_test",
              "rail_ritorno"):
        remove(n)
    for ob in list(cmark.objects):
        if ob.name.startswith(("mine_", "barrier_", "drone_", "spawn_kmine_", "portal_")):
            bpy.data.objects.remove(ob, do_unlink=True)
    crail = B["collection"]("arena_rails", scene.collection)

    # the station: a pillar out of the rock, a platform with a low railing on three sides (open toward the rail)
    v, c = B["Geo"](B["VIS_CELL"]), B["Geo"](B["COL_CELL"], uv=False)
    x0, y0, x1, y1 = PLAT
    B["box"](v, c, "wall", -61.5, 73.5, 8.0, -58.5, 76.5, TOP - 0.6, skip=("top",))
    B["box"](v, c, "wall", -61.5, 80.0, 8.0, -58.5, 83.0, TOP - 0.6, skip=("top",))
    B["box"](v, c, "steel", x0, y0, TOP - 0.6, x1, y1, TOP, top="grating")
    for rx0, ry0, rx1, ry1 in ((x0, y0, x0 + 0.3, y1), (x0, y1 - 0.3, x1, y1), (x0, y0, x1, y0 + 0.3)):
        B["box"](v, c, "steel", rx0, ry0, TOP, rx1, ry1, TOP + 0.8, top="hazard")
    B["box"](v, None, "trim", x1 - 0.15, y0 + 0.3, TOP, x1, y1 - 0.3, TOP + 0.05)  # (a glowing edge: the jump's)
    B["mesh_object"]("rail_station", v, cvis, False)
    B["mesh_object"]("col_rail_station", c, ccol, True)
    B["empty"](cmark, "spawn_rail_course", (-60.0, 80.0, TOP), "SINGLE_ARROW", 1.5, yaw=0.0)
    B["light"](clig, "light_rail_station", (-60.0, 78.5, TOP + 3.0), (0.3, 0.85, 1.0), 1.5, 14.0)

    total = course(crail, cmark)
    starts = [COURSE[0]] + [(COURSE[0][0], COURSE[0][1] + k * LANE, COURSE[0][2]) for k in (-1, 1)]
    print("rails: the course, %.0f units; its lanes' starts %.1f..%.1f from the spot; %d obstacles" % (
        total, min(math.dist(p, SPOT) for p in starts), max(math.dist(p, SPOT) for p in starts), len(OBSTACLES)))
    print("rails: the escape, %.0f units, %d obstacles, the portal %s before its end" % (escape(crail, cmark), len(ESCAPE_OBSTACLES), PORTAL_BACK))
    rail_curve(crail, "rail_ritorno", RITORNO)
    d0 = Vector(RITORNO[1]) - Vector(RITORNO[0])
    B["empty"](cmark, "portal_ritorno", (RITORNO[0][0], RITORNO[0][1], RITORNO[0][2] + 0.9), "CIRCLE", 3.0,
               yaw=math.degrees(math.atan2(d0.y, d0.x)))
    print("rails: the way back, %d points" % len(RITORNO))


main()
