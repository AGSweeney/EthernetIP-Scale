using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using EulerLink.Models;
using EulerLink.Services;
using Windows.UI;

namespace EulerLink.Views;

public sealed partial class InputAssemblyPage : Page
{
    private readonly DeviceApiService _apiService = DeviceApiService.Instance;
    private DispatcherTimer? _updateTimer;
    private bool _isUpdating = false;
    private ObservableCollection<ByteDisplayItem>? _leftItems;
    private ObservableCollection<ByteDisplayItem>? _rightItems;
    private byte[]? _lastBytes;
    private int? _assemblySize;

    public InputAssemblyPage()
    {
        this.InitializeComponent();
        this.NavigationCacheMode = Microsoft.UI.Xaml.Navigation.NavigationCacheMode.Enabled;
        Loaded += InputAssemblyPage_Loaded;
        Unloaded += InputAssemblyPage_Unloaded;
    }

    protected override async void OnNavigatedTo(Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionState();
        if (_apiService.IsConnected)
        {
            await LoadAssemblySize();
        }
    }

    private void InputAssemblyPage_Loaded(object sender, RoutedEventArgs e)
    {
        UpdateConnectionState();
        _leftItems = new ObservableCollection<ByteDisplayItem>();
        _rightItems = new ObservableCollection<ByteDisplayItem>();
        
        for (int i = 0; i < 32; i++)
        {
            var item = new ByteDisplayItem
            {
                ByteNumber = $"Byte {i}",
                HexValue = $"HEX (0x00)",
                DecValue = $"DEC 0",
                Bits = Enumerable.Range(0, 8).Select(_ => new BitDisplayItem
                {
                    IsSet = false,
                    BackgroundColor = new SolidColorBrush(Microsoft.UI.Colors.Gray)
                }).ToList()
            };
            if (i < 16)
                _leftItems.Add(item);
            else
                _rightItems.Add(item);
        }
        
        AssemblyItemsControlLeft.ItemsSource = _leftItems;
        AssemblyItemsControlRight.ItemsSource = _rightItems;
        
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
        AssemblyContentBorder.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
        AssemblySizesPanel.Visibility = isConnected 
            ? Microsoft.UI.Xaml.Visibility.Visible 
            : Microsoft.UI.Xaml.Visibility.Collapsed;
    }

    private async System.Threading.Tasks.Task LoadAssemblySize()
    {
        var sizes = await _apiService.GetAssemblySizesAsync();
        if (sizes != null)
        {
            _assemblySize = sizes.InputAssemblySize;
            AssemblySizeTextBlock.Text = $"{sizes.InputAssemblySize} bytes";
        }
        else
        {
            AssemblySizeTextBlock.Text = "Unknown";
        }
    }

    private void InputAssemblyPage_Unloaded(object sender, RoutedEventArgs e)
    {
        _updateTimer?.Stop();
    }

    private async void UpdateTimer_Tick(object? sender, object? e)
    {
        if (_isUpdating || !IsLoaded || _leftItems == null || _rightItems == null)
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
            }
            
            var assemblies = await _apiService.GetAssembliesAsync();
            if (assemblies?.InputAssembly100?.RawBytes != null)
            {
                var bytes = assemblies.InputAssembly100.RawBytes;
                if (_lastBytes == null || !BytesEqual(_lastBytes, bytes))
                {
                    _lastBytes = (byte[])bytes.Clone();
                }
                UpdateBytes(bytes);
            }
            else
            {
                var zeroBytes = Enumerable.Repeat((byte)0, 32).ToArray();
                if (_lastBytes == null || !BytesEqual(_lastBytes, zeroBytes))
                {
                    _lastBytes = zeroBytes;
                }
                UpdateBytes(zeroBytes);
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"InputAssemblyPage error: {ex}");
        }
        finally
        {
            _isUpdating = false;
        }
    }
    
    private void UpdateBytes(byte[] bytes)
    {
        if (_leftItems == null || _rightItems == null)
            return;
            
        for (int i = 0; i < 32; i++)
        {
            byte b = i < bytes.Length ? bytes[i] : (byte)0;
            ByteDisplayItem? item = i < 16 ? _leftItems[i] : _rightItems[i - 16];
            
            if (item != null)
            {
                string newHex = $"HEX (0x{b:X2})";
                string newDec = $"DEC {b}";
                
                item.HexValue = newHex;
                item.DecValue = newDec;
                
                for (int bitIndex = 0; bitIndex < 8; bitIndex++)
                {
                    bool isSet = (b & (1 << bitIndex)) != 0;
                    if (item.Bits[bitIndex].IsSet != isSet)
                    {
                        item.Bits[bitIndex].IsSet = isSet;
                        item.Bits[bitIndex].BackgroundColor = new SolidColorBrush(isSet ? Microsoft.UI.Colors.Blue : Microsoft.UI.Colors.Gray);
                    }
                }
            }
        }
    }
    
    private bool BytesEqual(byte[]? a, byte[]? b)
    {
        if (a == null || b == null)
            return a == b;
        if (a.Length != b.Length)
            return false;
        for (int i = 0; i < a.Length; i++)
        {
            if (a[i] != b[i])
                return false;
        }
        return true;
    }
}

