#!/usr/bin/env python3
"""rac1-wiiu-backend: the PC side of the Wii U launcher (design: LAUNCHER_DESIGN.md section 4). Python 3.8+, standard
library only. It runs the setups, then sends the SD card's tree to rac1-link on the console over the LAN and starts the
game; the console needs nothing but rac1-link (ports/wiiu/link) open on the TV.

  wiiu_backend.py setup [--iso IMAGE]   the PC setup when it is due (tools/setup.py), then setup.py --wiiu: the Wii U
                                        build and the SD tree staged in build-wiiu/sd with its manifest
  wiiu_backend.py discover              the consoles running rac1-link on this network
  wiiu_backend.py pair --code NNNNNN    pair with the console (the code is on the TV)
  wiiu_backend.py status                the console's card against the staged tree
  wiiu_backend.py send [--launch]       what the console lacks or has older: the game's .rpx, the shaders, the core
                                        data, then the levels (and the movies), each file verified on the console
  wiiu_backend.py sync [--launch]       setup, then send: "update my console"
  wiiu_backend.py launch                start the game on the console

--json prints JSON lines ({"event": ...}) on stdout for the C# launcher's Wii U page. Files already on the card (copied
by hand, or by an older receiver) are hashed by the console and kept when they match ("adopt"), not sent again.
The PC never listens on a port: it broadcasts on UDP 47198 and connects to the console's TCP 47199.
"""
import argparse
import importlib.util
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LINK_PY = ROOT / "ports/wiiu/link/pc/rac1_link.py"
SETUP_PY = ROOT / "tools/setup.py"
sys.path.insert(0, str(ROOT / "ports/wiiu/tools"))
import wiiu_setup  # noqa: E402


def load_link():
    spec = importlib.util.spec_from_file_location("rac1_link", LINK_PY)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


link = load_link()
MiB = link.MiB
LinkError = link.LinkError


class Out(link.Out):
    """rac1_link's output (messages on stderr, or JSON lines on stdout), with the backend's events."""

    def phase(self, name, text):
        self.event("phase", phase=name, text=text)
        if not self.json:
            self.info(text)


# ------------------------------------------------------------------------------------------------------------------
# setup

def run_child(cmd, out, json_lines=False):
    """A setup run as a child: its JSON lines passed on as they are, its text as info events (or printed)."""
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT,
                         stdin=subprocess.DEVNULL)
    for line in p.stdout:
        line = line.rstrip("\n")
        if out.json:
            if json_lines and line.startswith("{"):
                print(line, flush=True)
            elif line.strip():
                out.event("setup_text", text=line)
        else:
            print(line, flush=True)
    return p.wait()


def cmd_setup(args, out):
    py = sys.executable
    if not args.wiiu_only:
        r = subprocess.run([py, str(SETUP_PY), "--check"], cwd=ROOT, capture_output=True, text=True)
        if r.returncode == 10 or args.iso:
            out.phase("pc_setup", "the PC setup is due (%s): running it" % ", ".join(r.stdout.split()) if r.returncode
                      == 10 else "the PC setup with %s" % args.iso)
            cmd = [py, str(SETUP_PY), "--no-play"] + (["--iso", args.iso] if args.iso else []) + \
                  (["-j", str(args.jobs)] if args.jobs else [])
            if run_child(cmd, out):
                raise LinkError("the PC setup failed (above)")
        elif r.returncode:
            raise LinkError("setup.py --check failed: %s" % (r.stdout + r.stderr).strip()[-500:])
        else:
            out.phase("pc_setup", "the PC setup is up to date")
    out.phase("wiiu_setup", "the Wii U build and the SD tree")
    cmd = [py, str(SETUP_PY), "--wiiu", "--build-dir", args.build_dir] + (["-j", str(args.jobs)] if args.jobs else []) + \
          (["--no-build"] if args.no_build else []) + (["--json"] if out.json else [])
    if run_child(cmd, out, json_lines=True):
        raise LinkError("setup.py --wiiu failed (above)")
    return 0


# ------------------------------------------------------------------------------------------------------------------
# send

