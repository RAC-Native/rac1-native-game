# Movie subtitles

The original never shows text over its movies, on any disc: its movie player draws nothing over the picture and the
movie files (PSS) carry no subtitle stream (only the Japanese disc has captions, burned into the pictures of eleven
movies). The scenes drawn by the game have subtitles (the game's Options > Subtitles); the movies do not.

This port can show subtitles over the movies, from subtitle files on the player's machine. Nothing of them is in the
repository or in the game's releases (they are the movies' dialogue): the setup can make them from the player's own disc
with speech to text, or they come from a subtitles pack, or the player writes them. It is an enhancement, off by
default: **Options > Language > Movie Subtitles** (main menu and pause menu). Off, the movies are the original's, pixel
for pixel.

## Making them at the setup

Optional, off unless asked for:

```bash
./setup.sh --movie-subtitles            # every language the disc's movies have
./setup.sh --movie-subtitles en,it      # the languages given (en, fr, de, es, it)
./setup.sh --no-movie-subtitles         # no longer (the files made stay)
```

An interactive setup asks once (yes or no, then the languages); the launcher has the same in Settings > **Movie
subtitles** (a box for each language) and **Make movie subtitles**. The setup remembers the choice
(`assets/setup-state.json`): the next setups (after an update, say) make only what is missing, in a few seconds when
everything is there.

