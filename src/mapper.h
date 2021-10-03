#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	uint16_t mapperID;

	uint8_t internalVRAM[0x07FF];

	uint8_t *PRG;
	uint8_t *CHR;
	uint8_t PRGsize;
	uint8_t CHRsize;
} Cartridge;


uint8_t cartReadPPU(Cartridge *cart, uint16_t address);
void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value);
uint8_t cartReadCPU(Cartridge *cart, uint16_t address);
void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value);

// TODO deal with ROM vs RAM

uint8_t cartReadPPU(Cartridge *cart, uint16_t address) {
	// TODO this
	// TODO assuming NROM with horizontal mirroring
	if (address >= 0x2000)
		return cart->internalVRAM[(address & 0x3FF) | (address >> 1) & 0x400];
	return cart->CHR[address];
}

void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value) {
	// TODO this
	// TODO assuming NROM with horizontal mirroring
	if (address >= 0x2000)
		cart->internalVRAM[(address & 0x3FF) | (address >> 1) & 0x400] = value;
	else cart->CHR[address] = value;
}

uint8_t cartReadCPU(Cartridge *cart, uint16_t address) {
	// TODO this
	// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL (cartridge space starts at 0x6000)
	return cart->PRG[(address - 0x6000) & (cart->PRGsize - 1)];
}

void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value) {
	// TODO this
	// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL
	cart->PRG[(address - 0x6000) & (cart->PRGsize - 1)] = value;
}

#endif // ifndef MAPPER_H