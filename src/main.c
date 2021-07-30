#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "graphics.h"

#define HEIGHT_PIXELS 240 // Number of pixels on the y axis
#define WIDTH_PIXELS 256 // Number of pixels on the x axis

// Texture unit used to send pixel data to the fragment shader
#define TEXTURE_UNIT 0

void callbackErrorGL(GLenum source, GLenum type, GLenum id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
	if (type == GL_DEBUG_TYPE_ERROR)
		printf("OpenGL Error 0x%X : %s (severity : 0x%X)\n", type, message, severity);
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

	GLFWwindow *window = glfwCreateWindow(1920, 1080, "NesREV", NULL, NULL);
	if (window == NULL) {
		printf("Fatal error : couldn't create window.\n");
		return -0x02;
	}
	glViewport(0, 0, 1920, 1080);
	glfwSetFramebufferSizeCallback(window, callbackFrameBufferSize);
	glfwMakeContextCurrent(window);

	// Initializes GLEW
	if (glewInit() != GLEW_OK) {
		printf("Fatal error : GLEW couldn't initialize correctly.\n");
		return -0x03;
	}
	
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(callbackErrorGL, NULL);

	int idShaderProgram = initShaders("src/shaders/vertexMain.vert", "src/shaders/fragmentMain.frag");
	if (idShaderProgram < 0) {
		printf("Fatal error : couldn't create shader program.\n");
		return -0x04;
	}

	unsigned int idVertexArray, idVertexBuffer, idElementBuffer, idTextureBuffer, idFrameTexture;
	glGenVertexArrays(1, &idVertexArray);
	glGenBuffers(1, &idVertexBuffer);
	glGenBuffers(1, &idElementBuffer);
	glGenBuffers(1, &idTextureBuffer);
	glGenTextures(1, &idFrameTexture);

	glBindVertexArray(idVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, idVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idElementBuffer);

	const int verticesCount = setupPixels(WIDTH_PIXELS, HEIGHT_PIXELS, glGetAttribLocation(idShaderProgram, "pos"), glGetAttribLocation(idShaderProgram, "textureSampler"), TEXTURE_UNIT);
	if (verticesCount < 0) {
		printf("Fatal error : couldn't send pixel data.\n");
		return -0x05;
	}

	// Unbinds vertex buffer and vertex array objects
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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

		// Drawing
		glUseProgram(idShaderProgram);
		glBindVertexArray(idVertexArray);
		draw(HEIGHT_PIXELS * WIDTH_PIXELS, colors, verticesCount, idTextureBuffer, idFrameTexture, TEXTURE_UNIT);
		glBindVertexArray(0);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	
	glDeleteVertexArrays(1, &idVertexArray);
	glDeleteBuffers(1, &idVertexBuffer);
	glDeleteBuffers(1, &idElementBuffer);
	glDeleteBuffers(1, &idTextureBuffer);
	glDeleteTextures(1, &idFrameTexture);
	glDeleteProgram(idShaderProgram);

	glfwTerminate();

	return 0;
}