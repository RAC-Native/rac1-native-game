#!/usr/bin/env python3
"""rac1-native's update check (docs/settings.md, "Update check"): run by the game in the background at start,
never by the player. Python's standard library only.

One anonymous HTTPS request to the GitHub API, for this repository only, and nothing else: no identifier,
no telemetry, nothing about the machine or the player is sent (the request carries a generic User-Agent,
which GitHub requires, and nothing more).

  Stable channel:      GET /repos/<repo>/releases/latest: its tag compared with the installed version.
  Development channel: GET /repos/<repo>/compare/<installed commit>...main: the commits on main the
                       installed build does not have (only for a build from a git clone on main).

The result is written to --out as "key value" lines, which the game reads (src/host/update_check.cpp):

  checked <unix time>        when GitHub was asked
  channel stable|development
  installed <version>        (stable) the installed version / (development) the installed commit
  status ok
  newer 0|1                  something newer than the installed build
  id <tag or commit>         what is newer: the release's tag / main's commit (what "Skip Version" keeps)
  version <x.y.z>            (stable) the release's version
  commits <n>                (development) the commits on main not in the installed build
  notes <text>               (stable) the release notes in short: one line, plain ASCII

A result for the same channel and installed build younger than --max-age seconds is kept as it is,
without asking GitHub again. On any failure (no network, an error, GitHub's rate limit) nothing is
written and the exit code is 1: the game shows nothing. Exit code 0: --out holds a valid result.

Testing by hand:
  python3 tools/update_check.py --out /tmp/u.txt --installed 0.0.9 --force     (an older version: a release found)
  python3 tools/update_check.py --out /tmp/u.txt --api https://127.0.0.1:9 --force  (unreachable: exit 1, nothing written)
"""

import argparse
import json
import re
import sys
import time
import unicodedata
import urllib.request
from pathlib import Path

REPO = "RAC-Native/rac1-native-game"
API = "https://api.github.com"
TIMEOUT = 10  # seconds, the connection and each read
NOTES_MAX = 120  # characters of the release notes kept (the menu's help box scrolls slowly)


def version_tuple(v):
    """'v1.2.3' / '1.2' -> (1, 2, 3) / (1, 2, 0); None when it is not a version."""
    m = re.match(r"^v?(\d+)(?:\.(\d+))?(?:\.(\d+))?", v.strip())
    if not m:
        return None
    return tuple(int(x) if x else 0 for x in m.groups())


def plain_ascii(text):
    """Plain ASCII the game's font can draw: accents dropped, markdown's marks removed, no '%' (a button in the font)."""
    text = unicodedata.normalize("NFKD", text)
    text = text.replace("—", " - ").replace("–", "-").replace("’", "'").replace("‘", "'")
    text = text.replace("“", '"').replace("”", '"')
    text = text.encode("ascii", "ignore").decode("ascii")
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)  # links and images: their text
    text = re.sub(r"[*_`#>|~]", "", text)
    text = text.replace("%", " percent")
    text = re.sub(r"[^ -~]", " ", text)
    return re.sub(r"\s+", " ", text).strip()


def notes_short(name, body):
    """The release's title (without "rac1-native x.y.z:") and its first paragraph, in short."""
    title = plain_ascii(re.sub(r"^\s*rac1-native\s+v?[\d.]+\s*[:\-]?\s*", "", name or ""))
    para = ""
    for block in re.split(r"\n\s*\n", (body or "").replace("\r", "")):
        b = block.strip()
        if not b or b.startswith("#") or b.startswith("```"):
            continue
        para = plain_ascii(b)
        if para:
            break
    text = title[:1].upper() + title[1:] if title else ""
    if para:
        text = f"{text}. {para}" if text and not text.endswith(".") else (f"{text} {para}" if text else para)
    if len(text) > NOTES_MAX:
        text = text[:NOTES_MAX].rsplit(" ", 1)[0].rstrip(",;:") + "..."
    return text


