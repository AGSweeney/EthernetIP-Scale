using System;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using EulerLink.Models;
using EulerLink.Services;

namespace EulerLink.Views;

public sealed partial class LogsPage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;
    private DispatcherTimer? _updateTimer;
    private bool _isUpdating = false;

    public LogsPage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += LogsPage_Loaded;
        Unloaded += LogsPage_Unloaded;
    }

    protected override void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
    }

    private void LogsPage_Loaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateConnectionState();
        _updateTimer = new DispatcherTimer();
        _updateTimer.Interval = TimeSpan.FromSeconds(2);
        _updateTimer.Tick += UpdateTimer_Tick;
        _updateTimer.Start();
        UpdateTimer_Tick(null, null);
    }

    private void LogsPage_Unloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        _updateTimer?.Stop();
    }

    private async void UpdateTimer_Tick(object? sender, object? e)
    {
        if (_isUpdating || !IsLoaded)
            return;
            
        _isUpdating = true;
        try
        {
            if (!_apiService.IsConnected)
            {
                UpdateConnectionState();
                return;
            }
            else
            {
                UpdateConnectionState();
                await LoadLogs();
            }
        }
        finally
        {
            _isUpdating = false;
        }
    }

    public void UpdateConnectionState()
    {
        bool isConnected = _apiService.IsConnected;
        NotConnectedWarningBorder.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Collapsed 
            : Microsoft.UI.Xaml.Visibility.Visible;
        LogsContentPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private async Task LoadLogs()
    {
        if (!_apiService.IsConnected)
        {
            LogsTextBlock.Text = "Not connected to device.";
            return;
        }

        try
        {
            var logResponse = await _apiService.GetLogsAsync();
            if (logResponse != null && logResponse.Status == "ok")
            {
                LogsTextBlock.Text = logResponse.Logs;
                LogSizeTextBlock.Text = $"{logResponse.Size:N0} bytes";
                TotalSizeTextBlock.Text = $"{logResponse.TotalSize:N0} bytes";
                TruncatedTextBlock.Text = logResponse.Truncated 
                    ? "⚠️ Logs truncated (buffer wrapped)" 
                    : "✓ Complete";
            }
            else
            {
                LogsTextBlock.Text = "Failed to load logs from device.";
                LogSizeTextBlock.Text = "N/A";
                TotalSizeTextBlock.Text = "N/A";
                TruncatedTextBlock.Text = "Unknown";
            }
        }
        catch (Exception ex)
        {
            LogsTextBlock.Text = $"Error loading logs: {ex.Message}";
            LogSizeTextBlock.Text = "Error";
            TotalSizeTextBlock.Text = "Error";
            TruncatedTextBlock.Text = "Error";
        }
    }

    private async void RefreshButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        await LoadLogs();
    }
}

