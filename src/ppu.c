#include "ppu.h"

// Undefined later
#define PUTADDRBUS(ppu, address) ppu->addressBusLatch = address
#define RENDERING(ppu) ((ppu->registers[PPUMASK] & (MASK_RENDERSPR | MASK_RENDERBG)) && (ppu->scanline < 240 || ppu->scanline == 261))
#define UPDATENMI(ppu) ppu->outInterrupt = !((ppu->registers[PPUSTATUS] & STATUS_VBLANK) && (ppu->registers[PPUCTRL] & CTRL_NMI))
#define NAMETABLEADDR(ppu) (0x2000 | (ppu->addressVRAM & 0x0FFF))
#define ATTRIBUTEADDR(ppu) (0x23C0 | (ppu->addressVRAM & (VRAM_XNAMETABLE | VRAM_YNAMETABLE)) | ((ppu->addressVRAM & VRAM_COARSEX) >> 2) | ((ppu->addressVRAM & 0b1110000000) >> 4))
#define BGPATTERNADDR(ppu) (((ppu->registers[PPUCTRL] & CTRL_BGPATTERN) << 8) | (ppu->bgNametableLatch << 4) | ((ppu->addressVRAM & VRAM_FINEY) >> 12))
// Just trust me for this one
#define SPRPATTERNADDR(ppu) (((ppu->registers[PPUCTRL] & CTRL_SPRSIZE) \
							? ((ppu->sprPatternIndex & 0b10000) << 8) | (ppu->sprPatternIndex & (VRAM_XNAMETABLE | VRAM_YNAMETABLE | VRAM_COARSEY)) | ((ppu->sprPatternIndex & 0b1000) << 1) \
							: ((ppu->sprPatternIndex & 0b111111110000) | ((ppu->registers[PPUCTRL] & CTRL_SPRPATTERN) << 9))) | (ppu->sprPatternIndex & 0b111))


// Non-interface functions
void shiftRegistersPPU(PPU *ppu) {
	// These internal registers use the most significant bit to render next
	ppu->bgPatternData[0] <<= 1;
	ppu->bgPaletteData[0] <<= 1;
	ppu->bgPatternData[1] <<= 1;
	ppu->bgPaletteData[1] <<= 1;
	ppu->bgPaletteData[0] |= ppu->bgSerialPaletteLatch[0];
	ppu->bgPaletteData[1] |= ppu->bgSerialPaletteLatch[1];
}

void incrementX(PPU *ppu) {
	if ((ppu->addressVRAM & VRAM_COARSEX) == VRAM_COARSEX) {
		ppu->addressVRAM &= ~VRAM_COARSEX;
		ppu->addressVRAM ^= VRAM_XNAMETABLE;
	} else ppu->addressVRAM++;
}

void incrementY(PPU *ppu) {
	if ((ppu->addressVRAM & VRAM_FINEY) == VRAM_FINEY) {
		ppu->addressVRAM &= ~VRAM_FINEY;
		switch ((ppu->addressVRAM & VRAM_COARSEY) >> 5) {
			case 0b11101: ppu->addressVRAM ^= VRAM_YNAMETABLE;
			case 0b11111: ppu->addressVRAM &= ~VRAM_COARSEY; break;
			default: ppu->addressVRAM += 0b100000;
		}
	} else ppu->addressVRAM += 0b001000000000000;
}

void feedShiftRegisters(PPU *ppu) {
	ppu->bgPatternData[0] |= ppu->bgPatternLatch[0];
	ppu->bgPatternData[1] |= ppu->bgPatternLatch[1];
	// Unlike pattern data, bgPaletteLatch uses its least significant bits to render next
	ppu->bgSerialPaletteLatch[0] = ppu->bgPaletteLatch & 0b1;
	ppu->bgSerialPaletteLatch[1] = ppu->bgPaletteLatch & 0b10;
}

