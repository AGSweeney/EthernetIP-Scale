#nullable enable
using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class Nau7802Config
{
    [JsonPropertyName("enabled")]
    public bool Enabled { get; set; }

    [JsonPropertyName("byte_offset")]
    public int ByteOffset { get; set; }

    [JsonPropertyName("unit")]
    public int Unit { get; set; }

    [JsonPropertyName("unit_label")]
    public string? UnitLabel { get; set; }

    [JsonPropertyName("gain")]
    public int Gain { get; set; }

    [JsonPropertyName("gain_label")]
    public string? GainLabel { get; set; }

    [JsonPropertyName("sample_rate")]
    public int SampleRate { get; set; }

    [JsonPropertyName("sample_rate_label")]
    public string? SampleRateLabel { get; set; }

    [JsonPropertyName("channel")]
    public int Channel { get; set; }

    [JsonPropertyName("channel_label")]
    public string? ChannelLabel { get; set; }

    [JsonPropertyName("ldo_value")]
    public int LdoValue { get; set; }

    [JsonPropertyName("ldo_voltage")]
    public float LdoVoltage { get; set; }

    [JsonPropertyName("average")]
    public int Average { get; set; }

    [JsonPropertyName("initialized")]
    public bool Initialized { get; set; }

    [JsonPropertyName("connected")]
    public bool Connected { get; set; }

    [JsonPropertyName("available")]
    public bool Available { get; set; }

    [JsonPropertyName("weight")]
    public float Weight { get; set; }

    [JsonPropertyName("raw_reading")]
    public int RawReading { get; set; }

    [JsonPropertyName("calibration_factor")]
    public float CalibrationFactor { get; set; }

    [JsonPropertyName("zero_offset")]
    public float ZeroOffset { get; set; }

    [JsonPropertyName("revision_code")]
    public int RevisionCode { get; set; }

    [JsonPropertyName("channel1")]
    public Nau7802ChannelCalibration? Channel1 { get; set; }

    [JsonPropertyName("channel2")]
    public Nau7802ChannelCalibration? Channel2 { get; set; }

    [JsonPropertyName("status")]
    public Nau7802Status? Status { get; set; }
}

public class Nau7802ChannelCalibration
{
    [JsonPropertyName("offset")]
    public int Offset { get; set; }

    [JsonPropertyName("gain")]
    public int Gain { get; set; }
}

public class Nau7802Status
{
    [JsonPropertyName("available")]
    public bool Available { get; set; }

    [JsonPropertyName("power_digital")]
    public bool PowerDigital { get; set; }

    [JsonPropertyName("power_analog")]
    public bool PowerAnalog { get; set; }

    [JsonPropertyName("power_regulator")]
    public bool PowerRegulator { get; set; }

    [JsonPropertyName("calibration_active")]
    public bool CalibrationActive { get; set; }

    [JsonPropertyName("calibration_error")]
    public bool CalibrationError { get; set; }

    [JsonPropertyName("oscillator_ready")]
    public bool OscillatorReady { get; set; }

    [JsonPropertyName("avdd_ready")]
    public bool AvddReady { get; set; }
}

public class Nau7802ConfigRequest
{
    [JsonPropertyName("enabled")]
    public bool? Enabled { get; set; }

    [JsonPropertyName("byte_offset")]
    public int? ByteOffset { get; set; }

    [JsonPropertyName("unit")]
    public int? Unit { get; set; }

    [JsonPropertyName("gain")]
    public int? Gain { get; set; }

    [JsonPropertyName("sample_rate")]
    public int? SampleRate { get; set; }

    [JsonPropertyName("channel")]
    public int? Channel { get; set; }

    [JsonPropertyName("ldo_value")]
    public int? LdoValue { get; set; }

    [JsonPropertyName("average")]
    public int? Average { get; set; }
}

