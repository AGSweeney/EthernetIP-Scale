namespace EulerLink.Models;

public class DiscoveredDevice
{
    public string IpAddress { get; set; } = string.Empty;
    public ushort VendorId { get; set; }
    public ushort ProductCode { get; set; }
    public string ProductName { get; set; } = string.Empty;
    public string SerialNumber { get; set; } = string.Empty;
    public byte State { get; set; }
    public string DeviceType { get; set; } = string.Empty;
}

