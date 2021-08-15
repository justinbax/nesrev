#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
	uint8_t PCL; // Low byte of program counter
	uint8_t PCH; // High byte of program counter
	uint8_t SP; // Stack pointer register
	uint8_t IR; // Instruction register
	uint8_t step; // Micro-instruction step counter to keep track of the current step inside an instruction

	uint8_t A;
	uint8_t X;
	uint8_t Y;

	uint8_t B; // With A, this is the second ALU register, not accessible by the user but used to perform arithmetic and logical operations on memory data during read-modify-write instructions

	bool negFlag;
	bool oflowFlag;
	bool decFlag;
	bool noIRQFlag;
	bool zeroFlag;
	bool carryFlag;

	// I am unaware of the specific usage (and even existence!) of these registers, as they are very rarely documented
	// However, to achieve cycle accuracy, they are needed in emulation
	// Fortunately, this has no effect whatsoever on the output of the NES, as none of them change the timing of read/writes.
	uint8_t DPL; // Low byte of data pointer register used in address calculation
	uint8_t DPH; // High byte of data pointer register used in address calculation
	uint8_t temp; // Temporary register used during indirect addressing modes
} CPU;

// Interface functions
void initCPU(CPU *cpu);
extern inline void tickCPU(CPU *cpu);

// Non-interface functions, still accessible by external code (explanation below)
extern inline uint8_t read(uint16_t address);
extern inline void write(uint16_t address, uint8_t data);
extern inline uint8_t fetch(CPU *cpu);
extern inline void push(CPU *cpu, uint8_t data);
extern inline uint8_t pull(CPU *cpu);
extern inline void zpiAddressing(CPU *cpu, uint8_t indexReg);
extern inline void absAddressing(CPU *cpu, uint8_t indexReg);
extern inline void izxAddressing(CPU *cpu);
extern inline void izyAddressing(CPU *cpu);
extern inline void branch(CPU *cpu, bool condition);
extern inline void nzFlags(CPU *cpu, uint8_t result);
extern inline void add(CPU *cpu, uint8_t value);


// The definition of all functions (even those which aren't supposed to be accessed by external code -- all of them except initCPU and tickCPU) are in the header file instead of a source file to make use of inlining
// Inline functions are also extern in order to be forced in the same translation unit as external code (a requirement of C inlining)

// Undefined later
#define DATAPTR(cpu) (cpu->DPH << 8) | cpu->DPL
#define PROGCOUNTER(cpu) ((cpu)->PCH << 8) | (cpu)->PCL
#define END(cpu) cpu->step = -1
#define RESET_STEP 0xF0

extern inline uint8_t read(uint16_t address) {
	// TODO this
	uint8_t data = 0x00;
	printf("%4X r %2X\n", address, data);
	return data;
}

extern inline void write(uint16_t address, uint8_t data) {
	printf("%4X W %2X\n", address, data);
	// TODO this
}

extern inline uint8_t fetch(CPU *cpu) {
	uint8_t result = read(PROGCOUNTER(cpu));
	cpu->PCL++;
	cpu->PCH += !cpu->PCL;
	return result;
}

extern inline void push(CPU *cpu, uint8_t data) {
	write(0x0100 | cpu->SP, data);
	cpu->SP--;
}

extern inline uint8_t pull(CPU *cpu) {
	uint8_t result = read(0x0100 | cpu->SP);
	cpu->SP++;
	return result;
}

extern inline void zpiAddressing(CPU *cpu, uint8_t indexReg) {
	switch (cpu->step) {
		case 1: cpu->DPL = fetch(cpu); break;
		case 2: read(0x0000 | cpu->DPL); cpu->DPL += indexReg; break;
	}
}

extern inline void absAddressing(CPU *cpu, uint8_t indexReg) {
	switch (cpu->step) {
		case 1: cpu->DPL = fetch(cpu); break;
		case 2: cpu->DPH = fetch(cpu); cpu->DPL += indexReg; break;
	}
}

extern inline void izxAddressing(CPU *cpu) {
	switch (cpu->step) {
		case 1: cpu->temp = fetch(cpu); break;
		case 2: read(0x0000 | cpu->temp); cpu->temp += cpu->X; break;
		case 3: cpu->DPL = read(0x0000 | cpu->temp); cpu->temp++; break;
		case 4: cpu->DPH = read(0x0000 | cpu->temp); break;
	}
}

extern inline void izyAddressing(CPU *cpu) {
	switch (cpu->step) {
		case 1: cpu->temp = fetch(cpu); break;
		case 2: cpu->DPL = read(0x0000 | cpu->temp); cpu->temp++; break;
		case 3: cpu->DPH = read(0x0000 | cpu->temp); cpu->DPL += cpu->Y; break;
	}
}

extern inline void branch(CPU *cpu, bool condition) {
	switch (cpu->step) {
		case 1: cpu->temp = fetch(cpu); if (!condition) END(cpu); break;
		// TODO ???
		case 2: read(PROGCOUNTER(cpu)); cpu->PCL += ((cpu->temp & 0b10000000) > 0 ? cpu->temp - 256 : cpu->temp); if (!(((cpu->temp & 0b10000000) == 0 && cpu->PCL < cpu->temp) || ((cpu->temp & 0b10000000) > 0 && cpu->PCL > (~cpu->temp) + 1))) END(cpu); break;
		case 3: read(PROGCOUNTER(cpu)); if (cpu->temp < 128 && cpu->PCL > cpu->temp) cpu->PCH--; else cpu->PCH++; break;
	}
}

extern inline void nzFlags(CPU *cpu, uint8_t result) {
	cpu->negFlag = result & 0b10000000;
	cpu->zeroFlag = !result;
}

extern inline void add(CPU *cpu, uint8_t value) {
	uint8_t result = cpu->A + value + cpu->carryFlag;
	cpu->carryFlag = (result < cpu->A);
	cpu->oflowFlag = (((cpu->A ^ result) & (value & result) & 0b10000000) > 0);
	cpu->A = result;
	nzFlags(cpu, cpu->A);
}


void initCPU(CPU *cpu) {
	// Note: this is the status of the CPU BEFORE the reset sequence
	cpu->A = cpu->X = cpu->Y = 0;
	cpu->PCH = 0x00;
	cpu->PCL = 0x00;
	cpu->negFlag = cpu->oflowFlag = cpu->decFlag = cpu->zeroFlag = cpu->carryFlag = false;
	cpu->noIRQFlag = true;
	cpu->SP = 0x00;
	cpu->step = RESET_STEP;
}

