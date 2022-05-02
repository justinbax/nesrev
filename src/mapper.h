#ifndef MAPPER_H
#define MAPPER_H

#include <stdlib.h>
#include <stdbool.h>
#include "cartridge.h"


extern inline uint8_t cartReadPPU(Cartridge *cart, uint16_t address);
extern inline void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value);
extern inline uint8_t cartReadCPU(Cartridge *cart, uint16_t address);
extern inline void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value, uint64_t timestamp);

// TODO deal with ROM vs RAM

#define MIRROR_HORZ_ADDR(address) ((address & 0x3FF) | ((address >> 1) & 0x400))
#define MIRROR_VERT_ADDR(address) (address & 0x7FF)
#define MIRROR_1SCA_ADDR(address) (address & 0x3FF)
#define MIRROR_1SCB_ADDR(address) ((address & 0x3FF) | 0x400)

extern inline uint8_t cartReadPPU(Cartridge *cart, uint16_t address) {
	// TODO this
	
	switch (cart->mapperID) {
		case MAPPER_NROM:
			// TODO NROM-128 vs NROM-256
			if (address >= 0x2000) {
				if (cart->mirroringType == MIRROR_HORIZONTAL) {
					return cart->internalVRAM[MIRROR_HORZ_ADDR(address)];
				} else if (cart->mirroringType == MIRROR_VERTICAL) {
					return cart->internalVRAM[MIRROR_VERT_ADDR(address)];
				}
			} // TODO 4 screen
			return cart->CHR[address & 0x1FFF];

		case MAPPER_MMC1:
			if (address >= 0x2000) {
				// Nametable
				switch (cart->registers[MMC1_REG_CTRL] & 0b00011) {
					case 0b00:
						return cart->internalVRAM[MIRROR_1SCA_ADDR(address)];
					case 0b01:
						return cart->internalVRAM[MIRROR_1SCB_ADDR(address)];
					case 0b10:
						return cart->internalVRAM[MIRROR_VERT_ADDR(address)];
					case 0b11:
						return cart->internalVRAM[MIRROR_HORZ_ADDR(address)];
				}
			} else {
				// Pattern
				if (cart->registers[MMC1_REG_CTRL] & 0b10000)
					return cart->CHR[(address & 0x0FFF) | (cart->registers[address < 0x1000 ? MMC1_REG_CHR1 : MMC1_REG_CHR2] << 12)];
				return cart->CHR[(address & 0x1FFF) | ((cart->registers[MMC1_REG_CHR1] & 0b11110) << 12)];
			}
			break;
	}

	// Something went wrong
	return 0;
}

extern inline void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value) {
	// TODO this

	switch (cart->mapperID) {
		case MAPPER_NROM:
			if (address >= 0x2000) {
				if (cart->mirroringType == MIRROR_HORIZONTAL) {
					cart->internalVRAM[MIRROR_HORZ_ADDR(address)] = value;
				} else if (cart->mirroringType == MIRROR_VERTICAL) {
					cart->internalVRAM[MIRROR_VERT_ADDR(address)] = value;
				} // TODO 4 screen
			} //else cart->CHR[address & 0x1FFF] = value;
			break;
		
		case MAPPER_MMC1:
			if (address >= 0x2000) {
				switch (cart->registers[MMC1_REG_CTRL] & 0b00011) {
					case 0b00:
						cart->internalVRAM[MIRROR_1SCA_ADDR(address)] = value;
						break;
					case 0b01:
						cart->internalVRAM[MIRROR_1SCB_ADDR(address)] = value;
						break;
					case 0b10:
						cart->internalVRAM[MIRROR_VERT_ADDR(address)] = value;
						break;
					case 0b11:
						cart->internalVRAM[MIRROR_HORZ_ADDR(address)] = value;
						break;
				}
			}
			break;
	}
}

extern inline uint8_t cartReadCPU(Cartridge *cart, uint16_t address) {
	// TODO this

	switch (cart->mapperID) {
		case MAPPER_NROM:
			// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL (cartridge space starts at 0x6000)
			if (address < 0x8000)
				// I have no idea how to deal with this
				return 0x00;
			return cart->PRG[(address - 0x8000) & (cart->PRGsize - 1)];
		
		case MAPPER_MMC1:
			if (address < 0x8000)
				return 0x00;

			if (cart->registers[MMC1_REG_CTRL] & 0b01000) {
				// 32K mode
				return cart->PRG[((address - 0x8000) | ((cart->registers[MMC1_REG_PRG] & 0b1110) << 14)) & (cart->PRGsize - 1)];
			} else if (address < 0xC000) {
				// First 16K mode
				if (cart->registers[MMC1_REG_CTRL] & 0b00100) {
					// Fixed to first bank
					return cart->PRG[(address - 0x8000) & (cart->PRGsize - 1)];
				}
				return cart->PRG[((address - 0x8000) | (cart->registers[MMC1_REG_PRG] << 14)) & (cart->PRGsize - 1)];
			} else {
				// Last 16K mode
				if (cart->registers[MMC1_REG_CTRL] & 0b00100) {
					return cart->PRG[((address - 0xC000) | (cart->registers[MMC1_REG_PRG] << 14)) & (cart->PRGsize - 1)];
				}
				// Fixed to last bank
				return cart->PRG[((address - 0xC000) | (0xF << 14)) & (cart->PRGsize - 1)];
			}

			break;
	}

	// Something went wrong
	return 0x00;
}

extern inline void cartWriteCPU(Cartridge *cart, uint16_t address, uint16_t value, uint64_t timestamp) {
	// TODO this

	switch (cart->mapperID) {
		case MAPPER_NROM:
			// PRG ROM; no writes
			break;
		case MAPPER_MMC1:
			if (address < 0x8000) {
				// Write to PRG RAM, if any
				// TODO
			} else {
				// Write to cartridge register

				// Consecutives writes are ignored
				if (timestamp <= cart->registers[MMC1_REG_TIMESTAMP] + 1) {
					//printf("ignored\n");
					break;
				}

				//printf("%04X W %02X (SR before: %02X)\n", (address), value, cart->registers[MMC1_REG_SHIFT]);

				cart->registers[MMC1_REG_TIMESTAMP] = timestamp;

				if (value & 0b10000000) {
					// We set the register to 1 so we can detect when there has been 5 shifts (5 writes) to dump the shift register's data into one of the other 4 registers
					cart->registers[MMC1_REG_SHIFT] = 0b100000;
					cart->registers[MMC1_REG_CTRL] |= 0b01100;
				} else {
					cart->registers[MMC1_REG_SHIFT] >>= 1;
					cart->registers[MMC1_REG_SHIFT] |= (value & 0b1) << 5;

					if (cart->registers[MMC1_REG_SHIFT] & 0b1) {
						// Writing sequence completed
						cart->registers[(address >> 13) & 0b11] = cart->registers[MMC1_REG_SHIFT] >> 1;
						//printf("wrote %02X to %01X.\n", cart->registers[MMC1_REG_SHIFT] >> 1, (address >> 13) & 0b11);
						cart->registers[MMC1_REG_SHIFT] = 0b100000;

						if (((address >> 13) & 0b11) == MMC1_REG_CTRL) {
							if (!(cart->registers[MMC1_REG_CTRL] & 0b10))
								cart->mirroringType = MIRROR_1SCREEN;
							else if (cart->registers[MMC1_REG_CTRL] & 0b1)
								cart->mirroringType = MIRROR_HORIZONTAL;
							else
								cart->mirroringType = MIRROR_VERTICAL;
						}
					}
				}
			}

			break;
	}
}

#endif // ifndef MAPPER_H