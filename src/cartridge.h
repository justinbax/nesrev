#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

#define MIRROR_UNKNOWN 0
#define MIRROR_HORIZONTAL 1
#define MIRROR_VERTICAL 2
#define MIRROR_4SCREEN 3

typedef struct {
	uint16_t mapperID;
	uint8_t mirroringType;

	uint8_t internalVRAM[0x0800];

	uint8_t *PRG;
	uint8_t *CHR;
	uint32_t PRGsize;
	uint32_t CHRsize;
} Cartridge;

#endif // ifndef CARTRIDGE_H