#!/usr/bin/env python3
"""Lays the extracted disc's files out on the Wii U's SD card copy (rac1-native/assets/disc/) the way the Wii U build reads them:
toc/'s files spread in sub-folders by the name's first two hex digits (toc/row_1ac4.vag -> toc/1a/row_1ac4.vag; runtime/kernel/disc.cpp
files_host_path). A folder of 1,581 files made each open scan it on the card's FAT (140-190 ms).

The sub-folders are made in a new folder that then takes toc/'s name: toc/ itself keeps the entries of every file once in it (a FAT
folder does not shrink when its files leave), and an open of toc/<xx>/... scans them all - measured on the console (2026-11-06) with
the files moved within toc/: an open ~250 ms, a file's first read ~130 ms more; with the fresh folder ~10 and ~20 ms. Works on a
mounted card (moves files and folders within it, nothing copied); run again it does nothing.

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
    names = sorted(os.listdir(toc))
    files = [n for n in names if os.path.isfile(os.path.join(toc, n)) and n.startswith("row_") and len(n) > 10]
    marker = os.path.join(toc, ".laid_out")
    if not files and os.path.exists(marker):
        print(f"{toc}: laid out already")
        return
    fresh = os.path.join(sys.argv[1], "toc.new")
    os.makedirs(fresh, exist_ok=True)
    for n in names:  # (sub-folders of an earlier layout: moved whole)
        if os.path.isdir(os.path.join(toc, n)):
            os.replace(os.path.join(toc, n), os.path.join(fresh, n))
    for n in files:
        sub = os.path.join(fresh, n[4:6])
        os.makedirs(sub, exist_ok=True)
        os.replace(os.path.join(toc, n), os.path.join(sub, n))
    left = [n for n in os.listdir(toc) if n != ".laid_out"]
    if left:
        sys.exit(f"{toc} still holds {len(left)} entries (e.g. {left[0]}): the new folder is {fresh}")
    if os.path.exists(marker):
        os.remove(marker)
    os.rmdir(toc)
    os.replace(fresh, toc)
    open(marker, "w").close()
    print(f"{len(files)} files moved; {len([d for d in os.listdir(toc) if os.path.isdir(os.path.join(toc, d))])} sub-folders in a fresh {toc}")


if __name__ == "__main__":
    main()
