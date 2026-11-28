"""tools/setup.py and a full disk (GitHub issue #189): the free space checked before the steps that write, a disk that
fills up during a step reported as such, and a step that fails not recorded as made. No game files needed: the disk's
free space, the sizes and the steps' records are faked. Run: python3 tests/setup/test_disk_space.py"""
import contextlib
import errno
import importlib.util
import io
import json
import shutil
import subprocess
import sys
import tempfile
from collections import namedtuple
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("rac1_setup", ROOT / "tools/setup.py")
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)

GB = 2**30
Usage = namedtuple("Usage", "total used free")
FP = "SCUS_971.99:4214095872"


def run(fn, *args, **kw):
    """(exit code or None, what it printed)"""
    out = io.StringIO()
    code = None
    with contextlib.redirect_stdout(out):
        try:
            fn(*args, **kw)
        except SystemExit as e:
            code = e.code
    return code, out.getvalue()


class Fake:
    """A game folder in a temporary directory, with the disk's free space, the folders' sizes and the steps' state
    faked (setup.py's globals and helpers patched, put back by restore)."""

    def __init__(self):
        self.dir = Path(tempfile.mkdtemp(prefix="rac1-space-"))
        self.free = 100 * GB
        self.sizes = {}  # folder name -> bytes there
        self.missing = {}  # step -> "... is missing"
        self.saved = {}
        assets = self.dir / "assets"
        self.patch(ROOT=self.dir, ASSETS_DIR=assets, DISC_DIR=assets / "disc", MRP=assets / "generated/mrp",
                   GEN_SRC=assets / "generated/src", CODE_ROOT=assets / "generated", BUILD_DIR=self.dir / "build",
                   EXE=self.dir / "build/rac1-native", STATE_FILE=assets / "setup-state.json")
        self.patch(disc_fingerprint=lambda iso: FP, sources_fingerprint=lambda patterns: "src",
                   code_step_fingerprint=lambda name, patterns, disc, state: f"{disc}:code",
                   step_missing=lambda name, iso: self.missing.get(name),
                   tree_size=lambda path: self.sizes.get(Path(path).name, 0),
                   newest_mtime=lambda paths: 0.0)
        self.layout = None
        self.saved_layout = setup.disc_extract.layout_missing
        setup.disc_extract.layout_missing = lambda d: self.layout
        self.saved_usage = shutil.disk_usage
        shutil.disk_usage = lambda p: Usage(500 * GB, 0, self.free)

    def patch(self, **kw):
        for k, v in kw.items():
            self.saved.setdefault(k, getattr(setup, k))
            setattr(setup, k, v)

    def up_to_date(self):
        """Every step recorded as made, its files there, the game built."""
        setup.save_state({"disc": f"{FP}:src", "elf": f"{FP}:code", "overlay": f"{FP}:code", "code": f"{FP}:code",
                          "moby": f"{FP}:src", "world": f"{FP}:src", "flight": f"{FP}:src"})
        setup.EXE.parent.mkdir(parents=True, exist_ok=True)
        setup.EXE.write_text("exe")
        self.sizes = {"disc": int(4.0 * GB), "src": 50 << 20, "build": int(1.4 * GB), "moby": 339 << 20,
                      "world": 298 << 20}

    def restore(self):
        for k, v in self.saved.items():
            setattr(setup, k, v)
        setup.disc_extract.layout_missing = self.saved_layout
        shutil.disk_usage = self.saved_usage
        shutil.rmtree(self.dir, ignore_errors=True)


def test(fn):
    f = Fake()
    try:
        fn(f)
        print(f"ok  {fn.__name__}")
    finally:
        f.restore()


@test
def fresh_install_short(f):
    f.free = 3 * GB
    code, out = run(setup.check_space, "game.iso", set())
    assert code == 1, out
    assert "Not enough free disk space for the setup" in out and "3.0 GB is free" in out, out
    # 4.1 (disc) + 0.06 (code) + 2.0 (build) + 0.4 (moby) + 0.35 (world) + 0.5 (margin)
    assert "about 7.5 GB" in out and "the disc's files" in out and "the build" in out, out
    assert str(f.dir) in out, out


@test
def fresh_install_enough(f):
    f.free = 8 * GB
    code, out = run(setup.check_space, "game.iso", set())
    assert code is None and out == "", out


@test
def rerun_up_to_date_needs_nothing(f):
    f.up_to_date()
    f.free = 1 << 20  # (1 MB: a full disk, and nothing to write)
    assert setup.space_parts("game.iso", set()) == []
    code, out = run(setup.check_space, "game.iso", set())
    assert code is None, out


@test
def rerun_update_rewrites_in_place(f):
    # (an update that changed the data steps' sources: they are made again over the files that are there)
    f.up_to_date()
    setup.save_state(dict(setup.load_state(), moby="old", world="old"))
    f.sizes.update(moby=int(0.4 * GB), world=int(0.35 * GB))
    assert setup.space_parts("game.iso", set()) == [], setup.space_parts("game.iso", set())
    f.free = 100 << 20
    assert run(setup.check_space, "game.iso", set())[0] is None


