#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "graphics.h"

// Number of pixels on the x and y axies
#define HEIGHT_PIXELS 240
#define WIDTH_PIXELS 256

// Initial window dimensions
#define WINDOW_HEIGHT 1080
#define WINDOW_WIDTH 1920

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

int main() {
	printf("NESRev v3.6\n");

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

	const Context context = setupPixels(WIDTH_PIXELS, HEIGHT_PIXELS);
	if (context.status == false) {
		printf("Fatal error : couldn't set up communication with the shaders.\n");
		return -0x05;
	}

	// Array is dynamically allocated to avoid stack depletion and allow dynamic resolution
	float *colors = malloc(sizeof(float) * HEIGHT_PIXELS * WIDTH_PIXELS * COLOR_COMPONENTS);
	if (colors == NULL) {
		printf("Fatal error : couldn't allocate enough memory.\n");
		return 0x06;
	}

	while (!glfwWindowShouldClose(window)) {
		// Updates every pixel's color
		for (int i = 0; i < HEIGHT_PIXELS; i++) {
			for (int j = 0; j < WIDTH_PIXELS; j++) {
				colors[3 * (WIDTH_PIXELS * i + j) + 0] = (float)(i + 1) / HEIGHT_PIXELS;
				colors[3 * (WIDTH_PIXELS * i + j) + 1] = (float)(j + 1) / WIDTH_PIXELS;
				colors[3 * (WIDTH_PIXELS * i + j) + 2] = 0.0f;
			}
		}

		draw(context, WIDTH_PIXELS, HEIGHT_PIXELS, colors);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	terminate(context);
	glfwTerminate();

	return 0;
}