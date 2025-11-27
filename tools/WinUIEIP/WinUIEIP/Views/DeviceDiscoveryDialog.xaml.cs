using System;
using System.Collections.ObjectModel;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Controls;
using EulerLink.Models;
using EulerLink.Services;

namespace EulerLink.Views;

public sealed partial class DeviceDiscoveryDialog : ContentDialog
{
    private readonly DeviceDiscoveryService _discoveryService;
    private CancellationTokenSource? _cancellationTokenSource;
    public DiscoveredDevice? SelectedDevice { get; private set; }

    public DeviceDiscoveryDialog()
    {
        this.InitializeComponent();
        _discoveryService = new DeviceDiscoveryService();
        DevicesListView.ItemsSource = new ObservableCollection<DiscoveredDevice>();
    }

    private async void ScanButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        ScanButton.IsEnabled = false;
        StatusTextBlock.Text = "Scanning for devices...";
        CountTextBlock.Text = "Scanning...";
        
        _cancellationTokenSource?.Cancel();
        _cancellationTokenSource = new CancellationTokenSource();
        
        try
        {
            var devices = await _discoveryService.DiscoverDevicesAsync(_cancellationTokenSource.Token);
            
            var collection = DevicesListView.ItemsSource as ObservableCollection<DiscoveredDevice>;
            collection?.Clear();
            
            foreach (var device in devices)
            {
                collection?.Add(device);
            }
            
            CountTextBlock.Text = $"{devices.Count} device(s) found";
            
            if (devices.Count == 0)
            {
                StatusTextBlock.Text = "No devices found. Make sure devices are powered on and connected to the network.";
            }
            else
            {
                StatusTextBlock.Text = $"Found {devices.Count} device(s). Select a device and click 'Select'.";
            }
        }
        catch (OperationCanceledException)
        {
            StatusTextBlock.Text = "Scan cancelled.";
        }
        catch (Exception ex)
        {
            StatusTextBlock.Text = $"Error during scan: {ex.Message}";
        }
        finally
        {
            ScanButton.IsEnabled = true;
        }
    }

    private void ContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        if (DevicesListView.SelectedItem is DiscoveredDevice device)
        {
            SelectedDevice = device;
        }
        else
        {
            args.Cancel = true;
            StatusTextBlock.Text = "Please select a device first.";
        }
    }

    private void ContentDialog_SecondaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        _cancellationTokenSource?.Cancel();
    }
}

