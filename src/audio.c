#include "audio.h"

#include <stdio.h>


// Non-interface functions
float mod(uint64_t dividend, float divisor) {
	int integerQuotient = dividend / divisor;
	return dividend - integerQuotient * divisor;
}

void terminatePortaudio(AudioEngine *engine, const char *file, int line) {
	if (line > 0) {
		printf("Portaudio error from file %s:%i.\n", file, line);
	}
	engine->portaudioIsUp = false;
	PaError error = Pa_Terminate();
	if (error != paNoError) {
		printf("Portaudio error while terminating: %s\n", Pa_GetErrorText(error));
	}
}

int portaudioCallback(const void *input, void *output, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags status, void *userData) {
	AudioData *data = (AudioData *)userData;
	float *out = (float *)output;
	*out = data->buffer[data->nextInBuffer];
	if (data->nextEmpty != data->nextInBuffer) {
		data->nextInBuffer++;
	}
	return 0;
}

// Interface functions
void initAudioEngine(AudioEngine *engine) {
	engine->portaudioIsUp = false;
	engine->stream = NULL;

	engine->currentSampleCount = 0;
	engine->sampleSum = 0.0f;
	engine->samplesDownsampled = 0;
	engine->totalSourceSamples = 0;
	engine->nextResampling = 40;

	for (uint32_t i = 0; i < 65536; i++) {
		engine->data.buffer[i] = 0.0f;
	}
	engine->data.nextEmpty = 0;
	engine->data.nextInBuffer = 0;

	if (Pa_Initialize() != paNoError) {
		printf("Portaudio error: couldn't initialize.\n");
		return;
	}

	if (Pa_OpenDefaultStream(&engine->stream, 0, 1, paFloat32, TARGET_SAMPLE_RATE, 1, portaudioCallback, &engine->data) != paNoError) {
		terminatePortaudio(engine, __FILE__, __LINE__);
	}
	engine->portaudioIsUp = true;
}

void terminateAudioEngine(AudioEngine *engine) {
	if (Pa_CloseStream(engine->stream) != paNoError) {
		terminatePortaudio(engine, __FILE__, __LINE__);
		return;
	}
	engine->portaudioIsUp = false;
	terminatePortaudio(engine, NULL, 0);
}

void startStream(AudioEngine *engine) {
	if (Pa_StartStream(engine->stream) != paNoError) {
		terminatePortaudio(engine, __FILE__, __LINE__);
	}
}

void stopStream(AudioEngine *engine) {
	if (Pa_StopStream(engine->stream) != paNoError) {
		terminatePortaudio(engine, __FILE__, __LINE__);
	}
}

void newSamplef(AudioEngine *engine, float sample) {
	engine->sampleSum += sample;
	engine->currentSampleCount++;
	engine->totalSourceSamples++;

	if (engine->currentSampleCount >= engine->nextResampling) {
		engine->data.buffer[engine->data.nextEmpty] = engine->sampleSum / engine->currentSampleCount;
		engine->samplesDownsampled++;

		// TODO remove this magic number
		// Source sample rate = 1786830Hz
		// Target sample rate = 44100Hz
		// Downsampling factor: 1789830Hz / 44100Hz == 40.517687...
		// So every 0.517687... target sample, we want to resample using 41 source samples instead of 40.
		// This is equivalent to every 1 / 0.517687... == 1.931668857... target sample, use 41 source samples instead of 40.
		engine->nextResampling = (mod(engine->samplesDownsampled, 1.931668857f) >= 1 ? 40 : 41);

		engine->data.nextEmpty++;
		engine->currentSampleCount = 0;
		engine->sampleSum = 0.0f;
	}
}