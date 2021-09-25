#ifndef PPU_H
#define PPU_H

#include <stdio.h>
#include <stdint.h>
#include "mapper.h"

#define PPUCTRL 0
#define PPUMASK 1
#define PPUSTATUS 2
#define OAMADDR 3
#define OAMDATA 4
#define PPUSCROLL 5
#define PPUADDR 6
#define PPUDATA 7

typedef struct {
	// Registers and CPU / PPU interface
	uint8_t registers[8];
	uint8_t dataBusCPU;

	// Internal VRAM
	uint8_t OAM[256];
	uint8_t secondOAM[32];
	uint8_t palettes[32];

	// Color emulation
	uint8_t colors[64][3];

	// Internal sprite evaluation counters and flags
	uint8_t secondOAMptr;
	bool spriteInRange;
	uint8_t sprCount;
	uint16_t sprPatternIndex;
	bool sprZeroOnNext;
	bool sprZeroOnCurrent;

	// Internal registers for fetching and rendering
	uint16_t addressVRAM;
	uint16_t tempAddressVRAM;
	uint8_t readBufferVRAM;
	uint8_t fineX;
	bool secondWrite;

	// Internal registers / shift registers when fetching tile data
	uint16_t bgPatternData[2];
	uint8_t bgPaletteData[2];
	uint8_t sprPatternLow[8];
	uint8_t sprPatternHigh[8];
	uint8_t sprAttributes[8];
	uint8_t sprXPos[8];

	// Internal latches used to fetch tile data
	uint8_t bgNametableLatch;
	uint8_t bgPaletteLatch;
	bool bgSerialPaletteLatch[2];
	uint8_t bgPatternLatch[2];

	// Internal flags
	bool allowRegWrites;
	bool oddFrame;

	// External latch for VRAM access
	uint8_t addressBusLatch;

	uint16_t scanline;
	uint16_t pixel;

	uint8_t *framebuffer;

	Cartridge *cart;
} PPU;


// Interface functions
void initPPU(PPU *ppu, uint8_t *framebuffer, Cartridge *cart);
void terminatePPU(PPU *ppu);
extern inline void tickPPU(PPU *ppu);
extern inline uint8_t readRegisterPPU(PPU *ppu, uint8_t reg);
extern inline void writeRegisterPPU(PPU *ppu, uint8_t reg, uint8_t value);
void loadPalette(PPU *ppu, uint8_t values[192]);

// Non-interface functions (still accessible to keep inlining)
extern inline uint8_t readAddressPPU(PPU *ppu, uint16_t address);
extern inline void writeAddressPPU(PPU *ppu, uint16_t address, uint8_t value);
extern inline void shiftRegistersPPU(PPU *ppu);
extern inline void incrementX(PPU *ppu);
extern inline void incrementY(PPU *ppu);
extern inline void feedShiftRegisters(PPU *ppu);
extern inline void renderPixel(PPU *ppu);


// Undefined later
#define PUTADDRBUS(ppu, address) ppu->addressBusLatch = address
#define RENDERING(ppu) (ppu->registers[PPUMASK] & 0b00011000)
#define NAMETABLEADDR(ppu) (0x2000 | (ppu->addressVRAM & 0x0FFF))
#define ATTRIBUTEADDR(ppu) (0x23C0 | (ppu->addressVRAM & 0x0C00) | ((ppu->addressVRAM >> 2) & 0b111) | ((ppu->addressVRAM >> 4) & 0b111000))
#define BGPATTERNADDR(ppu) (((ppu->registers[PPUCTRL] & 0b10000) << 8) | (ppu->bgNametableLatch << 4) | ((ppu->addressVRAM & 0x7000) >> 12))
// Just trust me for this one
#define SPRPATTERNADDR(ppu) (((ppu->registers[PPUCTRL] & 0b00100000) ? ((ppu->sprPatternIndex & 0b10000) << 8) | (ppu->sprPatternIndex & 0b111111100000) | ((ppu->sprPatternIndex & 0b1000) << 1) : ((ppu->sprPatternIndex & 0b111111110000) | ((ppu->registers[PPUCTRL] & 0b00010000) << 8))) | (ppu->sprPatternIndex & 0b111))