The first time, the setup installs the speech to text into `assets/stt/`: a Python virtual environment (`assets/stt/venv`,
about 470 MB: [faster-whisper](https://github.com/SYSTRAN/faster-whisper) and CTranslate2) and, at the first run, the
Whisper model (`assets/stt/models`, about 1.6 GB for `large-v3-turbo`). Nothing is installed outside `assets/stt/`;
deleting that folder removes it. It runs on the CPU (int8), about half an hour for English and Italian on a fast one, or
on an NVIDIA GPU with CUDA when there is one.

For each movie and language, the movie's own sound track in that language goes to the speech to text: the North
American and PAL discs carry five (English, French, German, Spanish, Italian) in 66 of their 84 movies; the other 18
(the logos, the attract movies, the credits) have one track, taken as English. A movie without a track in a language
gets no file for it.

Speech to text gets most lines right, not all: a name, a word said fast or under music, a line split in an odd place.
The files are plain text, to be corrected by hand ([below](#the-files)); a corrected file is never written again.

### Another speech to text server

Any OpenAI-compatible server (`POST <url>/audio/transcriptions`, `response_format=verbose_json`) can do the work
instead, with nothing installed:

```bash
./setup.sh --movie-subtitles --stt-url http://127.0.0.1:8000/v1 --stt-model whisper-large-v3-turbo
```

A request that fails is tried again twice; a movie that still fails is skipped (the next run makes it).

## A subtitles pack

Subtitle files made elsewhere (corrected by someone, or in another language) are used the same way: put the
`row_XXXX.<lang>.srt` files into the game folder's `assets/movie-subtitles/` (make the folder if it is not there), or,
with the launcher, Settings > **Install subtitle pack** and choose the .zip: its subtitle files go into that folder,
anything else in the zip is left out, and a file already there that the pack changes is kept beside it as `.srt.bak`.
The setup never writes over a file it did not make itself.

Then, in the game: **Options > Language > Movie Subtitles: On**. The subtitles follow the **text** language
(Options > Language > Text), as the game's own subtitles do: with the English voice and the Italian text, the Italian
subtitles are the Italian dub's words. A language without files shows none.

## The files

```
assets/movie-subtitles/row_XXXX.<lang>.srt
```

One [SubRip](https://en.wikipedia.org/wiki/SubRip) file per movie and language, UTF-8: `row_XXXX` is the movie's file
in the extracted disc (`assets/disc/toc/row_XXXX.pss`), `<lang>` `en`, `fr`, `de`, `es` or `it`. They are plain text,
for the player to fix or rewrite with any text editor:

```
12
00:00:03,400 --> 00:00:07,720
The text of the line,
a second line where the file breaks it
```

- **Editing**: the text, the times and the line breaks can all be changed; lines can be added or removed (their numbers
  are not read). A line shows from its first time to its second; two lines of the box hold about 90 characters.
  `assets/movie-subtitles/generated.txt` holds the SHA-256 of every file the setup wrote: a file whose bytes differ was
  edited (or is not the setup's), and no later run writes it again.
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

## The tool

`tools/movie_subtitles.py` is what the setup runs; on its own it also makes a draft to correct (a subtitles pack's
first step):

```bash
python3 tools/movie_subtitles.py --install                       # the local speech to text, once
python3 tools/movie_subtitles.py --languages all                 # every language (default: English and Italian)
python3 tools/movie_subtitles.py --languages it --movies row_1978 --force
python3 tools/movie_subtitles.py --model large-v3 --out ../draft # slower: a second opinion on doubtful lines
python3 tools/movie_subtitles.py --list                          # each movie's files: missing, generated, edited
```

It decodes each movie's sound with the game itself (`build/rac1-native --setup --movie-audio`, the same decoder the
game plays them with, to a 16 kHz mono WAV; `--movie-tracks` lists a movie's tracks), transcribes it and writes one file
per movie and language. Files already written are skipped (it can be stopped and run again); a file whose bytes differ
from `generated.txt` (or the `--out` folder's) is never written again, not even with `--force` (`--force-edited` does,
keeping the edited one as `.srt.bak`).

What it does to get fewer mistakes, all of it generic (no game text is in the tool):

- **Names**: the characters', planets' and gadgets' names (only the names) go to Whisper as its hotwords, which
  faster-whisper puts before every 30-second window (an initial prompt reaches only the first one). Without them it
  wrote "Quark", "Darla Gretsch", "Joe Y. Resort", "HoloGuys", "Lolo Maschera".
- **Voice detection** on, so music and silence give no text; Whisper's usual phantom lines over silence ("Thanks for
  watching", "Sottotitoli a cura di...") dropped; `condition_on_previous_text` off (on, a mistake repeats itself and
  phantom lines appear at the ends).
- **The cues**: at most about 90 characters (two lines of the box) and 7 seconds; a longer segment is cut where it
  costs least: after a sentence's end, then a clause's, at a pause, where the two sides are the most even, never after a
  short word ("di", "the", "non") and never leaving a word or two alone; a long silence inside a segment always cuts.
  A word Whisper gives in pieces ("dall" "'invasione", "snot" "-nosed") stays whole. "&" becomes "and" (the font has
  none).

### What it gets right

The English and Italian files of the North American disc (2026-11) were reviewed line by line against the sound, with
two more transcriptions of every movie (`large-v3`; `large-v3-turbo` with beam 10) and three of each doubtful line. On
the 31 Italian and 39 English tracks that differ (some movies are on the disc twice), the tool before these changes
and the tool now, against the reviewed files (words in lower case, punctuation aside; a "corrected span" is one place
where the review changed words; an "orphan" is a cue of a word or two cut from its sentence):

| | Italian: wrong words | corrected spans it gets right | English: wrong words | corrected spans | orphan cues |
|---|---|---|---|---|---|
| before (initial prompt with the names, greedy cuts) | 116 of 2446 (4.7 %) | 0 of 83 | 93 of 3940 (2.4 %) | 0 of 53 | 33 of 753 |
| now (names as hotwords, the cuts above) | 106 (4.3 %) | 15 of 83 | 76 (1.9 %) | 16 of 53 | 1 of 694 |

What it gets right now: the names (Qwark, Darla Gratch, Jowai, Hologuise, Olomaschera, Al's RoboShack...) and the cues.
What still needs a person: words heard wrong in fast or noisy speech ("impinato" for "inquinato", "Ste." for "a
stento"), a word the speaker cuts off (Whisper writes it whole), punctuation, phantom words over music. A prompt of
hesitations ("Ehm... io- cioè...") to keep cut-off words was tried and left out: it reaches only the first 30 seconds,
and over silence Whisper wrote the prompt itself as a line.

### Which model

Measured on four movies (206 s of speech in each language), on a Ryzen AI 9 HX 470 (CPU, int8, 8 threads, while a
build ran beside it). "Differs" is the share of words that differ from `large-v3-turbo`'s transcript; reading them,
`large-v3-turbo`'s were right where the others differ.

| Model | Download | English: time / sound | differs | Italian: time / sound | differs |
|---|---|---|---|---|---|
| base | 145 MB | 0.06 | 0 to 15 % | 0.06 | 13 to 32 % |
| small | 480 MB | 0.09 | 0 to 3 % | 0.11 | 9 to 25 % |
| large-v3-turbo (the default) | 1.6 GB | 0.18 | - | 0.20 | - |
| large-v3 | 3.1 GB | about 0.5 | | about 0.6 | |

`base` is not usable (in Italian it makes up words); `small` is good in English and loses whole phrases in Italian's
faster scenes; `large-v3-turbo` gets nearly everything. `large-v3` is slower and heard better several of the Italian
lines the review corrected ("Accidenti", "pur fare", "Espulsione"), but it also dropped whole stretches of two movies:
it is a second opinion, not the default. All 84 movies in English and the 66 with an Italian track (92 minutes of sound
together) take about 26 minutes with `large-v3-turbo` on that CPU, the model's download aside.

## Testing

- `RAC1_MOVIE_SUBTITLES=on` shows them also in a headless run (the setting is off there), `RAC1_LANGUAGE` picks the
  language, `RAC1_MOVIE_AT=<n>@<frame>` starts story movie n, `RAC1_MOVIE_SUBTITLES_DIR` reads the files from another
  folder (the broken-file cases).
- Off, the reference images and a movie's frames are those of the build without this; on, `RAC1_RAMHASH` is the same.
