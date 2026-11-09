# The example level

`make_example.py` writes the example level mod, "Example Plateau", into a mod folder (`mods/example-plateau` by
default); the setup composes it with your disc. See [docs/mods.md](../../docs/mods.md) ("Level mods"),
[docs/sdk.md](../../docs/sdk.md) for its plugin (`plugin/`) and [docs/custom-levels.md](../../docs/custom-levels.md).

## Music

Music: "Overworld" by Daino Selvatico (`music/overworld.ogg`), licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Unlike the rest of this repository (MIT), the track is under
that licence: you may share and adapt it, also commercially, as long as you credit the author and link the licence.

It is a stereo Ogg Vorbis file, 44.1 kHz, 110.8 s, that loops seamlessly. The script copies it into the level folder
(`music overworld.ogg` in `level.txt`), and the composer encodes it as the game's music when the level is composed.