// Non-interface functions
extern inline uint8_t readAddressPPU(PPU *ppu, uint16_t address) {
	// TODO this
	if (address >= 0x3F00)
		return ppu->palettes[address & 0x1F];
	return cartReadPPU(ppu->cart, address);
}

extern inline void writeAddressPPU(PPU *ppu, uint16_t address, uint8_t value) {
	// TODO this
	if (address >= 0x3F00)
		ppu->palettes[address & 0x1F] = value;
	else
		cartWritePPU(ppu->cart, address, value);
}

extern inline void shiftRegistersPPU(PPU *ppu) {
	ppu->bgPatternData[0] >>= 1;
	ppu->bgPatternData[1] >>= 1;
	ppu->bgPaletteData[0] >>= 1;
	ppu->bgPaletteData[1] >>= 1;
	ppu->bgPaletteData[0] |= (ppu->bgSerialPaletteLatch[0] << 7);
	ppu->bgPaletteData[1] |= (ppu->bgSerialPaletteLatch[1] << 7);
}

extern inline void incrementX(PPU *ppu) {
	if ((ppu->addressVRAM & 0b1111) == 0b1111) {
		ppu->addressVRAM &= 0b111111111100000;
		ppu->addressVRAM ^= 0b000010000000000;
	} else ppu->addressVRAM++;
}

extern inline void incrementY(PPU *ppu) {
	// TODO the magic binary number wizard has arrived
	if (ppu->addressVRAM >> 12 == 0b111) {
		ppu->addressVRAM &= 0b000111111111111;
		switch ((ppu->addressVRAM >> 5) & 0b11111) {
			case 0b11101: ppu->addressVRAM ^= 0b000100000000000;
			case 0b11111: ppu->addressVRAM &= 0b111110000011111; break;
			default: ppu->addressVRAM += 0b100000;
		}
	} else ppu->addressVRAM += 0b001000000000000;
}

extern inline void feedShiftRegisters(PPU *ppu) {
	ppu->bgPatternData[0] |= (ppu->bgPatternLatch[0] << 8);
	ppu->bgPatternData[1] |= (ppu->bgPatternLatch[1] << 8);
	ppu->bgSerialPaletteLatch[0] = ppu->bgPaletteLatch & 0b1;
	ppu->bgSerialPaletteLatch[1] = ppu->bgPaletteLatch & 0b10;
}

extern inline void renderPixel(PPU *ppu) {
	uint8_t bgColor = 0, sprColor = 0, attributes = 0, outputUnit = 8;

	// Updates X position of sprites and checks for the first active sprite
	for (int i = 7; i >= 0; i--) {
		ppu->sprXPos[i]--;
		if ((((ppu->sprXPos[i] - 1) & 0xFF) >= (0x100 - 8)) && (ppu->sprPatternLow[i] || ppu->sprPatternHigh[i])) {
			sprColor = (ppu->sprPatternLow[i] >> ppu->fineX) & 0b1;
			sprColor |= ((ppu->sprPatternHigh[i] >> ppu->fineX) & 0b1) << 1;
			attributes = ppu->secondOAM[(i << 2) | 0b10];
			sprColor |= (attributes & 0b11) << 2;
			outputUnit = i;
		}
	}

	bgColor = (ppu->bgPatternData[0] >> ppu->fineX) & 0b1;
	bgColor |= ((ppu->bgPatternData[1] >> ppu->fineX) & 0b1) << 1;
	bgColor |= ((ppu->bgPaletteData[0] >> ppu->fineX) & 0b1) << 2;
	bgColor |= ((ppu->bgPaletteData[1] >> ppu->fineX) & 0b1) << 3;

	// Disables rendering according to PPUMASK
	if (!(ppu->registers[PPUMASK] & 0b00010000) || (!(ppu->registers[PPUMASK] & 0b00000100) && ppu->pixel < 8)) sprColor = 0;
	if (!(ppu->registers[PPUMASK] & 0b00001000) || (!(ppu->registers[PPUMASK] & 0b00000010) && ppu->pixel < 8)) bgColor = 0;

	// Checks for sprite 0 hits
	if (ppu->sprZeroOnCurrent && outputUnit == 0 && sprColor && bgColor && ppu->pixel != 255)
		ppu->registers[PPUSTATUS] |= 0b01000000;

	// TODO change color index according to addressVRAM during vblank (bg palette hack)
	// Multiplexer : checks which pixel (background, sprite or backdrop) should be rendered on screen
	int framebufferIndex = (ppu->scanline * 256 + ppu->pixel) * 3;
	uint8_t paletteIndex = 0;
	if (bgColor && (!sprColor || attributes & 0b00100000))
		paletteIndex = bgColor;
	else if (sprColor && (!bgColor || !(attributes & 0b00100000)))
		paletteIndex = sprColor | 0b10000;

	if ((paletteIndex & 0b11) == 0)
		paletteIndex &= 11101111;

	// Render with greyscale, if specified in PPUMASK
	ppu->framebuffer[framebufferIndex] = ppu->colors[ppu->palettes[paletteIndex] & (ppu->registers[PPUMASK] & 0b1 ? 0x30 : 0x3F)][0];
	ppu->framebuffer[framebufferIndex + 1] = ppu->colors[ppu->palettes[paletteIndex] & (ppu->registers[PPUMASK] & 0b1 ? 0x30 : 0x3F)][1];
	ppu->framebuffer[framebufferIndex + 2] = ppu->colors[ppu->palettes[paletteIndex] & (ppu->registers[PPUMASK] & 0b1 ? 0x30 : 0x3F)][2];
}


