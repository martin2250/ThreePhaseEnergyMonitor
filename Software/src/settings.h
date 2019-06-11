void initSettings();
void handleSettingsGet();
void handleSettingsPost();
void save_setting(uint8_t index_setting);

extern int64_t setting_energy_total[4];

extern int64_t setting_sample_count;

extern int64_t setting_voltage_gain[3];
extern int64_t setting_current_gain[3];

extern char setting_metric_name[];
extern char setting_location_tag[];
extern char setting_wifi_ssid[];
extern char setting_wifi_psk[];
extern char setting_wifi_hostname[];

extern char setting_wifi_ip_fixed[];
extern char setting_wifi_ip_gateway[];
extern char setting_wifi_ip_netmask[];
