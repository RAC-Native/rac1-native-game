#!/usr/bin/env python3
"""rac1-link, the PC side (prototype): find the console, pair, send files with resume, measure the transfer.

Python 3.8+, standard library only. The console side is ../console (the Wii U app) or ../host (the same receiver
built for a PC, for tests). The wire format is ../core/link_proto.h; the numbers below mirror it.

  rac1_link.py discover
  rac1_link.py pair --code 123456 [--console IP]
  rac1_link.py status
  rac1_link.py send LOCAL [--dest rac1-native/...]
  rac1_link.py adopt LOCAL [--dest rac1-native/...]
  rac1_link.py bench --mode net|net-hash|sd|both|all --mb 256
  rac1_link.py config --rcvbuf 262144 --winscale 1 --sack 1 [--somemopt 1]

The PC only connects out (TCP 47199) and broadcasts (UDP 47198); it never listens.
"""
import argparse
import collections
import hashlib
import hmac
import json
import os
import random
import secrets
import select
import socket
import struct
import sys
import time
import zlib

PROTO = 1
TCP_PORT = 47199
UDP_PORT = 47198
CHUNK = 1 << 20
FILE_MAX = 0xFFFFFFFF

HELLO, AUTH, AUTH_OK, ERROR, GET_MANIFEST, MANIFEST, BEGIN_FILE, READY, DATA, ACK, RESEND, END_FILE, FILE_OK, \
    FILE_BAD, BENCH_SD, BENCH_RESULT, CONFIG, CONFIG_OK, PROGRESS, PING, PONG, CANCEL, BYE, QUIT, LAUNCH, ADOPT, \
    ADOPT_OK = range(1, 28)
NAMES = {v: k for k, v in globals().items() if isinstance(v, int) and k.isupper() and 1 <= v <= 27 and k not in
         ("PROTO",)}
F_DISCARD, F_NOHASH, F_BENCH, F_PIPE = 1, 2, 4, 8
PIPE_MAX = 4 << 20      # files up to this size go in a row (F_PIPE), without a round trip each
PIPE_WINDOW = 16 << 20  # bytes of them sent and not yet answered
PIPE_FILES = 256        # files of them sent and not yet answered
ADOPT_BATCH = 60000     # bytes of "size\tsha1\tpath" lines per ADOPT (the console takes control payloads up to 64 KiB)

MiB = 1 << 20


class LinkError(Exception):
    pass


def kv_parse(b):
    out = {}
    for line in b.decode("utf-8", "replace").split("\n"):
        if "=" in line:
            k, v = line.split("=", 1)
            out[k] = v
    return out


def kv_make(**kw):
    return "".join("%s=%s\n" % (k, v) for k, v in kw.items()).encode()


def config_path():
    p = os.environ.get("RAC1_LINK_CONFIG")
    if p:
        return p
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    if os.name == "nt":
        base = os.environ.get("APPDATA", base)
    return os.path.join(base, "rac1-link", "consoles.json")


def load_config():
    try:
        with open(config_path()) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {"consoles": {}, "last": None}


def save_config(cfg):
    p = config_path()
    os.makedirs(os.path.dirname(p), exist_ok=True)
    tmp = p + ".tmp"
    with open(tmp, "w") as f:
        json.dump(cfg, f, indent=1)
    os.replace(tmp, p)


# --------------------------------------------------------------------------------------------------------------
# Output

class Out:
    def __init__(self, json_mode=False, quiet=False):
        self.json = json_mode
        self.quiet = quiet
        self.last = 0.0
        self.tty = sys.stderr.isatty()

    def event(self, kind, **kw):
        if self.json:
            kw["event"] = kind
            print(json.dumps(kw), flush=True)

    def info(self, msg):
        if self.json:
            self.event("info", text=msg)
        elif not self.quiet:
            if self.tty:
                sys.stderr.write("\r\033[K")
            print(msg, file=sys.stderr, flush=True)

    def progress(self, name, done, total, t0, force=False):
        now = time.monotonic()
        if not force and now - self.last < (0.5 if self.tty else 5.0):
            return
        self.last = now
        rate = done / MiB / max(now - t0, 1e-6)
        if self.json:
            self.event("progress", file=name, done=done, total=total, mib_s=round(rate, 3))
            return
        if self.quiet:
            return
        pct = 100.0 * done / total if total else 100.0
        eta = (total - done) / MiB / rate if rate > 0 else 0
        line = "  %s %7.1f/%.1f MiB %5.1f%% %6.2f MiB/s ETA %ds" % (name[-40:], done / MiB, total / MiB, pct, rate,
                                                                      eta)
        if self.tty:
            sys.stderr.write("\r\033[K" + line)
            sys.stderr.flush()
        else:
            print(line, file=sys.stderr, flush=True)


