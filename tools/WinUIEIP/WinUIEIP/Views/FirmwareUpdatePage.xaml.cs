using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using EulerLink.Services;
using Windows.Storage.Pickers;

namespace EulerLink.Views;

public sealed partial class FirmwareUpdatePage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;
    private string? _selectedFilePath;
    private DispatcherTimer? _otaStatusTimer;

    public FirmwareUpdatePage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += FirmwareUpdatePage_Loaded;
        Unloaded += FirmwareUpdatePage_Unloaded;
    }

    protected override void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
    }

    private void FirmwareUpdatePage_Loaded(object sender, RoutedEventArgs e)
    {
        UpdateConnectionState();
        StartOtaStatusTimer();
    }

    private void FirmwareUpdatePage_Unloaded(object sender, RoutedEventArgs e)
    {
        _otaStatusTimer?.Stop();
    }

    private void StartOtaStatusTimer()
    {
        _otaStatusTimer = new DispatcherTimer();
        _otaStatusTimer.Interval = TimeSpan.FromSeconds(1);
        _otaStatusTimer.Tick += OtaStatusTimer_Tick;
        _otaStatusTimer.Start();
        OtaStatusTimer_Tick(null, null);
    }

    private async void OtaStatusTimer_Tick(object? sender, object? e)
    {
        if (!_apiService.IsConnected)
        {
            OtaStatusTextBlock.Text = "Not Connected";
            OtaProgressTextBlock.Text = "N/A";
            OtaMessageTextBlock.Text = "Please connect to device";
            OtaProgressBar.Visibility = Visibility.Collapsed;
            return;
        }

        var status = await _apiService.GetOtaStatusAsync();
        if (status != null)
        {
            OtaStatusTextBlock.Text = status.Status;
            OtaProgressTextBlock.Text = $"{status.Progress}%";
            OtaMessageTextBlock.Text = status.Message;

            if (status.Status == "in_progress")
            {
                OtaProgressBar.Visibility = Visibility.Visible;
                OtaProgressBar.Value = status.Progress;
            }
            else
            {
                OtaProgressBar.Visibility = Visibility.Collapsed;
            }
        }
        else
        {
            OtaStatusTextBlock.Text = "Unknown";
            OtaProgressTextBlock.Text = "N/A";
            OtaMessageTextBlock.Text = "Failed to get status";
            OtaProgressBar.Visibility = Visibility.Collapsed;
        }
    }

    public void UpdateConnectionState()
    {
        bool isConnected = _apiService.IsConnected;
        NotConnectedWarningBorder.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Collapsed 
            : Microsoft.UI.Xaml.Visibility.Visible;
        FirmwareContentPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
        OtaStatusBorder.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private async void ChooseFile_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FileOpenPicker();
        var app = Application.Current as App;
        if (app?.m_window != null)
        {
            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(app.m_window);
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);
        }
        picker.FileTypeFilter.Add(".bin");
        picker.SuggestedStartLocation = PickerLocationId.DocumentsLibrary;

        var file = await picker.PickSingleFileAsync();
        if (file != null)
        {
            _selectedFilePath = file.Path;
            SelectedFileTextBlock.Text = $"Selected: {file.Name}";
            UploadButton.IsEnabled = true;
        }
    }

    private async void UploadFirmware_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrEmpty(_selectedFilePath))
        {
            await ShowMessage("Please select a firmware file first.");
            return;
        }

        UploadProgressBar.Visibility = Visibility.Visible;
        UploadButton.IsEnabled = false;

        try
        {
            bool success = await _apiService.UploadFirmwareAsync(_selectedFilePath);
            if (success)
            {
                await ShowMessage("Firmware update started. The device will reboot after validation.");
            }
            else
            {
                await ShowMessage("Firmware upload failed. Please check the file and try again.");
            }
        }
        catch (Exception ex)
        {
            await ShowMessage($"Error: {ex.Message}");
        }
        finally
        {
            UploadProgressBar.Visibility = Visibility.Collapsed;
            UploadButton.IsEnabled = true;
        }
    }

    private async System.Threading.Tasks.Task ShowMessage(string message)
    {
        var dialog = new ContentDialog
        {
            Title = "Firmware Update",
            Content = message,
            CloseButtonText = "OK",
            XamlRoot = this.XamlRoot
        };
        await dialog.ShowAsync();
    }
}

