#!/usr/bin/env python3
"""docs/port-inventory.md from the scan (tools/inventory/port_inventory.py scan): the tables are counts, addresses, sizes
and our own descriptions (tools/inventory/notes.txt, the names of the ports); the prose around them is this file's."""
import collections
import csv
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
IMAGES = ["front"] + ["L%d" % i for i in range(19)]
LEVEL_NAMES = {"front": "boot module", "core": "core libraries", "L0": "Veldin", "L1": "Novalis", "L2": "Aridia",
               "L3": "Kerwan", "L4": "Eudora", "L5": "Rilgar", "L6": "Blarg (Nebula G34)", "L7": "Umbris",
               "L8": "Batalia", "L9": "Gaspar", "L10": "Orxon", "L11": "Pokitaru", "L12": "Hoven", "L13": "Gemlik Base",
               "L14": "Oltanis", "L15": "Quartu", "L16": "Kalebo III", "L17": "Drek's fleet", "L18": "Veldin (finale)"}
NOT_PORTED = {"todo", "unwired", "standin"}
SETTLED = {"host": "replaced by the host", "debug": "debug feature", "inline": "covered inline", "false": "not a function"}
PACKAGES = [
    ("P1", "The Metal Detector's HUD on every level",
     "Latent bug. The Metal Detector's on-screen display (HUD slot 7, the camera 0x753a) draws through a function each "
     "level carries in the same build; only Batalia and Drek's fleet register it. Once the player owns the Metal "
     "Detector, its display does not draw on the other planets: the tour's `detector` runs (the item given, the hero put by "
     "a buried-treasure marker) logged the TODO_PORT on nine levels. One port of Veldin's "
     "copy, translated, serves every level; the text box it calls (0x1fb470) comes with it. Playtest: with the Metal "
     "Detector, use it on Gaspar, Orxon, Pokitaru."),
    ("P2", "The particle types without a port",
     "In progress on the branch fix/particles-unported (17 types and the spawner 0x270758, regen group veldin_particles), "
     "with the two helpers they call. Every level's particle table lists them; the earlier fix showed that ported level "
     "code spawns some of them."),
    ("P3", "Menu widget functions outside the dispatch",
     "Widget functions that the game's page data names but the port's widget dispatch (pause_menu.cpp, "
     "pause_menu_widgets.cpp, end_menu.cpp) did not handle. Those of the pause menu's Goodies page (shown once the game "
     "is finished) are ported (pause_menu_special.cpp, batch 411); the levels' own copies of a help page's focus-in and "
     "of a label's update are proved the same as the Veldin ports that run in their place (batch 411); the title's "
     "pages that every overlay carries and two widgets no page lists are `unopened` (below)."),
    ("P4", "The levels' own builds the Veldin port stands in for",
     "Functions a level builds differently from Veldin, reached by direct calls from ported code that runs the Veldin "
     "port instead: each compared with the level's own copy by the per-level oracle (batch 410). Kerwan's rail search "
     "differs and has its own port (find_rail_l3_v); Aridia's and Kerwan's cutscene extras and Gemlik's hit record "
     "are proved the same as the Veldin ports (their annotations list the levels' addresses)."),
    ("P5", "Quitting to the title",
     "Quit from the pause menu ends the program instead of going back to the title: the boot data's read (0x243d38) "
     "and the boot module's restart are not ported (docs/status.md lists it)."),
    ("P6", "Level functions only data points to",
     "Small functions of single levels that only the level's data points to (class hooks, table entries) and that no "
     "run has met: port them with their tables when a playtest or an oracle subject reaches them."),
    ("P7", "The boot module's own leftovers",
     "Copies in the boot module (the title) of shared functions with other data, and its start-up steps the host "
     "replaces."),
    ("P8", "Registrations only",
     "Functions ported for the level where they run (or the same code ported for another level) whose other copies "
     "nothing reaches in practice: a registration closes them."),
    ("P9", "The rest: reached, not ported, not described yet",
     "Found by the call graph, without a note yet: read each one, describe it in tools/inventory/notes.txt, assign it "
     "a package."),
]


def load(out):
    R = list(csv.DictReader(open(out / "functions.csv")))
    E = list(csv.DictReader(open(out / "edges.csv")))
    T = list(csv.DictReader(open(out / "runtime_todo.csv"))) if (out / "runtime_todo.csv").exists() else []
    S = json.loads((out / "summary.json").read_text())
    return R, E, T, S


