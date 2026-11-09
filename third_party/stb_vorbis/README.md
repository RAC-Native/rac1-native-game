# stb_vorbis

`stb_vorbis.c` is Sean Barrett's Ogg Vorbis decoder, **v1.22**, from [nothings/stb](https://github.com/nothings/stb),
unmodified:

```
https://raw.githubusercontent.com/nothings/stb/4e9b368bfc071e97206ea5c6adba68dd0cbf8897/stb_vorbis.c
sha256 4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636
```

It is public domain or MIT, at your choice (the end of the file; `LICENSE` is a copy of that text).

Only the level composer uses it (`src/setup/level_music.cpp`): a level mod's music or sound given as an `.ogg` is
decoded when the level is composed, then encoded like a WAV (`docs/mods.md`, "Level mods"). The game itself never
reads Ogg. It is built as a static library with `STB_VORBIS_NO_PUSHDATA_API` and `STB_VORBIS_NO_STDIO` (it decodes
from memory); users include `stb_vorbis.c` with `STB_VORBIS_HEADER_ONLY` defined.

To update it, replace `stb_vorbis.c` with a newer upstream copy and update the commit, the version and the hash above.
