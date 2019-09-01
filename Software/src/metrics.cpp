#include "Arduino.h"
#include "ATM90E36.h"
#include "metrics.h"
#include "fram.h"
#include "settings.h"
#include "web.h"
#include "globals.h"

enum ValueType {LSB_UNSIGNED = 1, LSB_COMPLEMENT = 2, NOLSB_UNSIGNED = 3, NOLSB_SIGNED = 4};

struct Metric
{
	// content of the name tag
	const char *name;
	// content for the phase tag, every char makes a new value.
	// ABC=individual phases, N=neutral (for calculated current and metrics that don't belong to a particular phase),
	// T=total
	const char *phases;
	// address of SPI register
	unsigned short address;
	// factor to convert the raw integer value to the proper floating point value
	// when LSB is used, a factor of 1/256 is added automatically
	double factor;
	// wether to use the additional LSB register
	enum ValueType type;
	// number of decimal places to show
	uint8_t decimals;
	// showInMain = false -> the metric is only shown on /allmetrics
	bool showInMain;
	int32_t **values;
};

struct Metric metrics[] = {
	{"voltage", "ABC", UrmsA, 1./100, LSB_UNSIGNED, 2, true},

	{"current", "T", IrmsN0, 1./1000, NOLSB_UNSIGNED, 3, true},
	{"current", "ABC", IrmsA, 1./1000, LSB_UNSIGNED, 5, true},

	{"power", "T", PmeanT, 4., LSB_COMPLEMENT, 2, true},
	{"power", "ABC", PmeanA, 1., LSB_COMPLEMENT, 2, true},

	{"power_reactive", "T", QmeanT, 4./1000, LSB_COMPLEMENT, 2, false},
	{"power_reactive", "ABC", QmeanA, 1./1000, LSB_COMPLEMENT, 2, false},

	{"power_apparent", "T", SmeanT, 4./1000, LSB_COMPLEMENT, 2, false},
	{"power_apparent", "ABC", SmeanA, 1./1000, LSB_COMPLEMENT, 2, false},

	{"power_factor", "TABC", PFmeanT, 1./1000, NOLSB_SIGNED, 3, false},

	{"phase_angle_voltage", "ABC", UangleA, 1./10, NOLSB_SIGNED, 2, false},
	{"phase_angle_current", "ABC", PAngleA, 1./10, NOLSB_SIGNED, 2, false},

	{"thdn_voltage", "ABC", THDNUA, 1./100, NOLSB_UNSIGNED, 1, false},
	{"thdn_current", "ABC", THDNIA, 1./100, NOLSB_UNSIGNED, 1, false},

	{"frequency", "T", Freq, 1./100, NOLSB_UNSIGNED, 3, true},
	{"temperature", "T", Temp, 1., NOLSB_SIGNED, 0, false}
};
#define METRIC_COUNT (sizeof(metrics)/sizeof(metrics[0]))

void startMetricSocket()
{
	pushClient.println("SERIES name:power loc:main");
	pushClient.print("COLUMNS ");
	
	for(uint8_t index_metric = 0; index_metric < METRIC_COUNT; index_metric++)
	{
		struct Metric metric = metrics[index_metric];

		if (!metric.showInMain)
			continue;

		uint8_t phasecount = strlen(metric.phases);

		for(uint8_t index_phase = 0; index_phase < phasecount; index_phase++)
		{
			if (index_metric != 0 || index_phase != 0)
				pushClient.print("|");
			pushClient.print("name:");
			pushClient.print(metrics[index_metric].name);
			pushClient.print(" phase:");
			pushClient.print(metrics[index_metric].phases[index_phase]);
		}
	}

	pushClient.print("|name:energy phase:T");
	pushClient.print("|name:energy phase:A");
	pushClient.print("|name:energy phase:B");
	pushClient.print("|name:energy phase:C");

	pushClient.println();
}

