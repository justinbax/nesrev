#include "apu.h"

#include <stdbool.h>

// Undefined later
#define FRAMECOUNTER_5STEP(apu) ((apu)->registers[APU_FRAMECOUNTER] & 0b10000000)
#define SQUARE1ENABLED(apu) ((apu)->registers[APU_CTRL] & 0b00000001)
#define SQUARE2ENABLED(apu) ((apu)->registers[APU_CTRL] & 0b00000010)
#define TRIANGLEENABLED(apu) ((apu)->registers[APU_CTRL] & 0b00000100)
#define NOISEENABLED(apu) ((apu)->registers[APU_CTRL] & 0b00001000)
#define DMCENABLED(apu) ((apu)->registers[APU_CTRL] & 0b00010000)
#define SQUARE1PERIOD(apu) (((uint16_t)(apu)->registers[APU_SQUARE1_COUNTER_TIMERHIGH] & 0b00000111) << 8 | (apu)->registers[APU_SQUARE1_TIMERLOW])
#define SQUARE2PERIOD(apu) (((uint16_t)(apu)->registers[APU_SQUARE2_COUNTER_TIMERHIGH] & 0b00000111) << 8 | (apu)->registers[APU_SQUARE2_TIMERLOW])
#define TRIANGLEPERIOD(apu) (((uint16_t)(apu)->registers[APU_TRIANGLE_TIMERHIGH] & 0b00000111) << 8 | (apu)->registers[APU_TRIANGLE_TIMERLOW])