# --------------------------------------------------------------------------------------------------------------
# Discovery

def discover(udp_port=UDP_PORT, broadcasts=None, timeout=5.0, out=None):
    """Broadcast RAC1LINK? once a second for `timeout` s; return {ip: info}."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 0))
    query = ("RAC1LINK? proto=%d" % PROTO).encode()
    targets = broadcasts or ["255.255.255.255"]
    found = {}
    end = time.monotonic() + timeout
    next_send = 0.0
    while time.monotonic() < end:
        if time.monotonic() >= next_send:
            for t in targets:
                try:
                    s.sendto(query, (t, udp_port))
                except OSError as e:
                    if out:
                        out.info("discover: cannot send to %s: %s" % (t, e))
            next_send = time.monotonic() + 1.0
        r, _, _ = select.select([s], [], [], 0.2)
        if not r:
            continue
        data, (ip, _) = s.recvfrom(512)
        if not data.startswith(b"RAC1LINK! "):
            continue
        info = {}
        for tok in data[10:].decode("utf-8", "replace").split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                info[k] = v
        if ip not in found:
            found[ip] = info
            if out:
                out.event("found", ip=ip, **info)
        if broadcasts and len(found) and all(b not in ("255.255.255.255",) for b in broadcasts) and \
                len(found) >= len(broadcasts):
            break  # unicast/loopback targets: no more answers to wait for
    s.close()
    return found


# --------------------------------------------------------------------------------------------------------------
# Session

class Session:
    def __init__(self, host, port=TCP_PORT, timeout=30.0, out=None):
        self.out = out or Out()
        self.sock = socket.create_connection((host, port), timeout=10.0)
        self.sock.settimeout(timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.seq = 0
        self.host = host
        t, _, body = self.recv()
        if t != HELLO:
            raise LinkError("expected HELLO, got %s" % NAMES.get(t, t))
        self.hello = kv_parse(body)
        if int(self.hello.get("proto", 0)) != PROTO:
            raise LinkError("protocol %s on the console, %d here: update the launcher" %
                            (self.hello.get("proto"), PROTO))
        self.frames = 0  # DATA frames sent (for the debug options)

    # framing
    def send(self, t, payload=b"", flags=0):
        self.sock.sendall(struct.pack(">2sBBII", b"R1", t, flags, len(payload), self.seq) + payload)
        self.seq += 1

    def send_data(self, offset, data, nohash=False, corrupt=False):
        crc = 0 if nohash else zlib.crc32(data) & 0xFFFFFFFF
        if corrupt:
            crc ^= 0xDEADBEEF
        hdr = struct.pack(">2sBBII", b"R1", DATA, 0, 12 + len(data), self.seq) + struct.pack(">QI", offset, crc)
        self.seq += 1
        self.frames += 1
        self.sock.sendall(hdr + data)

    def _recv_exact(self, n):
        b = bytearray()
        while len(b) < n:
            c = self.sock.recv(n - len(b))
            if not c:
                raise LinkError("the console closed the connection")
            b += c
        return bytes(b)

    def recv(self):
        h = self._recv_exact(12)
        magic, t, flags, length, _seq = struct.unpack(">2sBBII", h)
        if magic != b"R1":
            raise LinkError("bad frame from the console")
        return t, flags, self._recv_exact(length) if length else b""

    def poll(self):
        """A message if one is waiting, else None."""
        r, _, _ = select.select([self.sock], [], [], 0)
        return self.recv() if r else None

    def expect(self, *types):
        while True:
            t, f, body = self.recv()
            if t in types:
                return t, kv_parse(body) if t != MANIFEST else body
            if t == ERROR:
                e = kv_parse(body)
                raise LinkError("console: %s (%s)" % (e.get("msg", "?"), e.get("code", "?")))
            if t in (ACK, PONG):
                continue
            raise LinkError("unexpected %s" % NAMES.get(t, t))

    # commands
    def auth(self, code):
        key = hashlib.sha256(b"rac1-link pairing v1:" + code.encode()).digest()
        nonce_c = self.hello["nonce"]
        nonce_p = secrets.token_hex(16)
        mac = hmac.new(key, ("pc" + nonce_c + nonce_p).encode(), hashlib.sha256).hexdigest()
        self.send(AUTH, kv_make(nonce=nonce_p, mac=mac, client=socket.gethostname()[:32]))
        _, r = self.expect(AUTH_OK)
        want = hmac.new(key, ("console" + nonce_p + nonce_c).encode(), hashlib.sha256).hexdigest()
        if not hmac.compare_digest(want, r.get("mac", "")):
            raise LinkError("the console's answer does not match: not the console you paired with")

    def manifest(self):
        self.send(GET_MANIFEST)
        old = self.sock.gettimeout()
        self.sock.settimeout(max(old or 0, 600.0))  # (the console checks each committed file on the card: thousands)
        try:
            _, body = self.expect(MANIFEST)
        finally:
            self.sock.settimeout(old)
        files, parts = {}, {}
        for line in body.decode("utf-8", "replace").splitlines():
            f = line.split("\t")
            if f[0] == "F" and len(f) == 4:
                files[f[3]] = (int(f[1]), f[2])
            elif f[0] == "P" and len(f) == 5:
                parts[f[3]] = (int(f[1]), f[2], int(f[4]))
        return files, parts

    def adopt(self, entries):
        """entries: (path, size, sha1). The console records as committed the files already on its card with that size and
        SHA-1. Returns {"had", "adopted", "missing": [paths], "differ": [paths], "hashed", "ms"}."""
        res = {"had": 0, "adopted": 0, "missing": [], "differ": [], "hashed": 0, "ms": 0}
        batch, size = [], 0
        entries = list(entries)
        for i, (path, sz, sha) in enumerate(entries):
            line = "%d\t%s\t%s\n" % (sz, sha, path)
            batch.append(line)
            size += len(line)
            if size < ADOPT_BATCH and i + 1 < len(entries):
                continue
            self.send(ADOPT, "".join(batch).encode())
            batch, size = [], 0
            while True:
                t, _, body = self.recv()
                if t == ACK:
                    self.out.event("adopt_progress", hashed=res["hashed"] + int(kv_parse(body).get("offset", 0)))
                    continue
                if t == ERROR:
                    raise LinkError("console: %s" % kv_parse(body).get("msg"))
                if t != ADOPT_OK:
                    raise LinkError("unexpected %s" % NAMES.get(t, t))
                break
            for line in body.decode("utf-8", "replace").splitlines():
                if "=" in line and "\t" not in line:
                    k, v = line.split("=", 1)
                    if k in ("had", "adopted", "hashed", "ms"):
                        res[k] += int(v)
                elif line[:2] in ("M\t", "D\t"):
                    res["missing" if line[0] == "M" else "differ"].append(line[2:])
            self.out.event("adopt", done=i + 1, total=len(entries), **{k: res[k] if isinstance(res[k], int) else
                                                                        len(res[k]) for k in res})
        return res

    def send_pipelined(self, items, on_done, nohash=False):
        """Small files in a row (F_PIPE): each file's BEGIN_FILE, DATA and END_FILE sent without waiting, with at most
        PIPE_WINDOW bytes / PIPE_FILES files unanswered. items: (path, size, sha1, read_all() -> bytes, ...).
        on_done(item, res) in order: res is FILE_OK's dict, or {"bad": reason, "code": code}."""
        inflight = collections.deque()
        pending = 0

        def answer(msg):
            nonlocal pending
            t, _, body = msg
            if t in (ACK, PONG):
                return
            if t == ERROR:
                raise LinkError("console: %s" % kv_parse(body).get("msg"))
            if t not in (FILE_OK, FILE_BAD):
                raise LinkError("unexpected %s" % NAMES.get(t, t))
            if not inflight:
                raise LinkError("an answer for no file")
            item = inflight.popleft()
            pending -= item[1]
            r = kv_parse(body)
            on_done(item, r if t == FILE_OK else {"bad": r.get("reason", "?"), "code": r.get("code", "")})

        for item in items:
            path, size, sha, read_all = item[:4]
            if size > FILE_MAX:
                raise LinkError("%s: over 4 GiB - 1 (FAT32)" % path)
            while inflight and (pending + size > PIPE_WINDOW or len(inflight) >= PIPE_FILES):
                answer(self.recv())
            data = read_all()
            if len(data) != size:
                raise LinkError("%s: changed while it was sent (%d bytes, %d expected)" % (path, len(data), size))
            kw = kv_make(path=path, size=size, **({"sha1": sha} if sha else {}))
            frames = [struct.pack(">2sBBII", b"R1", BEGIN_FILE, F_PIPE | (F_NOHASH if nohash else 0), len(kw),
                                  self.seq), kw]
            self.seq += 1
            for off in range(0, size, CHUNK):
                chunk = data[off:off + CHUNK]
                crc = 0 if nohash else zlib.crc32(chunk) & 0xFFFFFFFF
                frames.append(struct.pack(">2sBBII", b"R1", DATA, 0, 12 + len(chunk), self.seq) +
                              struct.pack(">QI", off, crc))
                frames.append(chunk)
                self.seq += 1
            frames.append(struct.pack(">2sBBII", b"R1", END_FILE, 0, 0, self.seq))
            self.seq += 1
            inflight.append(item)
            pending += size
            self.sock.sendall(b"".join(frames))
            while True:
                m = self.poll()
                if m is None:
                    break
                answer(m)
        while inflight:
            answer(self.recv())

    def progress_text(self, text):
        self.send(PROGRESS, kv_make(text=text.replace("\n", " ")[:110]))

    def bye(self):
        try:
            self.send(BYE)
        except OSError:
            pass
        self.sock.close()

    def send_stream(self, name, size, sha1, reader, flags=0, path=None, dbg=None):
        """Send one file. reader(offset, n) -> bytes. Returns the console's FILE_OK dict; raises on FILE_BAD."""
        dbg = dbg or {}
        if size > FILE_MAX:
            raise LinkError("%s: over 4 GiB - 1 (FAT32)" % name)
        nohash = bool(flags & F_NOHASH)
        kw = {"size": size}
        if path is not None:
            kw["path"] = path
        if sha1:
            kw["sha1"] = sha1
        self.send(BEGIN_FILE, kv_make(**kw), flags)
        _, r = self.expect(READY)
        start = int(r["offset"])
        if start:
            self.out.info("  resuming %s at %.1f MiB" % (name, start / MiB))
            self.out.event("resume", file=name, offset=start)
        t0 = time.monotonic()
        acked = start
        rate = dbg.get("rate")  # MiB/s cap (tests)
        drop_after = dbg.get("drop_after")
        corrupt = set(dbg.get("corrupt", ()))
        corrupt_all = dbg.get("corrupt_all", False)
        sent_bytes = 0

        def handle(msg):
            nonlocal acked, pos
            t, _, body = msg
            if t == ACK:
                acked = int(kv_parse(body)["offset"])
            elif t == RESEND:
                pos = int(kv_parse(body)["offset"])
                self.out.info("  %s: the console asked to resend from %.1f MiB (bad CRC)" % (name, pos / MiB))
                return "resend"
            elif t == ERROR:
                e = kv_parse(body)
                raise LinkError("console: %s" % e.get("msg"))
            elif t == FILE_BAD:
                raise LinkError("%s: FILE_BAD %s" % (name, kv_parse(body).get("reason")))
            elif t == FILE_OK:
                return kv_parse(body)
            elif t != PONG:
                raise LinkError("unexpected %s" % NAMES.get(t, t))
            return None

        pos = start
        while True:
            while pos < size:
                n = min(CHUNK, size - pos)
                data = reader(pos, n)
                frame = self.frames
                bad = corrupt_all or frame in corrupt
                corrupt.discard(frame)
                self.send_data(pos, data, nohash, bad)
                pos += n
                sent_bytes += n
                if drop_after is not None and sent_bytes >= drop_after * MiB:
                    self.sock.close()
                    raise LinkError("debug: connection dropped after %d MiB" % drop_after)
                if rate:
                    ahead = sent_bytes / MiB / rate - (time.monotonic() - t0)
                    if ahead > 0:
                        time.sleep(ahead)
                while True:
                    m = self.poll()
                    if m is None:
                        break
                    if handle(m) == "resend":
                        break
                self.out.progress(name, pos, size, t0)
            self.send(END_FILE)
            while True:
                res = handle(self.recv())
                if res == "resend":
                    break
                if isinstance(res, dict):
                    wall = time.monotonic() - t0
                    res["pc_wall_s"] = wall
                    res["start"] = start
                    self.out.progress(name, size, size, t0, force=True)
                    if not self.out.json and not self.out.quiet and self.out.tty:
                        sys.stderr.write("\n")
                    return res
            # resend: loop again from pos


