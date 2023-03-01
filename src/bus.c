#include "bus.h"
#include "cpu.h"
#include "ppu.h"

#define MIRROR_HORZ_ADDR(address) ((address & 0x3FF) | ((address >> 1) & 0x400))
#define MIRROR_VERT_ADDR(address) (address & 0x7FF)
#define MIRROR_1SCA_ADDR(address) (address & 0x3FF)
#define MIRROR_1SCB_ADDR(address) ((address & 0x3FF) | 0x400)

// TODO deal with ROM vs RAM

// TODO seperate mapper logic

void initBus(Bus *bus, CPU *cpu, PPU *ppu, Port *ports, Cartridge *cartridge) {
	bus->cpu = cpu;
	bus->ppu = ppu;
	bus->ports = ports;
	bus->cartridge = cartridge;
}

uint8_t cpuRead(Bus *bus, uint16_t address) {
	// TODO deal with open buses
	uint8_t result = 0x00;

	if (address < 0x2000) {
		result = bus->cpu->internalRAM[address & 0x7FF];
	} else if (address < 0x4000) {
		result = readRegisterPPU(bus->ppu, address);
	} else if (address < 0x4020) {
		switch (address) {
			case OAMDMA: result = 0x00; break;
			case JOY1:
			case JOY2: result = readController(&bus->ports[address - JOY1]); break;
			default: result = 0x00; break;
		}
	} else {
		// Mapped to cartridge
		// TODO this

		switch (bus->cartridge->mapperID) {
			case MAPPER_NROM:
				// TODO the mirroring (... & (cart->PRGsize - 1)) isn't right AT ALL (cartridge space starts at 0x6000)
				if (address < 0x8000) {
					// TODO I have no idea how to deal with this
					result = 0x00;
				} else {
					result = bus->cartridge->PRG[(address - 0x8000) & (bus->cartridge->PRGsize - 1)];
				}

				break;
			
			case MAPPER_MMC1:
				if (address < 0x8000) {
					// TODO the same as NROM.
					result = 0x00;
					if (address >= 0x6000 && bus->cartridge->persistentRAM) {
						result = bus->cartridge->persistentRAM[address - 0x6000];
					}
				} else if (!(bus->cartridge->registers[MMC1_REG_CTRL] & 0b01000)) {
					// 32K mode
					result = bus->cartridge->PRG[((address - 0x8000) | ((bus->cartridge->registers[MMC1_REG_PRG] & 0b1110) << 14)) & (bus->cartridge->PRGsize - 1)];
				} else if (!(bus->cartridge->registers[MMC1_REG_CTRL] & 0b00100)) {
					// First 16K is fixed, second is switchable
					if (address < 0xC000) {
						result = bus->cartridge->PRG[(address & 0x7FFF)];
					} else {
						result = bus->cartridge->PRG[((address & 0x7FFF) | (bus->cartridge->registers[MMC1_REG_PRG] << 14)) & (bus->cartridge->PRGsize - 1)];
					}
				} else {
					// First 16K is switchable, second is fixed
					if (address < 0xC000) {
						result = bus->cartridge->PRG[((address & 0x7FFF) | (bus->cartridge->registers[MMC1_REG_PRG] << 14)) & (bus->cartridge->PRGsize - 1)];
					} else {
						result = bus->cartridge->PRG[((address & 0x7FFF) | (0xFF << 14)) & (bus->cartridge->PRGsize - 1)];
					}
				}


				break;
			
			default:
				result = 0x00;
				break;
		}
	}

	bus->cpu->rw = READ;
	bus->cpu->addressPins = address;
	bus->cpu->dataPins = result;

	return result;
}

void cpuWrite(Bus *bus, uint16_t address, uint8_t data) {
	if (address < 0x2000) {
		bus->cpu->internalRAM[address & 0x7FF] = data;
	} else if (address < 0x4000) {
		writeRegisterPPU(bus->ppu, address, data);
	} else if (address < 0x4020) {
		switch (address) {
			case OAMDMA:
				if (bus->cpu->OAMDMAstatus == DMA_WAIT && (bus->cpu->cycleCount & 0b1)) {
					bus->cpu->OAMDMAstatus = DMA_READ;
				} else {
					bus->cpu->OAMDMAstatus = DMA_WAIT;
				}
				bus->cpu->OAMDMApage = data;
				break;
			case JOY1:
			case JOY2: writeController(&bus->ports[0], data); writeController(&bus->ports[1], data); break;
			default: break;
		}
	} else {
		// Mapped to cartridge
		// TODO this

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

	bus->cpu->rw = WRITE;
	bus->cpu->addressPins = address;
	bus->cpu->dataPins = data;
}

uint8_t ppuRead(Bus *bus, uint16_t address) {
	if (address >= 0x3F00) {
		if ((address & 0b11) == 0) {
			return bus->ppu->palettes[address & 0x0F];
		}
		return bus->ppu->palettes[address & 0x1F];
	}

	// Else, mapped to cartridge space
	// TODO this
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
	// TODO maybe, like CPU (although we don't need it because of data pins, which are needed because debug output etc) have a result var and return it at the end?
	return 0;
}

void ppuWrite(Bus *bus, uint16_t address, uint8_t data) {
	if (address >= 0x3F00) {
		if ((address & 0b11) == 0) {
			bus->ppu->palettes[address & 0x0F] = data;
		}
		bus->ppu->palettes[address & 0x1F] = data;
	} else {
		
		// Else, mapped to cartridge space
		// TODO this
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
}