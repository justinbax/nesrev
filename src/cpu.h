#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ppu.h"
#include "input.h"

#define DBG_NONE 0
#define DBG_REDUCED 1
#define DBG_FULL 2

#define DMA_NONE 0
#define DMA_WAIT 1
#define DMA_READ 2
#define DMA_WRITE 3

#define OAMDMA 0x4014
#define JOY1 0x4016
#define JOY2 0x4017

#define RESET_VECTOR 0xFFFC
#define RESET_STEP 0xF0
#define NMI_VECTOR 0xFFFA
#define NMI_STEP 0xE0
#define IRQ_VECTOR 0xFFFE
#define IRQ_STEP 0xD0

#define READ 'r'
#define WRITE 'W'

// Used to avoid confusion when working with pins
#define HIGH true
#define LOW false

typedef struct {
	uint8_t PCL; // Low byte of program counter
	uint8_t PCH; // High byte of program counter
	uint8_t SP; // Stack pointer register
	uint8_t IR; // Instruction register
	uint8_t step; // Micro-instruction step counter to keep track of the current step inside an instruction

	// I am unaware of the specific usage (and even existence!) of these registers on a physical 6502, as they are very rarely documented.
	// However, to achieve cycle accuracy, they are needed in emulation to retain information across cycles.
	// Fortunately, this has no effect whatsoever on the output of the NES, as none of them change the timing of read/writes.
	// In this emulator, DPL and DPH are used as a general 16-bit register for simple addressing modes. Meanwhile, temp is mostly used with DP in indirect addressing, jumps, branches and obscure illegal instructions.
	uint8_t DPL; // Low byte of data pointer register used in address calculation
	uint8_t DPH; // High byte of data pointer register used in address calculation
	uint8_t temp; // Temporary register used during indirect addressing modes

	// General purpose register
	uint8_t A;
	uint8_t X;
	uint8_t Y;

	// With A, this is the second ALU register, not accessible by the user but used to perform arithmetic and logical operations on memory data during read-modify-write instructions.
	// Like DPL, DPH and temp, this register isn't used when it doesn't need to be (emulation-wise), even when a physical 6502 might. This doesn't change the timing of r/w operations.
	uint8_t B;

	bool negFlag;
	bool oflowFlag;
	bool decFlag;
	bool noIRQFlag;
	bool zeroFlag;
	bool carryFlag;

	// *Pin represents the physical inputs accessible by external hardware, while *Pending represents the internal signal raised during PHI2 to tell the next instruction should be an interrupt routine
	// prevNMI represents the status of the NMI pin at the last PHI2 cycle, and is used to detect a level detection.
	// Because of weird behaviour of the branching operations (checking mid-instruction without immediately changing instruction counter) and conflicts of step rewrite with END, we need a last signal telling if the next instruction is an interrupt handler (nextIs*)
	bool IRQPin;
	bool IRQPending;
	bool nextIsIRQ;
	
	bool NMIPin;
	bool NMIPending;
	bool prevNMI;
	bool nextIsNMI;

	uint8_t OAMDMAstatus;
	uint8_t OAMDMApage;

	uint8_t internalRAM[0x0800];

	PPU *ppu;
	Port *ports[2];

	// Debug information
	int debugLog;
	FILE *logFile;

	char rw;
	uint16_t addressPins;
	uint8_t dataPins;
	uint64_t cycleCount;
} CPU;

// Interface functions
void initCPU(CPU *cpu, PPU *ppu, Port ports[2]);
extern inline void pollInterrupts(CPU *cpu);
extern inline void tickCPU(CPU *cpu);
void setLogCPU(CPU *cpu, int logOption, FILE *logFile);

// Non-interface functions, still accessible by external code (explanation below)
extern inline uint8_t read(CPU *cpu, uint16_t address);
extern inline void write(CPU *cpu, uint16_t address, uint8_t data);
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
extern inline void checkInterrupts(CPU *cpu);


// The definition of all functions (even those which aren't supposed to be accessed by external code -- all of them except initCPU and tickCPU) are in the header file instead of a source file to make use of inlining
// Inline functions are also extern in order to be forced in the same translation unit as external code (a requirement of C inlining)

// Undefined later
#define DATAPTR(cpu) (cpu->DPH << 8) | cpu->DPL
#define PROGCOUNTER(cpu) ((cpu)->PCH << 8) | (cpu)->PCL
#define END(cpu) cpu->step = -1
#define PRINTFULLDBG(cpu, instruction, step) fprintf(cpu->logFile, "%04X %c %02X (%7s step %c)\n", cpu->addressPins, cpu->rw, cpu->dataPins, instruction, step)

// Used for debug log and disassembly
const char instructions[256][8] = {
	"BRK", "ORA_IZX", "KIL", "SLO_IZX", "NOP_ZP", "ORA_ZP", "ASL_ZP", "SLO_ZP", "PHP", "ORA_IMM", "ASL", "ANC_IMM", "NOP_ABS", "ORA_ABS", "ASL_ABS", "SLO_ABS",
	"BPL", "ORA_IZY", "KIL", "SLO_IZY", "NOP_ZPX", "ORA_ZPX", "ASL_ZPX", "SLO_ZPX", "CLC", "ORA_ABY", "NOP", "SLO_ABY", "NOP_ABX", "ORA_ABX", "ASL_ABX", "SLO_ABX",
	"JSR", "AND_IZX", "KIL", "RLA_IZX", "BIT_ZP", "AND_ZP", "ROL_ZP", "RLA_ZP", "PLP", "AND_IMM", "ROL", "ANC_IMM", "BIT_ABS", "AND_ABS", "ROL_ABS", "RLA_ABS",
	"BMI", "AND_IZY", "KIL", "RLA_AZY", "NOP_ZPX", "AND_ZPX", "ROL_ZPX", "RLA_ZPX", "SEC", "AND_ABY", "NOP", "RLA_ABY", "NOP_ABX", "AND_ABX", "ROL_ABX", "RLA_ABX",
	"RTI", "EOR_IZX", "KIL", "SRE_IZX", "NOP_ZP", "EOR_ZP", "LSR_ZP", "SRE_ZP", "PHA", "EOR_IMM", "LSR", "ALR_IMM", "JMP_ABS", "EOR_ABS", "LSR_ABS", "SRE_ABS",
	"BVC", "EOR_IZY", "KIL", "SRE_IZY", "NOP_ZPX", "EOR_ZPX", "LSR_ZPX", "SRE_ZPX", "CLI", "EOR_ABY", "NOP", "SRE_ABY", "NOP_ABX", "EOR_ABX", "LSR_ABX", "SRE_ABX",
	"RTS", "ADC_IZX", "KIL", "RRA_IZX", "NOP_ZP", "ADC_ZP", "ROR_ZP", "RRA_ZP", "PLA", "ADC_IMM", "ROR", "ARR_IMM", "JPM_IND", "ADC_ABS", "ROR_ABS", "RRA_ABS",
	"BVS", "ADC_IZY", "KIL", "RRA_IZY", "NOP_ZPX", "ADC_ZPX", "ROR_ZPX", "RRA_ZPX", "SEI", "ADC_ABY", "NOP", "RRA_ABY", "NOP_ABX", "ADC_ABX", "ROR_ABX", "RRA_ABX",
	"NOP_IMM", "STA_IZX", "NOP_IMM", "SAX_IZX", "STY_ZP", "STA_ZP", "STX_ZP", "SAX_ZP", "DEY", "NOP_IMM", "TXA", "XAA_IMM", "STY_ABS", "STA_ABS", "STX_ABS", "SAX_ABS",
	"BCC", "STA_IZY", "KIL", "AHX_IZY", "STY_ZPX", "STA_ZPX", "STX_ZPY", "SAX_ZPY", "TYA", "STA_ABY", "TXS", "TAS_ABY", "SHY_ABX", "STA_ABX", "SHX_ABY", "AHX_ABY",
	"LDY_IMM", "LDA_IZX", "LDX_IMM", "LAX_IZX", "LDY_ZP", "LDA_ZP", "LDX_ZP", "LAX_ZP", "TAY", "LDA_IMM", "TAX", "LAX_IMM", "LDY_ABS", "LDA_ABS", "LDX_ABS", "LAX_ABS",
	"BCS", "LDA_IZY", "KIL", "LAX_IZY", "LDY_ZPX", "LDA_ZPX", "LDX_ZPY", "LAX_ZPY", "CLV", "LDA_ABY", "TSX", "LAS_ABY", "LDY_ABX", "LDA_ABX", "LDX_ABY", "LAX_ABY",
	"CPY_IMM", "CMP_IZX", "NOP_IMM", "DCP_IZX", "CPY_ZP", "CMP_ZP", "DEC_ZP", "DCP_ZP", "INY", "CMP_IMM", "DEX", "AXS_IMM", "CPY_ABS", "CMP_ABS", "DEC_ABS", "DCP_ABS",
	"BNE", "CMP_IZY", "KIL", "DCP_IZY", "NOP_ZPX", "CMP_ZPX", "DEC_ZPX", "DCP_ZPX", "CLD", "CMP_ABY", "NOP", "DCP_ABY", "NOP_ABX", "CMP_ABX", "DEC_ABX", "DCP_ABX",
	"CPX_IMM", "SBC_IZX", "NOP_IMM", "ISC_IZX", "CPX_ZP", "SBC_ZP", "INC_ZP", "ISC_ZP", "INX", "SBC_IMM", "NOP", "SBC_IMM", "CPX_ABS", "SBC_ABS", "INC_ABS", "ISC_ABS",
	"BEQ", "SBC", "KIL", "ISC", "NOP_ZPX", "SBC_ZPX", "INC_ZPX", "ISC_ZPX", "SED", "SBC_ABY", "NOP", "ISC_ABY", "NOP_ABX", "SBC_ABX", "INC_ABX", "ISC_ABX"
};

