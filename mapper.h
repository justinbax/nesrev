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

extern inline void cartWritePPU(Cartridge *cart, uint16_t address, uint16_t value) {
	
}

#endif // ifndef MAPPER_H