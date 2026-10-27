#!/usr/bin/env python3
"""The console's sampling profile (ports/wiiu/game/wiiu_sampler.cpp's samples.bin) mapped on the ELF's functions.

    ports/wiiu/tools/samples.py build-wiiu/rac1_wiiu.elf samples.bin [rows]

Prints the functions the samples' pc fall in (self time) and, for the hottest, the callers their lr point into.
"""
import bisect
import collections
import os
import struct
import subprocess
import sys


def symbols(elf):
    nm = os.path.join(os.environ.get('DEVKITPPC', '/opt/devkitpro/devkitPPC'), 'bin', 'powerpc-eabi-nm')
    out = subprocess.run([nm, '-n', '-C', '--defined-only', elf], capture_output=True, text=True, check=True).stdout
    addrs, names, anchor = [], [], None
    for line in out.splitlines():
        parts = line.split(' ', 2)
        if len(parts) < 3 or parts[1] not in 'tTwW':
            continue
        a = int(parts[0], 16)
        if parts[2] == 'wiiu_sampler_anchor':
            anchor = a
        addrs.append(a)
        names.append(parts[2])
    return addrs, names, anchor


def main():
    elf, path = sys.argv[1], sys.argv[2]
    rows = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    data = open(path, 'rb').read()
    if data[:8] != b'RAC1SMP1':
        sys.exit('not a samples.bin')
    anchor_run, n, lost, _ = struct.unpack('>4I', data[8:24])
    pairs = struct.unpack('>%dI' % (2 * n), data[24:24 + 8 * n])
    addrs, names, anchor = symbols(elf)
    delta = anchor_run - anchor

    def name(a):
        i = bisect.bisect_right(addrs, a - delta) - 1
        return names[i] if i >= 0 else '?'

    self_c = collections.Counter()
    callers = collections.defaultdict(collections.Counter)
    for k in range(n):
        f = name(pairs[2 * k])
        self_c[f] += 1
        callers[f][name(pairs[2 * k + 1])] += 1
    print(f'{n} samples ({lost} lost), code moved by {delta:#x}')
    for f, c in self_c.most_common(rows):
        print(f'{100.0 * c / n:6.2f}%  {f[:150]}')
    print('\ncallers (by lr) of the ten hottest:')
    for f, _ in self_c.most_common(10):
        top = ', '.join(f'{g[:60]} {100.0 * c / self_c[f]:.0f}%' for g, c in callers[f].most_common(4))
        print(f'  {f[:70]}: {top}')


if __name__ == '__main__':
    main()
