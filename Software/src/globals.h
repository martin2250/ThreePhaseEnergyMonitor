extern const char* ssid_ap;
extern const char* password_ap;

extern const IPAddress apip;
extern const IPAddress apgateway;
extern const IPAddress subnet;

extern unsigned long uptime_seconds;
extern double loop_duration;
extern double loop_duration_max;

#define SCRIPT_SET_BACKURL "<script>document.getElementById('backurl_element').value = window.location.href.split('?')[0];</script>"

// must be zero terminated
bool parse_int64(int64_t &output, const char *input);
String int64_to_string(int64_t input);
