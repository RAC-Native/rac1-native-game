"""Exports the arena from Blender to a level mod folder (docs/mods.md, "Level mods", format 2).

In Blender (the GUI):   p = "<this folder>/export_arena.py"; g = {"__file__": p}; exec(open(p).read(), g)
                        g["export"](None, True)   (True: also copied into the game's mods/)
Headless:               blender -b arena.blend --python export_arena.py -- [<out folder>] [--install] [--bundle]
                        (--bundle: also writes bundled-mods/night-arena, the copy the game ships: docs/mods.md)
The game's checkout (its mods/): local.json's "game" beside this script when there is one
({"game": "/path/to/rac1-native-game"}), else the repository this script is in (examples/night-arena/../..).

It reads four collections (build_arena.py makes them; edit them freely in Blender):

  arena_visual     each mesh object -> an "o" of meshes.obj (its materials, UVs, the "Col" colour attribute); a
                   material's "rac1_material" property -> its line in lights.txt (metal, lava, emissive, ...)
  arena_collision  every mesh object -> collision.obj, all triangles ("rac1_collision_type": the surface, 31 plain,
                   1 the deadly liquid: the lava)
  arena_markers    empties by name: hero, ship (their x axis is the way they face), spawn_<kind>_<name>,
                   trigger_<name> (a cube empty: its box), obj_<class> (a moby of the base level's class),
                   flame_<name> (a flame vent: the plugin's class "flame"; a "delay" property in frames)
  arena_lights     light_sun (a sun lamp: its direction is lights.txt's sun line) and point lamps light_<name>
                   (their colour; "intensity", "radius", "flicker" properties)
  arena_rails      (build_rails.py; optional) curves rail_<name>: level.txt's rail lines (their points every RAIL_STEP
                   units; the composer puts a collision strip under each) and a class rail_<name> each (a tube on
                   pylons, one object)
  prova_*          (build_prova.py; optional) a section of the session (SECTIONS): <name>_visual, <name>_collision
                   (<name>_meshes.obj, <name>_collision.obj) and <name>_markers (entry_<n>)

and writes <out>/mod.json, <out>/plugin/ (the flame vents' code), <out>/level/{level.txt, collision.obj,
meshes.obj, meshes.mtl, *.png, lights.txt, the classes' models, the sounds, the music}. The level is made round
(0, 0, 0): the offset is written out (lights.txt's coordinates are the game's: they get it added here): x and y as
"offset auto" would (the lowest corner, the meshes' too, 16 units above 0, in steps of 4), z putting the floor at
LEVEL["floor_z"] (Kalebo III's hero dies under z 77).
"""
import json
import math
import os
import shutil
import struct
import sys
import wave

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(globals().get("__file__") or bpy.data.filepath))


def game_checkout():
    try:
        with open(os.path.join(HERE, "local.json")) as f:
            return json.load(f)["game"]
    except (OSError, ValueError, KeyError):
        here = os.path.dirname(os.path.realpath(globals().get("__file__") or os.path.join(HERE, "export_arena.py")))
        return os.path.dirname(os.path.dirname(here))


GAME = game_checkout()
MODS = os.path.join(GAME, "mods")
FOLDER = "night-arena"

LEVEL = {
    "id": "night-arena",
    "name": {"": "Daino's Arena", "it": "Arena di Daino", "de": "Dainos Arena", "es": "Arena de Daino",
             "fr": "Arène de Daino"},
    "region": {"": "Custom Levels", "it": "Livelli personalizzati", "de": "Eigene Level", "es": "Niveles propios",
               "fr": "Niveaux personnalisés"},
    "format": 3,            # a session (docs/mods.md): loaded once, a death a soft reset at a checkpoint, sections
    "base": 16,             # Kalebo III: its enemies (classes 1445, 52, 541, 638, 1356) for the waves
    "base_name": "kalebo",
    "hero_build": "generic",  # Ratchet on the generic hero code (Veldin's, Gaspar's, Umbris's): the game's own lava
    "floor_z": 104.0,       # the floor's height in the game: Kalebo III's hero code ends a fall under z 77
    "death_height": -6.0,   # (the scene's: under the lava)
    # the session's checkpoints (the scene's coordinates, yaw): "arena" south of the hub, where a death during a challenge
    # brings Ratchet back (the plugin's choice; outside a challenge the session's start, the landing)
    "checkpoints": [("arena", (0.0, -20.0, 0.5), 1.5708)],
    # main's entries (where a switch from a section puts Ratchet back): "ritorno" south of the hub
    "entries": [("ritorno", (0.0, -20.0, 0.5), 1.5708), ("rientro", (-16.0, 66.0, 20.6), -1.1903)],  # (rientro: rail_ritorno's start)
    "sky": 0,               # Veldin's sky (Quartu's, 15, crashed on Veldin's code: sky_hand::spheres)
    "music": ("overworld.ogg", os.path.join(os.path.dirname(os.path.realpath(globals().get("__file__") or bpy.data.filepath)),
                                            "music", "overworld.ogg")),
    # the mod's other tracks (level.txt's music_track lines: the plugin's music_track, ABI 19): the challenges' music
    "music_tracks": [("fight", "arena_fight.ogg", os.path.join(os.path.dirname(os.path.realpath(globals().get("__file__") or bpy.data.filepath)),
                                                                "music", "arena_fight.ogg"))],
    "background": (120, 80, 58),  # a warm haze: the lava, Veldin's low sun
    "fog": (120, 80, 58),
    # room for the base level's objects the plugin starts (base_spawn): 24 robots, 8 bomb bots, 6 Gadgetron robots,
    # 8 jetpack troopers (the dropships' too), 2 dropships (SDK ABI 5), the hub's 4 crates of each kind, the grind
    # course's 13 rail mines and the escape's 6 (933: Kalebo III's, on its rails)
    "spawn": {1445: 24, 52: 8, 541: 6, 638: 8, 1356: 2, 501: 4, 511: 4, 933: 22, 439: 1},  # (439: Kalebo III's hoverboard)
    # other planets' classes in main from the load (level.txt's import lines; the challenges' enemies are loaded per wave)
    "imports": [],
    # (text, box in the scene's coordinates or None): embers rising from the lava (on a level mod `source lava` reads
    # the collision's type 3 faces too), as on Gaspar, and a little ash
    "particles": [
        ("embers 3  source lava  rise 1.3", None),
        ("embers 1.5  1 0.714 0.247  source lava", None),
        ("ash 0.8  0.25 0.25 0.25", None),
    ],
    "sounds": [
        "flame_roar flame_roar.wav loop volume 0.7 range 3 30",
        "flame_burst flame_burst.wav volume 0.9 range 3 40",
        "flame_hiss flame_hiss.wav volume 0.6 range 2 25",
        "wave_start wave_start.wav volume 0.8",
        "challenge_win challenge_win.wav volume 0.8",
        "challenge_fail challenge_fail.wav volume 0.8",
        "bomb_whistle bomb_whistle.wav volume 0.7 range 4 50",
        "bomb_boom bomb_boom.wav volume 1 range 6 70",
        "zap_crackle zap_crackle.wav volume 0.7 range 3 35",
        "zap_buzz zap_buzz.wav loop volume 0.6 range 3 35",
    ],
}
MAX_REPEAT = 8.0         # a texture across a triangle, at most (the format's)
MAX_MATERIALS = 16       # a mesh's
MAX_COLOURS = 64         # a mesh's vertex colours (more are made coarser by the composer)
CLASSES = [              # the level's own classes: (name, model, its line's options, its model's textures' material lines)
    ("spawn", "spawn.obj", "size 0.5 ghost scale 0.01", []),
    ("flame", "flame.obj", "size 1.5 ghost", ["emissive 3", "emissive 3.5"]),
    ("bomb", "bomb.obj", "size 1 ghost", ["metal 0.35 0.8 glow 2 0.6"]),  # (the plugin's bombardment)
    ("mark", "mark.obj", "size 1 ghost", ["emissive 2.5"]),               # (where a bomb falls)
    ("mine", "mine.obj", "size 1 ghost", ["metal 0.35 0.8 glow 2 0.6"]),  # (the course's: on a rail, jumped)
    ("zap", "zap.obj", "size 1 ghost", ["emissive 3"]),                  # (a drop zone's ring electrified: Grate impazzite)
    ("portal", "portal.obj", "size 1 ghost", ["emissive 3"]),            # (the escape's end: into the section prova)
    ("flood", "flood.obj", "size 1 ghost", ["emissive 2.2"]),            # (the lava flooding the arena: La grande fuga)
]
# the session's sections (build_prova.py's collections <name>_visual, <name>_collision, <name>_markers: entry_<n>), each
# with another planet's sky round it
SECTIONS = [("prova", {"sky": 15, "background": (34, 44, 92), "fog": (34, 44, 92), "death_height": -12.0}),
            # game planets' worlds as sections (no collections: their scenery, collision, sky and music are the planets',
            # their coordinates too; only the objects an "objects" line names): the worlds "La grande fuga" goes through
            # after the escape's portal, each reached through a flash of white (the plugin's: its CHAIN)
            # - Quartu: our rail quartu_in (its model drawn: composer 7) from high south of its Giant Clank's pad down by it,
            #   then Kalebo III's hoverboard (the plugin's, ABI 16) east across the Giant Clank's round arena (every zone
            #   drawn: zones all) to the ring in its middle; none of Quartu's objects (its 1451, the pad, shows a broken help text)
            ("quartu", {"world": "quartu", "fresh": True, "zones_all": True, "entries": [("arrivo", (130.5, 186.0, 61.0), 1.5708)],
                        "rails": [("quartu_in", [(130.5, 186.0, 60.0), (130.5, 200.0, 48.0), (130.5, 210.0, 36.0),
                                                 (130.5, 216.0, 28.0)])],
                        "rail_mesh": True, "death_height": 8.0}),
            # - Batalia: its long rail (its own, drawn), from its start to its end, where it leaps: Ratchet comes in just over
            #   its start and lands on it (no rail of ours on top: the section keeps the planet's own collision; with one, the
            #   composer's copy of it with the rail's strip, the camera shook on the way down)
            #   With its spiked rail mines (621: jumped, or broken with the wrench, as on Batalia).
            # (every world `fresh`: put back as at its first entry at every entry, its mines, its boss: a challenge played again)
            ("batalia", {"world": "batalia", "fresh": True, "objects": "621", "entries": [("arrivo", (286.0, 86.9, 33.0), 2.91)],
                         "death_height": 15.0}),
            # - Rilgar: its hoverboard race (its own hero code and board: hero rilgar board; its HUD) from its finish line, to
            #   the small platform past the big ramp (the plugin's ring there), with its turbo pads (1140). Not its crates (500:
            #   346 of them, along the whole track): every section's objects take the game's moby array from the session's
            #   start, and with them it was full (Umbris's objects not all placed, the vendor crashed)
            ("rilgar", {"world": "rilgar", "fresh": True, "hero": "rilgar board", "objects": "1140",
                        "entries": [("arrivo", (275.49, 334.35, 73.61), -2.04)], "death_height": 30.0}),
            # - Kalebo III (Assalto Finale's): its own long rail, from its station by the arena's (Ratchet comes in just over it)
            #   to its leap, its rail mines the plugin's; Kalebo III is the session's base: none of its objects (main's spawn
            #   pools serve the mines)
            #   (hero kalebo: its own hero code, which rides its rail to the end; the generic code stopped at its point 2099)
            ("kalebo", {"world": "kalebo", "fresh": True, "hero": "kalebo", "entries": [("arrivo", (111.6, 264.04, 134.6), 3.1416)],
                        "death_height": 90.0}),
            # - Umbris: the Snagglebeast's cave, its boss (the challenge won when it is beaten). Its objects by class (all of
            #   them would place code-alone triggers too), 460 its lava's sea (code alone, no model); not 529, the parked ship
            ("umbris", {"world": "umbris", "fresh": True, "objects": "187 341 447 460 500 501 505 511 605 805 886 902 968 1059 1063 1069 1106 "
                                                      "1113 1114 1115 1116 1128 1129 1131 1133 1135",
                        "box": (195, 355, 25, 320, 500, 90), "boss": "umbris.snagglebeast",
                        "entries": [("arrivo", (263.35, 483.16, 36.5), -1.13)],
                        "death_height": 20.0})]