extern inline void tickCPU(CPU *cpu) {
	// TODO reset routine
	// TODO reorder (0B and 2B are both ANC_IMM, E9 and EB are both SBC_IMM)
	// TODO none of this is tested
	if (cpu->step == 0) {
		cpu->IR = fetch(cpu);
	} else {
		switch ((cpu->step << 8) | cpu->IR) {
			// Because the micro-instruction step counter will never exceed 3 bits in width, all cases setting a bit 3-7 will never occur during execution and can be used for other micro-instructions
			// RESET
			case 0x00 | ((RESET_STEP + 0) << 8):
			case 0x00 | ((RESET_STEP + 1) << 8):
			case 0x00 | ((RESET_STEP + 2) << 8): read(0x00FF); break;
			case 0x00 | ((RESET_STEP + 3) << 8): 
			case 0x00 | ((RESET_STEP + 4) << 8):
			case 0x00 | ((RESET_STEP + 5) << 8): read(0x0100 | cpu->SP); cpu->SP--; break;
			case 0x00 | ((RESET_STEP + 6) << 8): cpu->PCL = read(0xFFFC); break;
			case 0x00 | ((RESET_STEP + 7) << 8): cpu->PCH = read(0xFFFD); END(cpu); break;

			// BRK
			case 0x00 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x00 | (0b010 << 8): push(cpu, cpu->PCH); break;
			case 0x00 | (0b011 << 8): push(cpu, cpu->PCL); break;
			case 0x00 | (0b100 << 8): push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00110000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); break;
			case 0x00 | (0b101 << 8): cpu->PCL = read(0xFFFE); break;
			case 0x00 | (0b110 << 8): cpu->PCH = read(0xFFFF); END(cpu); break;

			// ORA_IZX
			case 0x01 | (0b001 << 8):
			case 0x01 | (0b010 << 8):
			case 0x01 | (0b011 << 8):
			case 0x01 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x01 | (0b101 << 8): cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SLO_IZX
			case 0x03 | (0b001 << 8): 
			case 0x03 | (0b010 << 8):
			case 0x03 | (0b011 << 8):
			case 0x03 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x03 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x03 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->B); break;
			case 0x03 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ORA_ZP
			case 0x05 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x05 | (0b010 << 8): cpu->A |= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL_ZP
			case 0x06 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x06 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x06 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x06 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SLO_ZP
			case 0x07 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x07 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x07 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; break;
			case 0x07 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// PHP
			case 0x08 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x08 | (0b010 << 8): push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00110000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); END(cpu); break;

			// ORA_IMM
			case 0x09 | (0b001 << 8): cpu->A |= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL
			case 0x0A | (0b001 << 8): fetch(cpu); cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1; nzFlags(cpu, cpu->A); END(cpu); break;

			// ANC_IMM TODO 2B IS ALSO ANC_IMM
			case 0x0B | (0b001 << 8): cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b10000000; nzFlags(cpu, cpu->A); END(cpu); break;

			// ORA_ABS
			case 0x0D | (0b001 << 8):
			case 0x0D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0D | (0b011 << 8): cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL_ABS
			case 0x0E | (0b001 << 8):
			case 0x0E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0E | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x0E | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x0E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SLO_ABS
			case 0x0F | (0b001 << 8):
			case 0x0F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0F | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x0F | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x0F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BPL
			case 0x10 | (0b001 << 8):
			case 0x10 | (0b010 << 8): branch(cpu, !cpu->negFlag); break;
			case 0x10 | (0b011 << 8): branch(cpu, !cpu->negFlag); END(cpu); break;

			// ORA_IZY
			case 0x11 | (0b001 << 8):
			case 0x11 | (0b010 << 8):
			case 0x11 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x11 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x11 | (0b101 << 8): cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SLO_IZY
			case 0x13 | (0b001 << 8):
			case 0x13 | (0b010 << 8):
			case 0x13 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x13 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x13 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x13 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu); break;

			// ORA_ZPX
			case 0x15 | (0b001 << 8):
			case 0x15 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x15 | (0b011 << 8): cpu->A |= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL_ZPX
			case 0x16 | (0b001 << 8):
			case 0x16 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x16 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x16 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x16 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SLO_ZPX
			case 0x17 | (0b001 << 8):
			case 0x17 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x17 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x17 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x17 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// CLC
			case 0x18 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->carryFlag = false; END(cpu); break;

			// ORA_ABY
			case 0x19 | (0b001 << 8):
			case 0x19 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x19 | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x19 | (0b100 << 8): cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SLO_ABY
			case 0x1B | (0b001 << 8):
			case 0x1B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x1B | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x1B | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x1B | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x1B | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ORA_ABX
			case 0x1D | (0b001 << 8):
			case 0x1D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1D | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x1D | (0b100 << 8): cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->B); END(cpu); break;

			// ASL_ABX
			case 0x1E | (0b001 << 8):
			case 0x1E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1E | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x1E | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x1E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x1E | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SLO_ABX
			case 0x1F | (0b001 << 8):
			case 0x1F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1F | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x1F | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x1F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x1F | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// JSR
			case 0x20 | (0b001 << 8): cpu->temp = fetch(cpu); break;
			case 0x20 | (0b010 << 8): read(0x0100 | cpu->SP); break;
			case 0x20 | (0b011 << 8): push(cpu, cpu->PCH); break;
			case 0x20 | (0b100 << 8): push(cpu, cpu->PCL); break;
			case 0x20 | (0b101 << 8): cpu->PCH = fetch(cpu); cpu->PCL = cpu->temp; END(cpu); break;

			// AND_IZX
			case 0x21 | (0b001 << 8):
			case 0x21 | (0b010 << 8):
			case 0x21 | (0b011 << 8):
			case 0x21 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x21 | (0b101 << 8): cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// RLA_IZX
			case 0x23 | (0b001 << 8):
			case 0x23 | (0b010 << 8):
			case 0x23 | (0b011 << 8):
			case 0x23 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x23 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x23 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x23 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BIT_ZP
			case 0x24 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x24 | (0b010 << 8): {uint8_t value = read(0x0000 | cpu->DPL); cpu->oflowFlag = value & 0b01000000; cpu->negFlag = value & 0b10000000; cpu->zeroFlag = !(value & cpu->A);} END(cpu); break;

			// AND_ZP
			case 0x25 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x25 | (0b010 << 8): cpu->A &= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ZP
			case 0x26 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x26 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x26 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x26 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RLA_ZP
			case 0x27 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x27 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x27 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x27 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// PLP
			case 0x28 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x28 | (0b010 << 8): pull(cpu); break;
			case 0x28 | (0b011 << 8): {uint8_t flags = read(0x0100 | cpu->SP); cpu->negFlag = flags & 0b10000000; cpu->oflowFlag = flags & 0b01000000; cpu->decFlag = flags & 0b00001000; cpu->noIRQFlag = flags & 0b00000100; cpu->zeroFlag = flags & 0b00000010; cpu->carryFlag = flags & 0b00000001;} END(cpu); break;

			// AND_IMM
			case 0x29 | (0b001 << 8): cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL
			case 0x2A | (0b001 << 8): read(PROGCOUNTER(cpu)); if (cpu->carryFlag) {cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1; cpu->A++;} else {cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1;} nzFlags(cpu, cpu->A); END(cpu); break;

			// ANC_IMM TODO 0B IS ALSO ANC_IMM
			case 0x2B | (0b001 << 8): cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b10000000; nzFlags(cpu, cpu->A); END(cpu); break;

			// BIT_ABS
			case 0x2C | (0b001 << 8):
			case 0x2C | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2C | (0b011 << 8): {uint8_t value = read(DATAPTR(cpu)); cpu->oflowFlag = value & 0b01000000; cpu->negFlag = value & 0b10000000; cpu->zeroFlag = !(value & cpu->A);} END(cpu); break;

			// AND_ABS
			case 0x2D | (0b001 << 8):
			case 0x2D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2D | (0b011 << 8): cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ABS
			case 0x2E | (0b001 << 8):
			case 0x2E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2E | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x2E | (0b100 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x2E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RLA_ABS
			case 0x2F | (0b001 << 8):
			case 0x2F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2F | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x2F | (0b100 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x2F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BMI
			case 0x30 | (0b001 << 8):
			case 0x30 | (0b010 << 8): branch(cpu, cpu->negFlag); break;
			case 0x30 | (0b011 << 8): branch(cpu, cpu->negFlag); END(cpu); break;

			// AND_IZY
			case 0x31 | (0b001 << 8):
			case 0x31 | (0b010 << 8):
			case 0x31 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x31 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x31 | (0b101 << 8): cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// RLA_IZY
			case 0x33 | (0b001 << 8):
			case 0x33 | (0b010 << 8):
			case 0x33 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x33 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x33 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x33 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x33 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// AND_ZPX
			case 0x35 | (0b001 << 8):
			case 0x35 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x35 | (0b011 << 8): cpu->A &= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ZPX
			case 0x36 | (0b001 << 8):
			case 0x36 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x36 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x36 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RLA_ZPX
			case 0x37 | (0b001 << 8):
			case 0x37 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x37 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x37 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x37 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SEC
			case 0x38 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->carryFlag = true; END(cpu); break;

			// AND_ABY
			case 0x39 | (0b001 << 8):
			case 0x39 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x39 | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x39 | (0b100 << 8): cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// RLA_ABY
			case 0x3B | (0b001 << 8):
			case 0x3B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x3B | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x3B | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x3B | (0b101 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x3B | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// AND_ABX
			case 0x3D | (0b001 << 8):
			case 0x3D | (0b010 << 8): absAddressing(cpu, cpu->X);
			case 0x3D | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x3D | (0b100 << 8): cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ABX
			case 0x3E | (0b001 << 8):
			case 0x3E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x3E | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x3E | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x3E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x3E | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RLA_ABX
			case 0x3F | (0b001 << 8):
			case 0x3F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x3F | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x3F | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x3F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x3F | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RTI
			case 0x40 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x40 | (0b010 << 8): pull(cpu); break;
			case 0x40 | (0b011 << 8): {uint8_t flags = pull(cpu); cpu->negFlag = flags & 0b10000000; cpu->oflowFlag = flags & 0b01000000; cpu->decFlag = flags & 0b00001000; cpu->noIRQFlag = flags & 0b00000100; cpu->zeroFlag = flags & 0b00000010; cpu->carryFlag = flags & 0b00000001;} break;
			case 0x40 | (0b100 << 8): cpu->PCL = pull(cpu); break;
			case 0x40 | (0b101 << 8): cpu->PCH = read(0x0100 | cpu->SP); END(cpu); break;

			// EOR_IZX
			case 0x41 | (0b001 << 8):
			case 0x41 | (0b010 << 8):
			case 0x41 | (0b011 << 8):
			case 0x41 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x41 | (0b101 << 8): cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SRE_IZX
			case 0x43 | (0b001 << 8):
			case 0x43 | (0b010 << 8):
			case 0x43 | (0b011 << 8):
			case 0x43 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x43 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x43 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x43 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// EOR_ZP
			case 0x45 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x45 | (0b010 << 8): cpu->A ^= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR_ZP
			case 0x46 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x46 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x46 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x46 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SRE_ZP
			case 0x47 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x47 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x47 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x47 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// PHA
			case 0x48 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x48 | (0b010 << 8): push(cpu, cpu->A); END(cpu); break;

			// EOR_IMM
			case 0x49 | (0b001 << 8): cpu->A ^= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR
			case 0x4A | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; nzFlags(cpu, cpu->A); END(cpu); break;

			// ALR_IMM
			case 0x4B | (0b001 << 8): cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; nzFlags(cpu, cpu->A); END(cpu); break;

			// JMP_ABS
			case 0x4C | (0b001 << 8): cpu->temp = fetch(cpu); break;
			case 0x4C | (0b010 << 8): cpu->PCH = fetch(cpu); cpu->PCL = cpu->temp; END(cpu); break;

			// EOR_ABS
			case 0x4D | (0b001 << 8):
			case 0x4D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x4D | (0b011 << 8): cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR_ABS
			case 0x4E | (0b001 << 8):
			case 0x4E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x4E | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x4E | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x4E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SRE_ABS
			case 0x4F | (0b001 << 8):
			case 0x4F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x4F | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x4F | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x4F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BVC
			case 0x50 | (0b001 << 8):
			case 0x50 | (0b010 << 8): branch(cpu, !cpu->oflowFlag); break;
			case 0x50 | (0b011 << 8): branch(cpu, !cpu->oflowFlag); END(cpu); break;

			// EOR_IZY
			case 0x51 | (0b001 << 8):
			case 0x51 | (0b010 << 8):
			case 0x51 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x51 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x51 | (0b101 << 8): cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SRE_IZY
			case 0x53 | (0b001 << 8):
			case 0x53 | (0b010 << 8):
			case 0x53 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x53 | (0b100 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x53 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x53 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x53 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// EOR_ZPX
			case 0x55 | (0b001 << 8):
			case 0x55 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x55 | (0b011 << 8): cpu->A ^= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR_ZPX
			case 0x56 | (0b001 << 8):
			case 0x56 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x56 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x56 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x56 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SRE_ZPX
			case 0x57 | (0b001 << 8):
			case 0x57 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x57 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x57 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x57 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// CLI
			case 0x58 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->noIRQFlag = false; END(cpu); break;

			// EOR_ABY
			case 0x59 | (0b001 << 8):
			case 0x59 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x59 | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x59 | (0b100 << 8): cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SRE_ABY
			case 0x5B | (0b001 << 8):
			case 0x5B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x5B | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x5B | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x5B | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x5B | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// EOR_ABX
			case 0x5D | (0b001 << 8):
			case 0x5D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x5D | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0x5D | (0b100 << 8): cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->B); END(cpu); break;

			// LSR_ABX
			case 0x5E | (0b001 << 8):
			case 0x5E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x5E | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x5E | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x5E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x5E | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SRE_ABX
			case 0x5F | (0b001 << 8):
			case 0x5F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x5F | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x5F | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x5F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x5F | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RTS
			case 0x60 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x60 | (0b010 << 8): pull(cpu); break;
			case 0x60 | (0b011 << 8): cpu->PCL = pull(cpu); break;
			case 0x60 | (0b100 << 8): cpu->PCH = read(0x0100 | cpu->SP); break;
			case 0x60 | (0b101 << 8): fetch(cpu); END(cpu); break;

			// ADC_IZX
			case 0x61 | (0b001 << 8):
			case 0x61 | (0b010 << 8):
			case 0x61 | (0b011 << 8):
			case 0x61 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x61 | (0b101 << 8): add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// RRA_IZX
			case 0x63 | (0b001 << 8):
			case 0x63 | (0b010 << 8):
			case 0x63 | (0b011 << 8):
			case 0x63 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x63 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x63 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x63 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ADC_ZP
			case 0x65 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x65 | (0b010 << 8): add(cpu, read(0x0000 | cpu->DPL)); END(cpu); break;

			// ROR_ZP
			case 0x66 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x66 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x66 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x66 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RRA_ZP
			case 0x67 | (0b000 << 8): cpu->DPL = fetch(cpu); break;
			case 0x67 | (0b001 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x67 | (0b010 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x67 | (0b011 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// PLA
			case 0x68 | (0b001 << 8): read(PROGCOUNTER(cpu)); break;
			case 0x68 | (0b010 << 8): pull(cpu); break;
			case 0x68 | (0b011 << 8): cpu->A = pull(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// ADC_IMM
			case 0x69 | (0b001 << 8): add(cpu, fetch(cpu)); END(cpu); break;

			// ROR
			case 0x6A | (0b001 << 8): read(PROGCOUNTER(cpu)); if (cpu->carryFlag) {cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; cpu->A |= 0b10000000;} else {cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1;} END(cpu); break;

			// ARR_IMM
			// TODO some sources say the V flag is set from the XOR of bit 6 and 7 of A, not bit 5 and 6
			case 0x6B | (0b001 << 8): cpu->B = fetch(cpu); cpu->A &= cpu->B; cpu->A >>= 1; if (cpu->carryFlag) cpu->A |= 0b10000000; nzFlags(cpu, cpu->A); cpu->carryFlag = cpu->A & 0b01000000; cpu->oflowFlag = (cpu->A & 0b01000000) ^ (cpu->A & 0b00100000); END(cpu); break;

			// JMP_IND
			case 0x6C | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x6C | (0b010 << 8): cpu->DPH = fetch(cpu); break;
			case 0x6C | (0b011 << 8): cpu->temp = read(DATAPTR(cpu)); cpu->DPL++; break;
			case 0x6C | (0b100 << 8): cpu->PCH = read(DATAPTR(cpu)); cpu->PCL = cpu->temp; END(cpu); break;

			// ADC_ABS
			case 0x6D | (0b001 << 8):
			case 0x6D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x6D | (0b011 << 8): add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// ROR_ABS
			case 0x6E | (0b001 << 8):
			case 0x6E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x6E | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x6E | (0b100 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x6E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RRA_ABS
			case 0x6F | (0b001 << 8):
			case 0x6F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x6F | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x6F | (0b100 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x6F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BVS
			case 0x70 | (0b001 << 8):
			case 0x70 | (0b010 << 8): branch(cpu, cpu->oflowFlag); break;
			case 0x70 | (0b011 << 8): branch(cpu, cpu->oflowFlag); END(cpu); break;

			// ADC_IZY
			case 0x71 | (0b001 << 8):
			case 0x71 | (0b010 << 8):
			case 0x71 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x71 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, cpu->B); END(cpu);} break;
			case 0x71 | (0b101 << 8): add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// RRA_IZY
			case 0x73 | (0b001 << 8):
			case 0x73 | (0b010 << 8):
			case 0x73 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x73 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x73 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x73 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x73 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ADC_ZPX
			case 0x75 | (0b001 << 8):
			case 0x75 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x75 | (0b011 << 8): add(cpu, read(0x0000 | cpu->DPL)); END(cpu); break;

			// ROR_ZPX
			case 0x76 | (0b001 << 8):
			case 0x76 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x76 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x76 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->X); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x76 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RRA_ZPX
			case 0x77 | (0b001 << 8):
			case 0x77 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x77 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0x77 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->X); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x77 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SEI
			case 0x78 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->noIRQFlag = true; END(cpu); break;

			// ADC_ABY
			case 0x79 | (0b001 << 8):
			case 0x79 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x79 | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, cpu->B); END(cpu);} break;
			case 0x79 | (0b100 << 8): add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// RRA_ABY
			case 0x7B | (0b001 << 8):
			case 0x7B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x7B | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x7B | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x7B | (0b101 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x7B | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ADC_ABX
			case 0x7D | (0b001 << 8):
			case 0x7D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x7D | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {add(cpu, cpu->B); END(cpu);} break;
			case 0x7D | (0b100 << 8): add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// ROR_ABX
			case 0x7E | (0b001 << 8):
			case 0x7E | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x7E | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x7E | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x7E | (0b101 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x7E | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RRA_ABX
			case 0x7F | (0b001 << 8):
			case 0x7F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x7F | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x7F | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0x7F | (0b101 << 8): write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x7F | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// STA_IZX
			case 0x81 | (0b001 << 8):
			case 0x81 | (0b010 << 8):
			case 0x81 | (0b011 << 8):
			case 0x81 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x81 | (0b101 << 8): write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// SAX_IZX
			case 0x83 | (0b001 << 8):
			case 0x83 | (0b010 << 8):
			case 0x83 | (0b011 << 8):
			case 0x83 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x83 | (0b101 << 8): write(DATAPTR(cpu), cpu->A & cpu->X); END(cpu); break;

			// STY_ZP
			case 0x84 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x84 | (0b010 << 8): write(0x0000 | cpu->DPL, cpu->Y); END(cpu); break;

			// STY_ZP
			case 0x85 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x85 | (0b010 << 8): write(0x0000 | cpu->DPL, cpu->A); END(cpu); break;

			// STY_ZP
			case 0x86 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x86 | (0b010 << 8): write(0x0000 | cpu->DPL, cpu->X); END(cpu); break;

			// STY_ZP
			case 0x87 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x87 | (0b010 << 8): write(0x0000 | cpu->DPL, cpu->A & cpu->X); END(cpu); break;

			// DEY
			case 0x88 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->Y--; nzFlags(cpu, cpu->Y); END(cpu); break;
			
			// TXA
			case 0x8A | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->A); END(cpu); break;

			// XAA_IMM
			// TODO sources contradict each other regarding this
			case 0x8B | (0b001 << 8): cpu->A |= 0xEE; cpu->A &= cpu->X; cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// STY_ABS
			case 0x8C | (0b001 << 8):
			case 0x8C | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8C | (0b011 << 8): write(DATAPTR(cpu), cpu->Y); END(cpu); break;

			// STA_ABS
			case 0x8D | (0b001 << 8):
			case 0x8D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8D | (0b011 << 8): write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// STX_ABS
			case 0x8E | (0b001 << 8):
			case 0x8E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8E | (0b011 << 8): write(DATAPTR(cpu), cpu->X); END(cpu); break;

			// SAX_ABS
			case 0x8F | (0b001 << 8):
			case 0x8F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8F | (0b011 << 8): write(DATAPTR(cpu), cpu->A & cpu->X); END(cpu); break;

			// BCC
			case 0x90 | (0b001 << 8):
			case 0x90 | (0b010 << 8): branch(cpu, !cpu->carryFlag); break;
			case 0x90 | (0b011 << 8): branch(cpu, !cpu->carryFlag); END(cpu); break;

			// STA_IZY
			case 0x91 | (0b001 << 8):
			case 0x91 | (0b010 << 8):
			case 0x91 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x91 | (0b100 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x91 | (0b101 << 8): write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// AHX_IZY
			case 0x93 | (0b001 << 8):
			case 0x93 | (0b010 << 8):
			case 0x93 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x93 | (0b100 << 8): read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x93 | (0b101 << 8): write(DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); END(cpu); break;

			// STY_ZPX
			case 0x94 | (0b001 << 8):
			case 0x94 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x94 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->Y); END(cpu); break;

			// STA_ZPX
			case 0x95 | (0b001 << 8):
			case 0x95 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x95 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->A); END(cpu); break;

			// STX_ZPY
			case 0x96 | (0b001 << 8):
			case 0x96 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0x96 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->X); END(cpu); break;

			// SAX_ZPY
			case 0x97 | (0b001 << 8):
			case 0x97 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0x97 | (0b011 << 8): write(0x0000 | cpu->DPL, cpu->A & cpu->X); END(cpu); break;

			// TYA
			case 0x98 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->A = cpu->Y; nzFlags(cpu, cpu->A); END(cpu); break;

			// STA_ABY
			case 0x99 | (0b001 << 8):
			case 0x99 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x99 | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x99 | (0b100 << 8): write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// TXS
			case 0x9A | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->SP = cpu->X; END(cpu); break;

			// TAS_ABY
			case 0x9B | (0b001 << 8):
			case 0x9B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x9B | (0b011 << 8): read(DATAPTR(cpu)); cpu->SP = cpu->A & cpu->X; cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x9B | (0b100 << 8): write(DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); END(cpu); break;

			// SHY_ABX
			case 0x9C | (0b001 << 8):
			case 0x9C | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x9C | (0b011 << 8): read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x9C | (0b100 << 8): write(DATAPTR(cpu), cpu->X & cpu->temp); END(cpu); break;

			// STA_ABX
			case 0x9D | (0b001 << 8):
			case 0x9D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x9D | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x9D | (0b100 << 8): write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// SHX_ABY
			case 0x9E | (0b001 << 8):
			case 0x9E | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x9E | (0b011 << 8): read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x9E | (0b100 << 8): write(DATAPTR(cpu), cpu->X & cpu->temp); END(cpu); break;

			// AHX_ABY
			case 0x9F | (0b001 << 8):
			case 0x9F | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x9F | (0b011 << 8): read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x9F | (0b100 << 8): write(DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); END(cpu); break;

			// LDY_IMM
			case 0xA0 | (0b001 << 8): cpu->Y = fetch(cpu); nzFlags(cpu, cpu->Y); END(cpu); break;

			// LDA_IZX
			case 0xA1 | (0b001 << 8):
			case 0xA1 | (0b010 << 8):
			case 0xA1 | (0b011 << 8):
			case 0xA1 | (0b100 << 8): izxAddressing(cpu);
			case 0xA1 | (0b101 << 8): cpu->A = read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;
			
			// LDX_IMM
			case 0xA2 | (0b001 << 8): cpu->X = fetch(cpu); nzFlags(cpu, cpu->X); END(cpu); break;

			// LAX_IZX
			case 0xA3 | (0b001 << 8):
			case 0xA3 | (0b010 << 8):
			case 0xA3 | (0b011 << 8):
			case 0xA3 | (0b100 << 8): izxAddressing(cpu);
			case 0xA3 | (0b101 << 8): cpu->X = read(DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->A); END(cpu); break;

			// LDY_ZP
			case 0xA4 | (0b001 << 8): cpu->DPL = fetch(cpu);
			case 0xA4 | (0b010 << 8): cpu->Y = read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->Y); END(cpu); break;

			// LDA_ZP
			case 0xA5 | (0b001 << 8): cpu->DPL = fetch(cpu);
			case 0xA5 | (0b010 << 8): cpu->A = read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// LDX_ZP
			case 0xA6 | (0b001 << 8): cpu->DPL = fetch(cpu);
			case 0xA6 | (0b010 << 8): cpu->X = read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->X); END(cpu); break;

			// LAX_ZP
			case 0xA7 | (0b001 << 8): cpu->DPL = fetch(cpu);
			case 0xA7 | (0b010 << 8): cpu->X = read(0x0000 | cpu->DPL); cpu->A = cpu->X; nzFlags(cpu, cpu->A); END(cpu); break;

			// TAY
			case 0xA8 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->Y = cpu->A; nzFlags(cpu, cpu->Y); END(cpu); break;

			// LDA_IMM
			case 0xA9 | (0b001 << 8): cpu->A = fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// TAX
			case 0xAA | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->X = cpu->A; nzFlags(cpu, cpu->X); END(cpu); break;

			// LAX_IMM
			// TODO not even all sources mention the immediate mode of this operation, and it is said to be unstable
			case 0xAB | (0b001 << 8): cpu->A &= fetch(cpu); cpu->X = cpu->A; nzFlags(cpu, cpu->X); END(cpu); break;

			// LDY_ABS
			case 0xAC | (0b001 << 8):
			case 0xAC | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAC | (0b011 << 8): cpu->Y = read(DATAPTR(cpu)); nzFlags(cpu, cpu->Y); END(cpu); break;

			// LDA_ABS
			case 0xAD | (0b001 << 8):
			case 0xAD | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAD | (0b011 << 8): cpu->A = read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// LDX_ABS
			case 0xAE | (0b001 << 8):
			case 0xAE | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAE | (0b011 << 8): cpu->X = read(DATAPTR(cpu)); nzFlags(cpu, cpu->X); END(cpu); break;

			// LAX_ABS
			case 0xAF | (0b001 << 8):
			case 0xAF | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAF | (0b011 << 8): cpu->X = read(DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->X); END(cpu); break;

			// BCS
			case 0xB0 | (0b001 << 8):
			case 0xB0 | (0b010 << 8): branch(cpu, cpu->carryFlag); break;
			case 0xB0 | (0b011 << 8): branch(cpu, cpu->carryFlag); END(cpu); break;

			// LDA_IZY
			case 0xB1 | (0b001 << 8):
			case 0xB1 | (0b010 << 8):
			case 0xB1 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xB1 | (0b100 << 8): cpu->A = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0xB1 | (0b101 << 8): cpu->A = read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); break;

			// LAX_IZY
			case 0xB3 | (0b001 << 8):
			case 0xB3 | (0b010 << 8):
			case 0xB3 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xB3 | (0b100 << 8): cpu->X = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A = cpu->X; nzFlags(cpu, cpu->X); END(cpu);} break;
			case 0xB3 | (0b101 << 8): cpu->A = cpu->X; nzFlags(cpu, cpu->X); END(cpu); break;
			
			// LDY_ZPX
			case 0xB4 | (0b001 << 8):
			case 0xB4 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xB4 | (0b011 << 8): cpu->Y = read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->Y); END(cpu); break;

			// LDA_ZPX
			case 0xB5 | (0b001 << 8):
			case 0xB5 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xB5 | (0b011 << 8): cpu->A = read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// LDX_ZPY
			case 0xB6 | (0b001 << 8):
			case 0xB6 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0xB6 | (0b011 << 8): cpu->X = read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->X); END(cpu); break;

			// LAX_ZPY
			case 0xB7 | (0b001 << 8):
			case 0xB7 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0xB7 | (0b011 << 8): cpu->X = read(0x0000 | cpu->DPL); cpu->A = cpu->X; nzFlags(cpu, cpu->X); END(cpu); break;

			// CLV
			case 0xB8 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->oflowFlag = false; END(cpu); break;

			// LDA_ABY
			case 0xB9 | (0b001 << 8):
			case 0xB9 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xB9 | (0b011 << 8): cpu->A = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0xB9 | (0b100 << 8): cpu->A = read(DATAPTR(cpu)); END(cpu); break;

			// TSX
			case 0xBA | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->X = cpu->SP; END(cpu); break;

			// LAS_ABY
			case 0xBB | (0b001 << 8):
			case 0xBB | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xBB | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->SP &= cpu->B; cpu->A = cpu->SP; cpu->X = cpu->SP; nzFlags(cpu, cpu->SP); END(cpu);} break;
			case 0xBB | (0b100 << 8): cpu->SP &= read(DATAPTR(cpu)); cpu->A = cpu->SP; cpu->X = cpu->SP; nzFlags(cpu, cpu->SP); END(cpu); break;

			// LDY_ABX
			case 0xBC | (0b001 << 8):
			case 0xBC | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xBC | (0b011 << 8): cpu->Y = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {nzFlags(cpu, cpu->Y); END(cpu);} break;
			case 0xBC | (0b100 << 8): cpu->Y = read(DATAPTR(cpu)); nzFlags(cpu, cpu->Y); END(cpu); break;

			// LDA_ABX
			case 0xBD | (0b001 << 8):
			case 0xBD | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xBD | (0b011 << 8): cpu->A = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {nzFlags(cpu, cpu->A); END(cpu);} break;
			case 0xBD | (0b100 << 8): cpu->A = read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// LDX_ABY
			case 0xBE | (0b001 << 8):
			case 0xBE | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xBE | (0b011 << 8): cpu->X = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {nzFlags(cpu, cpu->X); END(cpu);} break;
			case 0xBE | (0b100 << 8): cpu->X = read(DATAPTR(cpu)); nzFlags(cpu, cpu->X); END(cpu); break;

			// LAX_ABY
			case 0xBF | (0b001 << 8):
			case 0xBF | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xBF | (0b011 << 8): cpu->X = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A = cpu->X; nzFlags(cpu, cpu->Y); END(cpu);} break;
			case 0xBF | (0b100 << 8): cpu->X = read(DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->X); END(cpu); break;

			// CPY_IMM
			case 0xC0 | (0b001 << 8): cpu->B = fetch(cpu); cpu->carryFlag = cpu->Y >= cpu->B; nzFlags(cpu, cpu->Y - cpu->B); END(cpu); break;
			
			// CMP_IZX
			case 0xC1 | (0b001 << 8):
			case 0xC1 | (0b010 << 8):
			case 0xC1 | (0b011 << 8):
			case 0xC1 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xC1 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;

			// DCP_IZX
			// TODO sources contradict each other on which flags to set. However, the standard CMP flags seem a likely behaviour
			case 0xC3 | (0b001 << 8):
			case 0xC3 | (0b010 << 8):
			case 0xC3 | (0b011 << 8):
			case 0xC3 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xC3 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xC3 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xC3 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// CPY_ZP
			case 0xC4 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC4 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); cpu->carryFlag = cpu->Y >= cpu->B; nzFlags(cpu, cpu->Y - cpu->B); END(cpu); break;
			
			// CMP_ZP
			case 0xC5 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC5 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;
			
			// DEC_ZP
			case 0xC6 | (0b000 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC6 | (0b001 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xC6 | (0b010 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xC6 | (0b011 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// DCP_ZP
			case 0xC7 | (0b000 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC7 | (0b001 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xC7 | (0b010 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xC7 | (0b011 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// INY
			case 0xC8 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->Y++; nzFlags(cpu, cpu->Y); END(cpu); break;

			// CMP_IMM
			case 0xC9 | (0b001 << 8): cpu->B = fetch(cpu); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;
			
			// DEX
			case 0xCA | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->X--; nzFlags(cpu, cpu->X); END(cpu); break;

			// AXS_IMM
			case 0xCB | (0b001 << 8): cpu->B = fetch(cpu); cpu->X &= cpu->A; cpu->carryFlag = cpu->X >= cpu->B; cpu->X -= cpu->B; nzFlags(cpu, cpu->X); END(cpu); break;

			// CPY_ABS
			case 0xCC | (0b001 << 8):
			case 0xCC | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCC | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->Y >= cpu->B; nzFlags(cpu, cpu->Y - cpu->B); END(cpu); break;

			// CMP_ABS
			case 0xCD | (0b001 << 8):
			case 0xCD | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCD | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;

			// DEC_ABS
			case 0xCE | (0b001 << 8):
			case 0xCE | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCE | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xCE | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xCE | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// DCP_ABS
			case 0xCF | (0b001 << 8):
			case 0xCF | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCF | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xCF | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xCF | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BNE
			case 0xD0 | (0b001 << 8):
			case 0xD0 | (0b010 << 8): branch(cpu, !cpu->zeroFlag); break;
			case 0xD0 | (0b011 << 8): branch(cpu, !cpu->zeroFlag); END(cpu); break;

			// CMP_IZY
			case 0xD1 | (0b001 << 8):
			case 0xD1 | (0b010 << 8):
			case 0xD1 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xD1 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu);} break;
			case 0xD1 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;

			// DCP_IZY
			case 0xD3 | (0b001 << 8):
			case 0xD3 | (0b010 << 8):
			case 0xD3 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xD3 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xD3 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xD3 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xD3 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// CMP_ZPX
			case 0xD5 | (0b001 << 8):
			case 0xD5 | (0b010 << 8): zpiAddressing(cpu, cpu->X);
			case 0xD5 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;

			// DEC_ZPX
			case 0xD6 | (0b001 << 8):
			case 0xD6 | (0b010 << 8): zpiAddressing(cpu, cpu->X);
			case 0xD6 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xD6 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xD6 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// DCP_ZPX
			case 0xD7 | (0b001 << 8):
			case 0xD7 | (0b010 << 8): zpiAddressing(cpu, cpu->X);
			case 0xD7 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xD7 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xD7 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// CLD
			case 0xD8 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->decFlag = false; END(cpu); break;

			// CMP_ABY
			case 0xD9 | (0b001 << 8):
			case 0xD9 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xD9 | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu);} break;
			case 0xD9 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;

			// DCP_ABY
			case 0xDB | (0b001 << 8):
			case 0xDB | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xDB | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xDB | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xDB | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xDB | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;
			
			// CMP_ABX
			case 0xDD | (0b001 << 8):
			case 0xDD | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xDD | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu);} break;
			case 0xDD | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); END(cpu); break;

			// DEC_ABX
			case 0xDE | (0b001 << 8):
			case 0xDE | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xDE | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xDE | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xDE | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xDE | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// DCP_ABX
			case 0xDF | (0b001 << 8):
			case 0xDF | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xDF | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xDF | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xDF | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xDF | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// CPX_IMM
			case 0xE0 | (0b001 << 8): cpu->B = fetch(cpu); cpu->carryFlag = cpu->X >= cpu->B; nzFlags(cpu, cpu->X - cpu->B); END(cpu); break;

			// SBC_IZX
			case 0xE1 | (0b001 << 8):
			case 0xE1 | (0b010 << 8):
			case 0xE1 | (0b011 << 8):
			case 0xE1 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xE1 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xE1 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); add(cpu, ~cpu->B); break;
			case 0xE1 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ISC_IZX
			case 0xE3 | (0b001 << 8):
			case 0xE3 | (0b010 << 8):
			case 0xE3 | (0b011 << 8):
			case 0xE3 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xE3 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xE3 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xE3 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// CPX_ZP
			case 0xE4 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE4 | (0b010 << 8): cpu->B = read(0x0000 | cpu->DPL); cpu->carryFlag = cpu->X >= cpu->B; nzFlags(cpu, cpu->X - cpu->B); END(cpu); break;
			
			// SBC_ZP
			case 0xE5 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE5 | (0b010 << 8): add(cpu, ~(read(0x0000 | cpu->DPL))); END(cpu); break;

			// INC_ZP
			case 0xE6 | (0b000 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE6 | (0b001 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xE6 | (0b010 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xE6 | (0b011 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ISC_ZP
			case 0xE7 | (0b000 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE7 | (0b001 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xE7 | (0b010 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xE7 | (0b011 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// INX
			case 0xE8 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->X++; nzFlags(cpu, cpu->X); END(cpu); break;

			// SBC_IMM
			case 0xE9 | (0b001 << 8):

			case 0xEB | (0b001 << 8): add(cpu, ~(fetch(cpu))); END(cpu); break;

			// CPX_ABS
			case 0xEC | (0b001 << 8):
			case 0xEC | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xEC | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); cpu->carryFlag = cpu->X >= cpu->B; nzFlags(cpu, cpu->X - cpu->B); END(cpu); break;
			
			// SBC_ABS
			case 0xED | (0b001 << 8):
			case 0xED | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xED | (0b011 << 8): add(cpu, ~(read(DATAPTR(cpu)))); END(cpu); break;

			// INC_ABS
			case 0xEE | (0b001 << 8):
			case 0xEE | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xEE | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xEE | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xEE | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ISC_ABS
			case 0xEF | (0b001 << 8):
			case 0xEF | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xEF | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xEF | (0b100 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xEF | (0b101 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BEQ
			case 0xF0 | (0b001 << 8):
			case 0xF0 | (0b010 << 8): branch(cpu, cpu->zeroFlag); break;
			case 0xF0 | (0b011 << 8): branch(cpu, cpu->zeroFlag); END(cpu); break;

			// SBC_IZY
			case 0xF1 | (0b001 << 8):
			case 0xF1 | (0b010 << 8):
			case 0xF1 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xF1 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, ~cpu->B); END(cpu);} break;
			case 0xF1 | (0b101 << 8): add(cpu, ~(read(DATAPTR(cpu)))); END(cpu); break;

			// ISC_IZY
			case 0xF3 | (0b001 << 8):
			case 0xF3 | (0b010 << 8):
			case 0xF3 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xF3 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xF3 | (0b101 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xF3 | (0b110 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xF3 | (0b111 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;
			
			// SBC_ZPX
			case 0xF5 | (0b001 << 8):
			case 0xF5 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xF5 | (0b011 << 8): add(cpu, ~(read(0x0000 | cpu->DPL))); END(cpu); break;

			// INC_ZPX
			case 0xF6 | (0b001 << 8):
			case 0xF6 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xF6 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xF6 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xF6 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// ISC_ZPX
			case 0xF7 | (0b001 << 8):
			case 0xF7 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xF7 | (0b011 << 8): cpu->B = read(0x0000 | cpu->DPL); break;
			case 0xF7 | (0b100 << 8): write(0x0000 | cpu->DPL, cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xF7 | (0b101 << 8): write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SED
			case 0xF8 | (0b001 << 8): read(PROGCOUNTER(cpu)); cpu->decFlag = true; END(cpu); break;

			// SBC_ABY
			case 0xF9 | (0b001 << 8):
			case 0xF9 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xF9 | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, ~cpu->B); END(cpu);} break;
			case 0xF9 | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); add(cpu, ~cpu->B); END(cpu); break;

			// ISC_ABY
			case 0xFB | (0b001 << 8):
			case 0xFB | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xFB | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xFB | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xFB | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xFB | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SBC_ABX
			case 0xFD | (0b001 << 8):
			case 0xFD | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xFD | (0b011 << 8): cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {add(cpu, ~cpu->B); END(cpu);} break;
			case 0xFD | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); add(cpu, ~cpu->B); END(cpu); break;

			// INC_ABX
			case 0xFE | (0b001 << 8):
			case 0xFE | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xFE | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xFE | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xFE | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xFE | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ISC_ABX
			case 0xFF | (0b001 << 8):
			case 0xFF | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xFF | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xFF | (0b100 << 8): cpu->B = read(DATAPTR(cpu)); break;
			case 0xFF | (0b101 << 8): write(DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xFF | (0b110 << 8): write(DATAPTR(cpu), cpu->B); END(cpu); break;


			// NOP
			case 0x1A | (0b001 << 8):
			case 0x3A | (0b001 << 8):
			case 0x5A | (0b001 << 8):
			case 0x7A | (0b001 << 8):
			case 0xDA | (0b001 << 8):
			case 0xEA | (0b001 << 8):
			case 0xFA | (0b001 << 8): read(PROGCOUNTER(cpu)); END(cpu); break;

			// NOP_IMM
			case 0x80 | (0b001 << 8):
			case 0x89 | (0b001 << 8): fetch(cpu); END(cpu); break;

			// NOP_ZP
			case 0x04 | (0b001 << 8):
			case 0x44 | (0b001 << 8):
			case 0x64 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x04 | (0b010 << 8):
			case 0x44 | (0b010 << 8):
			case 0x64 | (0b010 << 8): read(0x0000 | cpu->DPL); END(cpu); break;

			// NOP_ZPX
			case 0x14 | (0b001 << 8):
			case 0x34 | (0b001 << 8):
			case 0x54 | (0b001 << 8):
			case 0x74 | (0b001 << 8):
			case 0xD4 | (0b001 << 8):
			case 0xF4 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x14 | (0b010 << 8):
			case 0x34 | (0b010 << 8):
			case 0x54 | (0b010 << 8):
			case 0x74 | (0b010 << 8):
			case 0xD4 | (0b010 << 8):
			case 0xF4 | (0b010 << 8): read(0x0000 | cpu->DPL); cpu->DPL += cpu->X; break;
			case 0x14 | (0b011 << 8):
			case 0x34 | (0b011 << 8):
			case 0x54 | (0b011 << 8):
			case 0x74 | (0b011 << 8):
			case 0xD4 | (0b011 << 8):
			case 0xF4 | (0b011 << 8): read(0x0000 | cpu->DPL); END(cpu); break;

			// NOP_ABS
			case 0x0C | (0b001 << 8):
			case 0x0C | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0C | (0b011 << 8): read(DATAPTR(cpu)); END(cpu); break;

			// NOP_ABX
			case 0x1C | (0b001 << 8):
			case 0x3C | (0b001 << 8):
			case 0x5C | (0b001 << 8):
			case 0x7C | (0b001 << 8):
			case 0xDC | (0b001 << 8):
			case 0xFC | (0b001 << 8):
			case 0x1C | (0b010 << 8):
			case 0x3C | (0b010 << 8):
			case 0x5C | (0b010 << 8):
			case 0x7C | (0b010 << 8):
			case 0xDC | (0b010 << 8):
			case 0xFC | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1C | (0b011 << 8):
			case 0x3C | (0b011 << 8):
			case 0x5C | (0b011 << 8):
			case 0x7C | (0b011 << 8):
			case 0xDC | (0b011 << 8):
			case 0xFC | (0b011 << 8): read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else END(cpu); break;
			case 0x1C | (0b100 << 8):
			case 0x3C | (0b100 << 8):
			case 0x5C | (0b100 << 8):
			case 0x7C | (0b100 << 8):
			case 0xDC | (0b100 << 8):
			case 0xFC | (0b100 << 8): read(DATAPTR(cpu)); END(cpu); break;

			// KIL
			default: cpu->step--; break;
		}
	}
	cpu->step++;
}

#undef DATAPTR
#undef PROGCOUNTER
#undef END

#endif // ifndef CPU_H