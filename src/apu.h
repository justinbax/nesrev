#ifndef APU_H
#define APU_H

#include <stdint.h>
#include <stdbool.h>

#define APU_LENGTHCOUNTER_MASK 0b11111000

#define APU_SQUARE1_ENVELOPE_MISC 0x00
#define APU_SQUARE1_SWEEP 0x01
#define APU_SQUARE1_TIMERLOW 0x02
#define APU_SQUARE1_COUNTER_TIMERHIGH 0x03
#define APU_SQUARE2_ENVELOPE_MISC 0x04
#define APU_SQUARE2_SWEEP 0x05
#define APU_SQUARE2_TIMERLOW 0x06
#define APU_SQUARE2_COUNTER_TIMERHIGH 0x07
#define APU_TRIANGLE_COUNTER 0x08
#define APU_TRIANGLE_TIMERLOW 0x0A
#define APU_TRIANGLE_TIMERHIGH 0x0B
#define APU_NOISE_ENVELOPE 0x0C
#define APU_NOISE_LOOP 0x0E
#define APU_NOISE_COUNTER 0x0F
#define APU_DMC_LOOP 0x10
#define APU_DMC_DIRECTLOAD 0x11
#define APU_DMC_ADDRESS 0x12
#define APU_DMC_LENGTH 0x13
#define APU_CTRL 0x15
#define APU_FRAMECOUNTER 0x17

// TODO open bus, particularly the internal CPU open bus behavious when reading 0x4015

typedef struct APU {
	uint8_t registers[0x18]; // There are unused registers but this makes memory mapping easier
	uint32_t frameCounterDivider;
	bool irqOutFrame;
	bool irqOutDMC;

	uint8_t square1LengthCounter;
	uint8_t square2LengthCounter;
	uint8_t triangleLengthCounter;
	uint8_t noiseLengthCounter;

	uint8_t square1SweepDivider;
	uint8_t square2SweepDivider;
	bool reloadSquare1Sweep;
	bool reloadSquare2Sweep;

	uint16_t square1PeriodTimer;
	uint16_t square2PeriodTimer;
	uint16_t trianglePeriodTimer;

	uint8_t triangleLinearCounter;
	bool reloadTriangleLinearCounter;
	uint8_t triangleWaveformSequencer;

	double squareMixerLookup[31];
	double tndMixerLookup[203];
} APU;

// Interface functions
void initAPU(APU *apu);
uint8_t readRegisterAPU(APU *apu, uint16_t address);
void writeRegisterAPU(APU *apu, uint16_t address, uint8_t data);
void tickAPU(APU *apu);

// Non-interface functions
double mixChannels(APU *apu, uint8_t square1In, uint8_t square2In, uint8_t triangleIn, uint8_t noiseIn, uint8_t DMCIn);
void clockLengthCounters(APU *apu);
void clockSweepUnits(APU *apu);
void clockLinearCounter(APU *apu);
void clockEnvelopes(APU *apu);
uint16_t targetSweepPeriod(uint8_t sweepRegister, uint16_t currentPeriod, bool isSquare2);

#endif // ifndef APU_H