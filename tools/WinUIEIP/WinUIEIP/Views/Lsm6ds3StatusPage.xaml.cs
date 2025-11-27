using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using EulerLink.Services;
using EulerLink.Models;

namespace EulerLink.Views;

public sealed partial class Lsm6ds3StatusPage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;
    private DispatcherTimer? _updateTimer;
    private bool _isUpdating = false;
    
    private RotateTransform? _rollNeedleTransform;
    private RotateTransform? _pitchNeedleTransform;
    private RotateTransform? _groundAngleNeedleTransform;

    public Lsm6ds3StatusPage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += Lsm6ds3StatusPage_Loaded;
        Unloaded += Lsm6ds3StatusPage_Unloaded;
    }

    protected override void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
    }

    private void Lsm6ds3StatusPage_Loaded(object sender, RoutedEventArgs e)
    {
        UpdateConnectionState();
        
        _rollNeedleTransform = new RotateTransform { CenterX = 75, CenterY = 75 };
        RollNeedle.RenderTransform = _rollNeedleTransform;
        
        _pitchNeedleTransform = new RotateTransform { CenterX = 75, CenterY = 75 };
        PitchNeedle.RenderTransform = _pitchNeedleTransform;
        
        _groundAngleNeedleTransform = new RotateTransform { CenterX = 75, CenterY = 75 };
        GroundAngleNeedle.RenderTransform = _groundAngleNeedleTransform;
        
        _updateTimer = new DispatcherTimer();
        _updateTimer.Interval = TimeSpan.FromMilliseconds(50);
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
        Lsm6ds3ContentPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private void Lsm6ds3StatusPage_Unloaded(object sender, RoutedEventArgs e)
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
                NotConnectedWarningBorder.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
                Lsm6ds3ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                return;
            }
            else
            {
                NotConnectedWarningBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                Lsm6ds3ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
            }

            var status = await _apiService.GetLsm6ds3StatusAsync();
            
            if (status == null)
            {
                RollGaugeValueTextBlock.Text = "N/A";
                PitchGaugeValueTextBlock.Text = "N/A";
                GroundAngleGaugeValueTextBlock.Text = "N/A";
                BottomPressureGaugeValueTextBlock.Text = "N/A";
                TopPressureGaugeValueTextBlock.Text = "N/A";
                return;
            }

            if (!status.Enabled)
            {
                DisabledWarningBorder.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
                Lsm6ds3ReadingsBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                return;
            }
            else
            {
                DisabledWarningBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                Lsm6ds3ReadingsBorder.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
            }

            UpdateGauge(_rollNeedleTransform, status.Roll, RollGaugeValueTextBlock);
            UpdateGauge(_pitchNeedleTransform, status.Pitch, PitchGaugeValueTextBlock);
            UpdateGauge(_groundAngleNeedleTransform, status.GroundAngle, GroundAngleGaugeValueTextBlock);
            UpdatePressureBar(BottomPressureBar, status.BottomPressurePsi, BottomPressureGaugeValueTextBlock);
            UpdatePressureBar(TopPressureBar, status.TopPressurePsi, TopPressureGaugeValueTextBlock);
        }
        catch (Exception ex)
        {
            RollGaugeValueTextBlock.Text = "Error";
            PitchGaugeValueTextBlock.Text = "Error";
            GroundAngleGaugeValueTextBlock.Text = "Error";
            BottomPressureGaugeValueTextBlock.Text = "Error";
            TopPressureGaugeValueTextBlock.Text = "Error";
        }
        finally
        {
            _isUpdating = false;
        }
    }
    
    private void UpdateGauge(RotateTransform? transform, float angle, TextBlock valueTextBlock)
    {
        if (transform == null) return;
        
        float clampedAngle = Math.Clamp(angle, -90f, 90f);
        
        valueTextBlock.Text = $"{angle:F2}°";
        
        transform.Angle = clampedAngle;
    }
    
    private void UpdatePressureBar(Border bar, float pressurePsi, TextBlock valueTextBlock)
    {
        if (bar == null) return;
        
        float clampedPressure = Math.Clamp(pressurePsi, 0f, 100f);
        
        valueTextBlock.Text = $"{pressurePsi:F2} PSI";
        
        if (bar.Parent is Grid parentGrid && parentGrid.ActualWidth > 0)
        {
            double barWidth = (clampedPressure / 100f) * parentGrid.ActualWidth;
            bar.Width = barWidth;
        }
        else
        {
            double barWidth = (clampedPressure / 100f) * 200.0;
            bar.Width = barWidth;
        }
    }
}

