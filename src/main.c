#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "graphics.h"
#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "ines.h"
#include "audio.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

// Number of pixels on the x and y axies
#define HEIGHT_PIXELS 240
#define WIDTH_PIXELS 256

#define WIN32_TIMERESOLUTION 2
#define UNIX_TIMERESOLUTION 2000

// Initial window dimensions
#define WINDOW_HEIGHT HEIGHT_PIXELS * 4
#define WINDOW_WIDTH WIDTH_PIXELS * 4

#ifndef NESREV_DEBUG
#define NESREV_DEBUG DBG_NONE
#endif // ifndef NESREV_DEBUG

void callbackErrorGL(GLenum source, GLenum type, GLenum id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
	if (type == GL_DEBUG_TYPE_ERROR) {
		printf("OpenGL Error 0x%X from 0x%X : %s (severity : 0x%X)\n", type, id, message, severity);
	}
}

void callbackErrorGLFW(int code, const char *description) {
	printf("GLFW Error %i : %s\n", code, description);
}

void callbackFrameBufferSize(GLFWwindow *window, int width, int height) {
	glViewport(0, 0, width, height);
}

int main(int argc, char *argv[]) {
	printf("NESRev v3.6\n");

	if (argc != 2) {
		printf("Usage : nesrev rom\n");
		return -0x08;
	}

	// To clarify why OpenGL / GLFW / GLUT setup isn't handled by an interface function :
	// The main function directly needs the GLFW window in order to process input and check if the main loop should continue.
	// I think it's therefore straightforward to expect main to create the window and handle it.
	// Also letting an interface function load two libraries and a graphical API without the callee's knowing sounds like a bad idea.
	// Finally, features like GL error callback handling should be easily disabled, which becomes much more chaotic through an interface.

	// Creates a GLFW window with and OpenGL context
	glfwSetErrorCallback(callbackErrorGLFW);
	if (!glfwInit()) {
		printf("Fatal error : couldn't load GLFW.\n");
		return -0x01;
	}

	GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "NESRev v3.6", NULL, NULL);
	if (window == NULL) {
		printf("Fatal error : couldn't create window.\n");
		glfwTerminate();
		return -0x02;
	}
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glfwSetFramebufferSizeCallback(window, callbackFrameBufferSize);
	glfwMakeContextCurrent(window);

	// Initializes GLEW
	if (glewInit() != GLEW_OK) {
		printf("Fatal error : GLEW couldn't initialize correctly.\n");
		glfwTerminate();
		return -0x03;
	}
	
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(callbackErrorGL, NULL);

	const Context context = setupContext(WIDTH_PIXELS, HEIGHT_PIXELS);
	if (context.status == false) {
		printf("Fatal error : couldn't set up shader communication with GPU.\n");
		glfwTerminate();
		return -0x05;
	}

	// Array is dynamically allocated to avoid stack depletion and allow dynamic resolution
	uint8_t *colors = malloc(sizeof(uint8_t) * HEIGHT_PIXELS * WIDTH_PIXELS * COLOR_COMPONENTS);
	if (colors == NULL) {
		printf("Fatal error : couldn't allocate enough memory.\n");
		terminateContext(context);
		glfwTerminate();
		return -0x06;
	}

	// Default key scancodes for standard controller setup
	int keys[8] = {
		GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, // A, B mapped to Space, LShift
		GLFW_KEY_BACKSPACE, GLFW_KEY_ENTER, // SELECT, START mapped to Backspace, Enter
		GLFW_KEY_W, GLFW_KEY_S, // UP, DOWN mapped to W, S
		GLFW_KEY_A, GLFW_KEY_D // LEFT, RIGHT mapped to A, D
	};

	// Initialization of all components
	Bus bus;
	CPU cpu;
	PPU ppu;
	APU apu;
	Port ports[2];
	Cartridge cart;
	initBus(&bus, &cpu, &ppu, &apu, ports, &cart);
	initCPU(&cpu, &bus);
	initPPU(&ppu, colors, &bus);
	initAPU(&apu);
	initPort(&ports[0], PORT_STDCONTROLLER, window, keys, 8);
	initPort(&ports[1], PORT_NONE, window, NULL, 0);

	AudioEngine engine;
	initAudioEngine(&engine);

	if (loadROMFromFile(&cart, argv[1], true) != 0) {
		printf("Fatal error : couldn't load ROM.\n");
		terminateContext(context);
		terminateAudioEngine(&engine);
		glfwTerminate();
		return -0x09;
	}

	// Debug logging
	// With the provided Makefile, NESREV_DEBUG is always defined according to an environment variable
	// However, even by default, NESREV_DEBUG will be set to DBG_NONE
	FILE *logFile = NULL;
	if (NESREV_DEBUG != DBG_NONE) {
		logFile = fopen("log.txt", "w+");
		if (logFile == NULL) {
			printf("Error : can't open / create log file.\n");
			// TODO fix this (#define is """called""" regardless of the execution of the code block)
			#define NESREV_DEBUG DBG_NONE
		}
	}
	setLogCPU(&cpu, NESREV_DEBUG, logFile);

	// Palette information is stored in .pal files (no header, 3-byte RGB for each of 64 palette colors)
	FILE *paletteFile = fopen("default.pal", "r");
	uint8_t palette[0x40 * 3];
	if (fread(palette, sizeof(uint8_t), 0x40 * 3, paletteFile) != 0x40 * 3) {
		printf("Fatal error : corrupted default palette file (default.pal).\n");
		if (paletteFile != NULL) {
			fclose(paletteFile);
		}

		if (logFile != NULL) {
			fclose(logFile);
		}
		free(colors);
		freeCartridge(&cart);
		terminateContext(context);
		glfwTerminate();
		return -0x04;
	}
	
	if (paletteFile != NULL) {
		fclose(paletteFile);
	}

	loadPalette(&ppu, palette);

	double frameDuration = 1.0f / 60;
	double frameStart = glfwGetTime();

