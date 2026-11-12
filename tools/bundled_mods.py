#!/usr/bin/env python3
"""The mods the game ships, ready made (bundled-mods/, docs/mods.md "Mods that come with the game").

A bundled mod is a mod folder exactly as its exporter writes it (mod.json, level/, plugin/), kept in bundled-mods/<folder>/.
Files that already live elsewhere in the repository are not kept twice: bundled-mods/<folder>/bundle.json lists them
("shared": the path inside the mod -> the path in the repository, as the plugin's source and the music), and the install
puts them in. bundle.json itself is not part of the mod.

The setup (tools/setup.py, so setup.sh, setup.cmd, the launcher's setup and update, play.cmd's check) calls install() before it
composes the level mods and builds their plugins:

  - mods/<folder> missing: the bundled mod is copied in;
  - mods/<folder> there, untouched since an earlier install (its files hash to what mods/<folder>/.bundled-mod says): replaced
    when the bundled mod is different (a new version), left alone when it is the same;
  - mods/<folder> there and changed by the player (or made by hand, with no .bundled-mod, as the Blender scripts' --install does): kept as it is,
    and the log says so. A folder that is the same as the bundled mod, with no marker, is adopted (the marker is written).

Run by hand:
  python3 tools/bundled_mods.py --install        what the setup does
  python3 tools/bundled_mods.py --check          for the tests: every bundled mod complete and not older than its exporter
  python3 tools/bundled_mods.py --due            the mods an install would copy (one per line)
  python3 tools/bundled_mods.py --refresh <folder> <exported mod folder>
                                                 what the exporter's --bundle does: writes bundled-mods/<folder> from a fresh export

Plain Python 3.8 and the standard library, no shell and no symbolic links: the same on Windows.
"""
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUNDLED = ROOT / "bundled-mods"
MARKER = ".bundled-mod"
BUNDLE_JSON = "bundle.json"


def _say(msg):
    print(msg, flush=True)


def bundled_folders():
    if not BUNDLED.is_dir():
        return []
    return sorted(d.name for d in BUNDLED.iterdir() if d.is_dir() and not d.name.startswith((".", "_")) and (d / "mod.json").is_file())


def read_bundle(folder):
    try:
        return json.loads((BUNDLED / folder / BUNDLE_JSON).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def shipped_files(folder):
    """{path inside the mod (with /): the file that holds it}: the folder's own files and the shared ones."""
    files = {}
    base = BUNDLED / folder
    for p in sorted(base.rglob("*")):
        if p.is_file() and p.name != BUNDLE_JSON:
            files[p.relative_to(base).as_posix()] = p
    for inside, outside in read_bundle(folder).get("shared", {}).items():
        files[inside] = ROOT / outside
    return files


def hash_files(files):
    """The content hash of a set of {path: file}: the paths and the bytes (MARKER left out)."""
    h = hashlib.sha256()
    for rel in sorted(files):
        if rel == MARKER:
            continue
        h.update(rel.encode() + b"\0" + hashlib.sha256(Path(files[rel]).read_bytes()).digest())
    return h.hexdigest()


def tree_files(d):
    return {p.relative_to(d).as_posix(): p for p in sorted(d.rglob("*")) if p.is_file()}


def mod_version(d):
    try:
        return str(json.loads((Path(d) / "mod.json").read_text(encoding="utf-8")).get("version", ""))
    except (OSError, ValueError):
        return ""


def decide(mods, folder):
    """('install' | 'update' | 'same' | 'adopt' | 'keep', the shipped hash, the installed hash)."""
    shipped = hash_files(shipped_files(folder))
    dst = mods / folder
    if not dst.exists():
        return "install", shipped, None
    if not dst.is_dir():
        return "keep", shipped, None
    have = hash_files(tree_files(dst))
    try:
        marked = json.loads((dst / MARKER).read_text(encoding="utf-8")).get("hash")
    except (OSError, ValueError):
        marked = None
    if have == shipped:
        return ("same" if marked == shipped else "adopt"), shipped, have
    if marked is not None and marked == have:
        return "update", shipped, have  # (untouched since the install: an older shipped version)
    return "keep", shipped, have


def due(mods):
    """The bundled mods an install would copy (missing or older)."""
    return [f for f in bundled_folders() if decide(Path(mods), f)[0] in ("install", "update")]


def _write_marker(dst, shipped, version):
    (dst / MARKER).write_text(json.dumps({"hash": shipped, "version": version}, indent=2) + "\n", encoding="utf-8")


def _copy_in(mods, folder, shipped):
    """Written beside the final place and renamed (a name starting with . is not a mod: the game and the setup skip it)."""
    tmp = mods / (".bundling-" + folder)
    old = mods / (".replaced-" + folder)
    for stale in (tmp, old):
        shutil.rmtree(stale, ignore_errors=True)
    for rel, src in shipped_files(folder).items():
        out = tmp / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, out)
    _write_marker(tmp, shipped, mod_version(tmp))
    dst = mods / folder
    if dst.exists():
        os.replace(dst, old)  # (a directory: rename it away first, the same on every system)
    try:
        os.replace(tmp, dst)
    except OSError:
        if old.exists() and not dst.exists():
            os.replace(old, dst)  # put the player's folder back
        raise
    shutil.rmtree(old, ignore_errors=True)


