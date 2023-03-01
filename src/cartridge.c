#include "cartridge.h"
#include "bus.h"
#include "cpu.h"

#define MIRROR_HORZ_ADDR(address) ((address & 0x3FF) | ((address >> 1) & 0x400))
#define MIRROR_VERT_ADDR(address) (address & 0x7FF)
#define MIRROR_1SCA_ADDR(address) (address & 0x3FF)
#define MIRROR_1SCB_ADDR(address) ((address & 0x3FF) | 0x400)

uint8_t cartridgeReadPRG(Bus *bus, uint16_t address) {
	switch (bus->cartridge->mapperID) {
		case MAPPER_NROM:
			// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL (cartridge space starts at 0x6000)
			if (address < 0x8000) {
				// TODO I have no idea how to deal with this
				return 0x00;
			} else {
				return bus->cartridge->PRG[(address - 0x8000) & (bus->cartridge->PRGsize - 1)];
			}

			break;
		
		case MAPPER_MMC1:
			if (address < 0x8000) {
				// TODO the same as NROM.
				if (address >= 0x6000 && bus->cartridge->persistentRAM) {
					return bus->cartridge->persistentRAM[address - 0x6000];
				}
				return 0x00;
			} else if (!(bus->cartridge->registers[MMC1_REG_CTRL] & 0b01000)) {
				// 32K mode
				return bus->cartridge->PRG[((address - 0x8000) | ((bus->cartridge->registers[MMC1_REG_PRG] & 0b1110) << 14)) & (bus->cartridge->PRGsize - 1)];
			} else if (!(bus->cartridge->registers[MMC1_REG_CTRL] & 0b00100)) {
				// First 16K is fixed, second is switchable
				if (address < 0xC000) {
					return bus->cartridge->PRG[(address & 0x7FFF)];
				} else {
					return bus->cartridge->PRG[((address & 0x7FFF) | (bus->cartridge->registers[MMC1_REG_PRG] << 14)) & (bus->cartridge->PRGsize - 1)];
				}
			} else {
				// First 16K is switchable, second is fixed
				if (address < 0xC000) {
					return bus->cartridge->PRG[((address & 0x7FFF) | (bus->cartridge->registers[MMC1_REG_PRG] << 14)) & (bus->cartridge->PRGsize - 1)];
				} else {
					return bus->cartridge->PRG[((address & 0x7FFF) | (0xFF << 14)) & (bus->cartridge->PRGsize - 1)];
				}
			}


			break;
		
		default:
			return 0x00;
			break;
	}

	// Something went wrong.
	return 0x00;
}

void cartridgeWritePRG(Bus *bus, uint16_t address, uint8_t data) {
	switch (bus->cartridge->mapperID) {
		case MAPPER_NROM:
			// PRG ROM; no writes
			break;

		case MAPPER_MMC1:
			if (address >= 0x6000 && address < 0x8000 && bus->cartridge->persistentRAM) {
				bus->cartridge->persistentRAM[address - 0x6000] = data;
			} else {
				// Write to cartridge register

				// Consecutives writes are ignored
				if (bus->cpu->cycleCount <= bus->cartridge->registers[MMC1_REG_TIMESTAMP] + 1) {
					break;
				}

				bus->cartridge->registers[MMC1_REG_TIMESTAMP] = bus->cpu->cycleCount;

				if (data & 0b10000000) {
					// We set the register to 1 so we can detect when there has been 5 shifts (5 writes) to dump the shift register's data into one of the other 4 registers
					bus->cartridge->registers[MMC1_REG_SHIFT] = 0b100000;
					bus->cartridge->registers[MMC1_REG_CTRL] |= 0b01100;
				} else {
					bus->cartridge->registers[MMC1_REG_SHIFT] >>= 1;
					bus->cartridge->registers[MMC1_REG_SHIFT] |= (data & 0b1) << 5;

					if (bus->cartridge->registers[MMC1_REG_SHIFT] & 0b1) {
						// Writing sequence completed
						bus->cartridge->registers[(address >> 13) & 0b11] = bus->cartridge->registers[MMC1_REG_SHIFT] >> 1;
						// TODO delete these printfs
						//printf("wrote %02X to %01X.\n", bus->cartridge->registers[MMC1_REG_SHIFT] >> 1, (address >> 13) & 0b11);
						bus->cartridge->registers[MMC1_REG_SHIFT] = 0b100000;

						if (((address >> 13) & 0b11) == MMC1_REG_CTRL) {
							switch (bus->cartridge->registers[MMC1_REG_CTRL] & 0b11) {
								case 0b00: bus->cartridge->mirroringType = MIRROR_1SCREENA; break;
								case 0b01: bus->cartridge->mirroringType = MIRROR_1SCREENB; break;
								case 0b10: bus->cartridge->mirroringType = MIRROR_VERTICAL; break;
								case 0b11: bus->cartridge->mirroringType = MIRROR_HORIZONTAL; break;
							}
						}
					}
				}
			}

			break;
		
		default:
			break;
	}
}

