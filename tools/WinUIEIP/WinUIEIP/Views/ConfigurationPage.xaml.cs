using System;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using EulerLink.Models;
using EulerLink.Services;
using EulerLink;

namespace EulerLink.Views;

public sealed partial class ConfigurationPage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;

    public ConfigurationPage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += ConfigurationPage_Loaded;
    }

    protected override async void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
        // Reload data when navigating to this page if connected
        if (_apiService.IsConnected)
        {
            await LoadAllConfigs();
        }
    }

    private async void ConfigurationPage_Loaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateConnectionState();
        // If already connected, load the data automatically
        if (_apiService.IsConnected)
        {
            await LoadAllConfigs();
        }
    }

    public void UpdateConnectionState()
    {
        bool isConnected = _apiService.IsConnected;
        NotConnectedWarningBorder.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Collapsed 
            : Microsoft.UI.Xaml.Visibility.Visible;
        ConfigurationContentPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    public async Task LoadAllConfigs()
    {
        if (_apiService.IsConnected)
        {
            await LoadNetworkConfig();
            await LoadModbusConfig();
            await LoadI2cConfig();
            await LoadMpu6050Config();
            await LoadLsm6ds3Config();
        }
    }

    private void UseDhcpCheckBox_Checked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateDnsVisibility();
    }

    private void UseDhcpCheckBox_Unchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateDnsVisibility();
    }

    private void UpdateDnsVisibility()
    {
        bool isEnabled = !UseDhcpCheckBox.IsChecked ?? true;
        Dns1Label.Visibility = isEnabled ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        Dns1TextBox.Visibility = isEnabled ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        Dns2Label.Visibility = isEnabled ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        Dns2TextBox.Visibility = isEnabled ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private async Task LoadNetworkConfig()
    {
        var config = await _apiService.GetNetworkConfigAsync();
        if (config != null)
        {
            UseDhcpCheckBox.IsChecked = config.UseDhcp;
            IpAddressTextBox.Text = config.IpAddress;
            NetmaskTextBox.Text = config.Netmask;
            GatewayTextBox.Text = config.Gateway;
            Dns1TextBox.Text = config.Dns1;
            Dns2TextBox.Text = config.Dns2;
            UpdateDnsVisibility();
        }
    }

    private async void SaveNetworkConfig_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        var config = new NetworkConfig
        {
            UseDhcp = UseDhcpCheckBox.IsChecked ?? false,
            IpAddress = IpAddressTextBox.Text,
            Netmask = NetmaskTextBox.Text,
            Gateway = GatewayTextBox.Text,
            Dns1 = Dns1TextBox.Text,
            Dns2 = Dns2TextBox.Text
        };

        bool success = await _apiService.SaveNetworkConfigAsync(config);
        if (success)
        {
            ShowRebootRequiredMessage("Network configuration saved. Reboot required.");
        }
        else
        {
            ShowMessage("Failed to save network configuration.");
        }
    }

    private async Task LoadModbusConfig()
    {
        var enabled = await _apiService.IsModbusEnabledAsync();
        if (enabled.HasValue)
        {
            ModbusEnabledCheckBox.IsChecked = enabled.Value;
        }
    }

    private async void SaveModbusConfig_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        bool enabled = ModbusEnabledCheckBox.IsChecked ?? false;
        bool success = await _apiService.SetModbusEnabledAsync(enabled);
        ShowMessage(success ? "Modbus configuration saved." : "Failed to save Modbus configuration.");
    }

    private async void LoadNetworkConfig_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Please connect to device first.");
            return;
        }
        await LoadNetworkConfig();
        ShowMessage("Network settings loaded.");
    }

    private async void LoadModbusConfig_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Please connect to device first.");
            return;
        }
        await LoadModbusConfig();
        ShowMessage("Modbus settings loaded.");
    }

    private async void ShowRebootRequiredMessage(string message)
    {
        var dialog = new ContentDialog
        {
            Title = "Configuration Saved",
            Content = message,
            PrimaryButtonText = "Reboot Now",
            CloseButtonText = "Later",
            XamlRoot = this.XamlRoot
        };

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            // User clicked "Reboot Now"
            bool success = await _apiService.RebootDeviceAsync();
            if (success)
            {
                ShowMessage("Reboot command sent. The device will reboot shortly. You may need to reconnect after it restarts.");
            }
            else
            {
                ShowMessage("Failed to send reboot command. Please try again.");
            }
        }
    }

    private async Task LoadI2cConfig()
    {
        if (!_apiService.IsConnected) return;

        var enabled = await _apiService.IsI2cPullupEnabledAsync();
        if (enabled.HasValue)
        {
            I2cPullupEnabledCheckBox.IsChecked = enabled.Value;
        }
    }

    private async void SaveI2cConfig_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var enabled = I2cPullupEnabledCheckBox.IsChecked ?? false;
        bool success = await _apiService.SetI2cPullupEnabledAsync(enabled);

        if (success)
        {
            ShowRebootRequiredMessage("I2C pull-up configuration saved successfully. Restart required for changes to take effect.");
        }
        else
        {
            ShowMessage("Failed to save I2C configuration.");
        }
    }

    private async Task LoadMpu6050Config()
    {
        if (!_apiService.IsConnected) return;

        var enabled = await _apiService.IsMpu6050EnabledAsync();
        if (enabled.HasValue)
        {
            Mpu6050EnabledCheckBox.IsChecked = enabled.Value;
            UpdateMpu6050ConfigVisibility();
        }

        if (enabled == true)
        {
            var byteOffset = await _apiService.GetMpu6050ByteOffsetAsync();
            if (byteOffset.HasValue)
            {
                foreach (ComboBoxItem item in Mpu6050ByteOffsetComboBox.Items)
                {
                    if (item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == byteOffset.Value)
                    {
                        Mpu6050ByteOffsetComboBox.SelectedItem = item;
                        break;
                    }
                }
            }

            var toolWeight = await _apiService.GetToolWeightAsync();
            if (toolWeight.HasValue)
            {
                ToolWeightTextBlock.Text = toolWeight.Value.ToString();
            }

            var tipForce = await _apiService.GetTipForceAsync();
            if (tipForce.HasValue)
            {
                TipForceTextBlock.Text = tipForce.Value.ToString();
            }

            var cylinderBore = await _apiService.GetCylinderBoreAsync();
            if (cylinderBore.HasValue)
            {
                CylinderBoreNumberBox.Value = cylinderBore.Value;
            }
        }
    }

    private void UpdateMpu6050ConfigVisibility()
    {
        bool enabled = Mpu6050EnabledCheckBox.IsChecked ?? false;
        Mpu6050ConfigSection.Visibility = enabled ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        Mpu6050DisabledSection.Visibility = enabled ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
    }

    private void Mpu6050EnabledCheckBox_Checked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateMpu6050ConfigVisibility();
    }

    private void Mpu6050EnabledCheckBox_Unchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateMpu6050ConfigVisibility();
    }

    private async void SaveMpu6050Config_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var enabled = Mpu6050EnabledCheckBox.IsChecked ?? false;
        bool success = await _apiService.SetMpu6050EnabledAsync(enabled);

            if (success && enabled)
            {
                var selectedItem = Mpu6050ByteOffsetComboBox.SelectedItem as ComboBoxItem;
                if (selectedItem?.Tag != null && int.TryParse(selectedItem.Tag.ToString(), out int byteOffset))
                {
                    success = await _apiService.SetMpu6050ByteOffsetAsync(byteOffset);
                }

                if (success)
                {
                    success = await _apiService.SetCylinderBoreAsync((float)CylinderBoreNumberBox.Value);
                }
            }

        if (success)
        {
            ShowRebootRequiredMessage("MPU6050 configuration saved successfully. Restart required for changes to take effect.");
            // Update navigation visibility in MainWindow
            await UpdateMainWindowNavigation();
        }
        else
        {
            ShowMessage("Failed to save MPU6050 configuration.");
        }
    }

    private async Task LoadLsm6ds3Config()
    {
        if (!_apiService.IsConnected) return;

        var enabled = await _apiService.IsLsm6ds3EnabledAsync();
        if (enabled.HasValue)
        {
            Lsm6ds3EnabledCheckBox.IsChecked = enabled.Value;
            UpdateLsm6ds3ConfigVisibility();
        }

        if (enabled == true)
        {
            var byteOffset = await _apiService.GetLsm6ds3ByteOffsetAsync();
            if (byteOffset.HasValue)
            {
                foreach (ComboBoxItem item in Lsm6ds3ByteOffsetComboBox.Items)
                {
                    if (item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == byteOffset.Value)
                    {
                        Lsm6ds3ByteOffsetComboBox.SelectedItem = item;
                        break;
                    }
                }
            }
            await LoadLsm6ds3Calibration();
        }
    }

    private async Task LoadLsm6ds3Calibration()
    {
        if (!_apiService.IsConnected) return;

        var calibrationStatus = await _apiService.GetLsm6ds3CalibrationStatusAsync();
        if (calibrationStatus != null)
        {
            if (calibrationStatus.Calibrated)
            {
                Lsm6ds3CalibrationStatusTextBlock.Text = $"Calibration Status: Calibrated (X: {calibrationStatus.GyroOffsetXMdps:F2}, Y: {calibrationStatus.GyroOffsetYMdps:F2}, Z: {calibrationStatus.GyroOffsetZMdps:F2})";
            }
            else
            {
                Lsm6ds3CalibrationStatusTextBlock.Text = "Calibration Status: Not calibrated";
            }
        }
        else
        {
            Lsm6ds3CalibrationStatusTextBlock.Text = "Calibration Status: N/A (Error loading)";
        }
    }

    private async void CalibrateLsm6ds3Gyro_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var dialog = new ContentDialog
        {
            Title = "Calibrate LSM6DS3 Gyroscope",
            Content = "Ensure the device is stationary and level before starting calibration. This will take a few seconds.",
            PrimaryButtonText = "Start Calibration",
            CloseButtonText = "Cancel",
            XamlRoot = this.XamlRoot
        };

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            Lsm6ds3CalibrationStatusTextBlock.Text = "Calibration Status: Calibrating...";
            var calibrationResult = await _apiService.CalibrateLsm6ds3Async();
            if (calibrationResult != null && calibrationResult.Status == "ok")
            {
                ShowMessage($"LSM6DS3 Gyroscope Calibration Successful: {calibrationResult.Message}");
            }
            else
            {
                ShowMessage($"LSM6DS3 Gyroscope Calibration Failed: {calibrationResult?.Message ?? "Unknown error"}");
            }
            // Always refresh calibration status after calibration attempt
            await LoadLsm6ds3Calibration();
        }
    }

    private void UpdateLsm6ds3ConfigVisibility()
    {
        bool enabled = Lsm6ds3EnabledCheckBox.IsChecked ?? false;
        Lsm6ds3ConfigSection.Visibility = enabled ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        Lsm6ds3DisabledSection.Visibility = enabled ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
    }

    private void Lsm6ds3EnabledCheckBox_Checked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateLsm6ds3ConfigVisibility();
    }

    private void Lsm6ds3EnabledCheckBox_Unchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateLsm6ds3ConfigVisibility();
    }

    private async void SaveLsm6ds3Config_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var enabled = Lsm6ds3EnabledCheckBox.IsChecked ?? false;
        bool success = await _apiService.SetLsm6ds3EnabledAsync(enabled);

        if (success && enabled)
        {
            var selectedItem = Lsm6ds3ByteOffsetComboBox.SelectedItem as ComboBoxItem;
            if (selectedItem?.Tag != null && int.TryParse(selectedItem.Tag.ToString(), out int byteOffset))
            {
                success = await _apiService.SetLsm6ds3ByteOffsetAsync(byteOffset);
            }
        }

        if (success)
        {
            ShowRebootRequiredMessage("LSM6DS3 configuration saved successfully. Restart required for changes to take effect.");
        }
        else
        {
            ShowMessage("Failed to save LSM6DS3 configuration.");
        }
    }

    private async void ShowMessage(string message)
    {
        var dialog = new ContentDialog
        {
            Title = "Message",
            Content = message,
            CloseButtonText = "OK",
            XamlRoot = this.XamlRoot
        };
        await dialog.ShowAsync();
    }

    private async Task UpdateMainWindowNavigation()
    {
        // Use static reference to MainWindow
        if (MainWindow.Instance != null)
        {
            await MainWindow.Instance.UpdateMpu6050NavigationVisibility();
        }
    }
}

