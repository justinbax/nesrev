#ifndef MAPPER_H
#define MAPPER_H

#include <stdlib.h>
#include <stdbool.h>
#include "cartridge.h"


extern inline uint8_t cartReadPPU(Cartridge *cart, uint16_t address);
extern inline void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value);
extern inline uint8_t cartReadCPU(Cartridge *cart, uint16_t address);
extern inline void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value);

// TODO deal with ROM vs RAM

#define MIRROR_HORZ_ADDR(address) ((address & 0x3FF) | ((address >> 1) & 0x400))
#define MIRROR_VERT_ADDR(address) (address & 0x7FF)

extern inline uint8_t cartReadPPU(Cartridge *cart, uint16_t address) {
	// TODO this
	// TODO assuming NROM with horizontal mirroring
	if (address >= 0x2000) {
		if (cart->mirroringType == MIRROR_HORIZONTAL) {
			return cart->internalVRAM[MIRROR_HORZ_ADDR(address)];
		} else if (cart->mirroringType == MIRROR_VERTICAL) {
			return cart->internalVRAM[MIRROR_VERT_ADDR(address)];
		} // TODO 4 screen
	}
	return cart->CHR[address];
}

extern inline void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value) {
	// TODO this
	// TODO assuming NROM with horizontal mirroring
	if (address >= 0x2000) {
		if (cart->mirroringType == MIRROR_HORIZONTAL) {
			cart->internalVRAM[MIRROR_HORZ_ADDR(address)] = value;
		} else if (cart->mirroringType == MIRROR_VERTICAL) {
			cart->internalVRAM[MIRROR_VERT_ADDR(address)] = value;
		} // TODO 4 screen
	} else cart->CHR[address] = value;
}

extern inline uint8_t cartReadCPU(Cartridge *cart, uint16_t address) {
	// TODO this
	// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL (cartridge space starts at 0x6000)
	if (address < 0x8000)
		// I have no idea how to deal with this
		return 0x00;
	return cart->PRG[(address - 0x8000) & (cart->PRGsize - 1)];
}

extern inline void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value) {
	// TODO this
	// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL
	// TODO this should be ROM, not RAM
	if (address < 0x8000) return;
	cart->PRG[(address - 0x8000) & (cart->PRGsize - 1)] = value;
}

#endif // ifndef MAPPER_H