void sendMetricsSocket(uint8_t index)
{
	message_buffer.remove(0);
	message_buffer += "POINT";

	for(uint8_t index_metric = 0; index_metric < METRIC_COUNT; index_metric++)
	{
		struct Metric metric = metrics[index_metric];

		if (!metric.showInMain)
			continue;

		uint8_t phasecount = strlen(metric.phases);

		for(uint8_t index_phase = 0; index_phase < phasecount; index_phase++)
		{
			int32_t value_int = metric.values[index_phase][index];
			double value = value_int * metric.factor;

			if(metric.type == LSB_COMPLEMENT || metric.type == LSB_UNSIGNED)
				value /= (1 << 8);

			message_buffer += " ";
			message_buffer += String(value, metric.decimals);
		}
	}

	const char *phases = "TABC";

	for(uint8_t i = 0; i < 4; i++)
	{
		message_buffer += " ";

		if(setting_energy_total[i] < 0)
			message_buffer += "-";

		int64_t abs_total_energy = abs(setting_energy_total[i]);
		message_buffer += int64_to_string(abs_total_energy / 10000) + ".";

		String fractionalstr = String((uint16_t)(abs_total_energy % 10000));

		for(uint8_t i = fractionalstr.length(); i < 4; i++)
			message_buffer += "0";

		message_buffer += fractionalstr;
	}

	message_buffer += "\n";

	pushClient.print(message_buffer);
	pushClient.flush();
}

// last time taken to read all metrics from the ATM90E36A (in microseconds)
unsigned long lastMetricReadTime = 0;
// fill value buffers completely before serving metrics to webpage
uint8_t webpage_wait_counter = SAMPLE_COUNT_MAX;
// index of next value to be replaced
uint8_t index_nextvalue = 0;

void initMetrics()
{
	for(uint8_t index_metric = 0; index_metric < METRIC_COUNT; index_metric++)
	{
		int8_t phasecount = strlen(metrics[index_metric].phases);

		metrics[index_metric].values = (int32_t**)malloc(phasecount * sizeof(int32_t*));

		for(uint8_t index_phase = 0; index_phase < phasecount; index_phase++)
			metrics[index_metric].values[index_phase] = (int32_t*)malloc(SAMPLE_COUNT_MAX * sizeof(int32_t));
	}

	resetMetrics();
}

void resetMetrics()
{
	webpage_wait_counter = setting_sample_count + 2;
	index_nextvalue = 0;
}

void getMetricsNew(int8_t index)
{
	message_buffer.remove(0);

	char phases[] = "TABC";

	for(uint8_t index_phase = 0; index_phase < 4; index_phase++)
	{
		message_buffer += "power,loc=main,phase=";
		message_buffer += phases[index_phase];
		message_buffer += " ";

		for(uint8_t index_metric = 0; index_metric < METRIC_COUNT; index_metric++)
		{
			struct Metric metric = metrics[index_metric];

			if (!metric.showInMain)
				continue;

			char *phase_ptr = strchr(metric.phases, phases[index_phase]);

			if(!phase_ptr)
				continue;

			uint8_t offset_phase = phase_ptr - metric.phases;

			double value;

			if(index < 0)
			{
				int64_t valuesum = 0;

				int *valueptr = metric.values[offset_phase];

				for(uint8_t i = 0; i < setting_sample_count; i++)
					valuesum += *(valueptr++);

				value = valuesum * metric.factor / setting_sample_count;
			}
			else
			{
				int *valueptr = metric.values[offset_phase];

				value = valueptr[index] * metric.factor;
			}


			if(metric.type == LSB_COMPLEMENT || metric.type == LSB_UNSIGNED)
				value /= (1 << 8);

			message_buffer += String(metric.name) + "=" + String(value, metric.decimals) + ",";
		}

		message_buffer += "energy_total=";

		if(setting_energy_total[index_phase] < 0)
			message_buffer += "-";

		int64_t abs_total_energy = abs(setting_energy_total[index_phase]);
		message_buffer += int64_to_string(abs_total_energy / 10000) + ".";

		String fractionalstr = String((uint16_t)(abs_total_energy % 10000));

		for(uint8_t y = fractionalstr.length(); y < 4; y++)
			message_buffer += "0";

		message_buffer += fractionalstr + "\n";
	}
}

const uint8_t total_energy_write_interval = 50;
uint8_t total_energy_countdown = total_energy_write_interval;

