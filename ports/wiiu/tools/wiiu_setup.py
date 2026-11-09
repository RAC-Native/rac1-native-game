#!/usr/bin/env python3
"""The Wii U's setup (tools/setup.py --wiiu, non-interactive): from a game folder whose PC setup has run, builds the Wii U
game (ports/wiiu/game: devkitPPC + wut, Release; its shaders with CafeGLSL's compiler when there is one, else the .gsh
files of an earlier build) and stages the whole SD card tree in a folder, laid out as the console reads it:

  wiiu/apps/rac1-native/rac1-native.rpx           the game
  rac1-native/run.cfg                             the settings of a run (a default; the console's own is kept)
  rac1-native/shaders/*.gsh                       the picture's shaders
  rac1-native/assets/disc/...                     the disc's files: toc/ in sub-folders by the name's first two hex
                                                  digits (sd_disc_layout.py), each level's hdr_ files in its hdr.pack
                                                  (sd_level_packs.py) and their index levels/hdr_packs.txt
  rac1-native/assets/generated/mrp/...            the MRP export: each moby folder's classes in its files.pack
                                                  (sd_mrp_packs.py)
  rac1-native/assets/generated/overlay_map/...
  rac1-native/assets/dumps/<boot executable>

A file a pack holds is not staged on its own (the game reads it from the pack). The tree is made of hard links to
assets/ and the build where the file system allows it (copies otherwise), and kept in step: run again, it changes only
what changed and removes what is no longer part of it. Then a manifest of it, one line per file:

  <sha1>\\t<size>\\t<component>\\t<path on the card>

(the disc's files take their SHA-1 from assets/disc/layout.txt; the others are hashed, remembered by size and mtime).
Components: game, shaders, config, core, level/<n>, movies (toc's .pss files) - tools/wiiu_backend.py sends them in that
order.

    python3 tools/setup.py --wiiu [--build-dir build-wiiu] [--sd DIR] [-j N] [--no-build] [--json]
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
import sd_level_packs  # noqa: E402
import sd_mrp_packs  # noqa: E402

ASSETS = ROOT / "assets"
DISC = ASSETS / "disc"
MRP = ASSETS / "generated/mrp"
OVERLAY = ASSETS / "generated/overlay_map"
DUMPS = ASSETS / "dumps"
BOOT_ELFS = ("SCUS_971.99", "SCES_509.16")
MANIFEST_HEAD = "# rac1-native Wii U SD manifest 1"
CARD_GAME = "rac1-native"  # the game's folder on the card
RPX_ON_CARD = "wiiu/apps/rac1-native/rac1-native.rpx"
COMPONENT_ORDER = ("game", "shaders", "config", "core", "level", "movies")

# The console's run.cfg as it is today (2026-11-09): sent only when the card has none (wiiu_backend.py).
DEFAULT_RUN_CFG = """# rac1-native on the Wii U: the RAC1_* settings of a run (docs/env-vars.md)
# (to play: the picture on the TV and the GamePad, the pads, no frame limit; the MRP-lite settings are the defaults)
RAC1_WIIU_GFX=1
RAC1_REALTIME=1
# (0: no frame limit; the Wii U run is headless, whose default is 300 frames)
RAC1_FRAMES=0
"""


class SetupError(Exception):
    pass


class Report:
    """Messages for a person, or JSON lines ({"event": ...}) for the launcher's Wii U page with --json."""

    def __init__(self, json_mode):
        self.json = json_mode

    def event(self, kind, **kw):
        if self.json:
            print(json.dumps(dict(kw, event=kind)), flush=True)

    def step(self, n, total, msg):
        if self.json:
            self.event("step", n=n, total=total, text=msg)
        else:
            print("\n[%d/%d] %s" % (n, total, msg), flush=True)

    def info(self, msg):
        if self.json:
            self.event("info", text=msg)
        else:
            print("  " + msg, flush=True)


# ------------------------------------------------------------------------------------------------------------------
# Checks