void renderPixel(PPU *ppu) {
	uint8_t bgColor = 0, sprColor = 0, attributes = 0, outputUnit = 8;

	// Updates X position of sprites and checks for the first active sprite
	uint16_t pix = ppu->pixel;
	for (int i = 7; i >= 0; i--) {
		const uint8_t xPos = ppu->sprXPos[i];
		const uint8_t shiftValue = (pix - xPos);
		// Checks if (x pos in range of current scanline) AND (selected color bit is non-zero)
		if ((pix >= xPos && pix < xPos + 8) && (((ppu->sprPatternLow[i] | ppu->sprPatternHigh[i]) << shiftValue) & 0x80)) {
			// Similar to background data, most significant bits of sprite data are the next to be rendered
			sprColor = ((ppu->sprPatternLow[i] << shiftValue) & 0x80) >> 7;
			sprColor |= ((ppu->sprPatternHigh[i] << shiftValue) & 0x80) >> 6;

			attributes = ppu->sprAttributes[i];
			outputUnit = i;
		}
	}

	// Most significant bits of bgPatternData (0x8000; bit 15) and bgPaletteData (0x80; bit 7) are the next one to be rendered
	bgColor = ((ppu->bgPatternData[0] << ppu->fineX) & 0x8000) >> 15;
	bgColor |= ((ppu->bgPatternData[1] << ppu->fineX) & 0x8000) >> 14;

	// Disables rendering according to PPUMASK
	if (!(ppu->registers[PPUMASK] & MASK_RENDERSPR) || (!(ppu->registers[PPUMASK] & MASK_SHOWLEFTSPR) && pix < 8))
		sprColor = 0;
	if (!(ppu->registers[PPUMASK] & MASK_RENDERBG) || (!(ppu->registers[PPUMASK] & MASK_SHOWLEFTBG) && pix < 8))
		bgColor = 0;

	// Checks for sprite 0 hits
	if (ppu->sprZeroOnCurrent && outputUnit == 0 && sprColor && bgColor && pix != 255)
		ppu->registers[PPUSTATUS] |= STATUS_SPR0;

	// Multiplexer : checks which pixel (background, sprite or backdrop) should be rendered on screen
	// Only when sprite priority has been computed we can add palette information (palette doesn't affect priority, so we can use !bgColor in peace)
	uint8_t paletteIndex = 0;
	if (sprColor && (!bgColor || !(attributes & SPR_PRIORITY))) {
		paletteIndex = sprColor | 0b10000;
		paletteIndex |= (attributes & 0b11) << 2;
	} else if (bgColor) {
		paletteIndex = bgColor;
		paletteIndex |= ((ppu->bgPaletteData[0] << ppu->fineX) & 0x80) >> 5;
		paletteIndex |= ((ppu->bgPaletteData[1] << ppu->fineX) & 0x80) >> 4;
	}

	// Weird palette mirroring
	if ((paletteIndex & 0b11) == 0) {
		paletteIndex &= 0b10000;
	}

	// Background palette hack : when rendering is disabled, background is determined by VRAM address if in range [3F00, 3FFF]
	if (!RENDERING(ppu) && ppu->addressVRAM > 0x3F00 && ppu->addressVRAM <= 0x3FFF) {
		paletteIndex = (ppu->addressVRAM - 0x3F00) & 0b1111;
	}

	// Render with greyscale accprding to PPUMASK
	int framebufferIndex = (ppu->scanline * 256 + pix) * 3;
	ppu->framebuffer[framebufferIndex] = ppu->colors[ppu->palettes[paletteIndex] & (ppu->registers[PPUMASK] & 0b1 ? 0x30 : 0x3F)][0];
	ppu->framebuffer[framebufferIndex + 1] = ppu->colors[ppu->palettes[paletteIndex] & (ppu->registers[PPUMASK] & 0b1 ? 0x30 : 0x3F)][1];
	ppu->framebuffer[framebufferIndex + 2] = ppu->colors[ppu->palettes[paletteIndex] & (ppu->registers[PPUMASK] & 0b1 ? 0x30 : 0x3F)][2];
}

uint8_t flipByte(uint8_t value) {
	uint8_t result = 0;
	for (int i = 0; i < 8; i++)
		result |= (value & (1 << i)) >> i << (7 - i);

	return result;
}