# --------------------------------------------------------------------------------------------------------------
# Connecting

def pick_console(args, out):
    if args.console:
        return args.console
    found = discover(args.udp_port, args.broadcast, args.discover_timeout, out)
    if len(found) == 1:
        ip = next(iter(found))
        out.info("console: %s (%s)" % (ip, found[ip].get("name")))
        return ip
    if len(found) > 1:
        raise LinkError("several consoles answered (%s): choose one with --console" % ", ".join(sorted(found)))
    last = load_config().get("last")
    if last:
        out.info("no answer to the broadcast; trying the last console, %s" % last)
        return last
    raise LinkError("no console found: open rac1-link on the Wii U, or give its IP (shown on the TV) with --console")


def connect(args, out, need_auth=True):
    ip = pick_console(args, out)
    s = Session(ip, args.port, out=out)
    if not need_auth:
        return s
    cfg = load_config()
    serial = s.hello.get("serial", "")
    code = args.code or cfg["consoles"].get(serial, {}).get("code")
    if not code:
        s.bye()
        raise LinkError("not paired with this console: run `rac1_link.py pair --code NNNNNN` with the code on the TV")
    s.auth(code)
    cfg["consoles"].setdefault(serial, {}).update(code=code, ip=ip, name=s.hello.get("name"))
    cfg["last"] = ip
    save_config(cfg)
    return s


