#include "audio.h"

#include <stdio.h>


// Non-interface functions
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
	data->nextInBuffer++;
	return 0;
}

// Interface functions
void initAudioEngine(AudioEngine *engine) {
	engine->portaudioIsUp = false;
	engine->stream = NULL;

	engine->sampleCount = 0;
	engine->sampleSum = 0.0f;

	for (uint32_t i = 0; i < 65536; i++) {
		engine->data.buffer[i] = 0.0f;
	}
	engine->data.nextEmpty = 0;
	engine->data.nextInBuffer = 0;

	if (Pa_Initialize() != paNoError) {
		printf("Portaudio error: couldn't initialize.\n");
		return;
	}

	if (Pa_OpenDefaultStream(&engine->stream, 0, 1, paFloat32, SAMPLE_RATE, 1, portaudioCallback, &engine->data) != paNoError) {
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
	(engine->sampleCount)++;
	// TODO it's not really 40
	if (engine->sampleCount == 40) {
		engine->data.buffer[engine->data.nextEmpty] = engine->sampleSum / 40;
		engine->data.nextEmpty++;
		engine->sampleCount = 0;
		engine->sampleSum = 0.0f;
	}
}