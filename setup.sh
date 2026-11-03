#!/usr/bin/env bash
# rac1-native's guided setup: checks your system, builds the game, checks your disc image and
# generates the game's data. The work is done by tools/setup.py, which needs Python 3.8 or newer.
# Usage: ./setup.sh [--iso <path-to-your-iso>] [--force [steps]] [-j N]  (./setup.sh --help)
# Without --iso it picks the disc images in assets/iso/ by itself (docs/setup.md, "Which disc images").

case "${BASH_SOURCE[0]}" in
    */*) DIR="$(cd "${BASH_SOURCE[0]%/*}" && pwd)" ;;
    *) DIR="$PWD" ;;
esac

if ! command -v python3 >/dev/null 2>&1 ||
    ! python3 -c 'import sys; sys.exit(sys.version_info < (3, 8))' 2>/dev/null; then
    echo "The setup needs Python 3.8 or newer. Install it, then run ./setup.sh again:"
    ID="" ID_LIKE=""
    [ -f /etc/os-release ] && . /etc/os-release
    case " $ID $ID_LIKE " in
        *" debian "*|*" ubuntu "*) echo "    sudo apt install python3" ;;
        *" fedora "*|*" rhel "*) echo "    sudo dnf install python3" ;;
        *" arch "*) echo "    sudo pacman -S --needed python" ;;
        *) echo "    (with your distribution's package manager)" ;;
    esac
    exit 1
fi

exec python3 "$DIR/tools/setup.py" "$@"