def sha1_file(path, out=None):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        while True:
            b = f.read(4 * MiB)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def dbg_opts(args):
    return {
        "rate": args.debug_rate,
        "drop_after": args.debug_drop_after,
        "corrupt": [int(x) for x in args.debug_corrupt.split(",")] if args.debug_corrupt else [],
        "corrupt_all": args.debug_corrupt_all,
    }


def report(out, label, res, size):
    mib = int(res.get("bytes", size)) / MiB

    def rate(ms_key):
        ms = int(res.get(ms_key, 0))
        return "%.2f" % (mib / (ms / 1000.0)) if ms > 0 else "-"

    line = ("%s: %.1f MiB, PC %.2f MiB/s | console: wall %s, net %s, sd %s, hash %s MiB/s; waited for the card %s ms" %
            (label, mib, mib / max(res["pc_wall_s"], 1e-9), rate("wall_ms"), rate("net_ms"), rate("sd_ms"),
             rate("hash_ms"), res.get("wait_ms", "-")))
    if int(res.get("rehash_ms", 0)):
        line += ", re-hash on resume %s ms" % res["rehash_ms"]
    out.info(line)
    out.event("result", label=label, **{k: v for k, v in res.items()})


# --------------------------------------------------------------------------------------------------------------
# Commands

def cmd_discover(args, out):
    found = discover(args.udp_port, args.broadcast, args.discover_timeout, out)
    if not found:
        out.info("no console answered (is rac1-link open on the Wii U? same network?)")
        return 1
    for ip, info in sorted(found.items()):
        if not out.json:
            print("%s  %s %s  port %s  serial %s%s" % (ip, info.get("name"), info.get("version"), info.get("port"),
                                                      info.get("serial"), "  (busy)" if info.get("busy") == "1"
                                                      else ""))
    return 0


