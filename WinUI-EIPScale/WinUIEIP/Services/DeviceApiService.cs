#nullable enable
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
            
            var responseJson = await response.Content.ReadAsStringAsync();
            System.Diagnostics.Debug.WriteLine($"SetModbusEnabledAsync: HTTP {response.StatusCode} - {responseJson}");
            
            if (!response.IsSuccessStatusCode)
            {
                return false;
            }
            
            // Parse response to check status field
            var result = JsonSerializer.Deserialize<JsonElement>(responseJson);
            if (result.TryGetProperty("status", out var statusProperty))
            {
                var status = statusProperty.GetString();
                if (status == "ok")
                {
                    return true;
                }
                else if (status == "error")
                {
                    var message = result.TryGetProperty("message", out var msgProperty) 
                        ? msgProperty.GetString() 
                        : "Unknown error";
                    System.Diagnostics.Debug.WriteLine($"SetModbusEnabledAsync: Error status - {message}");
                    return false;
                }
            }
            
            // If no status field, assume success if HTTP status was OK
            return true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"SetModbusEnabledAsync exception: {ex.Message}");
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

    public async Task<Nau7802Config?> GetNau7802ConfigAsync()
    {
        try
        {
            var url = $"{GetBaseUrl()}/api/nau7802";
            System.Diagnostics.Debug.WriteLine($"GetNau7802ConfigAsync: Calling {url}");
            var response = await _httpClient.GetAsync(url);
            response.EnsureSuccessStatusCode();
            var json = await response.Content.ReadAsStringAsync();
            System.Diagnostics.Debug.WriteLine($"GetNau7802ConfigAsync: Response: {json}");
            
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;
            using var stream = new System.IO.MemoryStream();
            using (var writer = new System.Text.Json.Utf8JsonWriter(stream))
            {
                writer.WriteStartObject();
                foreach (var prop in root.EnumerateObject())
                {
                    if (prop.Name == "unit" && prop.Value.ValueKind == System.Text.Json.JsonValueKind.String)
                    {
                        continue;
                    }
                    prop.WriteTo(writer);
                }
                writer.WriteEndObject();
            }
            stream.Position = 0;
            var cleanedJson = System.Text.Encoding.UTF8.GetString(stream.ToArray());
            System.Diagnostics.Debug.WriteLine($"GetNau7802ConfigAsync: Cleaned JSON: {cleanedJson}");
            
            var config = JsonSerializer.Deserialize<Nau7802Config>(cleanedJson);
            System.Diagnostics.Debug.WriteLine($"GetNau7802ConfigAsync: Deserialized - Enabled={config?.Enabled}, ByteOffset={config?.ByteOffset}");
            return config;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"GetNau7802ConfigAsync error: {ex.Message}");
            return null;
        }
    }

    public async Task<bool> SaveNau7802ConfigAsync(Nau7802ConfigRequest config)
    {
        try
        {
            var json = JsonSerializer.Serialize(config);
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/nau7802", content);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<CalibrationResult?> CalibrateNau7802Async(string action, float? knownWeight = null, int samples = 10)
    {
        try
        {
            var requestBody = new Dictionary<string, object>
            {
                ["action"] = action
            };
            
            if (action == "afe")
            {
            }
            else
            {
                requestBody["samples"] = samples;
                if (knownWeight.HasValue)
                {
                    requestBody["known_weight"] = knownWeight.Value;
                }
            }

            var json = JsonSerializer.Serialize(requestBody);
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _httpClient.PostAsync($"{GetBaseUrl()}/api/nau7802/calibrate", content);
            response.EnsureSuccessStatusCode();
            var responseJson = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<CalibrationResult>(responseJson);
        }
        catch
        {
            return null;
        }
    }
}

