#include <stdio.h>
#include "ines.h"

#define HEADER6_MIRRORING 0b00000001
#define HEADER6_NONVOLATILE 0b00000010
#define HEADER6_TRAINER 0b00000100
#define HEADER6_4SCREEN 0b00001000

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

	// TODO nametable mirroring

	if (flags[6] & HEADER6_TRAINER) {
		uint8_t trainer[512];
		if (fread(flags, sizeof(uint8_t), 512, input) != 512) {
			fclose(input);
			return -0x02;
		}
	}

	// TODO error handling is the absolute worst
	cart->PRG = malloc(cart->PRGsize);
	cart->CHR = malloc(cart->CHRsize);
	if (!cart->PRG || !cart->CHR) {
		free(cart->PRG);
		free(cart->CHR);
		fclose(input);
		return -0x04;
	}

	if (fread(cart->PRG, sizeof(uint8_t), cart->PRGsize, input) != cart->PRGsize) {
		free(cart->PRG);
		free(cart->CHR);
		fclose(input);
		return -0x02;
	} else if (fread(cart->CHR, sizeof(uint8_t), cart->CHRsize, input) != cart->CHRsize) {
		free(cart->PRG);
		free(cart->CHR);
		fclose(input);
		return -0x02;
	}
}