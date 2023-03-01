#ifndef BUS_H
#define BUS_H

#include "input.h"
#include "cartridge.h"

// TODO maybe the components need an extra interface for things
// TODO at this point the entire mapper.h file is copied into bus.h and bus.c

// TODO deal with ROM vs RAM

#define MIRROR_HORZ_ADDR(address) ((address & 0x3FF) | ((address >> 1) & 0x400))
#define MIRROR_VERT_ADDR(address) (address & 0x7FF)
#define MIRROR_1SCA_ADDR(address) (address & 0x3FF)
#define MIRROR_1SCB_ADDR(address) ((address & 0x3FF) | 0x400)

// Forward declarations so we don't need includes and there is no circular dependency

typedef struct CPU CPU;
typedef struct PPU PPU;

typedef struct {
	CPU *cpu;
	PPU *ppu;
	Port *ports;
	Cartridge *cartridge;
} Bus;

void initBus(Bus *bus, CPU *cpu, PPU *ppu, Port *ports, Cartridge *cartridge);
uint8_t cpuRead(Bus *bus, uint16_t address);
void cpuWrite(Bus *bus, uint16_t address, uint8_t data);
uint8_t ppuRead(Bus *bus, uint16_t address);
void ppuWrite(Bus *bus, uint16_t address, uint8_t data);

#endif // ifndef BUS_H