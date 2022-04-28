#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

#define MIRROR_UNKNOWN 0
#define MIRROR_HORIZONTAL 1
#define MIRROR_VERTICAL 2
#define MIRROR_4SCREEN 3
#define MIRROR_1SCREEN 4

#define MAPPER_NROM 0
#define MAPPER_MMC1 1

#define MMC1_REG_CTRL 0
#define MMC1_REG_CHR1 1
#define MMC1_REG_CHR2 2
#define MMC1_REG_PRG 3
#define MMC1_REG_SHIFT 4
#define MMC1_REG_TIMESTAMP 5

typedef struct {
	uint16_t mapperID;
	uint8_t mirroringType;

	uint8_t internalVRAM[0x0800];

	uint8_t *PRG;
	uint8_t *CHR;
	uint32_t PRGsize;
	uint32_t CHRsize;

	uint8_t *registers;
	int registerCount; // TODO think about if we need this
} Cartridge;

#endif // ifndef CARTRIDGE_H