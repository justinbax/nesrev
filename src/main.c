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

int main() {
	printf("NESRev v3.6\n");

	GLFWwindow *window = createWindow(1920, 1080, "NESRev");
	if (window == NULL)
		return 0x01;

	int idShaderProgram = initShaders("src/shaders/vertexMain.glsl", "src/shaders/fragmentMain.glsl");
	if (idShaderProgram < 0)
		return 0x02;

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

	// Unbinds vertex buffer and vertex array objects
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// Array is dynamically allocated to allow dynamic resolution
	float *colors = malloc(sizeof(float) * HEIGHT_PIXELS * WIDTH_PIXELS * 3);

	// Main loop
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