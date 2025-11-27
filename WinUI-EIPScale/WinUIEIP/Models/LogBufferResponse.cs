using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class LogBufferResponse
{
    [JsonPropertyName("status")]
    public string Status { get; set; } = string.Empty;

    [JsonPropertyName("logs")]
    public string Logs { get; set; } = string.Empty;

    [JsonPropertyName("size")]
    public int Size { get; set; }

    [JsonPropertyName("total_size")]
    public int TotalSize { get; set; }

    [JsonPropertyName("truncated")]
    public bool Truncated { get; set; }
}

