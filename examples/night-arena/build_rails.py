"""Builds the arena's grind rails and their station into the open scene (after build_arena.py; run it again freely).

In Blender (the GUI):   p = "<this folder>/build_rails.py"; exec(open(p).read(), {"__file__": p})
Headless (after build_arena.py):   blender -b --python build_arena.py --python build_rails.py ...

  arena_rails      a curve object per rail, rail_<name> (a Bezier: edit its points in Blender). export_arena.py writes
                   its points (rails.txt, the game's coordinates) for the plugin, which hands them to the game's rail
                   table during a rail challenge, and its look (a tube on pylons: the class rail_<name>, shown only then)
  rail_station     the start's platform (arena_visual, arena_collision: col_rail_station); spawn_rail_start (a marker:
                   where a rail challenge puts Ratchet)

Kalebo III's hero code (the base level's, hero_rail_land_check 0x20cf58) takes a rail only within 12 units of two
spots of its own level: (115, 264, 133) and (257, 301, 128) in the game's coordinates, (-61, 76, 29) and (81, 113, 24)
in the scene's (the offset 176 188 104). So a rail starts by the first one; once on it, Ratchet rides it to its end.
"""
import math
import os

import bpy

HERE = os.path.dirname(os.path.abspath(globals().get("__file__") or bpy.data.filepath))
_src = open(os.path.join(HERE, "build_arena.py")).read().rsplit("\nmain()", 1)[0]
B = {"__name__": "build_arena_lib", "__file__": os.path.join(HERE, "build_arena.py")}
exec(_src, B)  # (its helpers: Geo, box, mesh_object, empty, light, collection)

SPOT = (-61.0, 76.0, 29.0)  # Kalebo III's rail spot, the scene's coordinates (the game's (115, 264, 133))
TOP = 24.0                  # the station's floor
PLAT = (-63.0, 72.0, -57.0, 78.0)  # its platform, x0 y0 x1 y1
# the test rail: from the platform's east edge, half a unit over its floor, along the north wall's outside, over the
# wall and down into the arena, ending 8 over the north drop zone (Ratchet flies off its end and lands there)
RAIL_TEST = [(-57.0, 72.5, 24.5), (-46.0, 73.0, 22.5), (-32.0, 71.0, 19.0), (-20.0, 66.0, 15.0),
             (-12.0, 57.0, 12.0), (-7.0, 46.0, 9.5), (-4.0, 37.0, 8.0)]


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


def main():
    scene = bpy.context.scene
    cvis = bpy.data.collections["arena_visual"]
    ccol = bpy.data.collections["arena_collision"]
    cmark = bpy.data.collections["arena_markers"]
    clig = bpy.data.collections["arena_lights"]
    for n in ("rail_station", "col_rail_station", "spawn_rail_start", "light_rail_station"):
        remove(n)
    crail = B["collection"]("arena_rails", scene.collection)

    # the station: a pillar out of the rock, a platform with a low railing on three sides (open toward the rail)
    v, c = B["Geo"](B["VIS_CELL"]), B["Geo"](B["COL_CELL"], uv=False)
    x0, y0, x1, y1 = PLAT
    B["box"](v, c, "wall", -61.5, 73.5, 8.0, -58.5, 76.5, TOP - 0.6, skip=("top",))
    B["box"](v, c, "steel", x0, y0, TOP - 0.6, x1, y1, TOP, top="grating")
    for rx0, ry0, rx1, ry1 in ((x0, y0, x0 + 0.3, y1), (x0, y1 - 0.3, x1, y1), (x0, y0, x1, y0 + 0.3)):
        B["box"](v, c, "steel", rx0, ry0, TOP, rx1, ry1, TOP + 0.8, top="hazard")
    B["box"](v, None, "trim", x1 - 0.15, y0 + 0.3, TOP, x1, y1 - 0.3, TOP + 0.05)  # (a glowing edge: the jump's)
    B["mesh_object"]("rail_station", v, cvis, False)
    B["mesh_object"]("col_rail_station", c, ccol, True)
    B["empty"](cmark, "spawn_rail_start", (-60.0, 75.0, TOP), "SINGLE_ARROW", 1.5, yaw=0.0)
    B["light"](clig, "light_rail_station", (-60.0, 75.0, TOP + 3.0), (0.3, 0.85, 1.0), 1.5, 12.0)

    rail_curve(crail, "rail_test", RAIL_TEST)
    d = min(math.dist(p, SPOT) for p in RAIL_TEST)
    print("rails: rail_test, %d points; its start %.1f from Kalebo III's rail spot (12 at most)" % (len(RAIL_TEST), d))


main()
