#!/usr/bin/env python3
"""Movie subtitles of our own (an enhancement; docs/movie-subtitles.md): the movies' speech turned into subtitle files
with Whisper, on the player's own machine, from the player's own disc. The game shows them over the movies when
Options > Language > Movie Subtitles is on (src/game/enhance/movie_subtitles.cpp); the original never shows any.

For each movie (assets/disc/toc/row_XXXX.pss, the extracted disc) and each language asked, the movie's sound track in
that language (0 English, 2 French, 3 German, 4 Spanish, 5 Italian: `rac1-native --setup --movie-audio` decodes it to a
16 kHz WAV) goes to speech to text, and its timed segments become one SubRip file:

    assets/movie-subtitles/row_XXXX.<lang>.srt      (UTF-8; one file per movie and language)

The files are the player's to edit (the text, the timings, the line breaks). The tool keeps the SHA-256 of every file it
wrote in assets/movie-subtitles/generated.txt: a file whose bytes differ from it was edited and is never written again
(not even with --force; --force-edited does, keeping the edited one as .srt.bak). A movie already done is skipped, so
the tool can be stopped and run again.

Back ends (only when asked: the setup installs nothing for this unless the player opts in):
  - local (the default): faster-whisper (CTranslate2) in its own virtual environment, assets/stt/venv, made by --install;
    the models are downloaded into assets/stt/models the first time. CPU by default (int8), CUDA when there is one.
  - --stt-url URL: an OpenAI-compatible server (POST URL/audio/transcriptions, response_format verbose_json), e.g. a
    local Whisper server; nothing is installed then.

Usage (from the repository's root; ./setup.sh --movie-subtitles runs it for you):
    python3 tools/movie_subtitles.py --install                 # once: the virtual environment and faster-whisper
    python3 tools/movie_subtitles.py                           # English and Italian, model large-v3-turbo
    python3 tools/movie_subtitles.py --languages it --model small --force
    python3 tools/movie_subtitles.py --stt-url http://127.0.0.1:8000/v1
    python3 tools/movie_subtitles.py --list                    # what is there
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WINDOWS = os.name == "nt"
ASSETS = ROOT / "assets"
DISC_TOC = ASSETS / "disc" / "toc"
OUT_DIR = ASSETS / "movie-subtitles"
MANIFEST = OUT_DIR / "generated.txt"
STT_DIR = ASSETS / "stt"
VENV = STT_DIR / "venv"
MODELS = STT_DIR / "models"
EXE = ROOT / "build" / ("rac1-native.exe" if WINDOWS else "rac1-native")

# The languages: their sound track in the movies of the North American and PAL discs (the game's language codes 0, 2..5
# less one, as the PSS files number them; src/game/enhance/voice_language.h)
TRACKS = {"en": 0, "fr": 2, "de": 3, "es": 4, "it": 5}
DEFAULT_LANGUAGES = "en,it"
DEFAULT_MODEL = "large-v3-turbo"  # (measured: docs/movie-subtitles.md; small takes half the time and is worse in Italian)
FASTER_WHISPER = "faster-whisper==1.2.1"  # (what --install puts in the virtual environment)

# Proper nouns the speech to text would not know (Whisper's initial prompt): the names only, no game text
NAMES = ("Ratchet, Clank, Captain Qwark, Chairman Drek, Blarg, Gadgetron, Veldin, Novalis, Aridia, Kerwan, Eudora, "
         "Rilgar, Blackwater City, Umbris, Batalia, Gaspar, Orxon, Pokitaru, Hoven, Gemlik, Oltanis, Quartu, Kalebo, "
         "Big Al, Skid McMarx, Helga, Bolts.")

# The cues: as many characters as two lines of the game's subtitle box hold (432 pixels of the small font, about
# 45 characters a line), at most this long on screen, at least this long
MAX_CHARS = 90
MAX_SECONDS = 7.0
MIN_SECONDS = 1.0

# What Whisper writes over silence or music (dropped when a whole segment is one of them; lower case, no punctuation)
HALLUCINATIONS = {
    "thank you", "thanks for watching", "thank you for watching", "you", "bye", "subtitles by the amara org community",
    "sottotitoli creati dalla comunità amaraorg", "sottotitoli a cura di qtss", "grazie a tutti", "grazie",
    "sottotitoli e revisione a cura di qtss", "untertitel der amaraorg community", "untertitel im auftrag des zdf 2017",
    "sous-titres réalisés par la communauté d'amaraorg", "subtítulos realizados por la comunidad de amaraorg",
    "...", "music", "musica",
}


def say(msg):
    print(msg, flush=True)


def fail(msg):
    print("Error: " + msg, file=sys.stderr, flush=True)
    sys.exit(1)


def venv_python():
    return VENV / ("Scripts/python.exe" if WINDOWS else "bin/python")


def have_faster_whisper():
    try:
        import faster_whisper  # noqa: F401
        return True
    except ImportError:
        return False


def install():
    """--install: assets/stt/venv with faster-whisper (and its CTranslate2), nothing outside assets/stt/."""
    if not venv_python().exists():
        say(f"Making the virtual environment {VENV.relative_to(ROOT)}{os.sep} ...")
        STT_DIR.mkdir(parents=True, exist_ok=True)
        r = subprocess.run([sys.executable, "-m", "venv", str(VENV)])
        if r.returncode:
            fail("python3 -m venv failed (on Debian / Ubuntu: sudo apt install python3-venv)")
    say(f"Installing {FASTER_WHISPER} into it (about 500 MB) ...")
    r = subprocess.run([str(venv_python()), "-m", "pip", "install", "--disable-pip-version-check", "-q", FASTER_WHISPER])
    if r.returncode:
        fail("pip could not install faster-whisper (no network? the output above says why)")
    say("Speech to text installed. The model is downloaded at the first run into "
        f"{MODELS.relative_to(ROOT)}{os.sep} (large-v3-turbo: about 1.6 GB).")


# ---- the files and their record --------------------------------------------------------------------------------

def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_manifest():
    m = {}
    if MANIFEST.exists():
        for line in MANIFEST.read_text(encoding="utf-8", errors="replace").splitlines():
            parts = line.split()
            if len(parts) >= 2 and not line.startswith("#"):
                m[parts[0]] = parts[1:]
    return m


def save_manifest(m):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    lines = ["# The movie subtitles tools/movie_subtitles.py wrote: file, SHA-256 when written, how. A file whose bytes",
             "# differ was edited by hand and is not written again (--force-edited does, keeping it as .srt.bak)."]
    lines += [f"{k} {' '.join(v)}" for k, v in sorted(m.items())]
    MANIFEST.write_text("\n".join(lines) + "\n", encoding="utf-8")


def file_state(path, manifest):
    """'missing', 'generated' (as written), 'edited' (changed by hand, or not ours)."""
    if not path.exists():
        return "missing"
    rec = manifest.get(path.name)
    return "generated" if rec and rec[0] == sha256(path) else "edited"


def movies():
    if not DISC_TOC.is_dir():
        fail(f"{DISC_TOC.relative_to(ROOT)}{os.sep} is missing: run the setup first (./setup.sh)")
    return sorted(DISC_TOC.glob("row_*.pss"))


def movie_tracks(pss):
    r = subprocess.run([str(EXE), "--setup", "--movie-tracks", str(pss)], capture_output=True, text=True)
    if r.returncode or not r.stdout.startswith("tracks"):
        fail(f"{EXE.relative_to(ROOT)} --setup --movie-tracks failed on {pss.name}: {r.stderr.strip()} (build the game first)")
    return [int(t) for t in r.stdout.split()[1:]]


def movie_wav(pss, track, wav):
    r = subprocess.run([str(EXE), "--setup", "--movie-audio", str(pss), "--track", str(track), "--wav", str(wav)],
                       capture_output=True, text=True)
    if r.returncode:
        fail(f"the sound of {pss.name} could not be decoded: {r.stderr.strip()}")
    m = re.search(r"([0-9.]+) s", r.stdout)
    return float(m.group(1)) if m else 0.0


# ---- speech to text ---------------------------------------------------------------------------------------------

class Local:
    """faster-whisper in this process (the tool runs in assets/stt/venv's Python for it)."""

    def __init__(self, model, device, threads):
        from faster_whisper import WhisperModel
        MODELS.mkdir(parents=True, exist_ok=True)
        if device == "auto":
            try:
                import ctranslate2
                device = "cuda" if ctranslate2.get_cuda_device_count() > 0 else "cpu"
            except Exception:
                device = "cpu"
        self.name = f"faster-whisper:{model}:{device}"
        say(f"Loading the model {model} on {device} (the first time it is downloaded) ...")
        self.model = WhisperModel(model, device=device, compute_type="float16" if device == "cuda" else "int8",
                                  cpu_threads=threads, download_root=str(MODELS))

    def transcribe(self, wav, lang):
        # the samples handed over as they are (16 kHz mono, what --movie-audio writes): faster-whisper's own decoder (PyAV)
        # is not needed, nor its version's quirks
        import numpy
        import wave
        with wave.open(str(wav), "rb") as w:
            if w.getframerate() != 16000 or w.getnchannels() != 1 or w.getsampwidth() != 2:
                raise RuntimeError(f"{wav.name}: not 16 kHz mono 16-bit")
            audio = numpy.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(numpy.float32) / 32768.0
        segs, _ = self.model.transcribe(audio, language=lang, beam_size=5, vad_filter=True, word_timestamps=True,
                                        condition_on_previous_text=False, initial_prompt=NAMES)
        out = []
        for s in segs:
            words = [(w.start, w.end, w.word) for w in (s.words or [])]
            out.append({"start": s.start, "end": s.end, "text": s.text, "words": words,
                        "no_speech": s.no_speech_prob, "logprob": s.avg_logprob})
        return out


class Remote:
    """An OpenAI-compatible server: POST <url>/audio/transcriptions (multipart), response_format verbose_json."""

    def __init__(self, url, model, timeout):
        self.url = url.rstrip("/")
        if not self.url.endswith("/audio/transcriptions"):
            self.url += "/audio/transcriptions"
        self.model = model
        self.timeout = timeout
        self.name = f"url:{model}"

    def transcribe(self, wav, lang):
        boundary = uuid.uuid4().hex
        fields = [("model", self.model), ("language", lang), ("response_format", "verbose_json"),
                  ("timestamp_granularities[]", "segment"), ("timestamp_granularities[]", "word"),
                  ("prompt", NAMES), ("temperature", "0")]
        body = b""
        for k, v in fields:
            body += (f"--{boundary}\r\nContent-Disposition: form-data; name=\"{k}\"\r\n\r\n{v}\r\n").encode()
        body += (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"{wav.name}\"\r\n"
                 "Content-Type: audio/wav\r\n\r\n").encode() + wav.read_bytes() + f"\r\n--{boundary}--\r\n".encode()
        req = urllib.request.Request(self.url, data=body, method="POST",
                                     headers={"Content-Type": f"multipart/form-data; boundary={boundary}"})
        if os.environ.get("OPENAI_API_KEY"):
            req.add_header("Authorization", "Bearer " + os.environ["OPENAI_API_KEY"])
        last = None
        for attempt in range(3):  # (a server that drops a request now and then: tried again)
            try:
                with urllib.request.urlopen(req, timeout=self.timeout) as r:
                    data = json.loads(r.read().decode("utf-8"))
                break
            except (urllib.error.URLError, TimeoutError, ConnectionError, json.JSONDecodeError) as e:
                last = e
                time.sleep(2 + attempt * 3)
        else:
            raise RuntimeError(f"{self.url}: {last}")
        segs = data.get("segments")
        if segs is None:  # (a server without segments: the whole text as one, timed by the file)
            segs = [{"start": 0.0, "end": float(data.get("duration") or 0.0), "text": data.get("text", "")}]
        words = [(float(w["start"]), float(w["end"]), " " + w["word"].strip()) for w in data.get("words") or []]
        out = []
        for s in segs:
            st, en = float(s.get("start", 0.0)), float(s.get("end", 0.0))
            ws = [w for w in words if st - 0.01 <= w[0] < en + 0.01]
            out.append({"start": st, "end": en, "text": s.get("text", ""), "words": ws,
                        "no_speech": float(s.get("no_speech_prob", 0.0)), "logprob": float(s.get("avg_logprob", 0.0))})
        return out


# ---- the cues ---------------------------------------------------------------------------------------------------

def clean(text, lang):
    t = " ".join(text.replace("…", "...").split())
    t = t.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    t = t.replace("–", "-").replace("—", "-").replace("«", '"').replace("»", '"')
    t = re.sub(r"(\w) -(\w)", r"\1-\2", t)  # ("snot -nosed": Whisper's word pieces)
    if lang in ("it", "fr"):
        t = re.sub(r"(\w) '(\w)", r"\1'\2", t)  # ("l 'impianto", "d 'accensione": the elision written together)
    if lang == "it":
        t = re.sub(r"(^|(?<=[\s\"(]))E' ", "È ", t)  # ("E' ora": the capital E with its grave accent)
    return t.strip()


def hallucination(text):
    key = re.sub(r"[^\w\s']", "", text.lower()).strip()
    key = " ".join(key.split())
    return key in HALLUCINATIONS or not key


def split_cue(start, end, text, words):
    """One segment as cues of at most MAX_CHARS characters and MAX_SECONDS: split at the words (their own times when
    the back end gave them, else by the characters), the cut preferred after a sentence's or a clause's end."""
    if len(text) <= MAX_CHARS and end - start <= MAX_SECONDS:
        return [(start, end, text)]
    toks = text.split()
    if words and len(words) >= 2:
        wt = [(float(a), float(b), w.strip()) for a, b, w in words if w.strip()]
    else:  # (no word times: each word gets the segment's time in proportion to its characters)
        total = sum(len(w) + 1 for w in toks) or 1
        t, wt = start, []
        for w in toks:
            d = (end - start) * (len(w) + 1) / total
            wt.append((t, t + d, w))
            t += d
    cues, cur = [], []
    for i, w in enumerate(wt):
        cur.append(w)
        line = " ".join(x[2] for x in cur)
        nxt = wt[i + 1][2] if i + 1 < len(wt) else None
        if nxt is None:
            break
        over = len(line) + 1 + len(nxt) > MAX_CHARS or wt[i + 1][1] - cur[0][0] > MAX_SECONDS
        stop = line.endswith((".", "!", "?")) and len(line) >= MAX_CHARS // 3
        clause = line.endswith((",", ";", ":")) and len(line) >= MAX_CHARS * 2 // 3
        if over or stop or clause:
            cues.append((cur[0][0], cur[-1][1], line))
            cur = []
    if cur:
        cues.append((cur[0][0], cur[-1][1], " ".join(x[2] for x in cur)))
    return cues


def make_cues(segments, lang):
    cues = []
    for s in segments:
        text = clean(s["text"], lang)
        if hallucination(text):
            continue
        if s.get("no_speech", 0.0) > 0.6 and s.get("logprob", 0.0) < -1.0:
            continue
        cues += [(a, b, clean(t, lang)) for a, b, t in split_cue(float(s["start"]), float(s["end"]), text, s.get("words") or [])]
    out = []
    for i, (a, b, t) in enumerate(cues):  # (each at least MIN_SECONDS where the next one leaves room)
        nxt = cues[i + 1][0] if i + 1 < len(cues) else b + MIN_SECONDS
        if b - a < MIN_SECONDS:
            b = max(b, min(a + MIN_SECONDS, nxt))
        if out and a < out[-1][1]:
            a = out[-1][1]
        if b > a:
            out.append((a, b, t))
    return out


def srt_time(t):
    ms = int(round(t * 1000))
    return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"


def write_srt(path, cues):
    text = "".join(f"{i}\n{srt_time(a)} --> {srt_time(b)}\n{t}\n\n" for i, (a, b, t) in enumerate(cues, 1))
    tmp = path.with_suffix(".srt.part")
    tmp.write_text(text, encoding="utf-8", newline="\n")
    os.replace(tmp, path)


# ---- the run ----------------------------------------------------------------------------------------------------

def list_files(languages):
    manifest = load_manifest()
    for pss in movies():
        states = []
        for lang in languages:
            st = file_state(OUT_DIR / f"{pss.stem}.{lang}.srt", manifest)
            states.append(f"{lang} {st}")
        say(f"{pss.stem}: " + ", ".join(states))


def main():
    ap = argparse.ArgumentParser(description="Movie subtitles of our own: the movies' speech to subtitle files "
                                             "(assets/movie-subtitles/), with Whisper. docs/movie-subtitles.md")
    ap.add_argument("--languages", default=DEFAULT_LANGUAGES,
                    help=f"comma-separated: en, fr, de, es, it, or all (default {DEFAULT_LANGUAGES})")
    ap.add_argument("--model", default=None,
                    help=f"the Whisper model: tiny, base, small, medium, large-v3, large-v3-turbo... (default {DEFAULT_MODEL}; "
                         "with --stt-url, the name the server wants, default whisper-1)")
    ap.add_argument("--device", default="auto", choices=("auto", "cpu", "cuda"), help="local back end: where it runs")
    ap.add_argument("--threads", type=int, default=0, help="local back end: CPU threads (0: its default)")
    ap.add_argument("--stt-url", default=None, help="an OpenAI-compatible server's base URL (e.g. http://127.0.0.1:8000/v1) "
                                                    "instead of the local back end")
    ap.add_argument("--timeout", type=float, default=600.0, help="--stt-url: seconds to wait for one movie")
    ap.add_argument("--movies", default=None, help="only these movies (comma-separated, e.g. row_1810,row_1938)")
    ap.add_argument("--force", action="store_true", help="write again the files written before (not the edited ones)")
    ap.add_argument("--force-edited", action="store_true", help="write again the edited files too (each kept as .srt.bak)")
    ap.add_argument("--install", action="store_true", help="install the local back end (assets/stt/venv), then stop")
    ap.add_argument("--list", action="store_true", help="list the movies and their files' state, then stop")
    ap.add_argument("--check-installed", action="store_true",
                    help="exit status 0 when the local back end is installed (assets/stt/venv), 1 when not")
    ap.add_argument("--out", default=None, metavar="DIR", help="write the files there instead (testing; the game reads "
                                                               "assets/movie-subtitles/, or RAC1_MOVIE_SUBTITLES_DIR)")
    ap.add_argument("--keep-wav", default=None, metavar="DIR", help="keep the WAV files in DIR (testing)")
    ap.add_argument("--json", default=None, metavar="FILE", help="also write the timings and back end per file (testing)")
    args = ap.parse_args()

    if args.out:
        global OUT_DIR, MANIFEST
        OUT_DIR = Path(args.out).resolve()
        MANIFEST = OUT_DIR / "generated.txt"
    languages = list(TRACKS) if args.languages == "all" else [x.strip() for x in args.languages.split(",") if x.strip()]
    bad = [x for x in languages if x not in TRACKS]
    if bad:
        fail(f"unknown language {', '.join(bad)} (en, fr, de, es, it)")
    if args.install:
        install()
        return
    if args.check_installed:
        ok = venv_python().exists() and subprocess.run([str(venv_python()), "-c", "import faster_whisper"],
                                                       capture_output=True).returncode == 0
        sys.exit(0 if ok else 1)
    if args.list:
        list_files(languages)
        return
    if not args.stt_url and not have_faster_whisper():
        if venv_python().exists() and Path(sys.prefix).resolve() != VENV.resolve():  # (not in it yet: run in it)
            os.execv(str(venv_python()), [str(venv_python()), str(Path(__file__).resolve())] + sys.argv[1:])
        fail("the local speech to text is not installed: run python3 tools/movie_subtitles.py --install "
             "(or ./setup.sh --movie-subtitles), or give --stt-url")
    if not EXE.exists():
        fail(f"{EXE.relative_to(ROOT)} is missing: run the setup first (./setup.sh)")

    manifest = load_manifest()
    todo = []
    wanted = set(args.movies.split(",")) if args.movies else None
    for pss in movies():
        if wanted is not None and pss.stem not in wanted:
            continue
        tracks = None
        for lang in languages:
            out = OUT_DIR / f"{pss.stem}.{lang}.srt"
            st = file_state(out, manifest)
            if st == "edited" and not args.force_edited:
                say(f"{out.name}: edited by hand, kept (--force-edited writes it again)")
                continue
            if st == "generated" and not (args.force or args.force_edited):
                continue
            if tracks is None:
                tracks = movie_tracks(pss)
            if TRACKS[lang] not in tracks:
                continue  # (the movie has no sound in that language: no file, the game shows none)
            todo.append((pss, lang, out, st))
    if not todo:
        say("Movie subtitles: nothing to do (every movie done; --list shows them).")
        return

    model = args.model or ("whisper-1" if args.stt_url else DEFAULT_MODEL)
    stt = Remote(args.stt_url, model, args.timeout) if args.stt_url else Local(model, args.device, args.threads)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="movie-subs-", dir=str(STT_DIR if STT_DIR.exists() else OUT_DIR)))
    report = []
    t_all = time.time()
    try:
        for i, (pss, lang, out, st) in enumerate(todo, 1):
            wav = (Path(args.keep_wav) if args.keep_wav else work) / f"{pss.stem}.{lang}.wav"
            wav.parent.mkdir(parents=True, exist_ok=True)
            seconds = movie_wav(pss, TRACKS[lang], wav)
            t0 = time.time()
            try:
                segs = stt.transcribe(wav, lang)
            except Exception as e:  # (one movie that fails: said, the others go on)
                say(f"[{i}/{len(todo)}] {out.name}: speech to text failed ({e}); not written")
                continue
            dt = time.time() - t0
            cues = make_cues(segs, lang)
            if st == "edited":
                shutil.copy2(out, out.with_suffix(".srt.bak"))
            write_srt(out, cues)
            manifest[out.name] = [sha256(out), stt.name.replace(" ", "_")]
            save_manifest(manifest)
            if not args.keep_wav:
                wav.unlink(missing_ok=True)
            say(f"[{i}/{len(todo)}] {out.name}: {len(cues)} lines, {seconds:.0f} s of sound in {dt:.0f} s")
            report.append({"file": out.name, "seconds": seconds, "stt_seconds": dt, "cues": len(cues), "backend": stt.name})
    finally:
        shutil.rmtree(work, ignore_errors=True)
        if args.json:
            Path(args.json).write_text(json.dumps(report, indent=1), encoding="utf-8")
    say(f"Movie subtitles written into {OUT_DIR} in {time.time() - t_all:.0f} s.")


if __name__ == "__main__":
    main()