// Interface functions
void initPPU(PPU *ppu, uint8_t *framebuffer, Bus *bus) {
	// This emulates the PPU behaviour when it is first powered up after being off for some time. This is NOT perfectly good emulation for its behaviour on reset or on bootup when the NES was just recently turned off.
	// The main differences between power and reset are various registers working immediately and their content 
	ppu->dataBusCPU = 0;
	ppu->addressBusLatch = 0;
	ppu->scanline = ppu->pixel = 0;
	// TODO do we really need OAMDATA, PPUDATA, PPUADR and PPUSCROLL as registers
	// TODO load colors with default value
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

	ppu->allowRegWrites = true;

	UPDATENMI(ppu);

	// TODO assuming framebuffer is valid is a dangerous game. However, mallocating it ourselves would just add the need for a terminatePPU function, and would add no real benefit (the indexing would be the same because it would need to be heap-allocated anyway)
	ppu->framebuffer = framebuffer;
	ppu->bus = bus;

	// TODO remove this and check palettes initial value
	for (int i = 0; i < 32; i++) ppu->palettes[i] = i;
}

uint8_t readRegisterPPU(PPU *ppu, uint16_t reg) {
	switch (reg & 0b111) {
		case PPUCTRL:
		case PPUMASK:
		case OAMADDR:
		case PPUSCROLL:
		case PPUADDR:
			break;
		case PPUSTATUS:
			ppu->dataBusCPU &= 0b00011111;
			ppu->dataBusCPU |= (ppu->registers[PPUSTATUS] & 0b11100000);
			// Clears VBlank flag and updates NMI output accordingly
			ppu->registers[PPUSTATUS] &= ~STATUS_VBLANK;
			UPDATENMI(ppu);
			ppu->secondWrite = false;
			break;
		case OAMDATA:
			if (!RENDERING(ppu))
				ppu->registers[OAMDATA] = ppu->OAM[ppu->registers[OAMADDR]];
			ppu->dataBusCPU = ppu->registers[OAMDATA];
			break;
		case PPUDATA:
			if (ppu->addressVRAM < 0x3F00) {
				ppu->dataBusCPU = ppu->readBufferVRAM;
				// TODO readBufferVRAM is only updated "at the PPU's earliest convenience"
				// TODO this is not the NES behaviour at all, just temporary to get something working
				PUTADDRBUS(ppu, ppu->addressVRAM);
				ppu->readBufferVRAM = ppuRead(ppu->bus, ppu->addressVRAM);
			} else {
				// TODO this is handled incorrectly
				ppu->dataBusCPU = ppuRead(ppu->bus, 0x2000 | (ppu->addressVRAM & 0x0FFF));
				ppu->readBufferVRAM = ppuRead(ppu->bus, 0x2000 | (ppu->addressVRAM & 0x0FFF));
			}
			// TODO inc is weird during rendering
			ppu->addressVRAM += (ppu->registers[PPUCTRL] & CTRL_ADDRINC ? 32 : 1);
			break;
	}
	return ppu->dataBusCPU;
}