void readMetrics()
{
	unsigned long starttime = micros();

	if(webpage_wait_counter)
		webpage_wait_counter--;

	for(uint8_t i = 0; i < 4; i++)
		setting_energy_total[i] += readATM90E36(APenergyT + i);
	for(uint8_t i = 0; i < 4; i++)
		setting_energy_total[i] -= readATM90E36(ANenergyT + i);

	if(total_energy_countdown)
		total_energy_countdown--;
	else
	{
		total_energy_countdown = total_energy_write_interval;
		for(uint8_t i = 0; i < 4; i++)
			save_setting(i);
	}

	for(uint8_t index_metric = 0; index_metric < METRIC_COUNT; index_metric++)
	{
		struct Metric metric = metrics[index_metric];

		uint8_t phasecount = strlen(metric.phases);

		for(uint8_t index_phase = 0; index_phase < phasecount; index_phase++)
		{
			int32_t value;

			if(metric.type == LSB_COMPLEMENT)
			{
				uint32_t val = readATM90E36(metric.address + index_phase);

				if(val & 0x8000)
					val |= 0xFF0000;

				uint16_t lsb = (unsigned short)readATM90E36(metric.address + index_phase + 0x10);

				val = (val << 8) + (lsb >> 8);

				value = (int32_t)val;
			}
			else if(metric.type == LSB_UNSIGNED)
			{
				value = (unsigned short)readATM90E36(metric.address + index_phase);
				uint16_t lsb = (unsigned short)readATM90E36(metric.address + index_phase + 0x10);
				value = (value << 8) + (lsb >> 8);
			}
			else if(metric.type == NOLSB_SIGNED)
			{
				value = (signed short)readATM90E36(metric.address + index_phase);
			}
			else //if (metric.type == NOLSB_UNSIGNED)
			{
				value = (unsigned short)readATM90E36(metric.address + index_phase);
			}

			metrics[index_metric].values[index_phase][index_nextvalue] = value;
		}
	}

	/* ---------------------------------------------------------------------- */

	if (pushClient.connected()) {
		sendMetricsSocket(index_nextvalue);
	}

	/* ---------------------------------------------------------------------- */

	// WiFiClient client;
	// if (client.connect(IPAddress(192, 168, 2, 91), 10001))
	// {
	// 	getMetricsNew(index_nextvalue);
	// 	client.println(message_buffer);
	// 	client.flush();
	// 	client.stop();
	// }

	if(++index_nextvalue >= setting_sample_count)
		index_nextvalue = 0;

	lastMetricReadTime = micros() - starttime;
}



void handleMetricsNew()
{
	if(webpage_wait_counter)
	{
		httpServer.send(404, "text/plain", "please wait for buffers to fill");
		return;
	}

	getMetricsNew(-1);

	httpServer.send(200, "text/plain; version=0.0.4", message_buffer);
}

void handleMetricsInternal(bool all)
{
	if(webpage_wait_counter)
	{
		httpServer.send(404, "text/plain", "please wait for buffers to fill");
		return;
	}

	message_buffer.remove(0);

	String preamble = INFLUX_PREAMBLE;

	for(uint8_t index_metric = 0; index_metric < METRIC_COUNT; index_metric++)
	{
		struct Metric metric = metrics[index_metric];

		if((!all) && (!metric.showInMain))
			continue;

		uint8_t phasecount = strlen(metric.phases);

		for(uint8_t index_phase = 0; index_phase < phasecount; index_phase++)
		{
			int64_t valuesum = 0;

			int *valueptr = metric.values[index_phase];

			for(uint8_t i = 0; i < setting_sample_count; i++)
				valuesum += *(valueptr++);

			double value = valuesum * metric.factor / setting_sample_count;

			if(metric.type == LSB_COMPLEMENT || metric.type == LSB_UNSIGNED)
				value /= (1 << 8);

			message_buffer += preamble + metric.name;
			message_buffer += ",phase=";
			message_buffer.concat(metric.phases[index_phase]);
			message_buffer += " value=" + String(value, metric.decimals) + "\n";
		}
	}

	const char *phases = "TABC";

	for(uint8_t i = 0; i < 4; i++)
	{
		message_buffer += preamble + "total_energy,phase=" + phases[i];
		message_buffer += " value=";

		if(setting_energy_total[i] < 0)
			message_buffer += "-";

		int64_t abs_total_energy = abs(setting_energy_total[i]);
		message_buffer += int64_to_string(abs_total_energy / 10000) + ".";

		String fractionalstr = String((uint16_t)(abs_total_energy % 10000));

		for(uint8_t i = fractionalstr.length(); i < 4; i++)
			message_buffer += "0";

		message_buffer += fractionalstr + "\n";
	}

	httpServer.send(200, "text/plain; version=0.0.4", message_buffer);
}

void handleMetrics()
{
	handleMetricsInternal(false);
}

void handleAllMetrics()
{
	handleMetricsInternal(true);
}
