#!/usr/bin/env python3
"""Loopback test of rac1-link: the console's receiver built for the PC (../host, rac1-link-host) against
rac1_link.py, on generated data only.

  test_loopback.py --host-bin BUILD/rac1-link-host --work DIR [--mb 200]

DIR gets a fake SD card and a random file of --mb MiB; everything in it is deleted at the end (unless --keep).
Covers: self-test, discovery, pairing (wrong and right code, unpaired commands), send + SHA-1 on disk, skip of an
unchanged file, resume after a dropped connection, after the PC killed and after the receiver killed, CRC failure
(recovered and fatal), the folder whitelist, a folder send, the bench modes, config, quit.
"""
import argparse
import hashlib
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
CLI = os.path.join(HERE, "rac1_link.py")
CODE = "246810"
PORT, UDP = 47299, 47298
MiB = 1 << 20

results = []


def check(name, cond, detail=""):
    results.append((name, bool(cond)))
    print("%s %s%s" % ("PASS" if cond else "FAIL", name, (": " + detail) if detail and not cond else ""), flush=True)
    return cond


class Receiver:
    def __init__(self, binary, root, log):
        self.binary, self.root, self.logpath = binary, root, log
        self.p = None

    def start(self):
        self.log = open(self.logpath, "a")
        self.p = subprocess.Popen([self.binary, "--root", self.root, "--port", str(PORT), "--udp-port", str(UDP),
                                   "--code", CODE], stderr=self.log)
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.2).close()
                time.sleep(0.3)  # let it finish that probe session
                return
            except OSError:
                time.sleep(0.1)
        raise RuntimeError("receiver did not start")

    def stop(self, kill=False):
        if self.p:
            self.p.send_signal(signal.SIGKILL if kill else signal.SIGTERM)
            self.p.wait(10)
            self.p = None
            self.log.close()


def cli(*args, env=None, timeout=300, code=True):
    cmd = [sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "--udp-port", str(UDP)]
    if code:
        cmd += ["--code", CODE]
    return subprocess.run(cmd + list(args), env=env, capture_output=True, text=True, timeout=timeout)


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(4 * MiB), b""):
            h.update(b)
    return h.hexdigest()


