using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class Mpu6050Status
{
    [JsonPropertyName("roll")]
    public float Roll { get; set; }

    [JsonPropertyName("pitch")]
    public float Pitch { get; set; }

    [JsonPropertyName("ground_angle")]
    public float GroundAngle { get; set; }

    [JsonPropertyName("bottom_pressure_psi")]
    public float BottomPressurePsi { get; set; }

    [JsonPropertyName("top_pressure_psi")]
    public float TopPressurePsi { get; set; }

    [JsonPropertyName("roll_scaled")]
    public int RollScaled { get; set; }

    [JsonPropertyName("pitch_scaled")]
    public int PitchScaled { get; set; }

    [JsonPropertyName("ground_angle_scaled")]
    public int GroundAngleScaled { get; set; }

    [JsonPropertyName("bottom_pressure_scaled")]
    public int BottomPressureScaled { get; set; }

    [JsonPropertyName("top_pressure_scaled")]
    public int TopPressureScaled { get; set; }

    [JsonPropertyName("enabled")]
    public bool Enabled { get; set; }

    [JsonPropertyName("byte_offset")]
    public int ByteOffset { get; set; }

    [JsonPropertyName("byte_range_start")]
    public int ByteRangeStart { get; set; }

    [JsonPropertyName("byte_range_end")]
    public int ByteRangeEnd { get; set; }
}

