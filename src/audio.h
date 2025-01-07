#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#include "Portaudio/portaudio.h"

#define TARGET_SAMPLE_RATE 44100

typedef struct AudioData {
	uint16_t nextInBuffer;
	uint16_t nextEmpty;
	float buffer[65536];
} AudioData;

typedef struct AudioEngine {
	PaStream *stream;
	AudioData data;
	bool portaudioIsUp;
	float sampleSum;
	int currentSampleCount;
	uint64_t samplesDownsampled;
	uint64_t totalSourceSamples;
	int nextResampling;
} AudioEngine;

// Interface functions
void initAudioEngine(AudioEngine *engine);
void terminateAudioEngine(AudioEngine *engine);
void startStream(AudioEngine *engine);
void stopStream(AudioEngine *engine);
void newSamplef(AudioEngine *engine, float sample);

#ifdef NESREV_NOAUDIO
// If compiling without audio engine, define methods as empty
void initAudioEngine(AudioEngine *engine) {}
void terminateAudioEngine(AudioEngine *engine) {}
void startStream(AudioEngine *engine) {}
void stopStream(AudioEngine *engine) {}
void newSamplef(AudioEngine *engine, float sample) {}
#endif //ifdef NESREV_NOAUDIO

#endif // ifndef AUDIO_H