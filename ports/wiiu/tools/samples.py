#!/usr/bin/env python3
"""The console's sampling profile (ports/wiiu/game/wiiu_sampler.cpp's samples.bin) mapped on the ELF's functions.

    ports/wiiu/tools/samples.py build-wiiu/rac1_wiiu.elf samples.bin [--rows N] [--core C] [--thread NAME] [--paths N]

The ELF must be the run's own build (the .rpx's): its functions move with any change of the code. The file's self-test (the
addresses as loaded of a few functions spread over .text) is checked first - each must map back on its own name, with one
relocation for all - and nothing is printed for a build that does not match (--force: printed anyway, wrong).

Prints the threads the samples fell in, the functions by self time (the pc) and by inclusive time (the pc, the lr and the
return addresses of the stack's back chain: each function counted once per sample), and for the hottest the call paths.
Addresses outside our code: [os] (Cafe OS's libraries, named by coreinit's OSGetSymbolName when the file has the names),
[kernel]. --core / --thread keep the samples of one core / of the threads whose name holds NAME.
An old file (RAC1SMP1: pc and lr, one anchor) is read too, without the self-test.
"""
import argparse
import bisect
import collections
import os
import struct
import subprocess
import sys

BIN = os.path.join(os.environ.get('DEVKITPPC', '/opt/devkitpro/devkitPPC'), 'bin')
OS_TOP = 0x02000000  # (below: Cafe OS's libraries; no RPX code there)


class Elf:
    """.text's functions: sorted starts, sizes, mangled names (one name per address, the global one first)."""

    def __init__(self, path):
        out = subprocess.run([os.path.join(BIN, 'powerpc-eabi-readelf'), '-SW', path], capture_output=True, text=True,
                             check=True).stdout
        self.lo = self.hi = 0
        for line in out.splitlines():
            p = line.replace('[ ', '[').split()
            if len(p) > 5 and p[1] == '.text':
                self.lo = int(p[3], 16)
                self.hi = self.lo + int(p[5], 16)
        out = subprocess.run([os.path.join(BIN, 'powerpc-eabi-nm'), '-n', '-S', '--defined-only', path], capture_output=True,
                             text=True, check=True).stdout
        rank = {'T': 0, 'W': 1, 't': 2, 'w': 3}
        best = {}
        for line in out.splitlines():
            p = line.split(' ')
            if len(p) == 4:
                a, size, kind, name = int(p[0], 16), int(p[1], 16), p[2], p[3]
            elif len(p) == 3 and p[0]:
                a, size, kind, name = int(p[0], 16), 0, p[1], p[2]
            else:
                continue
            if kind not in rank or not self.lo <= a < self.hi:
                continue
            key = (rank[kind], name.startswith('_GLOBAL__'), -size)
            if a not in best or key < best[a][0]:
                best[a] = (key, size, name)
        self.addrs = sorted(best)
        self.sizes = [best[a][1] for a in self.addrs]
        self.names = [best[a][2] for a in self.addrs]
        self.by_name = {}
        for a, n in zip(self.addrs, self.names):
            self.by_name.setdefault(n, a)
        for line in out.splitlines():  # (aliases too: the self-test's names)
            p = line.split(' ')
            if len(p) >= 3 and p[0] and p[-2] in rank:
                self.by_name.setdefault(p[-1], int(p[0], 16))

    def function(self, e):
        i = bisect.bisect_right(self.addrs, e) - 1
        if i < 0:
            return None
        if self.sizes[i] and e >= self.addrs[i] + self.sizes[i] and i + 1 < len(self.addrs):
            return '?gap after ' + self.names[i]
        return self.names[i]


def demangle(names):
    names = list(names)
    out = subprocess.run([os.path.join(BIN, 'powerpc-eabi-c++filt')], input='\n'.join(names), capture_output=True, text=True,
                         check=True).stdout.split('\n')
    return dict(zip(names, out))


def cstr(b):
    return b.split(b'\0', 1)[0].decode('latin-1')