errors, warnings = [], []


def err(msg):
    errors.append(msg)


def warn(msg):
    warnings.append(msg)


def num(v):
    s = "%.3f" % v
    s = s.rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def coll(name):
    c = bpy.data.collections.get(name)
    if not c:
        err("no collection %s (run build_arena.py)" % name)
        return []
    return list(c.all_objects)


def base_name(ob):
    """An object's name without Blender's .001 suffix."""
    n = ob.name
    if "." in n and n.rsplit(".", 1)[1].isdigit():
        n = n.rsplit(".", 1)[0]
    return n


def yaw(ob):
    x = ob.matrix_world.to_3x3() @ Vector((1, 0, 0))
    return math.atan2(x.y, x.x)


def mesh_of(ob, depsgraph):
    """(world positions, mesh, evaluated object) of an object, its modifiers applied."""
    ev = ob.evaluated_get(depsgraph)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    mw = ob.matrix_world
    return [mw @ v.co for v in me.vertices], me, ev


def lowest(lo, pts):
    for p in pts:
        for i in range(3):
            lo[i] = min(lo[i], p[i])


# ---------------------------------------------------------------- collision

def export_collision(path, depsgraph, collection="arena_collision"):
    verts, faces = [], []
    walk = total = 0
    lo = [1e9] * 3
    for ob in sorted(coll(collection), key=lambda o: o.name):
        if ob.type != "MESH":
            continue
        pos, me, ev = mesh_of(ob, depsgraph)
        base = len(verts)
        verts += pos
        kind = int(ob.get("rac1_collision_type", 31))
        faces.append((ob.name, kind, [[base + me.loops[li].vertex_index + 1 for li in t.loops] for t in me.loop_triangles]))
        for t in me.loop_triangles:
            a, b, c = (pos[me.loops[li].vertex_index] for li in t.loops)
            n = (b - a).cross(c - a)
            if n.length > 1e-9 and n.normalized().z > 0.7:
                walk += 1
            total += 1
        ev.to_mesh_clear()
    if collection == "arena_collision" and bpy.data.collections.get("arena_rails"):  # (the rails' strips, past their objects')
        for ob in sorted((o for o in bpy.data.collections["arena_rails"].all_objects if o.type == "CURVE"), key=lambda o: o.name):
            pts = strip_split(base_name(ob), rail_points(ob))[1]
            if len(pts) < 2:
                continue
            sv, st = strip_quads(pts)
            base = len(verts)
            verts += sv
            faces.append(("col_" + base_name(ob), 31, [[base + j + 1 for j in tri] for tri in st]))
            total += len(st)
            walk += len(st)
    lowest(lo, verts)
    if not verts:
        err("%s: no triangles" % collection)
    with open(path, "w") as f:
        f.write("# the arena: collision (export_arena.py), z up, game units, made round (0, 0, 0)\n")
        for p in verts:
            f.write("v %s %s %s\n" % (num(p.x), num(p.y), num(p.z)))
        for name, kind, tl in faces:
            f.write("o %s\nusemtl type%d\n" % (name, kind))
            for t in tl:
                f.write("f %d %d %d\n" % tuple(t))
    print("%s: %d triangles (%d walkable, normal up), lowest corner (%s %s %s)" % (
        collection, total, walk, num(lo[0]), num(lo[1]), num(lo[2])))
    return lo


# ---------------------------------------------------------------- the meshes

def mat_info(mat):
    """(texture file or None, Kd) of a material: build_arena.py's properties, else its nodes."""
    tex = mat.get("rac1_tex") if mat else None
    kd = tuple(mat.get("rac1_kd", (1, 1, 1))) if mat else (0.8, 0.8, 0.8)
    if mat and tex is None and mat.node_tree:
        img = next((n.image for n in mat.node_tree.nodes if n.type == "TEX_IMAGE" and n.image), None)
        if img:
            tex = bpy.path.abspath(img.filepath)
        else:
            bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if bsdf:
                kd = tuple(bsdf.inputs["Base Color"].default_value[:3])
    if tex and not os.path.isabs(tex):
        tex = os.path.join(HERE, "textures", tex)
    return (tex or None), kd


def png_check(path):
    with open(path, "rb") as f:
        head = f.read(33)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        err("%s: not a PNG" % path)
        return
    w, h, depth, ctype = struct.unpack(">IIBB", head[16:26])
    for d in (w, h):
        if d > 256 or d & (d - 1):
            err("%s: %dx%d (powers of two, 256 at most)" % (os.path.basename(path), w, h))
            break
    if ctype != 3:
        warn("%s: not palettised (the composer may refuse more than 256 colours)" % os.path.basename(path))


def is_lava(ob):
    return any(s.material and str(s.material.get("rac1_material", "")).startswith("lava") for s in ob.material_slots)