def cmd_pair(args, out):
    if not args.code or not (len(args.code) == 6 and args.code.isdigit()):
        raise LinkError("--code: the 6 digits shown on the TV")
    s = connect(args, out)
    out.info("paired with %s (serial %s)" % (s.host, s.hello.get("serial")))
    s.bye()
    return 0


def cmd_status(args, out):
    s = connect(args, out)
    files, parts = s.manifest()
    h = s.hello
    out.info("%s %s at %s, serial %s, free %.1f MiB" % (h.get("name"), h.get("version"), s.host, h.get("serial"),
                                                        int(h.get("free", 0)) / MiB))
    out.info("%d committed file(s), %.1f MiB; %d partial" % (len(files), sum(v[0] for v in files.values()) / MiB,
                                                             len(parts)))
    for p, (size, _sha, have) in sorted(parts.items()):
        out.info("  partial: %s %.1f/%.1f MiB" % (p, have / MiB, size / MiB))
    s.bye()
    return 0


def local_items(args):
    """(local file, path on the card) for send / adopt."""
    src = os.path.abspath(args.local)
    if os.path.isdir(src):
        dest = (args.dest or "rac1-native/" + os.path.basename(src)).rstrip("/")
        items = []
        for root, dirs, fnames in os.walk(src):
            dirs.sort()
            for fn in sorted(fnames):
                full = os.path.join(root, fn)
                rel = os.path.relpath(full, src).replace(os.sep, "/")
                items.append((full, dest + "/" + rel))
    elif os.path.isfile(src):
        dest = args.dest or "rac1-native/" + os.path.basename(src)
        if dest.endswith("/"):
            dest += os.path.basename(src)
        items = [(src, dest)]
    else:
        raise LinkError("%s: no such file or folder" % args.local)
    return items


