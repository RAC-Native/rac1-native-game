// The saves (docs/saves.md): the virtual memory card's folder, %APPDATA%\rac1-native\memcard on Windows,
// ~/.local/share/rac1-native/memcard on Linux (or RAC1_MEMCARD_DIR), with one folder per disc (BASCUS-97199RATCHET, BESCES-50916RATCHET) holding save0.bin to
// save4.bin, the five slots, each with its PC settings in save<N>.pc.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;

namespace Rac1Launcher
{
    class SaveSlot
    {
        public string Region = "";
        public int Slot;  // (1 to 5, as the game's menus number them)
        public string File = "";
        public DateTime Saved;
        public long Size;
    }

    static class Saves
    {
        public static string CardDir
        {
            get
            {
                string env = Environment.GetEnvironmentVariable("RAC1_MEMCARD_DIR");
                return !string.IsNullOrEmpty(env) ? env : Path.Combine(Platform.DataDir, "memcard");
            }
        }

        public static string BackupDir => Path.Combine(Platform.DataDir, "save-backups");

        static string RegionName(string folder)
        {
            if (folder.StartsWith("BASCUS-97199", StringComparison.OrdinalIgnoreCase)) return "North America (NTSC-U)";
            if (folder.StartsWith("BESCES-50916", StringComparison.OrdinalIgnoreCase)) return "Europe (PAL)";
            return folder;
        }

        public static List<SaveSlot> List()
        {
            var list = new List<SaveSlot>();
            if (!Directory.Exists(CardDir)) return list;
            foreach (var dir in Directory.GetDirectories(CardDir).OrderBy(d => d))
                for (int n = 0; n < 5; n++)
                {
                    var f = new FileInfo(Path.Combine(dir, $"save{n}.bin"));
                    if (!f.Exists) continue;
                    list.Add(new SaveSlot { Region = RegionName(Path.GetFileName(dir)), Slot = n + 1, File = f.FullName,
                                            Saved = f.LastWriteTime, Size = f.Length });
                }
            return list;
        }

        /// <summary>The whole card (every disc's saves and their settings) into a zip.</summary>
        public static void Backup(string zip)
        {
            if (File.Exists(zip)) File.Delete(zip);
            ZipFile.CreateFromDirectory(CardDir, zip, CompressionLevel.Optimal, false);
        }

        /// <summary>A backup made by Backup put back: the card as it is now is backed up first (into BackupDir), then
        /// the zip's files replace it. Returns that automatic backup's path.</summary>
        public static string Restore(string zip)
        {
            using (var za = ZipFile.OpenRead(zip))
            {
                if (!za.Entries.Any(e => e.FullName.Replace('\\', '/').Contains("RATCHET/save")))
                    throw new InvalidDataException("This zip has no Ratchet & Clank saves (a backup made by the launcher has folders like BASCUS-97199RATCHET).");
                string before = null;
                if (Directory.Exists(CardDir) && Directory.EnumerateFileSystemEntries(CardDir).Any())
                {
                    Directory.CreateDirectory(BackupDir);
                    before = Path.Combine(BackupDir, $"before-restore-{DateTime.Now:yyyy-MM-dd_HH-mm-ss}.zip");
                    Backup(before);
                }
                Directory.CreateDirectory(CardDir);
                // (with its separator: an entry "../memcard2/x" must not pass as inside ".../memcard")
                string root = Path.GetFullPath(CardDir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
                foreach (var e in za.Entries)
                {
                    if (e.FullName.EndsWith("/") || e.FullName.EndsWith("\\")) continue;
                    string dst = Path.GetFullPath(Path.Combine(root, e.FullName));
                    if (!dst.StartsWith(root, StringComparison.OrdinalIgnoreCase)) continue;
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    e.ExtractToFile(dst, true);
                }
                return before;
            }
        }

        /// <summary>A slot's save and its PC settings to the Recycle Bin or the trash (so it can still be brought back).</summary>
        public static void Delete(SaveSlot s)
        {
            foreach (var f in new[] { s.File, Path.ChangeExtension(s.File, ".pc") })
                if (File.Exists(f))
                    Platform.Trash(f);
        }
    }
}
