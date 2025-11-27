using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class AssemblyData
{
    [JsonPropertyName("input_assembly_100")]
    public AssemblyDataItem? InputAssembly100 { get; set; }

    [JsonPropertyName("output_assembly_150")]
    public AssemblyDataItem? OutputAssembly150 { get; set; }
}

