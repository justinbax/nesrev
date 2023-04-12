#include "audio.h"

const uint8_t baseHeader[44] = {
	'R', 'I', 'F', 'F',
	0x00, 0x00, 0x00, 0x00, // To change, size of rest of file in bytes (little endian)
	'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
	16, 0, 0, 0, 1, 0, 1, 0,
	0x4D, 0x4F, 0x1B, 0x00, // Sample rate in Hz (little endian). In this case, 1 789 773Hz
	0x9A, 0x9E, 0x36, 0x00, // Avg bytes per sample == sample rate * bits per sample * 1 channel / 8
	2, 0,
	0x10, 0x00, // Bits per sample (little endian). In this case, 16
	'd', 'a', 't', 'a',
	0x00, 0x00, 0x00, 0x00 // To change, size of rest of file (sample data) in bytes (little endian)
};

void initAudioEngine(AudioEngine *engine) {
	engine->sampleNumber = 0;
	engine->output = fopen("out.wav", "wb+");
	if (engine->output == NULL) {
		printf("Fatal error: couldn't open out.wav.\n");
	}
	fwrite(baseHeader, 1, 44, engine->output);
}

void terminateAudioEngine(AudioEngine *engine) {
	uint64_t dataSectionSize = 2 * engine->sampleNumber;
	uint8_t data[4];
	data[0] = dataSectionSize & 0xFF;
	data[1] = (dataSectionSize >> 8) & 0xFF;
	data[2] = (dataSectionSize >> 16) & 0xFF;
	data[3] = (dataSectionSize >> 24) & 0xFF;
	fseek(engine->output, 40, SEEK_SET);
	fwrite(data, 1, 4, engine->output);
	dataSectionSize += 36;
	data[0] = dataSectionSize & 0xFF;
	data[1] = (dataSectionSize >> 8) & 0xFF;
	data[2] = (dataSectionSize >> 16) & 0xFF;
	data[3] = (dataSectionSize >> 24) & 0xFF;
	fseek(engine->output, 4, SEEK_SET);
	fwrite(data, 1, 4, engine->output);
	fclose(engine->output);
}

void newSamplef(AudioEngine *engine, float sample) {
	float normalized = sample * 0x10000;
	uint8_t toWrite[2] = {(uint16_t)(normalized) & 0xFF, ((uint16_t)(normalized) >> 8) & 0xFF};
	fwrite(toWrite, 1, 2, engine->output);
	engine->sampleNumber++;
}