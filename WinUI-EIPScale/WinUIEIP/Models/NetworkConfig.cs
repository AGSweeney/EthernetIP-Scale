using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class NetworkConfig
{
    [JsonPropertyName("use_dhcp")]
    public bool UseDhcp { get; set; }

    [JsonPropertyName("ip_address")]
    public string IpAddress { get; set; } = string.Empty;

    [JsonPropertyName("netmask")]
    public string Netmask { get; set; } = string.Empty;

    [JsonPropertyName("gateway")]
    public string Gateway { get; set; } = string.Empty;

    [JsonPropertyName("dns1")]
    public string Dns1 { get; set; } = string.Empty;

    [JsonPropertyName("dns2")]
    public string Dns2 { get; set; } = string.Empty;
}

