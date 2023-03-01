#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"

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
		result = cartridgeReadPRG(bus, address);
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
		cartridgeWritePRG(bus, address, data);
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
	return cartridgeReadCHR(bus, address);
}

void ppuWrite(Bus *bus, uint16_t address, uint8_t data) {
	if (address >= 0x3F00) {
		if ((address & 0b11) == 0) {
			bus->ppu->palettes[address & 0x0F] = data;
		}
		bus->ppu->palettes[address & 0x1F] = data;
	} else {
		// Else, mapped to cartridge space
		cartridgeWriteCHR(bus, address, data);
	}
}