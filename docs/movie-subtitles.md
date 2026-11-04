# Movie subtitles

The original never shows text over its movies, on any disc: its movie player draws nothing over the picture and the
movie files (PSS) carry no subtitle stream (only the Japanese disc has captions, burned into the pictures of eleven
movies). The scenes drawn by the game have subtitles (the game's Options > Subtitles); the movies do not.

This port can show subtitles of its own over the movies. Nothing of them is in the repository: their text is made on
the player's machine, from the player's own disc, by speech to text (OpenAI's Whisper). It is an enhancement, off by
default: **Options > Language > Movie Subtitles** (main menu and pause menu). Off, the movies are the original's, pixel
for pixel.

## Making them

```bash
./setup.sh --movie-subtitles            # English and Italian
./setup.sh --movie-subtitles en,it,fr   # the languages given (en, fr, de, es, it)
```

The launcher has the same: Settings > **Make movie subtitles**.

The first time, the setup installs the speech to text into `assets/stt/`: a Python virtual environment
(`assets/stt/venv`, about 500 MB: [faster-whisper](https://github.com/SYSTRAN/faster-whisper) and CTranslate2) and,
at the first run, the Whisper model (`assets/stt/models`, about 1.6 GB for `large-v3-turbo`). Nothing is installed
outside `assets/stt/`; deleting that folder removes it. It runs on the CPU (int8), or on an NVIDIA GPU with CUDA
when there is one.

The setup remembers the choice: the next setups (after an update, say) make only what is missing, in a few seconds when
everything is there. `./setup.sh --no-movie-subtitles` stops that (the files made stay).

For each movie and language, the movie's own sound track in that language goes to the speech to text: the North
American and PAL discs carry five (English, French, German, Spanish, Italian) in 66 of their 84 movies; the other 18
(the logos, the attract movies, the credits) have one track, taken as English. A movie without a track in a language
gets no file for it. The subtitles follow the **text** language (Options > Language > Text), as the game's own
subtitles do: with the English voice and the Italian text, the Italian subtitles are the Italian dub's words.

### Another speech to text server

Any OpenAI-compatible server (`POST <url>/audio/transcriptions`, `response_format=verbose_json`) can do the work
instead, with nothing installed:

```bash
./setup.sh --movie-subtitles --stt-url http://127.0.0.1:8000/v1 --stt-model whisper-large-v3-turbo
```

A request that fails is tried again twice; a movie that still fails is skipped (the next run makes it).

### The tool on its own

`tools/movie_subtitles.py` is what the setup runs:

```bash
python3 tools/movie_subtitles.py --install                       # the local speech to text, once
python3 tools/movie_subtitles.py                                 # English and Italian
python3 tools/movie_subtitles.py --languages it --movies row_1978 --force
python3 tools/movie_subtitles.py --model small                   # faster, worse (below)
python3 tools/movie_subtitles.py --list                          # each movie's files: missing, generated, edited
```

It decodes each movie's sound with the game itself (`build/rac1-native --setup --movie-audio`, the same decoder the
game plays them with, to a 16 kHz mono WAV), transcribes it (Whisper's voice detection on, so music and silence give no
text; Whisper's usual phantom lines over silence, "Thanks for watching" and the like, dropped), and cuts its timed
segments into lines of at most about 90 characters (two lines of the subtitle box) and 7 seconds, at the words' own
times, after a sentence's or a clause's end where it can.

## The files

```
assets/movie-subtitles/row_XXXX.<lang>.srt
```

One [SubRip](https://en.wikipedia.org/wiki/SubRip) file per movie and language, UTF-8: `row_XXXX` is the movie's file
in the extracted disc (`assets/disc/toc/row_XXXX.pss`), `<lang>` `en`, `fr`, `de`, `es` or `it`. They are plain text,
for the player to fix or rewrite:

```
12
00:00:03,400 --> 00:00:07,720
The text of the line,
a second line where the file breaks it
```

- **Editing**: the text, the times and the line breaks can all be changed; lines can be added or removed (their numbers
  are not read). `assets/movie-subtitles/generated.txt` holds the SHA-256 of every file the tool wrote: a file whose
  bytes differ was edited, and no later run writes it again, not even `--force`. `--force-edited` does (keeping the
  edited one as `.srt.bak`).
- **The game reads** the file of the text language when a movie starts. A missing or empty file shows nothing; a file
  over 1 MB is not read; a cue whose timing line is missing or wrong (`HH:MM:SS,mmm --> HH:MM:SS,mmm`, the end after the
  start) is skipped. Each is a line in the game's log (`[movie subtitles] ...`), never a crash. A file saved in the
  Windows code page instead of UTF-8 is read as Latin-1.
- **Characters**: the game's small font has the letters, the digits, the usual punctuation and the accented vowels
  (acute, grave, circumflex, diaeresis on A E I O U, both cases); other marked letters show as their plain letter
  (ñ as n, ç as c, ß as ss), and the characters the font draws as pad buttons or lacks (`# $ % @ ^ & * = [ ] { } | ~`
  and the rest) are left out (the log counts them). Curly quotes, dashes and the ellipsis become their ASCII forms.

## How they are drawn

As the game draws a scene's subtitle (`cutscene_subtitle_draw`, 0x1f9a20): the small font, the text laid out by the
game's own rules (`text_box`, 0x1fbf48: wrapped at spaces into balanced lines in a 432-pixel box, a line break of the
file kept), centred near the bottom of the 512-pixel-wide screen, grey `0x80b0b0b0`, over the dark rounded box of
alpha `0x60`. The font is read from EE RAM as the game uploads it (its glyph table and effect texture 1); the host
composes the cue into the movie's picture after the decode (the game's screen scaled onto the picture, as the frame is
onto the window). Nothing of the game's memory is written: with the subtitles on, the game's state is the same frame for
frame (`RAC1_RAMHASH`). The code is `src/game/enhance/movie_subtitles.cpp`, hooked into the movie player's stand-in
(`src/game/lib/movie_player.cpp`).

## Which model

Measured on four movies (206 s of speech in each language), on a Ryzen AI 9 HX 470 (CPU, int8, 8 threads, while a
build ran beside it). "Differs" is the share of words that differ from `large-v3-turbo`'s transcript; reading them,
`large-v3-turbo`'s were right where the others differ.

| Model | Download | English: time / sound | differs | Italian: time / sound | differs |
|---|---|---|---|---|---|
| base | 145 MB | 0.06 | 0 to 15 % | 0.06 | 13 to 32 % |
| small | 480 MB | 0.09 | 0 to 3 % | 0.11 | 9 to 25 % |
| large-v3-turbo (the default) | 1.6 GB | 0.18 | - | 0.20 | - |

`base` is not usable (in Italian it makes up words); `small` is good in English and loses whole phrases in Italian's
faster scenes; `large-v3-turbo` gets nearly everything, names included (Whisper is given the characters' and planets'
names as a hint). All 84 movies in English and the 66 with an Italian track (92 minutes of sound together) took 26
minutes with it on that CPU (its default threads), the model's download aside.

## Testing

- `RAC1_MOVIE_SUBTITLES=on` shows them also in a headless run (the setting is off there), `RAC1_LANGUAGE` picks the
  language, `RAC1_MOVIE_AT=<n>@<frame>` starts story movie n, `RAC1_MOVIE_SUBTITLES_DIR` reads the files from another
  folder (the broken-file cases).
- Off, the reference images and a movie's frames are those of the build without this; on, `RAC1_RAMHASH` is the same.