def staged(args):
    """The staged tree's manifest: [(path on the card, size, sha1, component)], in sending order."""
    build = Path(args.build_dir).expanduser().resolve()
    manifest = build / "sd-manifest.txt"
    sd = Path(args.sd).expanduser().resolve() if args.sd else build / "sd"
    if not manifest.is_file():
        raise LinkError("no staged tree (%s): run `wiiu_backend.py setup` first" % manifest)
    try:
        rows = wiiu_setup.read_manifest(manifest)
    except wiiu_setup.SetupError as e:
        raise LinkError(str(e))
    return rows, sd


def select(rows, args):
    comps = set(args.component or ())
    levels = set(args.level or ())
    out = []
    for r in rows:
        comp = r[3]
        head = comp.split("/")[0]
        if head == "movies" and args.no_movies:
            continue
        if comps or levels:
            lv = int(comp.split("/")[1]) if head == "level" else None
            if not (head in comps or comp in comps or (lv is not None and lv in levels)):
                continue
        out.append(r)
    return out


def plan(s, rows, args, out):
    """What to send: the staged files the console's manifest lacks (or has with another size / SHA-1), less those found
    on its card already (adopt). The run.cfg is sent only when the card has none (the console's own settings stay)."""
    files, _parts = s.manifest()
    todo = [r for r in rows if args.force or files.get(r[0]) != (r[1], r[2])]
    if todo and not args.no_adopt and not args.force:
        out.phase("adopt", "%d file(s) not recorded on the console: looking for them on its card" % len(todo))
        t0 = time.monotonic()
        res = s.adopt([(r[0], r[1], r[2]) for r in todo])
        out.info("adopt: %d adopted (the console hashed %.1f MiB in %.1f s), %d missing, %d different" % (
            res["adopted"], res["hashed"] / MiB, time.monotonic() - t0, len(res["missing"]), len(res["differ"])))
        out.event("adopted", adopted=res["adopted"], missing=len(res["missing"]), differ=len(res["differ"]),
                  hashed=res["hashed"], ms=res["ms"])
        need = set(res["missing"]) | set(res["differ"])
        keep_cfg = set(res["differ"])  # (a run.cfg of the console's own)
        todo = [r for r in todo if r[0] in need and not (r[3] == "config" and r[0] in keep_cfg)]
    return todo


def cmd_send(args, out):
    rows, sd = staged(args)
    rows = select(rows, args)
    missing = [r[0] for r in rows if not (sd / r[0]).is_file() or (sd / r[0]).stat().st_size != r[1]]
    if missing:
        raise LinkError("the staged tree does not match its manifest (%d files, e.g. %s): run setup again" % (
            len(missing), missing[0]))
    s = link.connect(args, out)
    try:
        out.event("console", ip=s.host, name=s.hello.get("name"), version=s.hello.get("version"),
                  serial=s.hello.get("serial"), free=int(s.hello.get("free", 0)))
        todo = plan(s, rows, args, out)
        total = sum(r[1] for r in todo)
        free = int(s.hello.get("free", 0))
        if free and total + total // 20 + (64 << 20) > free:
            raise LinkError("the console's card is too full: %.0f MiB to send, %.0f MiB free" % (total / MiB,
                                                                                               free / MiB))
        out.phase("send", "%d of %d file(s) to send, %.1f MiB" % (len(todo), len(rows), total / MiB))
        if args.dry_run:
            for r in todo:
                out.event("would_send", path=r[0], size=r[1], component=r[3])
                if not out.json:
                    print("%-10s %10d %s" % (r[3], r[1], r[0]))
            return 0
        sent_files, bad = send_rows(s, todo, sd, total, out)
        if args.launch and not bad:
            s.send(link.LAUNCH)
            s.sock.close()
            out.phase("launch", "the game is starting on the console")
            return 0
    finally:
        try:
            s.bye()
        except OSError:
            pass
    return 1 if bad else 0


