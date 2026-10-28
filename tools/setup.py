#!/usr/bin/env python3
"""rac1-native's guided setup (run it through ./setup.sh; on Windows, setup.cmd).

From a fresh clone and your own disc image (Ratchet & Clank: the North American disc SCUS-97199 or the European PAL disc
SCES-50916) to the game:
  1. checks the system and says what to install;
  2. checks your disc image;
  3. extracts the whole disc into assets/disc/ (tools/disc_extract.py): the game reads those files, not the image;
  4. generates the part of the game's code that is made from the game's own code on the disc
     (Python only, before the build): the boot executable and the overlay maps it needs, then
     tools/codegen/generate_all.py into assets/generated/src/;
  5. builds the game (build/rac1-native);
  6. generates the game's data into assets/ (`rac1-native --setup`, one step at a time), and makes the
     setup find the disc again (assets/iso/rac1-ntsc-u.iso, or assets/iso/rac1-pal.iso for the PAL disc);
  7. offers to start the game.

Re-runnable: the build only recompiles what changed, and each step is redone only when the sources
that make its output (or the disc) changed since it last ran, when its output is missing (deleted since: a quick
look at its files, step_missing), or when it is forced (--force).
The record of what was generated with what is assets/setup-state.json. docs/setup.md is the guide.

`tools/setup.py --code-inputs` (for CMake) prints the fingerprint of the generated code's sources, then
those sources, one per line. `tools/setup.py --check` (for play.cmd) changes nothing: it prints what a setup would
redo now, and exits with 10 when that is something (0 when the game is up to date). `tools/setup.py --check-system`
does step 1 only (exit status 0, 1, or 20: build in a container), `--container-info` (for the launcher) prints the
build container's name, image and packages, for immutable systems (SteamOS, Bazzite...; docs/setup.md).
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import urllib.request
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import disc_extract  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
WINDOWS = os.name == "nt"  # (a native Windows Python: MSYS2's CLANG64 / UCRT64 one, or python.org's)
MACOS = sys.platform == "darwin"  # (Apple silicon: the Metal renderer, docs/metal.md)
SETUP_CMD = "setup.cmd" if WINDOWS else "./setup.sh"  # (what the messages tell the player to run)
BUILD_DIR = ROOT / "build"
EXE = BUILD_DIR / ("rac1-native.exe" if WINDOWS else "rac1-native")
ASSETS_DIR = ROOT / "assets"
# where the setup finds the disc again (runtime/kernel/disc.cpp: the game too, with RAC1_READ_ISO=1): a link, one name
# per region
GAME_ISOS = {"NTSC-U": ASSETS_DIR / "iso/rac1-ntsc-u.iso", "PAL": ASSETS_DIR / "iso/rac1-pal.iso"}
# where links cannot be made (Windows without the privilege), the disc image's path instead (runtime/kernel/disc.cpp)
GAME_ISO_PATH_FILE = ASSETS_DIR / "iso/disc-path.txt"
STATE_FILE = ASSETS_DIR / "setup-state.json"
# the disc, extracted (tools/disc_extract.py): what the game reads while it runs (runtime/kernel/disc.cpp)
DISC_DIR = ASSETS_DIR / "disc"
SECTOR = 2048

# The generated code: tools/codegen/generate_all.py writes CODE_ROOT/src/... (the same tree as src/), and
# this tool adds GEN_LIST, the list of the generated files for CMake (src/game/CMakeLists.txt).
CODE_ROOT = ASSETS_DIR / "generated"
GEN_SRC = CODE_ROOT / "src"
GEN_LIST = GEN_SRC / "generated_sources.cmake"
GENERATE_ALL = ROOT / "tools/codegen/generate_all.py"
DUMPS = ASSETS_DIR / "dumps"  # the disc's boot executable: SCUS_971.99 (NTSC-U) or SCES_509.16 (PAL)
OVERLAY_MAPS = ASSETS_DIR / "generated/overlay_map"
# Another region's disc (PAL): the region table (addresses and their shifts against the NTSC-U build the ports are written
# against), data/regions/<serial>.txt; tools/overlay_match/region_maps.py and region_view.py read it.
REGIONS = ROOT / "data/regions"

# The generated code's sources: the generators and their inputs, and the headers (with variants_l<n>.cpp, the
# levels' variant tables) whose `// orig:` declarations the generators read. (The boot executable and the overlay
# maps they also read are steps of their own, below.)
CODE_SOURCES = ["tools/codegen/**/*", "tools/overlay_match/overlay_match.py", "tools/overlay_match/front_match.py",
                "tools/overlay_match/region_maps.py", "tools/overlay_match/region_view.py", "data/regions/*.txt",
                "src/game/**/*.h", "src/game/ovl/variants*.cpp"]
# What the generated VU programs come from, for the data steps that run them
VU_CODEGEN = ["tools/codegen/vu2cpp*.py", "tools/codegen/vu_micro.py"]

# The disc's files: every byte of the image, extracted once (redone when the disc or the extractor changes).
DISC_STEP = ("disc", "the disc's files", ["tools/disc_extract.py"])
# The steps before the build, done by this tool (Python only), with the sources that make each one's output.
CODE_STEPS = [
    ("elf", "the boot executable", []),
    ("overlay", "the overlay address maps", ["tools/overlay_match/*.py", "data/regions/*.txt"]),
    ("code", "the generated code", CODE_SOURCES),
]
# The data steps of `rac1-native --setup --step <name>`, in order, with the sources that make each
# one's output: a change to any of them (after a git pull) makes the step run again.
STEPS = [
    ("moby", "the moby models and textures",
     ["src/setup/mrp_export.*", "src/setup/kick_trace.*", "src/formats/**/*", "src/game/ovl/moby_vu1*",
      "tools/codegen/gen_moby_vu_fast.py", *VU_CODEGEN, "runtime/vu/**/*"]),
    ("world", "the world geometry and textures",
     ["src/setup/mrp_export.*", "src/setup/world_trace.*", "src/formats/**/*", "src/game/ovl/world_vu1*",
      "tools/codegen/gen_world_vu.py", *VU_CODEGEN, "src/game/render/mrp_world_assets.*", "runtime/vu/**/*"]),
    ("flight", "the flight between planets (the loading screen)",
     ["src/setup/scene_export.*", "src/setup/kick_trace.*", "src/formats/**/*", "src/game/ovl/moby_vu1*",
      "tools/codegen/gen_moby_vu_fast.py", *VU_CODEGEN, "src/game/render/mrp_world_assets.*", "runtime/vu/**/*"]),
]
STEP_NAMES = [DISC_STEP[0]] + [s[0] for s in CODE_STEPS] + [s[0] for s in STEPS]

def enable_windows_colors():
    """The console's escape sequences (Windows 10 and newer); False when they cannot be turned on."""
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
        mode = ctypes.c_uint32()
        return bool(kernel32.GetConsoleMode(handle, ctypes.byref(mode))) and \
            bool(kernel32.SetConsoleMode(handle, mode.value | 0x0004))  # ENABLE_VIRTUAL_TERMINAL_PROCESSING
    except (AttributeError, OSError):
        return False


USE_COLOR = sys.stdout.isatty() and (not WINDOWS or enable_windows_colors())
# Run by the launcher (tools/launcher/, RAC1_LAUNCHER=1): the progress also as lines of their own that it reads,
# "@step <n> <total> <message>" and "@progress <percent>" (it shows them as its progress bar, not as text)
LAUNCHER = os.environ.get("RAC1_LAUNCHER") == "1"


def disable_windows_quick_edit():
    """The console's QuickEdit mode off while the setup works, back on when it ends (atexit): with it, a click in the
    window starts a text selection and every program writing to the console (this script, the build, the game's
    --setup) waits until the selection ends - the setup looks frozen. At the end it is back, to copy a message."""
    try:
        import atexit
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-10)  # STD_INPUT_HANDLE
        mode = ctypes.c_uint32()
        if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            return
        # ENABLE_EXTENDED_FLAGS (0x80) set for ENABLE_QUICK_EDIT_MODE's bit (0x40) to be taken
        if kernel32.SetConsoleMode(handle, (mode.value | 0x0080) & ~0x0040):
            atexit.register(lambda: kernel32.SetConsoleMode(handle, mode.value | 0x0080))
    except (AttributeError, OSError):
        pass


if WINDOWS:
    disable_windows_quick_edit()


def color(code, text):
    return f"\033[{code}m{text}\033[0m" if USE_COLOR else text


def step(n, total, msg):
    if LAUNCHER:
        print(f"@step {n} {total} {msg}", flush=True)
    print("\n" + color("1;36", f"[{n}/{total}] {msg}"), flush=True)


