using System;
using Microsoft.UI.Xaml;

namespace EulerLink;

public partial class App : Application
{
    public App()
    {
        this.InitializeComponent();
    }

    protected override void OnLaunched(Microsoft.UI.Xaml.LaunchActivatedEventArgs args)
    {
        m_window = new MainWindow();
        
        var appWindow = m_window.AppWindow;
        if (appWindow != null)
        {
            var baseDir = AppContext.BaseDirectory;
            var iconPath = System.IO.Path.Combine(baseDir, "Assets", "Network_35252.ico");
            
            if (System.IO.File.Exists(iconPath))
            {
                try
                {
                    var fullPath = System.IO.Path.GetFullPath(iconPath);
                    appWindow.SetIcon(fullPath);
                    System.Diagnostics.Debug.WriteLine($"Icon set in OnLaunched: {fullPath}");
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"Failed to set icon in OnLaunched: {ex.Message}");
                }
            }
        }
        
        m_window.Activate();
    }

    public Window? m_window { get; private set; }
}

