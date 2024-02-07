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
	free(cart->persistentRAM);
}

int loadROMFromFile(Cartridge *cart, const char *path, bool printDetails) {
	if (printDetails) printf("Cartridge details:\n");

	FILE *input = fopen(path, "rb");
	if (input == NULL) {
		if (printDetails) printf("\tError: couldn't open file.\n");
		return -0x01;
	}

	uint8_t flags[16];
	if (fread(flags, sizeof(uint8_t), 16, input) != 16) {
		if (printDetails) printf("\tError: corrupted file does not contain flags.\n");
		fclose(input);
		return -0x02;
	}

	if (flags[0] != 'N' || flags[1] != 'E' || flags[2] != 'S' || flags[3] != 0x1A) {
		if (printDetails) printf("\tError: corrupted file does not contain NES header.\n");
		fclose(input);
		return -0x03;
	}

	cart->PRGsize = flags[4] * 0x4000;
	cart->CHRsize = flags[5] * 0x2000;
	cart->mapperID = flags[6] >> 4;
	cart->mapperID |= flags[7] & 0b11110000;
	cart->persistentRAM = NULL;
	cart->CHRisRAM = false;

	if (printDetails) {
		printf("\tPRG size: 0x%X\n", cart->PRGsize);
		printf("\tCHR size: 0x%X\n", cart->CHRsize);
		printf("\tMapper ID: %i\n", cart->mapperID);
	}

	// TODO select option for random- / 0- / 1- filled RAM (both PRG and CHR (below))
	if (flags[6] & HEADER6_NONVOLATILE) {
		cart->persistentRAM = malloc(0x2000 * sizeof(uint8_t));
		if (!cart->persistentRAM) {
			if (printDetails) printf("\tError: couldn't allocate memory for persistent RAM.\n");
			fclose(input);
			return -0x06;
		}

		printf("\tNOTE: Presence of non-volatile memory (defaults to 2KiB battery-backed PRG RAM)\n");
	}

	switch (cart->mapperID) {
		case MAPPER_NROM:
			cart->registerCount = 0;
			cart->registers = NULL;
			break;
		case MAPPER_MMC1:
			cart->registerCount = 6;
			CALLOC_REGS(cart);
			cart->registers[MMC1_REG_SHIFT] = MMC1_REG_SHIFT_DEFAULTVALUE;
			cart->registers[MMC1_REG_CTRL] = MMC1_REG_CTRL_DEFAULTVALUE;
			break;
		default:
			if (printDetails) printf("\tError: Mapper not supported.\n");
			fclose(input);
			return -0x05;
	}

	cart->mirroringType = (flags[6] & HEADER6_MIRRORING ? MIRROR_VERTICAL : MIRROR_HORIZONTAL);
	if (flags[6] & HEADER6_4SCREEN)
		cart->mirroringType = MIRROR_4SCREEN;

	if (printDetails) {
		printf("\tMirroring type: ");
		switch (cart->mirroringType) {
			case MIRROR_VERTICAL: printf("vertical.\n"); break;
			case MIRROR_HORIZONTAL: printf("horizontal.\n"); break;
			case MIRROR_4SCREEN: printf("4 screen.\n"); break;
		}
	}

	// TODO nametable mirroring

	if (flags[6] & HEADER6_TRAINER) {
		uint8_t trainer[512];
		if (fread(flags, sizeof(uint8_t), 512, input) != 512) {
			if (printDetails) printf("\tError: Corrupted file does not contain 512B trainer when indicated in header.\n");
			freeCartridge(cart);
			fclose(input);
			return -0x02;
		}
		if (printDetails) printf("\tNOTE: Presence of 512B trainer (currently unsupported).\n");
	}

	// TODO error handling is the absolute worst
	cart->PRG = malloc(cart->PRGsize);
	if (cart->CHRsize == 0) {
		cart->CHRisRAM = true;
		cart->CHRsize = 0x2000;
		printf("\tNOTE: CHR (of size 0B) replaced with writeable CHR RAM of size 2KiB.\n");
	}

	cart->CHR = malloc(cart->CHRsize);
	if (!cart->PRG || !cart->CHR) {
		if (printDetails) printf("\tError: couldn't allocate memory for PRG or CHR.\n");
		freeCartridge(cart);
		fclose(input);
		return -0x04;
	}

	uint8_t status = (fread(cart->PRG, sizeof(uint8_t), cart->PRGsize, input) != cart->PRGsize);
	if (!cart->CHRisRAM) {
		status |= (fread(cart->CHR, sizeof(uint8_t), cart->CHRsize, input) != cart->CHRsize);
	}

	fclose(input);

	if (status) {
		if (printDetails) printf("\tError: corrupted file does not contain the valid amount of PRG or CHR.\n");
		freeCartridge(cart);
		return -0x02;
	}

	return 0x00;
}