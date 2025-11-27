#nullable enable
using System;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Dispatching;
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
        
        if (isConnected && IsLoaded)
        {
            _ = LoadAllConfigs();
        }
    }

    public async Task LoadAllConfigs()
    {
        if (_apiService.IsConnected)
        {
            await LoadNetworkConfig();
            await LoadModbusConfig();
            await LoadI2cConfig();
            await LoadNau7802Config();
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

    private async Task LoadNau7802Config()
    {
        try
        {
            if (!_apiService.IsConnected)
            {
                System.Diagnostics.Debug.WriteLine("LoadNau7802Config: Not connected");
                return;
            }

            int retryCount = 0;
            while ((!IsLoaded || Nau7802ByteOffsetComboBox.Items.Count == 0) && retryCount < 20)
            {
                await Task.Delay(50);
                retryCount++;
            }

            if (Nau7802ByteOffsetComboBox.Items.Count == 0)
            {
                System.Diagnostics.Debug.WriteLine("LoadNau7802Config: ComboBoxes not initialized");
                return;
            }

            System.Diagnostics.Debug.WriteLine("LoadNau7802Config: Calling API");
            var config = await _apiService.GetNau7802ConfigAsync();
            
            if (config == null)
            {
                System.Diagnostics.Debug.WriteLine("LoadNau7802Config: API returned null");
                return;
            }

            System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Got config - Enabled={config.Enabled}, ByteOffset={config.ByteOffset}, Unit={config.Unit}, Gain={config.Gain}, SampleRate={config.SampleRate}, Channel={config.Channel}, LdoValue={config.LdoValue}, Average={config.Average}");
            System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: ComboBox item counts - ByteOffset: {Nau7802ByteOffsetComboBox.Items.Count}, Unit: {Nau7802UnitComboBox.Items.Count}, Gain: {Nau7802GainComboBox.Items.Count}");

            Nau7802EnabledCheckBox.IsChecked = config.Enabled;

            int selectedIndex = -1;
            for (int i = 0; i < Nau7802ByteOffsetComboBox.Items.Count; i++)
            {
                var item = Nau7802ByteOffsetComboBox.Items[i] as ComboBoxItem;
                if (item != null)
                {
                    System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: ByteOffset item {i}: Tag={item.Tag}, Content={item.Content}");
                    if (item.Tag != null)
                    {
                        string tagStr = item.Tag.ToString() ?? "";
                        if (int.TryParse(tagStr, out int tagValue))
                        {
                            System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: ByteOffset item {i}: parsed tagValue={tagValue}, config.ByteOffset={config.ByteOffset}");
                            if (tagValue == config.ByteOffset)
                            {
                                selectedIndex = i;
                                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: ByteOffset MATCH at index {i}");
                                break;
                            }
                        }
                    }
                }
            }
            if (selectedIndex >= 0)
            {
                Nau7802ByteOffsetComboBox.SelectedIndex = selectedIndex;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: ByteOffset {config.ByteOffset} set to index {selectedIndex}, SelectedIndex now={Nau7802ByteOffsetComboBox.SelectedIndex}");
            }
            else
            {
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: ByteOffset {config.ByteOffset} not found in ComboBox (searched {Nau7802ByteOffsetComboBox.Items.Count} items)");
            }

            selectedIndex = -1;
            for (int i = 0; i < Nau7802UnitComboBox.Items.Count; i++)
            {
                var item = Nau7802UnitComboBox.Items[i] as ComboBoxItem;
                if (item != null && item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == config.Unit)
                {
                    selectedIndex = i;
                    break;
                }
            }
            if (selectedIndex >= 0)
            {
                Nau7802UnitComboBox.SelectedIndex = selectedIndex;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Unit {config.Unit} set to index {selectedIndex}");
            }
            else
            {
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Unit {config.Unit} not found in ComboBox");
            }

            selectedIndex = -1;
            for (int i = 0; i < Nau7802GainComboBox.Items.Count; i++)
            {
                var item = Nau7802GainComboBox.Items[i] as ComboBoxItem;
                if (item != null && item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == config.Gain)
                {
                    selectedIndex = i;
                    break;
                }
            }
            if (selectedIndex >= 0)
            {
                Nau7802GainComboBox.SelectedIndex = selectedIndex;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Gain {config.Gain} set to index {selectedIndex}");
            }
            else
            {
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Gain {config.Gain} not found in ComboBox");
            }

            selectedIndex = -1;
            for (int i = 0; i < Nau7802SampleRateComboBox.Items.Count; i++)
            {
                var item = Nau7802SampleRateComboBox.Items[i] as ComboBoxItem;
                if (item != null && item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == config.SampleRate)
                {
                    selectedIndex = i;
                    break;
                }
            }
            if (selectedIndex >= 0)
            {
                Nau7802SampleRateComboBox.SelectedIndex = selectedIndex;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: SampleRate {config.SampleRate} set to index {selectedIndex}");
            }
            else
            {
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: SampleRate {config.SampleRate} not found in ComboBox");
            }

            selectedIndex = -1;
            for (int i = 0; i < Nau7802ChannelComboBox.Items.Count; i++)
            {
                var item = Nau7802ChannelComboBox.Items[i] as ComboBoxItem;
                if (item != null && item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == config.Channel)
                {
                    selectedIndex = i;
                    break;
                }
            }
            if (selectedIndex >= 0)
            {
                Nau7802ChannelComboBox.SelectedIndex = selectedIndex;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Channel {config.Channel} set to index {selectedIndex}");
            }
            else
            {
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Channel {config.Channel} not found in ComboBox");
            }

            selectedIndex = -1;
            for (int i = 0; i < Nau7802LdoComboBox.Items.Count; i++)
            {
                var item = Nau7802LdoComboBox.Items[i] as ComboBoxItem;
                if (item != null && item.Tag != null && int.TryParse(item.Tag.ToString(), out int tagValue) && tagValue == config.LdoValue)
                {
                    selectedIndex = i;
                    break;
                }
            }
            if (selectedIndex >= 0)
            {
                Nau7802LdoComboBox.SelectedIndex = selectedIndex;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: LdoValue {config.LdoValue} set to index {selectedIndex}");
            }
            else
            {
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: LdoValue {config.LdoValue} not found in ComboBox");
            }

            if (config.Average > 0)
            {
                Nau7802AverageNumberBox.Value = config.Average;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Average set to {config.Average}");
            }
            else
            {
                Nau7802AverageNumberBox.Value = 1;
                System.Diagnostics.Debug.WriteLine($"LoadNau7802Config: Average defaulted to 1");
            }

            if (config.CalibrationFactor > 0)
            {
                Nau7802CalibrationStatusTextBlock.Text = $"Calibration Factor: {config.CalibrationFactor:F2}, Zero Offset: {config.ZeroOffset:F2}";
            }
            else
            {
                Nau7802CalibrationStatusTextBlock.Text = "Not calibrated";
            }

            UpdateNau7802ConfigVisibility();
            System.Diagnostics.Debug.WriteLine("LoadNau7802Config: Completed successfully");
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"LoadNau7802Config error: {ex}");
        }
    }

    private void UpdateNau7802ConfigVisibility()
    {
        bool enabled = Nau7802EnabledCheckBox.IsChecked ?? false;
        Nau7802ConfigSection.Visibility = Microsoft.UI.Xaml.Visibility.Visible;
        Nau7802DisabledSection.Visibility = enabled ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
    }

    private void Nau7802EnabledCheckBox_Checked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateNau7802ConfigVisibility();
    }

    private void Nau7802EnabledCheckBox_Unchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        UpdateNau7802ConfigVisibility();
    }

    private async void SaveNau7802Config_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var config = new Nau7802ConfigRequest
        {
            Enabled = Nau7802EnabledCheckBox.IsChecked
        };

        if (Nau7802ByteOffsetComboBox.SelectedItem is ComboBoxItem byteOffsetItem && byteOffsetItem.Tag != null && int.TryParse(byteOffsetItem.Tag.ToString(), out int byteOffset))
        {
            config.ByteOffset = byteOffset;
        }

        if (Nau7802UnitComboBox.SelectedItem is ComboBoxItem unitItem && unitItem.Tag != null && int.TryParse(unitItem.Tag.ToString(), out int unit))
        {
            config.Unit = unit;
        }

        if (Nau7802GainComboBox.SelectedItem is ComboBoxItem gainItem && gainItem.Tag != null && int.TryParse(gainItem.Tag.ToString(), out int gain))
        {
            config.Gain = gain;
        }

        if (Nau7802SampleRateComboBox.SelectedItem is ComboBoxItem sampleRateItem && sampleRateItem.Tag != null && int.TryParse(sampleRateItem.Tag.ToString(), out int sampleRate))
        {
            config.SampleRate = sampleRate;
        }

        if (Nau7802ChannelComboBox.SelectedItem is ComboBoxItem channelItem && channelItem.Tag != null && int.TryParse(channelItem.Tag.ToString(), out int channel))
        {
            config.Channel = channel;
        }

        if (Nau7802LdoComboBox.SelectedItem is ComboBoxItem ldoItem && ldoItem.Tag != null && int.TryParse(ldoItem.Tag.ToString(), out int ldoValue))
        {
            config.LdoValue = ldoValue;
        }

        if (Nau7802AverageNumberBox.Value >= 1 && Nau7802AverageNumberBox.Value <= 50)
        {
            config.Average = (int)Nau7802AverageNumberBox.Value;
        }

        bool success = await _apiService.SaveNau7802ConfigAsync(config);

        if (success)
        {
            ShowRebootRequiredMessage("NAU7802 configuration saved. Reboot required to apply gain, sample rate, channel, or LDO changes.");
            await LoadNau7802Config();
        }
        else
        {
            ShowMessage("Failed to save NAU7802 configuration.");
        }
    }

    private async void Nau7802Tare_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        Nau7802CalibrationStatusTextBlock.Text = "Calibrating...";
        var result = await _apiService.CalibrateNau7802Async("tare");
        if (result != null && result.Status == "ok")
        {
            Nau7802CalibrationStatusTextBlock.Text = $"Tare completed. Zero offset: {result.ZeroOffset:F2}";
            ShowMessage($"Tare calibration completed. Zero offset: {result.ZeroOffset:F2}");
        }
        else
        {
            Nau7802CalibrationStatusTextBlock.Text = "Calibration failed";
            ShowMessage($"Tare calibration failed: {result?.Message ?? "Unknown error"}");
        }
    }

    private async void LoadNau7802Config_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Please connect to device first.");
            return;
        }
        await LoadNau7802Config();
        ShowMessage("NAU7802 settings loaded.");
    }

    private async void Nau7802Calibrate_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var knownWeightBox = new NumberBox
        {
            Minimum = 0.1,
            Maximum = 10000,
            Value = 100,
            Width = 200,
            HorizontalAlignment = Microsoft.UI.Xaml.HorizontalAlignment.Left
        };

        var stackPanel = new StackPanel();
        stackPanel.Children.Add(new TextBlock { Text = "Enter the known weight value:", Margin = new Microsoft.UI.Xaml.Thickness(0, 0, 0, 8) });
        stackPanel.Children.Add(knownWeightBox);

        var dialog = new ContentDialog
        {
            Title = "Calibrate with Known Weight",
            Content = stackPanel,
            PrimaryButtonText = "Calibrate",
            CloseButtonText = "Cancel",
            XamlRoot = this.XamlRoot
        };

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            float knownWeight = (float)knownWeightBox.Value;

            Nau7802CalibrationStatusTextBlock.Text = "Calibrating...";
            var calibrationResult = await _apiService.CalibrateNau7802Async("calibrate", knownWeight);
            if (calibrationResult != null && calibrationResult.Status == "ok")
            {
                Nau7802CalibrationStatusTextBlock.Text = $"Calibration completed. Factor: {calibrationResult.CalibrationFactor:F2}, Zero offset: {calibrationResult.ZeroOffset:F2}";
                ShowMessage($"Calibration completed successfully. Calibration factor: {calibrationResult.CalibrationFactor:F2}, Zero offset: {calibrationResult.ZeroOffset:F2}");
            }
            else
            {
                Nau7802CalibrationStatusTextBlock.Text = "Calibration failed";
                ShowMessage($"Calibration failed: {calibrationResult?.Message ?? "Unknown error"}");
            }
        }
    }

    private async void Nau7802AfeCalibrate_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_apiService.IsConnected)
        {
            ShowMessage("Not connected to device.");
            return;
        }

        var dialog = new ContentDialog
        {
            Title = "AFE Calibration",
            Content = "AFE (Analog Front End) calibration calibrates the chip's internal hardware. This is required after changing gain or sample rate settings. The scale must be empty and stable. This may take a few seconds.",
            PrimaryButtonText = "Start AFE Calibration",
            CloseButtonText = "Cancel",
            XamlRoot = this.XamlRoot
        };

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            Nau7802CalibrationStatusTextBlock.Text = "AFE calibrating...";
            var calibrationResult = await _apiService.CalibrateNau7802Async("afe");
            if (calibrationResult != null && calibrationResult.Status == "ok")
            {
                Nau7802CalibrationStatusTextBlock.Text = "AFE calibration completed successfully";
                ShowMessage("AFE calibration completed successfully");
            }
            else
            {
                Nau7802CalibrationStatusTextBlock.Text = "AFE calibration failed";
                ShowMessage($"AFE calibration failed: {calibrationResult?.Message ?? "Unknown error"}");
            }
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

}

