// A movie subtitle pack (docs/movie-subtitles.md): a zip of SubRip files, row_XXXX.<lang>.srt, unpacked into the game
// folder's assets/movie-subtitles/, where the game reads them. Only those names are taken, wherever they sit in the zip;
// anything else (a README, folders, other files) is left out, and no entry can write outside that folder.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.RegularExpressions;

namespace Rac1Launcher
{
    static class SubtitlePack
    {
        // (the movie's file in the extracted disc, assets/disc/toc/row_XXXX.pss, and a language the game has)
        static readonly Regex Name = new Regex(@"^row_[0-9a-f]{4}\.(en|fr|de|es|it)\.srt$", RegexOptions.IgnoreCase);
        const long MaxFile = 1 << 20;  // (the game reads no file over 1 MB)

        public static string Dir(GameDir game) => game.P("assets", "movie-subtitles");

        /// <summary>The languages the setup can make movie subtitles in (the movies' sound tracks).</summary>
        public static readonly string[] Languages = { "en", "fr", "de", "es", "it" };
        public static readonly string[] LanguageNames = { "English", "French", "German", "Spanish", "Italian" };

        /// <summary>Settings.MovieSubtitles from the Settings page's boxes: "off", "all" or "en,it"...</summary>
        public static string Choice(bool on, bool[] langs)
        {
            if (!on || !langs.Any(x => x)) return "off";
            if (langs.All(x => x)) return "all";
            return string.Join(",", Languages.Where((l, i) => langs[i]));
        }

        /// <summary>Whether a language is in Settings.MovieSubtitles (all of them when it says "all" or nothing yet).</summary>
        public static bool Has(string choice, string lang) =>
            string.IsNullOrEmpty(choice) || choice == "off" || choice == "all" || choice.Split(',').Contains(lang);

        /// <summary>Unpacks the pack's subtitle files; a different file already there with the same name is kept as .srt.bak
        /// first (a player's own edits are not lost). Returns how many were installed and how many entries were left out.</summary>
        public static (int installed, int skipped) Install(GameDir game, string zip)
        {
            string dir = Dir(game);
            int installed = 0, skipped = 0;
            using (var za = ZipFile.OpenRead(zip))
            {
                var take = new List<ZipArchiveEntry>();
                foreach (var e in za.Entries)
                {
                    if (e.FullName.EndsWith("/") || e.FullName.EndsWith("\\")) continue;
                    // (only the entry's own name: a pack with its files in a folder works, "../x" or "C:\x" cannot escape)
                    string name = e.FullName.Replace('\\', '/').Split('/').Last();
                    if (Name.IsMatch(name) && e.Length <= MaxFile) take.Add(e);
                    else skipped++;
                }
                if (take.Count == 0)
                    throw new InvalidDataException("This zip has no movie subtitles (files named like row_1810.en.srt).");
                Directory.CreateDirectory(dir);
                foreach (var e in take)
                {
                    string name = e.FullName.Replace('\\', '/').Split('/').Last().ToLowerInvariant();
                    string dst = Path.Combine(dir, name);
                    byte[] bytes;
                    using (var s = e.Open())
                    using (var m = new MemoryStream())
                    {
                        s.CopyTo(m);
                        bytes = m.ToArray();
                    }
                    if (File.Exists(dst))
                    {
                        if (File.ReadAllBytes(dst).SequenceEqual(bytes)) { installed++; continue; }
                        File.Copy(dst, dst + ".bak", true);
                    }
                    File.WriteAllBytes(dst, bytes);
                    installed++;
                }
            }
            return (installed, skipped);
        }
    }
}