def read(path):
    """(version, period, anchors [(addr, name, os_name)], cores [(core, n, lost, idle, elsewhere, samples)], threads, os_names)"""
    data = open(path, 'rb').read()
    if data[:8] == b'RAC1SMP1':
        anchor, n, lost, _ = struct.unpack('>4I', data[8:24])
        v = struct.unpack('>%dI' % (2 * n), data[24:24 + 8 * n])
        samples = [(v[2 * k], v[2 * k + 1], 0, ()) for k in range(n)]
        return 1, 0, [(anchor, 'wiiu_sampler_anchor', '')], [(-1, n, lost, 0, 0, samples)], {}, {}
    if data[:8] != b'RAC1SMP2':
        sys.exit('not a samples.bin')
    period, depth, n_anchors, n_cores, n_threads, n_names, _, _ = struct.unpack('>8I', data[8:40])
    at = 40

    def named():
        nonlocal at
        a = struct.unpack('>I', data[at:at + 4])[0]
        s = cstr(data[at + 4:at + 64])
        at += 64
        return a, s

    anchors = []
    for _ in range(n_anchors):
        a, s = named()
        _, o = named()
        anchors.append((a, s, o))
    cores = []
    rec = 12 + 4 * depth
    for _ in range(n_cores):
        core, n, lost, idle, elsewhere = struct.unpack('>5I', data[at:at + 20])
        at += 32
        v = struct.unpack('>%dI' % (n * rec // 4), data[at:at + n * rec])
        at += n * rec
        w = 3 + depth
        samples = [(v[k * w], v[k * w + 1], v[k * w + 2], tuple(r for r in v[k * w + 3:k * w + w] if r)) for k in range(n)]
        cores.append((core, n, lost, idle, elsewhere, samples))
    threads = dict(named() for _ in range(n_threads))
    os_names = dict(named() for _ in range(n_names))
    return 2, period, anchors, cores, threads, os_names


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('elf')
    ap.add_argument('samples')
    ap.add_argument('rows', nargs='?', type=int, default=40)
    ap.add_argument('--rows', dest='rows_opt', type=int)
    ap.add_argument('--core', type=int)
    ap.add_argument('--thread')
    ap.add_argument('--paths', type=int, default=10, help='the call paths of the N hottest (self)')
    ap.add_argument('--force', action='store_true', help='print a profile even when the self-test fails')
    ap.add_argument('--delta', help='the code\'s relocation (hex), instead of the anchors\'')
    ap.add_argument('--under', help='only the samples whose call chain holds a function whose mangled or demangled name holds this'
                    ' (e.g. moby_pass): the zone\'s breakdown, in % of the samples kept before the filter')
    args = ap.parse_args()
    rows = args.rows_opt or args.rows

    version, period, anchors, cores, threads, os_names = read(args.samples)
    elf = Elf(args.elf)

    # the relocation, and the self-test
    deltas, fails = [], 0
    print('self-test (address as loaded -> the ELF\'s function there):')
    first = anchors[0][0] - elf.by_name.get(anchors[0][1], 0)
    for a, name, osn in anchors:
        e = elf.by_name.get(name)
        if e is None:
            print(f'  {a:08x} {name}: not in the ELF')
            fails += 1
            continue
        deltas.append(a - e)
        back = elf.function(a - first)
        ok = back == name or elf.by_name.get(back) == e
        fails += not ok
        print(f'  {a:08x} {name}: ELF {e:08x}, moved by {a - e:#x}; at {a - first:08x} with the first one\'s: {back}'
              f' {"ok" if ok else "WRONG"}' + (f' (coreinit: {osn})' if osn else ''))
    delta = int(args.delta, 16) if args.delta else (deltas[0] if deltas else 0)
    if len(set(deltas)) > 1:
        fails += 1
        print('  the functions moved by different amounts: the ELF is not this run\'s build')
    if delta & 0xfff:
        print(f'  a relocation of {delta:#x} (not page-aligned): the ELF is very probably not this run\'s build')
        fails += version == 1
    if version == 1:
        print('  (an old file: one anchor, the build cannot be checked; check the delta is the usual 0xa000000)')
    if fails and not args.force and not args.delta:
        sys.exit('the self-test failed: give the ELF of the run\'s own build (or --force to print a wrong profile anyway)')
    print(f'code moved by {delta:#x}' + (f', a sample every {period} us' if period else ''))

    cache = {}
    os_keys = sorted(os_names)

    def name(a):
        f = cache.get(a)
        if f is None:
            e = a - delta
            if elf.lo <= e < elf.hi:
                f = elf.function(e) or '?'
            elif a < OS_TOP:
                o = os_names.get(a)
                if o is None and os_keys:
                    i = bisect.bisect_right(os_keys, a) - 1
                    o = os_names[os_keys[i]] if i >= 0 and a - os_keys[i] < 0x1000 else None
                f = '[os] ' + (o if o else f'{a >> 16 << 16:08x}')
            elif a >= 0x80000000:
                f = '[kernel]'
            else:
                f = f'[? {a >> 16 << 16:08x}]'
            cache[a] = f
        return f

    def thread_name(t):
        return threads.get(t) or f'thread {t:08x}'

    # the samples kept
    kept = []
    for core, n, lost, idle, elsewhere, samples in cores:
        if core >= 0:
            print(f'core {core}: {n} samples, {lost} lost (the buffer full), {idle} idle ticks, {elsewhere} run on another core')
        else:
            print(f'{n} samples, {lost} lost')
        if args.core is not None and core != args.core:
            continue
        for s in samples:
            if args.thread and args.thread not in thread_name(s[2]):
                continue
            kept.append((core,) + s)
    total = len(kept)
    if total == 0:
        sys.exit('no sample kept')
    if args.under:
        def chain_names(k):
            return [name(a) for a in (k[1], k[2]) + k[4]]
        cand = set(n for k in kept for n in chain_names(k) if n and n[0] not in '[?')
        dmu = demangle(cand)
        hit = set(n for n in cand if args.under in n or args.under in dmu.get(n, ''))
        under = [k for k in kept if any(n in hit for n in chain_names(k))]
        print(f'--under {args.under}: {len(under)} of {total} samples ({100.0 * len(under) / total:.2f}%); the rows below in % of all {total}')
        kept = under
        if not kept:
            sys.exit('no sample under it')

    by_thread = collections.Counter((k[0], k[3]) for k in kept)
    if version == 2:
        print('\nthreads:')
        for (core, t), c in by_thread.most_common(12):
            print(f'  {100.0 * c / total:6.2f}%  core {core} {thread_name(t)}')
    chained = sum(1 for k in kept if k[4])
    print(f'\n{total} samples kept, {100.0 * chained / total:.0f}% with a back chain')

    self_c = collections.Counter()
    incl = collections.Counter()
    paths = collections.defaultdict(collections.Counter)
    for core, pc, lr, t, rets in kept:
        f = name(pc)
        self_c[f] += 1
        chain = [f]
        for a in (lr,) + rets:
            g = name(a)
            if g != chain[-1]:
                chain.append(g)
        for g in set(chain):
            incl[g] += 1
        paths[f][tuple(chain[1:5])] += 1

    shown = set(f for f, _ in self_c.most_common(rows)) | set(f for f, _ in incl.most_common(rows))
    for p in paths.values():
        for c in p:
            shown.update(c)
    dm = demangle(n for n in shown if n and n[0] not in '[?')

    def pretty(f, width):
        s = dm.get(f, f)
        return s if len(s) <= width else s[:width - 1] + '~'

    print('\nself (the pc):')
    for f, c in self_c.most_common(rows):
        print(f'{100.0 * c / total:6.2f}%  {pretty(f, 150)}')
    print('\ninclusive (the pc, lr and back chain: the function or what it calls):')
    for f, c in incl.most_common(rows):
        print(f'{100.0 * c / total:6.2f}%  {100.0 * self_c[f] / total:6.2f}% self  {pretty(f, 140)}')
    print(f'\nthe call paths of the {args.paths} hottest (the callers, innermost first):')
    for f, c in self_c.most_common(args.paths):
        print(f'  {pretty(f, 100)} ({100.0 * c / total:.2f}%)')
        for path, k in paths[f].most_common(3):
            print(f'    {100.0 * k / c:3.0f}%  ' + ' < '.join(pretty(g, 50) for g in path))


if __name__ == '__main__':
    main()
