using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using EulerLink.Services;
using EulerLink.Models;

namespace EulerLink.Views;

public sealed partial class Mpu6050StatusPage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;
    private DispatcherTimer? _updateTimer;
    private bool _isUpdating = false;
    
    // Gauge needle transforms
    private RotateTransform? _rollNeedleTransform;
    private RotateTransform? _pitchNeedleTransform;
    private RotateTransform? _groundAngleNeedleTransform;

    public Mpu6050StatusPage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += Mpu6050StatusPage_Loaded;
        Unloaded += Mpu6050StatusPage_Unloaded;
    }

    protected override void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
    }

    private void Mpu6050StatusPage_Loaded(object sender, RoutedEventArgs e)
    {
        UpdateConnectionState();
        
        // Initialize gauge needle transforms - create with center point at 75,75 for 150x150 gauge
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
        Mpu6050ContentPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private void Mpu6050StatusPage_Unloaded(object sender, RoutedEventArgs e)
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
                Mpu6050ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                return;
            }
            else
            {
                NotConnectedWarningBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                Mpu6050ContentPanel.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
            }

            var status = await _apiService.GetMpu6050StatusAsync();
            
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
                Mpu6050ReadingsBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                return;
            }
            else
            {
                DisabledWarningBorder.Visibility = Microsoft.UI.Xaml.Visibility.Collapsed;
                Mpu6050ReadingsBorder.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
            }

            // Update gauge values and rotate needles
            // Angle gauges show -90° to +90° range, with 0° pointing up
            // Pressure gauges show 0-100 PSI range, with 0 PSI pointing left (-90°), 100 PSI pointing right (+90°)
            
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
        
        // Clamp angle to -90° to +90° range for display
        float clampedAngle = Math.Clamp(angle, -90f, 90f);
        
        // Update text
        valueTextBlock.Text = $"{angle:F2}°";
        
        // Rotate needle: The needle starts pointing down (0°)
        // -90° rotates needle to left, 0° is down (neutral), +90° rotates needle to right
        // Since needle points down, we rotate it by the angle value
        transform.Angle = clampedAngle;
    }
    
    private void UpdatePressureBar(Border bar, float pressurePsi, TextBlock valueTextBlock)
    {
        if (bar == null) return;
        
        // Clamp pressure to 0-100 PSI range
        float clampedPressure = Math.Clamp(pressurePsi, 0f, 100f);
        
        // Update text
        valueTextBlock.Text = $"{pressurePsi:F2} PSI";
        
        // Get the parent container width to calculate bar width
        if (bar.Parent is Grid parentGrid && parentGrid.ActualWidth > 0)
        {
            // Calculate bar width as percentage of container (0-100 PSI maps to 0-100% width)
            double barWidth = (clampedPressure / 100f) * parentGrid.ActualWidth;
            bar.Width = barWidth;
        }
        else
        {
            // Fallback: use a fixed width calculation if parent isn't ready
            // Assume container is about 200px wide
            double barWidth = (clampedPressure / 100f) * 200.0;
            bar.Width = barWidth;
        }
    }
}