def send_rows(s, todo, sd, total, out):
    """Sends `todo` in its order: runs of small files in a row (F_PIPE), larger ones one by one with resume. A file the
    console refuses or gets damaged is sent again once, alone."""
    t0 = time.monotonic()
    state = {"bytes": 0, "files": 0, "bad": []}
    last = [0.0]

    def tick(path, force=False):
        now = time.monotonic()
        if not force and now - last[0] < (0.5 if out.json else 2.0):
            return
        last[0] = now
        el = now - t0
        rate = state["bytes"] / max(el, 1e-6)
        eta = (total - state["bytes"]) / rate if rate > 0 else None
        if out.json:
            out.event("progress", file=path, files=state["files"], files_total=len(todo), done=state["bytes"],
                      total=total, mib_s=round(rate / MiB, 3), eta_s=round(eta) if eta is not None else None)
        elif not out.quiet:
            sys.stderr.write("\r\033[K" if out.tty else "")
            sys.stderr.write("  %d/%d files, %.1f/%.1f MiB, %.2f MiB/s, ETA %s %s" % (
                state["files"], len(todo), state["bytes"] / MiB, total / MiB, rate / MiB,
                "%dm%02ds" % divmod(int(eta), 60) if eta is not None else "-", path[-50:]))
            sys.stderr.write("" if out.tty else "\n")
            sys.stderr.flush()

    retry = []

    def done_one(row, res):
        if "bad" in res:
            out.event("failed", file=row[0], reason=res["bad"], code=res.get("code", ""))
            retry.append(row)
            return
        state["bytes"] += row[1]
        state["files"] += 1
        tick(row[0])

    def reader_of(path):
        def read_all():
            with open(path, "rb") as f:
                return f.read()
        return read_all

    def one(row):
        path = sd / row[0]
        with open(path, "rb") as f:
            def reader(off, n):
                f.seek(off)
                return f.read(n)
            base = state["bytes"]

            class P:  # (the file's own progress folded into the total)
                def __getattr__(self, k):
                    return getattr(out, k)

                def progress(self, name, done, size, t, force=False):
                    state["bytes"] = base + done
                    tick(name)
            saved = s.out
            s.out = P()
            try:
                s.progress_text("%s (%.1f MiB)" % (row[0], row[1] / MiB))
                res = s.send_stream(row[0], row[1], row[2], reader, path=row[0])
            except LinkError as e:
                if "FILE_BAD" not in str(e) and "not allowed" not in str(e) and "card full" not in str(e):
                    raise
                res = {"bad": str(e)}
            finally:
                s.out = saved
            state["bytes"] = base
        done_one(row, res)

    i = 0
    while i < len(todo):
        j = i
        while j < len(todo) and todo[j][1] <= link.PIPE_MAX:
            j += 1
        if j - i >= 2:
            s.progress_text("files %d-%d of %d" % (i + 1, j, len(todo)))
            s.send_pipelined([(r[0], r[1], r[2], reader_of(sd / r[0]), r) for r in todo[i:j]],
                             lambda item, res: done_one(item[4], res))
            i = j
        else:
            one(todo[i])
            i += 1
    bad = []
    for row in retry:  # (once more, alone)
        before = len(retry)
        one(row)
        if len(retry) > before:
            bad.append(row)
            out.info("FAILED %s" % row[0])
    tick("", force=True)
    if not out.json and not out.quiet:
        sys.stderr.write("\n")
    el = time.monotonic() - t0
    out.info("sent %d file(s), %.1f MiB in %.1f s (%.2f MiB/s)%s" % (
        state["files"], state["bytes"] / MiB, el, state["bytes"] / MiB / max(el, 1e-9),
        "; %d failed" % len(bad) if bad else ""))
    out.event("sent", files=state["files"], bytes=state["bytes"], seconds=round(el, 1), failed=[r[0] for r in bad])
    return state["files"], bad


# ------------------------------------------------------------------------------------------------------------------
# status, launch, sync

def cmd_status(args, out):
    rows, _sd = staged(args)
    rows = select(rows, args)
    s = link.connect(args, out)
    files, parts = s.manifest()
    h = s.hello
    s.bye()
    by = {}
    for r in rows:
        head = r[3].split("/")[0]
        n = by.setdefault(head, [0, 0, 0, 0])  # files, bytes, up to date, bytes to send
        n[0] += 1
        n[1] += r[1]
        if files.get(r[0]) == (r[1], r[2]):
            n[2] += 1
        else:
            n[3] += r[1]
    out.info("%s %s at %s, card free %.1f MiB; %d files recorded, %d partial" % (
        h.get("name"), h.get("version"), s.host, int(h.get("free", 0)) / MiB, len(files), len(parts)))
    for head in sorted(by, key=wiiu_setup.order_key):
        n = by[head]
        out.info("  %-8s %5d/%5d up to date, %8.1f MiB to send" % (head, n[2], n[0], n[3] / MiB))
        out.event("component", component=head, files=n[0], bytes=n[1], current=n[2], to_send=n[3])
    out.info("(files the console does not have recorded may still be on its card: `send` finds them first)")
    return 0