def get_json(api, path):
    req = urllib.request.Request(api + path, headers={"Accept": "application/vnd.github+json",
                                                     "User-Agent": "rac1-native-update-check",
                                                     "X-GitHub-Api-Version": "2022-11-28"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        return json.load(r)


def read_result(path):
    out = {}
    try:
        for line in path.read_text(encoding="ascii", errors="replace").splitlines():
            k, _, v = line.partition(" ")
            out[k] = v
    except OSError:
        return {}
    return out


def main():
    ap = argparse.ArgumentParser(description="rac1-native's update check (run by the game).")
    ap.add_argument("--out", required=True, help="the result's file")
    ap.add_argument("--channel", choices=("stable", "development"), default="stable")
    ap.add_argument("--installed", default="", help="stable: the installed version (x.y.z)")
    ap.add_argument("--describe", default="", help="stable: git describe of the installed build (detects switching from main to stable)")
    ap.add_argument("--branch", default="", help="stable: git branch of the installed build")
    ap.add_argument("--commit", default="", help="development: the installed commit or v<version> tag")
    ap.add_argument("--max-age", type=int, default=6 * 3600, help="seconds a result is kept without asking again")
    ap.add_argument("--force", action="store_true", help="ask GitHub even when the result is recent")
    ap.add_argument("--api", default=API, help="testing: another API's address")
    args = ap.parse_args()
    out = Path(args.out)
    installed = args.commit if args.channel == "development" else args.installed
    if not installed or (args.channel == "stable" and version_tuple(installed) is None):
        return 1

    old = read_result(out)
    now = int(time.time())
    try:
        age = now - int(old.get("checked", "0"))
    except ValueError:
        age = -1
    if (not args.force and old.get("status") == "ok" and old.get("channel") == args.channel and
            old.get("installed") == installed and old.get("describe", "") == args.describe and 0 <= age < args.max_age):
        return 0  # (recent: GitHub not asked)

    lines = [f"checked {now}", f"channel {args.channel}", f"installed {installed}", f"describe {args.describe}", "status ok"]
    try:
        if args.channel == "stable":
            rel = get_json(args.api, f"/repos/{REPO}/releases/latest")
            tag = str(rel.get("tag_name", ""))
            latest = version_tuple(tag)
            if latest is None:
                return 1
            # switching from Development (main, ahead of the release tag) to Stable also offers the latest release
            on_dev_main = (args.branch == "main" and bool(args.describe) and
                           args.describe != tag and args.describe != f"{tag}-dirty")
            newer = latest > version_tuple(installed) or on_dev_main
            lines += [f"newer {int(newer)}", f"id {plain_ascii(tag)}", "version " + ".".join(map(str, latest)),
                      "notes " + notes_short(str(rel.get("name") or ""), str(rel.get("body") or ""))]
        else:
            if not (re.fullmatch(r"[0-9a-f]{7,40}", installed) or re.fullmatch(r"v\d+(?:\.\d+)*", installed)):
                return 1
            cmp = get_json(args.api, f"/repos/{REPO}/compare/{installed}...main")
            ahead = int(cmp.get("ahead_by", 0))  # (main's commits the installed build lacks)
            commits = cmp.get("commits") or []
            head = str(commits[-1].get("sha", "")) if commits else installed
            if len(commits) < ahead:  # (the compare lists main's first 250 new commits only: its tip asked for itself)
                head = str(get_json(args.api, f"/repos/{REPO}/commits/main").get("sha", "")) or head
            lines += [f"newer {int(ahead > 0)}", f"id {head[:40]}", f"commits {ahead}"]
    except Exception:  # (no network, an HTTP error, the rate limit, an unexpected answer: silent)
        return 1

    try:
        out.parent.mkdir(parents=True, exist_ok=True)
        tmp = out.with_suffix(out.suffix + ".tmp")
        tmp.write_text("\n".join(lines) + "\n", encoding="ascii")
        tmp.replace(out)
    except OSError:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
