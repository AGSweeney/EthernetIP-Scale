using System;
using System.Threading.Tasks;
using Sres.Net.EEIP;
using EulerLink.Models;

namespace EulerLink.Services;

public class EtherNetIPService
{
    private static EtherNetIPService? _instance;
    private EEIPClient? _eeipClient;
    private string _deviceIp = "172.16.82.99";
    private bool _isConnected = false;
    private int _inputAssemblySize = 32;
    private int _outputAssemblySize = 32;
    private byte[]? _lastAssemblyData;

    public static EtherNetIPService Instance
    {
        get
        {
            if (_instance == null)
            {
                _instance = new EtherNetIPService();
            }
            return _instance;
        }
    }

    private EtherNetIPService()
    {
    }

    public bool IsConnected => _isConnected;

    public void SetDeviceIp(string ip)
    {
        _deviceIp = ip;
    }

    public void SetAssemblySizes(int inputSize, int outputSize)
    {
        _inputAssemblySize = inputSize;
        _outputAssemblySize = outputSize;
    }

    public async Task<bool> ConnectAsync()
    {
        try
        {
            return await Task.Run(() =>
            {
                System.Diagnostics.Debug.WriteLine($"EtherNet/IP: Attempting to connect to {_deviceIp}...");
                
                try
                {
                    _eeipClient = new EEIPClient();
                    _eeipClient.IPAddress = _deviceIp;
                    
                    System.Diagnostics.Debug.WriteLine("EtherNet/IP: Registering session...");
                    _eeipClient.RegisterSession();
                    System.Diagnostics.Debug.WriteLine("EtherNet/IP: Session registered");
                    
                    System.Diagnostics.Debug.WriteLine("EtherNet/IP: Verifying assembly exists via explicit messaging...");
                    try
                    {
                        var assemblyObject = _eeipClient.AssemblyObject;
                        var testData = assemblyObject.getInstance((byte)0x64);
                        System.Diagnostics.Debug.WriteLine($"EtherNet/IP: Input Assembly 100 exists, size: {testData?.Length ?? 0} bytes");
                        
                        if (testData != null && testData.Length > 0)
                        {
                            _lastAssemblyData = (byte[])testData.Clone();
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.Debug.WriteLine($"EtherNet/IP: Could not verify assembly via explicit messaging: {ex.Message}");
                        return false;
                    }
                    
                    System.Diagnostics.Debug.WriteLine("EtherNet/IP: Using explicit messaging mode (implicit messaging connection failed)");
                    System.Diagnostics.Debug.WriteLine("EtherNet/IP: Will read assembly data via explicit messaging");
                    
                    _isConnected = true;
                    System.Diagnostics.Debug.WriteLine($"EtherNet/IP: Successfully connected to {_deviceIp} (explicit messaging mode)");
                    return true;
                }
                catch (Exception innerEx)
                {
                    System.Diagnostics.Debug.WriteLine($"EtherNet/IP connection error (inner): {innerEx.GetType().Name}: {innerEx.Message}");
                    System.Diagnostics.Debug.WriteLine($"Stack trace: {innerEx.StackTrace}");
                    if (innerEx.InnerException != null)
                    {
                        System.Diagnostics.Debug.WriteLine($"Inner exception: {innerEx.InnerException.GetType().Name}: {innerEx.InnerException.Message}");
                    }
                    _isConnected = false;
                    try
                    {
                        if (_eeipClient != null)
                        {
                            _eeipClient.UnRegisterSession();
                        }
                    }
                    catch { }
                    _eeipClient = null;
                    return false;
                }
            });
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"EtherNet/IP connection error (outer): {ex.GetType().Name}: {ex.Message}");
            _isConnected = false;
            _eeipClient = null;
            return false;
        }
    }