uint8_t cartridgeReadCHR(Bus *bus, uint16_t address) {
	switch (bus->cartridge->mapperID) {
		case MAPPER_NROM:
			// TODO NROM-128 vs NROM-256
			if (address >= 0x2000) {
				if (bus->cartridge->mirroringType == MIRROR_HORIZONTAL) {
					return bus->cartridge->internalVRAM[MIRROR_HORZ_ADDR(address)];
				} else if (bus->cartridge->mirroringType == MIRROR_VERTICAL) {
					return bus->cartridge->internalVRAM[MIRROR_VERT_ADDR(address)];
				}
			} // TODO 4 screen
			return bus->cartridge->CHR[address & 0x1FFF];

		case MAPPER_MMC1:
			if (address >= 0x2000) {
				// Nametable
				// We rely on the register and not on bus->cartridge->mirroringType because I'm the developper and I get to make the rules
				switch (bus->cartridge->registers[MMC1_REG_CTRL] & 0b00011) {
					case 0b00:
						return bus->cartridge->internalVRAM[MIRROR_1SCA_ADDR(address)];
					case 0b01:
						return bus->cartridge->internalVRAM[MIRROR_1SCB_ADDR(address)];
					case 0b10:
						return bus->cartridge->internalVRAM[MIRROR_VERT_ADDR(address)];
					case 0b11:
						return bus->cartridge->internalVRAM[MIRROR_HORZ_ADDR(address)];
				}
			} else {
				// Pattern table
				if (bus->cartridge->registers[MMC1_REG_CTRL] & 0b10000) {
					return bus->cartridge->CHR[(address & 0x0FFF) | (bus->cartridge->registers[address < 0x1000 ? MMC1_REG_CHR1 : MMC1_REG_CHR2] << 12)];
				}
				
				return bus->cartridge->CHR[(address & 0x1FFF) | ((bus->cartridge->registers[MMC1_REG_CHR1] & 0b11110) << 12)];
			}
			break;
	}

	// Something went wrong
	return 0x00;
}

void cartridgeWriteCHR(Bus *bus, uint16_t address, uint8_t data) {
	switch (bus->cartridge->mapperID) {
		case MAPPER_NROM:
			if (address >= 0x2000) {
				if (bus->cartridge->mirroringType == MIRROR_HORIZONTAL) {
					bus->cartridge->internalVRAM[MIRROR_HORZ_ADDR(address)] = data;
				} else if (bus->cartridge->mirroringType == MIRROR_VERTICAL) {
					bus->cartridge->internalVRAM[MIRROR_VERT_ADDR(address)] = data;
				} // TODO 4 screen
			} //else bus->cartridge->CHR[address & 0x1FFF] = data;
			// TODO why above line exists/is commented?
			break;
		
		case MAPPER_MMC1:
			if (address >= 0x2000) {
				switch (bus->cartridge->registers[MMC1_REG_CTRL] & 0b00011) {
					case 0b00:
						bus->cartridge->internalVRAM[MIRROR_1SCA_ADDR(address)] = data;
						break;
					case 0b01:
						bus->cartridge->internalVRAM[MIRROR_1SCB_ADDR(address)] = data;
						break;
					case 0b10:
						bus->cartridge->internalVRAM[MIRROR_VERT_ADDR(address)] = data;
						break;
					case 0b11:
						bus->cartridge->internalVRAM[MIRROR_HORZ_ADDR(address)] = data;
						break;
				}
			}

			break;
	}
}