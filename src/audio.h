#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdint.h>

typedef struct AudioEngine {
	FILE *output;
	uint64_t sampleNumber;
} AudioEngine;

void initAudioEngine(AudioEngine *engine);
void terminateAudioEngine(AudioEngine *engine);
void newSamplef(AudioEngine *engine, float sample);

#endif // ifndef AUDIO_H