@test
def partial_disc_counts_what_is_there(f):
    f.up_to_date()
    f.layout = "assets/disc/files is missing"  # (an extraction cut short)
    f.sizes["disc"] = 3 * GB
    parts = setup.space_parts("game.iso", set())
    assert [(p[0], p[2]) for p in parts] == [("the disc's files", int(4.1 * GB) - 3 * GB)], parts
    f.free = int(1.5 * GB)
    assert run(setup.check_space, "game.iso", set())[0] == 1
    f.free = 2 * GB
    assert run(setup.check_space, "game.iso", set())[0] is None


@test
def build_folder_there(f):
    f.up_to_date()
    setup.newest_mtime = lambda paths: setup.EXE.stat().st_mtime + 10  # (sources newer than the program)
    assert [(p[0], p[2]) for p in setup.space_parts("game.iso", set())] == [("the build", setup.SPACE_REBUILD)]
    assert setup.space_parts(None, set(), build=False) == []


@test
def disk_full_errors(f):
    assert setup.is_disk_full(OSError(errno.ENOSPC, "No space left on device"))
    win = OSError(22, "There is not enough space on the disk")
    win.winerror = 112
    assert setup.is_disk_full(win)
    assert not setup.is_disk_full(OSError(errno.EACCES, "Permission denied"))
    assert not setup.is_disk_full(ValueError("x"))


@test
def disk_full_while_extracting(f):
    def full(iso, out, show):
        raise OSError(errno.ENOSPC, "No space left on device", str(out / "files/x"))
    saved = setup.disc_extract.extract
    setup.disc_extract.extract = full
    try:
        f.free = 0
        code, out = run(setup.extract_disc, Path("game.iso"), set())
    finally:
        setup.disc_extract.extract = saved
    assert code == 1 and "The disk is full: extracting the disc stopped for lack of space" in out, out
    assert "0.0 GB free" in out and "about 4.6 GB free there" in out, out
    assert "disc" not in setup.load_state()  # (not recorded as extracted)


@test
def child_failure_on_a_full_disk(f):
    f.free = 100 << 20
    code, out = run(setup.fail_if_disk_full, setup.MRP, "generating the moby models and textures")
    assert code == 1 and "The disk is full: generating the moby models" in out, out
    f.free = 50 * GB
    assert run(setup.fail_if_disk_full, setup.MRP, "the build")[0] is None
    code, out = run(setup.fail_if_disk_full, setup.BUILD_DIR, "the build", "ld: final link failed: No space left on device")
    assert code == 1 and "The disk is full: the build stopped" in out, out


@test
def failed_data_step_not_recorded(f):
    f.up_to_date()
    f.missing["moby"] = "assets/generated/mrp/moby/level3 has no models"  # (recorded as made, part of it gone)
    f.free = 10 << 20
    saved_run, saved_mods = subprocess.run, setup.level_mods
    subprocess.run = lambda *a, **kw: subprocess.CompletedProcess(a, 1)
    try:
        code, out = run(setup.generate, Path("game.iso"), set())
    finally:
        subprocess.run = saved_run
    assert code == 1 and "The disk is full: generating the moby models and textures" in out, out
    state = setup.load_state()
    assert "moby" not in state and state.get("world") == f"{FP}:src", state  # (the next setup makes it again)
    # the same step made again with room: recorded again
    f.free = 50 * GB
    f.missing.clear()
    subprocess.run = lambda *a, **kw: subprocess.CompletedProcess(a, 0)
    setup.level_mods = lambda: None
    try:
        code, out = run(setup.generate, Path("game.iso"), set())
    finally:
        subprocess.run, setup.level_mods = saved_run, saved_mods
    assert code is None and setup.load_state().get("moby") == f"{FP}:src", out


@test
def finished_on_a_full_disk_not_recorded(f):
    # (the step said it finished, but its disk is all but full: a file's last bytes may be missing)
    f.up_to_date()
    setup.save_state(dict(setup.load_state(), moby="old"))
    f.free = 10 << 20
    saved_run = subprocess.run
    subprocess.run = lambda *a, **kw: subprocess.CompletedProcess(a, 0)
    try:
        code, out = run(setup.generate, Path("game.iso"), set())
    finally:
        subprocess.run = saved_run
    assert code == 1 and "The disk is full: generating the moby models" in out, out
    assert "moby" not in setup.load_state()


@test
def state_kept_whole_on_a_full_disk(f):
    setup.save_state({"disc": "kept"})
    saved = Path.write_text

    def full(self, *a, **kw):
        raise OSError(errno.ENOSPC, "No space left on device")
    Path.write_text = full
    try:
        try:
            setup.save_state({"disc": "new"})
            raise AssertionError("no error")
        except OSError as e:
            assert setup.is_disk_full(e)
    finally:
        Path.write_text = saved
    assert json.loads(setup.STATE_FILE.read_text()) == {"disc": "kept"}
    assert not setup.STATE_FILE.with_name("setup-state.json.tmp").exists()


print("all passed")
sys.exit(0)
