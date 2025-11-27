using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class SensorStatus
{
    [JsonPropertyName("distance_mm")]
    public int DistanceMm { get; set; }

    [JsonPropertyName("status")]
    public int Status { get; set; }

    [JsonPropertyName("ambient_kcps")]
    public int AmbientKcps { get; set; }

    [JsonPropertyName("sig_per_spad_kcps")]
    public int SignalPerSpadKcps { get; set; }

    [JsonPropertyName("num_spads")]
    public int NumSpads { get; set; }

    [JsonPropertyName("distance_mode")]
    public int DistanceMode { get; set; }

    [JsonPropertyName("input_assembly_100")]
    public AssemblyDataItem? InputAssembly100 { get; set; }

    [JsonPropertyName("output_assembly_150")]
    public AssemblyDataItem? OutputAssembly150 { get; set; }
}

public class AssemblyDataItem
{
    [JsonPropertyName("raw_bytes")]
    [JsonConverter(typeof(ByteArrayConverter))]
    public byte[]? RawBytes { get; set; }
}