def cmd_launch(args, out):
    s = link.connect(args, out)
    s.send(link.LAUNCH)
    s.sock.close()
    out.phase("launch", "the game is starting on the console (%s)" % s.host)
    return 0


def cmd_sync(args, out):
    args.wiiu_only = False
    cmd_setup(args, out)
    return cmd_send(args, out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--console", help="the console's IP, shown on the TV (default: discovery, then the last one)")
    ap.add_argument("--port", type=int, default=link.TCP_PORT)
    ap.add_argument("--udp-port", type=int, default=link.UDP_PORT)
    ap.add_argument("--broadcast", action="append", help="discovery target (default 255.255.255.255); repeatable")
    ap.add_argument("--discover-timeout", type=float, default=5.0)
    ap.add_argument("--code", help="the pairing code (default: the one saved by `pair`)")
    ap.add_argument("--build-dir", default=str(ROOT / "build-wiiu"), help="the Wii U build (default build-wiiu)")
    ap.add_argument("--sd", help="the staged tree (default <build dir>/sd)")
    ap.add_argument("--json", action="store_true", help="JSON lines on stdout (for a GUI)")
    ap.add_argument("--quiet", action="store_true")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def setup_opts(p):
        p.add_argument("--iso", help="your disc image (for the PC setup, when it is due)")
        p.add_argument("-j", "--jobs", type=int, help="parallel jobs")
        p.add_argument("--no-build", action="store_true", help="stage the tree from the existing Wii U build")

    def send_opts(p):
        p.add_argument("--component", action="append", help="only this component: game, shaders, config, core, level, "
                                                             "level/N, movies (repeatable)")
        p.add_argument("--level", action="append", type=int, help="only this level's files (repeatable)")
        p.add_argument("--no-movies", action="store_true", help="leave the movies out (2.9 GB)")
        p.add_argument("--no-adopt", action="store_true", help="do not look for files already on the card")
        p.add_argument("--force", action="store_true", help="send everything again")
        p.add_argument("--dry-run", action="store_true", help="list what would be sent")
        p.add_argument("--launch", action="store_true", help="start the game when everything is sent")

    p = sub.add_parser("setup", help="the PC setup when due, then the Wii U build and the SD tree")
    setup_opts(p)
    p.add_argument("--wiiu-only", action="store_true", help="skip the PC setup's check")
    sub.add_parser("discover", help="find consoles running rac1-link")
    p = sub.add_parser("pair", help="pair with the code shown on the TV")
    p.add_argument("--code", dest="code", required=True)
    p = sub.add_parser("status", help="the console's card against the staged tree")
    p.add_argument("--component", action="append")
    p.add_argument("--level", action="append", type=int)
    p.add_argument("--no-movies", action="store_true")
    p = sub.add_parser("send", help="send what changed, in priority order")
    send_opts(p)
    p = sub.add_parser("sync", help="setup, then send")
    setup_opts(p)
    send_opts(p)
    sub.add_parser("launch", help="start the game on the console")
    args = ap.parse_args(argv)
    out = Out(args.json, args.quiet)
    try:
        if args.cmd == "discover":
            return link.cmd_discover(args, out)
        if args.cmd == "pair":
            return link.cmd_pair(args, out)
        return {"setup": cmd_setup, "status": cmd_status, "send": cmd_send, "sync": cmd_sync,
                "launch": cmd_launch}[args.cmd](args, out)
    except (LinkError, OSError) as e:
        out.info("error: %s" % e)
        out.event("error", text=str(e))
        return 2
    except KeyboardInterrupt:
        out.info("stopped: what was sent is kept, and the next send goes on from there")
        out.event("error", text="interrupted")
        return 130


if __name__ == "__main__":
    sys.exit(main())
