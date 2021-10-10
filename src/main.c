#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "graphics.h"
#include "cpu.h"
#include "ppu.h"
#include "ines.h"

// Number of pixels on the x and y axies
#define HEIGHT_PIXELS 240
#define WIDTH_PIXELS 256

// Initial window dimensions
#define WINDOW_HEIGHT HEIGHT_PIXELS * 4
#define WINDOW_WIDTH WIDTH_PIXELS * 4

void callbackErrorGL(GLenum source, GLenum type, GLenum id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
	if (type == GL_DEBUG_TYPE_ERROR)
		printf("OpenGL Error 0x%X from 0x%X : %s (severity : 0x%X)\n", type, id, message, severity);
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

	Cartridge cart;
	if (loadROMFromFile(&cart, argv[1]) != 0) {
		printf("Fatal error : couldn't load ROM.\n");
		return -0x09;
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

	GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "NesREV", NULL, NULL);
	if (window == NULL) {
		printf("Fatal error : couldn't create window.\n");
		return -0x02;
	}
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glfwSetFramebufferSizeCallback(window, callbackFrameBufferSize);
	glfwMakeContextCurrent(window);

	// Initializes GLEW
	if (glewInit() != GLEW_OK) {
		printf("Fatal error : GLEW couldn't initialize correctly.\n");
		return -0x03;
	}
	
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(callbackErrorGL, NULL);

	const Context context = setupContext(WIDTH_PIXELS, HEIGHT_PIXELS);
	if (context.status == false) {
		printf("Fatal error : couldn't set up shader communication with GPU.\n");
		return -0x05;
	}

	// Array is dynamically allocated to avoid stack depletion and allow dynamic resolution
	uint8_t *colors = malloc(sizeof(uint8_t) * HEIGHT_PIXELS * WIDTH_PIXELS * COLOR_COMPONENTS);
	if (colors == NULL) {
		printf("Fatal error : couldn't allocate enough memory.\n");
		return -0x06;
	}

	CPU cpu;
	PPU ppu;

	powerUpPPU(&ppu, colors, &cart);
	initCPU(&cpu, &ppu, false);

	// TODO very dangerous, just for test purposes
	// TODO unsigned and const things are just a mess
	unsigned char *palette = readFile("src/test.pal");
	loadPalette(&ppu, palette);
	free(palette);

	double previousTime = glfwGetTime();
	double frameDuration = 1.0f / 10;

	while (!glfwWindowShouldClose(window)) {
		// TODO very inefficient (no CPU sleep)
		double currentTime = glfwGetTime();
		if (currentTime > previousTime + frameDuration) {
			previousTime = currentTime;
			for (int i = 0; i < 80000; i++) {
				// TODO the CPU / PPU alignment is weird
				tickPPU(&ppu);
				tickPPU(&ppu);

				// PHI2
				cpu.NMIPin = ppu.outInterrupt;
				pollInterrupts(&cpu);

				tickPPU(&ppu);
				// PHI1
				tickCPU(&cpu);
			}
			
			draw(context, WIDTH_PIXELS, HEIGHT_PIXELS, colors);

			glfwSwapBuffers(window);
		}

		glfwPollEvents();
	}

	terminateContext(context);
	glfwTerminate();

	return 0;
}