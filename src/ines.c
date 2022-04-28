#include <stdio.h>
#include <stdlib.h>
#include "ines.h"

#define HEADER6_MIRRORING 0b00000001
#define HEADER6_NONVOLATILE 0b00000010
#define HEADER6_TRAINER 0b00000100
#define HEADER6_4SCREEN 0b00001000

// Useful
#define CALLOC_REGS(cart) cart->registers = calloc(1, sizeof(uint8_t) * cart->registerCount)

void freeCartridge(Cartridge *cart) {
	free(cart->PRG);
	free(cart->CHR);
	free(cart->registers);
}

int loadROMFromFile(Cartridge *cart, const char *path) {
	FILE *input = fopen(path, "rb");
	if (input == NULL)
		return -0x01;

	uint8_t flags[16];
	if (fread(flags, sizeof(uint8_t), 16, input) != 16) {
		fclose(input);
		return -0x02;
	}

	if (flags[0] != 'N' || flags[1] != 'E' || flags[2] != 'S' || flags[3] != 0x1A) {
		fclose(input);
		return -0x03;
	}

	cart->PRGsize = flags[4] * 0x4000;
	cart->CHRsize = flags[5] * 0x2000;
	cart->mapperID = flags[6] >> 4;
	cart->mapperID |= flags[7] & 0b11110000;

	switch (cart->mapperID) {
		case MAPPER_NROM:
			cart->registerCount = 0;
			cart->registers = NULL;
			break;
		case MAPPER_MMC1:
			cart->registerCount = 6;
			CALLOC_REGS(cart);
			cart->registers[MMC1_REG_SHIFT] = 0x01;
			cart->registers[MMC1_REG_CTRL] = 0b01100;
			cart->registers[MMC1_REG_TIMESTAMP] = 0;
			break;
		default:
			fclose(input);
			return -0x05;
	}

	cart->registers = calloc(1, sizeof(uint8_t) * cart->registerCount);

	cart->mirroringType = (flags[6] & HEADER6_MIRRORING ? MIRROR_VERTICAL : MIRROR_HORIZONTAL);
	if (flags[6] & HEADER6_4SCREEN)
		cart->mirroringType = MIRROR_4SCREEN;

	// TODO nametable mirroring

	if (flags[6] & HEADER6_TRAINER) {
		uint8_t trainer[512];
		if (fread(flags, sizeof(uint8_t), 512, input) != 512) {
			freeCartridge(cart);
			fclose(input);
			return -0x02;
		}
	}

	// TODO error handling is the absolute worst
	cart->PRG = malloc(cart->PRGsize);
	cart->CHR = malloc(cart->CHRsize);
	if (!cart->PRG || !cart->CHR) {
		freeCartridge(cart);
		fclose(input);
		return -0x04;
	}

	uint8_t status = (fread(cart->PRG, sizeof(uint8_t), cart->PRGsize, input) != cart->PRGsize);
	status |= (fread(cart->CHR, sizeof(uint8_t), cart->CHRsize, input) != cart->CHRsize);

	fclose(input);

	if (status) {
		freeCartridge(cart);
		return -0x02;
	}

	return 0x00;
}