def cmd_adopt(args, out):
    items = local_items(args)
    out.info("%d file(s): hashing" % len(items))
    entries = [(d, os.path.getsize(f), sha1_file(f)) for f, d in items]
    s = connect(args, out)
    r = s.adopt(entries)
    out.info("adopt: %d already recorded, %d adopted, %d missing, %d different; the console hashed %.1f MiB in %.1f s" %
             (r["had"], r["adopted"], len(r["missing"]), len(r["differ"]), r["hashed"] / MiB, r["ms"] / 1000.0))
    out.event("result", label="adopt", had=r["had"], adopted=r["adopted"], missing=len(r["missing"]),
              differ=len(r["differ"]), hashed=r["hashed"], ms=r["ms"])
    s.bye()
    return 0


def cmd_send(args, out):
    items = local_items(args)
    total = sum(os.path.getsize(f) for f, _ in items)
    out.info("%d file(s), %.1f MiB: hashing" % (len(items), total / MiB))
    hashed = [(f, d, os.path.getsize(f), sha1_file(f)) for f, d in items]
    s = connect(args, out)
    files, _parts = s.manifest()
    todo = [x for x in hashed if args.force or files.get(x[1]) != (x[2], x[3])]
    skipped = len(hashed) - len(todo)
    if skipped:
        out.info("%d file(s) already on the console, unchanged: skipped" % skipped)
    t0 = time.monotonic()
    sent = 0
    bad = 0
    done_files = 0
    total_todo = sum(x[2] for x in todo)

    def pipe_done(item, res):
        nonlocal sent, bad, done_files
        done_files += 1
        if "bad" in res:
            out.info("FAILED %s: %s" % (item[0], res["bad"]))
            out.event("failed", file=item[0], reason=res["bad"], code=res["code"])
            bad += 1
            return
        sent += item[1]
        out.progress("%d/%d files" % (done_files, len(todo)), sent, total_todo, t0)

    def reader_of(full):
        def read_all():
            with open(full, "rb") as f:
                return f.read()
        return read_all

    i = 0
    while i < len(todo):
        j = i
        while not args.no_pipe and j < len(todo) and todo[j][2] <= PIPE_MAX:
            j += 1
        if j - i >= 2:  # a run of small files: in a row
            s.progress_text("files %d-%d of %d" % (i + 1, j, len(todo)))
            s.send_pipelined([(d, sz, sha, reader_of(f)) for f, d, sz, sha in todo[i:j]], pipe_done)
            i = j
            continue
        full, dpath, size, sha = todo[i]
        i += 1
        with open(full, "rb") as f:
            def reader(off, n, f=f):
                f.seek(off)
                return f.read(n)
            if len(todo) > 1:
                s.progress_text("file %d/%d: %s" % (i, len(todo), dpath))
            try:
                res = s.send_stream(dpath, size, sha, reader, path=dpath, dbg=dbg_opts(args))
            except LinkError as e:
                if "FILE_BAD" in str(e) or "path not allowed" in str(e):
                    out.info("FAILED %s: %s" % (dpath, e))
                    out.event("failed", file=dpath, reason=str(e))
                    bad += 1
                    continue
                raise
        sent += size - res["start"]
        done_files += 1
        if size >= MiB or len(todo) == 1:
            report(out, dpath, res, size)
    wall = time.monotonic() - t0
    out.info("done: %d sent, %d skipped, %d failed; %.1f MiB in %.1f s (%.2f MiB/s)" %
             (len(todo) - bad, skipped, bad, sent / MiB, wall, sent / MiB / max(wall, 1e-9)))
    s.bye()
    return 1 if bad else 0


