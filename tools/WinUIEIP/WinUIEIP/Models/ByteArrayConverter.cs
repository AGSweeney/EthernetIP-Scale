using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class ByteArrayConverter : JsonConverter<byte[]>
{
    public override byte[]? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.Null)
        {
            System.Diagnostics.Debug.WriteLine("ByteArrayConverter: Received null token");
            return null;
        }

        if (reader.TokenType == JsonTokenType.StartArray)
        {
            var intList = new List<int>();
            while (reader.Read() && reader.TokenType != JsonTokenType.EndArray)
            {
                if (reader.TokenType == JsonTokenType.Number)
                {
                    int value = reader.GetInt32();
                    if (value < 0 || value > 255)
                    {
                        System.Diagnostics.Debug.WriteLine($"ByteArrayConverter: Warning - value {value} out of byte range, clamping");
                        value = Math.Clamp(value, 0, 255);
                    }
                    intList.Add(value);
                }
                else if (reader.TokenType != JsonTokenType.EndArray)
                {
                    System.Diagnostics.Debug.WriteLine($"ByteArrayConverter: Unexpected token in array: {reader.TokenType}");
                }
            }
            var result = intList.Select(i => (byte)i).ToArray();
            System.Diagnostics.Debug.WriteLine($"ByteArrayConverter: Converted {result.Length} bytes");
            return result;
        }

        System.Diagnostics.Debug.WriteLine($"ByteArrayConverter: Unexpected token type: {reader.TokenType}");
        throw new JsonException($"Unexpected token type: {reader.TokenType}");
    }

    public override void Write(Utf8JsonWriter writer, byte[] value, JsonSerializerOptions options)
    {
        if (value == null)
        {
            writer.WriteNullValue();
            return;
        }

        writer.WriteStartArray();
        foreach (var b in value)
        {
            writer.WriteNumberValue(b);
        }
        writer.WriteEndArray();
    }
}