const uint8_t lengthCounterLookup[0x20] = {
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06, 0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16, 0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

const uint8_t triangleWaveform[0x20] = {
	15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

// Non-interface functions
double mixChannels(APU *apu, uint8_t square1In, uint8_t square2In, uint8_t triangleIn, uint8_t noiseIn, uint8_t DMCIn) {
	double squareOut = apu->squareMixerLookup[square1In + square2In];
	double tndOut = apu->tndMixerLookup[3 * triangleIn + 2 * noiseIn + DMCIn];
	double output = squareOut + tndOut;
	return output;
}

void clockLengthCounters(APU *apu) {
	// TODO do we really need to check if they're enabled
	if (!(apu->registers[APU_SQUARE1_ENVELOPE_MISC] & 0b00100000) && apu->square1LengthCounter > 0 && SQUARE1ENABLED(apu)) {
		apu->square1LengthCounter--;
	}
	if (!(apu->registers[APU_SQUARE2_ENVELOPE_MISC] & 0b00100000) && apu->square2LengthCounter > 0 && SQUARE2ENABLED(apu)) {
		apu->square2LengthCounter--;
	}
	if (!(apu->registers[APU_TRIANGLE_COUNTER] & 0b10000000) && apu->triangleLengthCounter > 0 && TRIANGLEENABLED(apu)) {
		apu->triangleLengthCounter--;
	}
	if (!(apu->registers[APU_NOISE_ENVELOPE] & 0b00100000) && apu->noiseLengthCounter > 0 && NOISEENABLED(apu)) {
		apu->noiseLengthCounter--;
	}
}

void clockSweepUnits(APU *apu) {
	// TODO also mute the square channels in the mixer according to sweep quirks
	// Square 1
	uint16_t currentPeriod = SQUARE1PERIOD(apu);
	uint16_t newPeriod = targetSweepPeriod(apu->registers[APU_SQUARE1_SWEEP], currentPeriod, false);
	if (apu->square1SweepDivider == 0 && apu->registers[APU_SQUARE1_SWEEP] & 0b10000000 && !(currentPeriod < 8 || newPeriod > 0x7FF)) {
		// Set new period
		apu->registers[APU_SQUARE1_TIMERLOW] = newPeriod & 0xFF;
		apu->registers[APU_SQUARE1_COUNTER_TIMERHIGH] &= 0b00000111;
		apu->registers[APU_SQUARE1_COUNTER_TIMERHIGH] |= newPeriod >> 8;
	}
	if (apu->square1SweepDivider == 0 || apu->reloadSquare1Sweep) {
		// Reload divider
		apu->square1SweepDivider = ((apu->registers[APU_SQUARE1_SWEEP] & 0b01110000) >> 4) + 1;
	}

	// Square 2
	currentPeriod = SQUARE2PERIOD(apu);
	newPeriod = targetSweepPeriod(apu->registers[APU_SQUARE2_SWEEP], currentPeriod, true);
	if (apu->square2SweepDivider == 0 && apu->registers[APU_SQUARE2_SWEEP] & 0b10000000 && !(currentPeriod < 8 || newPeriod > 0x7FF)) {
		// Set new period
		apu->registers[APU_SQUARE2_TIMERLOW] = newPeriod & 0xFF;
		apu->registers[APU_SQUARE2_COUNTER_TIMERHIGH] &= 0b00000111;
		apu->registers[APU_SQUARE2_COUNTER_TIMERHIGH] |= newPeriod >> 8;
	}
	if (apu->square1SweepDivider == 0 || apu->reloadSquare1Sweep) {
		// Reload divider
		apu->square2SweepDivider = ((apu->registers[APU_SQUARE2_SWEEP] & 0b01110000) >> 4) + 1;
	}

	apu->square1SweepDivider--;
	apu->square2SweepDivider--;
}

void clockLinearCounter(APU *apu) {
	// Decrement if non-zero
	apu->triangleLinearCounter -= (apu->triangleLinearCounter != 0);
	// Reload if apu->reloadTriangleLinearCounter is set
	apu->triangleLinearCounter *= !apu->reloadTriangleLinearCounter;
	apu->triangleLinearCounter += (apu->registers[APU_TRIANGLE_COUNTER & 0b01111111]) * apu->reloadTriangleLinearCounter;
	// Clear reload flag if necessary
	apu->reloadTriangleLinearCounter = (apu->registers[APU_TRIANGLE_COUNTER] >> 7) == 1;
}

void clockEnvelopes(APU *apu) {
	// TODO
}

uint16_t targetSweepPeriod(uint8_t sweepRegister, uint16_t currentPeriod, bool isSquare2) {
	uint16_t toAdd = currentPeriod >> (sweepRegister & 0b00000111);
	bool negate = (sweepRegister & 0b00001000);
	toAdd *= 2 * negate - 1;
	toAdd -= negate && !isSquare2;
	return currentPeriod + isSquare2;
}

// Interface functions
void initAPU(APU *apu) {
	for (int i = 0; i < 0x18; i++) {
		apu->registers[i] = 0x00;
	}

	apu->frameCounterDivider = 0;
	apu->irqOutFrame = apu->irqOutDMC = false;
	apu->square1LengthCounter = apu->square2LengthCounter = apu->triangleLengthCounter = apu->noiseLengthCounter = 0;
	apu->square1SweepDivider = apu->square2SweepDivider = 0;
	apu->reloadSquare1Sweep = apu->reloadSquare2Sweep = false;
	apu->square1PeriodTimer = apu->square2PeriodTimer = apu->trianglePeriodTimer = 0;
	apu->triangleLinearCounter = 0;
	apu->reloadTriangleLinearCounter = false;
	apu->triangleWaveformSequencer = 0;

	apu->squareMixerLookup[0] = apu->tndMixerLookup[0] = 0.0f;
	for (int i = 1; i < 31; i++) {
		apu->squareMixerLookup[i] = 95.52f / (8128.0f / i + 100);
	}

	for (int i = 1; i < 203; i++) {
		apu->tndMixerLookup[i] = 163.67f / (24329.0f / i + 100);
	}
}

uint8_t readRegisterAPU(APU *apu, uint16_t address) {
	// TODO open bus (probably will replace apu by bus in parameters)
	if (address == 0x4015) {
		bool dmcOn = apu->registers[APU_DMC_LENGTH] > 0; // TODO check if that's correct
		bool noiseOn = (apu->registers[APU_NOISE_COUNTER] & APU_LENGTHCOUNTER_MASK) > 0;
		bool triangleOn = (apu->registers[APU_TRIANGLE_COUNTER] & APU_LENGTHCOUNTER_MASK) > 0;
		bool square2On = (apu->registers[APU_SQUARE2_COUNTER_TIMERHIGH] & APU_LENGTHCOUNTER_MASK) > 0;
		bool square1On = (apu->registers[APU_SQUARE1_COUNTER_TIMERHIGH] & APU_LENGTHCOUNTER_MASK) > 0;
		bool frameInterrupt = apu->irqOutFrame;
		bool DMCInterrupt = apu->irqOutDMC;
		apu->irqOutFrame = false;
		// TODO if read the same cycle it is set, do not clear the flags
		return (square1On) | (square2On << 1) | (triangleOn << 2) | (noiseOn << 3) | (dmcOn << 4) | (frameInterrupt << 6) | (DMCInterrupt << 7);
	}
	return 0x00;
}

void writeRegisterAPU(APU *apu, uint16_t address, uint8_t data) {
	apu->registers[address & 0x1F] = data;
	switch (address & 0x1F) {
		case 0x00:
			// TODO duty cycle is changed
			break;
		case 0x01:
			apu->reloadSquare1Sweep = true;
			break;
		case 0x03:
			// TODO sequencer and envelope are restarted
			if (SQUARE1ENABLED(apu)) {
				apu->square1LengthCounter = lengthCounterLookup[data >> 3];
			}
			break;
		case 0x04:
			// TODO duty cycle is changed
			break;
		case 0x05:
			apu->reloadSquare2Sweep = true;
			break;
		case 0x07:
			// TODO sequencer and envelope are restarted
			if (SQUARE2ENABLED(apu)) {
				apu->square2LengthCounter = lengthCounterLookup[data >> 3];
			}
			break;
		case 0x0B:
			if (TRIANGLEENABLED(apu)) {
				apu->reloadTriangleLinearCounter = true;
				apu->triangleLengthCounter = lengthCounterLookup[data >> 3];
			}
			break;
		case 0x0F:
			// TODO restart envelope
			if (NOISEENABLED(apu)) {
				apu->noiseLengthCounter = lengthCounterLookup[data >> 3];
			}
			break;
		case 0x15:
			apu->irqOutDMC = false;
			break;
	}
}

void tickAPU(APU *apu) {
	if (apu->frameCounterDivider >= 28828 && !(apu->registers[APU_FRAMECOUNTER] & 0b01000000) && !FRAMECOUNTER_5STEP(apu)) {
		apu->irqOutFrame = true;
	}
	if (apu->frameCounterDivider == 14913) {
		clockLengthCounters(apu);
		clockSweepUnits(apu);
	}
	if (apu->frameCounterDivider == 7457 || apu->frameCounterDivider == 14913 || apu->frameCounterDivider == 22371) {
		// TODO Clock envelopes and triangle linear counter
	}
	if ((apu->frameCounterDivider == 29829 && !FRAMECOUNTER_5STEP(apu)) || (apu->frameCounterDivider == 37281 && FRAMECOUNTER_5STEP(apu))) {
		// TODO Clock envelopes and triangle linear counter
		clockLengthCounters(apu);
		clockSweepUnits(apu);
	}
	apu->frameCounterDivider++;

	// TODO clock square channels and noise and DMC
	uint8_t square1Output = 0;
	uint8_t square2Output = 0;
	uint8_t triangleOutput = 0;
	uint8_t noiseOutput = 0;
	uint8_t DMCOutput = 0;

	// Clocks triangle channel every CPU clock
	if (apu->trianglePeriodTimer == 0) {
		apu->trianglePeriodTimer = TRIANGLEPERIOD(apu) + 1;
		if (apu->triangleLengthCounter && apu->triangleLinearCounter) {
			apu->triangleWaveformSequencer++;
		}
	}
	apu->trianglePeriodTimer--;
	triangleOutput = triangleWaveform[apu->triangleWaveformSequencer & 0x1F];

	double output = mixChannels(apu, square1Output, square2Output, triangleOutput, noiseOutput, DMCOutput);
}

#undef FRAMECOUNTER_5STEP
// TODO undefine all things on top