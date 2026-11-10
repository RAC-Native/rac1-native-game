#!/usr/bin/env python3
"""The classes' dependencies (the open engine, docs/open-engine.md): for every class of every level, the classes its code
spawns (projectiles, effects, debris, drops, the gadgets an enemy holds), found in the code, and what of its level's data
it reads. A session importing a class imports its closure with it (src/game/enhance/session.cpp).

A class's code is its level's: the generated functions of level N (assets/generated/src/game/ovl/, `<name>_l<N>`), from its
roots: the functions named for it (`..._class<C>_..._l<N>`), the port registered for its update function (the pool's
row, `ovl_register_level_moby_fn(..., L<N>_ADDR(<update>), <fn>)` in src/game/ovl/), the functions of its hook table
(the class table row's +0x2c word, read in the level's context image: pool/context/levelNN.img). From the roots the calls
are followed:
  - through the level's own functions (`<name>_l<N>`): a function that calls moby_spawn spawns every class id it loads
    into a0 (`set32(a0, zero.w() + <id>)`, the transliteration's `addiu a0, zero, <id>`): its "own" dependencies;
  - into the engine's (the hand ports, src/game/ovl/*.cpp, and the generated engine functions without a level): a
    `moby_spawn(<literal>)` there, and the engine's functions those call, are "engine" dependencies (a class the engine
    spawns for it: debris, an explosion, a drop).
Only ids that are classes of the level (its pool rows: pool.idx) are kept: a constant that is not a class of the level
is a value. What the code computes (an id from a table, a base plus a count) is not found so: the classes the game was
seen spawning (data/pool/spawns.txt in the repository, measured with RAC1_DEPS_PROBE=1 runs: `--probe <logs>` merges
them in) are added as "seen". The base of a session has many of them (the bolts, the common effects): the session imports only those it
lacks, so an engine dependency the base has costs nothing.

It also counts the level data the class's own functions name (DESIGN 6: path, group, camera, volume, rail tables): what
stage 4's registries must give it.

Output (generated: never in the repository): <pool>/deps/levelNN.txt, one line a class
  class <id> own <ids|-> engine <ids|-> seen <ids|-> reads <kind:n,...|-> fns <own functions> sounds <n>
and a summary. Usage (repo root, after the setup and the pool: `rac1-native --setup --pool`):
  tools/overlay_match/deps.py [--levels 13,16] [--pool assets/generated/pool] [--class 16:541]
  tools/overlay_match/deps.py --probe <RAC1_DEPS_PROBE logs...>   (data/pool/spawns.txt: the measured spawns merged in)
"""
import argparse
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "assets/generated/src/game/ovl"
HAND = ROOT / "src/game/ovl"
MAPS = ROOT / "assets/generated/overlay_map"
SPAWNS = ROOT / "data/pool/spawns.txt"
PROBE = re.compile(r"^\[deps\] probe level (\d+): class (\d+) spawned (\S+)")
BASE = 0x15EF00
# Veldin's level-data tables (DESIGN 6.1): what a class's sites name of its level's data
TABLES = {0x1B04B0: "path", 0x160104: "path", 0x1AB840: "group", 0x15EF50: "camera", 0x1600E4: "volume", 0x15F70C: "rail"}

FN_HEAD = re.compile(r"^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w:<>\*&\s]*?\b([A-Za-z_]\w*)\(([^;]*)\)\s*(?:const\s*)?\{\s*$")
CALL = re.compile(r"\b([A-Za-z_]\w*)\(")
A0_CONST = re.compile(r"set32\(a0, zero\.w\(\) \+ 0x([0-9a-f]+)\)")
SPAWN_LIT = re.compile(r"\bmoby_spawn\((0x[0-9a-fA-F]+|\d+)u?\)")
REG = re.compile(r"ovl_register_level_moby_fn\(\s*\w+,\s*L(\d+)_ADDR\(0x([0-9a-fA-F]+)u?\),\s*(\w+)\s*\)")
SITE = re.compile(r"\bL(\d+)_ADDR\(0x([0-9a-fA-F]+)u?\)")
LEVEL_FN = re.compile(r"_l(\d+)$")
ADDR_FN = re.compile(r"_([0-9a-f]{6})_l(\d+)$")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "static_cast", "reinterpret_cast", "const_cast", "set32",
            "lw_", "sw_", "lwu", "sw", "lbu", "sb", "lhu", "sh", "lw", "lh", "ld", "sd", "sq", "lq", "lwc1", "swc1", "OV", "OVF",
            "NOT_ADDR", "f2u", "u2f", "TrRet"}