def check_pc_setup(pc_build):
    """What the Wii U build and the SD tree need from the PC setup; the boot executable's name."""
    need = [
        (DISC / "layout.txt", "the disc's files (./setup.sh extracts them)"),
        (ASSETS / "generated/src/generated_sources.cmake", "the generated code (./setup.sh)"),
        (MRP / "moby", "the MRP export (./setup.sh, 'Generating the game's data')"),
        (OVERLAY, "the overlay maps (./setup.sh)"),
        (pc_build / "_deps", "the PC build (./setup.sh builds it; the Wii U build uses its downloaded dependencies)"),
    ]
    missing = [(p, why) for p, why in need if not p.exists()]
    if missing:
        raise SetupError("the PC setup has not run (or not to the end) in %s:\n%s" % (
            ROOT, "\n".join("    %s: %s" % (p, why) for p, why in missing)))
    elfs = [n for n in BOOT_ELFS if (DUMPS / n).is_file()]
    if len(elfs) != 1:
        raise SetupError("%s: expected one boot executable (%s), found %s" % (DUMPS, " or ".join(BOOT_ELFS),
                                                                             ", ".join(elfs) or "none"))
    return elfs[0]


def devkitpro():
    base = Path(os.environ.get("DEVKITPRO", "/opt/devkitpro"))
    wrapper = base / "portlibs/wiiu/bin/powerpc-eabi-cmake"
    if not wrapper.is_file() or not (base / "wut").is_dir():
        raise SetupError("devkitPro with wut not found in %s (install devkitPPC and wut: https://devkitpro.org/wiki/"
                         "Getting_Started; DEVKITPRO names another place)" % base)
    return wrapper


def cached(build_dir, key):
    try:
        for line in (build_dir / "CMakeCache.txt").read_text().splitlines():
            if line.startswith(key + ":"):
                return line.split("=", 1)[1]
    except OSError:
        pass
    return None


def find_glslc(arg, build_dir):
    """CafeGLSL's glslcompiler: --glslc, RAC1_WIIU_GLSLC, the build's cache, another Wii U build's of this folder
    (build-wiiu*), or on the PATH. None if there is none."""
    others = [cached(d, "RAC1_WIIU_GLSLC") for d in sorted(ROOT.glob("build-wiiu*")) if d != build_dir]
    for c in [arg, os.environ.get("RAC1_WIIU_GLSLC"), cached(build_dir, "RAC1_WIIU_GLSLC")] + others + \
            [shutil.which("glslcompiler")]:
        if c and Path(c).is_file() and os.access(c, os.X_OK):
            return str(Path(c).resolve())
    return None


# ------------------------------------------------------------------------------------------------------------------
# The build

def build_game(rep, build_dir, pc_build, glslc, jobs):
    wrapper = devkitpro()
    log = build_dir / "wiiu-build.log"
    build_dir.mkdir(parents=True, exist_ok=True)
    want = {"CMAKE_BUILD_TYPE": "Release", "RAC1_PC_BUILD": str(pc_build),
            "RAC1_GENERATED_SRC": str(ASSETS / "generated/src"), "RAC1_WIIU_GLSLC": glslc or ""}
    if any((cached(build_dir, k) or "") != v for k, v in want.items()):
        rep.info("configuring %s" % build_dir)
        cmd = [str(wrapper), "-S", str(ROOT / "ports/wiiu/game"), "-B", str(build_dir)] + \
              ["-D%s=%s" % kv for kv in want.items()]
        run_logged(rep, cmd, log, "the Wii U build's configuration")
    rep.info("building (%d jobs; the log: %s)" % (jobs, log))
    t0 = time.monotonic()
    run_logged(rep, ["cmake", "--build", str(build_dir), "-j", str(jobs)], log, "the Wii U build", progress=True)
    rep.info("built in %.0f s" % (time.monotonic() - t0))