// Interface functions
void initPPU(PPU *ppu, uint8_t *framebuffer, Cartridge *cart) {
	// TODO seperate powerup / reset. currently, these are for powerup except for allowRegWrites, which is for reset for some reason
	ppu->dataBusCPU = 0;
	ppu->addressBusLatch = 0;
	ppu->scanline = ppu->pixel = 0;
	// TODO set vlb to unasserted
	// TODO do we really need OAMDATA, PPUDATA, PPUADR and PPUSCROLL as registers
	// TODO load colors with default value
	// TODO deal with sprXPos etc for first frame
	ppu->secondOAMptr = ppu->sprCount = ppu->sprPatternIndex = 0;
	ppu->addressVRAM = ppu->tempAddressVRAM = ppu->readBufferVRAM = ppu->fineX = 0;
	ppu->spriteInRange = ppu->sprZeroOnNext = ppu->sprZeroOnCurrent = ppu->secondWrite = ppu->oddFrame = false;

	for (int i = 0; i < 256; i++) ppu->OAM[i] = 0x00;
	for (int i = 0; i < 32; i++) ppu->palettes[i] = ppu->secondOAM[i] = 0x00;

	ppu->bgPatternData[0] = ppu->bgPatternData[1] = 0;
	ppu->bgPaletteData[0] = ppu->bgPaletteData[1] = 0;
	ppu->bgSerialPaletteLatch[0] = ppu->bgSerialPaletteLatch[1] = false;
	ppu->bgPatternLatch[0] = ppu->bgPatternLatch[1] = 0;

	for (int i = 0; i < 8; i++) {
		ppu->registers[i] = 0;
		ppu->sprPatternLow[i] = 0;
		ppu->sprPatternHigh[i] = 0;
		ppu->sprAttributes[i] = 0;
		ppu->sprXPos[i] = 0;
	}

	ppu->registers[PPUSTATUS] = 0b10100000;
	ppu->allowRegWrites = false;
	// TODO assuming framebuffer is valid is a dangerous game. However, mallocating it ourselves would just add the need for a terminatePPU function, and would add no real benefit (the indexing would be the same because it would need to be heap-allocated anyway)
	ppu->framebuffer = framebuffer;
	ppu->cart = cart;

	// TODO remove this and check palettes initial value
	for (int i = 0; i < 32; i++) ppu->palettes[i] = i;
}

