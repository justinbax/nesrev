#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "bus.h"

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

typedef struct CPU {
	uint8_t PCL; // Low byte of program counter
	uint8_t PCH; // High byte of program counter
	uint8_t SP; // Stack pointer register
	uint8_t IR; // Instruction register
	uint8_t step; // Micro-instruction step counter to keep track of the current step inside an instruction

	// I am unaware of the specific usage of these registers on a physical 6502, as they are very rarely documented.
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

	// Debug information
	int debugLog;
	FILE *logFile;

	char rw;
	uint16_t addressPins;
	uint8_t dataPins;
	uint64_t cycleCount;

	Bus *bus;
} CPU;

// Interface functions
void initCPU(CPU *cpu, Bus *bus);
void pollInterrupts(CPU *cpu);
void tickCPU(CPU *cpu);
void setLogCPU(CPU *cpu, int logOption, FILE *logFile);

// Non-interface functions
uint8_t fetch(CPU *cpu);
void push(CPU *cpu, uint8_t data);
uint8_t pull(CPU *cpu);
void zpiAddressing(CPU *cpu, uint8_t indexReg);
void absAddressing(CPU *cpu, uint8_t indexReg);
void izxAddressing(CPU *cpu);
void izyAddressing(CPU *cpu);
void branch(CPU *cpu, bool condition);
void nzFlags(CPU *cpu, uint8_t result);
void add(CPU *cpu, uint8_t value);
void checkInterrupts(CPU *cpu);

#endif // ifndef CPU_H