def wait_part(path, at_least, timeout=60):
    end = time.time() + timeout
    while time.time() < end:
        if os.path.exists(path) and os.path.getsize(path) >= at_least:
            return True
        time.sleep(0.05)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host-bin", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--mb", type=int, default=200)
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    work = os.path.abspath(a.work)
    sd = os.path.join(work, "sd")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(sd)
    env = dict(os.environ, RAC1_LINK_CONFIG=os.path.join(work, "pc-config.json"))
    big = os.path.join(work, "big.bin")
    with open(big, "wb") as f:
        for _ in range(a.mb):
            f.write(os.urandom(MiB))
        f.write(os.urandom(12345))  # a tail that is not a multiple of 64
    big_sha = sha1(big)
    size = os.path.getsize(big)
    rx = Receiver(a.host_bin, sd, os.path.join(work, "receiver.log"))
    t_all = time.time()
    try:
        r = subprocess.run([a.host_bin, "--selftest"], capture_output=True, text=True)
        check("selftest (hashes, path whitelist, peer filter)", r.returncode == 0, r.stdout + r.stderr)
        rx.start()

        # discovery (unicast to loopback stands in for the broadcast)
        r = subprocess.run([sys.executable, CLI, "--udp-port", str(UDP), "--broadcast", "127.0.0.1",
                            "--discover-timeout", "3", "discover"], capture_output=True, text=True, env=env)
        check("discover", r.returncode == 0 and "127.0.0.1" in r.stdout and "rac1-link-host" in r.stdout,
              r.stdout + r.stderr)

        # pairing
        r = subprocess.run([sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "status"],
                           capture_output=True, text=True, env=env)
        check("unpaired PC is told to pair", r.returncode != 0 and "pair" in r.stderr, r.stderr)
        r = subprocess.run([sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "pair", "--code",
                            "111111"], capture_output=True, text=True, env=env)
        check("wrong pairing code refused", r.returncode != 0 and "wrong pairing code" in r.stderr, r.stderr)
        r = subprocess.run([sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "pair", "--code",
                            CODE], capture_output=True, text=True, env=env)
        check("pair", r.returncode == 0, r.stderr)
        r = subprocess.run([sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "status"],
                           capture_output=True, text=True, env=env)
        check("saved pairing used", r.returncode == 0 and "committed" in r.stderr, r.stderr)
        # a raw client that skips AUTH
        s = socket.create_connection(("127.0.0.1", PORT))
        s.settimeout(5)

        def frame():
            h = b""
            while len(h) < 12:
                h += s.recv(12 - len(h))
            n = struct.unpack(">I", h[4:8])[0]
            body = b""
            while len(body) < n:
                body += s.recv(n - len(body))
            return h + body
        frame()  # HELLO
        s.sendall(struct.pack(">2sBBII", b"R1", 5, 0, 0, 0))  # GET_MANIFEST
        reply = frame()
        s.close()
        check("commands refused before AUTH", reply[2:3] == bytes([4]) and b"not paired" in reply, repr(reply[:60]))

        # send + verify on disk
        dest = "rac1-native/assets/big.bin"
        on_sd = os.path.join(sd, dest)
        t0 = time.time()
        r = cli("send", big, "--dest", dest, env=env)
        dt = time.time() - t0
        ok = r.returncode == 0 and os.path.exists(on_sd) and sha1(on_sd) == big_sha
        check("send %d MiB (%.1f s, %.0f MiB/s)" % (a.mb, dt, size / MiB / dt), ok, r.stderr[-800:])
        print("   ", [l for l in r.stderr.splitlines() if "console:" in l][-1:], flush=True)
        check("no .part left", not os.path.exists(on_sd + ".part"))
        man = open(os.path.join(sd, "rac1-native/.link/manifest.txt")).read()
        check("manifest has the file", "F\t%d\t%s\t%s" % (size, big_sha, dest) in man, man)
        r = cli("send", big, "--dest", dest, env=env)
        check("unchanged file skipped", r.returncode == 0 and "skipped" in r.stderr and "1 skipped" in r.stderr,
              r.stderr)
        os.remove(on_sd)

        # resume after a dropped connection
        dest2 = "rac1-native/assets/resume1.bin"
        on2 = os.path.join(sd, dest2)
        r = cli("--debug-drop-after", "70", "send", big, "--dest", dest2, env=env)
        time.sleep(0.5)
        partsz = os.path.getsize(on2 + ".part") if os.path.exists(on2 + ".part") else 0
        check("dropped connection keeps the .part", r.returncode != 0 and partsz >= 64 * MiB,
              "rc %d part %d" % (r.returncode, partsz))
        r = cli("send", big, "--dest", dest2, env=env)
        m = [l for l in r.stderr.splitlines() if "resuming" in l]
        check("resume after drop", r.returncode == 0 and m and sha1(on2) == big_sha, r.stderr[-800:])
        print("   ", m, flush=True)
        os.remove(on2)

        # resume after the PC process is killed
        dest3 = "rac1-native/assets/resume2.bin"
        on3 = os.path.join(sd, dest3)
        cmd = [sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "--code", CODE, "--debug-rate",
               "100", "send", big, "--dest", dest3]
        p = subprocess.Popen(cmd, env=env, stderr=subprocess.DEVNULL)
        got = wait_part(on3 + ".part", 50 * MiB)
        p.kill()
        p.wait()
        time.sleep(0.5)
        r = cli("send", big, "--dest", dest3, env=env)
        m = [l for l in r.stderr.splitlines() if "resuming" in l]
        check("resume after the PC was killed", got and r.returncode == 0 and m and sha1(on3) == big_sha,
              r.stderr[-800:])
        print("   ", m, flush=True)
        os.remove(on3)

        # resume after the receiver (the console) is killed
        dest4 = "rac1-native/assets/resume3.bin"
        on4 = os.path.join(sd, dest4)
        cmd[cmd.index(dest3)] = dest4
        p = subprocess.Popen(cmd, env=env, stderr=subprocess.DEVNULL)
        got = wait_part(on4 + ".part", 50 * MiB)
        rx.stop(kill=True)
        p.wait(30)
        rx.start()
        r = cli("send", big, "--dest", dest4, env=env)
        m = [l for l in r.stderr.splitlines() if "resuming" in l]
        check("resume after the receiver was killed", got and r.returncode == 0 and m and sha1(on4) == big_sha,
              r.stderr[-800:])
        print("   ", m, flush=True)
        os.remove(on4)

        # HOME on the console (SIGUSR1 on the stand-in): the session closes, the .part stays, resume after
        dest6 = "rac1-native/assets/home.bin"
        on6 = os.path.join(sd, dest6)
        cmd[cmd.index(dest4)] = dest6
        p = subprocess.Popen(cmd, env=env, stderr=subprocess.PIPE, text=True)
        got = wait_part(on6 + ".part", 40 * MiB)
        rx.p.send_signal(signal.SIGUSR1)
        _, perr = p.communicate(timeout=30)
        r = subprocess.run([sys.executable, CLI, "--console", "127.0.0.1", "--port", str(PORT), "--code", CODE,
                            "status"], env=env, capture_output=True, text=True)
        refused = r.returncode != 0
        rx.p.send_signal(signal.SIGUSR1)
        time.sleep(0.5)
        r = cli("send", big, "--dest", dest6, env=env)
        m = [l for l in r.stderr.splitlines() if "resuming" in l]
        check("HOME: session closed, no listener while paused, resume after", got and p.returncode != 0 and refused
              and r.returncode == 0 and m and sha1(on6) == big_sha, perr[-300:] + r.stderr[-500:])
        print("   ", m, flush=True)
        os.remove(on6)

        # CRC failures
        dest5 = "rac1-native/assets/crc.bin"
        on5 = os.path.join(sd, dest5)
        r = cli("--debug-corrupt", "5,40", "send", big, "--dest", dest5, env=env)
        n = r.stderr.count("resend from")
        check("bad CRC chunks resent (2)", r.returncode == 0 and n == 2 and sha1(on5) == big_sha, r.stderr[-800:])
        os.remove(on5)
        r = cli("--debug-corrupt-all", "send", big, "--dest", dest5, env=env)
        check("persistent bad CRC: FILE_BAD, .part removed",
              r.returncode != 0 and "FILE_BAD" in r.stderr and not os.path.exists(on5 + ".part")
              and not os.path.exists(on5), r.stderr[-800:])

        # whitelist (the console decides; the PC sends the path as given)
        small = os.path.join(work, "small.bin")
        with open(small, "wb") as f:
            f.write(os.urandom(5000))
        bad_paths = ["../escape.bin", "rac1-native/../escape.bin", "/abs.bin", "other/x.bin",
                     "rac1-native/.link/manifest.txt", "wiiu/apps/other/x.wuhb", "rac1-native/a\\b.bin",
                     "rac1-native/x.part", "wiiu/x.bin", "RAC1-NATIVE/x.bin"]
        refused = 0
        for bp in bad_paths:
            r = cli("send", small, "--dest", bp, env=env)
            refused += r.returncode != 0 and "path not allowed" in r.stderr
        outside = [os.path.join(work, "escape.bin"), os.path.join(sd, "escape.bin"), os.path.join(sd, "other"),
                   os.path.join(sd, "abs.bin"), os.path.join(sd, "wiiu/apps/other")]
        check("whitelist: %d/%d bad paths refused, nothing written outside" % (refused, len(bad_paths)),
              refused == len(bad_paths) and not any(os.path.exists(o) for o in outside))
        man = open(os.path.join(sd, "rac1-native/.link/manifest.txt")).read()
        check("manifest not overwritten through the protocol", man.startswith(("F\t", "P\t")) or man == "", man[:80])
        r = cli("send", small, "--dest", "wiiu/apps/rac1-native/rac1-link.wuhb", env=env)
        check("allowed: wiiu/apps/rac1-native/", r.returncode == 0 and
              sha1(os.path.join(sd, "wiiu/apps/rac1-native/rac1-link.wuhb")) == sha1(small), r.stderr)

        # a folder, with an update of one file (replace of an existing file)
        tree = os.path.join(work, "tree")
        for i in range(30):
            d = os.path.join(tree, "sub%d" % (i % 4))
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "f%02d.bin" % i), "wb") as f:
                f.write(os.urandom(1000 * i + 1))
        r = cli("send", tree, "--dest", "rac1-native/tree", env=env)
        same = all(sha1(os.path.join(root, fn)) == sha1(os.path.join(sd, "rac1-native/tree",
                                                                       os.path.relpath(os.path.join(root, fn), tree)))
                   for root, _, fns in os.walk(tree) for fn in fns)
        check("folder send (30 files)", r.returncode == 0 and same, r.stderr[-500:])
        with open(os.path.join(tree, "sub1", "f01.bin"), "wb") as f:
            f.write(b"changed")
        r = cli("send", tree, "--dest", "rac1-native/tree", env=env)
        check("folder resend: 1 changed file sent, 29 skipped", r.returncode == 0 and "1 sent, 29 skipped" in r.stderr
              and open(os.path.join(sd, "rac1-native/tree/sub1/f01.bin"), "rb").read() == b"changed", r.stderr[-300:])
        check("no .old left after a replace", not any(fn.endswith((".old", ".part")) for _, _, fns in os.walk(sd)
                                                        for fn in fns))

        # bench
        r = cli("bench", "--mode", "all", "--mb", "64", env=env)
        lines = [l for l in r.stderr.splitlines() if l.startswith(("net", "sd", "both"))]
        check("bench all (net, net-hash, sd, both)", r.returncode == 0 and len(lines) == 4, r.stderr[-800:])
        for l in lines:
            print("   ", l, flush=True)
        r = cli("bench", "--mode", "sd", "--mb", "32", "--files", "64", "--chunk-kib", "256", "--fwrite", env=env)
        check("bench sd, 64 files, unaligned fwrite", r.returncode == 0, r.stderr[-400:])
        print("   ", r.stderr.strip().splitlines()[-1:], flush=True)
        check("bench leaves nothing", not os.path.exists(os.path.join(sd, "rac1-native/.link/bench")))
        r = cli("config", "--rcvbuf", "262144", "--winscale", "1", "--sack", "1", env=env)
        check("config", r.returncode == 0 and "rcvbuf=262144" in r.stderr, r.stderr)
        time.sleep(0.5)
        r = cli("bench", "--mode", "net", "--mb", "32", env=env)
        check("new listener after config", r.returncode == 0, r.stderr[-300:])
        r = cli("quit", env=env)
        try:
            rc = rx.p.wait(5)
        except subprocess.TimeoutExpired:
            rc = None
        check("quit closes the receiver", r.returncode == 0 and rc == 0, r.stderr)
        if rc is not None:
            rx.p = None
            rx.log.close()
    finally:
        rx.stop()
        print("receiver log: %s" % os.path.join(work, "receiver.log"))
        if not a.keep:
            for p in (big, os.path.join(work, "small.bin")):
                if os.path.exists(p):
                    os.remove(p)
            shutil.rmtree(sd, ignore_errors=True)
            shutil.rmtree(os.path.join(work, "tree"), ignore_errors=True)
    bad = [n for n, ok in results if not ok]
    print("%d/%d passed in %.0f s%s" % (len(results) - len(bad), len(results), time.time() - t_all,
                                         "; FAILED: " + ", ".join(bad) if bad else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