extern inline uint8_t readRegisterPPU(PPU *ppu, uint8_t reg) {
	switch (reg) {
		case PPUCTRL:
		case PPUMASK:
		case OAMADDR:
		case PPUSCROLL:
		case PPUADDR:
			break;
		case PPUSTATUS:
			ppu->dataBusCPU &= 0b00011111;
			ppu->dataBusCPU |= (ppu->registers[PPUSTATUS] & 0b11100000);
			ppu->registers[PPUSTATUS] &= 0b01111111;
			// TODO set vbl to unasserted
			ppu->secondWrite = false;
			break;
		case OAMDATA:
			// TODO make sure reading from OAMDATA during rendering exposes internal accesses
			// TODO inc oamaddr
			ppu->dataBusCPU = ppu->OAM[ppu->registers[OAMADDR]];
			break;
		case PPUDATA:
			// TODO see what happens during vblank
			// TODO VRAM accesses are 2 cycles ...
			if (ppu->registers[PPUADDR] < 0x3F00) {
				ppu->dataBusCPU = ppu->readBufferVRAM;
				// TODO readBufferVRAM is only updated "at the PPU's earliest convenience" 
				// ppu->readBufferVRAM = readAddressPPU(ppu->addressVRAM);
			} else {
				// ppu->dataBusCPU = readAddressPPU(ppu->registers[PPUADDR]);
				// ppu->readBufferVRAM = readAddressPPU(0x2000 | (ppu->registers[PPUADDR] & 0x0FFF));
			}
			// TODO inc is weird during rendering
			ppu->addressVRAM += (ppu->registers[PPUCTRL] & 0b00000100 ? 32 : 1);
			break;
	}
	return ppu->dataBusCPU;
}

extern inline void writeRegisterPPU(PPU *ppu, uint8_t reg, uint8_t value) {
	ppu->dataBusCPU = value;
	switch (reg) {
		case PPUCTRL:
			if (ppu->allowRegWrites) {
				ppu->registers[PPUCTRL] = value;
				ppu->tempAddressVRAM &= 0b111001111111111;
				ppu->tempAddressVRAM |= (value & 0b11) << 10;
				// TODO check for vbl
				// TODO apparently sometimes an open bus value is written
				// TODO replicate other bugs
			}
			break;
		case PPUMASK:
			if (ppu->allowRegWrites) {
				// TODO corruption when disabling rendering mid-frame
				ppu->registers[PPUMASK] = value;
			}
			break;
		case PPUSTATUS:
			break;
		case OAMADDR:
			// TODO corruption of OAM
			// TODO inc oamaddr
			ppu->registers[OAMADDR] = value;
			break;
		case OAMDATA:
			// TODO this
			// DO NOT WRITE TO REGISTER AT ALL if it's during rendering
			break;
		case PPUSCROLL:
			if (ppu->allowRegWrites) {
				if (!ppu->secondWrite) {
					ppu->tempAddressVRAM &= 0b111111111100000;
					ppu->tempAddressVRAM |= value >> 3;
					ppu->fineX = value & 0b111;
				} else {
					ppu->tempAddressVRAM &= 0b000110000011111;
					ppu->tempAddressVRAM |= (value & 0b11111000) << 2;
					ppu->tempAddressVRAM |= (value & 0b111) << 12;
				}
				ppu->secondWrite = !ppu->secondWrite;
			}
			break;
		case PPUADDR:
			if (ppu->allowRegWrites) {
				// TODO bus conflicts etc.
				ppu->tempAddressVRAM &= 0xFF << (ppu->secondWrite << 3);
				ppu->tempAddressVRAM |= value << (!ppu->secondWrite << 3);
				if (ppu->secondWrite) ppu->addressVRAM = ppu->tempAddressVRAM;
				else ppu->tempAddressVRAM &= 0b100000000000000;
				ppu->secondWrite = !ppu->secondWrite;
			}
			break;
		case PPUDATA:
			// TODO this
			// TODO VRAM accesses are 2 cycles ...
			// writeAddressPPU(ppu->addressVRAM, value);
			// TODO inc only if rendering is enabled
			// TODO if during rendering, increment is weird
			ppu->addressVRAM += (ppu->registers[PPUCTRL] & 0b00000100 ? 32 : 1);
			break;
	}
}

void loadPalette(PPU *ppu, uint8_t colors[192]) {
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 3; j++) {
			ppu->colors[i][j] = colors[i * 3 + j];
		}
	}
}

