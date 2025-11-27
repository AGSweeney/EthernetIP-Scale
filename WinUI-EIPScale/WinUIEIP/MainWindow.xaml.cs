#nullable enable
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;
using WinRT.Interop;
using EulerLink.Services;
using EulerLink.Views;

namespace EulerLink;

public sealed partial class MainWindow : Window
{
    private readonly DeviceApiService _apiService;
    private bool _windowInitialized = false;
    public static MainWindow? Instance { get; private set; }

    private const int ICON_SMALL = 0;
    private const int ICON_BIG = 1;
    private const int ICON_SMALL2 = 2;
    private const uint WM_SETICON = 0x0080;

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    private static extern IntPtr SendMessage(IntPtr hWnd, uint msg, int wParam, IntPtr lParam);

    public MainWindow()
    {
        this.InitializeComponent();
        Instance = this;
        _apiService = DeviceApiService.Instance;
        ContentFrame.Navigate(typeof(ConfigurationPage));
        
        this.Activated += MainWindow_Activated;
        
        SetWindowIcons();
    }

    private void SetWindowIcons()
    {
        try
        {
            var hWnd = WindowNative.GetWindowHandle(this);
            
            string? exePath = Environment.ProcessPath;
            if (exePath != null && System.IO.File.Exists(exePath))
            {
                using Icon? icon = Icon.ExtractAssociatedIcon(exePath);
                if (icon != null)
                {
                    SendMessage(hWnd, WM_SETICON, ICON_SMALL, icon.Handle);
                    SendMessage(hWnd, WM_SETICON, ICON_SMALL2, icon.Handle);
                    System.Diagnostics.Debug.WriteLine($"Icon set via P/Invoke from EXE: {exePath}");
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Failed to extract icon from EXE");
                }
            }
            else
            {
                var baseDir = AppContext.BaseDirectory;
                var iconPath = System.IO.Path.Combine(baseDir, "Assets", "Network_35252.ico");
                
                if (System.IO.File.Exists(iconPath))
                {
                    try
                    {
                        using Icon? icon = new Icon(iconPath);
                        SendMessage(hWnd, WM_SETICON, ICON_SMALL, icon.Handle);
                        SendMessage(hWnd, WM_SETICON, ICON_SMALL2, icon.Handle);
                        System.Diagnostics.Debug.WriteLine($"Icon set via P/Invoke from file: {iconPath}");
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.Debug.WriteLine($"Failed to load icon from file: {ex.Message}");
                    }
                }
            }
            
            var appWindow = this.AppWindow;
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
                        System.Diagnostics.Debug.WriteLine($"Icon also set via AppWindow.SetIcon: {fullPath}");
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.Debug.WriteLine($"Failed to set icon via AppWindow.SetIcon: {ex.Message}");
                    }
                }
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Error setting window icons: {ex.Message}");
        }
    }

    private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
    {
        if (_windowInitialized)
            return;

        var appWindow = this.AppWindow;
        if (appWindow != null)
        {
            appWindow.Resize(new Windows.Graphics.SizeInt32(1200, 850));
            
            if (appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsResizable = false;
            }
            
            _windowInitialized = true;
        }
    }

    private async void NavView_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    {
        if (args.SelectedItem is NavigationViewItem item)
        {
            var tag = item.Tag?.ToString();
            Type? pageType = tag switch
            {
                "Configuration" => typeof(ConfigurationPage),
                "Nau7802Status" => typeof(Nau7802StatusPage),
                "InputAssembly" => typeof(InputAssemblyPage),
                "OutputAssembly" => typeof(OutputAssemblyPage),
                "FirmwareUpdate" => typeof(FirmwareUpdatePage),
                "Logs" => typeof(LogsPage),
                _ => null
            };

            if (pageType != null)
            {
                ContentFrame.Navigate(pageType);
                
                // If navigating to Configuration page and connected, reload data
                if (pageType == typeof(ConfigurationPage) && _apiService.IsConnected)
                {
                    // Wait a moment for navigation to complete
                    await Task.Delay(100);
                    if (ContentFrame.Content is ConfigurationPage configPage)
                    {
                        await configPage.LoadAllConfigs();
                    }
                    await UpdateNau7802NavigationVisibility();
                }
            }
        }
    }

    private void DeviceIpTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (sender is TextBox textBox)
        {
            _apiService.SetDeviceIp(textBox.Text);
            ConnectionStatusTextBlock.Text = "";
        }
    }

    private async void DiscoverButton_Click(object sender, RoutedEventArgs e)
    {
        var discoveryDialog = new DeviceDiscoveryDialog();
        discoveryDialog.XamlRoot = this.Content.XamlRoot;
        var result = await discoveryDialog.ShowAsync();
        
        if (result == ContentDialogResult.Primary && discoveryDialog.SelectedDevice != null)
        {
            DeviceIpTextBox.Text = discoveryDialog.SelectedDevice.IpAddress;
            _apiService.SetDeviceIp(discoveryDialog.SelectedDevice.IpAddress);
        }
    }

    private async void ConnectButton_Click(object sender, RoutedEventArgs e)
    {
        ConnectButton.IsEnabled = false;
        DisconnectButton.IsEnabled = false;
        ConnectionStatusTextBlock.Text = "Connecting...";
        ConnectionStatusTextBlock.Foreground = new SolidColorBrush(Microsoft.UI.Colors.Orange);
        _apiService.SetConnected(false);

        try
        {
            var config = await _apiService.GetNetworkConfigAsync();
            if (config != null)
            {
                ConnectionStatusTextBlock.Text = "Connected";
                ConnectionStatusTextBlock.Foreground = new SolidColorBrush(Microsoft.UI.Colors.Green);
                ConnectButton.IsEnabled = false;
                DisconnectButton.IsEnabled = true;
                _apiService.SetConnected(true);
                
                // Auto-load configuration data when connected
                UpdateAllPagesConnectionState();
                if (ContentFrame.Content is ConfigurationPage configPage)
                {
                    await configPage.LoadAllConfigs();
                }
                await UpdateNau7802NavigationVisibility();
            }
            else
            {
                ConnectionStatusTextBlock.Text = "Connection failed";
                ConnectionStatusTextBlock.Foreground = new SolidColorBrush(Microsoft.UI.Colors.Red);
                ConnectButton.IsEnabled = true;
                DisconnectButton.IsEnabled = false;
                _apiService.SetConnected(false);
                
                // Update connection state when connection fails
                UpdateAllPagesConnectionState();
            }
        }
        catch (Exception ex)
        {
            ConnectionStatusTextBlock.Text = $"Error: {ex.Message}";
            ConnectionStatusTextBlock.Foreground = new SolidColorBrush(Microsoft.UI.Colors.Red);
            ConnectButton.IsEnabled = true;
            DisconnectButton.IsEnabled = false;
            _apiService.SetConnected(false);
            
            // Update connection state when connection fails
            UpdateAllPagesConnectionState();
        }
    }

    private async void DisconnectButton_Click(object sender, RoutedEventArgs e)
    {
        ConnectionStatusTextBlock.Text = "Disconnected";
        ConnectionStatusTextBlock.Foreground = new SolidColorBrush(Microsoft.UI.Colors.Gray);
        ConnectButton.IsEnabled = true;
        DisconnectButton.IsEnabled = false;
        _apiService.SetConnected(false);
        
        // Update connection state when disconnected
        UpdateAllPagesConnectionState();
        await UpdateNau7802NavigationVisibility();
    }

    private void UpdateAllPagesConnectionState()
    {
        if (ContentFrame.Content is ConfigurationPage configPage)
        {
            configPage.UpdateConnectionState();
        }
        else if (ContentFrame.Content is Nau7802StatusPage nau7802Page)
        {
            nau7802Page.UpdateConnectionState();
        }
        else if (ContentFrame.Content is InputAssemblyPage inputPage)
        {
            inputPage.UpdateConnectionState();
        }
        else if (ContentFrame.Content is OutputAssemblyPage outputPage)
        {
            outputPage.UpdateConnectionState();
        }
        else if (ContentFrame.Content is FirmwareUpdatePage firmwarePage)
        {
            firmwarePage.UpdateConnectionState();
        }
        else if (ContentFrame.Content is LogsPage logsPage)
        {
            logsPage.UpdateConnectionState();
        }
    }

    public async Task UpdateNau7802NavigationVisibility()
    {
        if (!_apiService.IsConnected)
        {
            Nau7802StatusNavItem.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
            return;
        }

        var config = await _apiService.GetNau7802ConfigAsync();
        if (config != null && config.Enabled)
        {
            Nau7802StatusNavItem.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
        }
        else
        {
            Nau7802StatusNavItem.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
            if (ContentFrame.Content is Nau7802StatusPage)
            {
                ContentFrame.Navigate(typeof(ConfigurationPage));
                NavView.SelectedItem = NavView.MenuItems[0];
            }
        }
    }
}

