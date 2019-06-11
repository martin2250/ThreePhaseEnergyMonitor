#ifndef FRAM_H
#define FRAM_H

#define FRAM_ADDRESS 0x50

void initFRAM();

void readFram(uint8_t *data, uint16_t address, uint8_t length);
void writeFram(uint8_t *data, uint16_t address, uint8_t length);

#define FRAM_TOTAL 0x00		// length: 4x int64

#endif
