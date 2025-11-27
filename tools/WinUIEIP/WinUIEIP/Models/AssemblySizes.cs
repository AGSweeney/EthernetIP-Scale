using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class AssemblySizes
{
    [JsonPropertyName("input_assembly_size")]
    public int InputAssemblySize { get; set; }

    [JsonPropertyName("output_assembly_size")]
    public int OutputAssemblySize { get; set; }
}