void writeRegisterPPU(PPU *ppu, uint16_t reg, uint8_t value) {
	ppu->dataBusCPU = value;
	switch (reg & 0b111) {
		case PPUCTRL:
			if (ppu->allowRegWrites) {
				ppu->registers[PPUCTRL] = value;
				// Updates VRAM address and NMI output
				ppu->tempAddressVRAM &= ~(VRAM_XNAMETABLE | VRAM_YNAMETABLE);
				ppu->tempAddressVRAM |= (value & 0b11) << 10;
				UPDATENMI(ppu);
				// TODO apparently sometimes an open bus value is written
				// TODO replicate other bugs
			}
			break;
		case PPUMASK:
			if (ppu->allowRegWrites)
				ppu->registers[PPUMASK] = value;
			break;
		case PPUSTATUS:
			break;
		case OAMADDR:
			// OAM corruption
			for (int i = 0; i < 8; i++)
				ppu->OAM[(reg & 0xF8) + i] = ppu->OAM[(ppu->registers[OAMDATA] & 0xF8) + i];
				
			ppu->registers[OAMADDR] = value;
			break;
		case OAMDATA:
			if (RENDERING(ppu))
				// TODO according to NesDev, "it's plausible that it could bump the low bits instead depending on the current status of sprite evaluation"
				ppu->registers[OAMADDR] += 4;
			else {
				ppu->OAM[ppu->registers[OAMADDR]] = value;
				ppu->registers[OAMADDR]++;
			}
			break;
		case PPUSCROLL:
			if (ppu->allowRegWrites) {
				if (!ppu->secondWrite) {
					ppu->tempAddressVRAM &= ~VRAM_COARSEX;
					ppu->tempAddressVRAM |= value >> 3;
					ppu->fineX = value & 0b111;
				} else {
					ppu->tempAddressVRAM &= ~VRAM_COARSEY & ~VRAM_FINEY;
					ppu->tempAddressVRAM |= (value & 0b11111000) << 2;
					ppu->tempAddressVRAM |= (value & 0b111) << 12;
				}
				ppu->secondWrite = !ppu->secondWrite;
			}
			break;
		case PPUADDR:
			if (ppu->allowRegWrites) {
				// TODO bus conflicts etc.
				if (!ppu->secondWrite) {
					ppu->tempAddressVRAM &= 0b000000011111111;
					ppu->tempAddressVRAM |= (value & 0b00111111) << 8;
				} else {
					ppu->tempAddressVRAM &= 0b111111100000000;
					ppu->tempAddressVRAM |= value;
					ppu->addressVRAM = ppu->tempAddressVRAM;
				}
				ppu->secondWrite = !ppu->secondWrite;
			}
			break;
		case PPUDATA:
			if (!RENDERING(ppu)) {
				// Outside of rendering; VRAM accesses are 2 cycles, explaining the presence of the read buffer
				// TODO the write itself takes 2 cycles, but I'm unsure how the PPU handles this (presumably immediatly sends the low bits with /ALE, "saves" the high bits in a temporary location then, on the next cycle, sends them with /R)
				// I can't find documentation on how the PPU handles the reads being 2 cycles, so I'll just do it in one. It's not a good solution, but at least it shouldn't have graphical implications, as this is only in VBlank.
				PUTADDRBUS(ppu, ppu->addressVRAM);
				ppuWrite(ppu->bus, ppu->addressVRAM, value);
				ppu->addressVRAM += (ppu->registers[PPUCTRL] & CTRL_ADDRINC) ? 32 : 1;
			} else {
				// During rendering; exact behaviour not yet implemented due to highly unpredictable results
				// TODO it is believed that if this is done during a cycle that would otherwise increment X or Y, the increments won't be doubled. Also the read itself yields highly unpredictable results (/R and /W or /ALE may be asserted at the same time, and the address gets mixed up)
				incrementX(ppu);
				incrementY(ppu);
			}
			break;
	}
}

void loadPalette(PPU *ppu, uint8_t colors[192]) {
	for (int i = 0; i < 64; i++)
		for (int j = 0; j < 3; j++)
			ppu->colors[i][j] = colors[i * 3 + j];
}

