#ifndef PPU_H
#define PPU_H

#include <stdio.h>
#include <stdint.h>

#include "bus.h"
// TODO now that we have a Bus, why is dataBusCPU needed?

// Register addresses at 0x2000
#define PPUCTRL 0
#define PPUMASK 1
#define PPUSTATUS 2
#define OAMADDR 3
#define OAMDATA 4
#define PPUSCROLL 5
#define PPUADDR 6
#define PPUDATA 7

// Bit selects for registers
#define CTRL_NMI 0b10000000
#define CTRL_SPRSIZE 0b00100000
#define CTRL_BGPATTERN 0b00010000
#define CTRL_SPRPATTERN 0b00001000
#define CTRL_ADDRINC 0b00000100
#define CTRL_NAMETABLE 0b00000011

#define MASK_EMPHASIS 0b11100000
#define MASK_RENDERSPR 0b00010000
#define MASK_RENDERBG 0b00001000
#define MASK_SHOWLEFTSPR 0b00000100
#define MASK_SHOWLEFTBG 0b00000010
#define MASK_GREYSCALE 0b00000001

#define STATUS_VBLANK 0b10000000
#define STATUS_SPR0 0b01000000
#define STATUS_OFLOW 0b00100000

#define VRAM_FINEY 0b111000000000000
#define VRAM_XNAMETABLE 0b000010000000000
#define VRAM_YNAMETABLE 0b000100000000000
#define VRAM_COARSEY 0b000001111100000
#define VRAM_COARSEX 0b000000000011111

#define SPR_PALETTE 0b00000011
#define SPR_PRIORITY 0b00100000
#define SPR_HORSYMMETRY 0b01000000
#define SPR_VERTSYMMETRY 0b10000000

typedef struct PPU {
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
	uint8_t secondOAMptr; // Pointer to next free second OAM location / next second OAM location to be evaluated
	bool spriteInRange; // The sprite being evaluated is on next scanline (copy it to second OAM)
	uint8_t sprCount; // Number of sprites on next scanline
	uint16_t sprPatternIndex; // Location of the pattern of next sprite, given by second OAM bytes 0 and 1 (final location given by SPRPATTERNADDR)
	bool sprZeroOnNext; // Sprite zero detected to be in next scanline
	bool sprZeroOnCurrent; // Initialized from sprZeroOnNext for current scanline

	// Internal registers for fetching and rendering
	uint16_t addressVRAM; // Also called 'v'
	uint16_t tempAddressVRAM; // Also called 't'
	uint8_t readBufferVRAM; // Used in PPUDATA writes
	uint8_t fineX; // Completes the scroll position with tempAddressVRAM
	bool secondWrite; // Next write to PPUSCROLL or PPUADDR is the second

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

	// VBL pin connected to the NMI pin of the 6502
	bool outInterrupt;

	uint16_t scanline;
	uint16_t pixel;

	uint8_t *framebuffer;

	Bus *bus;
} PPU;


// Interface functions
void initPPU(PPU *ppu, uint8_t *framebuffer, Bus *bus);
void tickPPU(PPU *ppu);
uint8_t readRegisterPPU(PPU *ppu, uint16_t reg);
void writeRegisterPPU(PPU *ppu, uint16_t reg, uint8_t value);
void loadPalette(PPU *ppu, uint8_t values[192]);

// Non-interface functions
void shiftRegistersPPU(PPU *ppu);
void incrementX(PPU *ppu);
void incrementY(PPU *ppu);
void feedShiftRegisters(PPU *ppu);
void renderPixel(PPU *ppu);
uint8_t flipByte(uint8_t value);

#endif // ifndef PPU_H