    public async Task<bool> DisconnectAsync()
    {
        try
        {
            await Task.Run(() =>
            {
                if (_eeipClient != null)
                {
                    try
                    {
                        _eeipClient.ForwardClose();
                    }
                    catch { }
                    
                    try
                    {
                        _eeipClient.UnRegisterSession();
                    }
                    catch { }
                }

                _eeipClient = null;
                _isConnected = false;
                _lastAssemblyData = null;
                System.Diagnostics.Debug.WriteLine("EtherNet/IP: Disconnected");
            });

            return true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"EtherNet/IP disconnect error: {ex.Message}");
            return false;
        }
    }

    public byte[]? ReadInputAssembly()
    {
        if (_eeipClient == null)
        {
            return null;
        }

        try
        {
            if (_isConnected)
            {
                var ioData = _eeipClient.T_O_IOData;
                if (ioData != null && ioData.Length > 0)
                {
                    _lastAssemblyData = (byte[])ioData.Clone();
                    return _lastAssemblyData;
                }
            }
            
            var assemblyObject = _eeipClient.AssemblyObject;
            var assemblyData = assemblyObject.getInstance((byte)0x64);
            if (assemblyData != null && assemblyData.Length > 0)
            {
                _lastAssemblyData = (byte[])assemblyData.Clone();
                return _lastAssemblyData;
            }
        }
        catch (Sres.Net.EEIP.CIPException cipEx)
        {
            System.Diagnostics.Debug.WriteLine($"EtherNet/IP CIP error (non-fatal): {cipEx.Message}");
            return _lastAssemblyData;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"EtherNet/IP read error: {ex.GetType().Name}: {ex.Message}");
            return _lastAssemblyData;
        }

        return _lastAssemblyData;
    }

    public Mpu6050Status? ParseMpu6050Data(byte[] assemblyData, int byteOffset)
    {
        if (assemblyData == null || byteOffset < 0 || byteOffset + 20 > assemblyData.Length)
        {
            System.Diagnostics.Debug.WriteLine($"MPU6050 parse: Invalid parameters - data: {assemblyData != null}, offset: {byteOffset}, length: {assemblyData?.Length ?? 0}");
            return null;
        }

        try
        {
            var status = new Mpu6050Status
            {
                Enabled = true,
                ByteOffset = byteOffset,
                ByteRangeStart = byteOffset,
                ByteRangeEnd = byteOffset + 20
            };

            int offset = byteOffset;
            
            System.Diagnostics.Debug.WriteLine($"MPU6050 parse: Reading from offset {offset}, bytes: {BitConverter.ToString(assemblyData, offset, Math.Min(20, assemblyData.Length - offset))}");
            
            status.RollScaled = BitConverter.ToInt32(assemblyData, offset);
            status.Roll = status.RollScaled / 10000.0f;
            offset += 4;

            status.PitchScaled = BitConverter.ToInt32(assemblyData, offset);
            status.Pitch = status.PitchScaled / 10000.0f;
            offset += 4;

            status.GroundAngleScaled = BitConverter.ToInt32(assemblyData, offset);
            status.GroundAngle = status.GroundAngleScaled / 10000.0f;
            offset += 4;

            status.BottomPressureScaled = BitConverter.ToInt32(assemblyData, offset);
            status.BottomPressurePsi = status.BottomPressureScaled / 1000.0f;
            offset += 4;

            status.TopPressureScaled = BitConverter.ToInt32(assemblyData, offset);
            status.TopPressurePsi = status.TopPressureScaled / 1000.0f;

            System.Diagnostics.Debug.WriteLine($"MPU6050 parse: RollScaled={status.RollScaled} ({status.Roll:F2}°), PitchScaled={status.PitchScaled} ({status.Pitch:F2}°), GroundAngleScaled={status.GroundAngleScaled} ({status.GroundAngle:F2}°), BottomPressureScaled={status.BottomPressureScaled} ({status.BottomPressurePsi:F2} PSI), TopPressureScaled={status.TopPressureScaled} ({status.TopPressurePsi:F2} PSI)");

            return status;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"MPU6050 parse error: {ex.GetType().Name}: {ex.Message}");
            return null;
        }
    }
}

