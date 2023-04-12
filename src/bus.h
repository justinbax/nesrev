#ifndef BUS_H
#define BUS_H

#include "input.h"

// Forward declarations so we don't need includes and there is no circular dependency
typedef struct CPU CPU;
typedef struct PPU PPU;
typedef struct APU APU;
typedef struct Cartridge Cartridge;

typedef struct Bus {
	CPU *cpu;
	PPU *ppu;
	APU *apu;
	Port *ports;
	Cartridge *cartridge;
} Bus;

void initBus(Bus *bus, CPU *cpu, PPU *ppu, APU *apu, Port *ports, Cartridge *cartridge);
uint8_t cpuRead(Bus *bus, uint16_t address);
void cpuWrite(Bus *bus, uint16_t address, uint8_t data);
uint8_t ppuRead(Bus *bus, uint16_t address);
void ppuWrite(Bus *bus, uint16_t address, uint8_t data);

#endif // ifndef BUS_H