def gen_reader(size, seed=1):
    """Deterministic generated bytes: one random 1 MiB block, its first 8 bytes replaced by the block number."""
    base = random.Random(seed).randbytes(CHUNK) if hasattr(random.Random, "randbytes") else \
        bytes(random.Random(seed).getrandbits(8) for _ in range(CHUNK))

    def reader(off, n):
        assert off % CHUNK == 0
        return (struct.pack(">Q", off // CHUNK) + base[8:])[:n]

    def sha1():
        h = hashlib.sha1()
        for off in range(0, size, CHUNK):
            h.update(reader(off, min(CHUNK, size - off)))
        return h.hexdigest()
    return reader, sha1


def cmd_bench(args, out):
    modes = ["net", "net-hash", "sd", "both"] if args.mode == "all" else [args.mode]
    size = args.mb * MiB
    s = connect(args, out)
    results = {}
    for m in modes:
        s.progress_text("bench %s %d MiB" % (m, args.mb))
        if m == "sd":
            s.send(BENCH_SD, kv_make(mb=args.mb, chunk_kib=args.chunk_kib, files=args.files,
                                     fwrite=int(args.fwrite)))
            t0 = time.monotonic()
            _, r = s.expect(BENCH_RESULT)
            mib = int(r["bytes"]) / MiB
            w, rd = int(r["write_ms"]) / 1000.0, int(r["read_ms"]) / 1000.0
            out.info("sd: %.1f MiB in %s file(s), %d KiB %s: write %.2f MiB/s, read %.2f MiB/s, rename %s ms, "
                     "delete %s ms (%.1f s)" % (mib, r["files"], args.chunk_kib,
                                                "unaligned fwrite" if args.fwrite else "aligned write()",
                                                mib / max(w, 1e-9), int(r["read_bytes"]) / MiB / max(rd, 1e-9),
                                                r["rename_ms"], r["delete_ms"], time.monotonic() - t0))
            out.event("result", label="sd", **r)
            results[m] = r
            continue
        reader, sha1 = gen_reader(size)
        flags = {"net": F_DISCARD | F_NOHASH, "net-hash": F_DISCARD, "both": F_BENCH}[m]
        digest = None if flags & F_NOHASH else sha1()
        res = s.send_stream("bench-" + m, size, digest, reader, flags=flags, dbg=dbg_opts(args))
        report(out, m, res, size)
        results[m] = res
    s.bye()
    return 0


def cmd_config(args, out):
    s = connect(args, out)
    kw = {k: v for k, v in (("rcvbuf", args.rcvbuf), ("winscale", args.winscale), ("sack", args.sack),
                            ("noslowstart", args.noslowstart), ("rusrbuf", args.rusrbuf),
                            ("somemopt", args.somemopt), ("somemopt_big", args.somemopt_big)) if v is not None}
    s.send(CONFIG, kv_make(**kw))
    _, r = s.expect(CONFIG_OK)
    out.info("console settings: " + ", ".join("%s=%s" % kv for kv in sorted(r.items())))
    out.info("socket options apply from the next connection; somemopt from the next start of rac1-link")
    s.bye()
    return 0


def cmd_quit(args, out):
    s = connect(args, out)
    s.send(QUIT)
    s.sock.close()
    out.info("asked rac1-link on %s to close (back to the Wii U Menu)" % s.host)
    return 0


def cmd_launch(args, out):
    s = connect(args, out)
    s.send(LAUNCH)
    s.sock.close()
    out.info("asked rac1-link on %s to start the game" % s.host)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--console", help="the console's IP (default: discovery, then the last one)")
    ap.add_argument("--port", type=int, default=TCP_PORT)
    ap.add_argument("--udp-port", type=int, default=UDP_PORT)
    ap.add_argument("--broadcast", action="append", help="discovery target (default 255.255.255.255); repeatable")
    ap.add_argument("--discover-timeout", type=float, default=5.0)
    ap.add_argument("--code", help="the pairing code (default: the one saved by `pair`)")
    ap.add_argument("--json", action="store_true", help="JSON lines on stdout (for a GUI)")
    ap.add_argument("--quiet", action="store_true")
    dbg = ap.add_argument_group("debug (tests)")
    dbg.add_argument("--debug-rate", type=float, help="cap the send rate, MiB/s")
    dbg.add_argument("--debug-drop-after", type=int, help="drop the connection after N MiB of a file")
    dbg.add_argument("--debug-corrupt", help="corrupt the CRC of these DATA frames (0-based, comma-separated)")
    dbg.add_argument("--debug-corrupt-all", action="store_true", help="corrupt every DATA frame's CRC")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("discover", help="find consoles running rac1-link")
    p = sub.add_parser("pair", help="pair with the code shown on the TV")
    p.add_argument("--code", dest="code", required=True)
    sub.add_parser("status", help="the console's free space and installed files")
    p = sub.add_parser("send", help="send a file or a folder (resumes, skips unchanged files)")
    p.add_argument("local")
    p.add_argument("--dest", help="where on the SD card, e.g. rac1-native/assets (default rac1-native/<name>)")
    p.add_argument("--force", action="store_true", help="send even files the console already has")
    p.add_argument("--no-pipe", action="store_true", help="send small files one by one (a round trip each)")
    p = sub.add_parser("adopt", help="record as installed the files of a local file or folder that the console's card "
                                     "already has (copied by hand): the console reads and hashes them")
    p.add_argument("local")
    p.add_argument("--dest", help="where on the SD card, as for send")
    p = sub.add_parser("bench", help="measure: net (receive and discard), net-hash (+ CRC/SHA-1), sd (write "
                                     "generated bytes on the console), both (network + SD), all")
    p.add_argument("--mode", choices=["net", "net-hash", "sd", "both", "all"], default="all")
    p.add_argument("--mb", type=int, default=256, help="MiB per test (default 256)")
    p.add_argument("--chunk-kib", type=int, default=1024, help="sd: write size in KiB (max 1024)")
    p.add_argument("--files", type=int, default=1, help="sd: split into this many files")
    p.add_argument("--fwrite", action="store_true", help="sd: unaligned fwrite instead of aligned write()")
    p = sub.add_parser("config", help="console socket options (next connection) and somemopt (next start)")
    for o in ("rcvbuf", "winscale", "sack", "noslowstart", "rusrbuf", "somemopt", "somemopt_big"):
        p.add_argument("--" + o.replace("_", "-"), dest=o, type=int)
    sub.add_parser("quit", help="close rac1-link on the console (back to the Wii U Menu)")
    sub.add_parser("launch", help="start the game on the console (sd:/wiiu/apps/rac1-native/rac1-native.rpx)")
    args = ap.parse_args(argv)
    out = Out(args.json, args.quiet)
    try:
        return {"discover": cmd_discover, "pair": cmd_pair, "status": cmd_status, "send": cmd_send, "adopt": cmd_adopt,
                "bench": cmd_bench, "config": cmd_config, "quit": cmd_quit,
                "launch": cmd_launch}[args.cmd](args, out)
    except (LinkError, OSError) as e:
        out.info("error: %s" % e)
        out.event("error", text=str(e))
        return 2


if __name__ == "__main__":
    sys.exit(main())
