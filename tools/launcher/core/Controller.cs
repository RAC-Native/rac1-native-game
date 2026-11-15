// What the launcher does, apart from its window: the game folder's state and what the Play button does about it, the
// update check (on the chosen channel), the download, the setup (with its retries), the launcher's own update, the log.
// The window (windows/MainForm.cs, linux/MainWindow.cs) shows it through IView, and asks the player the questions.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace Rac1Launcher
{
    enum GameState { Checking, NotInstalled, NeedsSetup, Ready }

    /// <summary>A disc image the setup found (tools/setup.py --discs): its role ("game", "data" or "unused"), the image,
    /// what it is, and why (why not, for one not used).</summary>
    sealed class DiscEntry
    {
        public string Role, Path, What, Why;
    }

    /// <summary>The window, as the work sees it. Called from the work's thread: the window passes it to its own.</summary>
    interface IView
    {
        void Step(string text);        // a new step: its text, and a progress bar that moves without a percent
        void StepDetail(string text);  // more about the current step
        void Progress(int percent);
        void Log(string line);
        bool Ask(string title, string text);  // a yes / no question, answered before the work goes on
    }

    sealed class Controller
    {
        public readonly Settings Settings = Settings.Load();
        public GameDir Game;
        public GitHub.Release Latest;
        public string DevCommit;    // (the newest commit on the dev branch, for the development channel)
        public string Failure;      // (why the last task failed, for the player)
        public readonly StringBuilder FullLog = new StringBuilder();
        public static readonly Version LauncherVersion = Assembly.GetExecutingAssembly().GetName().Version;

        readonly IView view;
        readonly List<string> errors = new List<string>();
        readonly object logLock = new object();
        StreamWriter logFile;
        bool sawPythonCrash, sawStepMarks, sawMissing, sawContainer;
        string packagesCommand;  // (Linux: the command that installs what the setup found missing)

        public Controller(IView view, string runningExe)
        {
            this.view = view;
            // the game's folder: the one the launcher is in when it is a game folder (the launcher copied beside the
            // game, or put in a downloaded zip's folder), else the one in the settings, else the default
            string own = Path.GetDirectoryName(runningExe);
            Game = new GameDir(new GameDir(own).Downloaded ? own
                               : !string.IsNullOrEmpty(Settings.GameDir) ? Settings.GameDir : Platform.DefaultGameDir());
        }

        public static string VersionText(Version v) => v == null ? "?" : $"{v.Major}.{v.Minor}.{v.Build}";
        static string Short(string sha) => sha.Substring(0, Math.Min(7, sha.Length));

        // --- The state ----------------------------------------------------------------------------------------

        /// <summary>What the game folder holds (it runs tools/setup.py --check: call it off the window's thread). due:
        /// what the setup would redo.</summary>
        public GameState ComputeState(out string due)
        {
            due = "";
            var g = Game;
            if (!g.Downloaded) return GameState.NotInstalled;
            if (!g.Built) return GameState.NeedsSetup;
            if (!Platform.CanRunSetup(g)) return GameState.Ready;  // (set up from a shell of the player's own: it just starts)
            // (tools/setup.py --check, as play.cmd: a new version unpacked over this one, or a setup that did not finish;
            // exit status 10 when the setup has something to do)
            var lines = new List<string>();
            int rc;
            using (var r = new Runner())
                rc = r.Run(Platform.Python(g), Platform.Quote(g.P("tools", "setup.py")) + " --check", g.Dir,
                           Platform.PythonEnv(g), l => lines.Add(l), Encoding.UTF8);
            due = string.Join("\n", lines.Take(4));
            return rc == 10 ? GameState.NeedsSetup : GameState.Ready;
        }

        /// <summary>The Play page's heading, its text and the main button's, for a state.</summary>
        public (string title, string detail, string button) Describe(GameState st, string due)
        {
            switch (st)
            {
                case GameState.NotInstalled:
                    return ("Not installed yet",
                            "Install downloads the game's latest version from GitHub and builds it on this computer from your own "
                            + "Ratchet & Clank disc image: the North American disc (SCUS-97199) or the European one (SCES-50916), "
                            + "as a plain .iso file.\n\nThe first time takes a while (often 20 to 40 minutes) and needs an internet "
                            + "connection and about 10 GB of free space. It goes into:\n" + Game.Dir,
                            "Install");
                case GameState.NeedsSetup:
                    return ("The setup needs to run",
                            (Game.Built ? "A new version was downloaded, or the last setup did not finish." : "The game is downloaded but not built yet.")
                            + (string.IsNullOrWhiteSpace(due) ? "" : "\n" + due) + "\nThe setup builds only what changed.",
                            "Set up");
                case GameState.Ready:
                    string commit = Game.InstalledCommit;
                    return ("Ready to play",
                            "Version " + VersionText(Game.SourceVersion) + (commit != null ? $" (development: dev at {Short(commit)})" : "")
                            + (Game.IsGitClone ? " (a git clone)" : "") + ", in " + Game.Dir,
                            "Play");
                default:
                    return ("Looking at the game folder...", "", "...");
            }
        }

        // --- Updates ------------------------------------------------------------------------------------------

        /// <summary>The latest release, and the dev branch's newest commit on the development channel (throws when GitHub cannot
        /// be reached). Off the window's thread.</summary>
        public void FetchUpdates()
        {
            var r = GitHub.Latest();
            string dev = Channel.Development ? GitHub.DevCommit() : null;
            Latest = r;
            if (dev != null) DevCommit = dev;
        }

        /// <summary>What is newer on the chosen channel than what is installed, or null (nothing newer, or not known).
        /// Stable: a release newer than the sources' version. Development: the dev branch, when it moved since the commit the last
        /// download unpacked (a stable install on the development channel: dev is always newer).</summary>
        public string UpdateMessage()
        {
            var v = Game.SourceVersion;
            if (Channel.Development)
            {
                if (DevCommit == null || Game.IsGitClone && Game.InstalledCommit == null) return null;
                string have = Game.InstalledCommit;
                if (have == DevCommit) return null;
                return have == null ? $"You are on the development channel: the dev branch ({Short(DevCommit)}) has the newest changes."
                                    : $"There are new changes on the dev branch ({Short(DevCommit)}; you have {Short(have)}).";
            }
            if (Latest?.Version != null && v != null && Latest.Version > v)
                return $"Version {VersionText(Latest.Version)} is out (you have {VersionText(v)}).";
            return null;
        }

        /// <summary>The answer to "Check for updates", after FetchUpdates.</summary>
        public string UpdateSummary()
        {
            var v = Game.SourceVersion;
            string newer = UpdateMessage();
            string dev = Channel.Development ? DevCommit : null;
            if (!Game.Downloaded)
                return dev != null ? $"Install gets the dev branch ({Short(dev)})." : $"The latest version is {Latest?.Tag}: Install gets it.";
            if (newer != null) return newer + " Use Update on the Play page.";
            if (dev != null && Game.IsGitClone && Game.InstalledCommit == null)
                return $"This folder is a git clone: Update brings it to the dev branch's newest changes ({Short(dev)}).";
            if (dev != null) return $"You have the newest changes on the dev branch ({Short(dev)}).";
            return $"You have the latest version ({VersionText(v)}).";
        }

        public const string DevelopmentWarning = "The development channel gets the newest changes from the dev branch, where they land before main and the releases: "
            + "new fixes sooner, but also new bugs. The game's own Update Channel option changes too.\n\nSwitch to the development channel?";

        /// <summary>The latest release's launcher version when it is newer than this one (after FetchUpdates), or null.
        /// Nothing is downloaded until the player agrees (DownloadNewLauncher): a program that fetches and swaps its own
        /// .exe unasked is what antivirus heuristics take for a dropper (GitHub issue #115).</summary>
        public Version NewLauncherVersion()
        {
            var rel = Latest;
            if (rel?.LauncherUrl == null || rel.Version == null) return null;
            var lv = LauncherVersion;
            if (lv.Major == 0 && lv.Minor == 0 && lv.Build == 0) return null;  // (a build of one's own)
            if (rel.Version <= new Version(lv.Major, lv.Minor, lv.Build)) return null;
            return rel.Version;
        }

        /// <summary>The latest release's launcher, downloaded beside the running one once the player agreed (off the
        /// window's thread) and checked against the SHA-256 GitHub lists for it; null when that did not work.</summary>
        public string DownloadNewLauncher(string runningExe)
        {
            var rel = Latest;
            if (NewLauncherVersion() == null) return null;
            string fresh = runningExe + ".new";
            try
            {
                Log($"Downloading the new launcher ({rel.Tag}): {rel.LauncherUrl}");
                GitHub.Download(rel.LauncherUrl, fresh, (a, b) => { }, CancellationToken.None);
                if (new FileInfo(fresh).Length < 32 * 1024) throw new InvalidDataException("the download is too small");
                if (rel.LauncherSha256 != null)
                {
                    string got = Hash.Sha256(fresh);
                    if (got != rel.LauncherSha256)
                        throw new InvalidDataException($"its SHA-256 is {got}, the release lists {rel.LauncherSha256}");
                    Log($"The new launcher's SHA-256 matches the release's ({got}).");
                }
                return fresh;
            }
            catch (Exception e)
            {
                Log("The new launcher could not be downloaded: " + e.Message);
                try { File.Delete(fresh); } catch (Exception) { }
                return null;
            }
        }

        /// <summary>The question the window asks before the launcher updates itself.</summary>
        public string NewLauncherQuestion(Version v) =>
            $"A new launcher is out ({VersionText(v)}; this one is {VersionText(LauncherVersion)}).\n\n"
            + "Download it from GitHub and restart the launcher with it now?";

        // --- The work: download, setup ------------------------------------------------------------------------

        /// <summary>The source zip of the chosen channel (the latest release, or the dev branch), unpacked into the game folder.</summary>
        public bool Download(Runner r, CancellationToken cancel)
        {
            view.Step("Finding the latest version");
            string label, url;
            string[] version;
            try
            {
                if (Channel.Development)
                {
                    string sha = GitHub.DevCommit();
                    DevCommit = sha;
                    label = GitHub.DevBranch + " " + Short(sha);
                    url = $"{GitHub.Page}/archive/{sha}.zip";  // (that commit, even if dev moves meanwhile)
                    version = new[] { "describe " + Short(sha), "commit " + sha, "branch " + GitHub.DevBranch };
                }
                else
                {
                    var rel = Latest ?? GitHub.Latest();
                    Latest = rel;
                    label = rel.Tag;
                    url = $"{GitHub.Page}/archive/refs/tags/{rel.Tag}.zip";
                    version = new[] { "describe " + rel.Tag, "commit ", "branch " };
                }
            }
            catch (Exception e) { Fail("GitHub could not be reached (" + e.Message + "). Check the internet connection, then try again."); return false; }
            string zip = Path.Combine(Path.GetTempPath(), $"rac1-native-{label.Replace(' ', '-')}.zip");
            view.Step($"Downloading the game ({label})");
            Log($"Downloading {url}");
            try
            {
                GitHub.Download(url, zip, (done, total) =>
                {
                    if (total > 0) view.Progress((int)(done * 100 / total));
                    view.StepDetail($"Downloading the game ({label}): {done >> 20} MB");
                }, cancel);
                view.Step("Unpacking");
                Install.Unpack(zip, Game.Dir, version, Log);
            }
            catch (OperationCanceledException) { return false; }
            catch (Exception e) { Fail("The download failed (" + e.Message + "). Check the internet connection, then try again."); return false; }
            finally { try { File.Delete(zip); } catch (Exception) { } }
            return true;
        }

        /// <summary>What setup.cmd / setup.sh do: on Windows the tools (tools\windows\toolchain.ps1), then tools/setup.py
        /// with extra ("", "--force", "--update --channel ..."). iso "": the setup picks the disc images itself (Discs).</summary>
        public bool Setup(Runner r, string iso, string extra)
        {
            var g = Game;
            if (Platform.Windows && !PrepareWindowsTools(r, g)) return false;
            view.Step(extra.StartsWith("--update") ? "Downloading the update" : "Starting the setup");

            int jobs = Settings.Jobs;
            // (the movie subtitles chosen in Settings, unless this run says itself)
            if (!extra.Contains("movie-subtitles") && !extra.StartsWith("--update"))
            {
                string subs = MovieSubtitlesArg();
                if (subs != "") extra = (extra + " " + subs).Trim();
            }
            bool installedPackages = false, madeContainer = false;
            for (int attempt = 0; ; attempt++)
            {
                sawPythonCrash = false;
                sawStepMarks = false;
                sawMissing = false;
                sawContainer = false;
                packagesCommand = null;
                errors.Clear();
                int rc;
                if (UsesContainer)
                {
                    // (Linux, an immutable system: the setup runs in the build container, its packages are there)
                    var argv = new List<string> { "python3", g.P("tools", "setup.py"), "--no-play" };
                    if (iso != "") argv.AddRange(new[] { "--iso", iso });
                    if (jobs > 0) argv.AddRange(new[] { "-j", jobs.ToString() });
                    if (extra != "") argv.AddRange(extra.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries));
                    rc = RunInContainer(r, argv, OnSetupLine);
                }
                else
                {
                    string args = Platform.Quote(g.P("tools", "setup.py")) + " --no-play" + (iso != "" ? " --iso " + Platform.Quote(iso) : "")
                                + (jobs > 0 ? " -j " + jobs : "") + (extra != "" ? " " + extra : "");
                    Log("> python " + args);
                    rc = r.Run(Platform.Python(g), args, g.Dir, Platform.PythonEnv(g), OnSetupLine, Encoding.UTF8);
                }
                if (r.Cancelled) return false;
                if (rc == 0) break;
                // (Linux, an immutable system: the build's packages cannot be installed on it; in a container instead.
                // An older setup.py does not say so (exit status 20, "@container"): its install command, on a system the
                // launcher sees is immutable)
                if (!Platform.Windows && !UsesContainer && !madeContainer
                    && (rc == Platform.ExitUseContainer || sawContainer || packagesCommand != null && Platform.ImmutableSystem() != null))
                {
                    madeContainer = true;
                    if (SetUpContainer(r)) { attempt--; continue; }
                    return false;
                }
                // (Linux: system packages missing; the setup printed the command that installs them)
                if (packagesCommand != null && !installedPackages)
                {
                    installedPackages = true;
                    if (InstallPackages(r, packagesCommand)) { attempt--; continue; }
                    return false;
                }
                // (Python itself crashed in a parallel job, GitHub issue #72: once more with one job at a time)
                if (attempt == 0 && sawPythonCrash && jobs != 1 && !extra.StartsWith("--update"))
                {
                    Log("Python crashed while generating the code: trying again with one job at a time.");
                    jobs = 1;
                    continue;
                }
                Fail(LastError() ?? "The setup failed.");
                return false;
            }
            if (UsesContainer)
            {
                try { Platform.WriteContainerPlayScript(g, Settings.Container); }
                catch (Exception e) { Log("The container's play script could not be written: " + e.Message); }
            }
            view.Step("Done");
            view.Progress(100);
            Log("The game is ready.");
            return true;
        }

        // --- Immutable systems (Linux): the build container ----------------------------------------------------

        /// <summary>The game is built and run in the build container (Linux, an immutable system: the choice is kept in
        /// launcher.txt, "container distrobox" or "container toolbox").</summary>
        public bool UsesContainer => !Platform.Windows && !string.IsNullOrEmpty(Settings.Container);

        /// <summary>A command run in the build container (Settings.Container), each line of its output to onLine. In its own
        /// session, its number in build/container-setup.pid: Cancel ends that whole session in the container (ending the
        /// container's tool here does not end what it started there).</summary>
        int RunInContainer(Runner r, List<string> argv, Action<string> onLine)
        {
            string tool = Settings.Container;
            string pidFile = Game.P("build", "container-setup.pid");
            Directory.CreateDirectory(Game.P("build"));
            var full = new List<string> { "setsid", "-w", "sh", "-c", "echo $$ > \"$0\"; exec \"$@\"", pidFile,
                                          "env", "RAC1_LAUNCHER=1", "PYTHONUNBUFFERED=1", "PYTHONIOENCODING=utf-8", "PYTHONUTF8=1" };
            full.AddRange(argv);
            var (exe, args) = Platform.InContainerCommand(tool, full);
            var (shownExe, shownArgs) = Platform.InContainerCommand(tool, argv);
            Log("> " + shownExe + " " + shownArgs);
            r.OnCancel = () =>
            {
                string pid = File.Exists(pidFile) ? File.ReadAllText(pidFile).Trim() : "";
                if (!Regex.IsMatch(pid, @"^\d+$")) return;
                var (kexe, kargs) = Platform.InContainerCommand(tool, new[] { "kill", "-TERM", "--", "-" + pid });
                using (var k = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(kexe, kargs)
                                                              { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = Game.Dir }))
                    k?.WaitForExit(15000);
            };
            try { return r.Run(exe, args, Game.Dir, null, onLine, Encoding.UTF8); }
            finally
            {
                r.OnCancel = null;
                try { File.Delete(pidFile); } catch (Exception) { }
            }
        }

        /// <summary>The build container, once the player agrees: made with distrobox (or toolbox) from an image that has
        /// every package the game needs, the packages installed in it (sudo needs no password in the container), and the
        /// choice kept. Then the setup runs in it.</summary>
        bool SetUpContainer(Runner r)
        {
            string system = Platform.ImmutableSystem() ?? "This system";
            string tool = Platform.ContainerTool();
            if (tool == null)
            {
                Fail(system + " is an immutable system: the programs the game needs cannot be installed on it, so the launcher "
                     + "builds the game in a container, with distrobox (or toolbox). Neither is installed. SteamOS (3.5 and newer) "
                     + "and Bazzite come with distrobox; elsewhere install it (https://distrobox.it), then click Set up.");
                return false;
            }
            string image = Platform.ContainerImage, packages = Platform.ContainerPackages;
            if (Platform.CanRunSetup(Game))
            {
                var info = new List<string>();
                using (var q = new Runner())
                    if (q.Run(Platform.Python(Game), Platform.Quote(Game.P("tools", "setup.py")) + " --container-info", Game.Dir,
                              Platform.PythonEnv(Game), l => info.Add(l), Encoding.UTF8) == 0)
                        foreach (var l in info)
                        {
                            if (l.StartsWith("image ") && l.Length > 6) image = l.Substring(6).Trim();
                            else if (l.StartsWith("packages ") && l.Length > 9) packages = l.Substring(9).Trim();
                        }
            }
            if (!Regex.IsMatch(packages, @"^[a-zA-Z0-9.+_ -]+$") || !Regex.IsMatch(image, @"^[a-zA-Z0-9./:_@-]+$"))
            {
                Fail("The setup's container information is not readable.");
                return false;
            }
            string manual = $"{tool} " + (tool == "toolbox" ? $"create -y -i {image} {Platform.ContainerName}"
                                                            : $"create -n {Platform.ContainerName} -i {image} --yes");
            if (!view.Ask("Set up a build container",
                          $"{system} is an immutable system: its system folders are read-only, so the programs that build the "
                          + $"game cannot be installed on it. The launcher can build and run the game in a container instead "
                          + $"({tool}, named \"{Platform.ContainerName}\"): it shares your home folder, display, graphics card and "
                          + "sound, and leaves the system as it is. It downloads about 1 GB the first time.\n\nSet up a build container?"))
            {
                Fail("The programs the game needs are not installed. On an immutable system, build it in a container: "
                     + "click Set up again and say yes, or see docs/setup.md (\"Steam Deck, Bazzite and other immutable systems\").");
                return false;
            }

            view.Step("Setting up the build container");
            view.StepDetail($"Setting up the build container ({tool}, {image}): the first time, it downloads the image");
            var existing = new List<string>();
            using (var q = new Runner())
                q.Run(tool, tool == "toolbox" ? "list -c" : "list --no-color", Game.Dir, null, l => existing.Add(l));
            bool exists = existing.Any(l => Regex.IsMatch(l, @"(^|[\s|])" + Regex.Escape(Platform.ContainerName) + @"($|[\s|])"));
            if (exists) Log($"The container {Platform.ContainerName} is there already: using it.");
            else
            {
                string args = tool == "toolbox" ? $"create -y -i {image} {Platform.ContainerName}"
                                                : $"create -n {Platform.ContainerName} -i {image} --yes"
                                                  + (File.Exists("/proc/driver/nvidia/version") ? " --nvidia" : "");
                Log($"> {tool} {args}");
                int rc = r.Run(tool, args, Game.Dir, null, Log);
                if (r.Cancelled) return false;
                if (rc != 0)
                {
                    Fail($"The build container could not be made (exit code {rc}; see Show the details). In a terminal: {manual}, "
                         + "then click Set up.");
                    return false;
                }
            }

            view.Step("Installing the programs the game needs in the container");
            view.StepDetail("Installing the programs the game needs in the container (the first time, several minutes)");
            Settings.Container = tool;  // (for RunInContainer; kept below once it worked)
            var install = new[] { "sudo", "dnf", "install", "-y" }.Concat(packages.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries)).ToList();
            int irc = RunInContainer(r, install, Log);
            if (r.Cancelled || irc != 0)
            {
                Settings.Container = "";
                var (iexe, iargs) = Platform.InContainerCommand(tool, install);
                if (!r.Cancelled)
                    Fail($"The programs could not be installed in the container (exit code {irc}). In a terminal: {iexe} {iargs}, "
                         + "then click Set up.");
                return false;
            }
            Settings.Save();
            Log($"The build container is ready ({tool}, {Platform.ContainerName}); the setup runs in it from now on. Running the setup again.");
            return true;
        }

        bool PrepareWindowsTools(Runner r, GameDir g)
        {
            RepairLinkedFiles(g);  // (first: an older release's toolchain.ps1 stops when clang.exe is missing)
            view.Step("Preparing the tools that build the game");
            view.StepDetail("Preparing the tools that build the game (the first time: about 1 GB to download, several minutes)");
            string ps = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe");
            int rc = r.Run(ps, "-NoProfile -ExecutionPolicy Bypass -File " + Platform.Quote(g.ToolchainScript), g.Dir,
                           new Dictionary<string, string> { ["MSYSTEM"] = null }, line =>
                           {
                               Log(line);
                               if (line.TrimStart().StartsWith("ERROR")) errors.Add(line.Trim());
                               var m = Regex.Match(line, @"^  ([A-Z][^:]{3,80})$");
                               if (m.Success && !line.Contains("ERROR")) view.StepDetail("Preparing the tools: " + m.Groups[1].Value.TrimEnd('.'));
                           });
            if (r.Cancelled) return false;
            if (rc != 0) { Fail(LastError() ?? "Preparing the tools failed."); return false; }
            RepairLinkedFiles(g);
            RepairCertificates(g);
            return true;
        }

        // The files MSYS2's packages hold as hard links to another of their files, which a drive that is not NTFS (exFAT,
        // FAT32: an external drive) cannot hold: pacman leaves them out, clang.exe among them (GitHub issue #126). The same
        // table as toolchain.ps1's ($LinkedFiles), which copies them too: here also for an older release's script, which
        // stops when clang.exe is missing before the update that brings the new one can run.
        static readonly string[,] LinkedFiles =
        {
            { "clang.exe", "clang++.exe" }, { "clang-cpp.exe", "clang++.exe" },
            { "x86_64-w64-mingw32-clang.exe", "clang++.exe" }, { "x86_64-w64-mingw32-clang++.exe", "clang++.exe" },
            { "lld.exe", "ld.exe" }, { "llvm-cov.exe", "gcov.exe" }, { "unzip.exe", "bsdunzip.exe" },
            { "libdeflate-gzip.exe", "libdeflate-gunzip.exe" },
        };

        /// <summary>The linked files missing from the private MSYS2, copied from their target (clang picks C or C++ by its
        /// own file name, so a copy works the same).</summary>
        void RepairLinkedFiles(GameDir g)
        {
            string bin = g.P(".toolchain", "msys64", "clang64", "bin");
            for (int i = 0; i < LinkedFiles.GetLength(0); i++)
            {
                string file = Path.Combine(bin, LinkedFiles[i, 0]), target = Path.Combine(bin, LinkedFiles[i, 1]);
                if (File.Exists(file) || !File.Exists(target)) continue;
                try
                {
                    File.Copy(target, file);
                    Log($"Copied {LinkedFiles[i, 1]} to {LinkedFiles[i, 0]} (MSYS2's package has it as a hard link, which this drive cannot hold)");
                }
                catch (Exception e) { Log($"Could not copy {LinkedFiles[i, 1]} to {LinkedFiles[i, 0]}: {e.Message}"); }
            }
        }

        /// <summary>The certificates the build downloads with, for a release whose toolchain.ps1 does not make them
        /// yet in a folder with a space in its path (GitHub issue #79; tools\windows\toolchain.ps1 does it since 0.5.0).</summary>
        void RepairCertificates(GameDir g)
        {
            string pem = g.P(".toolchain", "msys64", "clang64", "etc", "ssl", "cert.pem");
            if (File.Exists(pem) && new FileInfo(pem).Length >= 1024) return;
            Log("Making the certificates for the downloads");
            string own = g.P(".toolchain", "msys64", "usr", "ssl", "certs", "ca-bundle.crt");
            if (!File.Exists(own)) return;
            foreach (var dst in new[] { pem, g.P(".toolchain", "msys64", "clang64", "etc", "ssl", "certs", "ca-bundle.crt") })
            {
                Directory.CreateDirectory(Path.GetDirectoryName(dst));
                File.Copy(own, dst, true);
            }
        }

        /// <summary>Linux: the system packages the setup found missing, installed as root with pkexec (the desktop's
        /// password window) once the player agrees; when that cannot be done, the command to run in a terminal.</summary>
        bool InstallPackages(Runner r, string command)
        {
            string manual = "sudo sh -c '" + command + "'";
            if (!view.Ask("Programs to install",
                          "To build the game, a few programs from your Linux distribution are needed (they are listed under "
                          + "\"Show the details\"). Install them now? Your password is asked for.\n\n" + command))
            {
                Fail("The programs the game needs are not installed. Install them (in a terminal: " + manual + "), then click Set up.");
                return false;
            }
            if (Platform.Which("pkexec") == null)
            {
                Fail("This system has no pkexec to install programs from here. In a terminal, run:\n" + manual + "\nthen click Set up.");
                return false;
            }
            view.Step("Installing the programs the game needs");
            Log("> pkexec sh -c '" + command + "'");
            // (only the desktop's password window: pkexec's own agent would wait for the password on a terminal the
            // player cannot see, when the launcher was started from one; without a desktop agent, it fails at once)
            int rc = r.Run("pkexec", "--disable-internal-agent sh -c " + Platform.Quote(command), Game.Dir, null, Log);
            if (r.Cancelled) return false;
            if (rc != 0)
            {
                // (126: the password window was closed; 127: no password window on this desktop, or not allowed)
                Fail("The programs could not be installed" + (rc == 126 || rc == 127 ? " (no password was given)" : $" (exit code {rc})")
                     + ". In a terminal, run:\n" + manual + "\nthen click Set up.");
                return false;
            }
            Log("Installed. Running the setup again.");
            return true;
        }

        void OnSetupLine(string line)
        {
            if (line.StartsWith("@step "))
            {
                var m = Regex.Match(line, @"^@step (\d+) (\d+) (.*)$");
                if (m.Success) view.Step($"Step {m.Groups[1].Value} of {m.Groups[2].Value}: {m.Groups[3].Value}");
                sawStepMarks = true;
                return;
            }
            if (line.StartsWith("@container "))
            {
                sawContainer = true;  // (tools/setup.py: the packages are missing on an immutable system)
                return;
            }
            if (line.StartsWith("@progress "))
            {
                if (int.TryParse(line.Substring(10), out int p)) view.Progress(p);
                return;
            }
            if (!sawStepMarks)
            {
                // (a setup.py from before 0.5.0 prints no @step lines: its "[n/total] step" headings instead)
                var m = Regex.Match(line, @"^\[(\d+)/(\d+)\] (.+)$");
                if (m.Success) view.Step($"Step {m.Groups[1].Value} of {m.Groups[2].Value}: {m.Groups[3].Value}");
            }
            if (line.Contains("Fatal Python error")) sawPythonCrash = true;
            // (a required package missing: "  Missing: ..." lines, then the command that installs them all)
            if (line.TrimStart().StartsWith("Missing: ")) sawMissing = true;
            if (!Platform.Windows && sawMissing && packagesCommand == null) packagesCommand = Platform.PackageInstallCommand(line);
            if (line.TrimStart().StartsWith("ERROR")) errors.Add(line.Trim());
            else if (errors.Count > 0 && line.StartsWith("         ")) errors[errors.Count - 1] += "\n" + line.Trim();
            Log(line);
        }

        string LastError()
        {
            if (errors.Count == 0) return null;
            return Regex.Replace(errors[errors.Count - 1], @"^ERROR\s+", "");
        }

        // --- The log --------------------------------------------------------------------------------------------

        public void Fail(string msg) { Failure = msg; Log("ERROR  " + msg); }

        /// <summary>A line to the window's log, to build/launcher.log while a task runs, and to FullLog (what "copy the
        /// log" copies). Any thread.</summary>
        public void Log(string line)
        {
            lock (logLock)
            {
                try { logFile?.WriteLine(line); } catch (Exception) { }
                FullLog.Append(line).Append('\n');
                if (FullLog.Length > 4 << 20) FullLog.Remove(0, FullLog.Length - (2 << 20));
            }
            view.Log(line);
        }

        public void BeginTask(string what)
        {
            errors.Clear();
            Failure = null;
            lock (logLock)
            {
                try
                {
                    Directory.CreateDirectory(Game.P("build"));
                    logFile = new StreamWriter(Game.P("build", "launcher.log"), true, new UTF8Encoding(false)) { AutoFlush = true };
                }
                catch (Exception) { logFile = null; }
            }
            Log($"=== {what}: {DateTime.Now:yyyy-MM-dd HH:mm:ss}, launcher {VersionText(LauncherVersion)}, {Environment.OSVersion}, folder {Game.Dir}");
        }

        public void EndTask()
        {
            lock (logLock)
            {
                logFile?.Dispose();
                logFile = null;
            }
        }

        // --- The game ----------------------------------------------------------------------------------------------

        string GameLog => Game.P("build", "game.log");

        /// <summary>The game, started from its folder, its messages into build/game.log (a file, not a pipe to the
        /// launcher: the game keeps running, and writing, when the launcher is closed). The process is the game's own
        /// on Linux (sh execs it), cmd's on Windows (which ends with the game's exit code).</summary>
        public System.Diagnostics.Process StartGame()
        {
            Directory.CreateDirectory(Game.P("build"));
            // (built in the build container: the game runs in it too, through its script, ContainerPlayScript)
            string exe = Game.Exe;
            if (UsesContainer)
            {
                Platform.WriteContainerPlayScript(Game, Settings.Container);
                exe = Platform.ContainerPlayScript(Game);
            }
            string debug = Settings.DebugTools ? " --debug-tools" : "";  // (Settings > Enable debug tools)
            var psi = Platform.Windows
                ? new System.Diagnostics.ProcessStartInfo(Path.Combine(Environment.SystemDirectory, "cmd.exe"),
                      "/d /s /c \"" + Platform.Quote(Game.Exe) + debug + " > " + Platform.Quote(GameLog) + " 2>&1\"")
                : new System.Diagnostics.ProcessStartInfo("/bin/sh",
                      "-c " + Platform.Quote("exec \"$0\"" + debug + " > \"$1\" 2>&1") + " " + Platform.Quote(exe) + " " + Platform.Quote(GameLog));
            psi.WorkingDirectory = Game.Dir;
            psi.UseShellExecute = false;
            psi.CreateNoWindow = true;
            return System.Diagnostics.Process.Start(psi);
        }

        /// <summary>The setup's arguments for the movie subtitles chosen in Settings: "--movie-subtitles en,it",
        /// "--no-movie-subtitles", or "" (never chosen in the launcher: the setup keeps what it has).</summary>
        public string MovieSubtitlesArg()
        {
            string v = Settings.MovieSubtitles;
            if (string.IsNullOrEmpty(v)) return "";
            return v == "off" ? "--no-movie-subtitles" : "--movie-subtitles " + v;
        }

        /// <summary>The game's "[renderer]" line in build/game.log, as a sentence for the window: it ran with OpenGL instead of
        /// the Vulkan chosen, and why (no driver, a feature missing, the last Vulkan start crashed or lost the GPU:
        /// docs/vulkan.md#when-vulkan-cannot-start). Null when the game ran the renderer chosen.</summary>
        public string RendererFallback()
        {
            try
            {
                const string tag = "[renderer] ";
                string line = File.ReadLines(GameLog).LastOrDefault(l => l.StartsWith(tag, StringComparison.Ordinal));
                if (line == null) return null;
                string why = line.Substring(tag.Length).Trim().TrimEnd('.');
                return "The game ran with " + why + ".";
            }
            catch (Exception) { return null; }
        }

        /// <summary>What to tell the player when the game closed with an error right after starting.</summary>
        public string GameClosedMessage(int code)
        {
            string tail = "";
            try
            {
                var lines = File.ReadAllLines(GameLog);
                tail = string.Join("\n", lines.Skip(Math.Max(0, lines.Length - 10)));
            }
            catch (Exception) { }
            string why = tail.Contains("OpenGL 4.6")
                ? "The graphics driver does not offer OpenGL 4.6, which the game needs. Update the graphics driver "
                  + "(on a laptop with two graphics chips, make sure the game uses the dedicated one)."
                : "If it keeps happening, try Settings > Set up again from scratch, or report it (Settings > Report a problem).";
            return $"The game closed right after starting (exit code {code}). {why}" + (tail != "" ? "\n\nIts last messages:\n" + tail : "");
        }

        // --- The disc images ------------------------------------------------------------------------------------

        /// <summary>The disc images the setup finds and what each one is for (tools/setup.py --discs, with the disc image
        /// chosen in the settings as --iso when there is one); null when the setup cannot tell (not downloaded yet, no
        /// Python yet on Windows, or a setup.py from before --discs). Off the window's thread.</summary>
        public List<DiscEntry> Discs()
        {
            var g = Game;
            if (!g.Downloaded || !Platform.CanRunSetup(g)) return null;
            var lines = new List<string>();
            string iso = File.Exists(Settings.Iso) ? " --iso " + Platform.Quote(Settings.Iso) : "";
            int rc;
            try
            {
                using (var r = new Runner())
                    rc = r.Run(Platform.Python(g), Platform.Quote(g.P("tools", "setup.py")) + " --discs" + iso, g.Dir,
                               Platform.PythonEnv(g), l => lines.Add(l), Encoding.UTF8);
            }
            catch (Exception) { return null; }
            if (rc != 0) return null;
            var list = new List<DiscEntry>();
            foreach (var l in lines)
            {
                var f = l.Split('\t');
                if (f.Length >= 4 && (f[0] == "game" || f[0] == "data" || f[0] == "unused"))
                    list.Add(new DiscEntry { Role = f[0], Path = f[1], What = f[2], Why = f[3] });
            }
            // (the game's disc first, then the data discs, then the others, each in the setup's order)
            return list.OrderBy(d => d.Role == "game" ? 0 : d.Role == "data" ? 1 : 2).ToList();
        }

        /// <summary>assets/iso/more-discs.txt: images elsewhere the setup also looks at, one path a line (docs/setup.md,
        /// "Which disc images").</summary>
        string MoreDiscsFile => Game.P("assets", "iso", "more-discs.txt");

        /// <summary>A disc image the player added: its path listed in more-discs.txt (nothing is copied or linked); the next
        /// setup picks what it is for (a data disc removed before is taken again). Null when done, else what is wrong.</summary>
        public string AddDisc(string file)
        {
            if (!Game.Downloaded) return "Install the game first.";
            string problem = IsoProblem(file);
            if (problem != null) return problem;
            try
            {
                // (listed even when it is in assets/iso already: a data disc removed before is taken again when listed)
                string full = Path.GetFullPath(file);
                var listed = File.Exists(MoreDiscsFile) ? File.ReadAllLines(MoreDiscsFile, Encoding.UTF8).ToList() : new List<string>();
                if (listed.Any(l => string.Equals(l.Trim(), full, Platform.Windows ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)))
                    return null;
                Directory.CreateDirectory(Path.GetDirectoryName(MoreDiscsFile));
                if (listed.Count == 0) listed.Add("# disc images the setup also looks at, one path a line (added by the launcher)");
                listed.Add(full);
                File.WriteAllLines(MoreDiscsFile, listed, new UTF8Encoding(false));
                Log("Disc image added: " + full);
                return null;
            }
            catch (Exception e) { return "The disc image could not be added: " + e.Message; }
        }

        /// <summary>A data disc removed: tools/setup.py --remove-data-disc (its files deleted, not taken again from the
        /// images found), and its path out of more-discs.txt.</summary>
        public bool RemoveDataDisc(Runner r, DiscEntry d)
        {
            var g = Game;
            var m = Regex.Match(d.What, @"\b([A-Z]{4}-\d{5})\b");
            string which = m.Success ? m.Groups[1].Value : d.Path;
            Log("Removing the data disc " + d.What);
            int rc = r.Run(Platform.Python(g), Platform.Quote(g.P("tools", "setup.py")) + " --remove-data-disc " + Platform.Quote(which),
                           g.Dir, Platform.PythonEnv(g), OnSetupLine, Encoding.UTF8);
            if (r.Cancelled) return false;
            if (rc != 0) { Fail(LastError() ?? "The data disc could not be removed."); return false; }
            try
            {
                if (File.Exists(MoreDiscsFile) && d.Path != "")
                {
                    string full = Path.GetFullPath(d.Path);
                    var kept = File.ReadAllLines(MoreDiscsFile, Encoding.UTF8)
                                   .Where(l => !string.Equals(l.Trim(), full, Platform.Windows ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal));
                    File.WriteAllLines(MoreDiscsFile, kept, new UTF8Encoding(false));
                }
            }
            catch (Exception e) { Log("more-discs.txt could not be changed: " + e.Message); }
            return true;
        }

        /// <summary>A disc image's role as the window shows it.</summary>
        public static string RoleText(DiscEntry d) => d.Role == "game" ? "Game disc" : d.Role == "data" ? "Data disc" : "Not used";

        // --- Choices the window checks -------------------------------------------------------------------------

        /// <summary>Why a file cannot be the disc image (null: it can be).</summary>
        public static string IsoProblem(string file)
        {
            string ext = Path.GetExtension(file).ToLowerInvariant();
            if (new[] { ".bin", ".cue", ".chd", ".cso", ".zso", ".gz", ".7z", ".zip", ".rar", ".mdf", ".nrg" }.Contains(ext))
                return $"This is a {ext} file. The setup needs a plain .iso dump of the disc "
                     + "(a compressed image or a .bin/.cue pair does not work): convert it to .iso first.";
            if (new FileInfo(file).Length < 100L * 1024 * 1024)
                return "This file is too small to be the game's disc image.";
            return null;
        }

        /// <summary>The free space where the game goes, in GB, or -1 when it cannot be known.</summary>
        public long FreeGigabytes()
        {
            try
            {
                Directory.CreateDirectory(Game.Dir);
                if (Platform.Windows) return new DriveInfo(Path.GetPathRoot(Path.GetFullPath(Game.Dir))).AvailableFreeSpace >> 30;
                // (Linux: the mount point that holds the folder, the longest one that is a prefix of its path)
                string full = Path.GetFullPath(Game.Dir);
                var drive = DriveInfo.GetDrives().Where(d => d.IsReady && (full == d.Name || full.StartsWith(d.Name.TrimEnd('/') + "/")))
                                     .OrderByDescending(d => d.Name.Length).FirstOrDefault();
                return drive == null ? -1 : drive.AvailableFreeSpace >> 30;
            }
            catch (Exception) { return -1; }
        }

        /// <summary>The folder chosen in Settings: a game folder, or a new one (a non-empty folder gets a "rac1-native"
        /// folder inside it, when the player agrees: subfolder is set).</summary>
        public static string GameFolderFor(string chosen, out string subfolder)
        {
            subfolder = null;
            if (!new GameDir(chosen).Downloaded && Directory.Exists(chosen) && Directory.EnumerateFileSystemEntries(chosen).Any())
                subfolder = Path.Combine(chosen, "rac1-native");
            return chosen;
        }

        public void UseGameDir(string dir)
        {
            Game = new GameDir(dir);
            Settings.GameDir = dir;
            Settings.Save();
        }
    }
}