def run_logged(rep, cmd, log, what, progress=False):
    with open(log, "a") as f:
        f.write("\n$ %s\n" % " ".join(cmd))
        f.flush()
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
        last = 0.0
        for line in p.stdout:
            f.write(line)
            if progress and line.startswith("[") and "%]" in line[:7]:
                now = time.monotonic()
                if now - last > (2.0 if rep.json else 10.0):
                    last = now
                    pct = int(line[1:4].strip() or 0)
                    if rep.json:
                        rep.event("build", percent=pct)
                    else:
                        rep.info("%3d%%" % pct)
        rc = p.wait()
    if rc:
        tail = log.read_text(errors="replace").splitlines()[-25:]
        raise SetupError("%s failed (exit %d); the end of %s:\n%s" % (what, rc, log, "\n".join(tail)))


# ------------------------------------------------------------------------------------------------------------------
# The packs (made in <build>/packs, again only when their files changed)

def stamp_of(src, names):
    return [[n, *(lambda st: (st.st_size, st.st_mtime_ns))((src / n).stat())] for n in names]


def make_pack(pack_fn, src, dst, names, outputs):
    stamp = stamp_of(src, names)
    sf = dst / ".stamp.json"
    try:
        if json.loads(sf.read_text()) == stamp and all((dst / o).is_file() for o in outputs):
            return False
    except (OSError, ValueError):
        pass
    pack_fn(str(src), str(dst))
    sf.write_text(json.dumps(stamp))
    return True


def level_packs(rep, packs):
    """Each level's hdr_ files in its hdr.pack (sd_level_packs.py): {level: set of packed names}; levels/hdr_packs.txt."""
    packed = {}
    made = 0
    out = packs / "levels"
    for lv in sorted(d.name for d in (DISC / "levels").iterdir() if d.is_dir() and d.name.startswith("level")):
        src = DISC / "levels" / lv
        names = sorted(n.name for n in src.iterdir() if n.name.startswith("hdr_") and n.is_file())
        if not names:
            continue
        made += make_pack(sd_level_packs.pack, src, out / lv, names, ("hdr.pack", "hdr.pack.txt"))
        packed[lv] = set(names)
    lines = ["rac1-native hdr packs 1\n"]
    for lv in sorted(packed):
        lines += [line.replace("{lv}", lv) for line in open(out / lv / "hdr.pack.txt")]
    index = out / "hdr_packs.txt"
    text = "".join(lines)
    if not index.is_file() or index.read_text() != text:
        index.write_text(text)
    rep.info("levels' sound packs: %d levels (%d made again)" % (len(packed), made))
    return packed


def mrp_packs(rep, packs):
    """Each moby folder's classes in its files.pack (sd_mrp_packs.py): {folder: set of packed names}."""
    packed = {}
    made = 0
    for d in sorted(p for p in (MRP / "moby").iterdir() if p.is_dir()):
        names = sorted(n.name for n in d.iterdir() if n.name.endswith(".mrpm") and n.is_file())
        if not names:
            continue
        made += make_pack(sd_mrp_packs.pack, d, packs / "mrp" / d.name, names, ("files.pack", "files.pack.txt"))
        packed[d.name] = set(names)
    rep.info("MRP moby packs: %d folders (%d made again)" % (len(packed), made))
    return packed


# ------------------------------------------------------------------------------------------------------------------
# The tree

def level_of(name):
    """'level05' / 'level5' -> 5; None for anything else (title, armor, flight)."""
    if name.startswith("level") and name[5:].isdigit():
        return int(name[5:])
    return None


def disc_card_path(rel):
    """A disc file's path under assets/disc on the card (runtime/kernel/disc.cpp files_host_path, the Wii U's)."""
    if len(rel) > 10 and rel.startswith("toc/row_"):
        return "toc/%s/%s" % (rel[8:10], rel[4:])
    return rel