def export_meshes(obj_path, mtl_path, depsgraph, collection="arena_visual", first=0):
    """meshes.obj / .mtl; returns (the PNGs, the lowest corner, the material lines, the meshes' count). The objects with
    lava go first; the material lines name a host mesh class (its index among the written objects) and a texture of
    its own (its index among the class's materials in the .mtl's order), as the composer numbers them."""
    obs = [o for o in coll(collection) if o.type == "MESH"]
    obs.sort(key=lambda o: (not is_lava(o), o.name))
    data = []
    used = {}
    lo = [1e9] * 3
    for ob in obs:
        pos, me, ev = mesh_of(ob, depsgraph)
        if not me.loop_triangles:
            ev.to_mesh_clear()
            continue
        mats = [s.material for s in ob.material_slots] or [None]
        for m in mats:
            used[m.name if m else "default"] = m
        data.append((ob, pos, me, ev, mats))
        lowest(lo, pos)
    order = sorted(used)  # the .mtl's order: the composer's material index
    out = open(obj_path, "w")
    out.write("# the arena: meshes (export_arena.py), z up, game units, made round (0, 0, 0)\n")
    out.write("mtllib %s\n" % os.path.basename(mtl_path))
    nv = nt = 0
    lines, done = [], set()
    for k, (ob, pos, me, ev, mats) in enumerate(data):
        if len(mats) > MAX_MATERIALS:
            err("%s: %d materials (%d at most)" % (ob.name, len(mats), MAX_MATERIALS))
        uv = me.uv_layers.active
        ca = me.color_attributes.get("Col") or me.color_attributes.active_color
        cols = None
        if ca is not None:
            if ca.domain == "POINT":
                cols = [tuple(ca.data[i].color[:3]) for i in range(len(me.vertices))]
            else:  # a corner colour: one per vertex (the first corner's)
                cols = [None] * len(me.vertices)
                for l in me.loops:
                    if cols[l.vertex_index] is None:
                        cols[l.vertex_index] = tuple(ca.data[l.index].color[:3])
                cols = [c or (1, 1, 1) for c in cols]
            ncol = len({tuple(round(x * 255) for x in c) for c in cols})
            if ncol > MAX_COLOURS:
                warn("%s: %d vertex colours (the composer makes them coarser to %d)" % (ob.name, ncol, MAX_COLOURS))
        out.write("o %s\n" % base_name(ob))
        for i, p in enumerate(pos):
            if cols:
                c = cols[i]
                out.write("v %s %s %s %.3f %.3f %.3f\n" % (num(p.x), num(p.y), num(p.z), c[0], c[1], c[2]))
            else:
                out.write("v %s %s %s\n" % (num(p.x), num(p.y), num(p.z)))
        vt_index = {}
        if uv:
            for l in me.loops:
                key = (round(uv.data[l.index].uv[0], 4), round(uv.data[l.index].uv[1], 4))
                if key not in vt_index:
                    vt_index[key] = len(vt_index)
                    out.write("vt %.4f %.4f\n" % key)
        else:
            warn("%s: no UVs (its textures are not mapped)" % ob.name)
        by_mat = {}
        too_big = 0
        for t in me.loop_triangles:
            m = mats[min(t.material_index, len(mats) - 1)]
            by_mat.setdefault(m.name if m else "default", []).append(t)
            if uv:
                us = [uv.data[li].uv for li in t.loops]
                if max(max(u[q] for u in us) - min(u[q] for u in us) for q in (0, 1)) > MAX_REPEAT:
                    too_big += 1
        if too_big:
            err("%s: %d triangles where a texture repeats more than %g times (split them)" % (ob.name, too_big, MAX_REPEAT))
        local = sorted(by_mat, key=order.index)
        for li_, name in enumerate(local):
            spec = used[name].get("rac1_material") if used[name] else None
            if spec and name not in done:
                done.add(name)
                lines.append("# %s (%s's texture %d)" % (name, base_name(ob), li_))
                lines.append("material tie %d tex %d %s" % (first + k, li_, spec))
            out.write("usemtl %s\n" % name)
            for t in by_mat[name]:
                corners = []
                for li in t.loops:
                    v = nv + me.loops[li].vertex_index + 1
                    if uv:
                        key = (round(uv.data[li].uv[0], 4), round(uv.data[li].uv[1], 4))
                        corners.append("%d/%d" % (v, nt + vt_index[key] + 1))
                    else:
                        corners.append("%d" % v)
                out.write("f %s\n" % " ".join(corners))
        nv += len(pos)
        nt += len(vt_index)
        print("mesh %d %s: %d triangles, %d materials%s" % (k, base_name(ob), len(me.loop_triangles), len(by_mat),
                                                             ", vertex colours" if cols else ""))
        ev.to_mesh_clear()
    out.close()
    pngs = []
    with open(mtl_path, "w") as f:
        f.write("# the arena: materials (export_arena.py)\n")
        for name in order:
            tex, kd = mat_info(used[name])
            f.write("newmtl %s\nKd %.3f %.3f %.3f\n" % (name, kd[0], kd[1], kd[2]))
            if tex:
                if not os.path.exists(tex):
                    err("material %s: no texture %s" % (name, tex))
                    continue
                f.write("map_Kd %s\n" % os.path.basename(tex))
                pngs.append(tex)
    return sorted(set(pngs)), lo, lines, len(data)


# ---------------------------------------------------------------- the classes' models, the sounds

def model(level, name, tris, colour):
    """An OBJ model of triangles round its own origin, standing on it, one material (Kd 1), coloured per vertex."""
    with open(os.path.join(level, name + ".obj"), "w") as f:
        f.write("# the arena: the %s class's model (export_arena.py), round its origin\nmtllib %s.mtl\no %s\nusemtl %s\n" % (
            name, name, name, name))
        n = 0
        for t in tris:
            for p in t:
                c = colour(p)
                f.write("v %s %s %s %.3f %.3f %.3f\n" % (num(p[0]), num(p[1]), num(p[2]), c[0], c[1], c[2]))
            f.write("f %d %d %d\n" % (n + 1, n + 2, n + 3))
            n += 3
    with open(os.path.join(level, name + ".mtl"), "w") as f:
        f.write("# the arena: the %s class's material\nnewmtl %s\nKd 1 1 1\n" % (name, name))


def spawn_tris():
    r, h = 0.5, 0.6
    ring = [(r * math.cos(k * math.pi / 2), r * math.sin(k * math.pi / 2)) for k in range(4)]
    out = []
    for k in range(4):
        a, b = ring[k], ring[(k + 1) % 4]
        out.append(((a[0], a[1], h), (b[0], b[1], h), (0, 0, 2 * h)))
        out.append(((b[0], b[1], h), (a[0], a[1], h), (0, 0, 0)))
    return out


def flame_sprites(level):
    """The flame's model: tongues of fire as crossed upright planes, both sides, with the flame textures (their
    transparent texels cut out): three wide outer ones (flame.png) round two narrower, hotter ones (flame_core.png),
    standing on the origin. The plugin flickers, turns and scales it."""
    planes = ([("flame_a", 1.9, 3.4, a, 0.0, 0.0) for a in (0.0, 60.0, 120.0)] +
              [("flame_a", 1.3, 2.5, 30.0, 0.45, 0.25), ("flame_a", 1.2, 2.2, 100.0, -0.35, -0.4)] +  # (two licks off the middle)
              [("flame_b", 1.1, 2.6, a, 0.0, 0.0) for a in (30.0, 120.0)])
    with open(os.path.join(level, "flame.obj"), "w") as f:
        f.write("# the arena: the flame class's model (export_arena.py), round its origin\nmtllib flame.mtl\no flame\n")
        f.write("vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n")
        n = 0
        for mat in ("flame_a", "flame_b"):
            f.write("usemtl %s\n" % mat)
            for m, w, h, ang, ox, oy in planes:
                if m != mat:
                    continue
                c, s_ = math.cos(math.radians(ang)) * w / 2, math.sin(math.radians(ang)) * w / 2
                for p in ((ox - c, oy - s_, 0.0), (ox + c, oy + s_, 0.0), (ox + c, oy + s_, h), (ox - c, oy - s_, h)):
                    f.write("v %s %s %s\n" % tuple(num(q) for q in p))
                f.write("f %d/1 %d/2 %d/3 %d/4\n" % (n + 1, n + 2, n + 3, n + 4))
                f.write("f %d/2 %d/1 %d/4 %d/3\n" % (n + 2, n + 1, n + 4, n + 3))  # (its back)
                n += 4
    with open(os.path.join(level, "flame.mtl"), "w") as f:
        f.write("# the arena: the flame's materials\nnewmtl flame_a\nKd 1 1 1\nmap_Kd flame.png\n"
                "newmtl flame_b\nKd 1 1 1\nmap_Kd flame_core.png\n")
    for t in ("flame.png", "flame_core.png"):
        shutil.copy(os.path.join(HERE, "textures", t), os.path.join(level, t))


def ball_tris(r, subdiv=2):
    """A ball standing on its origin: an octahedron split `subdiv` times, pushed out to the sphere."""
    tris = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((0, 1, 0), (-1, 0, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
            ((0, -1, 0), (1, 0, 0), (0, 0, 1)), ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((-1, 0, 0), (0, 1, 0), (0, 0, -1)),
            ((0, -1, 0), (-1, 0, 0), (0, 0, -1)), ((1, 0, 0), (0, -1, 0), (0, 0, -1))]
    for _ in range(subdiv):
        out = []
        for a, b, c in tris:
            ab, bc, ca = (tuple((x + y) / 2 for x, y in zip(a, b)), tuple((x + y) / 2 for x, y in zip(b, c)),
                          tuple((x + y) / 2 for x, y in zip(c, a)))
            out += [(a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)]
        tris = out

    def at(p):
        l = math.sqrt(sum(q * q for q in p))
        return (r * p[0] / l, r * p[1] / l, r * p[2] / l + r)

    return [tuple(at(p) for p in t) for t in tris]


def bomb_tris():
    """The bomb: a ball, a fin cross on top (it falls nose down)."""
    out = ball_tris(0.5)
    for ang in (0.0, 90.0):
        c, s_ = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        q = [(-0.35 * c, -0.35 * s_, 0.85), (0.35 * c, 0.35 * s_, 0.85), (0.35 * c, 0.35 * s_, 1.35), (-0.35 * c, -0.35 * s_, 1.35)]
        out += [(q[0], q[1], q[2]), (q[0], q[2], q[3]), (q[1], q[0], q[3]), (q[1], q[3], q[2])]
    return out


def bomb_colour(p):
    if abs(p[2] - 0.5) < 0.1:
        return (1.0, 0.2, 0.1)  # (the glowing band)
    return (0.22, 0.22, 0.25) if p[2] < 0.86 else (0.5, 0.12, 0.1)


def ring_tris():
    """Where a bomb falls: two flat rings and a cross on the ground, facing up."""
    out = []
    for r0, r1, n in ((1.9, 2.3, 32), (0.8, 1.0, 20)):
        for k in range(n):
            a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
            p = [(r0 * math.cos(a0), r0 * math.sin(a0), 0.06), (r1 * math.cos(a0), r1 * math.sin(a0), 0.06),
                 (r1 * math.cos(a1), r1 * math.sin(a1), 0.06), (r0 * math.cos(a1), r0 * math.sin(a1), 0.06)]
            out += [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
    for c, s_ in ((1.0, 0.0), (0.0, 1.0)):
        w = 0.08
        p = [(-2.3 * c - w * s_, -2.3 * s_ + w * c, 0.06), (-2.3 * c + w * s_, -2.3 * s_ - w * c, 0.06),
             (2.3 * c + w * s_, 2.3 * s_ - w * c, 0.06), (2.3 * c - w * s_, 2.3 * s_ + w * c, 0.06)]
        out += [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
    return out


def mine_tris():
    """A rail mine: a ball on its origin, seven spikes."""
    out = ball_tris(0.42)
    c = Vector((0.0, 0.0, 0.42))
    for d in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0.6, 0.6, 0.5), (-0.6, -0.6, 0.5)):
        d = Vector(d).normalized()
        side = d.cross(Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))).normalized() * 0.08
        up = d.cross(side).normalized() * 0.08
        base = c + d * 0.36
        tip = c + d * 0.8
        q = [base + side, base + up, base - side, base - up]
        for k in range(4):
            out.append((tuple(q[k]), tuple(q[(k + 1) % 4]), tuple(tip)))
    return out


