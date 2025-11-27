using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Microsoft.UI.Xaml.Media;
using Windows.UI;

namespace EulerLink.Models;

public class ByteDisplayItem : INotifyPropertyChanged
{
    private string _byteNumber = string.Empty;
    private string _hexValue = string.Empty;
    private string _decValue = string.Empty;
    private List<BitDisplayItem> _bits = new();

    public string ByteNumber
    {
        get => _byteNumber;
        set { _byteNumber = value; OnPropertyChanged(); }
    }

    public string HexValue
    {
        get => _hexValue;
        set 
        { 
            if (_hexValue != value)
            {
                _hexValue = value; 
                OnPropertyChanged();
            }
            else
            {
                _hexValue = string.Empty;
                OnPropertyChanged();
                _hexValue = value;
                OnPropertyChanged();
            }
        }
    }

    public string DecValue
    {
        get => _decValue;
        set 
        { 
            if (_decValue != value)
            {
                _decValue = value; 
                OnPropertyChanged();
            }
            else
            {
                _decValue = string.Empty;
                OnPropertyChanged();
                _decValue = value;
                OnPropertyChanged();
            }
        }
    }

    public List<BitDisplayItem> Bits
    {
        get => _bits;
        set { _bits = value; OnPropertyChanged(); }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

public class BitDisplayItem : INotifyPropertyChanged
{
    private bool _isSet;
    private Brush _backgroundColor = new SolidColorBrush(Microsoft.UI.Colors.Gray);

    public bool IsSet
    {
        get => _isSet;
        set { _isSet = value; OnPropertyChanged(); }
    }

    public Brush BackgroundColor
    {
        get => _backgroundColor;
        set { _backgroundColor = value; OnPropertyChanged(); }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

