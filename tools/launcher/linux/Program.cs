using System;
using System.Threading;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Media;
using Avalonia.Styling;
using Avalonia.Themes.Fluent;

[assembly: System.Runtime.Versioning.SupportedOSPlatform("linux")]

namespace Rac1Launcher
{
    sealed class App : Application
    {
        public override void Initialize()
        {
            Styles.Add(new FluentTheme());
            // the launcher's own colours that differ with the light and the dark theme (MainWindow.Themed): the notes'
            // gray text, the update notice's background
            Resources.ThemeDictionaries[ThemeVariant.Light] = new ResourceDictionary
            {
                ["LauncherGray"] = new SolidColorBrush(Color.FromRgb(120, 120, 128)),
                ["LauncherNotice"] = new SolidColorBrush(Color.FromRgb(255, 244, 214)),
            };
            Resources.ThemeDictionaries[ThemeVariant.Dark] = new ResourceDictionary
            {
                ["LauncherGray"] = new SolidColorBrush(Color.FromRgb(150, 150, 160)),
                ["LauncherNotice"] = new SolidColorBrush(Color.FromRgb(72, 56, 24)),
            };
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
                desktop.MainWindow = new MainWindow();
            base.OnFrameworkInitializationCompleted();
        }
    }

    static class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            using (var one = new Mutex(true, "rac1-native-launcher", out bool first))
            {
                if (!first)
                {
                    Console.Error.WriteLine("The launcher is already open.");
                    return 1;
                }
                return AppBuilder.Configure<App>()
                                 .UsePlatformDetect()
                                 .WithInterFont()  // (its own font: the same text on every distribution)
                                 .StartWithClassicDesktopLifetime(args);
            }
        }
    }
}