def tree_files(build_dir, packs, boot_elf, shaders_dir, packed_hdr, packed_mrp):
    """{path on the card: (source file, component)}."""
    files = {}
    files[RPX_ON_CARD] = (build_dir / "rac1_wiiu.rpx", "game")
    files[CARD_GAME + "/run.cfg"] = (build_dir / "run.cfg", "config")
    for g in sorted(shaders_dir.glob("*.gsh")):
        files[CARD_GAME + "/shaders/" + g.name] = (g, "shaders")
    disc_on_card = CARD_GAME + "/assets/disc/"
    for dirpath, dirnames, names in os.walk(DISC):
        dirnames.sort()
        for n in sorted(names):
            src = Path(dirpath) / n
            rel = src.relative_to(DISC).as_posix()
            parts = rel.split("/")
            comp = "core"
            if parts[0] == "levels" and len(parts) == 3:
                if n in packed_hdr.get(parts[1], ()):
                    continue  # (in the level's hdr.pack)
                if level_of(parts[1]) is not None:
                    comp = "level/%d" % level_of(parts[1])
            elif parts[0] == "toc" and n.endswith(".pss"):
                comp = "movies"
            elif n.startswith(".") or n.endswith((".tmp", ".part")):
                continue
            files[disc_on_card + disc_card_path(rel)] = (src, comp)
    for lv in sorted(packed_hdr):
        files[disc_on_card + "levels/%s/hdr.pack" % lv] = (packs / "levels" / lv / "hdr.pack", "level/%d" % level_of(lv))
    if packed_hdr:
        files[disc_on_card + "levels/hdr_packs.txt"] = (packs / "levels/hdr_packs.txt", "core")
    mrp_on_card = CARD_GAME + "/assets/generated/mrp/"
    for dirpath, dirnames, names in os.walk(MRP):
        dirnames.sort()
        for n in sorted(names):
            src = Path(dirpath) / n
            rel = src.relative_to(MRP).as_posix()
            parts = rel.split("/")
            if n.startswith(".") or n.endswith((".tmp", ".part")):
                continue
            if parts[0] == "moby" and len(parts) == 3 and n in packed_mrp.get(parts[1], ()):
                continue  # (in the folder's files.pack)
            lv = level_of(parts[1]) if len(parts) >= 3 else None
            files[mrp_on_card + rel] = (src, "level/%d" % lv if lv is not None else "core")
    for folder in sorted(packed_mrp):
        lv = level_of(folder)
        comp = "level/%d" % lv if lv is not None else "core"
        for o in ("files.pack", "files.pack.txt"):
            files[mrp_on_card + "moby/%s/%s" % (folder, o)] = (packs / "mrp" / folder / o, comp)
    for p in sorted(OVERLAY.iterdir()):
        if p.is_file():
            files[CARD_GAME + "/assets/generated/overlay_map/" + p.name] = (p, "core")
    files[CARD_GAME + "/assets/dumps/" + boot_elf] = (DUMPS / boot_elf, "core")
    return files


def stage(rep, files, sd, link=True):
    """Makes `sd` hold exactly `files` (hard links, or copies); removes the rest. Returns (linked, copied, kept, removed)."""
    sd.mkdir(parents=True, exist_ok=True)
    counts = {"linked": 0, "copied": 0, "kept": 0, "removed": 0}
    for rel, (src, _comp) in files.items():
        dst = sd / rel
        s = src.stat()
        try:
            d = dst.stat()
            if (d.st_ino == s.st_ino and d.st_dev == s.st_dev) or \
                    (not link and d.st_size == s.st_size and d.st_mtime_ns == s.st_mtime_ns):
                counts["kept"] += 1
                continue
            dst.unlink()
        except FileNotFoundError:
            pass
        dst.parent.mkdir(parents=True, exist_ok=True)
        if link:
            try:
                os.link(src, dst)
                counts["linked"] += 1
                continue
            except OSError:
                link = False  # (another file system, or none: copies from now on)
        shutil.copy2(src, dst)
        counts["copied"] += 1
    want = {sd / rel for rel in files}
    for dirpath, dirnames, names in os.walk(sd, topdown=False):
        for n in names:
            p = Path(dirpath) / n
            if p not in want:
                p.unlink()
                counts["removed"] += 1
        if dirpath != str(sd) and not os.listdir(dirpath):
            os.rmdir(dirpath)
    return counts


# ------------------------------------------------------------------------------------------------------------------
# The manifest