void tickPPU(PPU *ppu) {
	// TODO color emphasis
	// TODO palette addressing / mirroring etc
	// TODO when rendering starts, if OAMADDR >= 8, OAM is corrupted slightly
	// TODO make oam[attribute] & 0b00011100 always 0
	// TODO words cannot describe how ugly this whole thing is
	// TODO I have no idea how any of this works, so anyone else won't either

	// This is used numerous times and does not change withing a cycle so we only compute it once
	const bool isRendering = RENDERING(ppu);
	const uint16_t pix = ppu->pixel;

	// Scanline-independant logic
	if (isRendering) {
		if (pix == 256) {
			incrementY(ppu);
		} else if ((pix & 0b111) == 0 && (pix <= 256 || pix >= 328) && pix != 0 && ppu->scanline) {
			incrementX(ppu);
		} else if (pix == 257) {
			ppu->addressVRAM &= ~(VRAM_COARSEX | VRAM_XNAMETABLE);
			ppu->addressVRAM |= ppu->tempAddressVRAM & (VRAM_COARSEX | VRAM_XNAMETABLE);
		}
	}

	// Visible scanlines
	// TODO the nested if/elses are quite a pain
	if (ppu->scanline < 240 || ppu->scanline == 261) {
		if (pix == 0) {
			if (ppu->scanline == 0 && ppu->oddFrame && isRendering) {
				ppu->bgNametableLatch = ppuRead(ppu->bus, NAMETABLEADDR(ppu));
			} else if (isRendering) {
				PUTADDRBUS(ppu, BGPATTERNADDR(ppu));
			}
				
			if (ppu->scanline == 261) ppu->oddFrame = !ppu->oddFrame;
			else renderPixel(ppu);

			ppu->spriteInRange = ppu->sprZeroOnNext = false;
			ppu->secondOAMptr = ppu->sprCount = 0;
		} else if (pix <= 256) {

			// Sprite evaluation
			if (isRendering && ppu->scanline != 261) {
				if (pix <= 64) {
					// Cycles 1-64 : fills the secondary OAM with 0xFF
					if (pix & 1) {
						// ppu->OAM[ppu->registers[OAMADDR]]; // Dummy read for future logging
						ppu->registers[OAMDATA] = 0xFF;
					} else {
						ppu->secondOAM[(pix - 1) >> 1] = ppu->registers[OAMDATA];
					}
				} else if ((ppu->registers[OAMADDR] == 0 && pix > 66) || ((ppu->registers[OAMADDR] & 0b11111100) == 0 && ppu->sprCount >= 8)) {
					// There are no more sprites to be evaluated
					// The above if statement is to ensure this is reached if we reached the end of OAM without filling up secondOAM (in which case OAMADDR will always be 0) OR if we did fill it (in which case the sprite overflow bug occured, so OAMADDR will be anywhere between 0 and 3)
					if (pix & 1) {
						ppu->registers[OAMDATA] = ppu->OAM[ppu->registers[OAMADDR]];
						ppu->registers[OAMADDR] &= 0b11111100;
						// TODO fix this
						// ppu->registers[OAMADDR] += 4;
					} // else
						// ppu->secondOAM[ppu->secondOAMptr]; // TODO Dummy read for future logging
				} else {
					// There are still sprites to be evaluated
					// TODO maybe join spriteInRange and else together
					if (pix & 1) ppu->registers[OAMDATA] = ppu->OAM[ppu->registers[OAMADDR]];
					else if (ppu->spriteInRange) {
						if (ppu->sprCount < 8)
							ppu->secondOAM[ppu->secondOAMptr] = ppu->registers[OAMDATA];
						// else ppu->secondOAM[ppu->secondOAMptr]; // Dummy read for future logging

						ppu->registers[OAMADDR]++;
						ppu->secondOAMptr++;

						// The last byte of entry was copied
						if ((ppu->secondOAMptr & 0b11) == 0) {
							ppu->spriteInRange = false;
							ppu->sprCount++;

							// OAMADDR is not aligned and must be updated accordignly
							if ((ppu->registers[OAMADDR] & 0b11) != 0)
								ppu->registers[OAMADDR] &= 0b11111100;
						}
					} else {
						if (ppu->sprCount < 8)
							ppu->secondOAM[ppu->secondOAMptr] = ppu->registers[OAMDATA];
						// else ppu->secondOAM[ppu->secondOAMptr]; // TODO Dummy read for future logging

						// Current sprite's Y position is in range for the next scanline
						if (ppu->scanline >= ppu->registers[OAMDATA] && ppu->scanline < ppu->registers[OAMDATA] + (ppu->registers[PPUCTRL] & CTRL_SPRSIZE ? 16 : 8)) {
							ppu->spriteInRange = true;
							ppu->secondOAMptr++;
							ppu->registers[OAMADDR]++;

							// Sprite zero is the first sprite read, not necessarily the one at OAM[0]
							if (pix == 66) ppu->sprZeroOnNext = true;

							// Sprite overflow occured
							if (ppu->sprCount >= 8) ppu->registers[PPUSTATUS] |= STATUS_OFLOW;
						} else {
							ppu->registers[OAMADDR] += 4;

							// Sprite overflow bug : both the attribute and the current sprite are incremented, without carry
							if (ppu->sprCount >= 8 && (ppu->registers[OAMADDR] & 0b11) != 0b11)
								ppu->registers[OAMADDR]++;
							else
								ppu->registers[OAMADDR] &= 0b11111100;
						}
					}
				}
			}

			// Status update
			if (((pix - 1) & 0b111) == 0) {
				if (pix != 1)
					feedShiftRegisters(ppu);
				else if (ppu->scanline == 261) {
					ppu->registers[PPUSTATUS] = 0;
					UPDATENMI(ppu);
					ppu->allowRegWrites = true;
				}
			}

			// Tile fetching
			if (isRendering) {
				switch ((pix - 1) & 0b111) {
					case 0b000: PUTADDRBUS(ppu, NAMETABLEADDR(ppu)); break;
					case 0b001: ppu->bgNametableLatch = ppuRead(ppu->bus, NAMETABLEADDR(ppu)); break;
					case 0b010: PUTADDRBUS(ppu, ATTRIBUTEADDR(ppu)); break;
					case 0b011: ppu->bgPaletteLatch = ppuRead(ppu->bus, ATTRIBUTEADDR(ppu)); ppu->bgPaletteLatch >>= ((ppu->addressVRAM & 0b1000000) >> 4) | (ppu->addressVRAM & 0b10); break;
					case 0b100: PUTADDRBUS(ppu, BGPATTERNADDR(ppu)); break;
					case 0b101: ppu->bgPatternLatch[0] = ppuRead(ppu->bus, BGPATTERNADDR(ppu)); break;
					case 0b110: PUTADDRBUS(ppu, 0b1000 | BGPATTERNADDR(ppu)); break;
					case 0b111: ppu->bgPatternLatch[1] = ppuRead(ppu->bus, 0b1000 | BGPATTERNADDR(ppu)); break;
				}
			}
			
			// Color output
			if (ppu->scanline != 261) renderPixel(ppu);

			// Status update 2: Electric Boogaloo
			shiftRegistersPPU(ppu);

		} else if (pix <= 320) {
			// Status update
			ppu->sprZeroOnCurrent = ppu->sprZeroOnNext;
			ppu->registers[OAMADDR] = 0;

			if (pix >= 280 && pix < 305 && ppu->scanline == 261 && isRendering) {
				ppu->addressVRAM &= ~(VRAM_COARSEY | VRAM_FINEY | VRAM_YNAMETABLE);
				ppu->addressVRAM |= ppu->tempAddressVRAM & (VRAM_COARSEY | VRAM_FINEY | VRAM_YNAMETABLE);
			} else if (pix == 257) {
				feedShiftRegisters(ppu);
			}

			// Sprite evaluation & tile fetching
			if (isRendering) {
				const uint8_t currentOAM = ((pix - 1) & 0b11) | (((pix - 1) >> 1) & 0b11100);
				const uint8_t currentSprite = ((pix - 1) >> 3) & 0b111;
				switch ((pix - 1) & 0b111) {
					case 0b000:
						// Sprite Y position
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM];
						ppu->sprPatternIndex = ppu->scanline - ppu->registers[OAMDATA];

						// Garbage nametable
						PUTADDRBUS(ppu, NAMETABLEADDR(ppu));
						break;
					case 0b001:
						// Sprite index
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM];
						ppu->sprPatternIndex |= ppu->registers[OAMDATA] << 4;

						// Garbage nametable
						ppuRead(ppu->bus, NAMETABLEADDR(ppu));
						break;
					case 0b010:
						// Sprite attributes
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM];
						ppu->sprAttributes[currentSprite] = ppu->registers[OAMDATA];
						if (ppu->registers[OAMDATA] & SPR_VERTSYMMETRY) {
							// TODO maybe add macros for those pattern bitmaps ?
							ppu->sprPatternIndex = (ppu->sprPatternIndex & 0b111111111000) | (7 - (ppu->sprPatternIndex & 0b111)); // Vertical symmetry, if applicable
						}

						// Garbage attribute table
						PUTADDRBUS(ppu, ATTRIBUTEADDR(ppu));
						break;
					case 0b011:
						// Sprite X position
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM];
						ppu->sprXPos[currentSprite] = ppu->registers[OAMDATA];

						// Garbage attribute table
						ppuRead(ppu->bus, ATTRIBUTEADDR(ppu));
						break;
					case 0b100:
						// Garbage OAM read
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM | 0b11];

						// Sprite pattern fetch
						PUTADDRBUS(ppu, SPRPATTERNADDR(ppu));
						break;
					case 0b101:
						// Garbage OAM read
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM | 0b11];

						// Sprite pattern fetch
						ppu->sprPatternLow[currentSprite] = ppuRead(ppu->bus, SPRPATTERNADDR(ppu));
						if (currentSprite >= ppu->sprCount)
							ppu->sprPatternLow[currentSprite] = 0x00;
						else if (ppu->sprAttributes[currentSprite] & SPR_HORSYMMETRY)
							ppu->sprPatternLow[currentSprite] = flipByte(ppu->sprPatternLow[currentSprite]); // Horizontal symmetry, if applicable
						break;
					case 0b110:
						// Garbage OAM read and sprite high pattern fetch
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM | 0b11];
						PUTADDRBUS(ppu, 0b1000 | SPRPATTERNADDR(ppu));
						break;
					case 0b111:
						// Garbage OAM read and sprite high pattern fetch
						ppu->registers[OAMDATA] = ppu->secondOAM[currentOAM | 0b11];
						ppu->sprPatternHigh[currentSprite] = ppuRead(ppu->bus, 0b1000 | SPRPATTERNADDR(ppu));
						if (currentSprite >= ppu->sprCount)
							ppu->sprPatternHigh[currentSprite] = 0x00;
						else if (ppu->sprAttributes[currentSprite] & SPR_HORSYMMETRY)
							ppu->sprPatternHigh[currentSprite] = flipByte(ppu->sprPatternHigh[currentSprite]); // Horizontal symmetry, if applicable
						break;
				}
			}

			shiftRegistersPPU(ppu);

		} else if (pix <= 336) {
			// TODO this repeats pixel <= 256
			// Status update
			ppu->registers[OAMDATA] = ppu->secondOAM[0];
			if (pix == 329) feedShiftRegisters(ppu);

			if (isRendering) {
				switch ((pix - 1) & 0b111) {
					case 0b000: PUTADDRBUS(ppu, NAMETABLEADDR(ppu)); break;
					case 0b001: ppu->bgNametableLatch = ppuRead(ppu->bus, NAMETABLEADDR(ppu)); break;
					case 0b010: PUTADDRBUS(ppu, ATTRIBUTEADDR(ppu)); break;
					case 0b011: ppu->bgPaletteLatch = ppuRead(ppu->bus, ATTRIBUTEADDR(ppu)); ppu->bgPaletteLatch >>= ((ppu->addressVRAM & 0b1000000) >> 4) | (ppu->addressVRAM & 0b10); break;
					case 0b100: PUTADDRBUS(ppu, BGPATTERNADDR(ppu)); break;
					case 0b101: ppu->bgPatternLatch[0] = ppuRead(ppu->bus, BGPATTERNADDR(ppu)); break;
					case 0b110: PUTADDRBUS(ppu, 0b1000 | BGPATTERNADDR(ppu)); break;
					case 0b111: ppu->bgPatternLatch[1] = ppuRead(ppu->bus, 0b1000 | BGPATTERNADDR(ppu)); break;
				}
			}

			shiftRegistersPPU(ppu);

		} else {
			ppu->registers[OAMDATA] = ppu->secondOAM[0];
			if (pix == 337) feedShiftRegisters(ppu);

			if (isRendering) {
				if (pix & 0b1)
					PUTADDRBUS(ppu, NAMETABLEADDR(ppu));
				else
					ppu->bgNametableLatch = ppuRead(ppu->bus, NAMETABLEADDR(ppu));
			}
		}
	} else if (ppu->scanline == 241 && pix == 1) {
		ppu->registers[PPUSTATUS] |= STATUS_VBLANK;
		UPDATENMI(ppu);
	}

	if (++ppu->pixel == 341) {
		ppu->pixel = 0;
		ppu->scanline++;
		if (ppu->scanline == 262) ppu->scanline = 0;
	} else if (ppu->pixel == 340 && ppu->scanline == 261 && ppu->oddFrame && isRendering) {
		ppu->pixel = 0;
		ppu->scanline = 0;
	}
}

#undef PUTADDRBUS
//#undef NAMETABLEADDR TODO uncomment
#undef ATTRIBUTEADDR
#undef BGPATTERNADDR
#undef SPRPATTERNADDR