#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

typedef struct {
	uint16_t mapperID;

	uint8_t internalVRAM[0x07FF];

	uint8_t *PRG;
	uint8_t *CHR;
	uint32_t PRGsize;
	uint32_t CHRsize;
} Cartridge;

#endif // ifndef CARTRIDGE_H