#!/usr/bin/env python3
"""standin_dumps.py [--iso <PAL 2.00 iso>] [<out dir>]: the oracle's set `standin_200/` (batches 504, 510): the PAL disc 2.00's own
build of functions the 1.02 build changed, as the code generators see it (tools/overlay_match/region_view.py's StandIn:
the disc's code with its relocated fields taken back to the North American addresses, placed at the North American entry;
what a PAL 2.00 player's generated copy of the function was translated from). One file per function,
<out>/standin_200/l<level>_<entry>.bin, the code words little-endian, nothing else. The batches load them at the entry and
run them in the interpreter against a port's 1.02 path.

Needs the PAL 2.00 disc's image (default assets/iso/rac1-pal-2.0.iso) and data/regions/SCES-50916-2.00.txt. The default
<out> is $RAC1_ORACLE_DUMPS, else assets/dumps/oracle (docs/oracle.md). The files are game code: never commit them."""
import os
import struct
import sys

W = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(W, 'tools', 'overlay_match'))
sys.path.insert(0, os.path.join(W, 'tools', 'codegen'))

# (level, North American entry): the functions of the set
FUNCTIONS = [
    (15, 0x2ed068),  # Giant Clank's pad, class 1451 (Quartu)
    (18, 0x2fb868),  # Giant Clank's pad, class 1899 (Veldin's finale)
    (11, 0x30b358),  # the open sea, class 1111 (Pokitaru)
    (16, 0x2dee10),  # the open sea, class 1111 (Kalebo III)
]


def main(argv):
    iso = os.path.join(W, 'assets', 'iso', 'rac1-pal-2.0.iso')
    args = list(argv)
    if args[:1] == ['--iso']:
        iso = args[1]
        args = args[2:]
    if args and args[0].startswith('-'):
        print(__doc__)
        return 0 if args[0] in ('-h', '--help') else 2
    import region_view as rv
    out = args[0] if args else os.environ.get('RAC1_ORACLE_DUMPS', os.path.join(W, 'assets', 'dumps', 'oracle'))
    table = os.path.join(W, 'data', 'regions', 'SCES-50916-2.00.txt')
    d = os.path.join(out, 'standin_200')
    os.makedirs(d, exist_ok=True)
    bad = 0
    views = {}
    for level, entry in FUNCTIONS:
        if level not in views:
            views[level] = rv.level_view(iso, level, table)
        s = views[level].standin(entry)
        if s is None:
            print(f"L{level} {entry:08x}: not a changed function on the 2.00 table")
            bad += 1
            continue
        if s.unresolved:
            print(f"L{level} {entry:08x}: {s.unresolved} relocated fields not taken back: not written")
            bad += 1
            continue
        p = os.path.join(d, f"l{level}_{entry:06x}.bin")
        with open(p + '.tmp', 'wb') as f:
            f.write(struct.pack(f'<{len(s.code)}I', *s.code))
        os.replace(p + '.tmp', p)
        print(f"L{level} {entry:08x}: the 2.00 disc's {s.pal:08x}, {len(s.code)} words -> {p}")
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
