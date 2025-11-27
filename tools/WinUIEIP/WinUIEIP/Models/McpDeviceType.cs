using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class McpDeviceType
{
    [JsonPropertyName("device_type")]
    public int DeviceType { get; set; }

    [JsonPropertyName("device_name")]
    public string DeviceName { get; set; } = string.Empty;
}

