// The Windows launcher's window: Play (install, set up, update, play), Saves and Settings. What it does is the
// Controller's (core/); this is how it looks, and the questions it asks. Everything long (downloads, the setup) runs in
// the background with its progress shown; the setup's own messages are under "Show the details".
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Rac1Launcher
{
    sealed class MainForm : Form, IView
    {
        readonly Controller ctl;
        GameState state = GameState.Checking;
        Runner runner;  // (the task running now, or null)
        CancellationTokenSource cancel;
        Process gameProc;
        readonly ConcurrentQueue<string> pending = new ConcurrentQueue<string>();

        // Play
        readonly Label lblStatus = new Label(), lblDetail = new Label(), lblStep = new Label(), lblUpdate = new Label();
        readonly Button btnMain = new Button(), btnCancel = new Button(), btnUpdate = new Button();
        readonly Panel updatePanel = new Panel();
        readonly ProgressBar bar = new ProgressBar();
        readonly CheckBox chkLog = new CheckBox();
        readonly TextBox txtLog = new TextBox();
        // Saves
        readonly ListView lstSaves = new ListView();
        readonly Label lblCard = new Label();
        // Settings
        readonly TextBox txtDir = new TextBox(), txtIso = new TextBox();
        readonly ComboBox cmbJobs = new ComboBox(), cmbChannel = new ComboBox();
        readonly TabControl tabs = new TabControl();
        readonly List<Control> busyDisabled = new List<Control>();

        static readonly Color Accent = Color.FromArgb(232, 120, 24);
        static readonly Color HeaderBack = Color.FromArgb(28, 36, 64);

        public MainForm()
        {
            Text = "Ratchet & Clank - rac1-native";
            AutoScaleMode = AutoScaleMode.None;  // (scaled by hand: Sz, Pd, Pt, S)
            using (var g = CreateGraphics()) k = g.DpiX / 96f;  // (DeviceDpi is 96 on .NET Framework without an .exe.config)
            Font = new Font("Segoe UI", 9.75f);
            ClientSize = Sz(820, 600);
            MinimumSize = Sz(640, 520);
            StartPosition = FormStartPosition.CenterScreen;
            try
            {
                using (var s = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher.ico"))
                    if (s != null) Icon = new Icon(s);
            }
            catch (Exception) { }

            ctl = new Controller(this, Application.ExecutablePath);
            BuildUi();
            NoMnemonics(this);

            var flush = new System.Windows.Forms.Timer { Interval = 120 };
            flush.Tick += (s, e) => FlushLog();
            flush.Start();

            Shown += (s, e) => { RefreshState(); CheckUpdates(false); };
            FormClosing += OnClosing;
        }

        GameDir Game => ctl.Game;

        // --- The window -------------------------------------------------------------------------------------

        void BuildUi()
        {
            var header = new Panel { Dock = DockStyle.Top, Height = S(78), BackColor = HeaderBack, Padding = Pd(18, 10, 18, 8) };
            var title = new Label
            {
                Text = "Ratchet & Clank", ForeColor = Color.White, AutoSize = true, Location = Pt(16, 10),
                Font = new Font("Segoe UI Semibold", 20f),
            };
            var sub = new Label
            {
                Text = "rac1-native, the PC port   ·   launcher " + Controller.VersionText(Controller.LauncherVersion),
                ForeColor = Color.FromArgb(190, 200, 225), AutoSize = true, Location = Pt(19, 50),
            };
            header.Controls.Add(title);
            header.Controls.Add(sub);

            tabs.Dock = DockStyle.Fill;
            tabs.Padding = Pt(14, 5);
            tabs.TabPages.Add(PlayPage());
            tabs.TabPages.Add(SavesPage());
            tabs.TabPages.Add(SettingsPage());
            tabs.SelectedIndexChanged += (s, e) => { if (tabs.SelectedIndex == 1) RefreshSaves(); };
            Controls.Add(tabs);
            Controls.Add(header);
        }

        TabPage PlayPage()
        {
            var page = new TabPage("Play") { Padding = Pd(18), UseVisualStyleBackColor = true };
            var t = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 8 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            for (int i = 0; i < 7; i++) t.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            t.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            lblStatus.AutoSize = true;
            lblStatus.Font = new Font("Segoe UI Semibold", 15f);
            lblStatus.Margin = Pd(0, 0, 0, 6);
            lblStatus.Text = "Looking at the game folder...";
            lblDetail.AutoSize = true;
            lblDetail.Anchor = AnchorStyles.Left | AnchorStyles.Right;
            lblDetail.Margin = Pd(0, 0, 0, 12);

            updatePanel.BackColor = Color.FromArgb(255, 244, 214);
            updatePanel.Dock = DockStyle.Top;
            updatePanel.Height = S(46);
            updatePanel.Padding = Pd(10, 8, 10, 8);
            updatePanel.Margin = Pd(0, 0, 0, 12);
            updatePanel.Visible = false;
            lblUpdate.AutoSize = false;
            lblUpdate.Dock = DockStyle.Fill;
            lblUpdate.TextAlign = ContentAlignment.MiddleLeft;
            btnUpdate.Text = "Update";
            btnUpdate.Dock = DockStyle.Right;
            btnUpdate.Width = S(110);
            btnUpdate.Click += (s, e) => DoUpdate();
            updatePanel.Controls.Add(lblUpdate);
            updatePanel.Controls.Add(btnUpdate);

            var buttons = new FlowLayoutPanel { AutoSize = true, Margin = Pd(0, 0, 0, 10), WrapContents = false };
            btnMain.Size = Sz(230, 56);
            btnMain.Font = new Font("Segoe UI Semibold", 13f);
            btnMain.BackColor = Accent;
            btnMain.ForeColor = Color.White;
            btnMain.FlatStyle = FlatStyle.Flat;
            btnMain.FlatAppearance.BorderSize = 0;
            btnMain.Enabled = false;
            btnMain.Text = "...";
            btnMain.Click += (s, e) => MainClick();
            btnCancel.Text = "Cancel";
            btnCancel.Size = Sz(110, 56);
            btnCancel.Margin = Pd(12, 3, 3, 3);
            btnCancel.Visible = false;
            btnCancel.Click += (s, e) => CancelTask();
            buttons.Controls.Add(btnMain);
            buttons.Controls.Add(btnCancel);

            lblStep.AutoSize = true;
            lblStep.Margin = Pd(0, 4, 0, 4);
            bar.Dock = DockStyle.Top;
            bar.Height = S(18);
            bar.Visible = false;
            bar.Margin = Pd(0, 0, 0, 10);

            chkLog.Text = "Show the details";
            chkLog.AutoSize = true;
            chkLog.CheckedChanged += (s, e) => txtLog.Visible = chkLog.Checked;
            txtLog.Multiline = true;
            txtLog.ReadOnly = true;
            txtLog.ScrollBars = ScrollBars.Both;
            txtLog.WordWrap = false;
            txtLog.Font = new Font("Consolas", 9f);
            txtLog.Dock = DockStyle.Fill;
            txtLog.BackColor = Color.FromArgb(24, 24, 28);
            txtLog.ForeColor = Color.FromArgb(220, 220, 220);
            txtLog.Visible = false;
            txtLog.MaxLength = 0;

            t.Controls.Add(lblStatus, 0, 0);
            t.Controls.Add(lblDetail, 0, 1);
            t.Controls.Add(updatePanel, 0, 2);
            t.Controls.Add(buttons, 0, 3);
            t.Controls.Add(lblStep, 0, 4);
            t.Controls.Add(bar, 0, 5);
            t.Controls.Add(chkLog, 0, 6);
            t.Controls.Add(txtLog, 0, 7);
            page.Controls.Add(t);
            return page;
        }

        TabPage SavesPage()
        {
            var page = new TabPage("Saves") { Padding = Pd(18), UseVisualStyleBackColor = true };
            var t = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            t.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            t.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            t.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            lblCard.AutoSize = true;
            lblCard.Anchor = AnchorStyles.Left | AnchorStyles.Right;
            lblCard.Margin = Pd(0, 0, 0, 8);
            lstSaves.View = View.Details;
            lstSaves.FullRowSelect = true;
            lstSaves.MultiSelect = false;
            lstSaves.HideSelection = false;
            lstSaves.Dock = DockStyle.Fill;
            lstSaves.Columns.Add("Disc", S(240));
            lstSaves.Columns.Add("Slot", S(60));
            lstSaves.Columns.Add("Last saved", S(200));
            lstSaves.Columns.Add("Size", S(90), HorizontalAlignment.Right);
            var row = new FlowLayoutPanel { AutoSize = true, Margin = Pd(0, 10, 0, 0) };
            row.Controls.Add(Btn("Refresh", (s, e) => RefreshSaves()));
            row.Controls.Add(Btn("Open the folder", (s, e) => Platform.OpenFolder(Saves.CardDir)));
            row.Controls.Add(Btn("Back up all...", (s, e) => BackupSaves()));
            row.Controls.Add(Btn("Restore a backup...", (s, e) => RestoreSaves()));
            row.Controls.Add(Btn("Delete the slot", (s, e) => DeleteSave()));
            t.Controls.Add(lblCard, 0, 0);
            t.Controls.Add(lstSaves, 0, 1);
            t.Controls.Add(row, 0, 2);
            page.Controls.Add(t);
            return page;
        }

        TabPage SettingsPage()
        {
            var page = new TabPage("Settings") { Padding = Pd(18), UseVisualStyleBackColor = true, AutoScroll = true };
            var t = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 3 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

            txtDir.ReadOnly = true;
            txtDir.Dock = DockStyle.Fill;
            txtDir.Text = Game.Dir;
            var dirButtons = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
            dirButtons.Controls.Add(Btn("Change...", (s, e) => ChangeGameDir(), true));
            dirButtons.Controls.Add(Btn("Open", (s, e) => Platform.OpenFolder(Game.Dir)));
            AddRow(t, "Game folder", txtDir, dirButtons);

            txtIso.ReadOnly = true;
            txtIso.Dock = DockStyle.Fill;
            txtIso.Text = ctl.Settings.Iso;
            AddRow(t, "Disc image", txtIso, Btn("Change...", (s, e) => { if (AskIso(true) != null) txtIso.Text = ctl.Settings.Iso; }, true));

            cmbChannel.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbChannel.Items.AddRange(new object[] { "Stable: the releases (recommended)", "Development: the newest changes on main" });
            cmbChannel.SelectedIndex = Channel.Development ? 1 : 0;
            cmbChannel.SelectedIndexChanged += (s, e) => ChangeChannel(cmbChannel.SelectedIndex == 1);
            AddRow(t, "Update channel", cmbChannel, new Label
            {
                Text = "(the same setting as the game's Main menu > Updates > Update Channel)", AutoSize = true, Anchor = AnchorStyles.Left,
                ForeColor = SystemColors.GrayText,
            });

            cmbJobs.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbJobs.Items.AddRange(new object[] { "Automatic", "1 (slowest, the safest)", "2", "4", "8", "16" });
            var jobValues = new[] { 0, 1, 2, 4, 8, 16 };
            cmbJobs.SelectedIndex = Math.Max(0, Array.IndexOf(jobValues, ctl.Settings.Jobs));
            cmbJobs.Width = S(200);
            cmbJobs.SelectedIndexChanged += (s, e) => { ctl.Settings.Jobs = jobValues[cmbJobs.SelectedIndex]; ctl.Settings.Save(); };
            AddRow(t, "Parallel jobs", cmbJobs, new Label
            {
                Text = "(for the setup: fewer is slower, but uses less memory)", AutoSize = true, Anchor = AnchorStyles.Left,
                ForeColor = SystemColors.GrayText,
            });

            var actions = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Margin = Pd(0, 18, 0, 0) };
            actions.Controls.Add(Btn("Check for updates", (s, e) => CheckUpdates(true), true));
            actions.Controls.Add(Btn("Update now", (s, e) => DoUpdate(), true));
            actions.Controls.Add(Btn("Set up again from scratch", (s, e) => DoRepair(), true));
            actions.Controls.Add(Btn("Create the shortcuts", (s, e) => CreateShortcuts(true), true));
            actions.Controls.Add(Btn("Open the setup's logs", (s, e) => Platform.OpenFolder(Game.P("build"))));
            actions.Controls.Add(Btn("Report a problem", (s, e) => Platform.OpenUrl(GitHub.Page + "/issues")));

            var note = new Label
            {
                AutoSize = true, Anchor = AnchorStyles.Left | AnchorStyles.Right, Margin = Pd(0, 18, 0, 0), ForeColor = SystemColors.GrayText,
                Text = "The game's own options (graphics, controls) are in the game's Options menu. "
                     + "Your settings and saves are in " + Platform.ConfigDir + ", not in the game folder: "
                     + "setting the game up again never touches them.",
            };
            var outer = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1 };
            outer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            outer.Controls.Add(t, 0, 0);
            outer.Controls.Add(actions, 0, 1);
            outer.Controls.Add(note, 0, 2);
            page.Controls.Add(outer);
            return page;
        }

        void AddRow(TableLayoutPanel t, string label, Control field, Control right)
        {
            int r = t.RowCount++;
            t.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            t.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left, Margin = Pd(0, 8, 12, 8) }, 0, r);
            field.Anchor = AnchorStyles.Left | AnchorStyles.Right;
            field.Margin = Pd(0, 6, 8, 6);
            t.Controls.Add(field, 1, r);
            t.Controls.Add(right, 2, r);
        }

        Button Btn(string text, EventHandler click, bool disabledWhileBusy = false)
        {
            var b = new Button { Text = text, AutoSize = true, Padding = Pd(8, 3, 8, 3), Margin = Pd(0, 3, 8, 3) };
            b.Click += click;
            if (disabledWhileBusy) busyDisabled.Add(b);
            return b;
        }

        /// <summary>"&" shown as itself in every label ("Ratchet & Clank"), not as a keyboard shortcut's mark.</summary>
        static void NoMnemonics(Control c)
        {
            if (c is Label l) l.UseMnemonic = false;
            foreach (Control child in c.Controls) NoMnemonics(child);
        }

        // Sizes in pixels at 96 DPI made the screen's (the manifest makes the launcher DPI-aware; the fonts, in points,
        // already follow the screen)
        readonly float k = 1f;
        int S(int px) => (int)Math.Round(px * k);
        Size Sz(int w, int h) => new Size(S(w), S(h));
        Padding Pd(int l, int t, int r, int b) => new Padding(S(l), S(t), S(r), S(b));
        Padding Pd(int all) => new Padding(S(all));
        Point Pt(int x, int y) => new Point(S(x), S(y));

        bool Yes(string text, string title, MessageBoxIcon icon = MessageBoxIcon.Question) =>
            MessageBox.Show(this, text, title, MessageBoxButtons.YesNo, icon) == DialogResult.Yes;

        void Say(string text, string title, MessageBoxIcon icon = MessageBoxIcon.Information) =>
            MessageBox.Show(this, text, title, MessageBoxButtons.OK, icon);

        // --- The state --------------------------------------------------------------------------------------

        void RefreshState()
        {
            if (runner != null) return;
            var g = Game;
            Task.Run(() =>
            {
                var st = ctl.ComputeState(out string due);
                // (a task started meanwhile keeps its own status)
                BeginInvoke((Action)(() => { if (g == Game && runner == null) ShowState(st, due); }));
            });
        }

        void ShowState(GameState st, string due)
        {
            state = st;
            cmbChannel.SelectedIndex = Channel.Development ? 1 : 0;  // (the game may have changed it)
            btnMain.Enabled = runner == null;
            var (title, detail, button) = ctl.Describe(st, due);
            lblStatus.Text = title;
            lblDetail.Text = detail;
            btnMain.Text = button;
            ShowUpdate();
        }

        void ShowUpdate()
        {
            if (runner != null) return;
            string msg = ctl.UpdateMessage();
            updatePanel.Visible = msg != null && Game.Downloaded;
            if (msg != null) lblUpdate.Text = msg + " The update keeps your saves and settings.";
        }

        void ChangeChannel(bool development)
        {
            if (development == Channel.Development) return;
            if (development && !Yes(Controller.DevelopmentWarning, "Update channel"))
            {
                cmbChannel.SelectedIndex = 0;
                return;
            }
            try { Channel.Development = development; }
            catch (Exception e)
            {
                Say("The setting could not be saved: " + e.Message, "Update channel", MessageBoxIcon.Error);
                cmbChannel.SelectedIndex = Channel.Development ? 1 : 0;
                return;
            }
            ctl.MainCommit = null;
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
                    string fresh = ctl.DownloadNewLauncher(Application.ExecutablePath);
                    BeginInvoke((Action)(() =>
                    {
                        ShowUpdate();
                        if (fresh != null && SwapLauncher(fresh)) return;
                        if (userAsked) Say(ctl.UpdateSummary(), "Updates");
                    }));
                }
                catch (Exception e)
                {
                    if (userAsked)
                        BeginInvoke((Action)(() => Say("GitHub could not be reached (" + e.Message + ").\nCheck the internet connection.",
                                                       "Updates", MessageBoxIcon.Warning)));
                }
            });
        }

        /// <summary>The new launcher replaces this one, which then starts it. True when it did (the window closes).</summary>
        bool SwapLauncher(string fresh)
        {
            string exe = Application.ExecutablePath;
            if (runner != null || (gameProc != null && !gameProc.HasExited)) return false;  // (next time)
            try { Platform.ReplaceLauncher(exe, fresh); }
            catch (Exception)
            {
                try { File.Delete(fresh); } catch (Exception) { }
                return false;
            }
            Process.Start(new ProcessStartInfo(exe) { UseShellExecute = false });
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
        string AskIso(bool always = false)
        {
            if (!always && File.Exists(ctl.Settings.Iso)) return ctl.Settings.Iso;
            if (!always)
            {
                string known = Game.KnownIso();
                if (known != null) return known;
            }
            using (var d = new OpenFileDialog
            {
                Title = "Choose your Ratchet & Clank disc image (.iso)",
                Filter = "Disc images (*.iso)|*.iso|All files (*.*)|*.*",
                CheckFileExists = true,
            })
            {
                if (File.Exists(ctl.Settings.Iso)) d.InitialDirectory = Path.GetDirectoryName(ctl.Settings.Iso);
                if (d.ShowDialog(this) != DialogResult.OK) return null;
                string problem = Controller.IsoProblem(d.FileName);
                if (problem != null)
                {
                    Say(problem, "Disc image", MessageBoxIcon.Warning);
                    return null;
                }
                ctl.Settings.Iso = d.FileName;
                ctl.Settings.Save();
                return d.FileName;
            }
        }

        void DoInstall()
        {
            string iso = AskIso();
            if (iso == null) return;
            long free = ctl.FreeGigabytes();
            if (free >= 0 && free < 10 &&
                !Yes($"There are {free} GB free where the game goes; it needs about 10 GB.\n"
                     + "Install anyway? (Settings > Game folder changes where it goes.)", "Free space", MessageBoxIcon.Warning))
                return;
            ctl.UseGameDir(Game.Dir);
            RunTask("Installing", r =>
            {
                if (!ctl.Download(r, cancel.Token)) return false;
                BeginInvoke((Action)(() => CreateShortcuts(false)));
                return ctl.Setup(r, iso, "");
            });
        }

        void DoSetup(string extra)
        {
            string iso = AskIso();
            if (iso == null) return;
            RunTask("Setting up", r => ctl.Setup(r, iso, extra));
        }

        void DoRepair()
        {
            if (!Game.Downloaded) { DoInstall(); return; }
            if (!Yes("This generates the game's code and data again from your disc image, and rebuilds the game. "
                     + "It takes as long as the first setup. Your saves and settings are kept.\n\nSet up again?", "Set up again"))
                return;
            tabs.SelectedIndex = 0;
            DoSetup("--force");
        }

        void DoUpdate()
        {
            if (!Game.Downloaded) { DoInstall(); return; }
            string iso = AskIso();
            if (iso == null) return;
            tabs.SelectedIndex = 0;
            if (!Game.IsGitClone && !Platform.CanRunSetup(Game))
            {
                // (never set up: the new version's zip over this one, then the setup)
                ctl.Latest = null;
                RunTask("Updating", r => ctl.Download(r, cancel.Token) && ctl.Setup(r, iso, ""));
                return;
            }
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
                Say("The game could not be started (" + e.Message + ").", "Play", MessageBoxIcon.Error);
                return;
            }
            var started = DateTime.Now;
            btnMain.Enabled = false;
            btnMain.Text = "Playing...";
            WindowState = FormWindowState.Minimized;
            gameProc.EnableRaisingEvents = true;
            gameProc.Exited += (s, e) => BeginInvoke((Action)(() =>
            {
                int code = gameProc.ExitCode;
                WindowState = FormWindowState.Normal;
                Activate();
                btnMain.Enabled = true;
                ShowState(state, "");
                CheckUpdates(false);  // (the channel may have changed in the game's options)
                if (code != 0 && (DateTime.Now - started).TotalSeconds < 20)
                    Say(ctl.GameClosedMessage(code), "Play", MessageBoxIcon.Warning);
            }));
        }

        // --- Running a task ---------------------------------------------------------------------------------

        void RunTask(string what, Func<Runner, bool> work)
        {
            if (runner != null) return;
            runner = new Runner();
            cancel = new CancellationTokenSource();
            tabs.SelectedIndex = 0;
            btnMain.Enabled = false;
            btnMain.Text = what + "...";
            lblStatus.Text = what + "...";
            lblDetail.Text = "This takes a while (the first time, often 20 to 40 minutes). You can use the PC meanwhile; "
                           + "keep this window open. Cancel stops it, and the next run continues from where it stopped.";
            btnCancel.Visible = true;
            btnCancel.Enabled = true;
            bar.Visible = true;
            bar.Style = ProgressBarStyle.Marquee;
            updatePanel.Visible = false;
            foreach (var c in busyDisabled) c.Enabled = false;
            ctl.BeginTask(what);
            var r = runner;
            Task.Run(() =>
            {
                bool ok;
                try { ok = work(r); }
                catch (Exception e) { ctl.Fail("Something went wrong: " + e.Message); ctl.Log(e.ToString()); ok = false; }
                BeginInvoke((Action)(() => TaskDone(ok, r.Cancelled)));
            });
        }

        void TaskDone(bool ok, bool cancelled)
        {
            FlushLog();
            ctl.EndTask();
            runner.Dispose();
            runner = null;
            btnCancel.Visible = false;
            bar.Visible = false;
            foreach (var c in busyDisabled) c.Enabled = true;
            lblStep.Text = cancelled ? "Cancelled." : ok ? "" : lblStep.Text;
            RefreshState();
            if (ok)
            {
                if (Yes("Ratchet & Clank is ready. Start the game now?", "Ready", MessageBoxIcon.Information))
                {
                    state = GameState.Ready;
                    Play();
                }
            }
            else if (!cancelled)
            {
                string msg = ctl.Failure ?? "The setup failed.";
                chkLog.Checked = true;
                if (Yes(msg + "\n\nYour saves and settings are safe. You can try again with the button on the Play page."
                        + "\n\nCopy the full log to the clipboard, for a bug report on GitHub?", "Something went wrong", MessageBoxIcon.Error))
                {
                    try { Clipboard.SetText(ctl.FullLog.ToString()); } catch (Exception) { }
                    Platform.OpenUrl(GitHub.Page + "/issues");
                }
            }
        }

        void CancelTask()
        {
            if (runner == null) return;
            if (!Yes("Stop now? What was done so far is kept: the next run continues from there.", "Cancel")) return;
            btnCancel.Enabled = false;
            cancel?.Cancel();
            runner?.Cancel();
        }

        void OnClosing(object sender, FormClosingEventArgs e)
        {
            if (runner == null) return;
            if (!Yes("The setup is still running. Stop it and close? It continues from where it stopped next time.", "Close",
                     MessageBoxIcon.Warning))
            {
                e.Cancel = true;
                return;
            }
            cancel?.Cancel();
            runner?.Cancel();
        }

        // --- IView: the work's progress, from its thread ------------------------------------------------------

        public void Step(string text) => BeginInvoke((Action)(() =>
        {
            lblStep.Text = text;
            bar.Style = ProgressBarStyle.Marquee;
        }));

        public void StepDetail(string text) => BeginInvoke((Action)(() => lblStep.Text = text));

        public void Progress(int pct) => BeginInvoke((Action)(() =>
        {
            bar.Style = ProgressBarStyle.Continuous;
            bar.Value = Math.Max(0, Math.Min(100, pct));
        }));

        public void Log(string line) => pending.Enqueue(line);

        public bool Ask(string title, string text) => (bool)Invoke((Func<bool>)(() => Yes(text, title)));

        void FlushLog()
        {
            if (pending.IsEmpty) return;
            var sb = new StringBuilder();
            while (pending.TryDequeue(out string line)) sb.Append(line).Append("\r\n");
            if (txtLog.TextLength > 1 << 20) txtLog.Text = txtLog.Text.Substring(txtLog.TextLength - (512 << 10));
            txtLog.AppendText(sb.ToString());
        }

        // --- Saves ------------------------------------------------------------------------------------------

        void RefreshSaves()
        {
            lblCard.Text = "Your saves are in " + Saves.CardDir + ". Back them up before trying something new; "
                         + "a deleted slot goes to " + Platform.TrashName + ".";
            lstSaves.Items.Clear();
            try
            {
                foreach (var s in Saves.List())
                    lstSaves.Items.Add(new ListViewItem(new[] { s.Region, s.Slot.ToString(), s.Saved.ToString("g"), $"{(s.Size + 1023) / 1024} KB" }) { Tag = s });
            }
            catch (Exception e) { lblCard.Text += "\n(" + e.Message + ")"; }
            if (lstSaves.Items.Count == 0)
                lstSaves.Items.Add(new ListViewItem(new[] { "No saves yet", "", "", "" }));
        }

        void BackupSaves()
        {
            if (!Directory.Exists(Saves.CardDir)) { Say("There are no saves yet.", "Saves"); return; }
            using (var d = new SaveFileDialog
            {
                Title = "Back up the saves", Filter = "Zip archive (*.zip)|*.zip",
                FileName = $"rac1-saves-{DateTime.Now:yyyy-MM-dd}.zip",
                InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            })
            {
                if (d.ShowDialog(this) != DialogResult.OK) return;
                try
                {
                    Saves.Backup(d.FileName);
                    Say("The saves were backed up to\n" + d.FileName, "Saves");
                }
                catch (Exception e) { Say("The backup failed: " + e.Message, "Saves", MessageBoxIcon.Error); }
            }
        }

        void RestoreSaves()
        {
            if (gameProc != null && !gameProc.HasExited) { Say("Close the game first.", "Saves"); return; }
            using (var d = new OpenFileDialog { Title = "Restore saves from a backup", Filter = "Zip archive (*.zip)|*.zip" })
            {
                if (d.ShowDialog(this) != DialogResult.OK) return;
                if (!Yes("The saves in the backup replace the ones with the same slot now. "
                         + "The saves as they are now are backed up first, into\n" + Saves.BackupDir + "\n\nRestore?", "Saves"))
                    return;
                try
                {
                    Saves.Restore(d.FileName);
                    RefreshSaves();
                    Say("The saves were restored.", "Saves");
                }
                catch (Exception e) { Say("The restore failed: " + e.Message, "Saves", MessageBoxIcon.Error); }
            }
        }

        void DeleteSave()
        {
            if (lstSaves.SelectedItems.Count == 0 || !(lstSaves.SelectedItems[0].Tag is SaveSlot s))
            {
                Say("Choose a slot in the list first.", "Saves");
                return;
            }
            if (gameProc != null && !gameProc.HasExited) { Say("Close the game first.", "Saves"); return; }
            if (!Yes($"Delete slot {s.Slot} ({s.Region}, saved {s.Saved:g})? It goes to {Platform.TrashName}.", "Saves", MessageBoxIcon.Warning))
                return;
            try { Saves.Delete(s); }
            catch (Exception e) { Say("The slot could not be deleted: " + e.Message, "Saves", MessageBoxIcon.Error); }
            RefreshSaves();
        }

        // --- Settings ---------------------------------------------------------------------------------------

        void ChangeGameDir()
        {
            using (var d = new FolderBrowserDialog
            {
                Description = "Choose the game's folder: an empty folder for a new install, or a folder that already has the game.",
                SelectedPath = Directory.Exists(Game.Dir) ? Game.Dir : "",
                ShowNewFolderButton = true,
            })
            {
                if (d.ShowDialog(this) != DialogResult.OK) return;
                string dir = Controller.GameFolderFor(d.SelectedPath, out string sub);
                if (sub != null)
                {
                    if (!Yes("This folder is not empty. Install into a new folder inside it?\n" + sub, "Game folder")) return;
                    dir = sub;
                }
                if (!GameDir.Plain(dir))
                    Say("This folder's path has spaces or special letters in it. It should work, but if the setup has "
                        + "trouble, a plain path (like C:\\Games\\rac1-native) is the first thing to try.", "Game folder");
                ctl.UseGameDir(dir);
                txtDir.Text = dir;
                RefreshState();
            }
        }

        /// <summary>The launcher copied into the game folder, and its shortcuts (Platform.InstallLauncher).</summary>
        void CreateShortcuts(bool userAsked)
        {
            try
            {
                string desktopLnk = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                                                 "Ratchet & Clank (rac1-native).lnk");
                bool desktop = userAsked || !File.Exists(desktopLnk) && Yes("Put a shortcut to the game on the desktop?", "Shortcut");
                Platform.InstallLauncher(Game, Application.ExecutablePath, desktop);
                if (userAsked) Say("The shortcuts are on the desktop and in the Start menu.", "Shortcuts");
            }
            catch (Exception e)
            {
                if (userAsked) Say("The shortcuts could not be made: " + e.Message, "Shortcuts", MessageBoxIcon.Error);
                else ctl.Log("The shortcuts could not be made: " + e.Message);
            }
        }
    }
}