def read_notes():
    """[(image, lo, hi, tag, package, text)]"""
    out = []
    for line in (HERE / "notes.txt").read_text().split("\n"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        m = re.match(r"(\w+):([0-9a-fA-F]{8})(?:-([0-9a-fA-F]{8}))?\s+(\w+)\s+(?:(P\d)\s+)?(.*)", line)
        if not m:
            continue
        lo = int(m.group(2), 16)
        hi = int(m.group(3), 16) if m.group(3) else lo + 1
        out.append((m.group(1), lo, hi, m.group(4), m.group(5), m.group(6)))
    return out


def note_of(notes, rows, rep):
    """The note of a unique function: one naming the address of any of its copies (or, `V:`, of their Veldin
    counterpart), else a range note covering its representative copy."""
    for im, lo, hi, tag, pkg, text in notes:
        if hi != lo + 1:
            continue
        for r in rows:
            if (r["image"] == im or im == "V" and r["veldin"]) and int(r["addr" if im != "V" else "veldin"], 16) == lo:
                return tag, pkg, text
    for im, lo, hi, tag, pkg, text in notes:
        if hi != lo + 1 and rep["image"] == im and lo <= int(rep["addr"], 16) < hi:
            return tag, pkg, text
    return None


def reached(r):
    return not r["reach"].startswith("none")


def kb(n):
    return "%.1f KB" % (n / 1024.0) if n >= 1024 else "%d B" % n


def describe(r, ev):
    """Our automatic description of a function without a note."""
    bits = []
    if r["name"]:
        bits.append("`%s`" % r["name"])
    if r["role"]:
        bits.append("the update of " + r["role"])
    elif r["roots"]:
        bits.append("named by the " + r["roots"].replace("/", " / ") + " data")
    if r["file"]:
        bits.append("next to " + ("`%s`" % r["file"].lstrip("~")) if r["file"].startswith("~") else "in `%s`" % r["file"])
    return "; ".join(bits)


def unique_functions(R, notes):
    """The unique functions (every copy but the core's, grouped by body) and each one's state: (U, by_copy, ev)."""
    G = [r for r in R if r["image"] != "core"]
    by_uid = collections.defaultdict(list)
    for r in G:
        by_uid[r["uid"]].append(r)
    ev = collections.defaultdict(set)
    for r in R:
        if r["runtime"]:
            ev[r["image"] + ":" + r["addr"]].update(x.strip() for x in r["runtime"].split(";"))
    U = []
    for uid, rows in by_uid.items():
        rr = [r for r in rows if reached(r)]
        bad = [r for r in rr if r["status"] in NOT_PORTED]
        rep = next((r for r in (bad or rows) if r["image"] == "L0"), (bad or rows)[0])
        nt = note_of(notes, rows, rep)
        size = int(rep["size"])
        if not rr:
            state = "dead"
        elif not bad:
            state = "ported"
        else:
            tag = nt[0] if nt else None
            state = {"host": "host", "debug": "debug", "inline": "ported", "false": "false",
                     "inprogress": "inprogress", "unopened": "unopened"}.get(tag, "left")
        U.append(dict(uid=uid, rows=rows, rep=rep, bad=bad, note=nt, size=size, state=state,
                      ported_somewhere=any(r["status"] == "ported" for r in rows),
                      runtime=sorted(set().union(*[ev.get(r["image"] + ":" + r["addr"], set()) for r in rows]))))
    by_copy = {r["image"] + ":" + r["addr"]: u for u in U for r in u["rows"]}
    return U, by_copy, ev


def write(out, doc, a):
    R, E, T, S = load(out)
    notes = read_notes()
    core = [r for r in R if r["image"] == "core"]
    G = [r for r in R if r["image"] != "core"]
    fams = {r["family"] for r in G}
    U, by_copy, ev = unique_functions(R, notes)
    st = collections.Counter(u["state"] for u in U)
    stb = collections.Counter()
    for u in U:
        stb[u["state"]] += u["size"]

    L = []
    w = L.append
    w("# Port inventory")
    w("")
    w("*Generated by `tools/inventory/port_inventory.py doc` from the scan of the player's disc; do not edit by hand "
      "(the descriptions live in `tools/inventory/notes.txt`).*")
    w("")
    w("What is left to port, and what the game never runs. Every function of the boot executable's core, the boot "
      "module and the 19 level overlays is put in one of three groups: **ported** (the port runs it), **reached but not "
      "ported** (the game's code can get to it and the port has no port of it), **not connected** (nothing reaches it: "
      "cut content and dead code). Each level carries its own relinked copy of the engine, so the functions are counted "
      "both per copy and as **unique** functions (the same instructions, relocated fields compared through the overlay "
      "maps), the measure of the work.")
    w("")
    w("Every function one by one (its port's file, its oracle batches, its status), every moby class, and the host code "
      "that stands in for the original's hardware, libraries and kernel: the [Port catalog](catalog/README.md).")
    w("")
    w("## How to regenerate")
    w("")
    w("```bash")
    w("tools/inventory/port_inventory.py tour            # headless runs of every level (RAC1_MOBY_LIST, TODO_PORT); ~1 h")
    w("tools/inventory/port_inventory.py scan --runtime assets/generated/inventory/tour")
    w("tools/inventory/port_inventory.py doc             # this page and the catalog (docs/catalog/)")
    w("```")
    w("")
    w("The scan reads the disc through the generators' helpers (`tools/overlay_match`, `tools/codegen/ovlist.py`) and the "
      "repository's ports; it writes `assets/generated/inventory/functions.csv` (one row per copy: status, reach, the "
      "Veldin counterpart, the unique body, callers, runtime evidence) and `edges.csv` (the call graph), both gitignored. "
      "`--census` adds the class placement counts of a census CSV (the development repository's "
      "`moby_class_census.csv`).")
    w("")
    w("## Totals")
    w("")
    nrow = len(G)
    w("Without the core libraries (below): **%d function copies** in the boot module and the 19 overlays, **%d unique "
      "functions** (%d families: a function and its other builds). PAL: the region table lists %d functions the PAL "
      "build changed (they run as the North American port; see [Setup](setup.md#the-european-pal-disc))."
      % (nrow, len(U), len(fams), sum(S["pal_changed"].values())))
    w("")
    w("| Unique functions | Count | Bytes |")
    w("|---|---:|---:|")
    for k, label in [("ported", "ported (including the empty ones and those covered inline by their callers)"),
                     ("inprogress", "reached, not ported: in progress on another branch"),
                     ("left", "**reached, not ported: the work left**"),
                     ("host", "reached, replaced by host code on purpose (hardware, DMA, GS, kernel, movie library)"),
                     ("debug", "reached, debug features of the original (not turned on in the retail game)"),
                     ("unopened", "named only by menu data that no code opens there (the title's pages carried in the "
                                  "levels' overlays, widgets no page lists)"),
                     ("false", "not functions (data words that look like code pointers)"),
                     ("dead", "not connected: no copy reached (cut content, dead code)")]:
        w("| %s | %d | %s |" % (label, st[k], kb(stb[k])))
    w("")
    left = [u for u in U if u["state"] == "left"]
    w("Of the %d left, %d are only a missing wiring (the same code is ported for another level), %d are a level's own "
      "build that a Veldin port stands in for, %d have no port at all."
      % (len(left), sum(1 for u in left if all(r["status"] == "unwired" for r in u["bad"])),
         sum(1 for u in left if all(r["status"] == "standin" for r in u["bad"])),
         sum(1 for u in left if any(r["status"] == "todo" for r in u["bad"]))))
    w("")
    w("### Per image")
    w("")
    w("Per copy: an image's functions, the share ported, the copies reached and not ported (all kinds: the host's and "
      "the debug ones included), the copies nothing on that image reaches (many are reached on another level).")
    w("")
    w("| Image | Functions | Ported | Reached, not ported | Not connected here | PAL-changed functions |")
    w("|---|---:|---:|---:|---:|---:|")
    for im in ["core"] + IMAGES:
        rows = [r for r in R if r["image"] == im]
        if not rows:
            continue
        p = sum(1 for r in rows if r["status"] in ("ported", "empty"))
        rn = [r for r in rows if reached(r) and r["status"] in NOT_PORTED]
        nc = sum(1 for r in rows if not reached(r))
        pal = S["pal_changed"].get(im[1:] if im.startswith("L") else im, "")
        w("| %s %s | %d | %d (%.0f%%) | %d (%s) | %d | %s |" % (im, LEVEL_NAMES[im], len(rows), p, 100.0 * p / len(rows),
                                                             len(rn), kb(sum(int(r["size"]) for r in rn)), nc, pal))
    w("")

    # latent bugs
    w("## Latent bugs: reached in a normal playthrough, not ported")
    w("")
    w("These are the ones a player can hit (as the particle updates were). The runtime evidence comes from the headless "
      "tour (five runs per level: walking, the wrench, firing, the pause menu's pages, the Metal Detector near a buried "
      "treasure; the title's menus). The levels' starts only: a level whose arrival scene outlasts the run's script "
      "(Novalis, Umbris, Batalia, Hoven, Gemlik, Oltanis) did not open its pause menu.")
    w("")
    lat = [u for u in U if u["note"] and u["note"][0] == "latent" and u["state"] == "left"]
    lat += [u for u in U if u["state"] == "left" and u not in lat and u["runtime"]]
    if not lat:
        w("None: every function the tour met or a note marks as latent is ported.")
    else:
        w("| Function | Size | Where | What | Seen in the tour |")
        w("|---|---:|---|---|---|")
        for u in sorted(lat, key=lambda u: (u["note"][1] if u["note"] and u["note"][1] else "P9", u["rep"]["addr"])):
            w(row_of(u, ev))
    w("")
    w("The [moby] and [particle] lines of RAC1_MOBY_LIST and the TODO_PORT lines the tour logged are in "
      "`assets/generated/inventory/runtime_todo.csv`; the live mobys with no port, per level:")
    w("")
    live = collections.defaultdict(set)
    for r in R:
        for x in (r["runtime"] or "").split(";"):
            x = x.strip()
            if x.startswith("live moby"):
                live[r["image"]].add("%s (%s)" % (x.replace("live moby of class ", "class "), r["addr"]))
    if live:
        for im in IMAGES:
            if live.get(im):
                w("- %s %s: %s" % (im, LEVEL_NAMES[im], ", ".join(sorted(live[im]))))
    else:
        w("- none: every live moby of the tour's levels has a port.")
    w("")
    todo_sites = collections.defaultdict(set)
    for t in T:
        todo_sites[(t["addr"], t["name"])].add(t["level"])
    host = {"122298", "2a08c8", "1187b0", "1187d0", "200f18", "1221b0", "24fad0", "250038", "295d48", "295c50",
            "295bf8", "23d968", "1f9f78", "28b440", "2648e0"}
    w("TODO_PORT lines of the tour other than the host's own stubs (vsync, DMA, display packet, the VU1 passes the MRP "
      "replaces):")
    w("")
    any_t = False
    for (addr, name), lv in sorted(todo_sites.items()):
        if addr.lstrip("0") in host:
            continue
        any_t = True
        what = ""
        for l in lv:
            u = by_copy.get(l + ":" + addr)
            if u and u["note"]:
                what = ": " + u["note"][2].split(":")[0]
                break
        w("- `0x%s` (%s): %s%s" % (addr.lstrip("0"), name, ", ".join(sorted(lv, key=lambda x: (len(x), x))), what))
    if not any_t:
        w("- none")
    w("")

    # packages
    w("## The work left, in packages")
    w("")
    w("Ordered by what a player meets first. Each row is one unique function (its Veldin copy when it has one; "
      "`images` lists where a copy is reached and not ported).")
    w("")
    pk = collections.defaultdict(list)
    for u in U:
        if u["state"] not in ("left", "inprogress"):
            continue
        p = u["note"][1] if u["note"] and u["note"][1] else "P9"
        pk[p].append(u)
    for pid, title, text in PACKAGES:
        us = pk.get(pid, [])
        if not us:
            continue
        w("### %s. %s (%d functions, %s)" % (pid, title, len(us), kb(sum(u["size"] for u in us))))
        w("")
        w(text)
        w("")
        w("| Function | Size | Where | What | Seen in the tour |")
        w("|---|---:|---|---|---|")
        for u in sorted(us, key=lambda u: (u["rep"]["image"] != "L0", u["rep"]["image"], u["rep"]["addr"])):
            w(row_of(u, ev))
        w("")

    # replaced / debug
    w("## Reached, not to port")
    w("")
    w("Replaced by host code or the runtime layer on purpose, development features of the original that the retail "
      "game never turns on (a port of them would be an optional extra, see [Enhancements](enhancements.md)), or named "
      "only by menu data that nothing opens where they are (`unopened`: checked by following the menu's pages from the "
      "states the code opens).")
    w("")
    w("| Function | Size | Copies | Kind | What |")
    w("|---|---:|---:|---|---|")
    groups = collections.OrderedDict()
    for u in sorted([u for u in U if u["state"] in ("host", "debug", "unopened")], key=lambda u: (u["rep"]["image"], u["rep"]["addr"])):
        key = u["note"][2] if u["note"] else ""
        groups.setdefault((u["state"], key), []).append(u)
    for (state, key), us in groups.items():
        rep = us[0]["rep"]
        addr = "%s:0x%s" % (rep["image"], rep["addr"]) + (" .. 0x%s (%d functions)" % (us[-1]["rep"]["addr"], len(us))
                                                         if len(us) > 1 else "")
        w("| %s | %s | %d | %s | %s |" % (addr, kb(sum(u["size"] for u in us)), max(len(u["rows"]) for u in us), state, key))
    w("")

    # core
    w("## The core libraries")
    w("")
    reached_core = [r for r in core if reached(r)]
    pc = [r for r in core if r["status"] in ("ported", "empty")]
    w("The boot executable's core (0x112380..0x12f438: the C library, libgcc's soft float, libcdvd, libgraph, libmc, "
      "libvu0, libpad and the SIF / IOP clients, the 989snd client, the game's `main`): %d functions, %d reached. The "
      "port does not port these function by function: the runtime layer models the hardware and the IOP behind them "
      "(`runtime/`), and the game code's calls into them go to C++ equivalents (`src/game/lib`) or to TODO_PORT stubs "
      "the host does not need. %d are ported as such (annotated)."
      % (len(core), len(reached_core), len(pc)))
    w("")
    libs = collections.Counter()
    libb = collections.Counter()
    for r in reached_core:
        if r["status"] in ("ported", "empty"):
            continue
        f = r["file"].lstrip("~").replace("src/game/lib/", "") or "?"
        libs[f] += 1
        libb[f] += int(r["size"])
    w("| Near the port of | Reached, not ported | Bytes |")
    w("|---|---:|---:|")
    for f, n in libs.most_common():
        w("| `%s` | %d | %s |" % (f, n, kb(libb[f])))
    w("")

    # not connected
    dead = [u for u in U if u["state"] == "dead"]
    w("## Not connected: cut content and dead code")
    w("")
    nore = [u for u in dead if all(r["reach"] == "none:noref" for r in u["rows"])]
    w("%d unique functions (%s) that no copy reaches: %d with **no reference found** anywhere (no call, no code-built "
      "pointer, no data word), %d **referenced only by unused code**. Most are a few words: stubs, tiny accessors "
      "and the remains of removed features. The confidence is the call graph's: a function reached through an address "
      "computed in a way the scan does not follow (a table walked with an offset, a pointer built in two functions) "
      "would look unconnected; the tour's evidence can only add reached functions."
      % (len(dead), kb(sum(u["size"] for u in dead)), len(nore), len(dead) - len(nore)))
    w("")
    w("| Function | Size | Copies | Confidence | What |")
    w("|---|---:|---:|---|---|")
    for u in sorted(dead, key=lambda u: -u["size"]):
        if u["size"] < 64 and not u["note"]:
            continue
        rep = u["rep"]
        conf = "no reference" if u in nore else "only from unused code"
        what = u["note"][2] if u["note"] else describe(rep, ev)
        if (rep["status"] == "ported" or u["ported_somewhere"]) and "ported anyway" not in what:
            what += " (ported anyway)"
        w("| %s:0x%s | %s | %d | %s | %s |" % (rep["image"], rep["addr"], kb(u["size"]), len(u["rows"]), conf, what))
    small = [u for u in dead if u["size"] < 64 and not u["note"]]
    w("| (%d smaller ones) | %s | | | |" % (len(small), kb(sum(u["size"] for u in small))))
    w("")
    w(KNOWN_CUT)
    w("")
    w(METHOD)
    doc.write_text("\n".join(L) + "\n")
    print("[port_inventory] wrote %s" % doc)


def row_of(u, ev):
    rep = u["rep"]
    ims = sorted({r["image"] for r in u["bad"]} or {r["image"] for r in u["rows"]}, key=lambda x: (len(x), x))
    where = ", ".join(ims) if len(ims) <= 4 else "%d images" % len(ims)
    what = u["note"][2] if u["note"] else describe(rep, ev)
    seen = "; ".join(sorted({x.split(" from ")[0] for x in u["runtime"]})) or ""
    return "| %s:0x%s | %s | %s | %s | %s |" % (rep["image"], rep["addr"], kb(u["size"]), where, what, seen)


KNOWN_CUT = """### What the development repository already knew

From `docs/reversing/cut_content.md` of the development repository (the details and the evidence are there), in short:

- **Kerwan's citizens "Fred 1 / 2 / 3"** (classes 915..917, 14 placements): their shared update (`L3:0x2dc310`) computes the
  distance and the angle to Ratchet, throws them away and deletes the moby on its first frame (ported as such). On Orxon
  class 915 is placed 5 times and only turns towards the hero.
- **A table of movie file names** in the boot module's data (114 rows, most a placeholder): the game plays its movies from
  the table of contents' sectors; no code builds the table's address.
- **Veldin's ship scenes for the other planets**: Veldin's data holds ten ship scenes, the game's control flow plays four;
  the landing plays only through the planet map's "same planet" case (`RAC1_ARRIVE=1`), its camera never authored to be seen.
  Level 0 also has a talker slot without a script.
- **Items whose hand / pickup class no level registers**: eight rows of the item table (0x179ac0) name classes absent from
  every level's class table; the Drone Device's class runs no code anywhere.
- **Classes with no code, empty, or never placed**, level by level (Novalis, Aridia, Eudora, Kerwan, Rilgar, Nebula G34,
  Umbris, Gaspar, Batalia, Orxon, Pokitaru): the Hologuise rows (483, 634) hold no function on every level; empty updates
  (a bare `jr ra`) such as the Green Ship (531), the "Lamp (Damaged)" replacement models, the hoverboard boost pads (they do
  nothing on their own); Eudora's self-deleting Transport Ships (484, as Fred); classes with code that nothing places or
  spawns (Aridia's Sand Mouse 437 and Sandshark Egg 755, Eudora's 161 and 437, Nebula G34's 156 / 1085..1089 / 1633,
  Umbris's 848 / 1221 / 1222, Pokitaru's Jet Fighter health crate 1220 and others).
- **The oxygen element on Aridia and Nebula G34**: both overlays carry their own build of the oxygen indicator, which
  nothing requests on those levels.
- **Levels' cutscene extras**: most levels' `cutscene_level_extras` is a bare `jr ra`.

The class-level findings (classes never placed) are not visible to the call graph, which sees every class table row as a
reference: they need the placements (`--census`, or the per-level oracle batches' `B<n>_INFO=1`). The functions this page
lists as not connected are the code-level part, new with this scan; the biggest are described in the table above."""

METHOD = """## Method and limits

- **Functions**: the matcher's starts (jal targets, code pointers in the data and in the code, prologues after a return),
  the ports' and maps' addresses, prologues after nop padding, then each function's end as the code generators' listings
  find it (the last `jr ra` no branch, `j` or jump table goes past) and the unreferenced code after it as functions of
  their own. A jump table's targets are case blocks, not functions.
- **Reach**: from each image's entry by calls (`entry`); then the tables (overlay sections 3 / 4 / 5: class updates and
  hooks, camera types, sound handlers; the particle table) and the code pointers the reached code builds (`pointer`); then
  any other data word that is a function's start (`data`: menus' widgets, state tables). A class row counts as reached
  even when the level never places the class.
- **Ported**: an `// orig:` annotation (src/game and the generated code), a registration with a literal address, a
  `case` of a dispatch switch on a code pointer, a variants row, a sky row; a copy the overlay matcher placed, or that the
  runtime's translation (`ovl_code_to_veldin`: the maps, the variants rows, an address inside a mapped function, the code
  pointer pairs) maps to a ported Veldin function with the same body. A level's copy whose body differs is a **stand-in**
  (the Veldin port runs in its place); a body ported for another level but not wired here is **unwired**; a copy only
  called directly by ported code shares its callers' port.
- **Unique functions**: the same instructions with the relocated fields masked, the addresses they build compared
  through the overlay maps (an address of the level's own matches anything, two different words of one image never
  match). Families also join a function's builds that differ (the stand-ins, the variants rows).
- **Limits**: partial ports (an annotation of a block) count as ported; the host's replacements are known only through
  the notes; the tour's runs are short and cover the levels' starts, not the planets' whole maps; the core libraries are
  summarised, not classified one by one."""


if __name__ == "__main__":
    import sys
    sys.path.insert(0, str(HERE))
    import port_inventory
    port_inventory.main()
