using System.Text.Json.Serialization;

namespace EulerLink.Models;

public class SensorConfig
{
    [JsonPropertyName("distance_mode")]
    public int DistanceMode { get; set; }

    [JsonPropertyName("timing_budget_ms")]
    public int TimingBudgetMs { get; set; }

    [JsonPropertyName("inter_measurement_ms")]
    public int InterMeasurementMs { get; set; }

    [JsonPropertyName("roi_x_size")]
    public int RoiXSize { get; set; }

    [JsonPropertyName("roi_y_size")]
    public int RoiYSize { get; set; }

    [JsonPropertyName("roi_center_spad")]
    public int RoiCenterSpad { get; set; }

    [JsonPropertyName("offset_mm")]
    public int OffsetMm { get; set; }

    [JsonPropertyName("xtalk_cps")]
    public int XtalkCps { get; set; }

    [JsonPropertyName("signal_threshold_kcps")]
    public int SignalThresholdKcps { get; set; }

    [JsonPropertyName("sigma_threshold_mm")]
    public int SigmaThresholdMm { get; set; }

    [JsonPropertyName("threshold_low_mm")]
    public int ThresholdLowMm { get; set; }

    [JsonPropertyName("threshold_high_mm")]
    public int ThresholdHighMm { get; set; }

    [JsonPropertyName("threshold_window")]
    public int ThresholdWindow { get; set; }

    [JsonPropertyName("interrupt_polarity")]
    public int InterruptPolarity { get; set; }

    [JsonPropertyName("i2c_address")]
    public int I2cAddress { get; set; }
}

