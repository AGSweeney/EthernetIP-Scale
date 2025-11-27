using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class CalibrationResult
{
    [JsonPropertyName("status")]
    public string Status { get; set; } = string.Empty;

    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;

    [JsonPropertyName("zero_offset")]
    public float? ZeroOffset { get; set; }

    [JsonPropertyName("calibration_factor")]
    public float? CalibrationFactor { get; set; }
}

