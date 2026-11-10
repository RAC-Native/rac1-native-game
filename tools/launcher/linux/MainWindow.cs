// The Linux launcher's window: Play (install, set up, update, play), Saves and Settings, as the Windows one
// (windows/MainForm.cs). What it does is the Controller's (core/); this is how it looks, and the questions it asks.
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;

namespace Rac1Launcher
{
    sealed class MainWindow : Window, IView
    {
        readonly Controller ctl;
        GameState state = GameState.Checking;
        Runner runner;  // (the task running now, or null)
        CancellationTokenSource cancel;
        Process gameProc;
        bool closing;
        readonly ConcurrentQueue<string> pending = new ConcurrentQueue<string>();
        readonly StringBuilder shown = new StringBuilder();
        static string RunningExe => Environment.ProcessPath;
        static string Home => Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);

        // Play
        readonly TextBlock lblStatus = new TextBlock(), lblDetail = new TextBlock(), lblStep = new TextBlock(), lblUpdate = new TextBlock();
        readonly Button btnMain = new Button(), btnCancel = new Button(), btnUpdate = new Button();
        readonly Border updatePanel = new Border();
        readonly ProgressBar bar = new ProgressBar();
        readonly CheckBox chkLog = new CheckBox();
        readonly TextBox txtLog = new TextBox();
        // Saves
        readonly ListBox lstSaves = new ListBox();
        readonly TextBlock lblCard = new TextBlock();
        // Settings
        readonly TextBox txtDir = new TextBox(), txtIso = new TextBox();
        readonly ComboBox cmbJobs = new ComboBox(), cmbChannel = new ComboBox();
        readonly CheckBox chkSubs = new CheckBox();
        readonly List<CheckBox> chkSubLangs = new List<CheckBox>();
        readonly ListBox lstDiscs = new ListBox();
        readonly TextBlock lblDiscs = new TextBlock();
        List<DiscEntry> discs = new List<DiscEntry>();
        readonly TabControl tabs = new TabControl();
        readonly List<Control> busyDisabled = new List<Control>();

        static readonly IBrush Accent = new SolidColorBrush(Color.FromRgb(232, 120, 24));
        static readonly IBrush HeaderBack = new SolidColorBrush(Color.FromRgb(28, 36, 64));

        public MainWindow()
        {
            Title = "Ratchet & Clank - rac1-native";
            Width = 860;
            Height = 640;
            MinWidth = 660;
            MinHeight = 540;
            WindowStartupLocation = WindowStartupLocation.CenterScreen;
            try
            {
                using (var s = typeof(MainWindow).Assembly.GetManifestResourceStream("launcher.png"))
                    if (s != null) Icon = new WindowIcon(s);
            }
            catch (Exception) { }

            ctl = new Controller(this, RunningExe);
            ApplyTheme(ctl.Settings.Theme);
            Content = BuildUi();

            var flush = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
            flush.Tick += (s, e) => FlushLog();
            flush.Start();

            Opened += (s, e) => { RefreshState(); CheckUpdates(false); };
            Closing += OnClosing;
        }

        GameDir Game => ctl.Game;

        // --- The window -------------------------------------------------------------------------------------

        Control BuildUi()
        {
            var header = new Border { Background = HeaderBack, Padding = new Thickness(20, 12, 20, 12) };
            var hs = new StackPanel();
            hs.Children.Add(new TextBlock { Text = "Ratchet & Clank", Foreground = Brushes.White, FontSize = 26, FontWeight = FontWeight.SemiBold });
            hs.Children.Add(new TextBlock
            {
                Text = "rac1-native, the PC port   ·   launcher " + Controller.VersionText(Controller.LauncherVersion),
                Foreground = new SolidColorBrush(Color.FromRgb(190, 200, 225)), Margin = new Thickness(2, 2, 0, 0),
            });
            header.Child = hs;

            tabs.Items.Add(new TabItem { Header = "Play", Content = PlayPage() });
            tabs.Items.Add(new TabItem { Header = "Saves", Content = SavesPage() });
            tabs.Items.Add(new TabItem { Header = "Settings", Content = SettingsPage() });
            tabs.Margin = new Thickness(8, 4, 8, 8);
            tabs.SelectionChanged += (s, e) =>
            {
                if (!ReferenceEquals(e.Source, tabs)) return;
                if (tabs.SelectedIndex == 1) RefreshSaves();
                if (tabs.SelectedIndex == 2) RefreshDiscs();
            };

            var dock = new DockPanel();
            DockPanel.SetDock(header, Dock.Top);
            dock.Children.Add(header);
            dock.Children.Add(tabs);
            return dock;
        }

        static Grid Rows(string rows) => new Grid { RowDefinitions = new RowDefinitions(rows), Margin = new Thickness(12, 14, 12, 4) };

        static void Put(Grid g, Control c, int row, int col = 0)
        {
            Grid.SetRow(c, row);
            Grid.SetColumn(c, col);
            g.Children.Add(c);
        }

