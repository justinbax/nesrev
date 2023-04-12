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

// Non-interface functions
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
	if (!(apu->registers[APU_NOISE_ENVELOPE] & 0b00100000) && apu->noiseLengthCounter > 0 && NOISENABLED(apu)) {
		apu->noiseLengthCounter--;
	}
}

void clockSweepUnits(APU *apu) {
	// TODO also mute the square channels in the mixer according to sweep quirks
	if (apu->square1SweepDivider == 0) {
		apu->square1SweepDivider = (apu->registers[APU_SQUARE1_SWEEP] & 0b01110000) >> 4 + 1;
		uint16_t newPeriod = targetSweepPeriod(apu->registers[APU_SQUARE1_SWEEP], apu->square1PeriodTimer, false);
		if (apu->registers[APU_SQUARE1_SWEEP] & 0b10000000 && !(SQUARE1PERIOD(apu) || newPeriod > 0x7FF)) {
			apu->registers[APU_SQUARE1_TIMERLOW] = newPeriod & 0xFF;
			apu->registers[APU_SQUARE1_COUNTER_TIMERHIGH] &= 0b00000111;
			apu->registers[APU_SQUARE1_COUNTER_TIMERHIGH] |= newPeriod >> 8;
		}
	}
	if (apu->square2SweepDivider == 0) {
		apu->square2SweepDivider = (apu->registers[APU_SQUARE2_SWEEP] & 0b01110000) >> 4 + 1;
		uint16_t newPeriod = targetSweepPeriod(apu->registers[APU_SQUARE2_SWEEP], apu->square2PeriodTimer, true);
		if (apu->registers[APU_SQUARE2_SWEEP] & 0b10000000 && !(SQUARE2PERIOD(apu) || newPeriod > 0x7FF)) {
			apu->registers[APU_SQUARE2_TIMERLOW] = newPeriod & 0xFF;
			apu->registers[APU_SQUARE2_COUNTER_TIMERHIGH] &= 0b00000111;
			apu->registers[APU_SQUARE2_COUNTER_TIMERHIGH] |= newPeriod >> 8;
		}
	}

	apu->square1SweepDivider--;
	apu->square2SweepDivider--;
}

void clockLinearCounter(APU *apu) {
	apu->triangleLinearCounter -= (apu->triangleLinearCounter != 0);
	apu->triangleLinearCounter *= (apu->registers[APU_TRIANGLE_COUNTER] >> 7);
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
	for (uint8_t *p = apu->registers; p <= &apu->registers[0x17]; p++) {
		*p = 0x00;
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
		// TODO do the interrupts
		return (square1On) | (square1On << 1) | (triangleOn << 2) | (noiseOn << 3) | (dmcOn << 4);
	}
	return 0x00;
}

void writeRegisterAPU(APU *apu, uint16_t address, uint8_t data) {
	apu->registers[address & 0x1F] = data;
	if (address == 0x4003) {

	} // then do the same for 4007
	// TODO 4001-4005: reload sweep dividers with (P+1)
}

void tickAPU(APU *apu) {
	if (apu->frameCounterDivider >= 28828 && !(apu->registers[APU_FRAMECOUNTER] & 0b01000000) && !FRAMECOUNTER_5STEP(apu)) {
		apu->irqOut = true;
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
}

#undef FRAMECOUNTER_5STEP
// TODO undefine all things on top