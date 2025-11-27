using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class OtaStatus
{
    [JsonPropertyName("status")]
    public string Status { get; set; } = string.Empty;

    [JsonPropertyName("progress")]
    public int Progress { get; set; }

    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;
}


