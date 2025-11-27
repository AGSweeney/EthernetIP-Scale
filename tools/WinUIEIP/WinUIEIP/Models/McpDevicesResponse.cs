using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class McpDevicesResponse
{
    [JsonPropertyName("devices")]
    public List<McpDeviceConfig> Devices { get; set; } = new();

    [JsonPropertyName("count")]
    public int Count { get; set; }
}