def functions(path):
    """{name: body} of a C++ file: the functions defined at column 0, to the closing brace at column 0."""
    out = {}
    lines = path.read_text(errors="replace").splitlines()
    i = 0
    while i < len(lines):
        if lines[i].startswith((" ", "\t", "}", "#", "//")) or "(" not in lines[i]:
            i += 1
            continue
        head, k = lines[i], i
        while not head.rstrip().endswith("{") and ";" not in head and k < i + 3 and k + 1 < len(lines):  # (a head on several lines)
            k += 1
            head += " " + lines[k].strip()
        m = FN_HEAD.match(head)
        if m is None:
            i += 1
            continue
        j = k + 1
        while j < len(lines) and lines[j] != "}":
            j += 1
        out[m.group(1)] = head + "\n" + "\n".join(lines[k + 1:j + 1])
        i = j + 1
    return out


class Code:
    def __init__(self):
        self.gen = {}     # level function name -> body (generated)
        self.engine = {}  # engine function name -> body (hand ports, generated engine code)
        self.reg = {}     # (level, address) -> function
        self.by_addr = {}  # (level, address) -> generated function named for it
        for f in sorted(GEN.glob("*.cpp")):
            for name, body in functions(f).items():
                (self.gen if LEVEL_FN.search(name) else self.engine)[name] = body
        for f in sorted(HAND.glob("*.cpp")):
            text = f.read_text(errors="replace")
            for lv, a, fn in REG.findall(text):
                self.reg[(int(lv), int(a, 16))] = fn
            for name, body in functions(f).items():
                if LEVEL_FN.search(name):
                    self.gen.setdefault(name, body)
                else:
                    self.engine.setdefault(name, body)
        for name in self.gen:
            m = ADDR_FN.search(name)
            if m:
                self.by_addr[(int(m.group(2)), int(m.group(1), 16))] = name
        # the engine's spawns, closed over its own calls: function -> the class ids it (or what it calls) spawns
        self.engine_spawns = {}
        # the engine's spawners: functions with a class parameter they spawn (debris_spawn's 2nd, chunk_spawn's 5th, ...):
        # name -> the register that carries it in the generated code's calls (a0..a3, t0..t3)
        self.spawners = {"moby_spawn": "a0"}
        regs = ["a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3"]
        for name, body in self.engine.items():
            head = body.split("{", 1)[0]
            params = [p.strip().split()[-1].lstrip("&*") if p.strip() else "" for p in head[head.find("(") + 1:head.rfind(")")].split(",")]
            for i, p in enumerate(params[:8]):
                if p and (re.search(r"\bmoby_spawn\(\s*%s\s*\)" % re.escape(p), body) or
                          re.search(r"\b%s_v\(\s*\w+\s*,\s*%s\s*," % (re.escape(name), re.escape(p)), body)):
                    self.spawners[name] = regs[i]
                    if name.endswith("_v"):  # (an EE-address form beside a host one: the same argument)
                        self.spawners[name[:-2]] = regs[i]
        self.spawns = read_spawns()

    def fn_at(self, level, a):
        return self.reg.get((level, a)) or self.by_addr.get((level, a))

    def engine_closure(self, name, seen):
        if name in self.engine_spawns:
            return self.engine_spawns[name]
        if name in seen or name not in self.engine:
            return set()
        seen.add(name)
        body = self.engine[name]
        ids = {int(x, 0) for x in SPAWN_LIT.findall(body)}
        if "moby_spawn(" in body:
            ids |= {int(x, 16) for x in A0_CONST.findall(body)}
        for c in set(CALL.findall(body)):
            if c != name and c not in KEYWORDS and c in self.engine:
                ids |= self.engine_closure(c, seen)
        self.engine_spawns[name] = ids
        return ids


