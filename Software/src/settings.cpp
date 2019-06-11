#include <climits>
#include <cstdio>

#include "web.h"
#include "fram.h"
#include "settings.h"
#include "metrics.h"
#include "globals.h"
#include "ATM90E36.h"

enum SettingsType
{
	INTEGER = 0,
	STRING = 1
};

struct Setting
{
	// setting address in eeprom (multiplied by four to obtain hardware address)
	uint16_t address;
	// id string
	const char *abbrev;
	// display name of the setting
	const char *name;
	// type
	enum SettingsType type;
	// min / max value (or string length)
	int64_t max;
	int64_t min;
	// default values for integers and strings
	union{
		int64_t as_int;
		char *as_str;
	}value_default;
	// pointer to value
	void *value;
};

// max string length I2C buffer length - 2
// also subtract one for terminating 0x00
#define MAX_STRING_LENGTH 30

int64_t setting_energy_total[4];

int64_t setting_sample_count;

int64_t setting_voltage_gain[3];
int64_t setting_current_gain[3];

char setting_metric_name[MAX_STRING_LENGTH];
char setting_location_tag[MAX_STRING_LENGTH];
char setting_wifi_ssid[MAX_STRING_LENGTH];
char setting_wifi_psk[MAX_STRING_LENGTH];
char setting_wifi_hostname[MAX_STRING_LENGTH];

char setting_wifi_ip_fixed[MAX_STRING_LENGTH];
char setting_wifi_ip_gateway[MAX_STRING_LENGTH];
char setting_wifi_ip_netmask[MAX_STRING_LENGTH];

char setting_metric_name_default[MAX_STRING_LENGTH] = "threephase";
char setting_location_tag_default[MAX_STRING_LENGTH] = "main";
char setting_wifi_ssid_default[MAX_STRING_LENGTH] = "";
char setting_wifi_psk_default[MAX_STRING_LENGTH] = "";
char setting_wifi_hostname_default[MAX_STRING_LENGTH] = "threephase";

char setting_wifi_ip_fixed_default[MAX_STRING_LENGTH] = "";
char setting_wifi_ip_gateway_default[MAX_STRING_LENGTH] = "";
char setting_wifi_ip_netmask_default[MAX_STRING_LENGTH] = "";

struct Setting settings[] = {
	{0x00, "totT", "total energy all phases", INTEGER, LLONG_MAX, LLONG_MIN + 1,        {0},    setting_energy_total},
	{0x01, "totA", "total energy phase A",    INTEGER, LLONG_MAX, LLONG_MIN + 1,        {0},    setting_energy_total + 1},
	{0x02, "totB", "total energy phase B",    INTEGER, LLONG_MAX, LLONG_MIN + 1,        {0},    setting_energy_total + 2},
	{0x03, "totC", "total energy phase C",    INTEGER, LLONG_MAX, LLONG_MIN + 1,        {0},    setting_energy_total + 3},

	{0x10, "buff",  "sample buffer size (0.5s interval)",  INTEGER, SAMPLE_COUNT_MAX, 1, {1}, &setting_sample_count},

	{0x21, "ugnA", "voltage gain phase A", INTEGER, ((2<<16)-1), 0,           {13285},    setting_voltage_gain},
	{0x22, "ugnB", "voltage gain phase B", INTEGER, ((2<<16)-1), 0,           {13251},    setting_voltage_gain + 1},
	{0x23, "ugnC", "voltage gain phase C", INTEGER, ((2<<16)-1), 0,           {13250},    setting_voltage_gain + 2},

	{0x28, "ignA", "current gain phase A", INTEGER, ((2<<16)-1), 0,           {20132},    setting_current_gain},
	{0x29, "ignB", "current gain phase B", INTEGER, ((2<<16)-1), 0,           {20328},    setting_current_gain + 1},
	{0x2A, "ignC", "current gain phase C", INTEGER, ((2<<16)-1), 0,           {20333},    setting_current_gain + 2},

