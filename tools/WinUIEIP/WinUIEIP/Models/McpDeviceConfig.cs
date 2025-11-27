using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class McpDeviceConfig
{
    [JsonPropertyName("i2c_address")]
    public int I2cAddress { get; set; }

    [JsonPropertyName("device_type")]
    public int DeviceType { get; set; }

    [JsonPropertyName("device_name")]
    public string? DeviceName { get; set; }

    [JsonPropertyName("enabled")]
    public bool Enabled { get; set; }

    [JsonPropertyName("pin_directions")]
    public int PinDirections { get; set; }

    [JsonPropertyName("input_byte_start")]
    public int InputByteStart { get; set; }

    [JsonPropertyName("output_byte_start")]
    public int OutputByteStart { get; set; }

    [JsonPropertyName("output_logic_inverted")]
    public bool OutputLogicInverted { get; set; }

    [JsonPropertyName("detected")]
    public bool? Detected { get; set; }
}