extern inline uint8_t read(CPU *cpu, uint16_t address) {
	// TODO deal with open buses
	if (address < 0x2000)
		cpu->dataPins = cpu->internalRAM[address & 0x7FF];
	else if (address < 0x4000)
		cpu->dataPins = readRegisterPPU(cpu->ppu, address);
	else if (address < 0x4020)
		switch (address) {
			case OAMDMA: cpu->dataPins = 0x00; break;
			case JOY1:
			case JOY2: cpu->dataPins = readController(cpu->ports[address - JOY1]); break;
			default: cpu->dataPins = 0x00; break;
		}
	else
		cpu->dataPins = cartReadCPU(cpu->ppu->cart, address);

	cpu->rw = READ;
	cpu->addressPins = address;

	return cpu->dataPins;
}

extern inline void write(CPU *cpu, uint16_t address, uint8_t data) {
	if (address < 0x2000)
		cpu->internalRAM[address & 0x7FF] = data;
	else if (address < 0x4000)
		writeRegisterPPU(cpu->ppu, address, data);
	else if (address < 0x4020)
		switch (address) {
			case OAMDMA:
				if (cpu->OAMDMAstatus == DMA_WAIT && (cpu->cycleCount & 0b1) == 1)
					cpu->OAMDMAstatus = DMA_READ;
				else
					cpu->OAMDMAstatus = DMA_WAIT;
				cpu->OAMDMApage = data;
				break;
			case JOY1:
			case JOY2: writeController(cpu->ports[0], data); writeController(cpu->ports[1], data); break;
			default: break;
		}
	else
		cartWriteCPU(cpu->ppu->cart, address, data);
	
	cpu->rw = WRITE;
	cpu->addressPins = address;
	cpu->dataPins = data;
}

extern inline uint8_t fetch(CPU *cpu) {
	uint8_t result = read(cpu, PROGCOUNTER(cpu));
	cpu->PCL++;
	cpu->PCH += !cpu->PCL;
	return result;
}

extern inline void push(CPU *cpu, uint8_t data) {
	write(cpu, 0x0100 | cpu->SP, data);
	cpu->SP--;
}

extern inline uint8_t pull(CPU *cpu) {
	uint8_t result = read(cpu, 0x0100 | cpu->SP);
	cpu->SP++;
	return result;
}

extern inline void zpiAddressing(CPU *cpu, uint8_t indexReg) {
	switch (cpu->step) {
		case 1: cpu->DPL = fetch(cpu); break;
		case 2: read(cpu, 0x0000 | cpu->DPL); cpu->DPL += indexReg; break;
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
		case 2: read(cpu, 0x0000 | cpu->temp); cpu->temp += cpu->X; break;
		case 3: cpu->DPL = read(cpu, 0x0000 | cpu->temp); cpu->temp++; break;
		case 4: cpu->DPH = read(cpu, 0x0000 | cpu->temp); break;
	}
}

extern inline void izyAddressing(CPU *cpu) {
	switch (cpu->step) {
		case 1: cpu->temp = fetch(cpu); break;
		case 2: cpu->DPL = read(cpu, 0x0000 | cpu->temp); cpu->temp++; break;
		case 3: cpu->DPH = read(cpu, 0x0000 | cpu->temp); cpu->DPL += cpu->Y; break;
	}
}

extern inline void branch(CPU *cpu, bool condition) {
	switch (cpu->step) {
		case 1:
			cpu->temp = fetch(cpu);
			if (!condition) END(cpu);
			checkInterrupts(cpu);
			break;
		case 2:
			read(cpu, PROGCOUNTER(cpu));
			// Two's complement
			cpu->PCL += ((cpu->temp & 0b10000000) > 0 ? (int8_t)(cpu->temp - 256) : cpu->temp);
			// If a page boundary is crossed
			if (((cpu->temp & 0b10000000) == 0 && cpu->PCL < cpu->temp) || ((cpu->temp & 0b10000000) > 0 && cpu->PCL >= cpu->temp))
				checkInterrupts(cpu);
			else
				END(cpu);
			break;
		case 3:
			read(cpu, PROGCOUNTER(cpu));
			if (cpu->temp & 0b10000000)
				cpu->PCH--;
			else
				cpu->PCH++;
			END(cpu);
			break;
	}
}

extern inline void nzFlags(CPU *cpu, uint8_t result) {
	cpu->negFlag = result & 0b10000000;
	cpu->zeroFlag = !result;
}

extern inline void add(CPU *cpu, uint8_t value) {
	uint8_t result = cpu->A + value + cpu->carryFlag;
	// The carry flag is also set if the 8-bit result is the same but it still overflowed because of the previous carry flag (SEC + ADC $FF sets the carry flag even though the 8-bit result doesn't appear to have overflowed)
	cpu->carryFlag = (result < cpu->A) || (result == cpu->A && cpu->carryFlag);
	// Weird signed overflow check, used by very few programs
	cpu->oflowFlag = (((cpu->A ^ result) & (value ^ result) & 0b10000000) > 0);
	cpu->A = result;
	nzFlags(cpu, cpu->A);
}

extern inline void checkInterrupts(CPU *cpu) {
	// TODO optimize
	cpu->nextIsIRQ = cpu->IRQPending;
	cpu->nextIsNMI = cpu->NMIPending;
}


// Interface functions
void initCPU(CPU *cpu, PPU *ppu, Port ports[2]) {
	// Note: this is the status of the CPU BEFORE the reset sequence
	cpu->A = cpu->X = cpu->Y = 0;
	cpu->PCH = 0x00;
	cpu->PCL = 0xFF;
	cpu->negFlag = cpu->oflowFlag = cpu->decFlag = cpu->noIRQFlag = cpu->zeroFlag = cpu->carryFlag = false;
	cpu->SP = cpu->IR = 0x00;
	cpu->step = RESET_STEP;
	cpu->IRQPin = cpu->NMIPin = HIGH;
	cpu->nextIsIRQ = cpu->nextIsNMI = false;
	
	cpu->ppu = ppu;
	cpu->ports[0] = &ports[0];
	cpu->ports[1] = &ports[1];

	for (int i = 0; i < 0x800; i++)
		cpu->internalRAM[i] = 0x00;

	cpu->OAMDMAstatus = DMA_NONE;
	cpu->OAMDMApage = 0x00;

	cpu->debugLog = DBG_NONE;
	cpu->logFile = NULL;

	cpu->addressPins = 0x0000;
	cpu->dataPins = 0x00;
	cpu->rw = '?';
	cpu->cycleCount = 0;
}

extern inline void pollInterrupts(CPU *cpu) {
	cpu->IRQPending = !cpu->IRQPin;
	if (cpu->prevNMI && !cpu->NMIPin) cpu->NMIPending = true;
	cpu->prevNMI = cpu->NMIPin;
}

void setLogCPU(CPU *cpu, int logOption, FILE *logFile) {
	cpu->logFile = logFile;

	cpu->debugLog = logOption;
	if (cpu->logFile == NULL)
		cpu->debugLog = DBG_NONE;
}


