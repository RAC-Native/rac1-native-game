// The game's folder, the launcher's settings, GitHub (the latest release, the downloads) and the first install.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace Rac1Launcher
{
    /// <summary>The launcher's own settings: %APPDATA%\rac1-native\launcher.txt, "key value" lines.</summary>
    class Settings
    {
        public string GameDir = "";
        public string Iso = "";
        public int Jobs;  // (0: the setup's own choice)
        public string Container = "";  // (Linux, an immutable system: "distrobox" or "toolbox", the game is built and run in a container)

        public static string ConfigDir => Platform.ConfigDir;
        static string FilePath => Path.Combine(ConfigDir, "launcher.txt");

        public static Settings Load()
        {
            var s = new Settings();
            try
            {
                foreach (var line in File.ReadAllLines(FilePath, Encoding.UTF8))
                {
                    int sp = line.IndexOf(' ');
                    if (sp < 0) continue;
                    string k = line.Substring(0, sp), v = line.Substring(sp + 1).Trim();
                    if (k == "game_dir") s.GameDir = v;
                    else if (k == "iso") s.Iso = v;
                    else if (k == "jobs") int.TryParse(v, out s.Jobs);
                    else if (k == "container") s.Container = v;
                }
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            return s;
        }

        public void Save()
        {
            try
            {
                Directory.CreateDirectory(ConfigDir);
                File.WriteAllText(FilePath, $"game_dir {GameDir}\r\niso {Iso}\r\njobs {Jobs}\r\n"
                                            + (string.IsNullOrEmpty(Container) ? "" : $"container {Container}\r\n"), new UTF8Encoding(false));
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }

    /// <summary>The update channel, the game's own setting (Main menu > Updates > Update Channel): the line
    /// "update_channel 0|1" of advanced_options.txt (src/game/enhance/advanced.cpp), which tools\setup.py --update also
    /// reads. The launcher reads and writes that same line, so the two never disagree.</summary>
    static class Channel
    {
        static string FilePath
        {
            get
            {
                string env = Environment.GetEnvironmentVariable("RAC1_ADVANCED_FILE");
                return !string.IsNullOrEmpty(env) ? env : Path.Combine(Settings.ConfigDir, "advanced_options.txt");
            }
        }

        public static bool Development
        {
            get
            {
                try
                {
                    foreach (var line in File.ReadAllLines(FilePath))
                    {
                        var p = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
                        if (p.Length == 2 && p[0] == "update_channel") return p[1] == "1";
                    }
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
                return false;
            }
            set
            {
                // (the other lines kept as they are: the game's other Advanced settings)
                var lines = new List<string>();
                try { lines.AddRange(File.ReadAllLines(FilePath)); }
                catch (IOException) { }
                string row = "update_channel " + (value ? "1" : "0");
                int i = lines.FindIndex(l => l.Split((char[])null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault() == "update_channel");
                if (i >= 0) lines[i] = row;
                else lines.Add(row);
                Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
                File.WriteAllText(FilePath, string.Join("\n", lines) + "\n", new UTF8Encoding(false));
            }
        }

        public static string Name => Development ? "development" : "stable";
    }

    /// <summary>A folder with the game: its sources (a release's zip, or a clone), and what the setup made in it.</summary>
    class GameDir
    {
        public readonly string Dir;
        public GameDir(string dir) { Dir = dir; }

        public string P(params string[] parts) => Path.Combine(new[] { Dir }.Concat(parts).ToArray());
        public bool Downloaded => File.Exists(P("tools", "setup.py")) && File.Exists(P("CMakeLists.txt"));
        public string Exe => P("build", Platform.ExeName);
        public bool Built => File.Exists(Exe);
        public string ToolchainScript => P("tools", "windows", "toolchain.ps1");
        public bool IsGitClone => Directory.Exists(P(".git"));

        /// <summary>The version of the sources (project(... VERSION) in CMakeLists.txt), or null.</summary>
        public Version SourceVersion
        {
            get
            {
                try
                {
                    var m = Regex.Match(File.ReadAllText(P("CMakeLists.txt")), @"project\(\s*rac1-native\s+VERSION\s+(\d+\.\d+\.\d+)");
                    return m.Success ? new Version(m.Groups[1].Value) : null;
                }
                catch (IOException) { return null; }
            }
        }

        /// <summary>The commit of main a development-channel download unpacked ("commit" in assets\update-version.txt,
        /// written by the launcher and by tools\setup.py --update), or null.</summary>
        public string InstalledCommit
        {
            get
            {
                try
                {
                    string f = P("assets", "update-version.txt");
                    if (!File.Exists(f)) return null;
                    bool main = false;
                    string commit = null;
                    foreach (var line in File.ReadAllLines(f))
                    {
                        if (line == "branch main") main = true;
                        else if (line.StartsWith("commit ") && line.Length > 7) commit = line.Substring(7).Trim();
                    }
                    return main && !string.IsNullOrEmpty(commit) ? commit : null;
                }
                catch (IOException) { return null; }
            }
        }

        /// <summary>The disc image the game already uses (what the setup linked or wrote in assets\iso\), or null.</summary>
        public string KnownIso()
        {
            try
            {
                string f = P("assets", "iso", "disc-path.txt");
                if (File.Exists(f))
                {
                    string first = File.ReadAllLines(f, Encoding.UTF8).FirstOrDefault()?.Trim();
                    if (!string.IsNullOrEmpty(first) && File.Exists(first)) return first;
                }
                foreach (var n in new[] { "rac1-ntsc-u.iso", "rac1-pal.iso" })
                    if (File.Exists(P("assets", "iso", n))) return P("assets", "iso", n);
            }
            catch (Exception) { }
            return null;
        }

        public static bool Plain(string path) => path.All(c => c < 128 && c != ' ' && c != '&' && c != '%' && c != '!');
    }

    static class GitHub
    {
        public const string Repo = "RAC-Native/rac1-native-game";
        public const string Api = "https://api.github.com/repos/" + Repo;
        public const string Page = "https://github.com/" + Repo;

        static GitHub()
        {
            // (TLS 1.2 and newer: older Windows 10 builds still offer TLS 1.0 by default to .NET Framework)
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12 | (SecurityProtocolType)12288;  // Tls13
        }

        static HttpWebRequest Request(string url)
        {
            var r = (HttpWebRequest)WebRequest.Create(url);
            r.UserAgent = "rac1-native-launcher";
            r.Accept = "application/vnd.github+json";
            r.Timeout = 20000;
            r.ReadWriteTimeout = 30000;
            r.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;
            return r;
        }

        public class Release
        {
            public string Tag = "";
            public Version Version;
            public string Notes = "";
            public string LauncherUrl;  // (the release's launcher for this system, Platform.LauncherAsset, when it has one)
            public string LauncherSha256;  // (its SHA-256 as GitHub computed it at the upload, lowercase hex, or null)
        }

        /// <summary>The latest release (throws when GitHub cannot be reached).</summary>
        public static Release Latest()
        {
            string json;
            using (var resp = Request(Api + "/releases/latest").GetResponse())
            using (var rd = new StreamReader(resp.GetResponseStream(), Encoding.UTF8))
                json = rd.ReadToEnd();
            var r = new Release();
            r.Tag = JsonString(json, "tag_name") ?? "";
            var m = Regex.Match(r.Tag, @"(\d+)\.(\d+)\.(\d+)");
            if (m.Success) r.Version = new Version(m.Value);
            r.Notes = JsonString(json, "body") ?? "";
            var a = Regex.Match(json, "\"browser_download_url\"\\s*:\\s*\"([^\"]*/" + Regex.Escape(Platform.LauncherAsset) + ")\"");
            if (a.Success)
            {
                r.LauncherUrl = a.Groups[1].Value;
                // (the asset's "digest", "sha256:...", is in its own object, before its download URL: after the
                // previous asset's URL, if any)
                int from = json.LastIndexOf("\"browser_download_url\"", Math.Max(0, a.Index - 1), StringComparison.Ordinal);
                string own = json.Substring(from < 0 ? 0 : from + 1, a.Index - (from < 0 ? 0 : from + 1));
                var d = Regex.Match(own, "\"digest\"\\s*:\\s*\"sha256:([0-9a-fA-F]{64})\"");
                if (d.Success) r.LauncherSha256 = d.Groups[1].Value.ToLowerInvariant();
            }
            if (r.Tag == "") throw new InvalidDataException("GitHub's answer has no release tag");
            return r;
        }

        /// <summary>The newest commit on main (the development channel), its full hash.</summary>
        public static string MainCommit()
        {
            string json;
            using (var resp = Request(Api + "/commits/main").GetResponse())
            using (var rd = new StreamReader(resp.GetResponseStream(), Encoding.UTF8))
                json = rd.ReadToEnd();
            string sha = JsonString(json, "sha");
            if (string.IsNullOrEmpty(sha)) throw new InvalidDataException("GitHub's answer has no commit");
            return sha;
        }

        /// <summary>A string field of a JSON object (the first one with this name), unescaped; null when absent.</summary>
        static string JsonString(string json, string key)
        {
            var m = Regex.Match(json, "\"" + Regex.Escape(key) + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
            if (!m.Success) return null;
            return Regex.Replace(m.Groups[1].Value, @"\\(u[0-9a-fA-F]{4}|.)", e =>
            {
                string s = e.Groups[1].Value;
                switch (s[0])
                {
                    case 'n': return "\n";
                    case 'r': return "\r";
                    case 't': return "\t";
                    case 'u': return ((char)Convert.ToInt32(s.Substring(1), 16)).ToString();
                    default: return s;
                }
            });
        }

        /// <summary>Downloads url into file; progress(bytes so far, total or -1).</summary>
        public static void Download(string url, string file, Action<long, long> progress, CancellationToken cancel)
        {
            var req = Request(url);
            req.Accept = "*/*";
            using (var resp = (HttpWebResponse)req.GetResponse())
            using (var src = resp.GetResponseStream())
            using (var dst = File.Create(file))
            {
                long total = resp.ContentLength, done = 0;
                var buf = new byte[1 << 16];
                int n;
                while ((n = src.Read(buf, 0, buf.Length)) > 0)
                {
                    cancel.ThrowIfCancellationRequested();
                    dst.Write(buf, 0, n);
                    done += n;
                    progress(done, total);
                }
            }
        }
    }

    static class Hash
    {
        /// <summary>A file's SHA-256, lowercase hex.</summary>
        public static string Sha256(string file)
        {
            using (var sha = SHA256.Create())
            using (var f = File.OpenRead(file))
                return BitConverter.ToString(sha.ComputeHash(f)).Replace("-", "").ToLowerInvariant();
        }
    }

    static class Install
    {
        /// <summary>Unpacks a release's source zip (GitHub's: one top folder) into dir, and records what it unpacked as
        /// tools\setup.py --update does (assets\update-version.txt for the version the game shows, and the files'
        /// hashes, so a later update removes the files a new version no longer has).</summary>
        /// <param name="versionLines">assets\update-version.txt's lines, as tools\setup.py --update writes them: for a
        /// release "describe v1.2.3", "commit ", "branch "; for main "describe abc1234", "commit &lt;hash&gt;", "branch main".</param>
        public static void Unpack(string zip, string dir, string[] versionLines, Action<string> log)
        {
            var hashes = new SortedDictionary<string, string>(StringComparer.Ordinal);
            using (var za = ZipFile.OpenRead(zip))
            using (var sha = SHA256.Create())
            {
                foreach (var e in za.Entries)
                {
                    string name = e.FullName.Replace('\\', '/');
                    int slash = name.IndexOf('/');
                    if (slash < 0) continue;
                    string rel = name.Substring(slash + 1);
                    if (rel == "" || rel.EndsWith("/")) continue;
                    string top = rel.Split('/')[0];
                    if (top == ".git" || top == ".toolchain" || top == "assets" || top == "build") continue;
                    string dst = Path.GetFullPath(Path.Combine(dir, rel.Replace('/', Path.DirectorySeparatorChar)));
                    string inside = Path.GetFullPath(dir).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
                    if (!dst.StartsWith(inside, StringComparison.OrdinalIgnoreCase)) continue;
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    e.ExtractToFile(dst, true);
#if !WINDOWS_LAUNCHER
                    // (the files' Unix modes, in the zip's external attributes: ./setup.sh and the tools' scripts executable)
                    int mode = (e.ExternalAttributes >> 16) & 0x1ff;
                    if (mode != 0) File.SetUnixFileMode(dst, (UnixFileMode)mode);
#endif
                    if (!rel.Contains("__pycache__"))
                        hashes[rel] = BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(dst))).Replace("-", "").ToLowerInvariant();
                }
            }
            log($"Unpacked {hashes.Count} files.");
            string assets = Path.Combine(dir, "assets");
            Directory.CreateDirectory(assets);
            File.WriteAllText(Path.Combine(assets, "update-version.txt"), string.Join("\n", versionLines) + "\n", Encoding.ASCII);
            string state = Path.Combine(assets, "setup-state.json");
            if (!File.Exists(state))
            {
                var sb = new StringBuilder("{\n  \"source_hashes\": {\n");
                sb.Append(string.Join(",\n", hashes.Select(kv => $"    \"{JsonEscape(kv.Key)}\": \"{kv.Value}\"")));
                sb.Append("\n  }\n}\n");
                File.WriteAllText(state, sb.ToString(), new UTF8Encoding(false));
            }
        }

        static string JsonEscape(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }
}
