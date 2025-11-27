using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EulerLink.Models;

namespace EulerLink.Services;

public class DeviceDiscoveryService
{
    private const int ETHERNET_IP_LISTIDENTITY_PORT = 2222;
    private const int ETHERNET_IP_STANDARD_PORT = 44818;
    private const int DISCOVERY_TIMEOUT_MS = 3000;
    
    public async Task<List<DiscoveredDevice>> DiscoverDevicesAsync(CancellationToken cancellationToken = default)
    {
        var discoveredDevices = new List<DiscoveredDevice>();
        UdpClient? udpClient = null;
        
        try
        {
            udpClient = new UdpClient(0);
            if (udpClient.Client != null)
            {
                udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.Broadcast, true);
                udpClient.Client.ReceiveTimeout = 500;
                udpClient.Client.ReceiveBufferSize = 65536;
                System.Diagnostics.Debug.WriteLine($"UDP receive buffer size: {udpClient.Client.ReceiveBufferSize}");
            }
            udpClient.EnableBroadcast = true;
            
            var startTime = DateTime.Now;
            var deviceIps = new HashSet<string>();
            
            System.Diagnostics.Debug.WriteLine($"Starting discovery loop, timeout: {DISCOVERY_TIMEOUT_MS}ms");
            
            byte[] listIdentityRequest = CreateListIdentityRequest();
            if (listIdentityRequest != null && listIdentityRequest.Length > 0)
            {
                var endPoint44818 = new IPEndPoint(IPAddress.Broadcast, ETHERNET_IP_STANDARD_PORT);
                System.Diagnostics.Debug.WriteLine($"Sending ListIdentity request to broadcast on port {ETHERNET_IP_STANDARD_PORT}");
                await udpClient.SendAsync(listIdentityRequest, listIdentityRequest.Length, endPoint44818);
                System.Diagnostics.Debug.WriteLine($"ListIdentity request sent, starting to listen for responses");
            }
            
            var receiveTasks = new List<Task<UdpReceiveResult>>();
            var maxConcurrentReceives = 10;
            
            while (!cancellationToken.IsCancellationRequested && 
                   (DateTime.Now - startTime).TotalMilliseconds < DISCOVERY_TIMEOUT_MS)
            {
                try
                {
                    if (udpClient == null || udpClient.Client == null)
                    {
                        System.Diagnostics.Debug.WriteLine("UDP client is null, breaking");
                        break;
                    }
                    
                    var remainingTime = DISCOVERY_TIMEOUT_MS - (DateTime.Now - startTime).TotalMilliseconds;
                    if (remainingTime <= 0)
                    {
                        System.Diagnostics.Debug.WriteLine("Discovery timeout reached");
                        break;
                    }
                    
                    while (receiveTasks.Count < maxConcurrentReceives && !cancellationToken.IsCancellationRequested)
                    {
                        receiveTasks.Add(udpClient.ReceiveAsync());
                    }
                    
                    System.Diagnostics.Debug.WriteLine($"Waiting for response (remaining: {remainingTime:F0}ms, found so far: {discoveredDevices.Count}, active receives: {receiveTasks.Count})");
                    var timeoutTask = Task.Delay((int)Math.Min(500, remainingTime), cancellationToken);
                    var allReceivesTask = Task.WhenAny(receiveTasks);
                    var completedTask = await Task.WhenAny(allReceivesTask, timeoutTask);
                    
                    if (completedTask == timeoutTask)
                    {
                        var elapsed = (DateTime.Now - startTime).TotalMilliseconds;
                        System.Diagnostics.Debug.WriteLine($"Timeout waiting for response (elapsed: {elapsed:F0}ms, remaining: {DISCOVERY_TIMEOUT_MS - elapsed:F0}ms, found so far: {discoveredDevices.Count})");
                        continue;
                    }
                    
                    var finishedReceive = await allReceivesTask;
                    receiveTasks.Remove(finishedReceive);
                    
                    if (!finishedReceive.IsFaulted && !finishedReceive.IsCanceled)
                    {
                        try
                        {
                            var result = await finishedReceive;
                            if (result.Buffer != null && result.RemoteEndPoint != null)
                            {
                                string ipAddress = result.RemoteEndPoint.Address?.ToString() ?? string.Empty;
                                System.Diagnostics.Debug.WriteLine($"Received response from {ipAddress}, buffer length: {result.Buffer.Length}");
                                var device = ParseListIdentityResponse(result.Buffer, ipAddress);
                                if (device != null && !string.IsNullOrEmpty(device.IpAddress))
                                {
                                    if (!deviceIps.Contains(device.IpAddress))
                                    {
                                        System.Diagnostics.Debug.WriteLine($"Discovered device: {device.IpAddress}, Product: {device.ProductName}, Vendor: {device.VendorId}, ProductCode: {device.ProductCode}");
                                        deviceIps.Add(device.IpAddress);
                                        discoveredDevices.Add(device);
                                    }
                                    else
                                    {
                                        System.Diagnostics.Debug.WriteLine($"Duplicate device ignored: {device.IpAddress} (already have {discoveredDevices.Count} device(s))");
                                    }
                                }
                                else
                                {
                                    System.Diagnostics.Debug.WriteLine($"Failed to parse response from {ipAddress}, buffer length: {result.Buffer.Length}");
                                }
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine($"Received response but buffer or endpoint is null");
                            }
                        }
                        catch (Exception ex)
                        {
                            System.Diagnostics.Debug.WriteLine($"Error processing response: {ex.Message}");
                        }
                    }
                    else if (finishedReceive.IsFaulted)
                    {
                        System.Diagnostics.Debug.WriteLine($"Receive task faulted: {finishedReceive.Exception?.Message}");
                    }
                }
                catch (SocketException ex) when (ex.SocketErrorCode == SocketError.TimedOut || ex.SocketErrorCode == SocketError.Interrupted)
                {
                    continue;
                }
                catch (ObjectDisposedException)
                {
                    break;
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"Discovery loop error: {ex.Message}");
                    continue;
                }
            }
            