extern inline void tickCPU(CPU *cpu) {
	// TODO optimize
	// TODO interrupts are ugly

	// An NMI has priority over an IRQ
	if (cpu->nextIsNMI && cpu->step == 0) {
		cpu->IR = 0x00;
		cpu->step = NMI_STEP;
		cpu->nextIsNMI = false;
		cpu->nextIsIRQ = false;
	} else if (cpu->nextIsIRQ && cpu->step == 0) {
		cpu->IR = 0x00;
		cpu->step = IRQ_STEP;
		cpu->nextIsIRQ = false;
	}
	
	if (cpu->OAMDMAstatus > DMA_WAIT) {

		if (cpu->OAMDMAstatus == DMA_READ) {
			cpu->B = read(cpu, (cpu->OAMDMApage << 8) | cpu->DPL);
			cpu->OAMDMAstatus = DMA_WRITE;
		} else {
			writeRegisterPPU(cpu->ppu, OAMDATA, cpu->B);
			cpu->DPL++;
			if (cpu->DPL == 0) cpu->OAMDMAstatus = DMA_NONE;
			else cpu->OAMDMAstatus = DMA_READ;
		}
		cpu->step--;

	} else if (cpu->step == 0) {
		// Every write cycle (how OAMDMAstatus becomes DMA_WAIT) is either followed by another write cycle in RMW instructions (which the DMA lets execute) or by an opcode fetch, which the DMA hijacks.
		if (cpu->OAMDMAstatus != DMA_WAIT) {
			cpu->IR = fetch(cpu);
		} else {
			cpu->DPL = 0x00;
			cpu->step--;
			read(cpu, PROGCOUNTER(cpu)); // Dummy read

			if ((cpu->cycleCount & 0b1) == 1)
				// This will cause the first read to start on an even cycle. For DMA, those are all 'get' cycles, while odd ones are 'put' cycles.
				cpu->OAMDMAstatus = DMA_READ;
		}
	} else {

		switch ((cpu->step << 8) | cpu->IR) {
			// Because the micro-instruction step counter will never exceed 3 bits in width, all cases setting a bit 3-7 will never occur during execution and can be used for other micro-instructions, such as interrupt sequences
			// RESET
			case 0x00 | ((RESET_STEP + 0) << 8):
			case 0x00 | ((RESET_STEP + 1) << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x00 | ((RESET_STEP + 2) << 8):
			case 0x00 | ((RESET_STEP + 3) << 8):
			case 0x00 | ((RESET_STEP + 4) << 8): read(cpu, 0x0100 | cpu->SP); cpu->SP--; break;
			case 0x00 | ((RESET_STEP + 5) << 8): cpu->PCL = read(cpu, RESET_VECTOR); cpu->noIRQFlag = true; break;
			case 0x00 | ((RESET_STEP + 6) << 8): cpu->PCH = read(cpu, RESET_VECTOR + 1); END(cpu); break;

			// NMI
			case 0x00 | ((NMI_STEP + 0) << 8):
			case 0x00 | ((NMI_STEP + 1) << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x00 | ((NMI_STEP + 2) << 8): push(cpu, cpu->PCH); break;
			case 0x00 | ((NMI_STEP + 3) << 8): push(cpu, cpu->PCL); break;
			case 0x00 | ((NMI_STEP + 4) << 8): push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00100000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); break;
			case 0x00 | ((NMI_STEP + 5) << 8): cpu->PCL = read(cpu, NMI_VECTOR); cpu->noIRQFlag = true; break;
			case 0x00 | ((NMI_STEP + 6) << 8): cpu->PCH = read(cpu, NMI_VECTOR + 1); cpu->NMIPending = false; END(cpu); break;

			// IRQ
			case 0x00 | ((IRQ_STEP + 0) << 8):
			case 0x00 | ((IRQ_STEP + 1) << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x00 | ((IRQ_STEP + 2) << 8): push(cpu, cpu->PCH); break;
			case 0x00 | ((IRQ_STEP + 3) << 8): push(cpu, cpu->PCL); break;
			case 0x00 | ((IRQ_STEP + 4) << 8): push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00100000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); if (cpu->NMIPending) cpu->step = NMI_STEP + 4; break;
			case 0x00 | ((IRQ_STEP + 5) << 8): cpu->PCL = read(cpu, IRQ_VECTOR); cpu->noIRQFlag = true; break;
			case 0x00 | ((IRQ_STEP + 6) << 8): cpu->PCH = read(cpu, IRQ_VECTOR + 1); END(cpu); break;

			// BRK
			case 0x00 | (0b001 << 8): fetch(cpu); break;
			case 0x00 | (0b010 << 8): push(cpu, cpu->PCH); break;
			case 0x00 | (0b011 << 8): push(cpu, cpu->PCL); break;
			case 0x00 | (0b100 << 8): push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00110000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); if (cpu->NMIPending) cpu->step = NMI_STEP + 4; else if (cpu->IRQPending) cpu->step = IRQ_STEP + 4; break;
			case 0x00 | (0b101 << 8): cpu->PCL = read(cpu, IRQ_VECTOR); cpu->noIRQFlag = true; break;
			case 0x00 | (0b110 << 8): cpu->PCH = read(cpu, IRQ_VECTOR + 1); END(cpu); break;

			// ORA_IZX
			case 0x01 | (0b001 << 8):
			case 0x01 | (0b010 << 8):
			case 0x01 | (0b011 << 8):
			case 0x01 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x01 | (0b101 << 8): cpu->A |= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SLO_IZX
			case 0x03 | (0b001 << 8): 
			case 0x03 | (0b010 << 8):
			case 0x03 | (0b011 << 8):
			case 0x03 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x03 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x03 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x03 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ORA_ZP
			case 0x05 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x05 | (0b010 << 8): cpu->A |= read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ASL_ZP
			case 0x06 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x06 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x06 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x06 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SLO_ZP
			case 0x07 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x07 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x07 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; break;
			case 0x07 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// PHP
			case 0x08 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x08 | (0b010 << 8): push(cpu, (cpu->negFlag << 7) | (cpu->oflowFlag << 6) | 0b00110000 | (cpu->decFlag << 3) | (cpu->noIRQFlag << 2) | (cpu->zeroFlag << 1) | cpu->carryFlag); checkInterrupts(cpu); END(cpu); break;

			// ORA_IMM
			case 0x09 | (0b001 << 8): cpu->A |= fetch(cpu); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ASL
			case 0x0A | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ANC_IMM
			case 0x0B | (0b001 << 8):

			case 0x2B | (0b001 << 8): cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); cpu->carryFlag = cpu->negFlag; checkInterrupts(cpu); END(cpu); break;

			// ORA_ABS
			case 0x0D | (0b001 << 8):
			case 0x0D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0D | (0b011 << 8): cpu->A |= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ASL_ABS
			case 0x0E | (0b001 << 8):
			case 0x0E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0E | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x0E | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x0E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SLO_ABS
			case 0x0F | (0b001 << 8):
			case 0x0F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0F | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x0F | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x0F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BPL
			case 0x10 | (0b001 << 8):
			case 0x10 | (0b010 << 8): branch(cpu, !cpu->negFlag); break;
			case 0x10 | (0b011 << 8): branch(cpu, !cpu->negFlag); checkInterrupts(cpu); break;

			// ORA_IZY
			case 0x11 | (0b001 << 8):
			case 0x11 | (0b010 << 8):
			case 0x11 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x11 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x11 | (0b101 << 8): cpu->A |= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SLO_IZY
			case 0x13 | (0b001 << 8):
			case 0x13 | (0b010 << 8):
			case 0x13 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x13 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x13 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; break;
			case 0x13 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->A |= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ORA_ZPX
			case 0x15 | (0b001 << 8):
			case 0x15 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x15 | (0b011 << 8): cpu->A |= read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ASL_ZPX
			case 0x16 | (0b001 << 8):
			case 0x16 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x16 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x16 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x16 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SLO_ZPX
			case 0x17 | (0b001 << 8):
			case 0x17 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x17 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x17 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x17 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CLC
			case 0x18 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->carryFlag = false; checkInterrupts(cpu); END(cpu); break;

			// ORA_ABY
			case 0x19 | (0b001 << 8):
			case 0x19 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x19 | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x19 | (0b100 << 8): cpu->A |= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SLO_ABY
			case 0x1B | (0b001 << 8):
			case 0x1B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x1B | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x1B | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x1B | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x1B | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ORA_ABX
			case 0x1D | (0b001 << 8):
			case 0x1D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1D | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A |= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x1D | (0b100 << 8): cpu->A |= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ASL_ABX
			case 0x1E | (0b001 << 8):
			case 0x1E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1E | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x1E | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x1E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; nzFlags(cpu, cpu->B); break;
			case 0x1E | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SLO_ABX
			case 0x1F | (0b001 << 8):
			case 0x1F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x1F | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x1F | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x1F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->A |= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x1F | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// JSR
			case 0x20 | (0b001 << 8): cpu->temp = fetch(cpu); break;
			case 0x20 | (0b010 << 8): read(cpu, 0x0100 | cpu->SP); break;
			case 0x20 | (0b011 << 8): push(cpu, cpu->PCH); break;
			case 0x20 | (0b100 << 8): push(cpu, cpu->PCL); break;
			case 0x20 | (0b101 << 8): cpu->PCH = fetch(cpu); cpu->PCL = cpu->temp; checkInterrupts(cpu); END(cpu); break;

			// AND_IZX
			case 0x21 | (0b001 << 8):
			case 0x21 | (0b010 << 8):
			case 0x21 | (0b011 << 8):
			case 0x21 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x21 | (0b101 << 8): cpu->A &= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// RLA_IZX
			case 0x23 | (0b001 << 8):
			case 0x23 | (0b010 << 8):
			case 0x23 | (0b011 << 8):
			case 0x23 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x23 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x23 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x23 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BIT_ZP
			case 0x24 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x24 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); cpu->oflowFlag = cpu->B & 0b01000000; cpu->negFlag = cpu->B & 0b10000000; cpu->zeroFlag = !(cpu->B & cpu->A); checkInterrupts(cpu); END(cpu); break;

			// AND_ZP
			case 0x25 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x25 | (0b010 << 8): cpu->A &= read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ROL_ZP
			case 0x26 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x26 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x26 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x26 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RLA_ZP
			case 0x27 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x27 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x27 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x27 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// PLP
			case 0x28 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x28 | (0b010 << 8): pull(cpu); break;
			case 0x28 | (0b011 << 8): checkInterrupts(cpu); {uint8_t flags = read(cpu, 0x0100 | cpu->SP); cpu->negFlag = flags & 0b10000000; cpu->oflowFlag = flags & 0b01000000; cpu->decFlag = flags & 0b00001000; cpu->noIRQFlag = flags & 0b00000100; cpu->zeroFlag = flags & 0b00000010; cpu->carryFlag = flags & 0b00000001;} END(cpu); break;

			// AND_IMM
			case 0x29 | (0b001 << 8): cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ROL
			case 0x2A | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); if (cpu->carryFlag) {cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1; cpu->A++;} else {cpu->carryFlag = cpu->A & 0b10000000; cpu->A <<= 1;} nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// BIT_ABS
			case 0x2C | (0b001 << 8):
			case 0x2C | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2C | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->oflowFlag = cpu->B & 0b01000000; cpu->negFlag = cpu->B & 0b10000000; cpu->zeroFlag = !(cpu->B & cpu->A); checkInterrupts(cpu); END(cpu); break;

			// AND_ABS
			case 0x2D | (0b001 << 8):
			case 0x2D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2D | (0b011 << 8): cpu->A &= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ROL_ABS
			case 0x2E | (0b001 << 8):
			case 0x2E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2E | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x2E | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x2E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RLA_ABS
			case 0x2F | (0b001 << 8):
			case 0x2F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x2F | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x2F | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x2F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BMI
			case 0x30 | (0b001 << 8):
			case 0x30 | (0b010 << 8): branch(cpu, cpu->negFlag); break;
			case 0x30 | (0b011 << 8): branch(cpu, cpu->negFlag); checkInterrupts(cpu); break;

			// AND_IZY
			case 0x31 | (0b001 << 8):
			case 0x31 | (0b010 << 8):
			case 0x31 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x31 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x31 | (0b101 << 8): cpu->A &= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// RLA_IZY
			case 0x33 | (0b001 << 8):
			case 0x33 | (0b010 << 8):
			case 0x33 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x33 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x33 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x33 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x33 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// AND_ZPX
			case 0x35 | (0b001 << 8):
			case 0x35 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x35 | (0b011 << 8): cpu->A &= read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ROL_ZPX
			case 0x36 | (0b001 << 8):
			case 0x36 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x36 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x36 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x36 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RLA_ZPX
			case 0x37 | (0b001 << 8):
			case 0x37 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x37 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x37 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x37 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SEC
			case 0x38 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->carryFlag = true; checkInterrupts(cpu); END(cpu); break;

			// AND_ABY
			case 0x39 | (0b001 << 8):
			case 0x39 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x39 | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x39 | (0b100 << 8): cpu->A &= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// RLA_ABY
			case 0x3B | (0b001 << 8):
			case 0x3B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x3B | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x3B | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x3B | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x3B | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// AND_ABX
			case 0x3D | (0b001 << 8):
			case 0x3D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x3D | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A &= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x3D | (0b100 << 8): cpu->A &= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ROL_ABX
			case 0x3E | (0b001 << 8):
			case 0x3E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x3E | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x3E | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x3E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} nzFlags(cpu, cpu->B); break;
			case 0x3E | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RLA_ABX
			case 0x3F | (0b001 << 8):
			case 0x3F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x3F | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x3F | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x3F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1; cpu->B++;} else {cpu->carryFlag = cpu->B & 0b10000000; cpu->B <<= 1;} cpu->A &= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x3F | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RTI
			case 0x40 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x40 | (0b010 << 8): pull(cpu); break;
			case 0x40 | (0b011 << 8): {uint8_t flags = pull(cpu); cpu->negFlag = flags & 0b10000000; cpu->oflowFlag = flags & 0b01000000; cpu->decFlag = flags & 0b00001000; cpu->noIRQFlag = flags & 0b00000100; cpu->zeroFlag = flags & 0b00000010; cpu->carryFlag = flags & 0b00000001;} break;
			case 0x40 | (0b100 << 8): cpu->PCL = pull(cpu); break;
			case 0x40 | (0b101 << 8): cpu->PCH = read(cpu, 0x0100 | cpu->SP); checkInterrupts(cpu); END(cpu); break;

			// EOR_IZX
			case 0x41 | (0b001 << 8):
			case 0x41 | (0b010 << 8):
			case 0x41 | (0b011 << 8):
			case 0x41 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x41 | (0b101 << 8): cpu->A ^= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SRE_IZX
			case 0x43 | (0b001 << 8):
			case 0x43 | (0b010 << 8):
			case 0x43 | (0b011 << 8):
			case 0x43 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x43 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x43 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x43 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// EOR_ZP
			case 0x45 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x45 | (0b010 << 8): cpu->A ^= read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LSR_ZP
			case 0x46 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x46 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x46 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x46 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SRE_ZP
			case 0x47 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x47 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x47 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x47 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// PHA
			case 0x48 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x48 | (0b010 << 8): push(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// EOR_IMM
			case 0x49 | (0b001 << 8): cpu->A ^= fetch(cpu); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LSR
			case 0x4A | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ALR_IMM
			case 0x4B | (0b001 << 8): cpu->A &= fetch(cpu); cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// JMP_ABS
			case 0x4C | (0b001 << 8): cpu->temp = fetch(cpu); break;
			case 0x4C | (0b010 << 8): cpu->PCH = fetch(cpu); cpu->PCL = cpu->temp; checkInterrupts(cpu); END(cpu); break;

			// EOR_ABS
			case 0x4D | (0b001 << 8):
			case 0x4D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x4D | (0b011 << 8): cpu->A ^= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LSR_ABS
			case 0x4E | (0b001 << 8):
			case 0x4E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x4E | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x4E | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x4E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SRE_ABS
			case 0x4F | (0b001 << 8):
			case 0x4F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x4F | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x4F | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x4F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BVC
			case 0x50 | (0b001 << 8):
			case 0x50 | (0b010 << 8): branch(cpu, !cpu->oflowFlag); break;
			case 0x50 | (0b011 << 8): branch(cpu, !cpu->oflowFlag); checkInterrupts(cpu); break;

			// EOR_IZY
			case 0x51 | (0b001 << 8):
			case 0x51 | (0b010 << 8):
			case 0x51 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x51 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x51 | (0b101 << 8): cpu->A ^= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SRE_IZY
			case 0x53 | (0b001 << 8):
			case 0x53 | (0b010 << 8):
			case 0x53 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x53 | (0b100 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x53 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x53 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x53 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// EOR_ZPX
			case 0x55 | (0b001 << 8):
			case 0x55 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x55 | (0b011 << 8): cpu->A ^= read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LSR_ZPX
			case 0x56 | (0b001 << 8):
			case 0x56 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x56 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x56 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x56 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SRE_ZPX
			case 0x57 | (0b001 << 8):
			case 0x57 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x57 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x57 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x57 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CLI
			case 0x58 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); checkInterrupts(cpu); cpu->noIRQFlag = false; END(cpu); break;

			// EOR_ABY
			case 0x59 | (0b001 << 8):
			case 0x59 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x59 | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x59 | (0b100 << 8): cpu->A ^= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SRE_ABY
			case 0x5B | (0b001 << 8):
			case 0x5B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x5B | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x5B | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x5B | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x5B | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// EOR_ABX
			case 0x5D | (0b001 << 8):
			case 0x5D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x5D | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0x5D | (0b100 << 8): cpu->A ^= read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// LSR_ABX
			case 0x5E | (0b001 << 8):
			case 0x5E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x5E | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x5E | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x5E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; nzFlags(cpu, cpu->B); break;
			case 0x5E | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SRE_ABX
			case 0x5F | (0b001 << 8):
			case 0x5F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x5F | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x5F | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x5F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->A ^= cpu->B; nzFlags(cpu, cpu->A); break;
			case 0x5F | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RTS
			case 0x60 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x60 | (0b010 << 8): pull(cpu); break;
			case 0x60 | (0b011 << 8): cpu->PCL = pull(cpu); break;
			case 0x60 | (0b100 << 8): cpu->PCH = read(cpu, 0x0100 | cpu->SP); break;
			case 0x60 | (0b101 << 8): fetch(cpu); checkInterrupts(cpu); END(cpu); break;

			// ADC_IZX
			case 0x61 | (0b001 << 8):
			case 0x61 | (0b010 << 8):
			case 0x61 | (0b011 << 8):
			case 0x61 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x61 | (0b101 << 8): add(cpu, read(cpu, DATAPTR(cpu))); checkInterrupts(cpu); END(cpu); break;

			// RRA_IZX
			case 0x63 | (0b001 << 8):
			case 0x63 | (0b010 << 8):
			case 0x63 | (0b011 << 8):
			case 0x63 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x63 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x63 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x63 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ADC_ZP
			case 0x65 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x65 | (0b010 << 8): add(cpu, read(cpu, 0x0000 | cpu->DPL)); checkInterrupts(cpu); END(cpu); break;

			// ROR_ZP
			case 0x66 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x66 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x66 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x66 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RRA_ZP
			case 0x67 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x67 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x67 | (0b011 << 8): write(cpu, cpu->DPL, cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x67 | (0b100 << 8): write(cpu, cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// PLA
			case 0x68 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); break;
			case 0x68 | (0b010 << 8): pull(cpu); break;
			case 0x68 | (0b011 << 8): cpu->A = read(cpu, 0x0100 | cpu->SP); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ADC_IMM
			case 0x69 | (0b001 << 8): add(cpu, fetch(cpu)); checkInterrupts(cpu); END(cpu); break;

			// ROR
			case 0x6A | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); if (cpu->carryFlag) {cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1; cpu->A |= 0b10000000;} else {cpu->carryFlag = cpu->A & 0b00000001; cpu->A >>= 1;} nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// ARR_IMM
			// TODO some sources say the V flag is set from the XOR of bit 6 and 7 of A, not bit 5 and 6
			case 0x6B | (0b001 << 8): cpu->B = fetch(cpu); cpu->A &= cpu->B; cpu->A >>= 1; if (cpu->carryFlag) cpu->A |= 0b10000000; nzFlags(cpu, cpu->A); cpu->carryFlag = cpu->A & 0b01000000; cpu->oflowFlag = (cpu->A & 0b01000000) ^ (cpu->A & 0b00100000); checkInterrupts(cpu); END(cpu); break;

			// JMP_IND
			case 0x6C | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x6C | (0b010 << 8): cpu->DPH = fetch(cpu); break;
			case 0x6C | (0b011 << 8): cpu->temp = read(cpu, DATAPTR(cpu)); cpu->DPL++; break;
			case 0x6C | (0b100 << 8): cpu->PCH = read(cpu, DATAPTR(cpu)); cpu->PCL = cpu->temp; checkInterrupts(cpu); END(cpu); break;

			// ADC_ABS
			case 0x6D | (0b001 << 8):
			case 0x6D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x6D | (0b011 << 8): add(cpu, read(cpu, DATAPTR(cpu))); checkInterrupts(cpu); END(cpu); break;

			// ROR_ABS
			case 0x6E | (0b001 << 8):
			case 0x6E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x6E | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x6E | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x6E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RRA_ABS
			case 0x6F | (0b001 << 8):
			case 0x6F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x6F | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x6F | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x6F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BVS
			case 0x70 | (0b001 << 8):
			case 0x70 | (0b010 << 8): branch(cpu, cpu->oflowFlag); break;
			case 0x70 | (0b011 << 8): branch(cpu, cpu->oflowFlag); checkInterrupts(cpu); break;

			// ADC_IZY
			case 0x71 | (0b001 << 8):
			case 0x71 | (0b010 << 8):
			case 0x71 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x71 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0x71 | (0b101 << 8): add(cpu, read(cpu, DATAPTR(cpu))); checkInterrupts(cpu); END(cpu); break;

			// RRA_IZY
			case 0x73 | (0b001 << 8):
			case 0x73 | (0b010 << 8):
			case 0x73 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x73 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x73 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x73 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x73 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ADC_ZPX
			case 0x75 | (0b001 << 8):
			case 0x75 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x75 | (0b011 << 8): add(cpu, read(cpu, 0x0000 | cpu->DPL)); checkInterrupts(cpu); END(cpu); break;

			// ROR_ZPX
			case 0x76 | (0b001 << 8):
			case 0x76 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x76 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x76 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->X); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x76 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RRA_ZPX
			case 0x77 | (0b001 << 8):
			case 0x77 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x77 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0x77 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->X); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x77 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SEI
			case 0x78 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); checkInterrupts(cpu); cpu->noIRQFlag = true; END(cpu); break;

			// ADC_ABY
			case 0x79 | (0b001 << 8):
			case 0x79 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x79 | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0x79 | (0b100 << 8): add(cpu, read(cpu, DATAPTR(cpu))); checkInterrupts(cpu); END(cpu); break;

			// RRA_ABY
			case 0x7B | (0b001 << 8):
			case 0x7B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x7B | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x7B | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x7B | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x7B | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ADC_ABX
			case 0x7D | (0b001 << 8):
			case 0x7D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x7D | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {add(cpu, cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0x7D | (0b100 << 8): add(cpu, read(cpu, DATAPTR(cpu))); checkInterrupts(cpu); END(cpu); break;

			// ROR_ABX
			case 0x7E | (0b001 << 8):
			case 0x7E | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x7E | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x7E | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x7E | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} nzFlags(cpu, cpu->B); break;
			case 0x7E | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// RRA_ABX
			case 0x7F | (0b001 << 8):
			case 0x7F | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x7F | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x7F | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0x7F | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); if (cpu->carryFlag) {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1; cpu->B |= 0b10000000;} else {cpu->carryFlag = cpu->B & 0b00000001; cpu->B >>= 1;} add(cpu, cpu->B); break;
			case 0x7F | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// STA_IZX
			case 0x81 | (0b001 << 8):
			case 0x81 | (0b010 << 8):
			case 0x81 | (0b011 << 8):
			case 0x81 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x81 | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SAX_IZX
			case 0x83 | (0b001 << 8):
			case 0x83 | (0b010 << 8):
			case 0x83 | (0b011 << 8):
			case 0x83 | (0b100 << 8): izxAddressing(cpu); break;
			case 0x83 | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->A & cpu->X); checkInterrupts(cpu); END(cpu); break;

			// STY_ZP
			case 0x84 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x84 | (0b010 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// STA_ZP
			case 0x85 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x85 | (0b010 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// STX_ZP
			case 0x86 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x86 | (0b010 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// SAX_ZP
			case 0x87 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x87 | (0b010 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->A & cpu->X); checkInterrupts(cpu); END(cpu); break;

			// DEY
			case 0x88 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->Y--; nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;
			
			// TXA
			case 0x8A | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// XAA_IMM
			// TODO sources contradict each other regarding this
			case 0x8B | (0b001 << 8): cpu->A |= 0xEE; cpu->A &= cpu->X; cpu->A &= fetch(cpu); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// STY_ABS
			case 0x8C | (0b001 << 8):
			case 0x8C | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8C | (0b011 << 8): write(cpu, DATAPTR(cpu), cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// STA_ABS
			case 0x8D | (0b001 << 8):
			case 0x8D | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8D | (0b011 << 8): write(cpu, DATAPTR(cpu), cpu->A); checkInterrupts(cpu); END(cpu); break;

			// STX_ABS
			case 0x8E | (0b001 << 8):
			case 0x8E | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8E | (0b011 << 8): write(cpu, DATAPTR(cpu), cpu->X); checkInterrupts(cpu); END(cpu); break;

			// SAX_ABS
			case 0x8F | (0b001 << 8):
			case 0x8F | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x8F | (0b011 << 8): write(cpu, DATAPTR(cpu), cpu->A & cpu->X); checkInterrupts(cpu); END(cpu); break;

			// BCC
			case 0x90 | (0b001 << 8):
			case 0x90 | (0b010 << 8): branch(cpu, !cpu->carryFlag); break;
			case 0x90 | (0b011 << 8): branch(cpu, !cpu->carryFlag); checkInterrupts(cpu); break;

			// STA_IZY
			case 0x91 | (0b001 << 8):
			case 0x91 | (0b010 << 8):
			case 0x91 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x91 | (0b100 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x91 | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->A); checkInterrupts(cpu); END(cpu); break;

			// AHX_IZY
			case 0x93 | (0b001 << 8):
			case 0x93 | (0b010 << 8):
			case 0x93 | (0b011 << 8): izyAddressing(cpu); break;
			case 0x93 | (0b100 << 8): read(cpu, DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x93 | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); checkInterrupts(cpu); END(cpu); break;

			// STY_ZPX
			case 0x94 | (0b001 << 8):
			case 0x94 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x94 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// STA_ZPX
			case 0x95 | (0b001 << 8):
			case 0x95 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0x95 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// STX_ZPY
			case 0x96 | (0b001 << 8):
			case 0x96 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0x96 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// SAX_ZPY
			case 0x97 | (0b001 << 8):
			case 0x97 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0x97 | (0b011 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->A & cpu->X); checkInterrupts(cpu); END(cpu); break;

			// TYA
			case 0x98 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->A = cpu->Y; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// STA_ABY
			case 0x99 | (0b001 << 8):
			case 0x99 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x99 | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x99 | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->A); checkInterrupts(cpu); END(cpu); break;

			// TXS
			case 0x9A | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->SP = cpu->X; checkInterrupts(cpu); END(cpu); break;

			// TAS_ABY
			case 0x9B | (0b001 << 8):
			case 0x9B | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x9B | (0b011 << 8): read(cpu, DATAPTR(cpu)); cpu->SP = cpu->A & cpu->X; cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x9B | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); checkInterrupts(cpu); END(cpu); break;

			// SHY_ABX
			case 0x9C | (0b001 << 8):
			case 0x9C | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x9C | (0b011 << 8): read(cpu, DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x9C | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->X & cpu->temp); checkInterrupts(cpu); END(cpu); break;

			// STA_ABX
			case 0x9D | (0b001 << 8):
			case 0x9D | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0x9D | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0x9D | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->A); checkInterrupts(cpu); END(cpu); break;

			// SHX_ABY
			case 0x9E | (0b001 << 8):
			case 0x9E | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x9E | (0b011 << 8): read(cpu, DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x9E | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->X & cpu->temp); checkInterrupts(cpu); END(cpu); break;

			// AHX_ABY
			case 0x9F | (0b001 << 8):
			case 0x9F | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0x9F | (0b011 << 8): read(cpu, DATAPTR(cpu)); cpu->temp = cpu->DPH + 1; if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0x9F | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->A & cpu->X & cpu->temp); checkInterrupts(cpu); END(cpu); break;

			// LDY_IMM
			case 0xA0 | (0b001 << 8): cpu->Y = fetch(cpu); nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// LDA_IZX
			case 0xA1 | (0b001 << 8):
			case 0xA1 | (0b010 << 8):
			case 0xA1 | (0b011 << 8):
			case 0xA1 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xA1 | (0b101 << 8): cpu->A = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;
			
			// LDX_IMM
			case 0xA2 | (0b001 << 8): cpu->X = fetch(cpu); nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LAX_IZX
			case 0xA3 | (0b001 << 8):
			case 0xA3 | (0b010 << 8):
			case 0xA3 | (0b011 << 8):
			case 0xA3 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xA3 | (0b101 << 8): cpu->X = read(cpu, DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LDY_ZP
			case 0xA4 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xA4 | (0b010 << 8): cpu->Y = read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// LDA_ZP
			case 0xA5 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xA5 | (0b010 << 8): cpu->A = read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LDX_ZP
			case 0xA6 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xA6 | (0b010 << 8): cpu->X = read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LAX_ZP
			case 0xA7 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xA7 | (0b010 << 8): cpu->X = read(cpu, 0x0000 | cpu->DPL); cpu->A = cpu->X; nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// TAY
			case 0xA8 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->Y = cpu->A; nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// LDA_IMM
			case 0xA9 | (0b001 << 8): cpu->A = fetch(cpu); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// TAX
			case 0xAA | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->X = cpu->A; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LAX_IMM
			// TODO not even all sources mention the immediate mode of this operation, and it is said to be unstable
			case 0xAB | (0b001 << 8): cpu->A &= fetch(cpu); cpu->X = cpu->A; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LDY_ABS
			case 0xAC | (0b001 << 8):
			case 0xAC | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAC | (0b011 << 8): cpu->Y = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// LDA_ABS
			case 0xAD | (0b001 << 8):
			case 0xAD | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAD | (0b011 << 8): cpu->A = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LDX_ABS
			case 0xAE | (0b001 << 8):
			case 0xAE | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAE | (0b011 << 8): cpu->X = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LAX_ABS
			case 0xAF | (0b001 << 8):
			case 0xAF | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xAF | (0b011 << 8): cpu->X = read(cpu, DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// BCS
			case 0xB0 | (0b001 << 8):
			case 0xB0 | (0b010 << 8): branch(cpu, cpu->carryFlag); break;
			case 0xB0 | (0b011 << 8): branch(cpu, cpu->carryFlag); checkInterrupts(cpu); break;

			// LDA_IZY
			case 0xB1 | (0b001 << 8):
			case 0xB1 | (0b010 << 8):
			case 0xB1 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xB1 | (0b100 << 8): cpu->A = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0xB1 | (0b101 << 8): cpu->A = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LAX_IZY
			case 0xB3 | (0b001 << 8):
			case 0xB3 | (0b010 << 8):
			case 0xB3 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xB3 | (0b100 << 8): cpu->X = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A = cpu->X; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu);} break;
			case 0xB3 | (0b101 << 8): cpu->X = read(cpu, DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;
			
			// LDY_ZPX
			case 0xB4 | (0b001 << 8):
			case 0xB4 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xB4 | (0b011 << 8): cpu->Y = read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// LDA_ZPX
			case 0xB5 | (0b001 << 8):
			case 0xB5 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xB5 | (0b011 << 8): cpu->A = read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LDX_ZPY
			case 0xB6 | (0b001 << 8):
			case 0xB6 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0xB6 | (0b011 << 8): cpu->X = read(cpu, 0x0000 | cpu->DPL); nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LAX_ZPY
			case 0xB7 | (0b001 << 8):
			case 0xB7 | (0b010 << 8): zpiAddressing(cpu, cpu->Y); break;
			case 0xB7 | (0b011 << 8): cpu->X = read(cpu, 0x0000 | cpu->DPL); cpu->A = cpu->X; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// CLV
			case 0xB8 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->oflowFlag = false; checkInterrupts(cpu); END(cpu); break;

			// LDA_ABY
			case 0xB9 | (0b001 << 8):
			case 0xB9 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xB9 | (0b011 << 8): cpu->A = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0xB9 | (0b100 << 8): cpu->A = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// TSX
			case 0xBA | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->X = cpu->SP; nzFlags(cpu, cpu->SP); checkInterrupts(cpu); END(cpu); break;

			// LAS_ABY
			case 0xBB | (0b001 << 8):
			case 0xBB | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xBB | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->SP &= cpu->B; cpu->A = cpu->SP; cpu->X = cpu->SP; nzFlags(cpu, cpu->SP); checkInterrupts(cpu); END(cpu);} break;
			case 0xBB | (0b100 << 8): cpu->SP &= read(cpu, DATAPTR(cpu)); cpu->A = cpu->SP; cpu->X = cpu->SP; nzFlags(cpu, cpu->SP); checkInterrupts(cpu); END(cpu); break;

			// LDY_ABX
			case 0xBC | (0b001 << 8):
			case 0xBC | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xBC | (0b011 << 8): cpu->Y = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu);} break;
			case 0xBC | (0b100 << 8): cpu->Y = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// LDA_ABX
			case 0xBD | (0b001 << 8):
			case 0xBD | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xBD | (0b011 << 8): cpu->A = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu);} break;
			case 0xBD | (0b100 << 8): cpu->A = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->A); checkInterrupts(cpu); END(cpu); break;

			// LDX_ABY
			case 0xBE | (0b001 << 8):
			case 0xBE | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xBE | (0b011 << 8): cpu->X = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu);} break;
			case 0xBE | (0b100 << 8): cpu->X = read(cpu, DATAPTR(cpu)); nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// LAX_ABY
			case 0xBF | (0b001 << 8):
			case 0xBF | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xBF | (0b011 << 8): cpu->X = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->A = cpu->X; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu);} break;
			case 0xBF | (0b100 << 8): cpu->X = read(cpu, DATAPTR(cpu)); cpu->A = cpu->X; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// CPY_IMM
			case 0xC0 | (0b001 << 8): cpu->B = fetch(cpu); cpu->carryFlag = cpu->Y >= cpu->B; nzFlags(cpu, cpu->Y - cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// CMP_IZX
			case 0xC1 | (0b001 << 8):
			case 0xC1 | (0b010 << 8):
			case 0xC1 | (0b011 << 8):
			case 0xC1 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xC1 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_IZX
			// TODO sources contradict each other on which flags to set. However, the standard CMP flags seem a likely behaviour
			case 0xC3 | (0b001 << 8):
			case 0xC3 | (0b010 << 8):
			case 0xC3 | (0b011 << 8):
			case 0xC3 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xC3 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xC3 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xC3 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CPY_ZP
			case 0xC4 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC4 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); cpu->carryFlag = cpu->Y >= cpu->B; nzFlags(cpu, cpu->Y - cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// CMP_ZP
			case 0xC5 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC5 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// DEC_ZP
			case 0xC6 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC6 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xC6 | (0b011 << 8): write(cpu, cpu->DPL, cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xC6 | (0b100 << 8): write(cpu, cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_ZP
			case 0xC7 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xC7 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xC7 | (0b011 << 8): write(cpu, cpu->DPL, cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xC7 | (0b100 << 8): write(cpu, cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// INY
			case 0xC8 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->Y++; nzFlags(cpu, cpu->Y); checkInterrupts(cpu); END(cpu); break;

			// CMP_IMM
			case 0xC9 | (0b001 << 8): cpu->B = fetch(cpu); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// DEX
			case 0xCA | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->X--; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// AXS_IMM
			case 0xCB | (0b001 << 8): cpu->B = fetch(cpu); cpu->X &= cpu->A; cpu->carryFlag = cpu->X >= cpu->B; cpu->X -= cpu->B; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// CPY_ABS
			case 0xCC | (0b001 << 8):
			case 0xCC | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCC | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->Y >= cpu->B; nzFlags(cpu, cpu->Y - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CMP_ABS
			case 0xCD | (0b001 << 8):
			case 0xCD | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCD | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DEC_ABS
			case 0xCE | (0b001 << 8):
			case 0xCE | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCE | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xCE | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xCE | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_ABS
			case 0xCF | (0b001 << 8):
			case 0xCF | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xCF | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xCF | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xCF | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BNE
			case 0xD0 | (0b001 << 8):
			case 0xD0 | (0b010 << 8): branch(cpu, !cpu->zeroFlag); break;
			case 0xD0 | (0b011 << 8): branch(cpu, !cpu->zeroFlag); checkInterrupts(cpu); break;

			// CMP_IZY
			case 0xD1 | (0b001 << 8):
			case 0xD1 | (0b010 << 8):
			case 0xD1 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xD1 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0xD1 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_IZY
			case 0xD3 | (0b001 << 8):
			case 0xD3 | (0b010 << 8):
			case 0xD3 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xD3 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xD3 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xD3 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xD3 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CMP_ZPX
			case 0xD5 | (0b001 << 8):
			case 0xD5 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xD5 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DEC_ZPX
			case 0xD6 | (0b001 << 8):
			case 0xD6 | (0b010 << 8): zpiAddressing(cpu, cpu->X);
			case 0xD6 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xD6 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xD6 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_ZPX
			case 0xD7 | (0b001 << 8):
			case 0xD7 | (0b010 << 8): zpiAddressing(cpu, cpu->X);
			case 0xD7 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xD7 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xD7 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CLD
			case 0xD8 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->decFlag = false; checkInterrupts(cpu); END(cpu); break;

			// CMP_ABY
			case 0xD9 | (0b001 << 8):
			case 0xD9 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xD9 | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0xD9 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_ABY
			case 0xDB | (0b001 << 8):
			case 0xDB | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xDB | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xDB | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xDB | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xDB | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// CMP_ABX
			case 0xDD | (0b001 << 8):
			case 0xDD | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xDD | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0xDD | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DEC_ABX
			case 0xDE | (0b001 << 8):
			case 0xDE | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xDE | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xDE | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xDE | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; nzFlags(cpu, cpu->B); break;
			case 0xDE | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// DCP_ABX
			case 0xDF | (0b001 << 8):
			case 0xDF | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xDF | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xDF | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xDF | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B--; cpu->carryFlag = cpu->A >= cpu->B; nzFlags(cpu, cpu->A - cpu->B); break;
			case 0xDF | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CPX_IMM
			case 0xE0 | (0b001 << 8): cpu->B = fetch(cpu); cpu->carryFlag = cpu->X >= cpu->B; nzFlags(cpu, cpu->X - cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SBC_IZX
			case 0xE1 | (0b001 << 8):
			case 0xE1 | (0b010 << 8):
			case 0xE1 | (0b011 << 8):
			case 0xE1 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xE1 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xE1 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); add(cpu, ~cpu->B); break;
			case 0xE1 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ISC_IZX
			case 0xE3 | (0b001 << 8):
			case 0xE3 | (0b010 << 8):
			case 0xE3 | (0b011 << 8):
			case 0xE3 | (0b100 << 8): izxAddressing(cpu); break;
			case 0xE3 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xE3 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xE3 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// CPX_ZP
			case 0xE4 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE4 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); cpu->carryFlag = cpu->X >= cpu->B; nzFlags(cpu, cpu->X - cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// SBC_ZP
			case 0xE5 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE5 | (0b010 << 8): add(cpu, ~(read(cpu, 0x0000 | cpu->DPL))); checkInterrupts(cpu); END(cpu); break;

			// INC_ZP
			case 0xE6 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE6 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xE6 | (0b011 << 8): write(cpu, cpu->DPL, cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xE6 | (0b100 << 8): write(cpu, cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ISC_ZP
			case 0xE7 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0xE7 | (0b010 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xE7 | (0b011 << 8): write(cpu, cpu->DPL, cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xE7 | (0b100 << 8): write(cpu, cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// INX
			case 0xE8 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->X++; nzFlags(cpu, cpu->X); checkInterrupts(cpu); END(cpu); break;

			// SBC_IMM
			case 0xE9 | (0b001 << 8):

			case 0xEB | (0b001 << 8): add(cpu, ~(fetch(cpu))); checkInterrupts(cpu); END(cpu); break;

			// CPX_ABS
			case 0xEC | (0b001 << 8):
			case 0xEC | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xEC | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); cpu->carryFlag = cpu->X >= cpu->B; nzFlags(cpu, cpu->X - cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// SBC_ABS
			case 0xED | (0b001 << 8):
			case 0xED | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xED | (0b011 << 8): add(cpu, ~(read(cpu, DATAPTR(cpu)))); checkInterrupts(cpu); END(cpu); break;

			// INC_ABS
			case 0xEE | (0b001 << 8):
			case 0xEE | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xEE | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xEE | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xEE | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ISC_ABS
			case 0xEF | (0b001 << 8):
			case 0xEF | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0xEF | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xEF | (0b100 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xEF | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// BEQ
			case 0xF0 | (0b001 << 8):
			case 0xF0 | (0b010 << 8): branch(cpu, cpu->zeroFlag); break;
			case 0xF0 | (0b011 << 8): branch(cpu, cpu->zeroFlag); checkInterrupts(cpu); break;

			// SBC_IZY
			case 0xF1 | (0b001 << 8):
			case 0xF1 | (0b010 << 8):
			case 0xF1 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xF1 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, ~cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0xF1 | (0b101 << 8): add(cpu, ~(read(cpu, DATAPTR(cpu)))); checkInterrupts(cpu); END(cpu); break;

			// ISC_IZY
			case 0xF3 | (0b001 << 8):
			case 0xF3 | (0b010 << 8):
			case 0xF3 | (0b011 << 8): izyAddressing(cpu); break;
			case 0xF3 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xF3 | (0b101 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xF3 | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xF3 | (0b111 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;
			
			// SBC_ZPX
			case 0xF5 | (0b001 << 8):
			case 0xF5 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xF5 | (0b011 << 8): add(cpu, ~(read(cpu, 0x0000 | cpu->DPL))); checkInterrupts(cpu); END(cpu); break;

			// INC_ZPX
			case 0xF6 | (0b001 << 8):
			case 0xF6 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xF6 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xF6 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xF6 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ISC_ZPX
			case 0xF7 | (0b001 << 8):
			case 0xF7 | (0b010 << 8): zpiAddressing(cpu, cpu->X); break;
			case 0xF7 | (0b011 << 8): cpu->B = read(cpu, 0x0000 | cpu->DPL); break;
			case 0xF7 | (0b100 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xF7 | (0b101 << 8): write(cpu, 0x0000 | cpu->DPL, cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SED
			case 0xF8 | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); cpu->decFlag = true; checkInterrupts(cpu); END(cpu); break;

			// SBC_ABY
			case 0xF9 | (0b001 << 8):
			case 0xF9 | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xF9 | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; else {add(cpu, ~cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0xF9 | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); add(cpu, ~cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ISC_ABY
			case 0xFB | (0b001 << 8):
			case 0xFB | (0b010 << 8): absAddressing(cpu, cpu->Y); break;
			case 0xFB | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->Y) cpu->DPH++; break;
			case 0xFB | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xFB | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xFB | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// SBC_ABX
			case 0xFD | (0b001 << 8):
			case 0xFD | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xFD | (0b011 << 8): cpu->B = read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {add(cpu, ~cpu->B); checkInterrupts(cpu); END(cpu);} break;
			case 0xFD | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); add(cpu, ~cpu->B); checkInterrupts(cpu); END(cpu); break;

			// INC_ABX
			case 0xFE | (0b001 << 8):
			case 0xFE | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xFE | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xFE | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xFE | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; nzFlags(cpu, cpu->B); break;
			case 0xFE | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;

			// ISC_ABX
			case 0xFF | (0b001 << 8):
			case 0xFF | (0b010 << 8): absAddressing(cpu, cpu->X); break;
			case 0xFF | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; break;
			case 0xFF | (0b100 << 8): cpu->B = read(cpu, DATAPTR(cpu)); break;
			case 0xFF | (0b101 << 8): write(cpu, DATAPTR(cpu), cpu->B); cpu->B++; add(cpu, ~cpu->B); break;
			case 0xFF | (0b110 << 8): write(cpu, DATAPTR(cpu), cpu->B); checkInterrupts(cpu); END(cpu); break;


			// On the 6502, every cycle MUST be either a read or a write cycle, even if it is unecessary or the results are unused.
			// Because of this, NOPs (No OPeration) still access memory at that time.
			// These "garbage reads" (or writes, in the case of read-modify-write) are also seen in a lot other operations where the CPU is busy calculating a result used in the next cycle.

			// NOP
			case 0x1A | (0b001 << 8):
			case 0x3A | (0b001 << 8):
			case 0x5A | (0b001 << 8):
			case 0x7A | (0b001 << 8):
			case 0xDA | (0b001 << 8):
			case 0xEA | (0b001 << 8):
			case 0xFA | (0b001 << 8): read(cpu, PROGCOUNTER(cpu)); checkInterrupts(cpu); END(cpu); break;

			// NOP_IMM
			case 0x80 | (0b001 << 8):
			case 0x89 | (0b001 << 8): fetch(cpu); checkInterrupts(cpu); END(cpu); break;

			// NOP_ZP
			case 0x04 | (0b001 << 8):
			case 0x44 | (0b001 << 8):
			case 0x64 | (0b001 << 8): cpu->DPL = fetch(cpu); break;
			case 0x04 | (0b010 << 8):
			case 0x44 | (0b010 << 8):
			case 0x64 | (0b010 << 8): read(cpu, 0x0000 | cpu->DPL); checkInterrupts(cpu); END(cpu); break;

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
			case 0xF4 | (0b010 << 8): read(cpu, 0x0000 | cpu->DPL); cpu->DPL += cpu->X; break;
			case 0x14 | (0b011 << 8):
			case 0x34 | (0b011 << 8):
			case 0x54 | (0b011 << 8):
			case 0x74 | (0b011 << 8):
			case 0xD4 | (0b011 << 8):
			case 0xF4 | (0b011 << 8): read(cpu, 0x0000 | cpu->DPL); checkInterrupts(cpu); END(cpu); break;

			// NOP_ABS
			case 0x0C | (0b001 << 8):
			case 0x0C | (0b010 << 8): absAddressing(cpu, 0); break;
			case 0x0C | (0b011 << 8): read(cpu, DATAPTR(cpu)); checkInterrupts(cpu); END(cpu); break;

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
			case 0xFC | (0b011 << 8): read(cpu, DATAPTR(cpu)); if (cpu->DPL < cpu->X) cpu->DPH++; else {checkInterrupts(cpu); END(cpu);} break;
			case 0x1C | (0b100 << 8):
			case 0x3C | (0b100 << 8):
			case 0x5C | (0b100 << 8):
			case 0x7C | (0b100 << 8):
			case 0xDC | (0b100 << 8):
			case 0xFC | (0b100 << 8): read(cpu, DATAPTR(cpu)); checkInterrupts(cpu); END(cpu); break;

			// KIL
			// This just jams the CPU in a KIL instruction or an illegal step counter (which should never be reached) forever by never updating IR or step and doing nothing.
			default: cpu->step--; break;
		}
	}

	if (cpu->debugLog == DBG_FULL) {
		switch (cpu->step & 0b11111000) {
			// TODO fix RESET, IRQ and NMI becoming BRK at last instruction. We may need to abandon the trick explained below in order to fix this.
			// In PRINTFULLDBG, the step is given as a char in order to handle cycles that finish with END(cpu), leaving the step counter at (uint8_t)(-1).
			// Conveniently, if we add '0' to this value to convert it to ASCII as we would with single-digit numbers, we get the char '/', which represents quite nicely the last step of an instruction.
			case RESET_STEP:
				PRINTFULLDBG(cpu, "RESET", (cpu->step - RESET_STEP + '0'));
				break;
			case NMI_STEP:
				PRINTFULLDBG(cpu, "NMI", (cpu->step - NMI_STEP + '0'));
				break;
			case IRQ_STEP:
				PRINTFULLDBG(cpu, "IRQ", (cpu->step - IRQ_STEP + '0'));
				break;
			default:
				if (instructions[cpu->IR][0] != 'K' || instructions[cpu->IR][1] != 'I' || instructions[cpu->IR][2] != 'L')
					PRINTFULLDBG(cpu, instructions[cpu->IR], (cpu->step + '0'));
				break;
		}
	} else if (cpu->debugLog == DBG_REDUCED && cpu->step == (uint8_t)(-1)) {
		// TODO handle arguments to instructions
		// TODO handle RESET, NMI, IRQ
		fprintf(cpu->logFile, "%7s\n", instructions[cpu->IR]);
	}

	cpu->step++;
	cpu->cycleCount++;
}

#undef DATAPTR
#undef PROGCOUNTER
#undef END

#endif // ifndef CPU_H