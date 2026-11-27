"""The folder the game's run-time data is in, for the tools: the game's own rule (src/io/asset_paths.h, docs/setup.md,
"Where the data goes").

  1. RAC1_ASSETS, when set (a relative path is taken against the current directory);
  2. else the first line of build/assets.txt that is not empty or a `#` comment (the pointer file next to the
     executable, `--setup --output-dir <dir> --remember` writes it; a relative path in it is taken against build/);
  3. else the repository's assets/.

The generated sources (assets/generated/src/) are a build input, not run-time data: they stay in the repository's
assets/ (REPO_ASSETS). A tool's explicit command-line paths win over this.
"""

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO_ASSETS = ROOT / "assets"
POINTER_FILE = ROOT / "build" / "assets.txt"


def remembered_root():
    """The folder build/assets.txt names, or None."""
    try:
        lines = POINTER_FILE.read_text(encoding="utf-8-sig").splitlines()
    except OSError:
        return None
    for line in lines:
        line = line.strip()
        if line and not line.startswith("#"):
            return (POINTER_FILE.parent / Path(line).expanduser()).resolve()
    return None


def env_root():
    """RAC1_ASSETS, made absolute, or None."""
    value = os.environ.get("RAC1_ASSETS", "").strip()
    return Path(value).expanduser().resolve() if value else None


def asset_root(default=REPO_ASSETS):
    """The root: RAC1_ASSETS, else build/assets.txt's, else `default` (the repository's assets/)."""
    return env_root() or remembered_root() or Path(default)
