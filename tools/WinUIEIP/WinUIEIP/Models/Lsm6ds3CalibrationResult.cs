using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class Lsm6ds3CalibrationResult
{
    [JsonPropertyName("status")]
    public string Status { get; set; } = string.Empty;

    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;

    [JsonPropertyName("calibrated")]
    public bool Calibrated { get; set; }

    [JsonPropertyName("gyro_offset_x_mdps")]
    public float GyroOffsetXMdps { get; set; }

    [JsonPropertyName("gyro_offset_y_mdps")]
    public float GyroOffsetYMdps { get; set; }

    [JsonPropertyName("gyro_offset_z_mdps")]
    public float GyroOffsetZMdps { get; set; }

    [JsonPropertyName("samples")]
    public int? Samples { get; set; }

    [JsonPropertyName("sample_delay_ms")]
    public int? SampleDelayMs { get; set; }
}

public class Lsm6ds3CalibrationStatus
{
    [JsonPropertyName("calibrated")]
    public bool Calibrated { get; set; }

    [JsonPropertyName("gyro_offset_x_mdps")]
    public float GyroOffsetXMdps { get; set; }

    [JsonPropertyName("gyro_offset_y_mdps")]
    public float GyroOffsetYMdps { get; set; }

    [JsonPropertyName("gyro_offset_z_mdps")]
    public float GyroOffsetZMdps { get; set; }
}

