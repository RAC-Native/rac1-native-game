# The launcher (Windows and Linux)

The launcher is the easy way to get the game: one file to download and open, with nothing to install
first. It downloads the game, sets it up from your own disc image, keeps it up to date, starts it,
and manages your saves.

It is attached to each [release](https://github.com/RAC-Native/rac1-native-game/releases):

| System | File | Needs |
|---|---|---|
| Windows 10 or 11 | `rac1-launcher.exe` | nothing (.NET Framework 4.8, which both have) |
| Linux (x86-64) | `rac1-launcher-linux-x64` | a desktop (X11 or Wayland); everything else is in the file |

> **Windows** may warn that the program is from an unknown publisher (SmartScreen: "Windows
> protected your PC"). The launcher is not signed: click **More info**, then **Run anyway**.
>
> **Linux** does not run a downloaded file until it is allowed to: in the file manager, right-click
> it, **Properties**, and turn on **Allow executing file as program** (or, in a terminal,
> `chmod +x rac1-launcher-linux-x64`). Then double-click it.
>
> The launcher's source is `tools/launcher/` in this repository.

## Using it

1. Download the launcher for your system and open it.
2. Click **Install** and choose your disc image (a plain `.iso` of the North American disc,
   SCUS-97199, or the European one, SCES-50916).
3. Wait. The first time takes a while (often 20 to 40 minutes): the launcher downloads the latest
   release, then builds the game and generates its code and data from your disc. The progress shows
   on the **Play** page; **Show the details** shows the setup's own messages.
   - On **Windows** it also downloads the tools that build the game (about 1 GB, a private copy of
     MSYS2 in the game folder).
   - On **Linux** the tools come from your distribution. When some are missing, the launcher says so
     and offers to install them (Debian, Ubuntu and their derivatives, Fedora, Arch): your desktop
     asks for your password, as for any other system change. Where that cannot be done, it shows the
     command to run in a terminal. On the Steam Deck, Bazzite and the other immutable systems it
     builds the game in a container instead (see
     [below](#steam-deck-bazzite-and-other-immutable-systems)).
4. Click **Play**. The launcher puts a copy of itself in the game's folder and a shortcut to it: in
   the Start menu (and, if you say yes, on the desktop) on Windows, in the applications menu on Linux.
   Next time, start the game from there.

When a new release is out, the **Play** page says so: **Update** downloads it and rebuilds what
changed, and your saves and settings are kept. The launcher updates itself the same way.

When the game closes with an error right after starting, the launcher shows the game's last
messages (they are also in the game folder's `build/game.log`). The most common cause is a graphics
driver without OpenGL 4.6.

## The pages

**Play**: Install, Set up (when the setup needs to run again, for example after a setup that did not
finish), Play, and Update. Cancel stops a running setup; the next run continues from where it stopped.

**Saves**: the save slots on the virtual memory card (see [Saves](saves.md)), with the time each was
last saved. **Back up all** puts the whole card in a zip; **Restore a backup** puts a zip back (the
saves as they are go first into `save-backups` beside the card); **Delete the slot** moves a slot to
the Recycle Bin, or to the trash on Linux.

**Settings**:

- **Game folder**: where the game goes. By default `%LOCALAPPDATA%\rac1-native` on Windows (or
  `C:\rac1-native` when that path has spaces or non-ASCII letters), `~/.local/share/rac1-native/game`
  on Linux. Choose a folder that already has the game (a clone, or a zip set up with `setup.cmd` or
  `./setup.sh`) to use it from the launcher.
- **Disc image**: the `.iso` the setup uses.
- **Update channel**: **Stable** gets the releases; **Development** gets the newest changes on
  `main`, before they are released (new fixes sooner, and new bugs too). It is the same setting as
  the game's Options > Advanced > Update Channel (the `update_channel` line of
  `advanced_options.txt`, see [Settings](settings.md)): changing it in one changes it in the other.
  Install and Update follow it: on Development, Install downloads `main` as it is now, the **Play**
  page says when `main` has moved since, and Update gets it (`tools/setup.py --update --channel
  development`). Back on Stable, the next Update goes to the latest release.
- **Parallel jobs**: how many jobs the setup runs at once. Automatic is fine; 1 is the slowest and
  uses the least memory.
- **Set up again from scratch**: generates everything again from the disc and rebuilds (the setup's
  `--force`).
- **Open the setup's logs**: the game folder's `build` folder, with `launcher.log` (everything the
  launcher ran and what it printed), `setup-build.log`, `setup-codegen.log` and `game.log`. Attach
  them to a bug report.

The game's other settings (graphics, controls) are in the game's Options menu.

## Where things are

| | Windows | Linux |
|---|---|---|
| The launcher's settings (`launcher.txt`: game folder, disc image, parallel jobs) | `%APPDATA%\rac1-native\` | `~/.config/rac1-native/` |
| The game's settings, with `advanced_options.txt` (the update channel) | `%APPDATA%\rac1-native\` | `~/.config/rac1-native/` |
| The saves (`memcard\`) and the launcher's backups (`save-backups\`) | `%APPDATA%\rac1-native\` | `~/.local/share/rac1-native/` |
| The shortcut | Start menu, desktop | `~/.local/share/applications/rac1-native.desktop` (and `rac1-native-play.desktop` with a build container) |

(`XDG_CONFIG_HOME` and `XDG_DATA_HOME` move the Linux ones, as for the game.)

### progress.json

For the [RAC Native launcher](https://github.com/RAC-Native/rac-native-launcher), which shows each game of the
trilogy as a planet on a galactic map, the game writes `progress.json` in its data folder, beside `memcard`
(`%APPDATA%\rac1-native\` on Windows, `~/.local/share/rac1-native/` on Linux):

```json
{"planet": "novalis", "planet_name": "Novalis", "level": 1, "visited": ["veldin", "novalis"], "percent": 11, "playtime_s": 1234, "updated": "2026-10-25T10:00:00Z"}
```

The planet is the level being played (Veldin's two levels are one planet). The planets reached and the time
played are counted by the game while it runs, not read from the memory card: they follow the last game played,
and start again when the title goes to Veldin (a new game, or a save made on Veldin). The time counts the frames
in a level, a quarter of a second at most per frame. The file is written when the game goes to another level,
once a minute in a level and when it closes, to a temporary file renamed over the old one. `RAC1_PROGRESS` moves
it, or turns it off with `0`; a headless run writes it only when that is set (src/host/progress.cpp).

## What it runs

The launcher does what `setup.cmd` and `./setup.sh` do, without a console window:

1. On Windows, `tools\windows\toolchain.ps1`: the private MSYS2 in `.toolchain\` (see
   [Setup](setup.md#on-windows)).
2. `tools/setup.py --no-play --iso <your image>` (with `--update --channel ...` to update, `--force`
   to set up again from scratch), with `RAC1_LAUNCHER=1`: the setup then also prints `@step` and
   `@progress` lines, which the launcher shows as its progress bar. On Linux it runs with the system's
   `python3`.

On Linux, when the setup reports a required package missing (on a system that is not immutable: see
[above](#steam-deck-bazzite-and-other-immutable-systems) for the others), the launcher takes the command the
setup prints (`sudo apt install ...`, `sudo dnf install ...`, `sudo pacman -S --needed ...`), asks,
and runs it as root with `pkexec` (the desktop's password window; without one, `pkexec` fails at
once and the launcher shows the command to run in a terminal). Then it runs the setup again.

The experimental [Vulkan](vulkan.md) renderer is built only when `glslc` is there (an optional
package; the game is otherwise built with OpenGL only). On Linux the setup's install command lists it
with the other missing packages, so the launcher installs it only when a required package is missing
too: install it yourself first (`glslc`; `shaderc` on Arch) to have the renderer. On Windows the
private MSYS2 does not include it.

Cancel, or closing the launcher, ends the whole tree of programs (on Windows they all run in a job,
on Linux the tree is killed). When the code generation fails because Python itself crashed (#72),
the launcher runs the setup once more with one job at a time.

A first install unpacks the release's source zip into the game folder and records it as
`tools/setup.py --update` would (`assets/update-version.txt`, and the files' hashes in
`assets/setup-state.json`), so later updates work the same as for a zip set up by hand.

## Steam Deck, Bazzite and other immutable systems

SteamOS, Bazzite, Fedora Silverblue and Kinoite, Bluefin, Aurora and the other immutable (atomic)
systems have read-only system folders, so the programs that build the game cannot be installed on
them. There the launcher builds and runs the game in a **build container** instead (distrobox, or
toolbox when that is the one installed), which shares your home folder, display, graphics card and
sound. (By hand: [Setup](setup.md#steam-deck-bazzite-and-other-immutable-systems).)

1. Open the launcher in the Desktop Mode (on the Steam Deck: Steam button, **Power**, **Switch to
   Desktop**) and click **Install** (or **Set up**) as usual.
2. The setup finds the system immutable and the build's programs missing (it exits with status 20),
   and the launcher asks **Set up a build container**. Say yes. It then, with its progress in the
   log (**Show the details**):
   - makes the container: `distrobox create -n rac1-native -i registry.fedoraproject.org/fedora-toolbox:44 --yes`
     (`--nvidia` too with an NVIDIA driver; with toolbox, `toolbox create -y -i ... rac1-native`).
     The first time it downloads the image;
   - installs the build's programs in it with Fedora's `dnf` (`sudo` asks for no password there);
   - runs the setup in it: `distrobox enter rac1-native -- python3 tools/setup.py --no-play --iso ...`.
3. **Play** starts the game in the container too, through `play-in-container.sh` in the game folder
   (`distrobox enter rac1-native -- ./build/rac1-native`, from the game folder).

The choice is kept in `launcher.txt` (a `container distrobox` or `container toolbox` line): Set up,
Update and Set up again from scratch run in the container from then on. Removing the line (and the
container: `distrobox rm rac1-native`) goes back to the system's own programs. Cancel ends the
setup's programs in the container too.

When neither distrobox nor toolbox is installed, the launcher says so. SteamOS 3.5 and newer and
Bazzite come with distrobox; on other systems install it ([distrobox.it](https://distrobox.it)).

**Gaming Mode**: once the game is set up, add it to Steam as a non-Steam game. In Desktop Mode, in
Steam: **Games > Add a Non-Steam Game to My Library**, tick **Ratchet & Clank (rac1-native, play)**
(the launcher's applications-menu entry for `play-in-container.sh`; if it is not listed, **Browse...**
to `play-in-container.sh` in the game folder, by default `~/.local/share/rac1-native/game`), then
**Add Selected Programs**. In Gaming Mode it is then in the library. The launcher itself needs the
Desktop Mode (its window and its file picker).

This route has not been tried on a Steam Deck or on Bazzite yet (GitHub issue #108): reports are welcome.

## Building it

The launcher's code is in `tools/launcher/`: what it does in `core/` (shared), the Windows window in
`windows/` (WinForms), the Linux one in `linux/` ([Avalonia](https://avaloniaui.net)).

```bash
# Windows: rac1-launcher.exe (any .NET SDK 6 or newer; the result needs only .NET Framework 4.8)
dotnet build tools/launcher/windows/Launcher.csproj -c Release -p:LauncherVersion=0.5.0 -o out
# Linux: one self-contained file, rac1-launcher (.NET SDK 8 or newer, on Linux or Windows)
dotnet publish tools/launcher/linux/Launcher.Linux.csproj -c Release -p:LauncherVersion=0.5.0 -o out
```

The [launcher workflow](../.github/workflows/launcher.yml) builds both at each push and attaches them
to each published release, with the release's version (`project(... VERSION)` in `CMakeLists.txt`).
A launcher built without a version (0.0.0) never updates itself.