        Control PlayPage()
        {
            var t = Rows("Auto,Auto,Auto,Auto,Auto,Auto,Auto,*");
            lblStatus.FontSize = 20;
            lblStatus.FontWeight = FontWeight.SemiBold;
            lblStatus.Margin = new Thickness(0, 0, 0, 6);
            lblStatus.Text = "Looking at the game folder...";
            lblDetail.TextWrapping = TextWrapping.Wrap;
            lblDetail.Margin = new Thickness(0, 0, 0, 12);

            Themed(updatePanel, Border.BackgroundProperty, "LauncherNotice");
            updatePanel.Padding = new Thickness(10, 6);
            updatePanel.Margin = new Thickness(0, 0, 0, 12);
            updatePanel.CornerRadius = new CornerRadius(4);
            updatePanel.IsVisible = false;
            var up = new DockPanel();
            btnUpdate.Content = "Update";
            btnUpdate.Click += (s, e) => DoUpdate();
            DockPanel.SetDock(btnUpdate, Dock.Right);
            lblUpdate.TextWrapping = TextWrapping.Wrap;
            lblUpdate.VerticalAlignment = VerticalAlignment.Center;
            up.Children.Add(btnUpdate);
            up.Children.Add(lblUpdate);
            updatePanel.Child = up;

            var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12, Margin = new Thickness(0, 0, 0, 10) };
            btnMain.Width = 230;
            btnMain.Height = 56;
            btnMain.FontSize = 18;
            btnMain.FontWeight = FontWeight.SemiBold;
            btnMain.Background = Accent;
            btnMain.Foreground = Brushes.White;
            btnMain.HorizontalContentAlignment = HorizontalAlignment.Center;
            btnMain.VerticalContentAlignment = VerticalAlignment.Center;
            btnMain.IsEnabled = false;
            btnMain.Content = "...";
            btnMain.Click += (s, e) => MainClick();
            btnCancel.Content = "Cancel";
            btnCancel.Width = 110;
            btnCancel.Height = 56;
            btnCancel.HorizontalContentAlignment = HorizontalAlignment.Center;
            btnCancel.VerticalContentAlignment = VerticalAlignment.Center;
            btnCancel.IsVisible = false;
            btnCancel.Click += (s, e) => CancelTask();
            buttons.Children.Add(btnMain);
            buttons.Children.Add(btnCancel);

            lblStep.Margin = new Thickness(0, 4, 0, 4);
            lblStep.TextWrapping = TextWrapping.Wrap;
            bar.Height = 18;
            bar.Minimum = 0;
            bar.Maximum = 100;
            bar.IsVisible = false;
            bar.Margin = new Thickness(0, 0, 0, 10);

            chkLog.Content = "Show the details";
            chkLog.IsCheckedChanged += (s, e) => txtLog.IsVisible = chkLog.IsChecked == true;
            txtLog.IsReadOnly = true;
            txtLog.AcceptsReturn = true;
            txtLog.TextWrapping = TextWrapping.NoWrap;
            txtLog.FontFamily = new FontFamily("DejaVu Sans Mono, Liberation Mono, Noto Mono, monospace");
            txtLog.FontSize = 12;
            txtLog.Background = new SolidColorBrush(Color.FromRgb(24, 24, 28));
            txtLog.Foreground = new SolidColorBrush(Color.FromRgb(220, 220, 220));
            txtLog.IsVisible = false;
            txtLog.Margin = new Thickness(0, 6, 0, 0);