def read_pool(pool):
    """{level: {class: (update_fn, word2c, sounds)}} from pool.idx"""
    rows = defaultdict(dict)
    for line in (pool / "pool.idx").read_text().splitlines():
        p = line.split()
        if len(p) >= 9 and p[0] == "class":
            snd = 0 if p[9] == "-" else len(p[9].split(",")) if len(p) > 9 else 0
            rows[int(p[1])][int(p[2])] = (int(p[6], 16), int(p[7], 16), snd)
    return rows


def hook_table(pool, level, word, code):
    """the functions of a class's hook table (its +0x2c word: words of the level's code from there, at most 16)"""
    img = pool / f"context/level{level:02d}.img"
    if not img.exists() or word < BASE:
        return []
    b = img.read_bytes()
    _, base, lo, hi = struct.unpack_from("<4I", b, 0)
    out = []
    for k in range(16):
        off = 16 + word - base + 4 * k
        if off + 4 > len(b):
            break
        w = struct.unpack_from("<I", b, off)[0]
        if not lo <= w < hi:
            break
        out.append(w)
    return out


def level_tables(level):
    """{level address: table kind} of the level-data tables, through the level's map (Veldin's address -> the level's)"""
    out = {}
    path = MAPS / f"level{level}.txt"
    if not path.exists():
        return out
    for line in path.read_text().splitlines():
        p = line.split()
        if len(p) >= 3 and p[0] == "data" and int(p[1], 16) in TABLES:
            out[int(p[2], 16)] = TABLES[int(p[1], 16)]
    return out


def class_deps(code, rows, pool, level, cls, tables):
    upd, word, snd = rows[level][cls]
    roots = {n for n in code.gen if re.search(r"_class%d_.*_l%d$" % (cls, level), n)}
    # (the class table's -1 row's function, which every class without a row of its own gets: not the class's code)
    vals = list(rows[level].values())
    common = max((r[0] for r in vals), key=lambda f: sum(1 for r in vals if r[0] == f))
    common_word = max((r[1] for r in vals), key=lambda w: sum(1 for r in vals if r[1] == w))
    for a in ([upd] if upd != common else []) + (hook_table(pool, level, word, code) if word != common_word else []):
        fn = code.fn_at(level, a)
        if fn:
            roots.add(fn)
    seen, todo = set(), list(roots)
    own, eng, engine_fns = set(), set(), set()
    reads = defaultdict(int)
    while todo:
        f = todo.pop()
        if f in seen or f not in code.gen:
            continue
        seen.add(f)
        body = code.gen[f]
        for sp, reg in code.spawners.items():  # (a class id loaded into the spawner's class argument, in a function calling it)
            if sp + "(" in body:
                own |= {int(x, 16) for x in re.findall(r"set32\(%s, zero\.w\(\) \+ 0x([0-9a-f]+)\)" % reg, body)}
        own |= {int(x, 0) for x in SPAWN_LIT.findall(body)}
        for lv, a in SITE.findall(body):
            if int(lv) == level and int(a, 16) in tables:
                reads[tables[int(a, 16)]] += 1
        for c in set(CALL.findall(body)):
            if c in KEYWORDS or c == f:
                continue
            if c in code.gen:
                m = LEVEL_FN.search(c)
                # (another class's update is not a call it makes: the generated code names them only as data)
                if m and int(m.group(1)) == level and not re.search(r"_class\d+_update_l\d+$", c):
                    todo.append(c)
            elif c in code.engine:
                engine_fns.add(c)
    for e in engine_fns:
        eng |= code.engine_closure(e, set())
    known = rows[level]
    own = sorted(c for c in own if c in known and c != cls)
    eng = sorted(c for c in eng if c in known and c != cls and c not in own)
    seen_ids = sorted(c for c in code.spawns.get((level, cls), ()) if c in known and c != cls and c not in own and c not in eng)
    return own, eng, seen_ids, dict(reads), len(seen), snd, bool(roots)


def read_spawns():
    """{(level, class): {spawned class}} of data/pool/spawns.txt (`<level>:<class> <ids>`)"""
    out = defaultdict(set)
    if SPAWNS.exists():
        for line in SPAWNS.read_text().splitlines():
            p = line.split()
            if len(p) >= 2 and not p[0].startswith("#") and ":" in p[0]:
                lv, c = (int(x) for x in p[0].split(":"))
                out[(lv, c)] |= {int(x) for x in p[1].split(",") if x}
    return out