#ifdef _WIN32
	timeBeginPeriod(WIN32_TIMERESOLUTION);
#endif

	startStream(&engine);

	while (!glfwWindowShouldClose(window)) {

		if (glfwGetTime() - frameStart >= frameDuration) {
			frameStart = glfwGetTime();
			for (int i = 0; i < 29781 - (ppu.oddFrame); i++) {
				// TODO the CPU / PPU alignment is weird
				tickPPU(&ppu);
				tickPPU(&ppu);
				// PHI2
				cpu.NMIPin = ppu.outInterrupt;
				cpu.IRQPin = !(apu.irqOutDMC || apu.irqOutFrame);
				// TODO IRQ is broken
				pollInterrupts(&cpu);
				tickPPU(&ppu);
				// PHI1
				tickCPU(&cpu);
				tickAPU(&apu);
				newSamplef(&engine, apu.currentSample);
			}

			draw(context, WIDTH_PIXELS, HEIGHT_PIXELS, colors);
			glfwSwapBuffers(window);
			glfwPollEvents();

#ifdef _WIN32
			while (frameDuration - (glfwGetTime() - frameStart) > 4.0f / 1000) {
				// Sleep by 2ms intervals while there is less than 4ms to wait
				// TODO make this variable
				Sleep(WIN32_TIMERESOLUTION);
			}
#else
			while (frameDuration - (glfwGetTime() - frameStart) > 4.0f / 1000) {
				struct timespec sleepTime;
				sleepTime.tv_sec = 0;
				sleepTime.tv_nsec = UNIX_TIMERESOLUTION;
				// For now, same as Windows
				nanosleep(&sleepTime, NULL);
			}
#endif
		}
	}

	stopStream(&engine);

#ifdef _WIN32
	timeEndPeriod(WIN32_TIMERESOLUTION);
#endif

	if (logFile != NULL) {
		fclose(logFile);
	}

	free(colors);
	freeCartridge(&cart);

	terminateAudioEngine(&engine);

	terminateContext(context);
	glfwTerminate();

	return 0;
}