def progress(done, total):
    """For the launcher: how far the current step is (a percent line, when it changed)."""
    if LAUNCHER and total:
        pct = min(100, done * 100 // total)
        if pct != getattr(progress, "last", None):
            progress.last = pct
            print(f"@progress {pct}", flush=True)


def ok(msg):
    print(color("32", f"  OK  {msg}"), flush=True)


def warn(msg):
    print(color("33", f"  !   {msg}"), flush=True)


def fail(msg):
    print(color("1;31", f"  ERROR  {msg}"), flush=True)
    sys.exit(1)


def ask(question, default):
    """A yes / no question; False when there is no one to answer it."""
    if not sys.stdin.isatty():
        return False
    try:
        reply = input(f"  {question} {'[Y/n]' if default else '[y/N]'} ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return False
    return default if not reply else reply.startswith("y")


# --- 1. The system ------------------------------------------------------------------------------

OS_RELEASE = Path("/etc/os-release")


def os_release():
    """/etc/os-release's keys (their values in lower case), {} when it cannot be read."""
    data = {}
    try:
        for line in OS_RELEASE.read_text().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                data[k.strip()] = v.strip().strip('"\'').lower()
    except OSError:
        pass
    return data


def detect_distro():
    data = os_release()
    if not data:
        return None
    ids = [data.get("ID", "")] + data.get("ID_LIKE", "").split()
    for family, names in (("debian", ("debian", "ubuntu")), ("fedora", ("fedora", "rhel", "centos")),
                          ("arch", ("arch",))):
        if any(i in names for i in ids):
            return family
    return None


def pkg_config(*modules, extra_path=None):
    if not shutil.which("pkg-config"):
        return False
    env = dict(os.environ)
    if extra_path and Path(extra_path).is_dir():
        env["PKG_CONFIG_PATH"] = extra_path + ":" + env.get("PKG_CONFIG_PATH", "")
    return subprocess.run(["pkg-config", "--exists", *modules], env=env, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL).returncode == 0


# (CMakeLists.txt also looks for FFmpeg in Homebrew's prefix)
HOMEBREW_PKGCONFIG = "/home/linuxbrew/.linuxbrew/lib/pkgconfig"


def has_header(*paths):
    return any(Path("/usr/include", p).exists() for p in paths)


def cmake_ok():
    if not shutil.which("cmake"):
        return False
    try:
        ver = subprocess.check_output(["cmake", "--version"], text=True).split()[2]
        return tuple(int(x) for x in ver.split(".")[:2]) >= (3, 20)
    except (subprocess.SubprocessError, ValueError, IndexError):
        return False


def compiler_ok():
    """A C++20 compiler: GCC 11+ or Clang 13+ (the one CMake would pick, c++, or either by name)."""
    for name, minimum in (("c++", 0), ("g++", 11), ("clang++", 13)):
        path = shutil.which(name)
        if not path:
            continue
        try:
            out = subprocess.check_output([path, "--version"], text=True)
            ver = int(subprocess.check_output([path, "-dumpversion"], text=True).strip().split(".")[0])
        except (subprocess.SubprocessError, ValueError):
            continue
        need = minimum or (13 if "clang" in out.lower() else 11)
        if ver >= need:
            return True
    return False


# (what, required, check, packages on Debian / Ubuntu, Fedora, Arch). The X11 and Wayland lists are
# what GLFW (built from source with both, cmake/dependencies.cmake) needs; xkbcommon is Wayland's keyboard.
DEPENDENCIES = [
    ("CMake 3.20 or newer", True, cmake_ok, "cmake", "cmake", "cmake"),
    ("a C++20 compiler (GCC 11+ or Clang 13+)", True, compiler_ok, "build-essential", "gcc-c++", "base-devel"),
    ("pkg-config", True, lambda: shutil.which("pkg-config") is not None, "pkg-config", "pkgconf-pkg-config", "pkgconf"),
    ("OpenGL development files", True, lambda: pkg_config("gl") or has_header("GL/gl.h"),
     "libgl-dev", "mesa-libGL-devel", "mesa"),
    ("X11 development files", True,
     lambda: pkg_config("x11", "xrandr", "xinerama", "xcursor", "xi", "xext"),
     "libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev",
     "libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXext-devel",
     "libx11 libxrandr libxinerama libxcursor libxi libxext"),
    ("Wayland development files", True,
     lambda: pkg_config("wayland-client", "wayland-cursor", "wayland-egl", "xkbcommon") and shutil.which("wayland-scanner"),
     "libwayland-dev libxkbcommon-dev", "wayland-devel libxkbcommon-devel", "wayland libxkbcommon"),
    ("FFmpeg's libavcodec (optional: plays the game's movies)", False,
     lambda: pkg_config("libavcodec", "libavutil") or pkg_config("libavcodec", "libavutil", extra_path=HOMEBREW_PKGCONFIG), "libavcodec-dev libavutil-dev", "ffmpeg-free-devel", "ffmpeg"),
    ("glslc (optional: the experimental Vulkan renderer's shaders)", False,
     lambda: shutil.which("glslc") is not None or shutil.which("glslangValidator") is not None, "glslc", "glslc", "shaderc"),
]
INSTALL = {"debian": "sudo apt install", "fedora": "sudo dnf install", "arch": "sudo pacman -S --needed"}
DISTRO_NAME = {"debian": "Debian / Ubuntu", "fedora": "Fedora", "arch": "Arch"}

# Immutable ("atomic") systems: SteamOS, Bazzite, Fedora Silverblue / Kinoite and the other Fedora Atomic and Universal
# Blue images (Bluefin, Aurora), openSUSE MicroOS / Aeon... Their system folders are read-only (or their packages come
# as one image), so the build's packages cannot be installed with the package manager: the game is built in a
# container instead, distrobox (or toolbox), which shares the home folder, the display, the GPU and the sound
# (docs/setup.md, "Steam Deck, Bazzite and other immutable systems"). GitHub issue #108.
IMMUTABLE_IDS = {"steamos": "SteamOS", "bazzite": "Bazzite", "bluefin": "Bluefin", "aurora": "Aurora",
                 "silverblue": "Fedora Silverblue", "kinoite": "Fedora Kinoite", "sericea": "Fedora Sway Atomic",
                 "onyx": "Fedora Budgie Atomic", "cosmic-atomic": "Fedora COSMIC Atomic", "endless": "Endless OS",
                 "chimeraos": "ChimeraOS", "opensuse-microos": "openSUSE MicroOS", "opensuse-aeon": "openSUSE Aeon",
                 "opensuse-kalpa": "openSUSE Kalpa", "vanilla": "Vanilla OS"}
OSTREE_BOOTED = Path("/run/ostree-booted")
CONTAINER_MARKS = [Path("/run/.containerenv"), Path("/.dockerenv")]  # (podman / toolbox / distrobox; docker)
# The container: its name, and an image that has every package the game needs (Fedora's own Mesa and FFmpeg, whatever
# the host's are). The packages: the Fedora column of DEPENDENCIES, Python, and what the game needs to run in the
# container (Mesa's OpenGL and Vulkan drivers, the sound's client libraries).
CONTAINER_NAME = "rac1-native"
CONTAINER_IMAGE = "registry.fedoraproject.org/fedora-toolbox:44"
CONTAINER_RUNTIME_PACKAGES = "python3 mesa-dri-drivers mesa-vulkan-drivers pulseaudio-libs alsa-lib"
# setup.py's exit status when the build's packages are missing on an immutable system: build in a container (the
# launcher offers to set one up)
EXIT_USE_CONTAINER = 20


def in_container():
    """True inside a container (distrobox, toolbox, podman, docker): its packages are installed the usual way."""
    return any(p.exists() for p in CONTAINER_MARKS) or bool(os.environ.get("CONTAINER_ID"))


def usr_read_only():
    try:
        return bool(os.statvfs("/usr").f_flag & os.ST_RDONLY)
    except (OSError, AttributeError):
        return False


def immutable_system():
    """The immutable system's name when this is one (and not a container on it), else None."""
    if WINDOWS or in_container():
        return None
    data = os_release()
    names = [data.get("ID", ""), data.get("VARIANT_ID", "")] + data.get("ID_LIKE", "").split()
    for n in names:  # (Bazzite's VARIANT_ID is "bazzite-deck", "bazzite-gnome-nvidia"...)
        for key, name in IMMUTABLE_IDS.items():
            if n == key or n.startswith(key + "-"):
                return name
    if OSTREE_BOOTED.exists():  # (Fedora Atomic and everything made from it, Endless OS)
        return data.get("NAME", "").title() or "an ostree system"
    if usr_read_only():
        return data.get("NAME", "").title() or "a system with a read-only /usr"
    return None


def container_packages():
    """Everything the container needs, in Fedora's names: the build's packages (required and optional) and the game's."""
    pkgs = []
    for dep in DEPENDENCIES:
        pkgs += dep[4].split()
    pkgs += CONTAINER_RUNTIME_PACKAGES.split()
    return list(dict.fromkeys(pkgs))


def container_tool():
    """distrobox (preferred: any image, and it shares the external drives too) or toolbox, whichever is installed."""
    for tool in ("distrobox", "toolbox"):
        if shutil.which(tool):
            return tool
    return None


def container_commands(tool, iso=None):
    """The commands that make the container, install the packages in it, run this setup in it and start the game."""
    import shlex
    pkgs = " ".join(container_packages())
    root = shlex.quote(str(ROOT))
    iso_arg = f" --iso {shlex.quote(str(iso))}" if iso else " --iso /path/to/your/ratchet.iso"
    if tool == "toolbox":
        run = f"toolbox run -c {CONTAINER_NAME}"
        create = f"toolbox create -y -i {CONTAINER_IMAGE} {CONTAINER_NAME}"
    else:
        run = f"distrobox enter {CONTAINER_NAME} --"
        create = f"distrobox create -n {CONTAINER_NAME} -i {CONTAINER_IMAGE} --yes"
    return [create, f"{run} sudo dnf install -y {pkgs}", f"cd {root}", f"{run} ./setup.sh{iso_arg}"], \
        f"cd {root} && {run} ./build/rac1-native"


def explain_container(system, iso=None):
    tool = container_tool()
    print(f"\n  This is {system}, an immutable system: its system folders are read-only, so the packages the build"
          "\n  needs cannot be installed with its package manager. Build the game in a container instead: it shares"
          "\n  your home folder, your display, your graphics card and your sound, and leaves the system as it is.")
    if tool is None:
        print("\n  Neither distrobox nor toolbox is installed. SteamOS (3.5 and newer) and Bazzite come with distrobox;"
              "\n  elsewhere install it (https://distrobox.it), then run the setup again. The commands, with distrobox:")
    else:
        print(f"\n  With {tool}, in a terminal:")
    setup_cmds, play = container_commands(tool or "distrobox", iso)
    for c in setup_cmds:
        print("    " + color("1", c))
    print(f"\n  Then play with:\n    {color('1', play)}"
          "\n  (The launcher does all of this by itself: docs/launcher.md.)")


# Windows: the MSYS2 CLANG64 environment's packages (docs/setup.md); clang is the recommended compiler there
# (the same float code as on Linux, docs/platforms.md). The OpenGL, window and audio libraries are Windows' own.
WINDOWS_DEPENDENCIES = [
    ("CMake 3.20 or newer", True, cmake_ok, "mingw-w64-clang-x86_64-cmake"),
    ("a C++20 compiler (clang 13+, or GCC 11+ for MinGW-w64)", True, compiler_ok, "mingw-w64-clang-x86_64-clang"),
    ("Ninja (the build tool)", True, lambda: shutil.which("ninja") is not None, "mingw-w64-clang-x86_64-ninja"),
    ("pkg-config (finds FFmpeg)", False, lambda: shutil.which("pkg-config") is not None, "mingw-w64-clang-x86_64-pkgconf"),
    ("FFmpeg's libavcodec (optional: plays the game's movies)", False, lambda: pkg_config("libavcodec", "libavutil"),
     "mingw-w64-clang-x86_64-ffmpeg"),
    ("glslc (optional: the experimental Vulkan renderer's shaders)", False,
     lambda: shutil.which("glslc") is not None or shutil.which("glslangValidator") is not None, "mingw-w64-clang-x86_64-shaderc"),
]


def macos_avcodec_ok() -> bool:
    """FFmpeg on macOS: by pkg-config, or Homebrew's headers and libraries as CMake finds them without it."""
    if shutil.which("pkg-config") and pkg_config("libavcodec", "libavutil"):
        return True
    return any(os.path.exists(os.path.join(p, "include", "libavcodec", "avcodec.h")) and
               os.path.exists(os.path.join(p, "lib", "libavcodec.dylib")) for p in ("/opt/homebrew", "/usr/local"))


# macOS: Apple's clang (the Command Line Tools or Xcode), the rest from Homebrew (https://brew.sh). No OpenGL, X11 or
# Wayland files (GLFW is built for Cocoa) and no glslc (the Metal shaders are in the repository, compiled at start).
# (what, required, check, Homebrew package)
MACOS_DEPENDENCIES = [
    ("CMake 3.20 or newer", True, cmake_ok, "cmake"),
    ("Apple's C++ compiler (the Command Line Tools: xcode-select --install)", True, compiler_ok, None),
    ("FFmpeg's libavcodec (optional: plays the game's movies)", False, macos_avcodec_ok, "ffmpeg"),
]


def check_system_macos():
    missing_req, missing_opt = [], []
    for dep in MACOS_DEPENDENCIES:
        if not dep[2]():
            (missing_req if dep[1] else missing_opt).append(dep)
    for dep in missing_opt:
        warn(f"Not found: {dep[0]}. The game runs without it.")
    if not missing_req:
        ok("Everything needed to build the game is installed.")
    else:
        for dep in missing_req:
            print(color("1;31", f"  Missing: {dep[0]}"))
    brew = [dep[3] for dep in missing_req + missing_opt if dep[3]]
    if brew:
        cmd = "brew install " + " ".join(brew)
        if shutil.which("brew") is None:
            print("\n  Install Homebrew (https://brew.sh), then:\n    " + color("1", cmd))
        else:
            print("\n  With Homebrew:\n    " + color("1", cmd))
            if missing_req and ask("Run this command now?", False):
                if subprocess.run(cmd, shell=True).returncode == 0 and all(d[2]() for d in missing_req if d[3]):
                    missing_req = [d for d in missing_req if not d[3]]
    if missing_req:
        print(f"\n  Then run {SETUP_CMD} again.")
        sys.exit(1)


def check_system_windows():
    missing_req, missing_opt = [], []
    for dep in WINDOWS_DEPENDENCIES:
        if not dep[2]():
            (missing_req if dep[1] else missing_opt).append(dep)
    for dep in missing_opt:
        warn(f"Not found: {dep[0]}. The game runs without it.")
    if not missing_req:
        ok("Everything needed to build the game is installed.")
    else:
        for dep in missing_req:
            print(color("1;31", f"  Missing: {dep[0]}"))
    wanted = missing_req + missing_opt
    if wanted:
        cmd = "pacman -S --needed " + " ".join(dep[3] for dep in wanted)
        print("\n  With MSYS2 (https://www.msys2.org), in its CLANG64 shell:\n    " + color("1", cmd) +
              "\n  then run the setup from that shell (or with MSYS2's clang64\\bin folder in your PATH).")
    if missing_req:
        print(f"\n  Then run {SETUP_CMD} again.")
        sys.exit(1)


def check_system(iso=None):
    """Exits (1) when something required is missing, or (EXIT_USE_CONTAINER) when it is missing on an immutable system,
    where it is installed in a container instead."""
    if WINDOWS:
        check_system_windows()
        return
    if MACOS:
        check_system_macos()
        return
    distro = detect_distro()
    immutable = immutable_system()
    column = {"debian": 3, "fedora": 4, "arch": 5}.get(distro)
    missing_req, missing_opt = [], []
    for dep in DEPENDENCIES:
        if not dep[2]():
            (missing_req if dep[1] else missing_opt).append(dep)
    for dep in missing_opt:
        warn(f"Not found: {dep[0]}. The game runs without it.")
    if not missing_req:
        ok("Everything needed to build the game is installed.")
    else:
        for dep in missing_req:
            print(color("1;31", f"  Missing: {dep[0]}"))
    wanted = missing_req + missing_opt
    if not wanted:
        return
    if immutable is not None:
        if not missing_req:  # (only optional ones: the game builds without them, as it is)
            print(f"\n  ({immutable} is an immutable system: to have them, build the game in a container, docs/setup.md.)")
            return
        explain_container(immutable, iso)
        if LAUNCHER:
            print(f"@container {CONTAINER_NAME} {CONTAINER_IMAGE}", flush=True)
        sys.exit(EXIT_USE_CONTAINER)
    if column is None:
        print(f"\n  Install them with your distribution's package manager, then run {SETUP_CMD} again.")
    else:
        cmd = f"{INSTALL[distro]} " + " ".join(dep[column] for dep in wanted)
        print(f"\n  To install {'them' if missing_req else 'it'} on {DISTRO_NAME[distro]}:\n    {color('1', cmd)}")
        if missing_req and ask("Run this command now (it asks for your password)?", False):
            if subprocess.run(cmd, shell=True).returncode == 0:
                missing_req = [d for d in missing_req if not d[2]()]
                if not missing_req:
                    ok("Installed.")
                    return
            fail(f"Something is still missing. Install it and run {SETUP_CMD} again.")
    if missing_req:
        print(f"\n  Then run {SETUP_CMD} again.")
        sys.exit(1)


# --- 2. The disc image --------------------------------------------------------------------------

def looks_like_ps2_disc(path):
    """A quick look before the build (the full check is `rac1-native --setup --check-iso`):
    an ISO 9660 image with a PS2 boot executable ("XXXX_NNN.NN") in its root. Returns its name, or None."""
    try:
        with open(path, "rb") as f:
            f.seek(16 * SECTOR)
            pvd = f.read(SECTOR)
            if len(pvd) < SECTOR or pvd[1:6] != b"CD001":
                return None
            lba, size = struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0]
            f.seek(lba * SECTOR)
            data = f.read(min(size, 64 * SECTOR))
    except OSError:
        return None
    off = 0
    while off < len(data):
        ln = data[off]
        if ln == 0:
            off = (off // SECTOR + 1) * SECTOR
            continue
        name = data[off + 33:off + 33 + data[off + 32]].split(b";")[0].decode(errors="replace")
        if len(name) == 11 and name[4] == "_" and name[8] == ".":
            return name
        off += ln
    return None


# Ratchet & Clank's releases, by their boot executable: (serial, region, supported, a good dump's size). The same table as
# KNOWN_DISCS in src/setup/iso_extract.cpp (the full check, after the build); this copy only lets the
# setup stop before the build when the disc is the wrong one. Add a release to both.
KNOWN_DISCS = {
    "SCUS_971.99": ("SCUS-97199", "NTSC-U", True, 4214095872),
    "SCES_509.16": ("SCES-50916", "PAL", True, 4213997568),
    "SCPS_150.55": ("SCPS-15055", "NTSC-J", False, 0),
}


def known_iso():
    """The disc image the game already uses (RAC1_ISO, the link in assets/iso/, or its path there), or None."""
    if os.environ.get("RAC1_ISO"):
        return Path(os.environ["RAC1_ISO"]).expanduser().resolve()
    for p in GAME_ISOS.values():
        if p.exists():
            return p
    if GAME_ISO_PATH_FILE.is_file():  # (where the setup could not link the image: its path)
        lines = GAME_ISO_PATH_FILE.read_text(encoding="utf-8").splitlines()
        if lines and Path(lines[0].strip()).is_file():
            return Path(lines[0].strip())
    return None


def find_iso(arg):
    if arg:
        return Path(arg).expanduser().resolve()
    iso = known_iso()
    if iso is not None:
        return iso
    if not sys.stdin.isatty():
        fail(f"No disc image given. Run {SETUP_CMD} --iso <path-to-your-iso>")
    print("  Where is your Ratchet & Clank disc image (NTSC-U SCUS-97199, or PAL SCES-50916)?")
    while True:
        try:
            val = input("  Path to the .iso file: ").strip().strip("'\"")
        except (EOFError, KeyboardInterrupt):
            print()
            sys.exit(1)
        if val:
            p = Path(val).expanduser().resolve()
            if p.is_file():
                return p
            print(f"  File not found: {p}")


def disc_fingerprint(iso):
    st = iso.stat()
    return f"{looks_like_ps2_disc(iso)}:{st.st_size}"


def disc_boot(iso):
    """The disc's boot executable's name ("SCUS_971.99"): read from the image, or, when the image is gone (the game
    does not need it once its files are extracted), from the record of the disc that was extracted."""
    if iso is not None and Path(iso).is_file():
        return looks_like_ps2_disc(iso)
    return load_state().get("disc_image", "").split(":")[0] or None


def iso_root_file(path, name):
    """(start, size) in bytes of a file in the disc image's root directory, or None."""
    with open(path, "rb") as f:
        f.seek(16 * SECTOR)
        pvd = f.read(SECTOR)
        lba, size = struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0]
        f.seek(lba * SECTOR)
        data = f.read(size)
    off = 0
    while off < len(data):
        ln = data[off]
        if ln == 0:  # (records do not cross sector boundaries)
            off = (off // SECTOR + 1) * SECTOR
            continue
        rec = data[off:off + ln]
        if rec[33:33 + rec[32]].split(b";")[0].decode(errors="replace") == name:
            return struct.unpack_from("<I", rec, 2)[0] * SECTOR, struct.unpack_from("<I", rec, 10)[0]
        off += ln
    return None


# --- 2b. The disc's files -----------------------------------------------------------------------
# The game reads its disc while it runs: the files tools/disc_extract.py cuts the image into, in assets/disc/ (every byte
# of the image, in the files its tables name). Extracted once, again when the disc or the extractor changes.

def disc_step_fingerprint(disc):
    return f"{disc}:{sources_fingerprint(DISC_STEP[2])}"


def extract_disc(iso, forced):
    state = load_state()
    disc = disc_fingerprint(iso)
    fingerprint = disc_step_fingerprint(disc)
    current = "disc" not in forced and state.get("disc") == fingerprint
    missing = disc_extract.layout_missing(DISC_DIR) if current else None
    if current and missing is None:
        ok("The disc's files: up to date")
        return
    if missing:
        warn(f"{missing}: extracting the disc again")
    for key in ("disc", "disc_image", "disc_check"):
        state.pop(key, None)
    save_state(state)
    print(f"  Extracting your disc into {DISC_DIR.relative_to(ROOT)}{os.sep} (about 4 GB; the old files there first removed)...",
          flush=True)
    t = time.time()
    shown = [-1]

    def show(done, total):
        progress(done, total)
        pct = done * 100 // total
        if sys.stdout.isatty() and not LAUNCHER and pct != shown[0]:
            shown[0] = pct
            print(f"\r  {pct:3d}% (written, then read back and checked)", end="", flush=True)

    try:
        plan = disc_extract.extract(iso, DISC_DIR, show)
    except (disc_extract.DiscError, OSError) as e:
        if sys.stdout.isatty() and not LAUNCHER:
            print()
        fail(f"Extracting the disc failed: {e}")
    if sys.stdout.isatty() and not LAUNCHER:
        print("\r" + " " * 48 + "\r", end="")
    ok(f"Extracted {len(plan.files)} files ({plan.total / 2**30:.2f} GB) into {DISC_DIR.relative_to(ROOT)}{os.sep} "
       f"in {time.time() - t:.0f} s, each checked against the image")
    state["disc"] = fingerprint
    state["disc_image"] = disc
    save_state(state)


# --- 3. The generated code ----------------------------------------------------------------------
# Part of the game's code is generated from the game's own MIPS and VU code on the disc, by tools/codegen/
# (docs/architecture.md, "The generated code"). It needs the boot executable and the overlay address maps, and
# comes before the build: the build compiles it from assets/generated/src/.

def sources_files(patterns):
    return sorted({p for pat in patterns for p in ROOT.glob(pat) if p.is_file() and "__pycache__" not in p.parts})


def code_sources_fingerprint():
    return sources_fingerprint(CODE_SOURCES)


def boot_elf(iso):
    """Where the disc's boot executable goes: assets/dumps/SCUS_971.99, or SCES_509.16 for the PAL disc."""
    return DUMPS / disc_boot(iso)


def region_table(iso):
    """The region table of another region's disc (data/regions/<serial>.txt), None for the NTSC-U disc."""
    serial, region = KNOWN_DISCS[disc_boot(iso)][:2]
    if region == "NTSC-U":
        return None
    table = REGIONS / f"{serial}.txt"
    if not table.is_file():
        fail(f"{table.relative_to(ROOT)} is missing: the {region} disc needs it (it is part of the repository).")
    return table


def extract_elf(iso):
    """The boot executable (SCUS_971.99, or the PAL disc's SCES_509.16), copied from the disc's root directory: the
    generators read the VU programs and the boot module from it, and the game reads it while it runs."""
    elf = boot_elf(iso)
    found = iso_root_file(iso, elf.name)
    if found is None:
        fail(f"{elf.name} is not on the disc image: it is damaged, or another disc.")
    start, size = found
    if start + size > iso.stat().st_size:
        fail("The disc image is damaged or incomplete (it ends before the boot executable does). Dump the disc again.")
    with open(iso, "rb") as f:
        f.seek(start)
        data = f.read(size)
    elf.parent.mkdir(parents=True, exist_ok=True)
    tmp = elf.with_name(elf.name + ".part")
    tmp.write_bytes(data)
    tmp.replace(elf)


def run_quiet(cmd, what):
    r = subprocess.run([str(c) for c in cmd], cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        print(r.stderr[-3000:])
        fail(f"{what} failed (see above). Run {SETUP_CMD} again to retry.")


def overlay_maps(iso):
    """Each level's code matched against Veldin's (level 0's), and the boot module's (tools/overlay_match/). On another
    region's disc, through the region table: the game's maps (region_maps.py apply: NTSC-U Veldin -> the disc's levels, and
    the table's runs) and the generators' (region_view.py maps: NTSC-U Veldin -> NTSC-U levels, on the code rebuilt at
    NTSC-U's addresses, in overlay_map/codegen/)."""
    OVERLAY_MAPS.mkdir(parents=True, exist_ok=True)
    tools = ROOT / "tools/overlay_match"
    table = region_table(iso)
    if table is not None:
        print("   the game's", end="", flush=True)
        run_quiet([sys.executable, tools / "region_maps.py", "apply", "--iso", iso, "--elf", boot_elf(iso), "--table", table,
                   "-o", OVERLAY_MAPS], "Making the overlay maps from the region table")
        print(", the code generators'", flush=True)
        run_quiet([sys.executable, tools / "region_view.py", "maps", "--iso", iso, "--table", table, "-o",
                   OVERLAY_MAPS / "codegen"], "Making the code generators' overlay maps")
        return
    print("   title", end="", flush=True)
    run_quiet([sys.executable, tools / "front_match.py", "--elf", boot_elf(iso), "--iso", iso, "--out", OVERLAY_MAPS],
              "Matching the boot module's code")
    for lv in range(1, 19):
        print(f" {lv}", end="", flush=True)
        run_quiet([sys.executable, tools / "overlay_match.py", "--iso", iso, "--out", OVERLAY_MAPS, "--levels", str(lv)],
                  f"Matching level {lv}'s code")
    print()


CODE_LOG = BUILD_DIR / "setup-codegen.log"


def generated_files():
    out = subprocess.run([sys.executable, GENERATE_ALL, "--files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return [l.strip() for l in out.splitlines() if l.strip()]


def generate_code(iso, jobs):
    """tools/codegen/generate_all.py into assets/generated/src/ (without the comments that carry each original
    instruction: the same C++), then the list of its files for CMake."""
    GEN_LIST.unlink(missing_ok=True)  # (until it is complete, the build asks for ./setup.sh)
    files = generated_files()
    units = subprocess.run([sys.executable, GENERATE_ALL, "--list"], cwd=ROOT, capture_output=True, text=True,
                           check=True).stdout.count("\n")
    BUILD_DIR.mkdir(exist_ok=True)
    env = dict(os.environ, RAC1_ASSETS=str(ASSETS_DIR), RAC1_ISO=str(iso))
    cmd = [sys.executable, str(GENERATE_ALL), "--out", str(CODE_ROOT), "--no-insn-comments", "--jobs", str(jobs)]
    done = 0
    with open(CODE_LOG, "w", encoding="utf-8", errors="replace") as log:
        log.write("$ " + " ".join(cmd) + "\n")
        proc = subprocess.Popen(cmd, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                errors="replace")
        for line in proc.stdout:
            log.write(line)
            if re.match(r"\S+:\S+\s+(ok|FAIL)\s", line):
                done = min(done + 1, units)  # (a unit run again prints a second line)
                progress(done, units)
                if USE_COLOR:
                    print(f"\r   {done}/{units}", end="", flush=True)
        rc = proc.wait()
    if USE_COLOR and done:
        print()
    if rc:
        print("".join(open(CODE_LOG, errors="replace").readlines()[-30:]))
        fail(f"Generating the code failed (see above; the full output is in {CODE_LOG.relative_to(ROOT)}).")
    missing = [f for f in files if not (CODE_ROOT / f).is_file()]
    if missing:
        fail(f"The code generation left out {len(missing)} file(s), e.g. {missing[0]} (see {CODE_LOG.relative_to(ROOT)}).")
    # files of an earlier generation that the generators no longer make
    keep = {(CODE_ROOT / f).resolve() for f in files}
    for p in GEN_SRC.rglob("*"):
        if p.is_file() and p.suffix in (".cpp", ".h") and p.resolve() not in keep:
            p.unlink()
    rel = [f[len("src/"):] for f in files]
    GEN_LIST.write_text("# Written by ./setup.sh (tools/setup.py) with the generated code: do not edit.\n"
                        f"set(RAC1_CODEGEN_FINGERPRINT \"{code_sources_fingerprint()}\")\n"
                        "set(RAC1_GENERATED_SOURCES\n" + "".join(f"    {f}\n" for f in rel) + ")\n")
    ok(f"Generated {len(files)} source files into {GEN_SRC.relative_to(ROOT)}/")


def code_current():
    """True when the generated code is there and was made from the current sources (what CMake checks), every file
    its list names there too (CMake would stop at the first one missing)."""
    try:
        text = GEN_LIST.read_text()
    except OSError:
        return False
    m = re.search(r'RAC1_CODEGEN_FINGERPRINT "(\w+)"', text)
    return bool(m) and m.group(1) == code_sources_fingerprint() and code_files_missing(text) is None


def code_files_missing(text):
    """The first file generated_sources.cmake lists that is not there, or None."""
    m = re.search(r"set\(RAC1_GENERATED_SOURCES\n(.*?)\)", text, re.S)
    if not m:
        return GEN_LIST
    for line in m.group(1).split():
        if not (GEN_SRC / line).is_file():
            return GEN_SRC / line
    return None


def code_step_fingerprint(name, patterns, disc, state):
    fingerprint = f"{disc}:{sources_fingerprint(patterns)}"
    if name == "code":  # (made from the two steps before it, too)
        fingerprint += ":" + hashlib.sha256((state.get("elf", "") + state.get("overlay", "")).encode()).hexdigest()
    return fingerprint


# What each step leaves, looked at before its record is trusted: a step whose output was deleted since (all of
# assets/generated, or part of it) runs again. Existence (and a non-empty file) only, so that a re-run stays quick.
# The data steps: the file each of the game's exporters writes last in a directory, and skips the directory on when it
# is there (src/setup/mrp_export.cpp, scene_export.cpp), and a class file at least in each level's mobys.
MRP = ASSETS_DIR / "generated/mrp"
MRP_LEVELS = [f"level{lv}" for lv in range(19)] + ["title"]


def first_missing(paths):
    for p in paths:
        try:
            if p.stat().st_size > 0:
                continue
        except OSError:
            pass
        return p
    return None


def step_missing(name, iso):
    """Why a step's output is not all there ("<path> is missing"), or None when it is."""
    if name == "elf":
        files = [boot_elf(iso)]
    elif name == "overlay":
        files = [OVERLAY_MAPS / "front_reloc.txt"] + [OVERLAY_MAPS / f"level{lv}.txt" for lv in range(1, 19)]
        if region_table(iso) is not None:
            files += [OVERLAY_MAPS / f"codegen/level{lv}.txt" for lv in range(1, 19)]
    elif name == "code":
        try:
            text = GEN_LIST.read_text()
        except OSError:
            return f"{GEN_LIST.relative_to(ROOT)} is missing"
        gone = code_files_missing(text)
        if gone is not None:
            return f"{gone.relative_to(ROOT)} is missing"
        return None if code_current() else f"{GEN_LIST.relative_to(ROOT)} is out of date"
    elif name == "moby":
        files = ([MRP / f"moby/{d}/textures.mrpt" for d in MRP_LEVELS] + [MRP / f"moby/armor/{k}.mrpt" for k in range(4)])
        if first_missing(files) is None:
            for d in MRP_LEVELS[:-1]:
                if not any(MRP.joinpath("moby", d).glob("*.mrpm")):
                    return f"{(MRP / 'moby' / d).relative_to(ROOT)} has no models"
    elif name == "world":
        files = [MRP / f"world/{d}/{f}" for d in MRP_LEVELS
                 for f in ("world.mrpw", "textures_tfrag.mrpt", "textures_tie.mrpt", "textures_shrub.mrpt")]
    elif name == "flight":
        files = [MRP / "moby/flight/textures.mrpt", MRP / "world/flight/world.mrpw"]
    else:
        return None
    gone = first_missing(files)
    if gone is None:
        return None
    # the highest directory that is gone, the clearest to read ("assets/generated/mrp/world is missing")
    top = gone
    while not top.parent.exists() and top.parent != ASSETS_DIR:
        top = top.parent
    return f"{top.relative_to(ROOT)} is missing"


def prepare_code(iso, forced, jobs):
    """The steps before the build, each redone only when the disc or its sources changed (or forced)."""
    state = load_state()
    disc = disc_fingerprint(iso)
    for name, what, patterns in CODE_STEPS:
        fingerprint = code_step_fingerprint(name, patterns, disc, state)
        missing = step_missing(name, iso) if state.get(name) == fingerprint else None
        if name not in forced and state.get(name) == fingerprint and missing is None:
            ok(f"{what[0].upper() + what[1:]}: up to date")
            continue
        if missing and name not in forced:
            warn(f"{missing}: generating {what} again")
        print(f"  {what[0].upper() + what[1:]}...", flush=True)
        state.pop(name, None)
        if name == "elf":
            extract_elf(iso)
            ok(f"Copied {boot_elf(iso).relative_to(ROOT)}")
        elif name == "overlay":
            overlay_maps(iso)
            ok(f"Wrote {OVERLAY_MAPS.relative_to(ROOT)}/")
        else:
            generate_code(iso, jobs)
        state[name] = fingerprint
        save_state(state)


def check_disc_files(iso):
    """Once after each extraction: the game itself reads every sector of the disc through its files and through the
    image, and compares (rac1-native --setup --compare-disc)."""
    state = load_state()
    if state.get("disc_check") == state.get("disc"):
        return
    print("  Checking the disc's files against your disc image (every sector, read the way the game reads them)...",
          flush=True)
    if subprocess.run([str(EXE), "--setup", "--iso", str(iso), "--compare-disc"], cwd=ROOT, env=game_env()).returncode:
        fail(f"The disc's files do not read the same as your disc image. Run {SETUP_CMD} --force disc to extract them again.")
    state["disc_check"] = state.get("disc")
    save_state(state)


# --- 4. The build -------------------------------------------------------------------------------

def available_memory_kb_windows():
    import ctypes

    class MemoryStatus(ctypes.Structure):
        _fields_ = [("dwLength", ctypes.c_uint32), ("dwMemoryLoad", ctypes.c_uint32), ("ullTotalPhys", ctypes.c_uint64),
                    ("ullAvailPhys", ctypes.c_uint64), ("ullTotalPageFile", ctypes.c_uint64),
                    ("ullAvailPageFile", ctypes.c_uint64), ("ullTotalVirtual", ctypes.c_uint64),
                    ("ullAvailVirtual", ctypes.c_uint64), ("ullAvailExtendedVirtual", ctypes.c_uint64)]
    st = MemoryStatus()
    st.dwLength = ctypes.sizeof(MemoryStatus)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(st)):
        raise OSError("GlobalMemoryStatusEx")
    return st.ullAvailPhys // 1024


def default_jobs():
    """One compile job per core, but no more than one per 2 GB of available memory."""
    jobs = os.cpu_count() or 2
    if WINDOWS:
        try:
            jobs = min(jobs, max(1, available_memory_kb_windows() // (2 * 1024 * 1024)))
        except (OSError, AttributeError):
            pass
        return max(1, jobs)
    try:
        for line in Path("/proc/meminfo").read_text().splitlines():
            if line.startswith("MemAvailable:"):
                jobs = min(jobs, max(1, int(line.split()[1]) // (2 * 1024 * 1024)))
    except (OSError, ValueError):
        pass
    return max(1, jobs)


BUILD_LOG = BUILD_DIR / "setup-build.log"


def run_logged(cmd, log):
    """Runs a build command with its output in `log`, showing only its progress; on failure, the errors."""
    with open(log, "a", encoding="utf-8", errors="replace") as f:
        f.write("$ " + " ".join(cmd) + "\n")
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        shown = ""
        for line in proc.stdout:
            f.write(line)
            m = re.match(r"\[\s*(\d+)%\]", line)
            if not m:  # (Ninja's progress: [done/total]; a build directory configured with Ninja on any system)
                n = re.match(r"\[(\d+)/(\d+)\]", line)
                if n:
                    m = re.match(r"(\d+)", str(int(n.group(1)) * 100 // max(1, int(n.group(2)))))
            if m and m.group(1) != shown:
                shown = m.group(1)
                progress(int(shown), 100)
                if USE_COLOR:
                    print(f"\r  {shown:>3}%", end="", flush=True)
        if USE_COLOR and shown:
            print()
        if proc.wait() == 0:
            return True
    errors = [l.rstrip() for l in open(log, errors="replace") if " error" in l or "Error" in l or "CMake Error" in l]
    print("\n".join("    " + l for l in errors[-25:]))
    print(f"  The full output is in {log.relative_to(ROOT)}.")
    return False


def windows_compilers():
    """CMake's C and C++ compilers on Windows, by their full paths: clang and clang++ (or gcc and g++), both checked.
    (clang++ alone was checked: with clang.exe missing, removed by an antivirus or left out by an unpacking stopped
    halfway, CMake found the C++ compiler but not the C one; GitHub issue #88)"""
    for c, cxx in (("clang", "clang++"), ("gcc", "g++")):
        c_path, cxx_path = shutil.which(c), shutil.which(cxx)
        if c_path and cxx_path:
            # (with /: a \ in a CMake cache entry can be read as an escape)
            return [f"-DCMAKE_C_COMPILER={Path(c_path).as_posix()}", f"-DCMAKE_CXX_COMPILER={Path(cxx_path).as_posix()}"]
        if cxx_path:
            fail(f"{cxx} is installed but {c} (the C compiler, {c}.exe beside {cxx_path}) is missing.\n"
                 "         An antivirus (Windows Security too) may have removed it, or the tools' unpacking stopped halfway:\n"
                 "         look in the antivirus' protection history and allow the file, then run "
                 f"{SETUP_CMD} again (it installs\n"
                 "         the compiler again). With your own MSYS2, in its CLANG64 shell: "
                 "pacman -S mingw-w64-clang-x86_64-clang")
    fail("No C and C++ compiler found (clang and clang++, or gcc and g++). In MSYS2's CLANG64 shell: "
         "pacman -S mingw-w64-clang-x86_64-clang")


# What CMake made in build/ (the configuration, the objects, the libraries' builds), removed by fresh_build_config(); the
# rest stays: the downloaded libraries' sources (_deps/*-src, *-subbuild), the game itself, the logs, an update's backups
BUILD_PRODUCTS = ("CMakeCache.txt", "CMakeFiles", "Makefile", "build.ninja", ".ninja_deps", ".ninja_log",
                  "cmake_install.cmake", "src", "third_party", "generated_vk")


def cached_compiler():
    """The C++ compiler the build folder was configured with, as CMake recorded it: (path, id, version), or None."""
    try:
        cache = (BUILD_DIR / "CMakeCache.txt").read_text(errors="replace")
    except OSError:
        return None
    m = re.search(r"^CMAKE_CXX_COMPILER:\w+=(.*)$", cache, re.M)
    if not m:
        return None
    ident = version = ""
    for f in sorted((BUILD_DIR / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")):
        text = f.read_text(errors="replace")
        i = re.search(r'^set\(CMAKE_CXX_COMPILER_ID "([^"]*)"\)', text, re.M)
        v = re.search(r'^set\(CMAKE_CXX_COMPILER_VERSION "([^"]*)"\)', text, re.M)
        if i and v:
            ident, version = i.group(1), v.group(1)
    return m.group(1).strip(), ident, version


def compiler_change():
    """Why the build folder's configuration no longer fits this system's compiler (a system update replaced it: GCC 15
    by GCC 16, say), or None. CMake detects the compiler only when it configures a new folder, and keeps using what it
    found then (GitHub issue #114)."""
    found = cached_compiler()
    if found is None:
        return None
    path, ident, version = found
    if not path or not version:
        return None
    if not Path(path).is_file() and shutil.which(path) is None:
        return f"the compiler it was configured with, {path}, is not there any more"
    flag = "-dumpfullversion" if ident == "GNU" else "-dumpversion"
    try:
        now = subprocess.run([path, flag], capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return None
    if not re.fullmatch(r"\d+(\.\d+)*", now) or now == version:
        return None
    # (clang's -dumpversion may give fewer parts than CMake recorded: compare only what both have)
    a, b = now.split("."), version.split(".")
    n = min(len(a), len(b))
    if a[:n] == b[:n]:
        return None
    return f"the compiler changed ({ident} {version} then, {now} now)"


def fresh_build_config():
    """build/ without what CMake made (BUILD_PRODUCTS, _deps/*-build): the next build configures and compiles afresh,
    as in a new folder, without downloading the libraries again. Returns the game's own on / off options the cache had
    (RAC1_NATIVE_ARCH and the other RAC1_* ones), as -D arguments for the new configuration; the paths CMake found
    (glslc...) are looked for again."""
    kept = []
    try:
        cache = (BUILD_DIR / "CMakeCache.txt").read_text(errors="replace")
        kept = [f"-D{m.group(1)}:{m.group(2)}={m.group(3)}"
                for m in re.finditer(r"^(RAC1_\w+):(BOOL)=(.*)$", cache, re.M)]
    except OSError:
        pass
    for name in BUILD_PRODUCTS:
        p = BUILD_DIR / name
        if p.is_dir():
            shutil.rmtree(p, ignore_errors=True)
        elif p.exists():
            p.unlink(missing_ok=True)
    for p in BUILD_DIR.glob("*.a"):
        p.unlink(missing_ok=True)
    for p in (BUILD_DIR / "_deps").glob("*-build"):
        shutil.rmtree(p, ignore_errors=True)
    return kept


def build(jobs, native_arch=None, retry=True, kept=()):
    BUILD_DIR.mkdir(exist_ok=True)
    BUILD_LOG.write_text("")
    change = compiler_change()
    if change:
        print(f"  The build folder was configured for another compiler: {change}. Configuring it afresh...", flush=True)
        kept = fresh_build_config()
    configured = (BUILD_DIR / "CMakeCache.txt").exists()
    # --native-arch on / off: the game compiled for this machine's CPU or not (CMakeLists.txt RAC1_NATIVE_ARCH), kept in the
    # build's CMake cache for the next builds (a change rebuilds everything)
    arch = [] if native_arch is None else [f"-DRAC1_NATIVE_ARCH={'ON' if native_arch == 'on' else 'OFF'}"]
    if not (BUILD_DIR / "CMakeCache.txt").exists() or not any((BUILD_DIR / f).exists() for f in ("Makefile", "build.ninja")):
        print("  Configuring (this downloads a few libraries the first time)...", flush=True)
        cmd = ["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR), "-DCMAKE_BUILD_TYPE=RelWithDebInfo"] + list(kept) + arch
        if WINDOWS:  # (Ninja and clang, not Visual Studio's generator and compiler: docs/platforms.md)
            cmd += ["-G", "Ninja"] + windows_compilers()
        if not run_logged(cmd, BUILD_LOG):
            fail(f"CMake could not configure the build (see above). If a library is missing, install it and run {SETUP_CMD} again.")
    elif arch:
        target = "this machine's CPU" if native_arch == "on" else "any x86-64 CPU"
        print(f"  Compiling for {target} (--native-arch {native_arch})...", flush=True)
        if not run_logged(["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR)] + arch, BUILD_LOG):
            fail(f"CMake could not configure the build (see above).")
    print(f"  Compiling with {jobs} parallel jobs (the first build takes several minutes)...", flush=True)
    if not run_logged(["cmake", "--build", str(BUILD_DIR), "-j", str(jobs)], BUILD_LOG):
        if retry and configured:
            # (a build folder kept from an earlier version or system can fail where a new one builds: files the sources
            # no longer have, a cache of another compiler. Once more from a fresh configuration before giving up; #114)
            shutil.copyfile(BUILD_LOG, BUILD_DIR / "setup-build-first.log")
            warn("The build failed in the build folder as it was. Trying once more with a fresh build configuration "
                 "(the first attempt's output is in build/setup-build-first.log)...")
            return build(jobs, native_arch, retry=False, kept=fresh_build_config())
        fail("The build failed (see above).")
    if not EXE.exists():
        # (the build ran to the end: the program was made, then removed, which is what an antivirus does with a new
        # program it does not know; GitHub issue #69)
        fail(f"{EXE} is missing after the build, although the build finished.\n"
             "         An antivirus (Windows Security too) may have removed it: look in its protection history, allow\n"
             f"         the file (or exclude this folder: {ROOT}), then run {SETUP_CMD} again.")
    ok(f"Built {EXE.relative_to(ROOT)}")


# --- 5. The game's data -------------------------------------------------------------------------

def sources_fingerprint(patterns):
    h = hashlib.sha256()
    # (the path always with /: the same fingerprint on every system, and from every Windows shell, MSYS2's Python
    # writing paths with / when MSYSTEM is set and with \ when it is not)
    for p in sources_files(patterns):
        h.update(p.relative_to(ROOT).as_posix().encode() + b"\0" + p.read_bytes() + b"\0")
    return h.hexdigest()


def load_state():
    try:
        return json.loads(STATE_FILE.read_text())
    except (OSError, ValueError):
        return {}


def save_state(state):
    STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    STATE_FILE.write_text(json.dumps(state, indent=2) + "\n")


def generate(iso, forced):
    state = load_state()
    old_wads = ASSETS_DIR / "extracted/level_wads"
    if old_wads.is_dir() and not old_wads.is_symlink():
        shutil.rmtree(old_wads, ignore_errors=True)
        try:
            old_wads.parent.rmdir()
        except OSError:
            pass
    if "wads" in state:
        state.pop("wads", None)
        save_state(state)
    disc = disc_fingerprint(iso)
    for name, what, patterns in STEPS:
        fingerprint = f"{disc}:{sources_fingerprint(patterns)}"
        current = name not in forced and state.get(name) == fingerprint
        missing = step_missing(name, iso) if current else None
        if current and missing is None:
            ok(f"{what[0].upper() + what[1:]}: up to date")
            continue
        cmd = [str(EXE), "--setup", "--iso", str(iso), "--step", name]
        if current:
            # up to date but part of it deleted: the step makes again only what is not there (its exporters skip a level
            # that is), and from scratch if that was not enough
            warn(f"{missing}: generating {what} again")
            if subprocess.run(cmd, cwd=ROOT, env=game_env()).returncode:
                fail(f"Generating {what} failed (see the messages above). Run {SETUP_CMD} again to retry.")
            if step_missing(name, iso) is None:
                continue
        # a step that is forced, whose sources changed, or that never finished, is redone from scratch
        if subprocess.run(cmd + ["--force"], cwd=ROOT, env=game_env()).returncode:
            fail(f"Generating {what} failed (see the messages above). Run {SETUP_CMD} again to retry.")
        gone = step_missing(name, iso)
        if gone is not None:  # (reported, not fatal: the game draws without what is missing, and says so)
            warn(f"Generating {what} left out files ({gone}).")
        state[name] = fingerprint
        save_state(state)


# What the build compiles (CMake's inputs): a file newer than the program means a new version (a git pull, a
# download over the old one) that was not built yet.
BUILD_INPUTS = ["CMakeLists.txt", "cmake", "src", "runtime", "third_party", "assets/generated/src"]


def newest_mtime(paths):
    newest = 0.0
    stack = [ROOT / p for p in paths]
    while stack:
        p = stack.pop()
        try:
            if p.is_dir():
                stack.extend(p.iterdir())
            else:
                newest = max(newest, p.stat().st_mtime)
        except OSError:
            pass
    return newest


def current_disc():
    """(the image, its fingerprint): the image the setup knows; once the disc is extracted the image may be gone
    (the game does not read it), and the fingerprint is then the extracted disc's. (None, None): no disc set up."""
    iso = known_iso()
    if iso is not None and iso.is_file():
        return iso, disc_fingerprint(iso)
    disc = load_state().get("disc_image")
    if disc and disc_extract.layout_missing(DISC_DIR) is None:
        return None, disc
    return None, None


def disc_steps_due(iso, disc):
    """What is made from the disc and is due again (everything but the build), for that disc."""
    due = []
    state = load_state()
    if state.get("disc") != disc_step_fingerprint(disc) or disc_extract.layout_missing(DISC_DIR):
        due.append(DISC_STEP[1])
    for name, what, patterns in CODE_STEPS:
        if state.get(name) != code_step_fingerprint(name, patterns, disc, state) or step_missing(name, iso):
            due.append(what)
    for name, what, patterns in STEPS:
        if state.get(name) != f"{disc}:{sources_fingerprint(patterns)}" or step_missing(name, iso):
            due.append(what)
    return due


def setup_due():
    """What a setup would redo now, without doing it ([] when the game is up to date): for play.cmd and
    ./setup.sh --check. Each step is judged as the setup itself judges it."""
    if not EXE.exists():
        return ["the game is not built yet"]
    iso, disc = current_disc()
    if disc is None:
        return ["the disc image is not set up"]
    due = disc_steps_due(iso, disc)
    if newest_mtime(BUILD_INPUTS) > EXE.stat().st_mtime:
        due.append("the game's program (its sources are newer)")
    return due


def game_env():
    """The environment the game's setup mode runs in: on Windows, the Python it runs the overlay matcher with
    (src/platform/platform_win32.cpp) is this one. (RAC1_LAUNCHER is the setup's, not the game's: the game would
    report it as an unknown testing switch.)"""
    env = {k: v for k, v in os.environ.items() if k != "RAC1_LAUNCHER"}
    if WINDOWS:
        env["RAC1_PYTHON"] = sys.executable
    return env


# --- 6. Updating (--update, GitHub issue #13) ---------------------------------------------------

REPO = "RAC-Native/rac1-native-game"
GITHUB_API = f"https://api.github.com/repos/{REPO}"
GITHUB_ZIP = f"https://github.com/{REPO}/archive/refs"
UPDATE_VERSION_FILE = ASSETS_DIR / "update-version.txt"


def config_dir():
    """The player's settings folder (platform.h config_dir)."""
    if WINDOWS:
        base = os.environ.get("APPDATA") or (os.path.join(os.environ["USERPROFILE"], "AppData", "Roaming")
                                             if os.environ.get("USERPROFILE") else "")
        return Path(base) / "rac1-native" if base else Path(".")
    base = os.environ.get("XDG_CONFIG_HOME") or (os.path.join(os.environ["HOME"], ".config")
                                                 if os.environ.get("HOME") else "")
    return Path(base) / "rac1-native" if base else Path(".")


def configured_channel(override):
    """The update channel: --channel when given, else Main menu > Updates > Update Channel (stable by default)."""
    if override:
        return override
    adv = Path(os.environ.get("RAC1_ADVANCED_FILE") or (config_dir() / "advanced_options.txt"))
    try:
        for line in adv.read_text(encoding="utf-8", errors="replace").splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0] == "update_channel":
                return "development" if parts[1] == "1" else "stable"
    except OSError:
        pass
    return "stable"


def github_get_json(path):
    req = urllib.request.Request(GITHUB_API + path, headers={"Accept": "application/vnd.github+json",
                                                             "User-Agent": "rac1-native-setup-update",
                                                             "X-GitHub-Api-Version": "2022-11-28"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.load(r)


def latest_release_tag():
    """The latest release's tag from GitHub (or from fetched git tags when offline / rate-limited)."""
    try:
        tag = str(github_get_json("/releases/latest").get("tag_name") or "").strip()
        if tag:
            return tag
    except Exception:
        pass
    if git_problem() is None:
        subprocess.run(["git", "fetch", "--tags", "origin"], cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        out = subprocess.run(["git", "tag", "-l", "v*"], cwd=ROOT, capture_output=True, text=True).stdout.splitlines()
        tags = []
        for t in out:
            m = re.match(r"^v(\d+)\.(\d+)\.(\d+)$", t.strip())
            if m:
                tags.append(((int(m.group(1)), int(m.group(2)), int(m.group(3))), t.strip()))
        if tags:
            return max(tags)[1]
    fail("Could not reach GitHub to find the latest release. Your current build was not changed.")


def wait_for_exe():
    """On Windows, waits briefly for the game to finish exiting before touching rac1-native.exe."""
    if not WINDOWS or not EXE.exists():
        return
    for _ in range(50):
        try:
            with open(EXE, "ab"):
                return
        except OSError:
            time.sleep(0.1)


def git_problem():
    """Why this folder cannot be updated with git (None: it can): no .git, no 'git' program (GitHub issue #47: a
    clone on Windows without Git installed), or a git that cannot read the clone (for example "dubious ownership")."""
    if not (ROOT / ".git").exists():
        return "this folder is not a git clone"
    if not shutil.which("git"):
        return "'git' is not installed"
    try:
        r = subprocess.run(["git", "rev-parse", "--verify", "-q", "HEAD"], cwd=ROOT, capture_output=True, text=True)
    except OSError as e:
        return f"'git' could not be run ({e})"
    if r.returncode != 0 or not r.stdout.strip():
        why = (r.stderr.strip().splitlines() or ["no commit checked out"])[0]
        return f"git cannot read this clone ({why})"
    return None


def update_git(channel):
    """Updates a git clone (`git fetch`, then checks out the release tag or fast-forwards main).
    Returns a restore callback on success, or None if nothing was changed."""
    dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT,
                           capture_output=True, text=True).stdout.strip()
    if dirty:
        fail("Your source files have local changes (git status). They are never overwritten: commit or stash them first.")
    branch = subprocess.run(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=ROOT,
                            capture_output=True, text=True).stdout.strip()
    old_head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    if branch not in ("main", "HEAD"):
        warn(f"This clone is on branch '{branch}' (not main): leaving the branch as it is.")
        return None
    restore_ref = branch if branch == "main" else old_head

    def restore():
        subprocess.run(["git", "checkout", "-q", restore_ref], cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if branch == "main" and old_head:
            subprocess.run(["git", "reset", "--hard", "-q", old_head], cwd=ROOT,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    if channel == "stable":
        tag = latest_release_tag()
        print(f"  Fetching release {tag}...", flush=True)
        if subprocess.run(["git", "fetch", "--tags", "origin"], cwd=ROOT).returncode != 0:
            fail("git fetch failed (check your internet connection). Your current build was not changed.")
        target = subprocess.run(["git", "rev-parse", f"{tag}^{{commit}}"], cwd=ROOT,
                                capture_output=True, text=True).stdout.strip()
        if not target:
            fail(f"Release tag {tag} was not found in the repository. Your current build was not changed.")
        if target == old_head and branch == "HEAD":
            ok(f"Already at release {tag}.")
            return restore
        if subprocess.run(["git", "checkout", "-q", tag], cwd=ROOT).returncode != 0:
            restore()
            fail(f"Could not check out {tag}. Your current build was not changed.")
        ok(f"Checked out release {tag}.")
        return restore

    print("  Fetching main from origin...", flush=True)
    if subprocess.run(["git", "fetch", "origin", "main"], cwd=ROOT).returncode != 0:
        fail("git fetch origin main failed (check your internet connection). Your current build was not changed.")
    if subprocess.run(["git", "merge-base", "--is-ancestor", old_head, "origin/main"], cwd=ROOT).returncode != 0:
        fail("This clone has local commits not on origin/main. They are never overwritten.")
    new_head = subprocess.run(["git", "rev-parse", "origin/main"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    if branch != "main":
        # (checkout -B resets the local main: never when it has commits origin/main lacks - a detached HEAD, a release
        # tag checked out by the stable channel, says nothing about them)
        has_main = subprocess.run(["git", "rev-parse", "-q", "--verify", "refs/heads/main"], cwd=ROOT,
                                  capture_output=True).returncode == 0
        if has_main and subprocess.run(["git", "merge-base", "--is-ancestor", "refs/heads/main", "origin/main"],
                                       cwd=ROOT).returncode != 0:
            fail("The local branch main has commits not on origin/main. They are never overwritten.")
        if subprocess.run(["git", "checkout", "-B", "main", "origin/main"], cwd=ROOT).returncode != 0:
            restore()
            fail("Could not switch to main. Your current build was not changed.")
    elif new_head != old_head:
        if subprocess.run(["git", "merge", "--ff-only", "origin/main"], cwd=ROOT).returncode != 0:
            restore()
            fail("Could not fast-forward main. Your current build was not changed.")
    if new_head == old_head and branch == "main":
        ok(f"Already up to date on main ({new_head[:7]}).")
    else:
        ok(f"Updated to main ({new_head[:7]}).")
    return restore


ZIP_PROTECTED = {".git", ".toolchain", "assets", "build"}


def zip_source_files(root):
    out = {}
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(root)
        if rel.parts[0] in ZIP_PROTECTED or "__pycache__" in rel.parts:
            continue
        out[rel.as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def git_index_paths(root):
    """The files a git clone tracks, read from .git/index without git (index versions 2 to 4): {path: (mtime in seconds,
    size)} as git last saw them on disk; None when it cannot be read. (The source zip unpacked over a clone without git
    removes, as a checkout would, the tracked files the new version no longer has, and asks first when a tracked file
    looks edited since: another size or time than git recorded.)"""
    try:
        data = (root / ".git" / "index").read_bytes()
        if data[:4] != b"DIRC":
            return None
        version, count = struct.unpack(">II", data[4:12])
        if version not in (2, 3, 4):
            return None
        pos, prev, out = 12, b"", {}
        for _ in range(count):
            mtime, size = struct.unpack(">I", data[pos + 8:pos + 12])[0], struct.unpack(">I", data[pos + 36:pos + 40])[0]
            flags = struct.unpack(">H", data[pos + 60:pos + 62])[0]
            start = pos + 62 + (2 if version >= 3 and flags & 0x4000 else 0)
            if version == 4:  # (the path: how many bytes of the previous one to drop, then the rest)
                c = data[start]
                start += 1
                drop = c & 127
                while c & 128:
                    c = data[start]
                    start += 1
                    drop = ((drop + 1) << 7) | (c & 127)
                end = data.index(b"\0", start)
                path = prev[:len(prev) - drop] + data[start:end]
                pos = end + 1
            else:
                end = data.index(b"\0", start)
                path = data[start:end]
                pos += ((end - pos) + 8) & ~7
            prev = path
            if (flags >> 12) & 3 == 0:
                out[path.decode("utf-8")] = (mtime, size)
        return out
    except (OSError, ValueError, IndexError, struct.error, UnicodeDecodeError):
        return None


def file_sha256(path):
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def update_zip(channel, over_git=False):
    """Updates a zip download (no .git), or a git clone that git cannot update (`over_git`): checks for local
    changes, downloads the archive, unpacks over ROOT (leaving assets/, build/, .toolchain/ and .git/ alone), and
    returns a restore callback. A file is deleted only when the previous archive brought it (source_hashes; the first time
    over a clone: when git tracks it), so files of the player's own are never removed."""
    state = load_state()
    recorded = state.get("source_hashes") or {}
    tracked = git_index_paths(ROOT) if over_git and not recorded else None
    if recorded:
        changed = [rel for rel, h in recorded.items() if file_sha256(ROOT / rel) not in (None, h)]
    else:  # (the first time over a clone: the files that look edited since git last wrote them)
        changed = []
        for rel, (mtime, size) in sorted((tracked or {}).items()):
            try:
                st = (ROOT / rel).stat()
            except OSError:
                continue
            if st.st_size & 0xffffffff != size or int(st.st_mtime) & 0xffffffff != mtime:
                changed.append(rel)
    if changed and not ask(f"{len(changed)} source file(s) have local changes (e.g. {changed[0]}). Overwrite them?", False):
        fail("Local changes were kept; the update was cancelled.")
    if channel == "stable":
        tag = latest_release_tag()
        url = f"{GITHUB_ZIP}/tags/{tag}.zip"
        label = f"release {tag}"
        ver_lines = [f"describe {tag}", "commit ", "branch "]
    else:
        try:
            sha = str(github_get_json("/commits/main").get("sha") or "").strip()
        except Exception:
            sha = ""
        url = f"{GITHUB_ZIP}/heads/main.zip"
        label = f"main ({sha[:7]})" if sha else "main"
        ver_lines = [f"describe {sha[:7]}" if sha else "describe main", f"commit {sha}", "branch main"]

    BUILD_DIR.mkdir(exist_ok=True)
    archive = BUILD_DIR / "update-download.zip"
    unpack_dir = BUILD_DIR / "update-unpack.tmp"
    backup_dir = BUILD_DIR / "update-src.bak"
    shutil.rmtree(unpack_dir, ignore_errors=True)
    shutil.rmtree(backup_dir, ignore_errors=True)
    print(f"  Downloading {label}...", flush=True)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "rac1-native-setup-update"})
        with urllib.request.urlopen(req, timeout=30) as r, open(archive, "wb") as f:
            shutil.copyfileobj(r, f)
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(unpack_dir)
            if not WINDOWS:  # (extractall drops the Unix modes: ./setup.sh and the tools' scripts stay executable)
                for info in zf.infolist():
                    mode = (info.external_attr >> 16) & 0o777
                    if mode & 0o111 and not info.is_dir():
                        target = unpack_dir / info.filename
                        target.chmod(target.stat().st_mode | (mode & 0o111))
    except Exception as e:
        archive.unlink(missing_ok=True)
        shutil.rmtree(unpack_dir, ignore_errors=True)
        fail(f"Could not download or unpack {label} ({e}). Your current build was not changed.")
    archive.unlink(missing_ok=True)
    subdirs = [d for d in unpack_dir.iterdir() if d.is_dir()]
    src_root = subdirs[0] if len(subdirs) == 1 else unpack_dir

    # Back up the source files about to be replaced or removed
    new_hashes = zip_source_files(src_root)
    new_files = list(new_hashes.keys())
    new_set = set(new_files)
    # (removed: what the previous archive brought and this one no longer has; only inside the archive's folders, as
    # the records made by 0.3.0's updater also list the player's own files in the top folder)
    new_dirs = {rel.split("/", 1)[0] for rel in new_files if "/" in rel}
    removed = [rel for rel in recorded if rel not in new_set and "/" in rel and rel.split("/", 1)[0] in new_dirs
               and rel.split("/", 1)[0] not in ZIP_PROTECTED and (ROOT / rel).is_file()]
    if tracked is not None:  # (the first time over a clone: what git tracks, as a checkout would)
        removed = sorted(rel for rel in tracked if rel not in new_set and rel.split("/", 1)[0] not in ZIP_PROTECTED
                         and (ROOT / rel).is_file())
    old_files = [rel for rel in new_files if (ROOT / rel).is_file()] + removed
    backup_dir.mkdir(parents=True, exist_ok=True)
    for rel in old_files:
        dst = backup_dir / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / rel, dst)
    old_set = set(old_files)
    old_ver = UPDATE_VERSION_FILE.read_bytes() if UPDATE_VERSION_FILE.is_file() else None
    if over_git:
        ver_lines.append("over-git 1")  # (cmake/version.cmake: this, not the clone's stale git commit)

    def restore():
        for rel in new_files:
            if rel not in old_set:
                (ROOT / rel).unlink(missing_ok=True)
        for rel in old_files:
            src = backup_dir / rel
            if src.is_file():
                dst = ROOT / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
        if old_ver is None:
            UPDATE_VERSION_FILE.unlink(missing_ok=True)
        else:
            UPDATE_VERSION_FILE.write_bytes(old_ver)
        shutil.rmtree(backup_dir, ignore_errors=True)

    try:
        for rel in removed:
            (ROOT / rel).unlink(missing_ok=True)
        copied = 0
        for rel in new_files:
            dst = ROOT / rel
            # (a file the new version does not change is left as it is, with its time: the build then recompiles
            # only what changed, not everything the archive brought)
            if rel in old_set and file_sha256(dst) == new_hashes[rel]:
                continue
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_root / rel, dst)
            copied += 1
        UPDATE_VERSION_FILE.parent.mkdir(parents=True, exist_ok=True)
        UPDATE_VERSION_FILE.write_text("\n".join(ver_lines) + "\n", encoding="ascii")
    except OSError as e:
        shutil.rmtree(unpack_dir, ignore_errors=True)
        restore()
        fail(f"Could not unpack the new sources ({e}). Your current build was not changed.")
    shutil.rmtree(unpack_dir, ignore_errors=True)
    state = load_state()
    state["source_hashes"] = {rel: file_sha256(ROOT / rel) for rel in new_files}
    if over_git:
        state["zip_over_git"] = True
    save_state(state)
    ok(f"Unpacked {label} ({copied} of its {len(new_files)} files changed, {len(removed)} removed).")
    return restore


def run_update(args):
    """Fetches the update (git or zip), runs the updated setup in a child process while keeping a backup of the
    current executable and generated sources, and rolls back if anything fails."""
    channel = configured_channel(args.channel)
    print(color("1", f"\nRatchet & Clank (rac1-native) update ({channel} channel)"))
    wait_for_exe()
    step(1, 2, f"Fetching the latest {'release' if channel == 'stable' else 'commits on main'}")
    exe_bak = EXE.with_name(EXE.name + ".bak")
    state_bak = STATE_FILE.with_name(STATE_FILE.name + ".bak")
    gen_bak = CODE_ROOT / "src.bak"
    exe_bak.unlink(missing_ok=True)
    state_bak.unlink(missing_ok=True)
    shutil.rmtree(gen_bak, ignore_errors=True)
    if EXE.is_file():
        shutil.copy2(EXE, exe_bak)
    had_state = STATE_FILE.is_file()
    if had_state:
        shutil.copy2(STATE_FILE, state_bak)
    if GEN_SRC.is_dir():
        shutil.copytree(GEN_SRC, gen_bak)

    try:
        restore_sources = fetch_update(channel)
    except SystemExit:
        # (stopped before anything changed: no network, local changes...; the backups, a copy of the program among
        # them, are not left behind)
        exe_bak.unlink(missing_ok=True)
        state_bak.unlink(missing_ok=True)
        shutil.rmtree(gen_bak, ignore_errors=True)
        raise
    finish_update(args, restore_sources, exe_bak, state_bak, gen_bak, had_state)


def fetch_update(channel):
    """The new sources, with git or the source zip; returns the callback that puts the previous ones back (or None)."""
    restore_sources = None
    if not (ROOT / ".git").exists():
        restore_sources = update_zip(channel)
    else:
        problem = git_problem()
        state = load_state()
        if problem is None and state.get("zip_over_git"):
            # (an earlier update unpacked the source zip over this clone: git sees those files as changes)
            if subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT,
                              capture_output=True, text=True).stdout.strip():
                problem = "an earlier update replaced this clone's files from the source zip, so git sees them as changed"
            else:  # (the clone was put back in step with git: git updates it again)
                state.pop("zip_over_git", None)
                state.pop("source_hashes", None)
                save_state(state)
                UPDATE_VERSION_FILE.unlink(missing_ok=True)
        if problem is None:
            restore_sources = update_git(channel)
        else:
            warn(f"This folder is a git clone, but it cannot be updated with git: {problem}.")
            print(f"      Downloading the {'release' if channel == 'stable' else 'main branch'}'s source zip from GitHub "
                  "instead (no git needed): it replaces the source files,\n"
                  "      and leaves assets/, build/, .git/ and any file of yours alone. "
                  "(To update with git again later: install Git, then\n"
                  "      `git fetch` and `git reset --hard origin/main`, or the release's tag.)", flush=True)
            restore_sources = update_zip(channel, over_git=True)
    return restore_sources


def finish_update(args, restore_sources, exe_bak, state_bak, gen_bak, had_state):
    """The updated setup, in a child process; on failure, the previous sources, program and generated code back."""
    step(2, 2, "Rebuilding and updating the game's data")
    child_cmd = [sys.executable, str(ROOT / "tools/setup.py"), "--no-play"]
    if args.iso:
        child_cmd += ["--iso", args.iso]
    if args.jobs:
        child_cmd += ["-j", str(args.jobs)]
    if args.force:
        child_cmd += ["--force", ",".join(sorted(args.force))]
    if args.native_arch:
        child_cmd += ["--native-arch", args.native_arch]
    rc = subprocess.run(child_cmd, cwd=ROOT).returncode
    if rc != 0:
        warn("The update failed: restoring the previous build and sources...")
        if restore_sources is not None:
            restore_sources()
        if gen_bak.is_dir():
            shutil.rmtree(GEN_SRC, ignore_errors=True)
            gen_bak.replace(GEN_SRC)
        if state_bak.is_file():
            state_bak.replace(STATE_FILE)
        elif not had_state:  # (no record before: none after, not the one the source zip's unpacking wrote)
            STATE_FILE.unlink(missing_ok=True)
        if exe_bak.is_file():
            exe_bak.replace(EXE)
        fail("The update did not succeed; your previous build was kept as it was.")

    exe_bak.unlink(missing_ok=True)
    state_bak.unlink(missing_ok=True)
    shutil.rmtree(gen_bak, ignore_errors=True)
    shutil.rmtree(BUILD_DIR / "update-src.bak", ignore_errors=True)
    (config_dir() / "update_check.txt").unlink(missing_ok=True)
    if args.play or (not args.no_play and ask("Start the game now?", True)):
        sys.exit(subprocess.run([str(EXE)], cwd=ROOT).returncode)


# --- main ---------------------------------------------------------------------------------------

def parse_force(value):
    groups = STEP_NAMES if value == "all" else [g.strip() for g in value.split(",") if g.strip()]
    bad = [g for g in groups if g not in STEP_NAMES]
    if bad:
        raise argparse.ArgumentTypeError(f"unknown step {', '.join(bad)} (steps: {', '.join(STEP_NAMES)})")
    return set(groups)


def check_disc(arg):
    iso = find_iso(arg)
    if not iso.is_file():
        fail(f"File not found: {iso}")
    boot = looks_like_ps2_disc(iso)
    if boot is None:
        fail(f"{iso} is not a PlayStation 2 disc image (an ISO 9660 image).\n"
             "         Use a plain .iso dump of your disc, not a compressed one (.chd, .cso, .gz) or a .bin/.cue pair.")
    if boot not in KNOWN_DISCS:
        fail(f"{iso} is another game ({boot}), not Ratchet & Clank.")
    serial, region, supported, good_size = KNOWN_DISCS[boot]
    if not supported:
        fail(f"This is Ratchet & Clank's {region} disc ({serial}). "
             "Only the North American disc, SCUS-97199, and the European PAL disc, SCES-50916, are supported for now.")
    if iso.stat().st_size < good_size:  # (cut short, or trimmed: checked fully after the build)
        warn(f"The image is smaller than a good dump ({iso.stat().st_size} bytes, not {good_size}): "
             "it may be incomplete.")
    ok(f"Ratchet & Clank, {region} ({serial}): {iso}")
    return iso


def print_code_inputs():
    """For CMake (src/game/CMakeLists.txt): the fingerprint of the generated code's sources, then the sources."""
    print(code_sources_fingerprint())
    for p in sources_files(CODE_SOURCES):
        print(p)


def main():
    if sys.argv[1:] == ["--code-inputs"]:
        print_code_inputs()
        return
    if sys.argv[1:] == ["--container-info"]:  # (for the launcher: the build container's name, image and packages)
        print(f"name {CONTAINER_NAME}\nimage {CONTAINER_IMAGE}\npackages {' '.join(container_packages())}")
        print(f"immutable {immutable_system() or ''}".rstrip())
        print(f"in_container {1 if in_container() else 0}")
        return
    if sys.argv[1:] == ["--check-system"]:  # step 1 only: exit status 0, 1 (missing), or 20 (build in a container)
        check_system()
        return
    if sys.argv[1:] == ["--check"]:  # exit status 10 when a setup is due (what it would redo, one per line)
        due = setup_due()
        print("\n".join(due) if due else "up to date")
        sys.exit(10 if due else 0)
    ap = argparse.ArgumentParser(description="Sets up rac1-native: checks the system and your disc image, generates "
                                             "the game's code from it, builds the game and generates the game's data.")
    ap.add_argument("--iso", help="your disc image (Ratchet & Clank: NTSC-U SCUS-97199, or PAL SCES-50916)")
    ap.add_argument("--update", action="store_true",
                    help="update to the latest release (or main on the development channel), then rebuild what changed")
    ap.add_argument("--channel", choices=("stable", "development"), default=None,
                    help="with --update: the channel to update to (default: Main menu > Updates > Update Channel)")
    ap.add_argument("--force", nargs="?", const="all", default=set(), type=parse_force, metavar="STEPS",
                    help=f"regenerate even if up to date: everything, or some steps ({','.join(STEP_NAMES)})")
    ap.add_argument("-j", "--jobs", type=int, default=None, help="parallel jobs (the code generation and the build)")
    ap.add_argument("--no-build", action="store_true", help="skip the build")
    ap.add_argument("--build-only", action="store_true",
                    help="only build the game (and generate its code first if that is missing or out of date)")
    ap.add_argument("--play", action="store_true", help="start the game when the setup finishes without asking")
    ap.add_argument("--no-play", action="store_true", help="do not offer to start the game at the end")
    ap.add_argument("--native-arch", choices=("on", "off"), default=None,
                    help="compile the game for this machine's CPU (on: -march=native, AVX2 / AVX-512 where the CPU has them, "
                         "the game's own update about 8 %% faster, its state bit for bit the same; the build then runs on this "
                         "machine's CPU only) or for any x86-64 CPU (off, the default); kept for the next builds")
    args = ap.parse_args()
    if args.iso:
        args.iso = str(Path(args.iso).expanduser().resolve())  # (relative to where the user ran the setup)
    os.chdir(ROOT)
    if args.update:
        run_update(args)
        return
    jobs = args.jobs or default_jobs()

    print(color("1", "\nRatchet & Clank (rac1-native) setup"))
    # --build-only needs the disc only when the generated code must be made again
    need_disc = not args.build_only or not code_current() or bool(args.force & {"elf", "overlay", "code"})
    # the image gone once the disc was extracted (the game does not read it): a setup with nothing to make from the disc
    # (only the build, after an update) goes on without it
    known, disc = current_disc()
    no_image = (need_disc and not args.build_only and not args.iso and not args.force and known is None and disc is not None
                and not disc_steps_due(None, disc))
    if no_image:
        need_disc = False
    total = 2 if no_image else 6 if not args.build_only else (4 if need_disc else 2)
    n = 0

    def next_step(msg):
        nonlocal n
        n += 1
        step(n, total, msg)

    next_step("Checking your system")
    check_system(args.iso)

    iso = None
    if need_disc:
        next_step("Checking your disc image")
        if not args.iso and known is None and disc is not None:
            warn("Something made from your disc must be made again (after an update): the setup needs your disc image.")
        iso = check_disc(args.iso)
        if not args.build_only:
            next_step("Extracting your disc's files")
            extract_disc(iso, args.force)
        next_step("Generating the game's code from your disc")
        prepare_code(iso, args.force, jobs)
    elif no_image:
        ok(f"Your disc image is not needed: everything made from it is up to date ({DISC_DIR.relative_to(ROOT)}{os.sep} "
           "and the rest).")

    next_step("Building the game")
    if args.no_build:
        if not EXE.exists():
            fail(f"--no-build, but {EXE.relative_to(ROOT)} does not exist yet.")
        ok("Skipped (--no-build)")
    else:
        build(jobs, args.native_arch)
    if args.build_only:
        return

    if not no_image:
        next_step("Generating the game's data from your disc")
        r = subprocess.run([str(EXE), "--setup", "--iso", str(iso), "--check-iso"], cwd=ROOT, env=game_env())
        if r.returncode:
            sys.exit(1)
        generate(iso, args.force)
        check_disc_files(iso)

    print(color("1;32", "\nAll done: Ratchet & Clank is ready."))
    print(f"The game reads your disc's files in {DISC_DIR.relative_to(ROOT)}{os.sep}, not your disc image: it no longer "
          "needs the image.\nKeep the image anyway: an update that changes what is made from the disc asks for it again.")
    play = "build\\rac1-native.exe" if WINDOWS else "./build/rac1-native"
    print(f"To play, run this from {ROOT}:\n    {play}\n")
    if args.play or (not args.no_play and ask("Start the game now?", True)):
        sys.exit(subprocess.run([str(EXE)], cwd=ROOT).returncode)


if __name__ == "__main__":
    main()