def mine_colour(p):
    r = math.sqrt(p[0] ** 2 + p[1] ** 2 + (p[2] - 0.42) ** 2)
    if r > 0.6:
        return (1.0, 0.25, 0.1)  # (the spikes' tips)
    return (1.0, 0.3, 0.12) if abs(p[2] - 0.42) < 0.08 else (0.22, 0.22, 0.25)


def zap_tris():
    """A drop zone's ring electrified, flat on the floor over the texture's ring (5 to 5.6 from its middle): the ring
    and twelve jagged sparks across the grated middle."""
    out = []
    for k in range(48):
        a0, a1 = 2 * math.pi * k / 48, 2 * math.pi * (k + 1) / 48
        p = [(r * math.cos(a), r * math.sin(a), 0.05) for r, a in ((4.95, a0), (5.65, a0), (5.65, a1), (4.95, a1))]
        out += [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
    for k in range(12):
        a = 2 * math.pi * k / 12 + 0.13
        pts = [(4.95 * t, 0.35 * math.sin(t * 17 + k) * (1 - t)) for t in (1.0, 0.75, 0.5, 0.25, 0.08)]
        for (r0, o0), (r1, o1) in zip(pts, pts[1:]):
            c, s_ = math.cos(a), math.sin(a)
            q = [(r0 * c - (o0 - 0.06) * s_, r0 * s_ + (o0 - 0.06) * c, 0.06), (r0 * c - (o0 + 0.06) * s_, r0 * s_ + (o0 + 0.06) * c, 0.06),
                 (r1 * c - (o1 + 0.06) * s_, r1 * s_ + (o1 + 0.06) * c, 0.06), (r1 * c - (o1 - 0.06) * s_, r1 * s_ + (o1 - 0.06) * c, 0.06)]
            out += [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
    return out


def zap_colour(p):
    return (0.55, 0.9, 1.0) if math.hypot(p[0], p[1]) > 4.9 else (0.75, 0.95, 1.0)


def portal_tris():
    """The portal: a ring standing across the way through it (its axis the object's x), 3 across inside, its tube 0.35
    thick and eight-sided, and an inner ring of sparks."""
    out = []
    n, m, R, r = 40, 8, 3.35, 0.35

    def at(a, b, rr=r, RR=R):
        c = RR + rr * math.cos(b)
        return (rr * math.sin(b), c * math.cos(a), c * math.sin(a))
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        for j in range(m):
            b0, b1 = 2 * math.pi * j / m, 2 * math.pi * (j + 1) / m
            p = [at(a0, b0), at(a1, b0), at(a1, b1), at(a0, b1)]
            out += [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
    for i in range(24):  # (sparks: short flat bars across the inside, turning round it)
        a = 2 * math.pi * i / 24
        p = [at(a - 0.05, 0, 0.0, 2.2), at(a + 0.05, 0, 0.0, 2.2), at(a + 0.03, 0, 0.0, 2.95), at(a - 0.03, 0, 0.0, 2.95)]
        out += [(p[0], p[1], p[2]), (p[0], p[2], p[3]), (p[0], p[2], p[1]), (p[0], p[3], p[2])]
    return out


def portal_colour(p):
    rr = math.hypot(p[1], p[2])
    return (0.55, 0.35, 1.0) if rr > 3.1 else (0.85, 0.7, 1.0) if rr > 2.9 else (0.4, 0.9, 1.0)


FLOOD_HALF = 47.9  # the flood's square: the arena's floor inside its wall


def flood_tris():
    """The lava flooding the arena: a square over its floor in 4-unit cells (each cell's corners coloured apart)."""
    out, n = [], 24
    for i in range(n):
        for j in range(n):
            x0, x1 = -FLOOD_HALF + 2 * FLOOD_HALF * i / n, -FLOOD_HALF + 2 * FLOOD_HALF * (i + 1) / n
            y0, y1 = -FLOOD_HALF + 2 * FLOOD_HALF * j / n, -FLOOD_HALF + 2 * FLOOD_HALF * (j + 1) / n
            p = [(x0, y0, 0.0), (x1, y0, 0.0), (x1, y1, 0.0), (x0, y1, 0.0)]
            out += [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
    return out


def flood_colour(p):
    h = math.sin(p[0] * 0.37 + math.sin(p[1] * 0.21) * 2.0) * math.cos(p[1] * 0.29 - math.sin(p[0] * 0.17) * 2.0)
    t = 0.5 + 0.5 * h
    return (1.0, 0.25 + 0.45 * t, 0.05 + 0.12 * t)


def flame_tris():
    """Three tongues of fire: tapered six-sided cones, leaning out a little, the middle one tallest (3.2)."""
    out = []
    for (ox, oy, r, h) in ((0.0, 0.0, 0.85, 3.2), (0.55, 0.3, 0.55, 2.3), (-0.45, -0.4, 0.6, 2.6), (-0.2, 0.55, 0.5, 2.0)):
        ring = [(ox + r * math.cos(k * math.pi / 3), oy + r * math.sin(k * math.pi / 3)) for k in range(6)]
        mid = [(ox + 0.55 * r * math.cos(k * math.pi / 3 + 0.5), oy + 0.55 * r * math.sin(k * math.pi / 3 + 0.5)) for k in range(6)]
        tip = (ox * 1.6, oy * 1.6, h)
        for k in range(6):
            a, b = ring[k], ring[(k + 1) % 6]
            c, d = mid[k], mid[(k + 1) % 6]
            pa, pb = (a[0], a[1], 0.0), (b[0], b[1], 0.0)
            pc, pd = (c[0], c[1], h * 0.45), (d[0], d[1], h * 0.45)
            out += [(pa, pb, pd), (pa, pd, pc), (pc, pd, tip)]
    return out


def flame_colour(p):
    t = max(0.0, min(1.0, p[2] / 3.2))
    if t < 0.4:
        return (1.0, 0.95 - 0.5 * t / 0.4, 0.55 - 0.45 * t / 0.4)
    return (1.0, 0.75 - 0.6 * (t - 0.4) / 0.6, 0.1 - 0.08 * (t - 0.4) / 0.6)


# ---------------------------------------------------------------- the grind rails (arena_rails: build_rails.py)

RAIL_STEP = 1.5      # a rail's points, units apart along it (the composer's step too)
RAIL_R = 0.12        # the tube's radius: its top is the rail's line (where Ratchet's feet go)
RAIL_WIDE = 0.5      # the collision strip under a rail's line (rail_strips, export_collision)
STRIP_POINTS = 120   # a strip's points per object at most (a class's collision mesh: 255 vertices at most)
# a rail's strip near the ground Ratchet walks on (from its start) is an object the plugin makes solid only while the rail is
# on (else he walks on it, in the air); the rest is the level's collision, solid always, too high to be walked onto (and
# what Kalebo III's rail mines float over: their floor probe sees the level's collision, not objects'): those units
STRIP_TOGGLE = {"rail_escape": 24.0}  # (over the hub's top and its west ramp)
STRIP_TOGGLE_DEFAULT = 8.0            # (by the station's platform)
STRIP_TOGGLE_END = {"rail_ritorno": 40.0}  # (from its end: over the arena's floor and the hub's top)


def strip_split(name, pts):
    """A rail's points: its strip's object part (from its start, STRIP_TOGGLE; or from its end, STRIP_TOGGLE_END) and the
    level's part (sharing a point)."""
    if name in STRIP_TOGGLE_END:
        k = min(len(pts) - 1, max(1, int(math.ceil(STRIP_TOGGLE_END[name] / RAIL_STEP))))
        return pts[len(pts) - 1 - k:], pts[:len(pts) - k]
    k = min(len(pts) - 1, max(1, int(math.ceil(STRIP_TOGGLE.get(name, STRIP_TOGGLE_DEFAULT) / RAIL_STEP))))
    return pts[:k + 1], pts[k:]


def strip_quads(pts):
    """A strip's vertices under the points (left, right of each) and its triangles (indices, facing up)."""
    verts = []
    for i in range(len(pts)):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        n = t.cross(Vector((0, 0, 1)))
        n = n.normalized() * (RAIL_WIDE / 2) if n.length > 1e-4 else Vector((RAIL_WIDE / 2, 0, 0))
        q = pts[i] - Vector((0, 0, 0.02))
        verts += [q + n, q - n]
    tris = []
    for i in range(len(pts) - 1):
        for tri in ((2 * i, 2 * i + 1, 2 * i + 3), (2 * i, 2 * i + 3, 2 * i + 2)):
            pa, pb, pc = (verts[j] for j in tri)
            tris.append(tri if (pb - pa).cross(pc - pa).z > 0 else (tri[0], tri[2], tri[1]))
    return verts, tris
PYLON_EVERY = 14.0   # a pylon under a rail every so many units
RAIL_SPEC = "metal 0.3 1"  # the rails' models' material line


def rail_points(ob):
    """A rail curve's line (world coordinates): its splines' points every RAIL_STEP units, its ends kept."""
    from mathutils.geometry import interpolate_bezier
    mw = ob.matrix_world
    line = []
    for sp in ob.data.splines:
        if sp.type == "BEZIER":
            bp = sp.bezier_points
            for i in range(len(bp) - 1):
                seg = interpolate_bezier(bp[i].co, bp[i].handle_right, bp[i + 1].handle_left, bp[i + 1].co, 33)
                line += [mw @ p for p in (seg if not line else seg[1:])]
        else:
            line += [mw @ Vector(p.co[:3]) for p in sp.points]
    if len(line) < 2:
        return line
    total = sum((line[i + 1] - line[i]).length for i in range(len(line) - 1))
    n = max(2, int(math.ceil(total / RAIL_STEP)) + 1)
    out, i, run = [line[0]], 0, 0.0
    for k in range(1, n - 1):
        want = total * k / (n - 1)
        while run + (line[i + 1] - line[i]).length < want:
            run += (line[i + 1] - line[i]).length
            i += 1
        seg = line[i + 1] - line[i]
        out.append(line[i] + seg * ((want - run) / seg.length))
    out.append(line[-1])
    return out


def obox(c, ax, ay, az):
    """An oriented box's triangles, outward: centre c, half extents along the vectors ax, ay, az."""
    out = []
    for a, b, n in ((ax, ay, az), (ay, ax, -az), (ay, az, ax), (az, ay, -ax), (az, ax, ay), (ax, az, -ay)):
        q = [c + n + s * a + t * b for s, t in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
            nn = (tri[1] - tri[0]).cross(tri[2] - tri[0])
            out.append(tri if nn.dot(n) >= 0 else (tri[0], tri[2], tri[1]))
    return out


def rail_model(level, name, pts, centre, depsgraph, pylons_on=True, beam=0.45, beam_until=1e9, end_stop=False):
    """A rail's look, round `centre`: an eight-sided tube under its line, pylons down to the ground with a cross bar
    (`beam` to each side: under the lanes beside it, as far as `beam_until` along it; none with pylons_on off); with
    end_stop, a striped buffer at its end (a lane that ends)."""
    tube, pylons, bars, stop = [], [], [], []
    up = Vector((0, 0, 1))
    rings = []
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        n = t.cross(up)
        n = n.normalized() if n.length > 1e-4 else Vector((1, 0, 0))
        b = n.cross(t)
        c = p - b * RAIL_R
        rings.append([(c + RAIL_R * (math.cos(k * math.pi / 4) * n + math.sin(k * math.pi / 4) * b), c) for k in range(8)])
    for i in range(len(rings) - 1):
        for k in range(8):
            (a, ca), (d, _) = rings[i][k], rings[i][(k + 1) % 8]
            (b2, cb), (e, _) = rings[i + 1][k], rings[i + 1][(k + 1) % 8]
            for tri in ((a, b2, e), (a, e, d)):
                nn = (tri[1] - tri[0]).cross(tri[2] - tri[0])
                tube.append(tri if nn.dot(tri[0] - ca) >= 0 else (tri[0], tri[2], tri[1]))
    run, next_at = 0.0, 6.0
    total = sum((pts[i + 1] - pts[i]).length for i in range(len(pts) - 1))
    scene = bpy.context.scene
    for i in range(len(pts) - 1):
        run += (pts[i + 1] - pts[i]).length
        if not pylons_on or run < next_at or run > total - 3.0:
            continue
        next_at += PYLON_EVERY
        p = pts[i + 1]
        hit, loc, _, _, _, _ = scene.ray_cast(depsgraph, p - Vector((0, 0, 0.5)), Vector((0, 0, -1)))
        g = loc.z if hit else p.z - 30.0
        top = p.z - 2 * RAIL_R - 0.2
        if top - g < 1.0:
            continue
        mid = Vector((p.x, p.y, (top + g - 0.5) / 2))
        pylons += obox(mid, Vector((0.16, 0, 0)), Vector((0, 0.16, 0)), Vector((0, 0, (top - g + 0.5) / 2)))
        t = (pts[i + 2 if i + 2 < len(pts) else i + 1] - pts[i]).normalized()
        n = t.cross(up).normalized()
        half = beam if run <= beam_until else 0.45
        bars += obox(Vector((p.x, p.y, top + 0.08)), n * half, Vector((t.x, t.y, 0)).normalized() * 0.1, Vector((0, 0, 0.08)))
    if end_stop:
        t = (pts[-1] - pts[-2]).normalized()
        n = t.cross(up).normalized()
        stop += obox(pts[-1] + t * 0.2 + Vector((0, 0, 0.1)), t * 0.2, n * 0.35, Vector((0, 0, 0.35)))
    with open(os.path.join(level, name + ".obj"), "w") as f:
        f.write("# the arena: the %s class's model (export_arena.py), round its origin\nmtllib %s.mtl\no %s\nusemtl %s\n" % (
            name, name, name, name))
        nv = 0
        for tris, col in ((tube, (0.82, 0.87, 0.92)), (pylons, (0.28, 0.3, 0.33)), (bars, (1.0, 0.72, 0.18)), (stop, (1.0, 0.2, 0.1))):
            for tri in tris:
                for q in tri:
                    q = q - centre
                    f.write("v %s %s %s %.3f %.3f %.3f\n" % ((num(q.x), num(q.y), num(q.z)) + col))
                f.write("f %d %d %d\n" % (nv + 1, nv + 2, nv + 3))
                nv += 3
    with open(os.path.join(level, name + ".mtl"), "w") as f:
        f.write("# the arena: the %s class's material\nnewmtl %s\nKd 1 1 1\n" % (name, name))
    return len(tube) + len(pylons) + len(bars) + len(stop)


def rail_strips(level, name, pts):
    """A rail's collision strip near the ground (strip_split's first part; RAIL_WIDE wide just under its line: the game's
    grind lets go of a rail with nothing under Ratchet's feet) as objects of their own: classes strip_<rail>_<k> of at most
    STRIP_POINTS points each, a collision mesh of their own, solid only while the plugin has the rail on (the composer's
    strips, `nostrip` not given, are always solid: a ledge in the air where a rail is off, walked on over the hub). Their
    models are the strips too, hidden by the plugin. [(class, centre)]"""
    pts = strip_split(name, pts)[0]
    out, k, i0 = [], 0, 0
    while i0 < len(pts) - 1:
        i1 = min(len(pts), i0 + STRIP_POINTS)
        verts, tris = strip_quads(pts[i0:i1])
        lo = [min(v[a] for v in verts) for a in range(3)]
        hi = [max(v[a] for v in verts) for a in range(3)]
        c = Vector([(a + b) / 2 for a, b in zip(lo, hi)])
        rel = [v - c for v in verts]
        cls = "strip_%s_%d" % (name[5:], k)
        with open(os.path.join(level, cls + "_col.obj"), "w") as f:
            f.write("# the arena: the %s class's collision (export_arena.py: a rail's strip), round its origin\no %s\nusemtl type31\n" % (cls, cls))
            for v in rel:
                f.write("v %s %s %s\n" % (num(v.x), num(v.y), num(v.z)))
            for tri in tris:
                f.write("f %d %d %d\n" % tuple(j + 1 for j in tri))
        model(level, cls, [tuple(tuple(rel[j]) for j in tri) for tri in tris], lambda p: (0.3, 0.3, 0.3))
        out.append((cls, c))
        k += 1
        i0 = i1 - 1  # (one point shared: the strips join)
    return out


def export_rails(level, depsgraph):
    """Each rail's class model and its strips (rail_strips); the rails as (class name, centre, level.txt's rail line: its
    points, made round (0, 0, 0) as every coordinate there, its strips)."""
    rails = []
    obs = sorted((o for o in bpy.data.collections["arena_rails"].all_objects if o.type == "CURVE"), key=lambda o: o.name) \
        if bpy.data.collections.get("arena_rails") else []
    for ob in obs:
        name = base_name(ob)
        if not name.startswith("rail_"):
            warn("%s: a rail curve is named rail_<name>: left out" % ob.name)
            continue
        pts = rail_points(ob)
        if len(pts) < 2:
            err("%s: %d points (2 at least)" % (ob.name, len(pts)))
            continue
        lo = [min(p[i] for p in pts) for i in range(3)]
        hi = [max(p[i] for p in pts) for i in range(3)]
        centre = Vector([(a + b) / 2 for a, b in zip(lo, hi)])
        ntris = rail_model(level, name, pts, centre, depsgraph, bool(ob.get("pylons", 1)), float(ob.get("beam", 0.45)),
                           float(ob.get("beam_until", 1e9)), bool(ob.get("end_stop", 0)))
        line = "rail %s nostrip %s" % (name[5:], " ".join("%s %s %s" % (num(p.x), num(p.y), num(p.z)) for p in pts))
        rails.append((name, centre, line, rail_strips(level, name, pts)))
        print("rail %s: %d points, %s units, its model %d triangles" % (
            name, len(pts), num(sum((pts[i + 1] - pts[i]).length for i in range(len(pts) - 1))), ntris))
    return rails


def export_section(level, name, opts, depsgraph, first):
    """A section's block for level.txt (its lines after main's): its collision and meshes (<name>_collision.obj,
    <name>_meshes.obj from its collections), its sky and colours, its entries (entry_<n> markers). Its meshes are host
    mesh classes after main's (and before the classes' models: `first` the first's index): (the block, their material
    lines, their count)."""
    if opts.get("world"):
        L = ["# the section %s: %s's world (its scenery, collision, sky, music; the planet's coordinates)" % (name, opts["world"]),
             "section %s remove respawn reset%s" % (name, " fresh" if opts.get("fresh") else ""), "  world %s" % opts["world"]]
        if opts.get("zones_all"):  # (every zone of the world drawn wherever the camera is: Quartu's Giant Clank arena)
            L.append("  zones all")
        if opts.get("hero"):  # (the planet's hero code, a mode: board, clank, giantclank)
            L.append("  hero %s" % opts["hero"])
        if opts.get("objects"):  # (its placed objects of these classes, or all; in the box)
            L.append("  objects %s" % opts["objects"])
        if opts.get("box"):
            L.append("  box %s" % " ".join(num(v) for v in opts["box"]))
        if opts.get("boss"):
            L.append("  boss %s" % opts["boss"])
        for e, at, a in opts.get("entries", []):
            L.append("  entry %s %s %s %s %s" % (e, num(at[0]), num(at[1]), num(at[2]), num(a)))
        for c, at, a in opts.get("checkpoints", []):
            L.append("  checkpoint %s %s %s %s %s" % (c, num(at[0]), num(at[1]), num(at[2]), num(a)))
        for r, pts in opts.get("rails", []):  # (added to the planet's own rails)
            L.append("  rail %s%s %s" % (r, " mesh" if opts.get("rail_mesh") else "",
                                          " ".join("%s %s %s" % (num(x), num(y), num(z)) for x, y, z in pts)))
        if opts.get("death_height") is not None:  # (the planet's coordinates: not main's, which is the arena's)
            L.append("  death_height %s" % num(opts["death_height"]))
        L.append("end")
        return L, [], 0
    if not bpy.data.collections.get(name + "_visual"):
        warn("section %s: no collection %s_visual (build_prova.py): left out" % (name, name))
        return [], [], 0
    export_collision(os.path.join(level, name + "_collision.obj"), depsgraph, name + "_collision")
    pngs, _, mat_lines, n = export_meshes(os.path.join(level, name + "_meshes.obj"), os.path.join(level, name + "_meshes.mtl"),
                                          depsgraph, name + "_visual", first)
    for p in pngs:
        png_check(p)
        shutil.copy(p, os.path.join(level, os.path.basename(p)))
    L = ["# the section %s (%s): its own collision, meshes and sky; Ratchet comes in by its entries" % (name, name + "_* collections"),
         "section %s remove respawn reset" % name,
         "  collision %s_collision.obj" % name,
         "  meshes %s_meshes.obj" % name]
    if opts.get("sky") is not None:
        L.append("  sky %d" % opts["sky"])
    for key in ("background", "fog"):
        if opts.get(key):
            L.append("  %s %d %d %d" % ((key,) + tuple(opts[key])))
    if opts.get("death_height") is not None:
        L.append("  death_height %s" % num(opts["death_height"]))
    for o in sorted(coll(name + "_markers"), key=lambda o: o.name):
        p = o.matrix_world.translation
        if base_name(o).startswith("entry_"):
            L.append("  entry %s %s %s %s %s" % (base_name(o)[6:], num(p.x), num(p.y), num(p.z), num(yaw(o))))
        elif base_name(o).startswith("checkpoint_"):
            L.append("  checkpoint %s %s %s %s %s" % (base_name(o)[11:], num(p.x), num(p.y), num(p.z), num(yaw(o))))
        else:
            warn("section %s: marker %s is not entry_* nor checkpoint_*: left out" % (name, o.name))
    rails = bpy.data.collections.get(name + "_rails")  # (its rails: the composer's bar for their look, its collision strip)
    for o in sorted((o for o in rails.all_objects if o.type == "CURVE") if rails else [], key=lambda o: o.name):
        pts = rail_points(o)
        L.append("  rail %s mesh %s" % (base_name(o)[5:], " ".join("%s %s %s" % (num(p.x), num(p.y), num(p.z)) for p in pts)))
    L.append("end")
    return L, mat_lines, n


# ---------------------------------------------------------------- the ship's map's picture

MAP_VIEW = {"eye": (-62.0, -118.0, 62.0), "at": (4.0, 6.0, 0.0), "lens": 34.0, "size": (612, 216)}


def render_map_picture(path):
    """level.txt's map_picture (306 x 108 in the game: rendered at twice that): the arena from the south-west, by the
    scene's own engine (EEVEE), with a sun from high up for this render only (the level's sun is Veldin's, low) and the
    collision and the markers hidden (the collision's meshes are wireframes in the viewport, faces in a render). The
    camera, the sun and every setting it touches are put back however it ends. False when it could not render."""
    sc = bpy.context.scene
    r = sc.render
    kept = (sc.camera, r.resolution_x, r.resolution_y, r.resolution_percentage, r.filepath, r.film_transparent,
            r.image_settings.file_format)
    cols = [c for c in (bpy.data.collections.get(n) for n in ("arena_collision", "arena_markers")) if c]
    hidden = [c.hide_render for c in cols]
    cd = bpy.data.cameras.new("_map_camera")
    cam = bpy.data.objects.new("_map_camera", cd)
    ld = bpy.data.lights.new("_map_sun", "SUN")
    sun = bpy.data.objects.new("_map_sun", ld)
    try:
        sc.collection.objects.link(cam)
        sc.collection.objects.link(sun)
        cd.lens = MAP_VIEW["lens"]
        cam.location = MAP_VIEW["eye"]
        cam.rotation_euler = (Vector(MAP_VIEW["at"]) - Vector(MAP_VIEW["eye"])).to_track_quat("-Z", "Y").to_euler()
        ld.energy, ld.color, ld.angle = 5.0, (1.0, 0.93, 0.85), math.radians(8)
        sun.rotation_euler = (math.radians(25), math.radians(-15), 0.0)
        for c in cols:
            c.hide_render = True
        sc.camera = cam
        r.resolution_x, r.resolution_y = MAP_VIEW["size"]
        r.resolution_percentage = 100
        r.film_transparent = False
        formats = [i.identifier for i in r.image_settings.bl_rna.properties["file_format"].enum_items]
        if "PNG" in formats:
            r.image_settings.file_format = "PNG"
        r.filepath = path
        bpy.ops.render.render(write_still=True)
        ok = os.path.exists(path)
        print("map picture: rendered" if ok else "map picture: not written")
        return ok
    except Exception as e:  # (no GPU for the engine in a headless run, ...)
        warn("map picture: could not render (%s): the level has none" % e)
        return False
    finally:
        sc.camera = kept[0]
        r.resolution_x, r.resolution_y, r.resolution_percentage, r.filepath, r.film_transparent = kept[1:6]
        r.image_settings.file_format = kept[6]
        for c, h in zip(cols, hidden):
            c.hide_render = h
        bpy.data.objects.remove(cam, do_unlink=True)
        bpy.data.objects.remove(sun, do_unlink=True)
        bpy.data.cameras.remove(cd)
        bpy.data.lights.remove(ld)


def wav(path, rate, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(v * 32767)))) for v in samples))


def rumble(n, seed, cut, rate):
    """Noise through two one-pole low-passes (cut in Hz): a roar's body."""
    r, a, b = seed, 0.0, 0.0
    k = 1.0 - math.exp(-2 * math.pi * cut / rate)
    out = []
    for _ in range(n):
        r = (r * 1103515245 + 12345) & 0x7fffffff
        x = r / 0x3fffffff - 1.0
        a += k * (x - a)
        b += k * (a - b)
        out.append(b)
    peak = max(abs(v) for v in out) or 1.0
    return [v / peak for v in out]


def tone_glide(rate, secs, f0, f1):
    """A sine gliding from f0 to f1 Hz, fading in and out."""
    out, ph = [], 0.0
    n = int(rate * secs)
    for i in range(n):
        f = f0 + (f1 - f0) * i / n
        ph += 2 * math.pi * f / rate
        env = min(1.0, i / 800.0) * min(1.0, (n - i) / 2000.0)
        out.append(0.4 * env * math.sin(ph))
    return out


def sounds(level):
    rate = 22050
    # the roar: a loop of 28-sample blocks, its end crossfaded into its start (no seam)
    n, fade = 28 * 800, 2000
    body = rumble(n + fade, 3, 420, rate)
    hiss = rumble(n + fade, 5, 3000, rate)
    mix = [0.75 * a + 0.25 * b for a, b in zip(body, hiss)]
    loop = [mix[i] * (i / fade if i < fade else 1.0) + (mix[n + i] * (1 - i / fade) if i < fade else 0.0) for i in range(n)]
    wav(os.path.join(level, "flame_roar.wav"), rate, [0.7 * v for v in loop])
    burst = rumble(int(rate * 0.6), 7, 900, rate)
    wav(os.path.join(level, "flame_burst.wav"), rate,
        [0.9 * v * min(1.0, i / 200.0) * math.exp(-i / (rate * 0.18)) for i, v in enumerate(burst)])
    hs = rumble(int(rate * 0.9), 9, 5000, rate)
    wav(os.path.join(level, "flame_hiss.wav"), rate,
        [0.5 * v * min(1.0, i / 3000.0) * math.exp(-i / (rate * 0.5)) for i, v in enumerate(hs)])

    def notes(seq, length, decay, shape=1.0):
        out = []
        for f in seq:
            for i in range(int(rate * length)):
                t = i / rate
                env = min(1.0, t * 60) * math.exp(-t * decay)
                ph = 2 * math.pi * f * t
                out.append(0.45 * env * (math.sin(ph) + shape * 0.35 * math.sin(2 * ph) + shape * 0.15 * math.sin(3 * ph)))
        return out

    # a bomb falling: a whistle gliding down; landing: a deep boom
    wav(os.path.join(level, "bomb_whistle.wav"), rate, tone_glide(rate, 1.1, 1900.0, 500.0))
    boom = rumble(int(rate * 1.2), 13, 260, rate)
    crack = rumble(int(rate * 1.2), 17, 2500, rate)
    wav(os.path.join(level, "bomb_boom.wav"), rate,
        [(0.85 * b + 0.35 * c * math.exp(-i / (rate * 0.05))) * min(1.0, i / 60.0) * math.exp(-i / (rate * 0.35))
         for i, (b, c) in enumerate(zip(boom, crack))])

    # an electrified drop zone: a crackle (its warning: noise in bursts), a buzz (live, a loop: a 120 Hz saw, noise)
    cr = rumble(int(rate * 1.5), 19, 6000, rate)
    wav(os.path.join(level, "zap_crackle.wav"), rate,
        [0.6 * v * (1.0 if (i // 700) % 3 != 1 else 0.15) * min(1.0, i / 300.0) * min(1.0, (int(rate * 1.5) - i) / 2000.0)
         for i, v in enumerate(cr)])
    n = int(rate / 120.0) * 240
    hum = rumble(n, 23, 4000, rate)
    wav(os.path.join(level, "zap_buzz.wav"), rate,
        [0.45 * (2.0 * ((i * 120.0 / rate) % 1.0) - 1.0) * 0.7 + 0.25 * h for i, h in enumerate(hum)])

    # a wave starts: two horn blasts; the challenge won: a rising arpeggio; lost: two falling notes
    wav(os.path.join(level, "wave_start.wav"), rate, notes((220.0, 330.0), 0.35, 3.0, 1.6))
    wav(os.path.join(level, "challenge_win.wav"), rate, notes((523.3, 659.3, 784.0, 1046.5), 0.18, 4.0) + notes((1046.5,), 0.6, 3.0))
    wav(os.path.join(level, "challenge_fail.wav"), rate, notes((392.0, 261.6), 0.4, 3.5, 1.2))


# ---------------------------------------------------------------- level.txt, lights.txt

def export(out=None, install=False, bundle=False):
    del errors[:], warnings[:]
    out = out or os.path.join(HERE, "out", FOLDER)
    level = os.path.join(out, "level")
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(level)
    depsgraph = bpy.context.evaluated_depsgraph_get()

    lo_c = export_collision(os.path.join(level, "collision.obj"), depsgraph)
    pngs, lo_v, mat_lines, n_meshes = export_meshes(os.path.join(level, "meshes.obj"), os.path.join(level, "meshes.mtl"), depsgraph)
    n_sec = 0  # (the sections' meshes: host mesh classes after main's, before the classes' models)
    for p in pngs:
        png_check(p)
        shutil.copy(p, os.path.join(level, os.path.basename(p)))
    # the offset: x, y as "offset auto" (the meshes' lowest corner too), z the floor at floor_z
    lo = [min(a, b) for a, b in zip(lo_c, lo_v)]
    offset = [4.0 * math.ceil((16.0 - lo[i]) / 4.0) for i in range(2)]
    offset.append(max(LEVEL["floor_z"], 4.0 * math.ceil((16.0 - lo_c[2]) / 4.0)))
    print("offset (%s %s %s): the floor at z %s, the lowest point at %s" % (
        num(offset[0]), num(offset[1]), num(offset[2]), num(offset[2]), num(lo[2] + offset[2])))

    model(level, "spawn", spawn_tris(), lambda p: (1.0, 0.2, 0.8))
    flame_sprites(level)
    model(level, "bomb", bomb_tris(), bomb_colour)
    model(level, "mark", ring_tris(), lambda p: (1.0, 0.18, 0.1))
    model(level, "mine", mine_tris(), mine_colour)
    model(level, "zap", zap_tris(), zap_colour)
    model(level, "portal", portal_tris(), portal_colour)
    model(level, "flood", flood_tris(), flood_colour)
    rails = export_rails(level, depsgraph)
    sounds(level)
    pictures = {}  # (level.txt's picture lines: the planet made by make_textures.py, the map's view rendered here)
    if os.path.exists(os.path.join(HERE, "textures", "planet.png")):
        shutil.copy(os.path.join(HERE, "textures", "planet.png"), os.path.join(level, "planet.png"))
        pictures["picture"] = "planet.png"
    else:
        warn("no textures/planet.png (make_textures.py): the level has no planet picture")
    if render_map_picture(os.path.join(level, "map.png")):
        pictures["map_picture"] = "map.png"
    music, music_src = LEVEL["music"]
    if not os.path.exists(music_src):
        err("no music file %s" % music_src)
    else:
        shutil.copy(music_src, os.path.join(level, music))
    for _name, track, src in LEVEL.get("music_tracks", []):
        if not os.path.exists(src):
            err("no music file %s" % src)
        else:
            shutil.copy(src, os.path.join(level, track))
    if os.path.isdir(os.path.join(HERE, "plugin")):
        shutil.copytree(os.path.join(HERE, "plugin"), os.path.join(out, "plugin"))

    markers = sorted(coll("arena_markers"), key=lambda o: o.name)
    by = {base_name(o): o for o in markers}
    L = ["# the arena: rac1-native level, format 2 (docs/mods.md, \"Level mods\"); written by export_arena.py",
         "# from the Blender scene: edit the scene and export again rather than this file"]
    for key in ("name", "region"):
        for lang, text in LEVEL[key].items():
            L.append("%s%s %s" % (key, "." + lang if lang else "", text))
    L.append("base %s" % LEVEL["base_name"])
    L.append("# made round (0, 0, 0): x and y as \"offset auto\", z the floor at %s (lights.txt has it added)" % num(LEVEL["floor_z"]))
    L.append("offset %s %s %s" % tuple(num(c) for c in offset))
    for key in ("hero", "ship"):
        ob = by.get(key)
        if not ob:
            err("no %s empty" % key)
            continue
        p = ob.matrix_world.translation
        L.append("%s %s %s %s %s" % (key, num(p.x), num(p.y), num(p.z), num(yaw(ob))))
    L.append("death_height %s" % num(LEVEL["death_height"]))
    if LEVEL.get("hero_build"):
        L.append("# Ratchet's moves from the generic hero code, not Kalebo III's own: the game's own lava (he sinks, Cross")
        L.append("# bounces him out, left in it he is lost), with its animation")
        L.append("hero %s" % LEVEL["hero_build"])
    if LEVEL["format"] >= 3:
        L.append("# a session: loaded once at the arrival; a death brings Ratchet back to the current checkpoint")
        L.append("session")
        for name, at, a in LEVEL["checkpoints"]:
            L.append("checkpoint %s %s %s %s %s" % (name, num(at[0]), num(at[1]), num(at[2]), num(a)))
        L.append("respawn reset")
    if LEVEL["sky"] is not None:
        L.append("sky %d" % LEVEL["sky"])
    L.append("music %s" % music)
    for name, track, _src in LEVEL.get("music_tracks", []):
        L.append("music_track %s %s" % (name, track))
    for key, f in pictures.items():
        L.append("%s %s" % (key, f))
    for key in ("background", "fog"):
        if LEVEL.get(key):
            L.append("%s %d %d %d" % ((key,) + tuple(LEVEL[key])))
    if LEVEL.get("spawn"):
        L.append("# room for the base level's objects the wave plugin starts (base_spawn)")
        for cls, count in sorted(LEVEL["spawn"].items()):
            L.append("spawn %d %d" % (cls, count))
    for name in LEVEL.get("imports", []):
        L.append("import %s" % name)
    L.append("# every moby updated however far the camera is (the robots across the arena do not freeze)")
    L.append("update_all")

    mobys = [o for o in markers if o.name.startswith("obj_")]
    if mobys:
        L.append("# objects of the base level's classes (obj_<class> empties). Their ids (1, 2, ... in this order) are")
        L.append("# what a save remembers: add new ones at the end")
    for o in mobys:
        cls = base_name(o)[4:]
        if not cls.isdigit():
            err("%s: obj_<class> wants a class number" % o.name)
            continue
        p = o.matrix_world.translation
        L.append("moby %s %s %s %s %s" % (cls, num(p.x), num(p.y), num(p.z), num(yaw(o))))

    L.append("# the level's own classes: the spawn points (hidden by the plugin; the wave plugin finds them by name=),")
    L.append("# the flame vents (the plugin's: they burn in turns and hurt)")
    for name, mdl, opts, _ in CLASSES:
        L.append("class %s %s %s" % (name, mdl, opts))
    if rails:
        L.append("# the grind rails (rail_<name> curves): the plugin turns a challenge's on (rail_enable) and shows their looks")
    for name, centre, line, _ in rails:
        L.append(line)
        L.append("class %s %s.obj size 1 ghost" % (name, name))
        L.append("object %s %s %s %s 0 name=%s" % (name, num(centre.x), num(centre.y), num(centre.z), name))
    if rails:
        L.append("# the rails' collision strips (their own objects: the plugin makes a rail's solid only while it is on)")
    for _, _, _, strips in rails:
        for cls, c in strips:
            L.append("class %s %s.obj collision %s_col.obj size 1" % (cls, cls, cls))
            L.append("object %s %s %s %s 0 name=%s" % (cls, num(c.x), num(c.y), num(c.z), cls))
    spawns = [o for o in markers if o.name.startswith("spawn_")]
    for o in spawns:
        name = o.name[6:]
        p = o.matrix_world.translation
        L.append("object spawn %s %s %s %s name=%s kind=%s" % (num(p.x), num(p.y), num(p.z), num(yaw(o)), name, name.split("_")[0]))
    obstacles = [o for o in markers if base_name(o).startswith("mine_")]
    if obstacles:
        L.append("# the grind course's mines to jump (mine_<n> markers): the plugin's, hidden but in it")
    for o in obstacles:
        p = o.matrix_world.translation
        L.append("object %s %s %s %s %s name=%s" % (base_name(o).split("_")[0], num(p.x), num(p.y), num(p.z), num(yaw(o)),
                                                  base_name(o)))
    portals = [o for o in markers if base_name(o).startswith("portal_")]
    if portals:
        L.append("# the portals' rings (portal_<name> markers; the plugin's switch to a section as Ratchet goes through)")
    for o in portals:
        p = o.matrix_world.translation
        L.append("object portal %s %s %s %s name=%s" % (num(p.x), num(p.y), num(p.z), num(yaw(o)), base_name(o)))
    L.append("# the lava flooding the arena (La grande fuga): under the lava until the plugin raises it")
    L.append("object flood 0 0 %s 0 name=flood" % num(-2.5))
    flames = [o for o in markers if o.name.startswith("flame_")]
    for i, o in enumerate(flames):
        p = o.matrix_world.translation
        L.append("object flame %s %s %s %s name=%s delay=%d" % (num(p.x), num(p.y), num(p.z), num(yaw(o)), o.name[6:],
                                                              int(o.get("delay", i * 130))))
    for s in LEVEL["sounds"]:
        L.append("sound %s" % s)

    trigs = [o for o in markers if o.name.startswith("trigger_")]
    if trigs:
        L.append("# triggers (trigger_<name> cube empties): the plugin's trigger callback")
    for o in trigs:
        s = o.empty_display_size
        pts = [o.matrix_world @ Vector((x * s, y * s, z * s)) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
        a = [min(p[i] for p in pts) for i in range(3)]
        b = [max(p[i] for p in pts) for i in range(3)]
        L.append("trigger %s %s" % (o.name[8:], " ".join(num(c) for c in a + b)))
    for o in markers:
        n = base_name(o)
        if n not in ("hero", "ship") and not n.startswith(("obj_", "spawn_", "trigger_", "flame_", "mine_", "portal_")):
            warn("marker %s: not hero, ship, spawn_*, trigger_*, obj_*, flame_*, portal_*: left out" % o.name)
    if LEVEL["format"] >= 3:
        for name, at, a in LEVEL.get("entries", []):
            L.append("entry %s %s %s %s %s" % (name, num(at[0]), num(at[1]), num(at[2]), num(a)))
        if SECTIONS:
            L.append("# main's objects while Ratchet is in a section: kept as they are (the plugin holds them), not updated")
            L.append("leave freeze")
        for name, opts in SECTIONS:
            block, lines, n = export_section(level, name, opts, depsgraph, n_meshes + n_sec)
            L += block
            mat_lines += lines
            n_sec += n
    with open(os.path.join(level, "level.txt"), "w") as f:
        f.write("\n".join(L) + "\n")

    # lights.txt: the game's coordinates (the offset added here)
    LL = ["# the arena: the modern lighting's file (export_arena.py)", "# game coordinates: offset (%s %s %s) added" % tuple(num(c) for c in offset)]
    nlights = 0
    for o in sorted(coll("arena_lights"), key=lambda o: o.name):
        if o.type != "LIGHT":
            continue
        if o.data.type == "SUN":
            d = (o.matrix_world.to_3x3() @ Vector((0, 0, 1))).normalized()
            az = math.degrees(math.atan2(d.y, d.x))
            el = math.degrees(math.asin(max(-1.0, min(1.0, d.z))))
            if not 5.0 <= el <= 90.0:
                warn("%s: elevation %.1f clamped to 5..90" % (o.name, el))
                el = max(5.0, min(90.0, el))
            LL.append("# the sun from %s: Veldin's key light's direction, the sun its sky shows" % o.name)
            LL.append("sun %s %s" % (num(az), num(el)))
            continue
        p = o.matrix_world.translation + Vector(offset)
        c = o.data.color
        inten = float(o.get("intensity", 1.0))
        radius = float(o.get("radius", o.data.cutoff_distance if o.data.use_custom_distance else 10.0))
        LL.append("# %s" % o.name)
        LL.append("light %s %s %s  %.3f %.3f %.3f  %.3f %.3f %.3f 1" % (num(p.x), num(p.y), num(p.z), c[0], c[1], c[2],
                                                                    inten, radius, float(o.get("flicker", 0.0))))
        nlights += 1
    LL.append("# the materials of the meshes' textures (host mesh classes 0.. in meshes.obj's order, then the sections', then the classes' models)")
    LL += mat_lines
    for i, (name, _, _, specs) in enumerate(CLASSES):
        for t, spec in enumerate(specs):
            LL.append("# the %s class's model, its texture %d" % (name, t))
            LL.append("material tie %d tex %d %s" % (n_meshes + n_sec + i, t, spec))
    for j, (name, _, _, _) in enumerate(rails):
        LL.append("# the %s class's model (a rail)" % name)
        LL.append("material tie %d tex 0 %s" % (n_meshes + n_sec + len(CLASSES) + j, RAIL_SPEC))
    LL.append("# ambient particles: embers rising from the lava, a little ash")
    for text, bx in LEVEL["particles"]:
        if bx:
            g = [bx[i] + offset[i % 3] for i in range(6)]
            LL.append("particles %s  box %s" % (text, " ".join(num(c) for c in g)))
        else:
            LL.append("particles %s" % text)
    with open(os.path.join(level, "lights.txt"), "w") as f:
        f.write("\n".join(LL) + "\n")

    with open(os.path.join(out, "mod.json"), "w") as f:
        json.dump({"name": LEVEL["name"][""], "author": "Daino Selvatico", "version": "0.5",
                   "description": "An arena in a lava lake ringed by mountains, with challenges against Kalebo III's enemies "
                                  "(waves, bombs, flame vents, weapon limits, championships) and grind rails (a test rail, a "
                                  "course of three lanes with mines to jump and Kalebo III's rail mines to break, and an escape from the lava "
                                  "through a portal into another place).",
                   "kind": "level", "level": {"id": LEVEL["id"], "format": LEVEL["format"], "base": LEVEL["base"]}}, f, indent=2)
        f.write("\n")

    print("level.txt: hero, ship, %d mobys, %d spawn points, %d flame vents, %d triggers; lights.txt: the sun, %d lights, %d material lines" % (
        len(mobys), len(spawns), len(flames), len(trigs), nlights, sum(1 for l in mat_lines if not l.startswith("#"))))
    for w in warnings:
        print("warning:", w)
    for e in errors:
        print("ERROR:", e)
    if errors:
        print("not exported cleanly: %d errors" % len(errors))
        return False
    print("wrote", out)
    if install:
        dst = os.path.join(MODS, FOLDER)
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.copytree(out, dst)
        print("installed", dst)
    if bundle:  # the copy the game ships (bundled-mods/, tools/bundled_mods.py): commit it with the scripts' changes
        sys.path.insert(0, os.path.join(GAME, "tools"))
        import bundled_mods
        bundled_mods.refresh(FOLDER, out)
    return True


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ok = export(next((a for a in argv if not a.startswith("--")), None), "--install" in argv, "--bundle" in argv)
    if bpy.app.background and not ok:
        sys.exit(1)
