# Setup

From a fresh clone and your own disc image to the game running.

> **The easy way, on Windows and Linux (x86-64), is the launcher**: one file from the latest release
> that downloads the game and does every step below by itself, in a window. See
> [The launcher](launcher.md). This page is the same by hand.

This repository contains none of the game's code or data. Everything the game needs is extracted
and generated from your own disc, on your machine: the game's data, and also part of the port's
source code, the files that are a mechanical translation of the game's own code (see
[Architecture](architecture.md#the-generated-code)).

## What you need

### Your game disc, as an ISO image

- **Ratchet & Clank**, North American release (NTSC-U), serial **`SCUS-97199`**, or the European
  release (PAL), serial **`SCES-50916`**. Either disc alone is enough. PAL support is new and still
  being tested: see [The European PAL disc](#the-european-pal-disc).
- An unmodified ISO image of the disc. A good dump is 4,214,095,872 bytes for the North American
  disc and 4,213,997,568 bytes for the PAL one; a different size is accepted with a warning.
- Other releases (the Japanese one, `SCPS-15055`) are not supported.

### A Linux system with

(Windows: see [On Windows](#on-windows) below.)

- **CMake** 3.20 or newer and a **C++20 compiler** (GCC 11 or newer, or Clang 13 or newer);
- **pkg-config**;
- **OpenGL**, **X11** and **Wayland** development files. The window library, GLFW, is built from
  source for both X11 and Wayland (it picks the one your desktop uses when the game starts), so it
  needs both: X11 with Xrandr, Xinerama, Xcursor, XInput and Xext, and Wayland with xkbcommon;
- **Python 3.8** or newer (the setup tool, the code generators and the overlay matcher are written
  in Python; no extra modules);
- optionally **FFmpeg** development files (`libavcodec`, `libavutil`), to play the game's movies.
  Without them the game runs and skips the movies;
- optionally **glslc** (shaderc), to build the experimental [Vulkan renderer](vulkan.md)'s shaders
  (`glslangValidator` works too); without it the game is built with OpenGL only;
- an internet connection for the first build (CMake downloads GLFW, GLM, Dear ImGui, TinyGLTF,
  miniaudio, Vulkan-Headers and volk);
- about 1 GB of free disk for the game's data and generated code, besides the disc image, plus about
  2 GB for the build;
- a GPU and driver with **OpenGL 4.6** (and **Vulkan 1.3** for the experimental Vulkan renderer,
  which is optional: OpenGL is the default).

The setup tool checks all of this and tells you what is missing, with the command to install it on
Debian and Ubuntu (and their derivatives), Fedora, and Arch. It never installs anything without
asking. For reference, everything at once:

```bash
# Debian / Ubuntu
sudo apt install cmake build-essential pkg-config libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev libwayland-dev libxkbcommon-dev python3 libavcodec-dev libavutil-dev glslc

# Fedora
sudo dnf install cmake gcc-c++ pkgconf-pkg-config mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libXext-devel wayland-devel libxkbcommon-devel python3 ffmpeg-free-devel glslc

# Arch
sudo pacman -S --needed cmake base-devel pkgconf mesa libx11 libxrandr libxinerama libxcursor libxi libxext wayland libxkbcommon python ffmpeg shaderc
```

## Step by step

1. **Clone the repository:**

   ```bash
   git clone https://github.com/RAC-Native/rac1-native-game.git
   cd rac1-native-game
   ```

2. **Run the setup, with the path to your disc image:**

   ```bash
   ./setup.sh --iso /path/to/your/ratchet.iso
   ```

   (Without `--iso` it asks for the path.) It goes through five steps:

   1. **Checks your system** and lists anything missing, with the install command for your
      distribution. It offers to run that command for you (it asks for your password); say no to
      run it yourself.
   2. **Checks your disc image**: that it is Ratchet & Clank's North American disc and the right
      size.
   3. **Generates the game's code** from your disc, with Python only: it copies the game's boot
      executable from the disc, matches each level's code against the first level's (the overlay
      address maps), then translates the parts of the game's code the port takes over mechanically
      into C++ under `assets/generated/src/` (about 140 files). This takes one to two minutes and
      uses the same number of parallel jobs as the build.
   4. **Builds the game** with CMake, in RelWithDebInfo, from the repository's sources and the
      generated ones. The result is `build/rac1-native`. It uses one job per CPU core, fewer when
      memory is short (`-j` changes that).
   5. **Generates the game's data** into `assets/` from your disc, step by step, showing each level
      as it goes. This takes a few minutes. First it checks the disc image fully (that every level
      is inside it), and, because the game reads the disc while it runs, makes
      `assets/iso/rac1-ntsc-u.iso` point to your image (a link: nothing is copied). Keep the image
      where it is; if you move it, run the setup again with its new path.

   At the end it offers to start the game.

3. **Play.** From the repository's root:

   ```bash
   ./build/rac1-native
   ```

   The game must be started from the repository's root: it finds `assets/` and its other files
   relative to the current directory.

## What the setup generates

All of it goes into `assets/`, which git ignores:

| Path | What |
|---|---|
| `assets/iso/rac1-ntsc-u.iso` (`rac1-pal.iso` for the PAL disc) | a link to your disc image, which the game reads while it runs |
| `assets/iso/disc-path.txt` | on Windows, when the link cannot be made: your disc image's path, which the game reads instead |
| `assets/dumps/SCUS_971.99` (`SCES_509.16` for the PAL disc) | the game's executable, copied from the disc |
| `assets/generated/src/` | the generated part of the game's source code, and `generated_sources.cmake`, its list for the build (only the build reads it) |
| `assets/generated/mrp/moby/` | the moby models and textures for the renderer, per level |
| `assets/generated/mrp/world/` | the world's geometry and textures for the renderer, per level |
| `assets/generated/mrp/{moby,world}/flight/` | the loading screen's scene: the ship's flight between planets |
| `assets/generated/overlay_map/` | the per-level address maps (made by `tools/overlay_match/`; for the PAL disc also `region_level<n>.txt`, `region_boot.txt` and `codegen/`, see below) |
| `assets/setup-state.json` | what the setup generated, and from which code (so a re-run redoes only what changed) |

How these are made is described in [Rendering](rendering.md) and
[Architecture](architecture.md#per-level-overlays-and-the-ov-macros).

## The European PAL disc

The port is written against the North American build: every address in it is that build's. The PAL
release is the same game built five days later, with most of its code moved by a few bytes, a few
functions changed, and some data changed (its translations, some art, the collision of four levels).

**How it works.** The region table, `data/regions/SCES-50916.txt`, says where the PAL build put each
piece of the North American build's code and data: runs of addresses and their shifts, and nothing of
the game's bytes. With it, from the PAL disc alone:

- the setup makes the overlay maps from the table (`tools/overlay_match/region_maps.py apply`);
- the code generators rebuild each overlay's code at the North American addresses before they translate
  it (`tools/overlay_match/region_view.py`), so the generated sources are the same as from the North
  American disc;
- while the game runs, the addresses of the level-only and title code go through the table
  ([Architecture](architecture.md#the-region-layer)).

The game runs in the PAL game's own 60 Hz mode (its Options offer 50 Hz or 60 Hz). The 50 Hz mode is
not supported yet.

**What differs from the North American disc.**

- The PAL disc's own data: its corrected French, German, Spanish and Italian texts, some re-exported
  art (for example Veldin's swaying plant, Kerwan's shrubs and textures, Eudora's tree), the collision
  of four levels.
- The save folder is the PAL game's (`BESCES-50916RATCHET`).
- The intro movie, the copyright screen and the title's attract movies are the North American ones
  (the Sony Computer Entertainment America logo). The game picks them by the video mode, not by the
  disc: the PAL disc carries both versions, and in its 60 Hz mode the PAL game plays the 60 Hz ones. A
  European PS2 starts the game at 50 Hz, so there the intro is the European one until the player
  switches to 60 Hz. Nothing of these movies is in the repository: they are read from your disc.

**Known gaps.** A few functions are different code in the PAL build:

- Where the port has a hand-written port of the North American function, that port runs in its place
  (the table maps one to the other). So the PAL fixes in these functions are missing: hero sound 1 on
  the hero's velocity, Rilgar's skill point message, the PAL text wrapping, the PAL new game keeping two
  settings, and the removed developer profile.
- Ten generated functions are on the PAL disc only in their PAL form: the memory card / result screen
  (`screen_draw_tr.cpp`), the hero's horizontal velocity on Rilgar, Blarg, Batalia and Pokitaru, the HUD
  counters on Rilgar (two), Blarg and Umbris, and Rilgar's class 918. From the PAL disc these are
  translated from the PAL code, as temporary stand-ins: they are marked `STAND-IN` in the generated
  sources and are not yet proved against the original. The four hero velocity stand-ins carry the same
  enhancement hook points as the North American code (R&C 3 movement's air braking and the strafe flip's
  speed), placed at the matching instructions of the PAL code (`PAL:` lines in `tools/codegen/hooks.glue`).

## Running it again

- **Run `./setup.sh` again at any time**, for example after a `git pull`. It rebuilds only what
  changed (CMake recompiles only what needs it), generates the code again only when the generators
  (`tools/codegen/`), the overlay matcher, the region table or the headers they read changed, and redoes a data step
  only when the code that makes it changed, or when that step did not finish. A step added by an update (such as the
  flight between planets) runs on its own, without redoing the others. It remembers the disc image's path.
  What was generated, and from which code, is recorded in `assets/setup-state.json`.
- **`./setup.sh --update`** (`setup.cmd --update` on Windows) fetches the latest release (or `main` when
  Options > Advanced > Update Channel is Development), then rebuilds and regenerates only what changed;
  the in-game update notice's **Update Now** button runs it and restarts the game when it finishes.
  A git clone is updated with git; a zip download, or a clone where git is not installed (or cannot read
  the clone), from the release's (or `main`'s) source zip, which replaces the source files and leaves
  `assets/`, `build/`, `.git/` and files of your own alone. It asks before overwriting files you edited.
  After such an update git sees the replaced files as changes, so the next updates use the zip too, until
  the clone is put back in step with git (`git fetch`, then `git reset --hard origin/main` or the release's tag).
- **`./setup.sh --force`** regenerates the code and all the data; **`./setup.sh --force moby,world`**
  only those steps.
- If you build with CMake yourself (`cmake -S . -B build && cmake --build build`), run `./setup.sh`
  once first: configuring stops with a message when the generated code is missing, or out of date
  after a `git pull`.
- **`./setup.sh --native-arch on`** builds the game for your own CPU (AVX2, AVX-512 where it has them):
  the game's own update takes about 8 % less CPU time, and plays exactly the same. The build then runs on
  this CPU only (do not copy it to an older machine); `--native-arch off` goes back.
- **`./build/rac1-native --setup --status`** shows which parts of the data are present.

### All the options

`./setup.sh` (which runs `tools/setup.py`):

| Option | Meaning |
|---|---|
| `--iso <path>` | your disc image (otherwise `$RAC1_ISO`, then `assets/iso/rac1-ntsc-u.iso` or `rac1-pal.iso`, then it asks) |
| `--update` | fetch the latest release (or `main` on the Development channel), then rebuild what changed; on failure the previous build is kept |
| `--channel stable\|development` | with `--update`: override Options > Advanced > Update Channel |
| `--force [steps]` | regenerate even if up to date: everything, or the steps given (`elf,overlay,code,moby,world,flight`) |
| `-j <n>`, `--jobs <n>` | parallel jobs, for the code generation and the build |
| `--no-build` | skip the build |
| `--build-only` | only check the system and build (generating the code first if it is missing or out of date, which needs the disc) |
| `--play` | start the game when the setup finishes without asking |
| `--no-play` | do not offer to start the game at the end |
| `--native-arch on\|off` | compile the game for this machine's CPU (`on`: AVX2 / AVX-512 where it has them, the game's own update about 8 % faster, its state the same bit for bit; the build then runs on this CPU only) or for any x86-64 CPU (`off`, the default); kept for the next builds, a change rebuilds everything |

`./build/rac1-native --setup` (the data generation on its own, run from the repository's root):

| Option | Meaning |
|---|---|
| `--iso <path>` | the disc image (default: `$RAC1_ISO`, then `assets/iso/rac1-ntsc-u.iso` or `rac1-pal.iso`) |
| `--assets <dir>` | where to write the data (default `assets`; the game itself only reads `assets/`) |
| `--force` | regenerate even if present |
| `--step elf\|moby\|world\|flight\|overlay\|all` | run one step only |
| `--check-iso` | only check the disc image, and make the game find it |
| `--status` | show what is present |

On its own, `--setup` skips a file that exists without checking whether it is up to date; that is
what `./setup.sh` adds. `./setup.sh` makes the boot executable and the overlay maps itself, before
the build, because the code generation needs them; the `elf` and `overlay` steps of `--setup` make
the same files.

The code generation on its own, from the repository's root (after `./setup.sh` has made the boot
executable and the overlay maps):

```bash
python3 tools/codegen/generate_all.py --out assets/generated --no-insn-comments
```

`--list` shows its units, `--files` the files it writes, `--only <unit>` runs some of them. Without
`--no-insn-comments`, each translated line keeps the original instruction as a comment.

## On Windows

The Windows build is made with **clang** (or GCC) from **MSYS2**, not with Visual Studio's compiler
(see [Platforms](platforms.md)).

**The simplest way is the launcher**, `rac1-launcher.exe` from the latest release: it does all of
the steps below by itself, in a window. See [The launcher](launcher.md). What follows is the same by
hand.

1. Get the repository (with git, or as a zip) and **double-click `setup.cmd`** in its folder (or drop
   your disc image on it, or run `setup.cmd --iso "C:\Users\you\Games\ratchet.iso"`; the quotes are
   needed when the path has spaces or characters such as `&`).

   Nothing needs to be installed first. The first time, the setup downloads the tools that build the
   game (about 1 GB, a few minutes): a private copy of [MSYS2](https://www.msys2.org) in the
   repository's `.toolchain\` folder, with Python, clang, CMake, Ninja and FFmpeg
   (`tools\windows\toolchain.ps1`). Nothing is installed elsewhere: deleting `.toolchain\` removes it.
   Then `tools/setup.py` runs, with the same steps as on Linux, and the result is
   `build\rac1-native.exe`.

   With your own MSYS2, the setup can also run from its **CLANG64** shell (`./setup.sh`, or
   `setup.cmd`): it then uses that shell's tools and tells you which packages to install.

   The private MSYS2 does not include `glslc`, so a game built this way has OpenGL only (the
   Renderer row's Vulkan falls back to OpenGL). The experimental [Vulkan renderer](vulkan.md) needs
   `mingw-w64-clang-x86_64-shaderc` installed in the MSYS2 the game is built with.

2. **Play**: double-click **`play.cmd`** in the repository's folder. (The game must start from the
   repository's folder, where it finds `assets\`: `build\rac1-native.exe` double-clicked on its own
   would not find them. From a terminal in that folder, `build\rac1-native.exe` works too.)

   `play.cmd` first checks that the setup is up to date (`tools/setup.py --check`, which changes
   nothing): after a new version is downloaded over this one, or when a setup did not finish, a
   window offers to run the setup again (**Run the setup**) or to start the game as it is (**Play
   anyway**).

What differs from Linux:

- **The disc image.** Windows only lets programs make links with Developer Mode on (or as an
  administrator). Without it, the setup writes your image's path into `assets/iso/disc-path.txt`
  instead, and the game reads it from there. Keep the image where it is, as on Linux.
- **The settings and saves** are in `%APPDATA%\rac1-native\` (the settings files, `controls.txt`,
  and the memory card in `memcard\`), instead of `~/.config/rac1-native/` and
  `~/.local/share/rac1-native/`.
- **The DLLs.** Built without FFmpeg, the program needs only Windows' own DLLs (the C++ runtime is
  linked in). Built with FFmpeg (the movies), it uses MSYS2's DLLs: the build copies them into
  `build\`, beside the program.
- **The GPU.** On a laptop with two (an integrated one and an NVIDIA or AMD one), the program asks
  the drivers for the dedicated one (`src/host/gpu_preference_win32.cpp`). A choice made in Windows'
  Settings (System > Display > Graphics) or in the driver's control panel still wins.

## Troubleshooting

**"This is Ratchet & Clank's NTSC-J disc"**: only the North American disc, `SCUS-97199`, and the
European PAL disc, `SCES-50916`, are supported for now.

**"... is missing: the PAL disc needs it"**: the region table `data/regions/SCES-50916.txt` is part of
the repository; restore it (`git checkout data/regions`).

**"... is not the build the table was made from (sha1 differs)"**: the PAL disc image is not the
release the region table describes (another PAL edition, such as a Platinum re-release, or a damaged
dump). Only the original PAL release is supported.

**"not a PlayStation 2 disc image"** or **"This is not a disc image (ISO 9660)"**: the file is not a
plain ISO. Use an unmodified `.iso` dump, not a compressed format (`.chd`, `.cso`, `.gz`) or a
`.bin`/`.cue` pair.

**"This is another game"**: the image is a different PS2 game.

**"The disc image is damaged or incomplete"**: the image is cut short (an interrupted copy or
dump). Dump the disc again.

**A size warning**: the image is not the expected 4,214,095,872 bytes but looks complete. It may
still work; if the game misbehaves, check your dump against the Redump record.

**"your disc image was not found"** when starting the game: the image was moved or deleted since the
setup. Run `./setup.sh --iso <new path>`.

**"The game's generated sources are missing"** or **"... are out of date"** when building: the build
needs the code that `./setup.sh` generates from your disc. Run `./setup.sh` (it generates the code,
then builds). If the code generation itself fails, its full output is in `build/setup-codegen.log`.

**"the game's data is missing"** when starting the game: it was started from another directory, or
the setup did not finish. Run it from the repository's root, and run `./setup.sh` again.

**The build stops with "Failed to find wayland-scanner"**, or about missing X11 or XKB headers: the
Wayland or X11 development files are missing; install the packages above.

**The movies are skipped**: FFmpeg's development files were not found when the game was built.
Install them (the optional packages above), delete `build/CMakeCache.txt` so the build looks again,
and run `./setup.sh`.

**Windows: "error adding trust anchors" or "Problem with the SSL CA cert"** when the build downloads
GLFW: MSYS2's certificate files are empty (#79, #69, #63). MSYS2's CLANG64 certificate tool breaks
when the folder's path has a space in it, and leaves them empty. Since 0.5.0 `setup.cmd` (and the
launcher) make them again with MSYS2's own tool whenever they are missing or empty: run `setup.cmd`
again. No need to reinstall anything by hand.

**Windows: "Fatal Python error" while generating the code** (#72): Python itself crashed in one of
the parallel jobs, not the generator. Since 0.5.0 a unit that crashes is run once more on its own;
if it still fails, run `setup.cmd -j 1` (or, in the launcher, Settings > Parallel jobs > 1).

**Windows: "... rac1-native.exe is missing after the build, although the build finished"**: an
antivirus (Windows Security too) removed the newly built program, which it does not know. Look in
its protection history, allow the file (or exclude the game's folder), and run the setup again.

**A gamepad is not recognised**: see [Input](input.md#how-the-host-builds-the-packet) to add its
mapping. Gamepads can be connected while the game runs.
