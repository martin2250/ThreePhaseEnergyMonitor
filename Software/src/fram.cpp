#include "Arduino.h"
#include <Wire.h>
#include "fram.h"

void initFRAM()
{
	Wire.begin(5, 4);
}

void readFram(uint8_t *data, uint16_t address, uint8_t length)
{
	// all values are stored in 8 byte blocks
	address *= sizeof(int64_t);

	// deal with maximum read size of wire library, BUFFER_LENGTH is defined in Wire.h
	while(length)
	{
		uint8_t sublength = length;
		if(sublength > BUFFER_LENGTH)
			sublength = BUFFER_LENGTH;

		Wire.beginTransmission(FRAM_ADDRESS);

		Wire.write((uint8_t)(address >> 8));
		Wire.write((uint8_t)(address & 0xFF));

		Wire.endTransmission(false);
		Wire.requestFrom((uint8_t)FRAM_ADDRESS, sublength);

		while(Wire.available())
			*(data++) = Wire.read();

		address += sublength;
		length -= sublength;
	}
}

void writeFram(uint8_t *data, uint16_t address, uint8_t length)
{
	address *= sizeof(int64_t);

	Wire.beginTransmission(FRAM_ADDRESS);

	Wire.write((uint8_t)(address >> 8));
	Wire.write((uint8_t)(address & 0xFF));

	Wire.write(data, length);

	Wire.endTransmission();
}
