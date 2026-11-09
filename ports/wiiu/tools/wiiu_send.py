#!/usr/bin/env python3
"""wiiu_send.py: sends files to rac1-native's Wii U receiver (ports/wiiu/receiver), which writes them to the console's SD card.

    wiiu_send.py <console address> <pairing code> [--launch | --quit] <local>=<sd path> ...

<local> a file or a folder (sent whole, its tree kept); <sd path> where it goes, relative to the SD card's root, inside
rac1-native/ or wiiu/apps/rac1-native/ - e.g.
    build-wiiu-mt/rac1_wiiu.rpx=wiiu/apps/rac1-native/rac1-native.rpx  assets/disc=rac1-native/assets/disc
A file already on the card at the same size and CRC-32 is skipped; one cut short goes on from where it stopped.
--launch: the game started at the end; --quit: the console back to its menu. The protocol: the receiver's main.cpp.
"""
import os
import socket
import sys
import time
import zlib

PORT = 47821


def crc32_of(path):
    crc = 0
    with open(path, "rb") as f:
        while True:
            b = f.read(1 << 20)
            if not b:
                return crc & 0xFFFFFFFF
            crc = zlib.crc32(b, crc)


class Conn:
    def __init__(self, host, code):
        self.s = socket.create_connection((host, PORT), timeout=60)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1 << 20)
        self.buf = b""
        r = self.ask(f"HELLO 1 {code}")
        if not r.startswith("OK"):
            sys.exit(f"the receiver said: {r}")
        print(f"connected: {r}")

    def line(self):
        while b"\n" not in self.buf:
            b = self.s.recv(4096)
            if not b:
                raise ConnectionError("the receiver closed the connection")
            self.buf += b
        l, self.buf = self.buf.split(b"\n", 1)
        return l.decode()

    def ask(self, cmd):
        self.s.sendall(cmd.encode() + b"\n")
        return self.line()


def files_of(local, remote):
    if os.path.isfile(local):
        yield local, remote
        return
    for root, dirs, names in os.walk(local):
        dirs.sort()
        for n in sorted(names):
            p = os.path.join(root, n)
            yield p, remote.rstrip("/") + "/" + os.path.relpath(p, local).replace(os.sep, "/")


def main():
    args = [a for a in sys.argv[1:] if a not in ("--launch", "--quit")]
    if len(args) < 3:
        sys.exit(__doc__)
    host, code, specs = args[0], args[1], args[2:]
    jobs = []
    for spec in specs:
        local, _, remote = spec.partition("=")
        if not remote or not os.path.exists(local):
            sys.exit(f"bad item: {spec}")
        jobs += list(files_of(local, remote))
    total = sum(os.path.getsize(p) for p, _ in jobs)
    print(f"{len(jobs)} files, {total >> 20} MB")
    c = Conn(host, code)
    free = c.ask("FREE")
    print(f"the SD card: {free}")
    have = {}  # (what the receiver wrote whole: path -> (size, crc))
    n = int(c.ask("MANIFEST").split()[1])
    for _ in range(n):
        p, size, crc = c.line().split()
        have[p] = (int(size), int(crc, 16))
    sent = skipped = 0
    t0 = time.time()
    shown = [0.0]

    def progress(i):
        now = time.time()
        if now - shown[0] > 2:
            shown[0] = now
            print(f"  {i + 1}/{len(jobs)} files, {sent >> 20} MB sent ({(sent >> 20) / max(now - t0, 0.001):.1f} MB/s)", flush=True)

    pending = []  # (small files sent ahead, their OK still to read)

    def drain(keep):
        while len(pending) > keep:
            r = c.line()
            name = pending.pop(0)
            if r != "OK":
                sys.exit(f"{name}: {r}")

    for i, (local, remote) in enumerate(jobs):
        size = os.path.getsize(local)
        crc = crc32_of(local)
        if have.get(remote) == (size, crc):
            skipped += 1
            continue
        if size < (4 << 20):  # (small: PUTS, several in flight)
            with open(local, "rb") as f:
                data = f.read()
            c.s.sendall(f"PUTS {remote} {size} {crc:08x}\n".encode() + data)
            pending.append(remote)
            sent += size
            drain(16)
            progress(i)
            continue
        drain(0)
        r = c.ask(f"PUT {remote} {size} {crc:08x}")
        if not r.startswith("READY "):
            sys.exit(f"{remote}: {r}")
        offset = int(r.split()[1])
        with open(local, "rb") as f:
            f.seek(offset)
            while True:
                b = f.read(1 << 20)
                if not b:
                    break
                c.s.sendall(b)
                sent += len(b)
                progress(i)
        r = c.line()
        if r != "OK":
            sys.exit(f"{remote}: {r}")
    drain(0)
    secs = time.time() - t0
    print(f"done: {len(jobs) - skipped} sent, {skipped} already there; {sent >> 20} MB in {secs:.0f} s ({(sent >> 20) / max(secs, 0.001):.1f} MB/s)")
    print(c.ask("LAUNCH" if "--launch" in sys.argv else "QUIT" if "--quit" in sys.argv else "BYE"))


if __name__ == "__main__":
    main()
