#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <SPI.h>

#include "metrics.h"
#include "ATM90E36.h"
#include "fram.h"
#include "web.h"
#include "settings.h"
#include "globals.h"

extern "C" {
	#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

const IPAddress apsubnet(255,255,255,0);
const IPAddress apip(192,168,4,1);
const IPAddress apgateway(192,168,4,1);

void setup(void)
{
	Serial.begin(115200);

	Serial.println("\n\nBooting Sketch...");

	SPI.begin();
	initFRAM();
	initSettings();
	initMetrics();
	initATM90E36();

	Serial.println("Initializing WiFi");

	WiFi.persistent(false);
	WiFi.mode(WIFI_OFF);
	delay(2000);
	//WiFi.mode(WIFI_AP_STA);
	WiFi.mode(WIFI_STA);
	WiFi.disconnect(true);
	delay(1000);
	if((strlen(setting_wifi_ssid) > 1) && (strlen(setting_wifi_psk) >= 8))
		WiFi.begin(setting_wifi_ssid, setting_wifi_psk);


	IPAddress ip;
	IPAddress gateway;
	IPAddress subnet;

	if(ip.fromString(setting_wifi_ip_fixed) && gateway.fromString(setting_wifi_ip_gateway) && subnet.fromString(setting_wifi_ip_netmask))
	{
		WiFi.config(ip, gateway, subnet);
	}
	else
	{
		wifi_station_dhcpc_start();
	}

	// WiFi.softAPConfig(apip, apgateway, apsubnet);
	// WiFi.softAP(ssid_ap, password_ap);

	WiFi.hostname(setting_wifi_hostname);

	initWeb();

	pushClient.setTimeout(500);

	Serial.println("setup finished");
}

void loop(void)
{
	static unsigned long last_spi_read_time = millis();
	static unsigned long last_uptime_update = millis();
	static unsigned long last_pushClient_attempt = 0;

	unsigned long loop_start = micros();

	httpServer.handleClient();

	unsigned long now = millis();

	if((now - last_spi_read_time) > SAMPLE_INTERVAL_MS)
	{
		last_spi_read_time += SAMPLE_INTERVAL_MS;

		readMetrics();
	}

	if((now - last_uptime_update) > 1000)
	{
		uptime_seconds++;
		last_uptime_update += 1000;
	}

	if ((!pushClient.connected()) && ((now - last_pushClient_attempt) > 10000)) {
		pushClient.connect(IPAddress(192, 168, 2, 186), 11111);
		last_pushClient_attempt = now;
	}

	unsigned long loop_duration_ul = micros() - loop_start;

	loop_duration = 0.99 * loop_duration + 0.01 * loop_duration_ul;
	loop_duration_max = max(loop_duration_max, (double)loop_duration_ul);
}
