using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using EulerLink.Models;

namespace EulerLink.Services;

public class DeviceApiService
{
    private static DeviceApiService? _instance;
    private readonly HttpClient _httpClient;
    private string _deviceIp = "172.16.82.99";
    private bool _isConnected = false;

    public static DeviceApiService Instance
    {
        get
        {
            if (_instance == null)
            {
                _instance = new DeviceApiService();
            }
            return _instance;
        }
    }

    private DeviceApiService()
    {
        _httpClient = new HttpClient();
        _httpClient.Timeout = TimeSpan.FromSeconds(10);
    }

    public bool IsConnected => _isConnected;

    public string GetDeviceIp() => _deviceIp;

    public void SetDeviceIp(string ip)
    {
        _deviceIp = ip;
    }

    public void SetConnected(bool connected)
    {
        _isConnected = connected;
    }

    private string GetBaseUrl() => $"http://{_deviceIp}";

    public async Task<NetworkConfig?> GetNetworkConfigAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/ipconfig");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<NetworkConfig>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SaveNetworkConfigAsync(NetworkConfig config)
    {
        try
        {
            var json = JsonSerializer.Serialize(config);
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/ipconfig", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<bool?> IsModbusEnabledAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/modbus");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("enabled").GetBoolean();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetModbusEnabledAsync(bool enabled)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { enabled });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/modbus", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<AssemblyData?> GetAssembliesAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/status");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            
            var result = new AssemblyData
            {
                InputAssembly100 = new Models.AssemblyDataItem(),
                OutputAssembly150 = new Models.AssemblyDataItem()
            };
            
            using (JsonDocument doc = JsonDocument.Parse(json))
            {
                var root = doc.RootElement;
                
                if (root.TryGetProperty("input_assembly_100", out var inputAssembly))
                {
                    if (inputAssembly.TryGetProperty("raw_bytes", out var rawBytes) && rawBytes.ValueKind == JsonValueKind.Array)
                    {
                        var bytes = new byte[32];
                        int index = 0;
                        foreach (var element in rawBytes.EnumerateArray())
                        {
                            if (index >= 32) break;
                            if (element.ValueKind == JsonValueKind.Number)
                            {
                                bytes[index++] = (byte)element.GetInt32();
                            }
                        }
                        result.InputAssembly100.RawBytes = bytes;
                    }
                }
                
                if (root.TryGetProperty("output_assembly_150", out var outputAssembly))
                {
                    if (outputAssembly.TryGetProperty("raw_bytes", out var rawBytes) && rawBytes.ValueKind == JsonValueKind.Array)
                    {
                        var bytes = new byte[32];
                        int index = 0;
                        foreach (var element in rawBytes.EnumerateArray())
                        {
                            if (index >= 32) break;
                            if (element.ValueKind == JsonValueKind.Number)
                            {
                                bytes[index++] = (byte)element.GetInt32();
                            }
                        }
                        result.OutputAssembly150.RawBytes = bytes;
                    }
                }
            }
            
            return result;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"GetAssembliesAsync error: {ex.Message}");
            return null;
        }
    }

    public async Task<bool> UploadFirmwareAsync(string filePath)
    {
        try
        {
            using var fileStream = File.OpenRead(filePath);
            using var content = new MultipartFormDataContent();
            using var fileContent = new StreamContent(fileStream);
            fileContent.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("application/octet-stream");
            content.Add(fileContent, "firmware", Path.GetFileName(filePath));

            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/ota/update", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<OtaStatus?> GetOtaStatusAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/ota/status");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<OtaStatus>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<AssemblySizes?> GetAssemblySizesAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/assemblies/sizes");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<AssemblySizes>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> RebootDeviceAsync()
    {
        try
        {
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/reboot", null);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<bool?> IsI2cPullupEnabledAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/i2c/pullup");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("enabled").GetBoolean();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetI2cPullupEnabledAsync(bool enabled)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { enabled });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/i2c/pullup", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<LogBufferResponse?> GetLogsAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/logs");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<LogBufferResponse>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool?> IsMpu6050EnabledAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/mpu6050/enabled");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("enabled").GetBoolean();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetMpu6050EnabledAsync(bool enabled)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { enabled });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/mpu6050/enabled", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<int?> GetMpu6050ByteOffsetAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/mpu6050/byteoffset");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("start_byte").GetInt32();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetMpu6050ByteOffsetAsync(int startByte)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { start_byte = startByte });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/mpu6050/byteoffset", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<Mpu6050Status?> GetMpu6050StatusAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/mpu6050/status");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<Mpu6050Status>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<int?> GetToolWeightAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/mpu6050/toolweight");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("tool_weight").GetInt32();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetToolWeightAsync(int toolWeight)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { tool_weight = toolWeight });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/mpu6050/toolweight", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<int?> GetTipForceAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/mpu6050/tipforce");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("tip_force").GetInt32();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetTipForceAsync(int tipForce)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { tip_force = tipForce });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/mpu6050/tipforce", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<float?> GetCylinderBoreAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/mpu6050/cylinderbore");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return (float)result.GetProperty("cylinder_bore").GetDouble();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetCylinderBoreAsync(float cylinderBore)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { cylinder_bore = cylinderBore });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/mpu6050/cylinderbore", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<bool?> IsLsm6ds3EnabledAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/lsm6ds3/enabled");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("enabled").GetBoolean();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetLsm6ds3EnabledAsync(bool enabled)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { enabled });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/lsm6ds3/enabled", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<int?> GetLsm6ds3ByteOffsetAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/lsm6ds3/byteoffset");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            var result = JsonSerializer.Deserialize<JsonElement>(json);
            return result.GetProperty("start_byte").GetInt32();
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> SetLsm6ds3ByteOffsetAsync(int startByte)
    {
        try
        {
            var json = JsonSerializer.Serialize(new { start_byte = startByte });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/lsm6ds3/byteoffset", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<Mpu6050Status?> GetLsm6ds3StatusAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/lsm6ds3/status");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<Mpu6050Status>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<Lsm6ds3CalibrationStatus?> GetLsm6ds3CalibrationStatusAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync($"{GetBaseUrl()}/api/lsm6ds3/calibrate");
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<Lsm6ds3CalibrationStatus>(json);
        }
        catch
        {
            return null;
        }
    }

    public async Task<Lsm6ds3CalibrationResult?> CalibrateLsm6ds3Async(int? samples = null, int? sampleDelayMs = null)
    {
        try
        {
            var requestBody = new Dictionary<string, object>();
            if (samples.HasValue)
            {
                requestBody["samples"] = samples.Value;
            }
            if (sampleDelayMs.HasValue)
            {
                requestBody["sample_delay_ms"] = sampleDelayMs.Value;
            }

            var json = JsonSerializer.Serialize(requestBody);
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/lsm6ds3/calibrate", content);
            response.EnsureSuccessStatusCode();
            var responseJson = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<Lsm6ds3CalibrationResult>(responseJson);
        }
        catch
        {
            return null;
        }
    }
}