	{0xA0, "meas", "metric name",           STRING, MAX_STRING_LENGTH - 1, 2, {.as_str = setting_metric_name_default},   setting_metric_name},
	{0xA8, "loc",  "location tag",          STRING, MAX_STRING_LENGTH - 1, 2, {.as_str = setting_location_tag_default},  setting_location_tag},
	{0xB0, "ssid", "WIFI SSID",             STRING, MAX_STRING_LENGTH - 1, 0, {.as_str = setting_wifi_ssid_default},     setting_wifi_ssid},
	{0xB8, "psk",  "WIFI PSK (hidden)",     STRING, MAX_STRING_LENGTH - 1, 0, {.as_str = setting_wifi_psk_default},      setting_wifi_psk},
	{0xC0, "host", "hostname",              STRING, MAX_STRING_LENGTH - 1, 2, {.as_str = setting_wifi_hostname_default}, setting_wifi_hostname},

	{0xC8, "ipf",  "fixed IP address (blank = DHCP)", STRING, MAX_STRING_LENGTH - 1, 2, {.as_str = setting_wifi_ip_fixed_default},   setting_wifi_ip_fixed},
	{0xD0, "ipg",  "gateway address",                 STRING, MAX_STRING_LENGTH - 1, 2, {.as_str = setting_wifi_ip_gateway_default}, setting_wifi_ip_gateway},
	{0xD8, "netm", "netmask",                         STRING, MAX_STRING_LENGTH - 1, 2, {.as_str = setting_wifi_ip_netmask_default}, setting_wifi_ip_netmask},
};
#define SETTINGS_COUNT ((int32_t)(sizeof(settings)/sizeof(settings[0])))

void initSettings()
{
	for(uint8_t index_setting = 0; index_setting < SETTINGS_COUNT; index_setting++)
	{
		if(settings[index_setting].type == INTEGER)
		{
			int64_t value_eeprom;
			readFram((uint8_t*)(&value_eeprom), settings[index_setting].address, sizeof(value_eeprom));

			if((value_eeprom >= settings[index_setting].min) && (value_eeprom <= settings[index_setting].max))
				*((int64_t*)settings[index_setting].value) = value_eeprom;
			else
				*((int64_t*)settings[index_setting].value) = settings[index_setting].value_default.as_int;
		}
		else if(settings[index_setting].type == STRING)
		{
			readFram((uint8_t*)settings[index_setting].value, settings[index_setting].address, MAX_STRING_LENGTH);

			int length = strlen((char*)settings[index_setting].value);

			if ((length < settings[index_setting].min) || (length > settings[index_setting].max))
				strcpy((char*)settings[index_setting].value, settings[index_setting].value_default.as_str);
		}
	}
}

void save_setting(uint8_t index_setting)
{
	if(settings[index_setting].type == INTEGER)
	{
		writeFram((uint8_t*)settings[index_setting].value, settings[index_setting].address, sizeof(int64_t));
	}
	else if(settings[index_setting].type == STRING)
	{
		writeFram((uint8_t*)settings[index_setting].value, settings[index_setting].address, MAX_STRING_LENGTH);
	}
}

