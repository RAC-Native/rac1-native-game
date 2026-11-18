"""Runs fault/timeout checks in child processes; no game assets required."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    exe = str(args.executable.resolve())
    normal = subprocess.run([exe], capture_output=True, text=True, timeout=20)
    assert normal.returncode == 0, (normal.returncode, normal.stdout, normal.stderr)
    print(normal.stdout.strip())
    timed = subprocess.run([exe, "timeout"], capture_output=True, text=True, timeout=10)
    if os.name == "nt":
        assert timed.returncode == 124, (timed.returncode, timed.stdout, timed.stderr)
        assert "native trial timed out" in timed.stderr, timed.stderr
    else:
        assert timed.returncode == 0, (timed.returncode, timed.stdout, timed.stderr)
        assert "timeout rejected" in timed.stdout, timed.stdout
    print("timeout behavior passed")


if __name__ == "__main__":
    main()
