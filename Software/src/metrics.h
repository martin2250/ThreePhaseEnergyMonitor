#ifndef METRICS_h
#define METRICS_h

void handleMetrics();
void handleMetricsNew();
void handleAllMetrics();
void readMetrics();
void initMetrics();
void resetMetrics();

extern int64_t total_energy[];

#define SAMPLE_COUNT_MAX 40
#define SAMPLE_INTERVAL_MS 500
#define INFLUX_PREAMBLE String(setting_metric_name) + ",loc=" + setting_location_tag + ",name="

extern unsigned long lastMetricReadTime;

#endif
