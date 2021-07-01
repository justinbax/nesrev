#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

// Triangle vertices information
#define VERTEX_COUNT 4
#define VERTEX_SIZE 3

void callbackError(int code, const char *description) {
	printf("GLFW Error %i : %s\n", code, description);
}

void callbackFrameBufferSize(GLFWwindow *window, int width, int height) {
	glViewport(0, 0, width, height);
}

char *loadShader(const char *path) {
	FILE *input = fopen(path, "r");
	if (input == NULL)
		return NULL;

	// Allocates enough memory
	fseek(input, 0, SEEK_END);
	long int size = ftell(input);
	rewind(input);
	char *buffer = malloc(size * sizeof(char));
	// TODO check if one more byte is needed for null termination with fread

	fread(buffer, size * sizeof(char), 1, input);
	return buffer;
}

int main() {
	printf("NESRev v3.6\n");

	// Initializes GLFW
	if (!glfwInit())
		return 1;
	printf("GLFW initialized.\n");

	glfwSetErrorCallback(callbackError);

	// Creates a window and an OpenGL context
	GLFWwindow *window = glfwCreateWindow(1920, 1080, "NESRev v3.6", NULL, NULL);
	if (window == NULL)
		return 1;
	glViewport(0, 0, 1920, 1080);
	glfwSetFramebufferSizeCallback(window, callbackFrameBufferSize);
	glfwMakeContextCurrent(window);

	// Initializes GLEW
	if (glewInit() != GLEW_OK)
		return 2;
	printf("GLEW initialized.\n");
	

	// Shader creation and compiling
	// Creates and compiles the vertex and fragment shaders
	const char *sourceVertexShader = loadShader("src/shaders/vertexMain.glsl");
	// TODO make sure it is always freed
	// TODO make it more elegant
	unsigned int idVertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(idVertexShader, 1, &sourceVertexShader, NULL);
	glCompileShader(idVertexShader);
	// TODO this isn't elegant
	free((char *)sourceVertexShader);
	// TODO query errors

	const char *sourceFragmentShader = loadShader("src/shaders/vertexMain.glsl");
	// TODO make sure it is always freed
	// TODO make it more elegant
	unsigned int idFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(idFragmentShader, 1, &sourceFragmentShader, NULL);
	glCompileShader(idFragmentShader);
	// TODO this isn't elegant
	free((char *)sourceFragmentShader);
	// TODO query errors

	// Creates the final shader program
	unsigned int idProgramShader = glCreateProgram();
	glAttachShader(idProgramShader, idVertexShader);
	glAttachShader(idProgramShader, idFragmentShader);
	glLinkProgram(idProgramShader);
	// TODO query errors
	glDeleteShader(idVertexShader);
	glDeleteShader(idFragmentShader);


	// Vertex data, buffers and attributes set up
	// Rectangle vertices in normalized device coordinates
	float rectangle[VERTEX_SIZE * VERTEX_COUNT] = {
		-1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f
	};

	// Indices of vertices for two triangles forming the rectangle
	// A polygon with n sides can always be dissected in (n - 2) triangles, so the number of indices for such a dissected polygon is
	// its number of triangles multiplied by the number of vertices of a triangle, giving 3(n - 2)
	unsigned int indices[(VERTEX_COUNT - 2) * 3] = {
		0, 1, 2,
		0, 2, 3
	};

	// Creates and binds a vertex array object containing a vertex buffer object and an element buffer object to pass vertex data
	unsigned int idVertexArray, idVertexBuffer, idElementBuffer;
	glGenVertexArrays(1, &idVertexArray);
	glGenBuffers(1, &idVertexBuffer);
	glGenBuffers(1, &idElementBuffer);

	glBindVertexArray(idVertexArray);

	// Sends vertex data
	glBindBuffer(GL_ARRAY_BUFFER, idVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangle), rectangle, GL_STATIC_DRAW);

	// Sends indices data
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idElementBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Specifies the interpretation of vertex data
	glVertexAttribPointer(0, VERTEX_SIZE, GL_FLOAT, false, VERTEX_SIZE * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	// Unbinds vertex buffer and vertex array objects
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);

		// Drawing
		glUseProgram(idProgramShader);
		glBindVertexArray(idVertexArray);
		glDrawElements(GL_TRIANGLES, (VERTEX_COUNT - 2) * 3, GL_UNSIGNED_INT, (void *)0);

		glBindVertexArray(glGetAttribLocation(idProgramShader, "pos"));

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &idVertexArray);
	glDeleteBuffers(1, &idVertexBuffer);
	glDeleteBuffers(1, &idElementBuffer);
	glDeleteProgram(idProgramShader);

	glfwTerminate();

	return 0;
}