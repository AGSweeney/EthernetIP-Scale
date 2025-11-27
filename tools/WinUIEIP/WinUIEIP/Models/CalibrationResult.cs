using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class CalibrationResult
{
    [JsonPropertyName("status")]
    public string Status { get; set; } = string.Empty;

    [JsonPropertyName("offset_mm")]
    public int? OffsetMm { get; set; }

    [JsonPropertyName("xtalk_cps")]
    public int? XtalkCps { get; set; }

    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;
}