def layout_sha1s():
    """{disc path: (size, sha1)} for the files layout.txt describes whole (one piece from offset 0)."""
    pieces = {}
    with open(DISC / "layout.txt") as f:
        for line in f:
            if line.startswith(("#", "rac1-native", "image")) or not line.strip():
                continue
            first, size, off, sha, path = line.rstrip("\n").split(" ", 4)
            pieces.setdefault(path, []).append((int(off), int(size), sha))
    return {p: (v[0][1], v[0][2]) for p, v in pieces.items() if len(v) == 1 and v[0][0] == 0}


def sha1_of(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(4 << 20), b""):
            h.update(b)
    return h.hexdigest()


def write_manifest(rep, files, manifest, cache_path):
    from_layout = layout_sha1s()
    try:
        cache = json.loads(cache_path.read_text())
    except (OSError, ValueError):
        cache = {}
    new_cache = {}
    disc_on_card = CARD_GAME + "/assets/disc/"
    rows = []
    hashed = 0
    t0 = time.monotonic()
    for rel in sorted(files, key=lambda r: (order_key(files[r][1]), r)):
        src, comp = files[rel]
        st = src.stat()
        key = str(src)
        sha = None
        if rel.startswith(disc_on_card) and str(src).startswith(str(DISC) + os.sep):
            drel = src.relative_to(DISC).as_posix()
            if drel in from_layout and from_layout[drel][0] == st.st_size:
                sha = from_layout[drel][1]
        c = cache.get(key)
        if sha is None and c and c[0] == st.st_size and c[1] == st.st_mtime_ns:
            sha = c[2]
        if sha is None:
            sha = sha1_of(src)
            hashed += st.st_size
        new_cache[key] = [st.st_size, st.st_mtime_ns, sha]
        rows.append("%s\t%d\t%s\t%s\n" % (sha, st.st_size, comp, rel))
    tmp = manifest.with_suffix(".tmp")
    with open(tmp, "w") as f:
        f.write(MANIFEST_HEAD + "\n")
        f.write("# made %s from %s; sha1, size, component, path on the card\n" % (
            time.strftime("%Y-%m-%d %H:%M:%S"), ROOT))
        state = ASSETS / "setup-state.json"
        if state.is_file():
            f.write("# setup-state %s\n" % hashlib.sha1(state.read_bytes()).hexdigest())
        f.writelines(rows)
    os.replace(tmp, manifest)
    cache_path.write_text(json.dumps(new_cache))
    rep.info("manifest: %d files, %.1f MiB hashed in %.1f s (the rest from layout.txt and the cache)" % (
        len(rows), hashed / 2**20, time.monotonic() - t0))


def order_key(comp):
    head = comp.split("/")[0]
    n = int(comp.split("/")[1]) if "/" in comp else 0
    return (COMPONENT_ORDER.index(head) if head in COMPONENT_ORDER else len(COMPONENT_ORDER), n)


def read_manifest(path):
    """[(path on the card, size, sha1, component)] in the manifest's order (game, shaders, config, core, levels, movies)."""
    out = []
    with open(path) as f:
        if f.readline().rstrip("\n") != MANIFEST_HEAD:
            raise SetupError("%s is not a Wii U SD manifest" % path)
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            sha, size, comp, rel = line.rstrip("\n").split("\t", 3)
            out.append((rel, int(size), sha, comp))
    return out