            System.Diagnostics.Debug.WriteLine($"Discovery complete. Found {discoveredDevices.Count} device(s) in {(DateTime.Now - startTime).TotalMilliseconds}ms");
        }
        catch (Exception)
        {
        }
        finally
        {
            try
            {
                udpClient?.Close();
                udpClient?.Dispose();
            }
            catch
            {
            }
        }
        
        return discoveredDevices;
    }
    
    private byte[] CreateListIdentityRequest()
    {
        var request = new List<byte>();
        
        request.AddRange(BitConverter.GetBytes((ushort)0x0063));
        request.AddRange(BitConverter.GetBytes((ushort)0x0000));
        request.AddRange(BitConverter.GetBytes((uint)0x00000000));
        request.AddRange(BitConverter.GetBytes((uint)0x00000000));
        request.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
        request.AddRange(BitConverter.GetBytes((uint)0x00000000));
        
        return request.ToArray();
    }
    
    private DiscoveredDevice? ParseListIdentityResponse(byte[]? data, string ipAddress)
    {
        try
        {
            if (data == null || data.Length < 24)
            {
                System.Diagnostics.Debug.WriteLine($"ParseListIdentityResponse: data is null or too short ({data?.Length ?? 0} bytes)");
                return null;
            }
            
            System.Diagnostics.Debug.WriteLine($"ParseListIdentityResponse: First 24 bytes: {BitConverter.ToString(data, 0, Math.Min(24, data.Length))}");
            
            ushort commandBigEndian = (ushort)((data[0] << 8) | data[1]);
            ushort commandLittleEndian = BitConverter.ToUInt16(data, 0);
            System.Diagnostics.Debug.WriteLine($"Command: big-endian=0x{commandBigEndian:X4}, little-endian=0x{commandLittleEndian:X4}, bytes=[{data[0]:X2} {data[1]:X2}]");
            
            ushort command = commandLittleEndian;
            if (command != 0x0063)
            {
                System.Diagnostics.Debug.WriteLine($"ParseListIdentityResponse: Invalid command 0x{command:X4}, expected 0x0063");
                return null;
            }
            
            ushort commandSpecificLength = BitConverter.ToUInt16(data, 2);
            System.Diagnostics.Debug.WriteLine($"Command specific length: {commandSpecificLength} bytes");
            if (commandSpecificLength == 0 || data.Length < 24 + commandSpecificLength)
            {
                System.Diagnostics.Debug.WriteLine($"ParseListIdentityResponse: Invalid length {commandSpecificLength} or buffer too short ({data.Length} bytes)");
                return null;
            }
            
            uint encapsulationStatus = BitConverter.ToUInt32(data, 8);
            System.Diagnostics.Debug.WriteLine($"Encapsulation status: 0x{encapsulationStatus:X8}");
            if (encapsulationStatus != 0)
            {
                System.Diagnostics.Debug.WriteLine($"ParseListIdentityResponse: Non-zero status 0x{encapsulationStatus:X8}");
                return null;
            }
            
            int offset = 24;
            
            if (offset + 2 > data.Length)
                return null;
            ushort itemCount = BitConverter.ToUInt16(data, offset);
            System.Diagnostics.Debug.WriteLine($"Item count: {itemCount}");
            offset += 2;
            
            if (itemCount == 0)
            {
                System.Diagnostics.Debug.WriteLine($"ParseListIdentityResponse: Item count is 0");
                return null;
            }
            
            for (int i = 0; i < itemCount; i++)
            {
                if (offset + 2 > data.Length)
                    return null;
                ushort itemTypeId = BitConverter.ToUInt16(data, offset);
                offset += 2;
                
                if (offset + 2 > data.Length)
                    return null;
                ushort itemLength = BitConverter.ToUInt16(data, offset);
                System.Diagnostics.Debug.WriteLine($"Item {i}: TypeID=0x{itemTypeId:X4}, Length={itemLength}");
                offset += 2;
                
                if (itemTypeId == 0x000C)
                {
                    if (offset + itemLength > data.Length)
                        return null;
                    
                    int identityStart = offset;
                    int identityEnd = offset + itemLength;
                    
                    System.Diagnostics.Debug.WriteLine($"Parsing Identity Object - start: {identityStart}, end: {identityEnd}, itemLength: {itemLength}");
                    System.Diagnostics.Debug.WriteLine($"Identity Object bytes: {BitConverter.ToString(data, identityStart, Math.Min(itemLength, identityEnd - identityStart))}");
                    
                    if (identityStart + 20 > identityEnd)
                        return null;
                    
                    int socketAddrStart = identityStart;
                    
                    if (identityStart + 18 > identityEnd)
                        return null;
                    
                    identityStart += 18;
                    
                    if (identityStart + 2 > identityEnd)
                        return null;
                    ushort vendorId = BitConverter.ToUInt16(data, identityStart);
                    System.Diagnostics.Debug.WriteLine($"Vendor ID: raw bytes [{data[identityStart]:X2} {data[identityStart + 1]:X2}] = {vendorId} (little-endian)");
                    identityStart += 2;
                    
                    if (identityStart + 2 > identityEnd)
                        return null;
                    ushort deviceType = BitConverter.ToUInt16(data, identityStart);
                    System.Diagnostics.Debug.WriteLine($"Device Type: raw bytes [{data[identityStart]:X2} {data[identityStart + 1]:X2}] = {deviceType} (little-endian)");
                    identityStart += 2;
                    
                    if (identityStart + 2 > identityEnd)
                        return null;
                    ushort productCode = BitConverter.ToUInt16(data, identityStart);
                    System.Diagnostics.Debug.WriteLine($"Product Code: raw bytes [{data[identityStart]:X2} {data[identityStart + 1]:X2}] = {productCode} (little-endian)");
                    identityStart += 2;
                    
                    if (identityStart + 1 > identityEnd)
                        return null;
                    byte revisionMajor = data[identityStart];
                    identityStart += 1;
                    
                    if (identityStart + 1 > identityEnd)
                        return null;
                    byte revisionMinor = data[identityStart];
                    identityStart += 1;
                    
                    if (identityStart + 2 > identityEnd)
                        return null;
                    ushort status = BitConverter.ToUInt16(data, identityStart);
                    identityStart += 2;
                    
                    byte state = (byte)(status & 0x0F);
                    
                    if (identityStart + 4 > identityEnd)
                        return null;
                    uint serialNumber = BitConverter.ToUInt32(data, identityStart);
                    System.Diagnostics.Debug.WriteLine($"Serial Number: raw bytes [{data[identityStart]:X2} {data[identityStart + 1]:X2} {data[identityStart + 2]:X2} {data[identityStart + 3]:X2}] = {serialNumber:X8} (little-endian)");
                    identityStart += 4;
                    
                    if (identityStart >= identityEnd)
                    {
                        System.Diagnostics.Debug.WriteLine($"No product name - reached end of Identity Object");
                        return new DiscoveredDevice
                        {
                            IpAddress = ipAddress ?? string.Empty,
                            VendorId = vendorId,
                            ProductCode = productCode,
                            ProductName = string.Empty,
                            SerialNumber = serialNumber.ToString("X8"),
                            State = state,
                            DeviceType = GetDeviceType(state)
                        };
                    }
                    
                    byte productNameLength = data[identityStart];
                    identityStart += 1;
                    
                    System.Diagnostics.Debug.WriteLine($"Product name length byte: {productNameLength}, remaining bytes: {identityEnd - identityStart}");
                    
                    string productName = string.Empty;
                    if (productNameLength > 0 && identityStart + productNameLength <= identityEnd)
                    {
                        productName = System.Text.Encoding.ASCII.GetString(data, identityStart, productNameLength);
                        System.Diagnostics.Debug.WriteLine($"Product name extracted: '{productName}' (length: {productNameLength})");
                    }
                    else if (productNameLength == 0)
                    {
                        System.Diagnostics.Debug.WriteLine($"Product name length is 0, no product name present");
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine($"Product name extraction failed - length: {productNameLength}, identityStart: {identityStart}, identityEnd: {identityEnd}, available: {identityEnd - identityStart}");
                    }
                    
                    return new DiscoveredDevice
                    {
                        IpAddress = ipAddress ?? string.Empty,
                        VendorId = vendorId,
                        ProductCode = productCode,
                        ProductName = productName,
                        SerialNumber = serialNumber.ToString("X8"),
                        State = state,
                        DeviceType = GetDeviceType(state)
                    };
                }
                else
                {
                    offset += itemLength;
                }
            }
            
            return null;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Error parsing ListIdentity response: {ex.Message}");
            return null;
        }
    }
    
    private string GetDeviceType(byte state)
    {
        return state switch
        {
            0 => "Unknown",
            1 => "Self Testing",
            2 => "Firmware Update",
            3 => "Running",
            4 => "Running (Minor Fault)",
            5 => "Running (Major Fault)",
            6 => "Stopped",
            _ => "Unknown"
        };
    }
}