def merge_probe(logs):
    """the probe logs' spawns (RAC1_DEPS_PROBE=1: `[deps] probe level L: class C spawned D1xn,...`) merged into
    data/pool/spawns.txt"""
    have = read_spawns()
    before = sum(len(v) for v in have.values())
    for f in logs:
        for line in Path(f).read_text(errors="replace").splitlines():
            m = PROBE.match(line)
            if m:
                have[(int(m.group(1)), int(m.group(2)))] |= {int(x.split("x")[0]) for x in m.group(3).split(",")}
    head = ["# The classes the game was seen spawning, by the spawner's level and class (open engine, docs/open-engine.md): measured",
            "# with RAC1_DEPS_PROBE=1 runs and merged by tools/overlay_match/deps.py --probe; what a class's code computes (an id",
            "# from a table, a base plus a count) and the static search of deps.py cannot find. Class ids only.",
            "# <level>:<class> <spawned ids>"]
    body = [f"{lv}:{c} {','.join(str(x) for x in sorted(v))}" for (lv, c), v in sorted(have.items()) if v]
    SPAWNS.parent.mkdir(parents=True, exist_ok=True)
    SPAWNS.write_text("\n".join(head + body) + "\n")
    print(f"{SPAWNS}: {sum(len(v) for v in have.values())} spawns ({before} before) of {len(body)} classes")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--levels", default=",".join(str(n) for n in range(0, 19)))
    ap.add_argument("--pool", default=str(ROOT / "assets/generated/pool"))
    ap.add_argument("--class", dest="one", help="<level>:<class>: print its closure only")
    ap.add_argument("--probe", nargs="+", help="RAC1_DEPS_PROBE logs: their spawns merged into data/pool/spawns.txt")
    a = ap.parse_args()
    if a.probe:
        merge_probe(a.probe)
        return
    pool = Path(a.pool)
    if not (pool / "pool.idx").exists():
        sys.exit(f"{pool}/pool.idx missing: make the pool first (rac1-native --setup --pool)")
    code = Code()
    rows = read_pool(pool)
    if a.one:
        lv, c = (int(x) for x in a.one.split(":"))
        own, eng, sn, reads, nf, snd, ok = class_deps(code, rows, pool, lv, c, level_tables(lv))
        print(f"class {lv}:{c}: own {own} engine {eng} seen {sn} reads {reads} fns {nf} sounds {snd}{'' if ok else ' (no code found)'}")
        return
    out = pool / "deps"
    out.mkdir(parents=True, exist_ok=True)
    for lv in [int(x) for x in a.levels.split(",") if x]:
        if lv not in rows:
            continue
        tables = level_tables(lv)
        body, n_own, n_eng, n_seen, n_code = [], 0, 0, 0, 0
        for c in sorted(rows[lv]):
            own, eng, sn, reads, nf, snd, ok = class_deps(code, rows, pool, lv, c, tables)
            n_own += bool(own)
            n_eng += bool(eng)
            n_seen += bool(sn)
            n_code += ok
            r = ",".join(f"{k}:{v}" for k, v in sorted(reads.items())) or "-"
            body.append(f"class {c} own {','.join(map(str, own)) or '-'} engine {','.join(map(str, eng)) or '-'} seen {','.join(map(str, sn)) or '-'} "
                        f"reads {r} fns {nf} sounds {snd}")
        head = [f"# level {lv}: each class's dependencies, found in its code (tools/overlay_match/deps.py)",
                f"# {len(rows[lv])} classes, {n_code} with code of their own; {n_own} spawn classes of their own code, {n_eng} classes the engine spawns for "
                f"them, {n_seen} more seen spawning (data/pool/spawns.txt)",
                "# class <id> own <ids> engine <ids> seen <ids> reads <kind:sites> fns <own functions> sounds <sound definitions>"]
        (out / f"level{lv:02d}.txt").write_text("\n".join(head + body) + "\n")
        print(f"level {lv}: {len(rows[lv])} classes, {n_code} with code; {n_own} with own dependencies, {n_eng} with engine ones, {n_seen} with seen ones")


if __name__ == "__main__":
    main()
