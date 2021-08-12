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
extern inline void sub(CPU *cpu, uint8_t value);


// The definition of all functions (even those which aren't supposed to be accessed by external code -- all of them except initCPU and tickCPU) are in the header file instead of a source file to make use of inlining
// Inline functions are also extern in order to be forced in the same translation unit as external code (a requirement of C inlining)

// Undefined later
#define DATAPTR(cpu) (cpu->DPH << 8) | cpu->DPL
#define PROGCOUNTER(cpu) ((cpu)->PCH << 8) | (cpu)->PCL
#define END(cpu) cpu->step = -1

extern inline uint8_t read(uint16_t address) {
	// TODO this
	uint8_t data = 0x00;
	//printf("%4X r %2X\n", address, data);
	return data;
}

extern inline void write(uint16_t address, uint8_t data) {
	//printf("%4X W %2X\n", address, data);
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
		case 1: cpu->temp = fetch(cpu); if (condition) END(cpu); break;
		case 2: read(PROGCOUNTER(cpu)); cpu->PCL += (cpu->temp - 128); if (!((cpu->temp < 128 && cpu->PCL > cpu->temp) || (cpu->temp > 128 && cpu->PCL < cpu->temp))) {END(cpu); cpu->PCL++; cpu->PCH += !cpu->PCL;} break;
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

extern inline void sub(CPU *cpu, uint8_t value) {
	// TODO this
}

void initCPU(CPU *cpu) {
	cpu->A = cpu->X = cpu->Y = 0;
	cpu->PCH = 0xFF;
	cpu->PCL = 0xFE;
	cpu->negFlag = cpu->oflowFlag = cpu->decFlag = cpu->zeroFlag = cpu->carryFlag = false;
	cpu->noIRQFlag = true;
	cpu->SP = 0xFD;
	cpu->step = 0;
}

extern inline void tickCPU(CPU *cpu) {
	// TODO reset routine
	// TODO handle end of instruction
	// TODO reorder (0B and 2B are both ANC_IMM, E9 and EB are both SBC_IMM)
	// TODO handle KILs
	if (cpu->step == 0) {
		cpu->IR = fetch(cpu);
	} else {
		switch ((cpu->IR << 3) | (cpu->step & 0b00000111)) {
			// BRK
			case (0x00 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x00 << 3) | 0b010: push(cpu, cpu->PCH); break;
			case (0x00 << 3) | 0b011: push(cpu, cpu->PCL); break;
			case (0x00 << 3) | 0b100: push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00110000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); break;
			case (0x00 << 3) | 0b101: cpu->PCL = read(0xFFFE); break;
			case (0x00 << 3) | 0b110: cpu->PCH = read(0xFFFF); END(cpu); break;

			// ORA_IZX
			case (0x01 << 3) | 0b001:
			case (0x01 << 3) | 0b010:
			case (0x01 << 3) | 0b011:
			case (0x01 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x01 << 3) | 0b101: cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SLO_IZX
			case (0x03 << 3) | 0b001: 
			case (0x03 << 3) | 0b010:
			case (0x03 << 3) | 0b011:
			case (0x03 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x03 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x03 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->B); break;
			case (0x03 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ORA_ZP
			case (0x05 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x05 << 3) | 0b010: cpu->A |= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL_ZP
			case (0x06 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x06 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x06 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case (0x06 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SLO_ZP
			case (0x07 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x07 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x07 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; break;
			case (0x07 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// PHP
			case (0x08 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x08 << 3) | 0b010: push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00110000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); END(cpu); break;

			// ORA_IMM
			case (0x09 << 3) | 0b001: cpu->A |= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL
			case (0x0A << 3) | 0b001: fetch(cpu); cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1; nzFlags(cpu, cpu->A); END(cpu); break;

			// ANC_IMM TODO 2B IS ALSO ANC_IMM
			case (0x0B << 3) | 0b001: cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b10000000; nzFlags(cpu, cpu->A); END(cpu); break;

			// ORA_ABS
			case (0x0D << 3) | 0b001:
			case (0x0D << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x0D << 3) | 0b011: cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL_ABS
			case (0x0E << 3) | 0b001:
			case (0x0E << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x0E << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x0E << 3) | 0b100: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case (0x0E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SLO_ABS
			case (0x0F << 3) | 0b001:
			case (0x0F << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x0F << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x0F << 3) | 0b100: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x0F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BPL
			case (0x10 << 3) | 0b001:
			case (0x10 << 3) | 0b010:
			case (0x10 << 3) | 0b011: branch(cpu, !cpu->negFlag); END(cpu); break;

			// ORA_IZY
			case (0x11 << 3) | 0b001:
			case (0x11 << 3) | 0b010:
			case (0x11 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x11 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x11 << 3) | 0b101: cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SLO_IZY
			case (0x13 << 3) | 0b001:
			case (0x13 << 3) | 0b010:
			case (0x13 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x13 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x13 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x13 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu); break;

			// ORA_ZPX
			case (0x15 << 3) | 0b001:
			case (0x15 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x15 << 3) | 0b011: cpu->A |= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ASL_ZPX
			case (0x16 << 3) | 0b001:
			case (0x16 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x16 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x16 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case (0x16 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SLO_ZPX
			case (0x17 << 3) | 0b001:
			case (0x17 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x17 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x17 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x17 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// CLC
			case (0x18 << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->carryFlag = false; END(cpu); break;

			// ORA_ABY
			case (0x19 << 3) | 0b001:
			case (0x19 << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x19 << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x19 << 3) | 0b100: cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SLO_ABY
			case (0x1B << 3) | 0b001:
			case (0x1B << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x1B << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x1B << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x1B << 3) | 0b101: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x1B << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ORA_ABX
			case (0x1D << 3) | 0b001:
			case (0x1D << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x1D << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x1D << 3) | 0b100: cpu->A |= read(DATAPTR(cpu)); nzFlags(cpu, cpu->B); END(cpu); break;

			// ASL_ABX
			case (0x1E << 3) | 0b001:
			case (0x1E << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x1E << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x1E << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x1E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case (0x1E << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SLO_ABX
			case (0x1F << 3) | 0b001:
			case (0x1F << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x1F << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x1F << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x1F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x1F << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// JSR
			case (0x20 << 3) | 0b001: cpu->temp = fetch(cpu); break;
			case (0x20 << 3) | 0b010: read(0x0100 | cpu->SP); break;
			case (0x20 << 3) | 0b011: push(cpu, cpu->PCH); break;
			case (0x20 << 3) | 0b100: push(cpu, cpu->PCL); break;
			case (0x20 << 3) | 0b101: cpu->PCH = fetch(cpu); cpu->PCL = cpu->temp; END(cpu); break;

			// AND_IZX
			case (0x21 << 3) | 0b001:
			case (0x21 << 3) | 0b010:
			case (0x21 << 3) | 0b011:
			case (0x21 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x21 << 3) | 0b101: cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// RLA_IZX
			case (0x23 << 3) | 0b001:
			case (0x23 << 3) | 0b010:
			case (0x23 << 3) | 0b011:
			case (0x23 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x23 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x23 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x23 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BIT_ZP
			case (0x24 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x24 << 3) | 0b010: {uint8_t value = read(0x0000 | cpu->DPL); cpu->oflowFlag = value & 0b01000000; cpu->negFlag = value & 0b10000000; cpu->zeroFlag = !(value & cpu->A);} END(cpu); break;

			// AND_ZP
			case (0x25 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x25 << 3) | 0b010: cpu->A &= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ZP
			case (0x26 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x26 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x26 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case (0x26 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RLA_ZP
			case (0x27 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x27 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x27 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x27 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// PLP
			case (0x28 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x28 << 3) | 0b010: pull(cpu); break;
			case (0x28 << 3) | 0b011: {uint8_t flags = read(0x0100 | cpu->SP); cpu->negFlag = flags & 0b10000000; cpu->oflowFlag = flags & 0b01000000; cpu->decFlag = flags & 0b00001000; cpu->noIRQFlag = flags & 0b00000100; cpu->zeroFlag = flags & 0b00000010; cpu->carryFlag = flags & 0b00000001;} END(cpu); break;

			// AND_IMM
			case (0x29 << 3) | 0b001: cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL
			case (0x2A << 3) | 0b001: read(PROGCOUNTER(cpu)); if (cpu->carryFlag) {cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1; cpu->A++;} else {cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1;} nzFlags(cpu, cpu->A); END(cpu); break;

			// ANC_IMM TODO 0B IS ALSO ANC_IMM
			case (0x2B << 3) | 0b001: cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b10000000; nzFlags(cpu, cpu->A); END(cpu); break;

			// BIT_ABS
			case (0x2C << 3) | 0b001:
			case (0x2C << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x2C << 3) | 0b011: {uint8_t value = read(DATAPTR(cpu)); cpu->oflowFlag = value & 0b01000000; cpu->negFlag = value & 0b10000000; cpu->zeroFlag = !(value & cpu->A);} END(cpu); break;

			// AND_ABS
			case (0x2D << 3) | 0b001:
			case (0x2D << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x2D << 3) | 0b011: cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ABS
			case (0x2E << 3) | 0b001:
			case (0x2E << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x2E << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x2E << 3) | 0b100: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case (0x2E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RLA_ABS
			case (0x2F << 3) | 0b001:
			case (0x2F << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x2F << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x2F << 3) | 0b100: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x2F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BMI
			case (0x30 << 3) | 0b001:
			case (0x30 << 3) | 0b010:
			case (0x30 << 3) | 0b011: branch(cpu, cpu->negFlag); END(cpu); break;

			// AND_IZY
			case (0x31 << 3) | 0b001:
			case (0x31 << 3) | 0b010:
			case (0x31 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x31 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x31 << 3) | 0b101: cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// RLA_IZY
			case (0x33 << 3) | 0b001:
			case (0x33 << 3) | 0b010:
			case (0x33 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x33 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x33 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x33 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x33 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// AND_ZPX
			case (0x35 << 3) | 0b001:
			case (0x35 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x35 << 3) | 0b011: cpu->A &= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ZPX
			case (0x36 << 3) | 0b001:
			case (0x36 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x36 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case (0x36 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RLA_ZPX
			case (0x37 << 3) | 0b001:
			case (0x37 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x37 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x37 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x37 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SEC
			case (0x38 << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->carryFlag = true; END(cpu); break;

			// AND_ABY
			case (0x39 << 3) | 0b001:
			case (0x39 << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x39 << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x39 << 3) | 0b100: cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// RLA_ABY
			case (0x3B << 3) | 0b001:
			case (0x3B << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x3B << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x3B << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x3B << 3) | 0b101: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x3B << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// AND_ABX
			case (0x3D << 3) | 0b001:
			case (0x3D << 3) | 0b010: absAddressing(cpu, cpu->X);
			case (0x3D << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x3D << 3) | 0b100: cpu->A &= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// ROL_ABX
			case (0x3E << 3) | 0b001:
			case (0x3E << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x3E << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x3E << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x3E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case (0x3E << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RLA_ABX
			case (0x3F << 3) | 0b001:
			case (0x3F << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x3F << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x3F << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x3F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x3F << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RTI
			case (0x40 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x40 << 3) | 0b010: pull(cpu); break;
			case (0x40 << 3) | 0b011: {uint8_t flags = pull(cpu); cpu->negFlag = flags & 0b10000000; cpu->oflowFlag = flags & 0b01000000; cpu->decFlag = flags & 0b00001000; cpu->noIRQFlag = flags & 0b00000100; cpu->zeroFlag = flags & 0b00000010; cpu->carryFlag = flags & 0b00000001;} break;
			case (0x40 << 3) | 0b100: cpu->PCL = pull(cpu); break;
			case (0x40 << 3) | 0b101: cpu->PCH = read(0x0100 | cpu->SP); END(cpu); break;

			// EOR_IZX
			case (0x41 << 3) | 0b001:
			case (0x41 << 3) | 0b010:
			case (0x41 << 3) | 0b011:
			case (0x41 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x41 << 3) | 0b101: cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SRE_IZX
			case (0x43 << 3) | 0b001:
			case (0x43 << 3) | 0b010:
			case (0x43 << 3) | 0b011:
			case (0x43 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x43 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x43 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x43 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// EOR_ZP
			case (0x45 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x45 << 3) | 0b010: cpu->A ^= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR_ZP
			case (0x46 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x46 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x46 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case (0x46 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SRE_ZP
			case (0x47 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x47 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x47 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x47 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// PHA
			case (0x48 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x48 << 3) | 0b010: push(cpu, cpu->A); END(cpu); break;

			// EOR_IMM
			case (0x49 << 3) | 0b001: cpu->A ^= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR
			case (0x4A << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; nzFlags(cpu, cpu->A); END(cpu); break;

			// ALR_IMM
			case (0x4B << 3) | 0b001: cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; nzFlags(cpu, cpu->A); END(cpu); break;

			// JMP_ABS
			case (0x4C << 3) | 0b001: cpu->temp = fetch(cpu); break;
			case (0x4C << 3) | 0b010: cpu->PCH = fetch(cpu); cpu->PCL = cpu->temp; END(cpu); break;

			// EOR_ABS
			case (0x4D << 3) | 0b001:
			case (0x4D << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x4D << 3) | 0b011: cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR_ABS
			case (0x4E << 3) | 0b001:
			case (0x4E << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x4E << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x4E << 3) | 0b100: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case (0x4E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SRE_ABS
			case (0x4F << 3) | 0b001:
			case (0x4F << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x4F << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x4F << 3) | 0b100: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x4F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BVC
			case (0x50 << 3) | 0b001:
			case (0x50 << 3) | 0b010:
			case (0x50 << 3) | 0b011: branch(cpu, !cpu->oflowFlag); END(cpu); break;

			// EOR_IZY
			case (0x51 << 3) | 0b001:
			case (0x51 << 3) | 0b010:
			case (0x51 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x51 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x51 << 3) | 0b101: cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SRE_IZY
			case (0x53 << 3) | 0b001:
			case (0x53 << 3) | 0b010:
			case (0x53 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x53 << 3) | 0b100: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x53 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x53 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x53 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// EOR_ZPX
			case (0x55 << 3) | 0b001:
			case (0x55 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x55 << 3) | 0b011: cpu->A ^= read(0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); END(cpu); break;

			// LSR_ZPX
			case (0x56 << 3) | 0b001:
			case (0x56 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x56 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x56 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case (0x56 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SRE_ZPX
			case (0x57 << 3) | 0b001:
			case (0x57 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x57 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x57 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x57 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// CLI
			case (0x58 << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->noIRQFlag = false; END(cpu); break;

			// EOR_ABY
			case (0x59 << 3) | 0b001:
			case (0x59 << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x59 << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x59 << 3) | 0b100: cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->A); END(cpu); break;

			// SRE_ABY
			case (0x5B << 3) | 0b001:
			case (0x5B << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x5B << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x5B << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x5B << 3) | 0b101: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x5B << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// EOR_ABX
			case (0x5D << 3) | 0b001:
			case (0x5D << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x5D << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); END(cpu);} break;
			case (0x5D << 3) | 0b100: cpu->A ^= read(DATAPTR(cpu)); nzFlags(cpu, cpu->B); END(cpu); break;

			// LSR_ABX
			case (0x5E << 3) | 0b001:
			case (0x5E << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x5E << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x5E << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x5E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case (0x5E << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// SRE_ABX
			case (0x5F << 3) | 0b001:
			case (0x5F << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x5F << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x5F << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x5F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case (0x5F << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RTS
			case (0x60 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x60 << 3) | 0b010: pull(cpu); break;
			case (0x60 << 3) | 0b011: cpu->PCL = pull(cpu); break;
			case (0x60 << 3) | 0b100: cpu->PCH = read(0x0100 | cpu->SP); break;
			case (0x60 << 3) | 0b101: fetch(cpu); END(cpu); break;

			// ADC_IZX
			case (0x61 << 3) | 0b001:
			case (0x61 << 3) | 0b010:
			case (0x61 << 3) | 0b011:
			case (0x61 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x61 << 3) | 0b101: add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// RRA_IZX
			case (0x63 << 3) | 0b001:
			case (0x63 << 3) | 0b010:
			case (0x63 << 3) | 0b011:
			case (0x63 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x63 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x63 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x63 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ADC_ZP
			case (0x65 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x65 << 3) | 0b010: add(cpu, read(0x0000 | cpu->DPL)); END(cpu); break;

			// ROR_ZP
			case (0x66 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x66 << 3) | 0b010: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x66 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case (0x66 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RRA_ZP
			case (0x67 << 3) | 0b000: cpu->DPL = fetch(cpu); break;
			case (0x67 << 3) | 0b001: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x67 << 3) | 0b010: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x67 << 3) | 0b011: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// PLA
			case (0x68 << 3) | 0b001: read(PROGCOUNTER(cpu)); break;
			case (0x68 << 3) | 0b010: pull(cpu); break;
			case (0x68 << 3) | 0b011: cpu->A = pull(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// ADC_IMM
			case (0x69 << 3) | 0b001: add(cpu, fetch(cpu)); END(cpu); break;

			// ROR
			case (0x6A << 3) | 0b001: read(PROGCOUNTER(cpu)); if (cpu->carryFlag) {cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; cpu->A |= 0b10000000;} else {cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1;} END(cpu); break;

			// ARR_IMM
			// TODO some sources say the V flag is set from the XOR of bit 6 and 7 of A, not bit 5 and 6
			case (0x6B << 3) | 0b001: cpu->B = fetch(cpu); cpu->A &= cpu->B; cpu->A >>= 1; if (cpu->carryFlag) cpu->A |= 0b10000000; nzFlags(cpu, cpu->A); cpu->carryFlag = cpu->A & 0b01000000; cpu->oflowFlag = (cpu->A & 0b01000000) ^ (cpu->A & 0b00100000); END(cpu); break;

			// JMP_IND
			case (0x6C << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x6C << 3) | 0b010: cpu->DPH = fetch(cpu); break;
			case (0x6C << 3) | 0b011: cpu->temp = read(DATAPTR(cpu)); cpu->DPL++; break;
			case (0x6C << 3) | 0b100: cpu->PCH = read(DATAPTR(cpu)); cpu->PCL = cpu->temp; END(cpu); break;

			// ADC_ABS
			case (0x6D << 3) | 0b001:
			case (0x6D << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x6D << 3) | 0b011: add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// ROR_ABS
			case (0x6E << 3) | 0b001:
			case (0x6E << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x6E << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x6E << 3) | 0b100: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case (0x6E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RRA_ABS
			case (0x6F << 3) | 0b001:
			case (0x6F << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x6F << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); break;
			case (0x6F << 3) | 0b100: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x6F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// BVS
			case (0x70 << 3) | 0b001:
			case (0x70 << 3) | 0b010:
			case (0x70 << 3) | 0b011: branch(cpu, cpu->oflowFlag); END(cpu); break;

			// ADC_IZY
			case (0x71 << 3) | 0b001:
			case (0x71 << 3) | 0b010:
			case (0x71 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x71 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, cpu->B); END(cpu);} break;
			case (0x71 << 3) | 0b101: add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// RRA_IZY
			case (0x73 << 3) | 0b001:
			case (0x73 << 3) | 0b010:
			case (0x73 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x73 << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x73 << 3) | 0b101: cpu->B = read(DATAPTR(cpu)); break;
			case (0x73 << 3) | 0b110: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x73 << 3) | 0b111: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ADC_ZPX
			case (0x75 << 3) | 0b001:
			case (0x75 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x75 << 3) | 0b011: add(cpu, read(0x0000 | cpu->DPL)); END(cpu); break;

			// ROR_ZPX
			case (0x76 << 3) | 0b001:
			case (0x76 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x76 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x76 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->X); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case (0x76 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// RRA_ZPX
			case (0x77 << 3) | 0b001:
			case (0x77 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x77 << 3) | 0b011: cpu->B = read(0x0000 | cpu->DPL); break;
			case (0x77 << 3) | 0b100: write(0x0000 | cpu->DPL, cpu->X); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x77 << 3) | 0b101: write(0x0000 | cpu->DPL, cpu->B); END(cpu); break;

			// SEI
			case (0x78 << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->noIRQFlag = true; END(cpu); break;

			// ADC_ABY
			case (0x79 << 3) | 0b001:
			case (0x79 << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x79 << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, cpu->B); END(cpu);} break;
			case (0x79 << 3) | 0b100: add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// RRA_ABY
			case (0x7B << 3) | 0b001:
			case (0x7B << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x7B << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x7B << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x7B << 3) | 0b101: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x7B << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// ADC_ABX
			case (0x7D << 3) | 0b001:
			case (0x7D << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x7D << 3) | 0b011: cpu->B = read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {add(cpu, cpu->B); END(cpu);} break;
			case (0x7D << 3) | 0b100: add(cpu, read(DATAPTR(cpu))); END(cpu); break;

			// ROR_ABX
			case (0x7E << 3) | 0b001:
			case (0x7E << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x7E << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x7E << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x7E << 3) | 0b101: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case (0x7E << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// RRA_ABX
			case (0x7F << 3) | 0b001:
			case (0x7F << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x7F << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x7F << 3) | 0b100: cpu->B = read(DATAPTR(cpu)); break;
			case (0x7F << 3) | 0b101: write(DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case (0x7F << 3) | 0b110: write(DATAPTR(cpu), cpu->B); END(cpu); break;

			// STA_IZX
			case (0x81 << 3) | 0b001:
			case (0x81 << 3) | 0b010:
			case (0x81 << 3) | 0b011:
			case (0x81 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x81 << 3) | 0b101: write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// SAX_IZX
			case (0x83 << 3) | 0b001:
			case (0x83 << 3) | 0b010:
			case (0x83 << 3) | 0b011:
			case (0x83 << 3) | 0b100: izxAddressing(cpu); break;
			case (0x83 << 3) | 0b101: write(DATAPTR(cpu), cpu->A & cpu->X); END(cpu); break;

			// STY_ZP
			case (0x84 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x84 << 3) | 0b010: write(0x0000 | cpu->DPL, cpu->Y); END(cpu); break;

			// STY_ZP
			case (0x85 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x85 << 3) | 0b010: write(0x0000 | cpu->DPL, cpu->A); END(cpu); break;

			// STY_ZP
			case (0x86 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x86 << 3) | 0b010: write(0x0000 | cpu->DPL, cpu->X); END(cpu); break;

			// STY_ZP
			case (0x87 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x87 << 3) | 0b010: write(0x0000 | cpu->DPL, cpu->A & cpu->X); END(cpu); break;

			// DEY
			case (0x88 << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->Y--; nzFlags(cpu, cpu->Y); END(cpu); break;
			
			// TXA
			case (0x8A << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->A); END(cpu); break;

			// XAA_IMM
			// TODO sources contradict each other regarding this
			case (0x8B << 3) | 0b001: cpu->A |= 0xEE; cpu->A &= cpu->X; cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); END(cpu); break;

			// STY_ABS
			case (0x8C << 3) | 0b001:
			case (0x8C << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x8C << 3) | 0b011: write(DATAPTR(cpu), cpu->Y); END(cpu); break;

			// STA_ABS
			case (0x8D << 3) | 0b001:
			case (0x8D << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x8D << 3) | 0b011: write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// STX_ABS
			case (0x8E << 3) | 0b001:
			case (0x8E << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x8E << 3) | 0b011: write(DATAPTR(cpu), cpu->X); END(cpu); break;

			// SAX_ABS
			case (0x8F << 3) | 0b001:
			case (0x8F << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x8F << 3) | 0b011: write(DATAPTR(cpu), cpu->A & cpu->X); END(cpu); break;

			// BCC
			case (0x90 << 3) | 0b001:
			case (0x90 << 3) | 0b010:
			case (0x90 << 3) | 0b011: branch(cpu, !cpu->carryFlag); END(cpu); break;

			// STA_IZY
			case (0x91 << 3) | 0b001:
			case (0x91 << 3) | 0b010:
			case (0x91 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x91 << 3) | 0b100: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x91 << 3) | 0b101: write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// AHX_IZY
			case (0x93 << 3) | 0b001:
			case (0x93 << 3) | 0b010:
			case (0x93 << 3) | 0b011: izyAddressing(cpu); break;
			case (0x93 << 3) | 0b100: read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x93 << 3) | 0b101: write(DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); END(cpu); break;

			// STY_ZPX
			case (0x94 << 3) | 0b001:
			case (0x94 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x94 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->Y); END(cpu); break;

			// STA_ZPX
			case (0x95 << 3) | 0b001:
			case (0x95 << 3) | 0b010: zpiAddressing(cpu, cpu->X); break;
			case (0x95 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->A); END(cpu); break;

			// STX_ZPY
			case (0x96 << 3) | 0b001:
			case (0x96 << 3) | 0b010: zpiAddressing(cpu, cpu->Y); break;
			case (0x96 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->X); END(cpu); break;

			// SAX_ZPY
			case (0x97 << 3) | 0b001:
			case (0x97 << 3) | 0b010: zpiAddressing(cpu, cpu->Y); break;
			case (0x97 << 3) | 0b011: write(0x0000 | cpu->DPL, cpu->A & cpu->X); END(cpu); break;

			// TYA
			case (0x98 << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->A = cpu->Y; nzFlags(cpu, cpu->A); END(cpu); break;

			// STA_ABY
			case (0x99 << 3) | 0b001:
			case (0x99 << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x99 << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x99 << 3) | 0b100: write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// TXS
			case (0x9A << 3) | 0b001: read(PROGCOUNTER(cpu)); cpu->SP = cpu->X; END(cpu); break;

			// TAS_ABY
			case (0x9B << 3) | 0b001:
			case (0x9B << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x9B << 3) | 0b011: read(DATAPTR(cpu)); cpu->SP = cpu->A & cpu->X; cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x9B << 3) | 0b100: write(DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); END(cpu); break;

			// SHY_ABX
			case (0x9C << 3) | 0b001:
			case (0x9C << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x9C << 3) | 0b011: read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x9C << 3) | 0b100: write(DATAPTR(cpu), cpu->X & cpu->temp); END(cpu); break;

			// STA_ABX
			case (0x9D << 3) | 0b001:
			case (0x9D << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x9D << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case (0x9D << 3) | 0b100: write(DATAPTR(cpu), cpu->A); END(cpu); break;

			// SHX_ABY
			case (0x9E << 3) | 0b001:
			case (0x9E << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x9E << 3) | 0b011: read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x9E << 3) | 0b100: write(DATAPTR(cpu), cpu->X & cpu->temp); END(cpu); break;

			// AHX_ABY
			case (0x9F << 3) | 0b001:
			case (0x9F << 3) | 0b010: absAddressing(cpu, cpu->Y); break;
			case (0x9F << 3) | 0b011: read(DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case (0x9F << 3) | 0b100: write(DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); END(cpu); break;

			case (0xA0 << 3) | 0b001: break; // LDY_IMM
			case (0xA1 << 3) | 0b001: break; // LDA_IMM
			case (0xA2 << 3) | 0b001: break; // LDX_IMM
			case (0xA3 << 3) | 0b001: break; // LAX_IZX
			case (0xA4 << 3) | 0b001: break; // LDY_ZP
			case (0xA5 << 3) | 0b001: break; // LDA_ZP
			case (0xA6 << 3) | 0b001: break; // LDX_ZP
			case (0xA7 << 3) | 0b001: break; // LAX_ZP
			case (0xA8 << 3) | 0b001: break; // TAY
			case (0xA9 << 3) | 0b001: break; // LDA_IMM
			case (0xAA << 3) | 0b001: break; // TAX
			case (0xAB << 3) | 0b001: break; // LAX_IMM
			case (0xAC << 3) | 0b001: break; // LDY_ABS
			case (0xAD << 3) | 0b001: break; // LDA_ABS
			case (0xAE << 3) | 0b001: break; // LDX_ABS
			case (0xAF << 3) | 0b001: break; // LAX_ABS
			case (0xB0 << 3) | 0b001: break; // BCS
			case (0xB1 << 3) | 0b001: break; // LDA_IZY
			case (0xB3 << 3) | 0b001: break; // LAX_IZY
			case (0xB4 << 3) | 0b001: break; // LDY_ZPX
			case (0xB5 << 3) | 0b001: break; // LDA_ZPX
			case (0xB6 << 3) | 0b001: break; // LDX_ZPY
			case (0xB7 << 3) | 0b001: break; // LAX_ZPY
			case (0xB8 << 3) | 0b001: break; // CLV
			case (0xB9 << 3) | 0b001: break; // LDA_ABY
			case (0xBA << 3) | 0b001: break; // TSX
			case (0xBB << 3) | 0b001: break; // LAS_ABY
			case (0xBC << 3) | 0b001: break; // LDY_ABX
			case (0xBD << 3) | 0b001: break; // LDA_ABX
			case (0xBE << 3) | 0b001: break; // LDX_ABY
			case (0xBF << 3) | 0b001: break; // LAX_ABY
			case (0xC0 << 3) | 0b001: break; // CPY_IMM
			case (0xC1 << 3) | 0b001: break; // CPM_IZX
			case (0xC3 << 3) | 0b001: break; // DCP_IZX
			case (0xC4 << 3) | 0b001: break; // CPY_ZP
			case (0xC5 << 3) | 0b001: break; // CMP_ZP
			case (0xC6 << 3) | 0b001: break; // DEC_ZP
			case (0xC7 << 3) | 0b001: break; // DCP_ZP
			case (0xC8 << 3) | 0b001: break; // INY
			case (0xC9 << 3) | 0b001: break; // CMP_IMM
			case (0xCA << 3) | 0b001: break; // DEX
			case (0xCB << 3) | 0b001: break; // AXS_IMM
			case (0xCC << 3) | 0b001: break; // CPY_ABS
			case (0xCD << 3) | 0b001: break; // CMP_ABS
			case (0xCE << 3) | 0b001: break; // DEC_ABS
			case (0xCF << 3) | 0b001: break; // DCP_ABS
			case (0xD0 << 3) | 0b001: break; // BNE
			case (0xD1 << 3) | 0b001: break; // CMP_IZY
			case (0xD3 << 3) | 0b001: break; // DCP_IZY
			case (0xD5 << 3) | 0b001: break; // CMP_ZPX
			case (0xD6 << 3) | 0b001: break; // DEC_ZPX
			case (0xD7 << 3) | 0b001: break; // DCP_ZPX
			case (0xD8 << 3) | 0b001: break; // CLD
			case (0xD9 << 3) | 0b001: break; // CMP_ABY
			case (0xDB << 3) | 0b001: break; // DCP_ABY
			case (0xDD << 3) | 0b001: break; // CMP_ABX
			case (0xDE << 3) | 0b001: break; // DEC_ABX
			case (0xDF << 3) | 0b001: break; // DCP_ABX
			case (0xE0 << 3) | 0b001: break; // CPX_IMM
			case (0xE1 << 3) | 0b001: break; // SBC_IZY
			case (0xE3 << 3) | 0b001: break; // ISC_IZX
			case (0xE4 << 3) | 0b001: break; // CPX_ZP
			case (0xE5 << 3) | 0b001: break; // SBC_ZP
			case (0xE6 << 3) | 0b001: break; // INC_ZP
			case (0xE7 << 3) | 0b001: break; // ISC_ZP
			case (0xE8 << 3) | 0b001: break; // INX
			case (0xE9 << 3) | 0b001: break; // SBC_IMM NOTE EB IS ALSO SBC_IMM
			case (0xEB << 3) | 0b001: break; // SBC_IMM NOTE E9 IS ALSO SBC_IMM
			case (0xEC << 3) | 0b001: break; // CPX_ABS
			case (0xED << 3) | 0b001: break; // SBC_ABS
			case (0xEE << 3) | 0b001: break; // INC_ABS
			case (0xEF << 3) | 0b001: break; // ISC_ABS
			case (0xF0 << 3) | 0b001: break; // BEQ
			case (0xF1 << 3) | 0b001: break; // SBC_IZY
			case (0xF3 << 3) | 0b001: break; // ISC_IZY
			case (0xF5 << 3) | 0b001: break; // SBC_ZPX
			case (0xF6 << 3) | 0b001: break; // INC_ZPX
			case (0xF7 << 3) | 0b001: break; // ISC_ZPX
			case (0xF8 << 3) | 0b001: break; // SED
			case (0xF9 << 3) | 0b001: break; // SBC_ABY
			case (0xFB << 3) | 0b001: break; // ISC_ABY
			case (0xFD << 3) | 0b001: break; // SBC_ABX
			case (0xFE << 3) | 0b001: break; // INC_ABX
			case (0xFF << 3) | 0b001: break; // ISC_ABX


			// NOP
			case (0x1A << 3) | 0b001:
			case (0x3A << 3) | 0b001:
			case (0x5A << 3) | 0b001:
			case (0x7A << 3) | 0b001:
			case (0xDA << 3) | 0b001:
			case (0xEA << 3) | 0b001:
			case (0xFA << 3) | 0b001: read(PROGCOUNTER(cpu)); END(cpu); break;

			// NOP_IMM
			case (0x80 << 3) | 0b001:
			case (0x89 << 3) | 0b001: fetch(cpu); END(cpu); break;

			// NOP_ZP
			case (0x04 << 3) | 0b001:
			case (0x44 << 3) | 0b001:
			case (0x64 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x04 << 3) | 0b010:
			case (0x44 << 3) | 0b010:
			case (0x64 << 3) | 0b010: read(0x0000 | cpu->DPL); END(cpu); break;

			// NOP_ZPX
			case (0x14 << 3) | 0b001:
			case (0x34 << 3) | 0b001:
			case (0x54 << 3) | 0b001:
			case (0x74 << 3) | 0b001:
			case (0xD4 << 3) | 0b001:
			case (0xF4 << 3) | 0b001: cpu->DPL = fetch(cpu); break;
			case (0x14 << 3) | 0b010:
			case (0x34 << 3) | 0b010:
			case (0x54 << 3) | 0b010:
			case (0x74 << 3) | 0b010:
			case (0xD4 << 3) | 0b010:
			case (0xF4 << 3) | 0b010: read(0x0000 | cpu->DPL); cpu->DPL += cpu->X; break;
			case (0x14 << 3) | 0b011:
			case (0x34 << 3) | 0b011:
			case (0x54 << 3) | 0b011:
			case (0x74 << 3) | 0b011:
			case (0xD4 << 3) | 0b011:
			case (0xF4 << 3) | 0b011: read(0x0000 | cpu->DPL); cpu->DPL += cpu->X; END(cpu); break;

			// NOP_ABS
			case (0x0C << 3) | 0b001:
			case (0x0C << 3) | 0b010: absAddressing(cpu, 0); break;
			case (0x0C << 3) | 0b011: read(DATAPTR(cpu)); END(cpu); break;

			// NOP_ABX
			case (0x1C << 3) | 0b001:
			case (0x3C << 3) | 0b001:
			case (0x5C << 3) | 0b001:
			case (0x7C << 3) | 0b001:
			case (0xDC << 3) | 0b001:
			case (0xFC << 3) | 0b001:
			case (0x1C << 3) | 0b010:
			case (0x3C << 3) | 0b010:
			case (0x5C << 3) | 0b010:
			case (0x7C << 3) | 0b010:
			case (0xDC << 3) | 0b010:
			case (0xFC << 3) | 0b010: absAddressing(cpu, cpu->X); break;
			case (0x1C << 3) | 0b011:
			case (0x3C << 3) | 0b011:
			case (0x5C << 3) | 0b011:
			case (0x7C << 3) | 0b011:
			case (0xDC << 3) | 0b011:
			case (0xFC << 3) | 0b011: read(DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else END(cpu); break;
			case (0x1C << 3) | 0b100:
			case (0x3C << 3) | 0b100:
			case (0x5C << 3) | 0b100:
			case (0x7C << 3) | 0b100:
			case (0xDC << 3) | 0b100:
			case (0xFC << 3) | 0b100: read(DATAPTR(cpu)); END(cpu); break;

			default: /* KIL */ break;
		}
	}
	cpu->step++;
}

#undef DATAPTR
#undef PROGCOUNTER
#undef END

#endif // ifndef CPU_H