def install(mods=None, log=_say):
    """Every bundled mod copied into mods/ when missing or older, never over the player's changes. Returns how many were
    installed or updated. A failure on one mod is logged, never raised."""
    mods = Path(mods) if mods is not None else ROOT / "mods"
    n = 0
    for folder in bundled_folders():
        try:
            what, shipped, _ = decide(mods, folder)
            version = mod_version(BUNDLED / folder)
            if what in ("install", "update"):
                mods.mkdir(parents=True, exist_ok=True)
                old = mod_version(mods / folder) if what == "update" else ""
                _copy_in(mods, folder, shipped)
                n += 1
                log(f"[bundled mods] {folder}: " + (f"installed (version {version})" if what == "install"
                                                  else f"updated ({old or '?'} -> {version or '?'})"))
            elif what == "adopt":
                _write_marker(mods / folder, shipped, version)
                log(f"[bundled mods] {folder}: already the shipped one")
            elif what == "keep":
                log(f"[bundled mods] {folder}: mods/{folder} is not the shipped one (your own changes, or made by hand): kept as it is")
        except Exception as e:  # (never fatal for the game's setup)
            log(f"[bundled mods] {folder}: could not be installed ({e})")
    return n


# --- the exporter's side -----------------------------------------------------------------------------------------------

def export_stamp(folder):
    """The hash of the scripts that write the mod (bundle.json's "export_sources", LF whatever the checkout's lines)."""
    h = hashlib.sha256()
    for rel in read_bundle(folder).get("export_sources", []):
        data = (ROOT / rel).read_bytes().replace(b"\r\n", b"\n")
        h.update(rel.encode() + b"\0" + data + b"\0")
    return h.hexdigest()


def refresh(folder, exported):
    """bundled-mods/<folder> written from a mod folder the exporter just wrote: everything but the shared files, and the
    export stamp renewed. The shared files must be the repository's own, as the exporter copies them from there."""
    exported = Path(exported)
    bundle = read_bundle(folder)
    shared = bundle.get("shared", {})
    if not bundle:
        raise SystemExit(f"bundled-mods/{folder}/{BUNDLE_JSON} is missing (its shared files and export sources)")
    for inside, outside in shared.items():
        a, b = exported / inside, ROOT / outside
        if not a.is_file() or a.read_bytes() != b.read_bytes():
            raise SystemExit(f"the export's {inside} is not {outside}: the export must take it from there")
    dst = BUNDLED / folder
    keep = {BUNDLE_JSON}
    new = {rel: p for rel, p in tree_files(exported).items() if rel not in shared}
    for rel, p in new.items():
        out = dst / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        if not out.is_file() or out.read_bytes() != p.read_bytes():
            shutil.copyfile(p, out)
        keep.add(rel)
    for rel in list(tree_files(dst)):  # (a file the export no longer writes)
        if rel not in keep:
            (dst / rel).unlink()
    for d in sorted((p for p in dst.rglob("*") if p.is_dir()), reverse=True):
        try:
            d.rmdir()
        except OSError:
            pass
    bundle["export_stamp"] = export_stamp(folder)
    (dst / BUNDLE_JSON).write_text(json.dumps(bundle, indent=2) + "\n", encoding="utf-8")
    _say(f"bundled-mods/{folder}: {len(new)} files written, {len(shared)} shared; export stamp {bundle['export_stamp'][:12]}")


def check():
    """0 when every bundled mod is complete and not older than the scripts that write it; else 1, each problem said."""
    bad = []
    for folder in bundled_folders():
        bundle = read_bundle(folder)
        files = shipped_files(folder)
        for rel, p in files.items():
            if not p.is_file():
                bad.append(f"{folder}: {rel} is missing ({p.relative_to(ROOT).as_posix() if ROOT in p.parents else p})")
        try:
            info = json.loads((BUNDLED / folder / "mod.json").read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            bad.append(f"{folder}: mod.json: {e}")
            continue
        if (info.get("level") or {}).get("id") and "level/level.txt" not in files:
            bad.append(f"{folder}: a level mod with no level/level.txt")
        if "plugin/CMakeLists.txt" not in files and (BUNDLED / folder / "plugin").exists():
            bad.append(f"{folder}: plugin/ with no CMakeLists.txt")
        for inside in bundle.get("shared", {}):
            if (BUNDLED / folder / inside).exists():
                bad.append(f"{folder}: {inside} is shared with the repository and also kept in the mod: keep it once")
        if bundle.get("export_sources"):
            if export_stamp(folder) != bundle.get("export_stamp"):
                bad.append(f"{folder}: older than its exporter ({', '.join(bundle['export_sources'])} changed): export it again "
                           f"(see bundle.json's \"how\") and commit bundled-mods/{folder}")
    for b in bad:
        _say("FAIL " + b)
    if not bad:
        _say("bundled mods: " + (", ".join(bundled_folders()) or "none") + ": complete and current")
    return 1 if bad else 0


def main(argv):
    if argv[:1] == ["--check"]:
        return check()
    if argv[:1] == ["--due"]:
        print("\n".join(due(ROOT / "mods")))
        return 0
    if argv[:1] == ["--install"]:
        install()
        return 0
    if argv[:1] == ["--refresh"] and len(argv) == 3:
        refresh(argv[1], argv[2])
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
