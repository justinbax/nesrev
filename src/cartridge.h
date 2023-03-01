#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"

#define MIRROR_UNKNOWN 0
#define MIRROR_1SCREENA 1
#define MIRROR_1SCREENB 2
#define MIRROR_VERTICAL 3
#define MIRROR_HORIZONTAL 4
#define MIRROR_4SCREEN 5

#define MAPPER_NROM 0
#define MAPPER_MMC1 1

#define MMC1_REG_CTRL 0
#define MMC1_REG_CHR1 1
#define MMC1_REG_CHR2 2
#define MMC1_REG_PRG 3
#define MMC1_REG_SHIFT 4
#define MMC1_REG_TIMESTAMP 5
#define MMC1_CTRL_PRG16K_ENABLE 0b01000
#define MMC1_CTRL_PRG16K_SELECT 0b00100
#define MMC1_CTRL_CHR4K_ENABLE 0b10000
#define MMC1_RESET_BIT 0b10000000
#define MMC1_REG_CTRL_DEFAULTVALUE 0b01100
#define MMC1_REG_SHIFT_DEFAULTVALUE 0b100000

typedef struct Cartridge {
	uint16_t mapperID;
	uint8_t mirroringType;

	uint8_t internalVRAM[0x0800];

	uint8_t *PRG;
	uint8_t *CHR;
	uint32_t PRGsize;
	uint32_t CHRsize;

	uint8_t *persistentRAM;
	bool CHRisRAM;

	uint8_t *registers;
	int registerCount; // TODO think about if we need this
} Cartridge;

uint8_t cartridgeReadPRG(Bus *bus, uint16_t address);
void cartridgeWritePRG(Bus *bus, uint16_t address, uint8_t data);
uint8_t cartridgeReadCHR(Bus *bus, uint16_t address);
void cartridgeWriteCHR(Bus *bus, uint16_t address, uint8_t data);

#endif // ifndef CARTRIDGE_H