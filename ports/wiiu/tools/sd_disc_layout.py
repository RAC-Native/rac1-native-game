#!/usr/bin/env python3
"""Lays the extracted disc's files out on the Wii U's SD card copy (rac1-native/assets/disc/) the way the Wii U build reads them:
toc/'s files spread in sub-folders by the name's first two hex digits (toc/row_1ac4.vag -> toc/1a/row_1ac4.vag; runtime/kernel/disc.cpp
files_host_path). A folder of 1,581 files made each open scan it on the card's FAT (140-190 ms). Works on a mounted card (moves files
within it, nothing copied twice); run again it does nothing.

    python3 ports/wiiu/tools/sd_disc_layout.py /run/media/$USER/<card>/rac1-native/assets/disc
"""
import os
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    toc = os.path.join(sys.argv[1], "toc")
    if not os.path.isdir(toc):
        sys.exit(f"no {toc}")
    moved = 0
    for name in sorted(os.listdir(toc)):
        src = os.path.join(toc, name)
        if not (os.path.isfile(src) and name.startswith("row_") and len(name) > 10):
            continue
        sub = os.path.join(toc, name[4:6])
        os.makedirs(sub, exist_ok=True)
        os.replace(src, os.path.join(sub, name))
        moved += 1
    print(f"{moved} files moved into {len([d for d in os.listdir(toc) if os.path.isdir(os.path.join(toc, d))])} sub-folders of {toc}")


if __name__ == "__main__":
    main()
