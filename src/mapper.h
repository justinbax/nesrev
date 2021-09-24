#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	uint8_t *CHR;
	uint8_t *PRG;
	int CHRsize;
	int PRGsize;
	uint16_t CHRoffset;
	uint16_t PRGoffset;
	// TODO implement battery-backed volatile RAM / EEPROM for saves
} Cartridge;


void initCartridge(Cartridge *cart, int CHRsize, int PRGsize, uint8_t *CHR, uint8_t *PRG);
uint8_t cartReadPPU(Cartridge *cart, uint16_t address);
void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value);
uint8_t cartReadCPU(Cartridge *cart, uint16_t address);
void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value);


void initCartridge(Cartridge *cart, int CHRsize, int PRGsize, uint8_t *CHR, uint8_t *PRG) {
	cart->CHRsize = CHRsize;
	cart->PRGsize = PRGsize;
	cart->CHR = CHR;
	cart->PRG = PRG;
	cart->CHRoffset = cart->PRGoffset = 0;
}

uint8_t cartReadPPU(Cartridge *cart, uint16_t address) {
	// TODO this
	// I can already tell the bus conflicts and open buses will require a more elaborate solution
	return cart->CHR[address + cart->CHRoffset];
}

void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value) {
	// TODO this
	cart->CHR[address + cart->CHRoffset] = value;
}

uint8_t cartReadCPU(Cartridge *cart, uint16_t address) {
	// TODO this
	if (address < 0x4020) return 0x00;

	return cart->PRG[address - 0x4020 + cart->PRGoffset];
}

void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value) {
	if (address < 0x4020) return;
	cart->PRG[address + cart->PRGoffset] = value;
}

#endif // ifndef MAPPER_H