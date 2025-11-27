using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class McpUpdateRate
{
    [JsonPropertyName("update_rate_ms")]
    public int UpdateRateMs { get; set; }

    [JsonPropertyName("update_rate_hz")]
    public double UpdateRateHz { get; set; }
}

