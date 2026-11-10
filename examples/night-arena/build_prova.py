"""Builds the portal's test place into the open scene (after build_arena.py; run it again freely): "prova", a section of
the session far east of the arena, where "La grande fuga" ends after its portal. A round platform of grating over the void
(another planet's sky round it: export_arena.py's SECTIONS), a glowing edge, four lamp posts, the entry Ratchet comes in by.

In Blender (the GUI):   p = "<this folder>/build_prova.py"; exec(open(p).read(), {"__file__": p})
Headless (after build_arena.py):   blender -b --python build_arena.py --python build_prova.py ...

  prova_visual     its meshes (export_arena.py: prova_meshes.obj, drawn only while Ratchet is there)
  prova_collision  its collision (prova_collision.obj)
  prova_markers    entry_<name> (an entry: where a switch puts Ratchet, facing its x axis), checkpoint_<name> (where a death
                   there brings him back)
  prova_rails      rail_<name> curves (level.txt's rail lines in the section's block): rail_prova_in, the escape's rail going on
                   beyond the portal, from high in the void down to the platform's south edge (the plugin puts Ratchet on it)
  arena_lights     light_prova_<n> (the lights file is the level's: these light only the platform)
"""
import math
import os

import bpy

HERE = os.path.dirname(os.path.abspath(globals().get("__file__") or bpy.data.filepath))
_src = open(os.path.join(HERE, "build_arena.py")).read().rsplit("\nmain()", 1)[0]
B = {"__name__": "build_arena_lib", "__file__": os.path.join(HERE, "build_arena.py")}
exec(_src, B)  # (its helpers: Geo, box, mesh_object, empty, light, collection)

CENTRE = (600.0, 0.0)  # the platform's centre (the scene's; a section has its own collision, so far is simplest)
R = 20.0               # its radius (an octagon's corners)
DEPTH = 3.0            # its edge's height under the top (z 0)
SIDES = 8
# the rail Ratchet arrives on: from high south of the platform (the entry arrivo at its start) down to its south edge, where
# he flies off it onto the platform
RAIL_IN = [(600.0, -62.0, 16.0), (600.0, -48.0, 13.0), (600.0, -34.0, 8.0), (600.0, -25.0, 3.6), (600.0, -19.5, 1.7)]


def corners(r, z):
    cx, cy = CENTRE
    return [(cx + r * math.cos(math.radians(22.5 + 45 * k)), cy + r * math.sin(math.radians(22.5 + 45 * k)), z)
            for k in range(SIDES)]


def main():
    scene = bpy.context.scene
    cvis = B["collection"]("prova_visual", scene.collection)
    ccol = B["collection"]("prova_collision", scene.collection)
    cmark = B["collection"]("prova_markers", scene.collection)
    crail = B["collection"]("prova_rails", scene.collection)
    clig = bpy.data.collections["arena_lights"]
    for ob in list(clig.objects):
        if ob.name.startswith("light_prova_"):
            bpy.data.objects.remove(ob, do_unlink=True)
    v, c = B["Geo"](B["VIS_CELL"]), B["Geo"](B["COL_CELL"], uv=False)
    cx, cy = CENTRE
    top, inner, low = corners(R, 0.0), corners(R - 1.2, 0.0), corners(R - 2.0, -DEPTH)
    mid = (cx, cy, 0.0)
    for k in range(SIDES):
        a, b = k, (k + 1) % SIDES
        v.poly("grating", [mid, inner[a], inner[b]], origin=(cx, cy, 0))  # (the top: a fan of grating...)
        c.poly(None, [mid, top[a], top[b]])
        v.poly("floor_dark", [inner[a], top[a], top[b], inner[b]], origin=(cx, cy, 0))  # (...a dark band round it)
        v.poly("steel", [top[b], top[a], low[a], low[b]], origin=(cx, cy, 0))  # (its side, slanting in)
        c.poly(None, [top[b], top[a], low[a], low[b]])
        e0, e1 = corners(R - 0.15, 0.05), corners(R, 0.05)
        v.poly("trim", [e0[a], e1[a], e1[b], e0[b]], origin=(cx, cy, 0))  # (the glowing edge)
    B["box"](v, c, "tech", cx - 3, cy - 3, 0, cx + 3, cy + 3, 0.15, mats=None)  # (a pad in the middle)
    for k in range(4):  # lamp posts on four corners, a light each
        x, y, _ = corners(R - 2.2, 0.0)[2 * k]
        B["box"](v, c, "wall", x - 0.25, y - 0.25, 0, x + 0.25, y + 0.25, 4.0)
        B["box"](v, None, "lamp", x - 0.45, y - 0.45, 4.0, x + 0.45, y + 0.45, 4.4)
        B["light"](clig, "light_prova_%d" % k, (x, y, 3.8), (0.55, 0.75, 1.0), 1.6, 16.0)
    B["mesh_object"]("prova_platform", v, cvis, False)
    B["mesh_object"]("col_prova_platform", c, ccol, True)
    B["empty"](cmark, "entry_arrivo", RAIL_IN[0], "SINGLE_ARROW", 1.5, yaw=90.0)
    B["empty"](cmark, "checkpoint_prova_piano", (cx, cy - 8.0, 0.5), "SINGLE_ARROW", 1.5, yaw=90.0)
    cu = bpy.data.curves.new("rail_prova_in", "CURVE")
    cu.dimensions = "3D"
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(RAIL_IN) - 1)
    for bp, p in zip(sp.bezier_points, RAIL_IN):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    cu.bevel_depth = 0.12
    crail.objects.link(bpy.data.objects.new("rail_prova_in", cu))
    print("prova: a platform of radius %s at (%s, %s), its entry arrivo on the rail prova_in, the checkpoint prova_piano" % (R, cx, cy))


main()
