#include "Arduino.h"

const char* ssid_ap = "3Ph-E-Mon-SHFD";
const char* password_ap = "RT73HD23";

unsigned long uptime_seconds = 0;
double loop_duration = 0;
double loop_duration_max = 0;

// must be zero terminated
bool parse_int64(int64_t &output, const char *input)
{
	output = 0;
	bool invert = false;

	// set invert flag
	if(*input == '-')
		invert = true;

	// skip first char if + or -
	if((*input == '-') || (*input == '+'))
		input++;

	// test if first char of number is a digit (prevent empty string from returning true)
	if((*input < '0') || (*input > '9'))
		return false;

	// loop over digits
	while(*input)
	{
		if((*input < '0') || (*input > '9'))
			return false;

		output *= 10;
		output += *(input++) - '0';
	}

	if(invert)
		output = -output;

	return true;
}

String int64_to_string(int64_t input)
{
	char buffer_array[22];
	char *buffer = buffer_array;

	if(input < 0)
	{
		(*buffer++) = '-';
		input = -input;
	}

	if(input == 0)
	{
		return String("0");
	}

	// keep a pointer to the first digit for later reversing
	char *pointer_start = buffer;

	// after this loop, buffer will point to the char after the last digit
	while(input)
	{
		*(buffer++) = '0' + (input % 10);
		input /= 10;
	}

	// set the char after the last digit to null (termination)
	// and move the buffer pointer back to the last digit
	*(buffer--) = 0;

	// reverse all digits
	while((pointer_start < buffer))
	{
		char temp = *pointer_start;
		*(pointer_start++) = *buffer;
		*(buffer--) = temp;
	}

	return String(buffer_array);
}