# ------------------------------------------------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(prog="setup.py --wiiu", description=__doc__.split("\n\n")[0])
    ap.add_argument("--build-dir", default=str(ROOT / "build-wiiu"), help="the Wii U build (default build-wiiu)")
    ap.add_argument("--sd", help="where to stage the SD card's tree (default <build dir>/sd)")
    ap.add_argument("--pc-build", default=str(ROOT / "build"), help="the PC build (default build)")
    ap.add_argument("--glslc", help="CafeGLSL's glslcompiler (default: RAC1_WIIU_GLSLC, the build's, or on the PATH)")
    ap.add_argument("--shaders-from", help="without CafeGLSL: a folder of .gsh files to use (default: the build's, from "
                                           "an earlier build with CafeGLSL)")
    ap.add_argument("-j", "--jobs", type=int, default=max(1, min(4, os.cpu_count() or 1)), help="parallel jobs "
                    "(default 4: the generated code takes ~2 GB a job)")
    ap.add_argument("--no-build", action="store_true", help="stage the tree from the existing build")
    ap.add_argument("--copy", action="store_true", help="copy the files into the tree instead of hard links")
    ap.add_argument("--json", action="store_true", help="JSON lines on stdout (for the launcher)")
    args = ap.parse_args(argv)
    rep = Report(args.json)
    build_dir = Path(args.build_dir).expanduser().resolve()
    sd = Path(args.sd).expanduser().resolve() if args.sd else build_dir / "sd"
    pc_build = Path(args.pc_build).expanduser().resolve()
    t_all = time.monotonic()
    try:
        total = 5
        rep.step(1, total, "Checking the PC setup")
        boot_elf = check_pc_setup(pc_build)
        rep.info("game folder %s, boot executable %s" % (ROOT, boot_elf))
        rep.step(2, total, "Building the Wii U game")
        glslc = find_glslc(args.glslc, build_dir)
        rep.info("CafeGLSL: %s" % (glslc or "not found: the shaders of an earlier build are used"))
        if args.no_build:
            rep.info("skipped (--no-build)")
        else:
            build_game(rep, build_dir, pc_build, glslc, args.jobs)
        rpx = build_dir / "rac1_wiiu.rpx"
        if not rpx.is_file():
            raise SetupError("no %s: build it first (without --no-build)" % rpx)
        shaders_dir = Path(args.shaders_from).expanduser().resolve() if args.shaders_from else build_dir / "shaders"
        if not list(shaders_dir.glob("*.gsh")):
            raise SetupError("no shaders (.gsh) in %s: install CafeGLSL (https://github.com/Exzap/CafeGLSL) and give "
                             "its bin/glslcompiler with --glslc, or a folder of .gsh files with --shaders-from" %
                             shaders_dir)
        run_cfg = build_dir / "run.cfg"
        if not run_cfg.is_file():
            run_cfg.write_text(DEFAULT_RUN_CFG)
        rep.step(3, total, "Packing the levels' sounds and the MRP classes")
        packs = build_dir / "packs"
        packed_hdr = level_packs(rep, packs)
        packed_mrp = mrp_packs(rep, packs)
        rep.step(4, total, "Staging the SD card's tree in %s" % sd)
        files = tree_files(build_dir, packs, boot_elf, shaders_dir, packed_hdr, packed_mrp)
        c = stage(rep, files, sd, link=not args.copy)
        size = sum(src.stat().st_size for src, _ in files.values())
        rep.info("%d files, %.2f GiB: %d linked, %d copied, %d unchanged, %d removed" % (
            len(files), size / 2**30, c["linked"], c["copied"], c["kept"], c["removed"]))
        rep.step(5, total, "Writing the manifest")
        manifest = build_dir / "sd-manifest.txt"
        write_manifest(rep, files, manifest, build_dir / "sd-hashes.json")
        comps = {}
        for src, comp in files.values():
            k = comp.split("/")[0]
            n, b = comps.get(k, (0, 0))
            comps[k] = (n + 1, b + src.stat().st_size)
        for k in sorted(comps, key=lambda k: order_key(k)):
            rep.info("  %-8s %5d files %9.1f MiB" % (k, comps[k][0], comps[k][1] / 2**20))
        rep.event("done", sd=str(sd), manifest=str(manifest), files=len(files), bytes=size,
                  seconds=round(time.monotonic() - t_all, 1))
        if not args.json:
            print("\nThe Wii U's SD tree is ready: %s\nIts manifest: %s\nSend it to the console with "
                  "tools/wiiu_backend.py send (or copy the tree onto the card)." % (sd, manifest))
        return 0
    except SetupError as e:
        rep.event("error", text=str(e))
        if not args.json:
            print("\nerror: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