void handleSettingsGet()
{
	message_buffer.remove(0);

	message_buffer +=
	"<html>"
		"<body>"
			"<h1>Settings</h1>"
			"<table>"
				"<tr>"
					"<th>Name</th>"
					"<th>Value</th>"
					"<th>Default</th>"
					"<th>Min</th>"
					"<th>Max</th>"
				"</tr>";

	for(uint8_t index_setting = 0; index_setting < SETTINGS_COUNT; index_setting++)
	{
		message_buffer +=
		String("<tr>") +
			"<td>" + settings[index_setting].name + "</td>"
			"<td>";

		if(settings[index_setting].type == INTEGER)
		{
			int64_t value = *((int64_t*)settings[index_setting].value);

			message_buffer += int64_to_string(value);
		}
		else if (settings[index_setting].type == STRING)
		{
			if (settings[index_setting].value == setting_wifi_psk)	// hide wifi psk
			{
				uint8_t counter = strlen((char*)settings[index_setting].value);
				while(counter--)
					message_buffer += "*";
			}
			else
			{
				message_buffer += String((char*)settings[index_setting].value);
			}
		}

		message_buffer +=
			"</td>"
			"<td>";

		if(settings[index_setting].type == INTEGER)
		{
			message_buffer += int64_to_string(settings[index_setting].value_default.as_int);
		}
		else if (settings[index_setting].type == STRING)
		{
			message_buffer += String(settings[index_setting].value_default.as_str);
		}
		message_buffer +=
			"</td>"
			"<td>" + int64_to_string(settings[index_setting].min) + "</td>"
			"<td>" + int64_to_string(settings[index_setting].max) + "</td>"
		"</tr>";
	}

	message_buffer +=
			"</table>"
			"<form method=\"post\">"
				"<select name=\"id\">"
					"<option value=\"--\">---</option>";

	for(uint8_t index_setting = 0; index_setting < SETTINGS_COUNT; index_setting++)
	{
		message_buffer += "<option value=\"";
		message_buffer += String(settings[index_setting].abbrev);
		message_buffer += "\">";
		message_buffer += settings[index_setting].name;
		message_buffer += "</option>";
	}

	message_buffer +=
				"</select>"
				"<input name=\"value\">"
				"<input type=\"submit\" value=\"Save\">"
				"<input type=\"hidden\" name=\"backurl\" id=\"backurl_element\"/>"
			"</form>"
			"<p>wifi settings are only applied after restarting the meter</p>"
		"</body>"
		SCRIPT_SET_BACKURL
	"</html>";

	httpServer.send(200, "text/html", message_buffer);
}

void handleSettingsPost()
{
	message_buffer.remove(0);

	if((!httpServer.hasArg("id")) || (!httpServer.hasArg("value")))
	{
		message_buffer += "bad request (id and value args are missing)";
		httpServer.send(400, "text/plain", message_buffer);
	}

	uint8_t index_setting = 0xFF;
	String id = httpServer.arg("id");

	for(uint8_t index_setting_test = 0; index_setting_test < SETTINGS_COUNT; index_setting_test++)
	{
		if(id.equals(settings[index_setting_test].abbrev))
		{
			index_setting = index_setting_test;
			break;
		}
	}

	if(index_setting == 0xFF)
	{
		message_buffer += "unknown id " + id;
		httpServer.send(400, "text/plain", message_buffer);
		return;
	}

	String value = httpServer.arg("value");

	if(settings[index_setting].type == INTEGER)
	{
		int64_t value_int;

		if(!parse_int64(value_int, value.c_str()))
		{
			message_buffer += "could not parse integer value";
			httpServer.send(400, "text/plain", message_buffer);
			return;
		}

		if((value_int < settings[index_setting].min) || (value_int > settings[index_setting].max))
		{
			message_buffer += "value is outside of allowed range";
			httpServer.send(400, "text/plain", message_buffer);
			return;
		}

		*((int64_t*)settings[index_setting].value) = value_int;
	}
	else if (settings[index_setting].type == STRING)
	{
		int length = value.length();

		if ((length < settings[index_setting].min) || (length > settings[index_setting].max))
		{
			message_buffer += "value length is outside of allowed range";
			httpServer.send(400, "text/plain", message_buffer);
			return;
		}

		value.getBytes((uint8_t*)settings[index_setting].value, MAX_STRING_LENGTH, 0);
	}

	save_setting(index_setting);

	// send user back to settings page, 303 is important so the browser uses the Location header and switches back to a GET request
	message_buffer += "ok";
	httpServer.sendHeader("Location", httpServer.arg("backurl"));
	httpServer.send(303, "text/plain", message_buffer);

	initATM90E36();
	resetMetrics();
}
