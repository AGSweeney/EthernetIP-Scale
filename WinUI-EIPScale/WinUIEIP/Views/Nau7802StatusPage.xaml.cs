#nullable enable
using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using EulerLink.Models;
using EulerLink.Services;

namespace EulerLink.Views;

public sealed partial class Nau7802StatusPage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;
    private DispatcherTimer? _updateTimer;
    private bool _isUpdating = false;

    public Nau7802StatusPage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += Nau7802StatusPage_Loaded;
        Unloaded += Nau7802StatusPage_Unloaded;
    }

    protected override void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
    }

    private void Nau7802StatusPage_Loaded(object sender, RoutedEventArgs e)
    {
        UpdateConnectionState();
        
        _updateTimer = new DispatcherTimer();
        _updateTimer.Interval = TimeSpan.FromMilliseconds(250);
        _updateTimer.Tick += UpdateTimer_Tick;
        _updateTimer.Start();
        UpdateTimer_Tick(null, null);
    }

    public void UpdateConnectionState()
    {
        bool isConnected = _apiService.IsConnected;
        NotConnectedWarningBorder.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Collapsed 
            : Microsoft.UI.Xaml.Visibility.Visible;
        Nau7802ContentPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private void Nau7802StatusPage_Unloaded(object sender, RoutedEventArgs e)
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
            
            UpdateConnectionState();
            
            var config = await _apiService.GetNau7802ConfigAsync();
            if (config != null)
            {
                if (!config.Enabled)
                {
                    Nau7802ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                    Nau7802DisabledBorder.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
                    return;
                }
                
                Nau7802ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
                Nau7802DisabledBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                
                WeightTextBlock.Text = config.Weight.ToString("F2");
                UnitTextBlock.Text = config.UnitLabel ?? "lbs";
                RawReadingTextBlock.Text = config.RawReading.ToString();
                CalibrationFactorTextBlock.Text = config.CalibrationFactor.ToString("F2");
                ZeroOffsetTextBlock.Text = config.ZeroOffset.ToString("F2");
                ByteOffsetTextBlock.Text = config.ByteOffset.ToString();
                
                InitializedTextBlock.Text = config.Initialized ? "Yes" : "No";
                ConnectedTextBlock.Text = config.Connected ? "Yes" : "No";
                AvailableTextBlock.Text = config.Available ? "Yes" : "No";
                GainTextBlock.Text = config.GainLabel ?? "-";
                SampleRateTextBlock.Text = config.SampleRateLabel ?? "-";
                ChannelTextBlock.Text = config.ChannelLabel ?? "-";
                LdoVoltageTextBlock.Text = $"{config.LdoVoltage:F1}V";
                RevisionCodeTextBlock.Text = $"0x{config.RevisionCode:X2}";
                
                if (config.Status != null)
                {
                    StatusAvailableTextBlock.Text = config.Status.Available ? "Yes" : "No";
                    StatusPowerDigitalTextBlock.Text = config.Status.PowerDigital ? "Yes" : "No";
                    StatusPowerAnalogTextBlock.Text = config.Status.PowerAnalog ? "Yes" : "No";
                    StatusPowerRegulatorTextBlock.Text = config.Status.PowerRegulator ? "Yes" : "No";
                    StatusCalibrationActiveTextBlock.Text = config.Status.CalibrationActive ? "Yes" : "No";
                    StatusCalibrationErrorTextBlock.Text = config.Status.CalibrationError ? "Yes" : "No";
                    StatusOscillatorReadyTextBlock.Text = config.Status.OscillatorReady ? "Yes" : "No";
                    StatusAvddReadyTextBlock.Text = config.Status.AvddReady ? "Yes" : "No";
                }
                else
                {
                    StatusAvailableTextBlock.Text = "-";
                    StatusPowerDigitalTextBlock.Text = "-";
                    StatusPowerAnalogTextBlock.Text = "-";
                    StatusPowerRegulatorTextBlock.Text = "-";
                    StatusCalibrationActiveTextBlock.Text = "-";
                    StatusCalibrationErrorTextBlock.Text = "-";
                    StatusOscillatorReadyTextBlock.Text = "-";
                    StatusAvddReadyTextBlock.Text = "-";
                }
            }
            else
            {
                Nau7802ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                Nau7802DisabledBorder.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Nau7802StatusPage error: {ex}");
        }
        finally
        {
            _isUpdating = false;
        }
    }
}