            Put(t, lblStatus, 0);
            Put(t, lblDetail, 1);
            Put(t, updatePanel, 2);
            Put(t, buttons, 3);
            Put(t, lblStep, 4);
            Put(t, bar, 5);
            Put(t, chkLog, 6);
            Put(t, txtLog, 7);
            return t;
        }

        Control SavesPage()
        {
            var t = Rows("Auto,Auto,*,Auto");
            lblCard.TextWrapping = TextWrapping.Wrap;
            lblCard.Margin = new Thickness(0, 0, 0, 8);
            lstSaves.SelectionMode = SelectionMode.Single;
            var row = Row();
            row.Children.Add(Btn("Refresh", () => RefreshSaves()));
            row.Children.Add(Btn("Open the folder", () => Platform.OpenFolder(Saves.CardDir)));
            row.Children.Add(Btn("Back up all...", () => BackupSaves()));
            row.Children.Add(Btn("Restore a backup...", () => RestoreSaves()));
            row.Children.Add(Btn("Delete the slot", () => DeleteSave()));
            row.Margin = new Thickness(0, 10, 0, 0);
            Put(t, lblCard, 0);
            Put(t, SaveRow("Disc", "Slot", "Last saved", "Size", true), 1);
            Put(t, lstSaves, 2);
            Put(t, row, 3);
            return t;
        }

        static Grid SaveRow(string disc, string slot, string saved, string size, bool heading = false)
        {
            var g = new Grid { ColumnDefinitions = new ColumnDefinitions("260,70,220,90"), Margin = heading ? new Thickness(12, 0, 0, 4) : new Thickness(0) };
            string[] cells = { disc, slot, saved, size };
            for (int i = 0; i < 4; i++)
            {
                var tb = new TextBlock { Text = cells[i], FontWeight = heading ? FontWeight.SemiBold : FontWeight.Normal,
                                         HorizontalAlignment = i == 3 ? HorizontalAlignment.Right : HorizontalAlignment.Left };
                Grid.SetColumn(tb, i);
                g.Children.Add(tb);
            }
            return g;
        }

        Control SettingsPage()
        {
            var t = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), Margin = new Thickness(12, 14, 12, 4) };
            var dirButtons = Row();
            dirButtons.Children.Add(Btn("Change...", () => ChangeGameDir(), true));
            dirButtons.Children.Add(Btn("Open", () => Platform.OpenFolder(Game.Dir)));
            txtDir.IsReadOnly = true;
            txtDir.Text = Game.Dir;
            AddRow(t, "Game folder", txtDir, dirButtons);

            txtIso.IsReadOnly = true;
            txtIso.Text = ctl.Settings.Iso;
            AddRow(t, "Disc image", txtIso, Btn("Change...", async () => { if (await AskIso(true) != null) txtIso.Text = ctl.Settings.Iso; }, true));

            cmbChannel.ItemsSource = new[] { "Stable: the releases (recommended)", "Development: the newest changes (the dev branch)" };
            cmbChannel.SelectedIndex = Channel.Development ? 1 : 0;
            cmbChannel.HorizontalAlignment = HorizontalAlignment.Stretch;
            cmbChannel.SelectionChanged += (s, e) => ChangeChannel(cmbChannel.SelectedIndex == 1);
            AddRow(t, "Update channel", cmbChannel, Note("(the same setting as the game's Main menu > Updates > Update Channel)"));

            var jobValues = new[] { 0, 1, 2, 4, 8, 16 };
            cmbJobs.ItemsSource = new[] { "Automatic", "1 (slowest, the safest)", "2", "4", "8", "16" };
            cmbJobs.SelectedIndex = Math.Max(0, Array.IndexOf(jobValues, ctl.Settings.Jobs));
            cmbJobs.Width = 220;
            cmbJobs.HorizontalAlignment = HorizontalAlignment.Left;
            cmbJobs.SelectionChanged += (s, e) => { ctl.Settings.Jobs = jobValues[Math.Max(0, cmbJobs.SelectedIndex)]; ctl.Settings.Save(); };
            AddRow(t, "Parallel jobs", cmbJobs, Note("(for the setup: fewer is slower, but uses less memory)"));

            // the movie subtitles the setup makes (docs/movie-subtitles.md) and the debug tools
            var subsPanel = new WrapPanel();
            chkSubs.Content = "Make them at the setup, in:";
            chkSubs.Margin = new Thickness(0, 0, 12, 0);
            string subs = ctl.Settings.MovieSubtitles;
            chkSubs.IsChecked = subs != "" && subs != "off";
            subsPanel.Children.Add(chkSubs);
            for (int i = 0; i < SubtitlePack.Languages.Length; i++)
            {
                var b = new CheckBox { Content = SubtitlePack.LanguageNames[i], Margin = new Thickness(0, 0, 10, 0),
                                       IsChecked = SubtitlePack.Has(subs, SubtitlePack.Languages[i]) };
                b.IsCheckedChanged += (s, e) => SaveMovieSubtitles();
                chkSubLangs.Add(b);
                subsPanel.Children.Add(b);
            }
            chkSubs.IsCheckedChanged += (s, e) => SaveMovieSubtitles();
            foreach (var b in chkSubLangs) b.IsEnabled = chkSubs.IsChecked == true;
            AddRow(t, "Movie subtitles", subsPanel, Note("(speech to text on this computer, from your disc: about 2 GB the first time, then about half an hour)"));
            var chkDebug = new CheckBox { Content = "Enable debug tools", IsChecked = ctl.Settings.DebugTools };
            chkDebug.IsCheckedChanged += (s, e) => { ctl.Settings.DebugTools = chkDebug.IsChecked == true; ctl.Settings.Save(); };
            AddRow(t, "Debug tools", chkDebug, Note("(the game starts with --debug-tools: its debug menu and overlays)"));
            var cmbTheme = new ComboBox { ItemsSource = new[] { "Follow the system", "Light", "Dark" }, Width = 220,
                                          HorizontalAlignment = HorizontalAlignment.Left };
            cmbTheme.SelectedIndex = Math.Max(0, Array.IndexOf(Themes, ctl.Settings.Theme));
            cmbTheme.SelectionChanged += (s, e) =>
            {
                ctl.Settings.Theme = Themes[Math.Max(0, cmbTheme.SelectedIndex)];
                ctl.Settings.Save();
                ApplyTheme(ctl.Settings.Theme);
            };
            AddRow(t, "Theme", cmbTheme, Note("(the launcher's colours: light or dark)"));

            var discsPanel = new StackPanel { Margin = new Thickness(0, 14, 0, 0) };
            discsPanel.Children.Add(new TextBlock { Text = "Disc images", FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 0, 0, 4) });
            lblDiscs.TextWrapping = TextWrapping.Wrap;
            Gray(lblDiscs);
            lblDiscs.Margin = new Thickness(0, 0, 0, 6);
            discsPanel.Children.Add(lblDiscs);
            discsPanel.Children.Add(DiscRow("Use", "Disc", "Image", heading: true));
            lstDiscs.SelectionMode = SelectionMode.Single;
            lstDiscs.MaxHeight = 340;
            discsPanel.Children.Add(lstDiscs);
            var discButtons = Row();
            discButtons.Margin = new Thickness(0, 6, 0, 0);
            discButtons.Children.Add(Btn("Add a disc image...", () => AddDisc(), true));
            discButtons.Children.Add(Btn("Remove the data disc", () => RemoveDisc(), true));
            discButtons.Children.Add(Btn("Refresh", () => RefreshDiscs()));
            discsPanel.Children.Add(discButtons);

            var actions = new WrapPanel { Margin = new Thickness(0, 18, 0, 0) };
            actions.Children.Add(Btn("Check for updates", () => CheckUpdates(true), true));
            actions.Children.Add(Btn("Update now", () => DoUpdate(), true));
            actions.Children.Add(Btn("Set up again from scratch", () => DoRepair(), true));
            actions.Children.Add(Btn("Make movie subtitles", () => DoMovieSubtitles(), true));
            actions.Children.Add(Btn("Install subtitle pack", () => InstallSubtitlePack(), true));
            actions.Children.Add(Btn("Add to the applications menu", () => CreateShortcuts(true), true));
            actions.Children.Add(Btn("Open the setup's logs", () => Platform.OpenFolder(Game.P("build"))));
            actions.Children.Add(Btn("Report a problem", () => Platform.OpenUrl(GitHub.Page + "/issues")));

            var note = Gray(new TextBlock
            {
                TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 18, 0, 0),
                Text = "The game's own options (graphics, controls) are in the game's Options menu. Your settings are in "
                     + Platform.ConfigDir + " and your saves in " + Platform.DataDir + ", not in the game folder: "
                     + "setting the game up again never touches them.",
            });
            var outer = new StackPanel();
            outer.Children.Add(t);
            outer.Children.Add(discsPanel);
            outer.Children.Add(actions);
            outer.Children.Add(note);
            return new ScrollViewer { Content = outer };
        }

        static TextBlock Note(string text) => Gray(new TextBlock
        {
            Text = text, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(8, 0, 0, 0),
            TextWrapping = TextWrapping.Wrap, MaxWidth = 260,
        });

        /// <summary>A property following one of the launcher's colours (App.Initialize): the light or the dark theme's.</summary>
        static T Themed<T>(T c, AvaloniaProperty property, string key) where T : Control
        {
            c.Bind(property, c.GetResourceObservable(key));
            return c;
        }

        static TextBlock Gray(TextBlock t) => Themed(t, TextBlock.ForegroundProperty, "LauncherGray");

        static readonly string[] Themes = { "", "light", "dark" };

        /// <summary>Settings > Theme: "" follows the system's light or dark setting (and its changes), else "light" or "dark";
        /// set on the application, so that the small windows (Dialogs) follow it too.</summary>
        static void ApplyTheme(string theme)
        {
            Application.Current.RequestedThemeVariant = theme == "dark" ? Avalonia.Styling.ThemeVariant.Dark
                                                      : theme == "light" ? Avalonia.Styling.ThemeVariant.Light
                                                      : Avalonia.Styling.ThemeVariant.Default;
        }

        static void AddRow(Grid t, string label, Control field, Control right)
        {
            int r = t.RowDefinitions.Count;
            t.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            var l = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 6, 14, 6) };
            field.Margin = new Thickness(0, 6, 8, 6);
            field.VerticalAlignment = VerticalAlignment.Center;
            right.VerticalAlignment = VerticalAlignment.Center;
            Put(t, l, r, 0);
            Put(t, field, r, 1);
            Put(t, right, r, 2);
        }

        static StackPanel Row() => new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };

        Button Btn(string text, Action click, bool disabledWhileBusy = false)
        {
            var b = new Button { Content = text, Margin = new Thickness(0, 3, 8, 3) };
            b.Click += (s, e) => click();
            if (disabledWhileBusy) busyDisabled.Add(b);
            return b;
        }

        Task Say(string text, string title) => Dialogs.Say(this, title, text);
        Task<bool> Yes(string text, string title) => Dialogs.Ask(this, title, text);

        // --- The state --------------------------------------------------------------------------------------

        void RefreshState()
        {
            if (runner != null) return;
            var g = Game;
            Task.Run(() =>
            {
                var st = ctl.ComputeState(out string due);
                // (a task started meanwhile keeps its own status)
                Dispatcher.UIThread.Post(() => { if (g == Game && runner == null) ShowState(st, due); });
            });
        }

        void ShowState(GameState st, string due)
        {
            state = st;
            int ch = Channel.Development ? 1 : 0;  // (the game may have changed it)
            if (cmbChannel.SelectedIndex != ch) { settingChannel = true; cmbChannel.SelectedIndex = ch; settingChannel = false; }
            btnMain.IsEnabled = runner == null;
            var (title, detail, button) = ctl.Describe(st, due);
            lblStatus.Text = title;
            lblDetail.Text = detail;
            btnMain.Content = button;
            ShowUpdate();
        }

        void ShowUpdate()
        {
            if (runner != null) return;
            string msg = ctl.UpdateMessage();
            updatePanel.IsVisible = msg != null && Game.Downloaded;
            if (msg != null) lblUpdate.Text = msg + " The update keeps your saves and settings.";
        }

        bool settingChannel;
        async void ChangeChannel(bool development)
        {
            if (settingChannel || development == Channel.Development) return;
            if (development && !await Yes(Controller.DevelopmentWarning, "Update channel"))
            {
                settingChannel = true;
                cmbChannel.SelectedIndex = 0;
                settingChannel = false;
                return;
            }
            try { Channel.Development = development; }
            catch (Exception e)
            {
                await Say("The setting could not be saved: " + e.Message, "Update channel");
                settingChannel = true;
                cmbChannel.SelectedIndex = Channel.Development ? 1 : 0;
                settingChannel = false;
                return;
            }
            ctl.DevCommit = null;
            ShowUpdate();
            CheckUpdates(false);
        }

        void CheckUpdates(bool userAsked)
        {
            Task.Run(() =>
            {
                try
                {
                    ctl.FetchUpdates();
                    Dispatcher.UIThread.Post(async () =>
                    {
                        ShowUpdate();
                        if (await OfferNewLauncher(userAsked)) return;
                        if (userAsked) await Say(ctl.UpdateSummary(), "Updates");
                    });
                }
                catch (Exception e)
                {
                    if (userAsked)
                        Dispatcher.UIThread.Post(() => Say("GitHub could not be reached (" + e.Message + ").\nCheck the internet connection.", "Updates"));
                }
            });
        }

        bool launcherDeclined;  // (the player said no to the new launcher: asked again only by Check for updates)

        /// <summary>A newer launcher, when there is one: the player is asked first, then it is downloaded (and checked)
        /// and swapped in. True when the question was asked.</summary>
        async Task<bool> OfferNewLauncher(bool userAsked)
        {
            var v = ctl.NewLauncherVersion();
            if (v == null || (launcherDeclined && !userAsked)) return false;
            if (runner != null || (gameProc != null && !gameProc.HasExited)) return false;  // (next time)
            if (!await Yes(ctl.NewLauncherQuestion(v), "Launcher update"))
            {
                launcherDeclined = true;
                return true;
            }
            string fresh = await Task.Run(() => ctl.DownloadNewLauncher(RunningExe));
            if (fresh == null)
                await Say("The new launcher could not be downloaded (see Show the details). Try again later, or download "
                          + "it from the release page.", "Launcher update");
            else if (!SwapLauncher(fresh))
                await Say("The new launcher could not replace this one now; it is tried again at the next update check.",
                          "Launcher update");
            return true;
        }

        /// <summary>The new launcher replaces this one, which then starts it. True when it did (the window closes).</summary>
        bool SwapLauncher(string fresh)
        {
            string exe = RunningExe;
            if (runner != null || (gameProc != null && !gameProc.HasExited)) return false;  // (next time)
            try { Platform.ReplaceLauncher(exe, fresh); }
            catch (Exception)
            {
                try { File.Delete(fresh); } catch (Exception) { }
                return false;
            }
            Process.Start(new ProcessStartInfo(exe) { UseShellExecute = false });
            closing = true;
            Close();
            return true;
        }

        // --- The Play button --------------------------------------------------------------------------------

        void MainClick()
        {
            switch (state)
            {
                case GameState.NotInstalled: DoInstall(); break;
                case GameState.NeedsSetup: DoSetup(""); break;
                case GameState.Ready: Play(); break;
            }
        }

        /// <summary>The disc image: the one already chosen (or the one the game uses), else asks for it.</summary>
        async Task<string> AskIso(bool always = false)
        {
            if (!always && File.Exists(ctl.Settings.Iso)) return ctl.Settings.Iso;
            if (!always)
            {
                // (the setup picks the game's disc itself among the images it finds: no --iso)
                var found = await Task.Run(() => ctl.Discs());
                if (found != null && found.Any(d => d.Role == "game")) return "";
                string known = Game.KnownIso();
                if (known != null) return known;
            }
            string file = await PickFile("Choose your Ratchet & Clank disc image (.iso)", "Disc images", new[] { "*.iso", "*.ISO" },
                                         File.Exists(ctl.Settings.Iso) ? Path.GetDirectoryName(ctl.Settings.Iso) : null);
            if (file == null) return null;
            if (!File.Exists(file)) { await Say("File not found: " + file, "Disc image"); return null; }
            string problem = Controller.IsoProblem(file);
            if (problem != null) { await Say(problem, "Disc image"); return null; }
            ctl.Settings.Iso = file;
            ctl.Settings.Save();
            return file;
        }

        /// <summary>A file chosen in the desktop's file chooser; when the desktop has none the launcher can open, its path
        /// typed in.</summary>
        async Task<string> PickFile(string title, string kind, string[] patterns, string startDir)
        {
            try
            {
                if (StorageProvider.CanOpen)
                {
                    var opts = new FilePickerOpenOptions
                    {
                        Title = title, AllowMultiple = false,
                        FileTypeFilter = new[] { new FilePickerFileType(kind) { Patterns = patterns }, FilePickerFileTypes.All },
                    };
                    opts.SuggestedStartLocation = await StorageProvider.TryGetFolderFromPathAsync(startDir ?? Home);
                    var files = await StorageProvider.OpenFilePickerAsync(opts);
                    return files.FirstOrDefault()?.TryGetLocalPath();
                }
            }
            catch (Exception) { }
            return await Dialogs.AskPath(this, title, "Type (or paste) the file's full path:", startDir);
        }

        async Task<string> PickSaveFile(string title, string suggested)
        {
            try
            {
                if (StorageProvider.CanSave)
                {
                    var f = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
                    {
                        Title = title, SuggestedFileName = suggested, DefaultExtension = "zip",
                        SuggestedStartLocation = await StorageProvider.TryGetFolderFromPathAsync(Home),
                        FileTypeChoices = new[] { new FilePickerFileType("Zip archive") { Patterns = new[] { "*.zip" } } },
                    });
                    return f?.TryGetLocalPath();
                }
            }
            catch (Exception) { }
            return await Dialogs.AskPath(this, title, "Type the full path of the zip to make:", Path.Combine(Home, suggested));
        }

        async Task<string> PickFolder(string title, string start)
        {
            try
            {
                if (StorageProvider.CanPickFolder)
                {
                    var opts = new FolderPickerOpenOptions { Title = title, AllowMultiple = false };
                    if (Directory.Exists(start)) opts.SuggestedStartLocation = await StorageProvider.TryGetFolderFromPathAsync(start);
                    var dirs = await StorageProvider.OpenFolderPickerAsync(opts);
                    return dirs.FirstOrDefault()?.TryGetLocalPath();
                }
            }
            catch (Exception) { }
            return await Dialogs.AskPath(this, title, "Type the folder's full path:", start);
        }

        async void DoInstall()
        {
            string iso = await AskIso();
            if (iso == null) return;
            long free = ctl.FreeGigabytes();
            if (free >= 0 && free < 10 &&
                !await Yes($"There are {free} GB free where the game goes; it needs about 10 GB.\n"
                           + "Install anyway? (Settings > Game folder changes where it goes.)", "Free space"))
                return;
            ctl.UseGameDir(Game.Dir);
            RunTask("Installing", r =>
            {
                if (!ctl.Download(r, cancel.Token)) return false;
                Dispatcher.UIThread.Post(() => CreateShortcuts(false));
                return ctl.Setup(r, iso, "");
            });
        }

        async void DoSetup(string extra)
        {
            string iso = await AskIso();
            if (iso == null) return;
            RunTask("Setting up", r => ctl.Setup(r, iso, extra));
        }

        async void DoRepair()
        {
            if (!Game.Downloaded) { DoInstall(); return; }
            if (!await Yes("This generates the game's code and data again from your disc image, and rebuilds the game. "
                           + "It takes as long as the first setup. Your saves and settings are kept.\n\nSet up again?", "Set up again"))
                return;
            tabs.SelectedIndex = 0;
            DoSetup("--force");
        }

        /// <summary>Settings > Movie subtitles: the choice kept, passed to the next setups (Controller.MovieSubtitlesArg).</summary>
        void SaveMovieSubtitles()
        {
            foreach (var b in chkSubLangs) b.IsEnabled = chkSubs.IsChecked == true;
            ctl.Settings.MovieSubtitles = SubtitlePack.Choice(chkSubs.IsChecked == true, chkSubLangs.Select(b => b.IsChecked == true).ToArray());
            ctl.Settings.Save();
        }

        /// <summary>The movie subtitles made now (tools/setup.py --movie-subtitles, docs/movie-subtitles.md), in the
        /// languages ticked in Settings: speech to text installed and run on the player's machine; the setup remembers
        /// it and makes only what is missing later.</summary>
        async void DoMovieSubtitles()
        {
            if (!Game.Downloaded) { DoInstall(); return; }
            string langs = SubtitlePack.Choice(true, chkSubLangs.Select(b => b.IsChecked == true).ToArray());
            if (langs == "off") langs = "all";
            if (!await Yes("This makes subtitles for the movies with speech to text (Whisper), on this computer, from your disc, in "
                     + (langs == "all" ? "every language of its sound tracks" : langs) + ". The first time it installs the "
                     + "speech to text (about 470 MB, and a 1.6 GB model); then it takes a while (about half an hour on a fast "
                     + "CPU). The next setups make only what is missing; files you edited are never written again. They show "
                     + "with Options > Language > Movie Subtitles.\n\nMake them now?", "Movie subtitles"))
                return;
            chkSubs.IsChecked = true;
            ctl.Settings.MovieSubtitles = langs;
            ctl.Settings.Save();
            tabs.SelectedIndex = 0;
            DoSetup("--movie-subtitles " + langs);
        }

        /// <summary>A movie subtitle pack (a zip of row_XXXX.<lang>.srt files, docs/movie-subtitles.md) unpacked into
        /// assets/movie-subtitles/ (core/SubtitlePack.cs); Options > Language > Movie Subtitles shows them.</summary>
        async void InstallSubtitlePack()
        {
            if (!Game.Downloaded) { DoInstall(); return; }
            string zip = await PickFile("Choose a movie subtitle pack (.zip)", "Zip archive", new[] { "*.zip", "*.ZIP" }, null);
            if (zip == null) return;
            try
            {
                var (n, skipped) = SubtitlePack.Install(Game, zip);
                await Say($"{n} subtitle files installed into\n{SubtitlePack.Dir(Game)}"
                          + (skipped > 0 ? $"\n({skipped} other files in the zip left out)" : "")
                          + "\n\nIn the game: Options > Language > Movie Subtitles: On. They follow the text language.",
                          "Movie subtitles");
            }
            catch (Exception e) { await Say("The pack could not be installed: " + e.Message, "Movie subtitles"); }
        }

        async void DoUpdate()
        {
            if (!Game.Downloaded) { DoInstall(); return; }
            string iso = await AskIso();
            if (iso == null) return;
            tabs.SelectedIndex = 0;
            RunTask("Updating", r => ctl.Setup(r, iso, "--update --channel " + Channel.Name));
        }

        void Play()
        {
            if (gameProc != null && !gameProc.HasExited) return;
            try
            {
                gameProc = ctl.StartGame();
            }
            catch (Exception e)
            {
                _ = Say("The game could not be started (" + e.Message + ").", "Play");
                return;
            }
            var started = DateTime.Now;
            btnMain.IsEnabled = false;
            btnMain.Content = "Playing...";
            WindowState = WindowState.Minimized;
            gameProc.EnableRaisingEvents = true;
            gameProc.Exited += (s, e) => Dispatcher.UIThread.Post(async () =>
            {
                int code = gameProc.ExitCode;
                WindowState = WindowState.Normal;
                Activate();
                btnMain.IsEnabled = true;
                ShowState(state, "");
                ShowRendererFallback();
                CheckUpdates(false);  // (the channel may have changed in the game's options)
                if (code != 0 && (DateTime.Now - started).TotalSeconds < 20)
                    await Say(ctl.GameClosedMessage(code), "Play");
            });
        }

        /// <summary>After the game: the line saying it ran with OpenGL instead of the Vulkan chosen, and why (build/game.log's
        /// "[renderer]" line), under the status, and in the log.</summary>
        void ShowRendererFallback()
        {
            string fb = ctl.RendererFallback();
            if (fb == null) return;
            lblDetail.Text = fb;
            ctl.Log(fb);
        }

        // --- Running a task ---------------------------------------------------------------------------------

        void RunTask(string what, Func<Runner, bool> work)
        {
            if (runner != null) return;
            runner = new Runner();
            cancel = new CancellationTokenSource();
            tabs.SelectedIndex = 0;
            btnMain.IsEnabled = false;
            btnMain.Content = what + "...";
            lblStatus.Text = what + "...";
            lblDetail.Text = "This takes a while (the first time, often 20 to 40 minutes). You can use the computer meanwhile; "
                           + "keep this window open. Cancel stops it, and the next run continues from where it stopped.";
            btnCancel.IsVisible = true;
            btnCancel.IsEnabled = true;
            bar.IsVisible = true;
            bar.IsIndeterminate = true;
            updatePanel.IsVisible = false;
            foreach (var c in busyDisabled) c.IsEnabled = false;
            ctl.BeginTask(what);
            var r = runner;
            Task.Run(() =>
            {
                bool ok;
                try { ok = work(r); }
                catch (Exception e) { ctl.Fail("Something went wrong: " + e.Message); ctl.Log(e.ToString()); ok = false; }
                Dispatcher.UIThread.Post(() => TaskDone(ok, r.Cancelled));
            });
        }

        async void TaskDone(bool ok, bool cancelled)
        {
            FlushLog();
            ctl.EndTask();
            runner.Dispose();
            runner = null;
            btnCancel.IsVisible = false;
            bar.IsVisible = false;
            foreach (var c in busyDisabled) c.IsEnabled = true;
            lblStep.Text = cancelled ? "Cancelled." : ok ? "" : lblStep.Text;
            if (closing) { Close(); return; }
            RefreshState();
            if (ok)
            {
                if (await Yes("Ratchet & Clank is ready. Start the game now?", "Ready"))
                {
                    state = GameState.Ready;
                    Play();
                }
            }
            else if (!cancelled)
            {
                string msg = ctl.Failure ?? "The setup failed.";
                chkLog.IsChecked = true;
                if (await Dialogs.Ask(this, "Something went wrong",
                                      msg + "\n\nYour saves and settings are safe. You can try again with the button on the Play page."
                                      + "\n\nCopy the full log, for a bug report on GitHub?", "Copy the log", "Close"))
                {
                    try { await Clipboard.SetTextAsync(ctl.FullLog.ToString()); } catch (Exception) { }
                    Platform.OpenUrl(GitHub.Page + "/issues");
                }
            }
        }

        async void CancelTask()
        {
            if (runner == null) return;
            if (!await Yes("Stop now? What was done so far is kept: the next run continues from there.", "Cancel")) return;
            btnCancel.IsEnabled = false;
            cancel?.Cancel();
            runner?.Cancel();
        }

        async void OnClosing(object sender, WindowClosingEventArgs e)
        {
            if (runner == null || closing) return;
            e.Cancel = true;
            if (!await Yes("The setup is still running. Stop it and close? It continues from where it stopped next time.", "Close"))
                return;
            closing = true;  // (TaskDone closes the window once the programs have ended)
            cancel?.Cancel();
            runner?.Cancel();
        }

        // --- IView: the work's progress, from its thread ------------------------------------------------------

        public void Step(string text) => Dispatcher.UIThread.Post(() =>
        {
            lblStep.Text = text;
            bar.IsIndeterminate = true;
        });

        public void StepDetail(string text) => Dispatcher.UIThread.Post(() => lblStep.Text = text);

        public void Progress(int pct) => Dispatcher.UIThread.Post(() =>
        {
            bar.IsIndeterminate = false;
            bar.Value = Math.Max(0, Math.Min(100, pct));
        });

        public void Log(string line) => pending.Enqueue(line);

        public bool Ask(string title, string text) =>
            Dispatcher.UIThread.InvokeAsync(() => Dialogs.Ask(this, title, text)).GetAwaiter().GetResult();

        void FlushLog()
        {
            if (pending.IsEmpty) return;
            while (pending.TryDequeue(out string line)) shown.Append(line).Append('\n');
            if (shown.Length > 1 << 20) shown.Remove(0, shown.Length - (512 << 10));
            txtLog.Text = shown.ToString();
            txtLog.CaretIndex = txtLog.Text.Length;  // (scrolled to the end)
        }

        // --- Disc images -------------------------------------------------------------------------------------

        static Control DiscRow(string use, string what, string image, string why = null, bool heading = false)
        {
            var g = new Grid { ColumnDefinitions = new ColumnDefinitions("100,290,*"), Margin = heading ? new Thickness(12, 0, 0, 4) : new Thickness(0) };
            string[] cells = { use, what, image };
            for (int i = 0; i < 3; i++)
            {
                var tb = new TextBlock { Text = cells[i], FontWeight = heading || (i == 0 && use != "Not used") ? FontWeight.SemiBold : FontWeight.Normal,
                                         TextTrimming = TextTrimming.CharacterEllipsis, Margin = new Thickness(0, 0, 8, 0) };
                if (!heading && use == "Not used") Gray(tb);
                Grid.SetColumn(tb, i);
                g.Children.Add(tb);
            }
            if (why == null) return g;
            var both = new StackPanel();
            both.Children.Add(g);
            both.Children.Add(Gray(new TextBlock { Text = why, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(100, 0, 0, 0) }));
            return both;
        }

        /// <summary>The disc images the setup finds (tools/setup.py --discs), and what each one is for.</summary>
        void RefreshDiscs()
        {
            var g = Game;
            lblDiscs.Text = "Looking at the disc images...";
            Task.Run(() =>
            {
                var list = ctl.Discs();
                Dispatcher.UIThread.Post(() =>
                {
                    if (g != Game) return;
                    discs = list ?? new List<DiscEntry>();
                    lstDiscs.ItemsSource = discs.Select(d => DiscRow(Controller.RoleText(d), d.What, d.Path == "" ? "(its image is gone)" : Path.GetFileName(d.Path),
                                                                     d.Why + (d.Path == "" ? "" : "\n" + d.Path))).ToList();
                    lblDiscs.Text = list == null
                        ? "The list shows once the game is installed (and, on a game folder set up by hand, its setup can run)."
                        : "The setup picks the game's disc and the data discs among the images in the game folder's assets/iso "
                          + "folder and the ones added here (nothing is copied). A data disc adds something the game's disc lacks: "
                          + "the Japanese disc, its voices." + (discs.Count == 0 ? " No disc image found yet." : "");
                });
            });
        }

        async void AddDisc()
        {
            if (!Game.Downloaded) { await Say("Install the game first.", "Disc images"); return; }
            string file = await PickFile("Add a Ratchet & Clank disc image (.iso)", "Disc images", new[] { "*.iso", "*.ISO" }, null);
            if (file == null) return;
            if (!File.Exists(file)) { await Say("File not found: " + file, "Disc images"); return; }
            string problem = ctl.AddDisc(file);
            if (problem != null) { await Say(problem, "Disc images"); return; }
            var list = await Task.Run(() => ctl.Discs());
            RefreshDiscs();
            RefreshState();
            string full = Path.GetFullPath(file);
            var d = list?.FirstOrDefault(x => x.Path == full);
            await Say(d == null ? "Added: the next setup looks at it."
                      : d.Role == "data" ? $"{d.What}: a data disc ({d.Why}). The setup adds it at its next run: click Set up on the Play page."
                      : d.Role == "game" ? $"{d.What}: the game's disc." : $"{d.What}: not used ({d.Why}).", "Disc images");
        }

        async void RemoveDisc()
        {
            if (runner != null) return;
            var d = lstDiscs.SelectedIndex >= 0 && lstDiscs.SelectedIndex < discs.Count ? discs[lstDiscs.SelectedIndex] : null;
            if (d == null || d.Role != "data") { await Say("Select a data disc in the list first.", "Disc images"); return; }
            if (!await Yes($"Remove the data disc {d.What}? Its files in the game folder are deleted (your disc image is kept), "
                           + "and the setup no longer takes it, until it is added again here.", "Disc images"))
                return;
            foreach (var c in busyDisabled) c.IsEnabled = false;
            ctl.BeginTask("Removing a data disc");
            bool ok = await Task.Run(() => { using (var r = new Runner()) return ctl.RemoveDataDisc(r, d); });
            ctl.EndTask();
            foreach (var c in busyDisabled) c.IsEnabled = true;
            FlushLog();
            if (!ok) await Say(ctl.Failure ?? "The data disc could not be removed.", "Disc images");
            RefreshDiscs();
            RefreshState();
        }

        // --- Saves ------------------------------------------------------------------------------------------

        void RefreshSaves()
        {
            lblCard.Text = "Your saves are in " + Saves.CardDir + ". Back them up before trying something new; "
                         + "a deleted slot goes to " + Platform.TrashName + ".";
            var items = new List<ListBoxItem>();
            try
            {
                foreach (var s in Saves.List())
                    items.Add(new ListBoxItem { Content = SaveRow(s.Region, s.Slot.ToString(), s.Saved.ToString("g"), $"{(s.Size + 1023) / 1024} KB"), Tag = s });
            }
            catch (Exception e) { lblCard.Text += "\n(" + e.Message + ")"; }
            if (items.Count == 0) items.Add(new ListBoxItem { Content = new TextBlock { Text = "No saves yet" }, IsEnabled = false });
            lstSaves.ItemsSource = items;
        }

        async void BackupSaves()
        {
            if (!Directory.Exists(Saves.CardDir)) { await Say("There are no saves yet.", "Saves"); return; }
            string zip = await PickSaveFile("Back up the saves", $"rac1-saves-{DateTime.Now:yyyy-MM-dd}.zip");
            if (zip == null) return;
            try
            {
                Saves.Backup(zip);
                await Say("The saves were backed up to\n" + zip, "Saves");
            }
            catch (Exception e) { await Say("The backup failed: " + e.Message, "Saves"); }
        }

        async void RestoreSaves()
        {
            if (gameProc != null && !gameProc.HasExited) { await Say("Close the game first.", "Saves"); return; }
            string zip = await PickFile("Restore saves from a backup", "Zip archive", new[] { "*.zip" }, null);
            if (zip == null) return;
            if (!await Yes("The saves in the backup replace the ones with the same slot now. "
                           + "The saves as they are now are backed up first, into\n" + Saves.BackupDir + "\n\nRestore?", "Saves"))
                return;
            try
            {
                Saves.Restore(zip);
                RefreshSaves();
                await Say("The saves were restored.", "Saves");
            }
            catch (Exception e) { await Say("The restore failed: " + e.Message, "Saves"); }
        }

        async void DeleteSave()
        {
            if (!(lstSaves.SelectedItem is ListBoxItem item) || !(item.Tag is SaveSlot s))
            {
                await Say("Choose a slot in the list first.", "Saves");
                return;
            }
            if (gameProc != null && !gameProc.HasExited) { await Say("Close the game first.", "Saves"); return; }
            if (!await Yes($"Delete slot {s.Slot} ({s.Region}, saved {s.Saved:g})? It goes to {Platform.TrashName}.", "Saves")) return;
            try { Saves.Delete(s); }
            catch (Exception e) { await Say("The slot could not be deleted: " + e.Message, "Saves"); }
            RefreshSaves();
        }

        // --- Settings ---------------------------------------------------------------------------------------

        async void ChangeGameDir()
        {
            string chosen = await PickFolder("Choose the game's folder: an empty folder for a new install, or a folder that already has the game",
                                             Game.Dir);
            if (string.IsNullOrEmpty(chosen)) return;
            string dir = Controller.GameFolderFor(chosen, out string sub);
            if (sub != null)
            {
                if (!await Yes("This folder is not empty. Install into a new folder inside it?\n" + sub, "Game folder")) return;
                dir = sub;
            }
            ctl.UseGameDir(dir);
            txtDir.Text = dir;
            RefreshState();
        }

        /// <summary>The launcher copied into the game folder, and its entry in the applications menu
        /// (Platform.InstallLauncher).</summary>
        async void CreateShortcuts(bool userAsked)
        {
            try
            {
                Platform.InstallLauncher(Game, RunningExe, false);
                if (userAsked) await Say("Ratchet & Clank is in your applications menu (it may take a moment to show).", "Applications menu");
            }
            catch (Exception e)
            {
                if (userAsked) await Say("The menu entry could not be made: " + e.Message, "Applications menu");
                else ctl.Log("The menu entry could not be made: " + e.Message);
            }
        }
    }
}