extern inline void tickPPU(PPU *ppu) {
	// TODO sprite 0
	// TODO don't render when ...
	// TODO color emphasis
	// TODO palette addressing / mirroring etc
	// TODO when rendering starts, if OAMADDR >= 8, OAM is corrupted slightly
	// TODO vbl
	// TODO make oam[attribute] & 0b00011100 always 0 (unused bits USED BY EMULATOR)
	// TODO words cannot describe how ugly this whole thing is
	// TODO I have no idea how any of this works, so anyone else won't either

	// Scanline-independant logic
	if (RENDERING(ppu)) {
		if (ppu->pixel == 256) incrementY(ppu);
		if ((ppu->pixel & 0b111) == 0 && (ppu->pixel <= 256 || ppu->pixel >= 328)) incrementX(ppu);
		else if (ppu->pixel == 257) {
			ppu->addressVRAM &= 0b111101111100000;
			ppu->addressVRAM |= (ppu->tempAddressVRAM & 0b000010000011111);
		}
	}

	// Visible scanlines
	// TODO sprite eval doesn't happend on scanline == 261 and disabled rendering
	// TODO the nested if/elses are quite a pain
	if (ppu->scanline < 240 || ppu->scanline == 261) {
		if (ppu->pixel == 0) {
			if (ppu->scanline == 261) ppu->oddFrame = !ppu->oddFrame;
			else renderPixel(ppu);

			if (ppu->scanline == 0 && ppu->oddFrame) ppu->bgNametableLatch = readAddressPPU(ppu, NAMETABLEADDR(ppu));
			else PUTADDRBUS(ppu, BGPATTERNADDR(ppu));

			ppu->sprZeroOnCurrent = ppu->sprZeroOnNext;
			ppu->spriteInRange = ppu->sprZeroOnNext = false;
			ppu->secondOAMptr = ppu->sprCount = 0;
		} else if (ppu->pixel <= 256) {
			// Sprite evaluation
			if (RENDERING(ppu) && ppu->scanline != 261) {
				if (ppu->pixel <= 64) {
					// Cycles 1-64 : fills the secondary OAM with 0xFF
					if (ppu->pixel & 1) {
						ppu->OAM[ppu->registers[OAMADDR]]; // Dummy read for future logging
						ppu->registers[OAMDATA] = 0xFF;
					}
					else ppu->secondOAM[ppu->pixel >> 1] = ppu->registers[OAMDATA];
				} else if ((ppu->registers[OAMADDR] == 0 && ppu->pixel > 66) || ((ppu->registers[OAMADDR] & 0b11111100) == 0 && ppu->sprCount >= 8)) {
					// There are no more sprites to be evaluated
					// The above if statement is to ensure this is reached if we reached the end of OAM without filling up secondOAM (in which case OAMADDR will always be 0) AND if we did fill it (in which case the sprite overflow bug occured, so OAMADDR will be anywhere between 0 and 3)
					if (ppu->pixel & 1) {
						ppu->registers[OAMDATA] = ppu->OAM[ppu->registers[OAMADDR]];
						ppu->registers[OAMADDR] &= 0b11111100;
						ppu->registers[OAMADDR] += 4;
					} else
						ppu->secondOAM[ppu->secondOAMptr]; // Dummy read for future logging
				} else {
					// There are still sprites to be evaluated
					// TODO ppu->secondOAMptr and ppu->currentSprite technically need to be incremented. Also there's a dummy read instead of a write.
					// TODO maybe join spriteInRange and else together
					if (ppu->pixel & 1) ppu->registers[OAMDATA] = ppu->OAM[ppu->registers[OAMADDR]];
					else if (ppu->spriteInRange) {
						if (ppu->sprCount < 8) ppu->secondOAM[ppu->secondOAMptr] = ppu->registers[OAMDATA];
						else ppu->secondOAM[ppu->secondOAMptr]; // Dummy read for future logging

						ppu->registers[OAMADDR]++;

						// The last byte of entry was copied
						if ((ppu->secondOAMptr++ & 0b11) == 0) {
							ppu->spriteInRange = false;
							ppu->sprCount++;

							// OAMADDR is not aligned and must be updated accordignly
							if ((ppu->registers[OAMADDR] & 0b11) != 0)
								ppu->registers[OAMADDR] &= 0b11111100;
						}
					} else {
						if (ppu->sprCount < 8) ppu->secondOAM[ppu->secondOAMptr] = ppu->registers[OAMDATA];
						else ppu->secondOAM[ppu->secondOAMptr]; // Dummy read for future logging

						// Current sprite's Y position is in range for the next scanline
						if (ppu->registers[OAMDATA] >= ppu->scanline && ppu->registers[OAMDATA] < ppu->scanline + (ppu->registers[PPUCTRL] & 0b00100000 ? 16 : 8)) {
							ppu->spriteInRange = true;
							ppu->secondOAMptr++;
							ppu->registers[OAMADDR]++;

							// Sprite zero is the first sprite read, not necessarily the one at OAM[0]
							if (ppu->pixel == 65) ppu->sprZeroOnNext = true;

							// Sprite overflow occured
							if (ppu->sprCount >= 8) ppu->registers[PPUSTATUS] |= 0b00100000;
						} else {
							ppu->registers[OAMADDR] += 4;

							// Sprite overflow bug : both the attribute and the current sprite are incremented, without carry
							if (ppu->sprCount >= 8 && (ppu->registers[OAMADDR] & 0b11) != 0b11) {
								ppu->registers[OAMADDR]++;
							} else
								ppu->registers[OAMADDR] &= 0b11111100;
						}
					}
				}
			}
			// Tile fetching
			switch ((ppu->pixel - 1) & 0b111) {
				case 0b000:
					if (ppu->pixel != 1) feedShiftRegisters(ppu);
					else if (ppu->scanline == 261) {
						ppu->registers[PPUSTATUS] = 0;
						// TODO set vbl to unasserted
						ppu->allowRegWrites = true;
					}
					PUTADDRBUS(ppu, NAMETABLEADDR(ppu));
					break;
				case 0b001: ppu->bgNametableLatch = readAddressPPU(ppu, NAMETABLEADDR(ppu)); break;
				case 0b010: PUTADDRBUS(ppu, ATTRIBUTEADDR(ppu)); break;
				case 0b011: ppu->bgPaletteLatch = readAddressPPU(ppu, ATTRIBUTEADDR(ppu)); ppu->bgPaletteLatch >>= ((ppu->addressVRAM & 0b1000000) >> 4) | (ppu->addressVRAM & 0b10); break;
				case 0b100: PUTADDRBUS(ppu, BGPATTERNADDR(ppu)); break;
				case 0b101: ppu->bgPatternLatch[0] = readAddressPPU(ppu, BGPATTERNADDR(ppu)); break;
				case 0b110: PUTADDRBUS(ppu, 0b1000 | BGPATTERNADDR(ppu)); break;
				case 0b111: ppu->bgPatternLatch[1] = readAddressPPU(ppu, 0b1000 |  BGPATTERNADDR(ppu)); break;
			}

			// Color output
			// TODO check sprite priority when rendering is partly disabled
			if (ppu->scanline != 261) renderPixel(ppu);

			shiftRegistersPPU(ppu);

		} else if (ppu->pixel <= 320) {
			if (ppu->pixel >= 280 && ppu->pixel < 305 && ppu->scanline == 261) {
				ppu->addressVRAM &= 0b111101111100000;
				ppu->addressVRAM |= ppu->tempAddressVRAM & 0b000010000011111;
			}

			ppu->registers[OAMADDR] = 0;
			// Sprite evaluation & tile fetching
			// TODO also dummy OAM reads on cycles 0b101 to 0b111
			// TODO pattern data is horizontally mirrored here, not when rendering
			uint8_t currentOAM = (ppu->pixel - 1) & 0b00111111;
			switch (currentOAM & 0b111) {
				case 0b000: ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM]; ppu->sprPatternIndex = ppu->scanline - ppu->registers[OAMDATA]; PUTADDRBUS(ppu, NAMETABLEADDR(ppu)); if (ppu->pixel == 257) feedShiftRegisters(ppu); break;
				case 0b001: ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM]; ppu->sprPatternIndex |= ppu->registers[OAMDATA] << 4; readAddressPPU(ppu, NAMETABLEADDR(ppu)); break;
				case 0b010: ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM]; ppu->sprAttributes[currentOAM >> 3] = ppu->registers[OAMDATA]; if (ppu->registers[OAMDATA] & 0b10000000) ppu->sprPatternIndex = (ppu->sprPatternIndex & 0b111111110000) | ~(ppu->sprPatternIndex & 0b1111); PUTADDRBUS(ppu, ATTRIBUTEADDR(ppu)); break;
				case 0b011: ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM]; ppu->sprXPos[currentOAM >> 3] = ppu->registers[OAMDATA]; readAddressPPU(ppu, ATTRIBUTEADDR(ppu)); break;
				case 0b100: ppu->registers[OAMDATA] = ppu->secondOAM[(currentOAM & 0b111000) | 0b000011]; PUTADDRBUS(ppu, SPRPATTERNADDR(ppu)); break;
				case 0b101: ppu->sprPatternLow[currentOAM >> 3] = readAddressPPU(ppu, SPRPATTERNADDR(ppu)); if (currentOAM >> 3 >= ppu->sprCount) ppu->sprPatternLow[currentOAM >> 3] = 0x00; break;
				case 0b110: PUTADDRBUS(ppu, 0b1000 | SPRPATTERNADDR(ppu)); break;
				case 0b111: ppu->sprPatternHigh[currentOAM >> 3] = readAddressPPU(ppu, 0b1000 | SPRPATTERNADDR(ppu)); if (currentOAM >> 3 >= ppu->sprCount) ppu->sprPatternHigh[currentOAM >> 3] = 0x00; break;
			}

			shiftRegistersPPU(ppu);

		} else if (ppu->pixel <= 336) {
			// TODO this repeats pixel <= 256
			// TODO maybe update OAMDATA only on 320 & 336
			ppu->registers[OAMDATA] = ppu->secondOAM[0];

			switch ((ppu->pixel - 1) & 0b111) {
				case 0b000:
					if (ppu->pixel == 329) feedShiftRegisters(ppu);
					PUTADDRBUS(ppu, NAMETABLEADDR(ppu));
					break;
				case 0b001: ppu->bgNametableLatch = readAddressPPU(ppu, NAMETABLEADDR(ppu)); break;
				case 0b010: PUTADDRBUS(ppu, ATTRIBUTEADDR(ppu)); break;
				case 0b011: ppu->bgPaletteLatch = readAddressPPU(ppu, ATTRIBUTEADDR(ppu)); break;
				case 0b100: PUTADDRBUS(ppu, BGPATTERNADDR(ppu)); break;
				case 0b101: ppu->bgPatternLatch[0] = readAddressPPU(ppu, BGPATTERNADDR(ppu)); break;
				case 0b110: PUTADDRBUS(ppu, 0b1000 | BGPATTERNADDR(ppu)); break;
				case 0b111: ppu->bgPatternLatch[1] = readAddressPPU(ppu, 0b1000 | BGPATTERNADDR(ppu)); break;
			}

			shiftRegistersPPU(ppu);

		} else {
			if (ppu->pixel & 0b1) {
				PUTADDRBUS(ppu, NAMETABLEADDR(ppu));
				if (ppu->pixel == 337) feedShiftRegisters(ppu);
			} else ppu->bgNametableLatch = readAddressPPU(ppu, NAMETABLEADDR(ppu));
		}
	} else if (ppu->scanline == 241 && ppu->pixel == 1) {
		ppu->registers[PPUSTATUS] |= 0b10000000;
		// TODO assert vbl
	}

	ppu->pixel++;
	if (ppu->pixel == 341) {
		ppu->pixel = 0;
		ppu->scanline++;
		if (ppu->scanline == 262) ppu->scanline = 0;
	} else if (ppu->pixel == 340 && ppu->scanline == 261 && ppu->oddFrame && RENDERING(ppu)) {
		ppu->pixel = 0;
		ppu->scanline = 0;
	}
}

#undef PUTADDRBUS
#undef NAMETABLEADDR
#undef ATTRIBUTEADDR
#undef BGPATTERNADDR
#undef SPRPATTERNADDR

#endif // ifndef PPU_H