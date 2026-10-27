#!/usr/bin/env python3
"""A copy of the Wii U build for the Cemu emulator (debugging only, never for the console): every conditional trap
instruction (tw / twi with a condition, e.g. libstdc++'s tweq on a shared_ptr's count overflow) made a nop, then the
.rpx made again with elf2rpl. Cemu 2.6 runs no conditional trap: its recompiler leaves to the interpreter at the
instruction, and the interpreter never steps past it, so a tw whose condition is false hangs the emulated thread. The
unconditional traps (twu, TO = 31: an abort) are kept.

    ports/wiiu/tools/cemu_rpx.py build-wiiu/rac1_wiiu.elf out.rpx
"""
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ELF2RPL = "/opt/devkitpro/tools/bin/elf2rpl"
NOP = 0x60000000


def text_section(data):
    shoff, = struct.unpack_from(">I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from(">HHH", data, 0x2E)
    names_off = struct.unpack_from(">I", data, shoff + shstrndx * shentsize + 16)[0]
    for i in range(shnum):
        name, _, _, _, off, size = struct.unpack_from(">IIIIII", data, shoff + i * shentsize)
        end = data.index(b"\0", names_off + name)
        if data[names_off + name:end] == b".text":
            return off, size
    raise SystemExit("no .text section")


def main():
    elf, out = Path(sys.argv[1]), Path(sys.argv[2])
    data = bytearray(elf.read_bytes())
    off, size = text_section(data)
    patched = 0
    for at in range(off, off + size - 3, 4):
        w, = struct.unpack_from(">I", data, at)
        op, to = w >> 26, (w >> 21) & 31
        is_tw = op == 31 and ((w >> 1) & 0x3FF) == 4
        is_twi = op == 3
        if (is_tw or is_twi) and to != 31:
            struct.pack_into(">I", data, at, NOP)
            patched += 1
    with tempfile.TemporaryDirectory() as tmp:
        patched_elf = Path(tmp) / elf.name
        patched_elf.write_bytes(data)
        subprocess.run([ELF2RPL, str(patched_elf), str(out)], check=True, stdout=subprocess.DEVNULL)
    print(f"{patched} conditional traps made nops; {out}")


if __name__ == "__main__":
    main()
