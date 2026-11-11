#!/usr/bin/env python3
"""Run deterministic headless RAC1_BEHAV_TRACE tours across boot, all 19 planets, and mods.

Usage:
  run_tours.py --out-dir /path/to/traces [--preset all|quick|mods] [--only tour1,tour2]
               [--compare-dir /path/to/baseline] [--float-eps 0.0]
               [--run-sh /home/daino/progetti/rac1-scratch/native/run.sh]
               [--mods-dir /home/daino/progetti/rac1-native-game-dev/mods]
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))


@dataclass(frozen=True)
class Tour:
    name: str
    frames: int
    env: Dict[str, str]
    needs_mods: bool = False


# Standard input script exercising running, jumping, wrench swings, camera stick, and pause menu.
PLAY_SEQ_SINGLE = (
    "seq:90-260:ly=-1;130+6:x;175+4:s;210-260:rx=0.5;"
    "280+4:st;310+4:r;340+4:st;370-520:ly=-1+lx=-0.35;420+6:x;470+4:s"
)

# Multi-planet tour input script (keeps moving, jumping, swinging, turning across transitions).
PLAY_SEQ_MULTI = (
    "seq:120-750:ly=-1;220+6:x;300+4:s;420-520:ly=-1+rx=0.4;"
    "850-1450:ly=-1;950+6:x;1050+4:s;1150-1300:ly=-1+lx=0.4;"
    "1550-2150:ly=-1;1650+6:x;1750+4:s;1850-2000:ly=-1+rx=-0.4;"
    "2250-2950:ly=-1;2350+6:x;2450+4:s;2550-2800:ly=-1+lx=-0.4"
)


def build_tours() -> List[Tour]:
    tours: List[Tour] = [
        # 1. Boot & title screen tour
        Tour(
            name="00_boot_title",
            frames=400,
            env={
                "RAC1_BOOT": "1",
                "RAC1_NO_MOVIES": "1",
                "RAC1_TITLE_WAIT": "260",
                "RAC1_SIM_INPUT": "title:",
            },
        ),
        # 2. Veldin opening + pause menu + wrench + movement (planet 0)
        Tour(
            name="01_veldin_play",
            frames=560,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_GOTO": "0@40",
                "RAC1_SIM_INPUT": PLAY_SEQ_SINGLE,
            },
        ),
        # 3..7. Five multi-planet tours covering all remaining 18 planets (1..18):
        #   - Group A: Novalis (1), Aridia (2), Kerwan (3), Eudora (4)
        Tour(
            name="02_planets_01_04",
            frames=3000,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_GOTO": "1@80,2@800,3@1520,4@2240",
                "RAC1_SIM_INPUT": PLAY_SEQ_MULTI,
            },
        ),
        #   - Group B: Rilgar (5), Blarg (6), Umbris (7), Batalia (8)
        Tour(
            name="03_planets_05_08",
            frames=3000,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_GOTO": "5@80,6@800,7@1520,8@2240",
                "RAC1_SIM_INPUT": PLAY_SEQ_MULTI,
            },
        ),
        #   - Group C: Gaspar (9), Orxon (10), Pokitaru (11), Hoven (12)
        Tour(
            name="04_planets_09_12",
            frames=3000,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_GOTO": "9@80,10@800,11@1520,12@2240",
                "RAC1_SIM_INPUT": PLAY_SEQ_MULTI,
            },
        ),
        #   - Group D: Gemlik (13), Oltanis (14), Quartu (15), Kalebo III (16)
        Tour(
            name="05_planets_13_16",
            frames=3000,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_GOTO": "13@80,14@800,15@1520,16@2240",
                "RAC1_SIM_INPUT": PLAY_SEQ_MULTI,
            },
        ),
        #   - Group E: Veldin Orbit (17) & Veldin Finale (18)
        Tour(
            name="06_planets_17_18",
            frames=1600,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_GOTO": "17@80,18@840",
                "RAC1_SIM_INPUT": PLAY_SEQ_MULTI,
            },
        ),
        # 8. Example Plateau SDK level mod tour
        Tour(
            name="07_mod_example_plateau",
            frames=1200,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_MODS": "1",
                "RAC1_MOD_CODE": "1",
                "RAC1_GOTO": "example-plateau@60",
                "RAC1_MOD_TEST": "sdk",
                "RAC1_SIM_INPUT": "seq:200-1150:ly=-1;320+6:x;450+4:s",
            },
            needs_mods=True,
        ),
        # 9. Night Arena rail & enemy wave SDK mod tour
        Tour(
            name="08_mod_night_arena",
            frames=1800,
            env={
                "RAC1_NO_MOVIES": "1",
                "RAC1_MODS": "1",
                "RAC1_MOD_CODE": "1",
                "RAC1_GOTO": "night-arena@60",
                "RAC1_MOD_TEST": "arena_rail",
            },
            needs_mods=True,
        ),
    ]
    return tours


def select_tours(all_tours: List[Tour], preset: str, only_csv: Optional[str]) -> List[Tour]:
    if only_csv:
        wanted = {x.strip() for x in only_csv.split(",") if x.strip()}
        return [t for t in all_tours if t.name in wanted]
    if preset == "quick":
        return [t for t in all_tours if t.name in {"00_boot_title", "01_veldin_play", "02_planets_01_04"}]
    if preset == "mods":
        return [t for t in all_tours if t.needs_mods]
    return all_tours


def run_one(tour: Tour, out_dir: str, run_sh: str, mods_dir: Optional[str], extra_env: Dict[str, str]) -> Tuple[bool, float, str]:
    trace_path = os.path.join(out_dir, f"{tour.name}.r1bt")
    log_path = os.path.join(out_dir, f"{tour.name}.log")
    if os.path.exists(trace_path):
        os.remove(trace_path)

    env_args = [
        "RAC1_HEADLESS=1",
        f"RAC1_FRAMES={tour.frames}",
        f"RAC1_BEHAV_TRACE={trace_path}",
    ]
    if tour.needs_mods and mods_dir:
        env_args.append(f"RAC1_MODS_DIR={mods_dir}")
    for k, v in tour.env.items():
        env_args.append(f"{k}={v}")
    for k, v in extra_env.items():
        env_args.append(f"{k}={v}")

    cmd = [run_sh, log_path] + env_args
    t0 = time.monotonic()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    dt = time.monotonic() - t0
    ok = proc.returncode == 0 and os.path.exists(trace_path) and os.path.getsize(trace_path) > 16
    return ok, dt, trace_path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out-dir", required=True, help="Directory to write .r1bt traces and .log files")
    ap.add_argument("--preset", choices=("all", "quick", "mods"), default="all")
    ap.add_argument("--only", default=None, help="Comma-separated tour names to run")
    ap.add_argument("--list", action="store_true", help="List available tours and exit")
    ap.add_argument("--compare-dir", default=None, help="Compare generated traces against this baseline directory")
    ap.add_argument("--float-eps", type=float, default=0.0, help="Float tolerance when --compare-dir is used")
    ap.add_argument("--run-sh", default="/home/daino/progetti/rac1-scratch/native/run.sh", help="Path to run.sh wrapper")
    ap.add_argument("--mods-dir", default="/home/daino/progetti/rac1-native-game-dev/mods", help="Path to mods directory")
    ap.add_argument("--ramhash", action="store_true", help="Also emit <tour>.ramhash via RAC1_RAMHASH")
    args = ap.parse_args()

    all_tours = build_tours()
    if args.list:
        for t in all_tours:
            print(f"{t.name:26s} frames={t.frames:<5d} mods={t.needs_mods}")
        return 0

    tours = select_tours(all_tours, args.preset, args.only)
    if not tours:
        print("error: no tours matched selection", file=sys.stderr)
        return 2

    os.makedirs(args.out_dir, exist_ok=True)
    verify_py = os.path.join(HERE, "verify_trace.py")

    failed = 0
    for t in tours:
        extra: Dict[str, str] = {}
        if args.ramhash:
            extra["RAC1_RAMHASH"] = os.path.join(args.out_dir, f"{t.name}.ramhash")
        ok, dt, trace_path = run_one(t, args.out_dir, args.run_sh, args.mods_dir, extra)
        if not ok:
            print(f"[FAIL] {t.name} ({dt:.1f}s) — see {os.path.join(args.out_dir, t.name + '.log')}")
            failed += 1
            continue
        sz_kb = os.path.getsize(trace_path) / 1024.0
        msg = f"[DONE] {t.name:26s} {dt:5.1f}s  {sz_kb:8.1f} KiB"
        if args.compare_dir:
            base_trace = os.path.join(args.compare_dir, f"{t.name}.r1bt")
            cmp_proc = subprocess.run(
                [sys.executable, verify_py, base_trace, trace_path, "--float-eps", str(args.float_eps)],
                capture_output=True,
                text=True,
            )
            cmp_out = cmp_proc.stdout.strip() or cmp_proc.stderr.strip()
            msg += f"  -> {cmp_out}"
            if cmp_proc.returncode != 0:
                failed += 1
        print(msg, flush=True)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
