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
	// Returns contents of text file in a stack-allocated char *
	FILE *input = fopen(path, "r");
	if (input == NULL)
		return NULL;

	// Allocates enough memory
	fseek(input, 0, SEEK_END);
	long int size = ftell(input);
	rewind(input);
	char *buffer = malloc(size * sizeof(char) + 1);

	fread(buffer, size * sizeof(char), 1, input);
	return buffer;
}

int createShader(const char *path, GLenum type) {
	// Creates and compiles a shader from a file
	unsigned int idShader = glCreateShader(type);
	const char *sourceShader = loadShader(path);
	glShaderSource(idShader, 1, &sourceShader, NULL);
	glCompileShader(idShader);
	// TODO not elegant
	free((char *)sourceShader);
	// TODO check for errors
	return idShader;
}

int createProgram(const char *vertexShaderPath, const char *fragmentShaderPath) {
	// Creates and compiles the vertex and fragment shaders
	unsigned int idVertexShader = createShader(vertexShaderPath, GL_VERTEX_SHADER);
	unsigned int idFragmentShader = createShader(fragmentShaderPath, GL_FRAGMENT_SHADER);

	// Creates and returns the final shader program ID
	unsigned int idProgramShader = glCreateProgram();
	glAttachShader(idProgramShader, idVertexShader);
	glAttachShader(idProgramShader, idFragmentShader);
	glLinkProgram(idProgramShader);
	// TODO query errors

	glDeleteShader(idVertexShader);
	glDeleteShader(idProgramShader);
	return idProgramShader;
}

int main() {
	printf("NESRev v3.6\n");

	// Initializes GLFW
	glfwSetErrorCallback(callbackError);
	if (!glfwInit())
		return 1;
	printf("GLFW initialized.\n");

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
	int idShaderProgram = createProgram("src/shaders/vertexMain.glsl", "src/shaders/fragmentMain.glsl");

	// Vertex data, buffers and attributes set up
	// Rectangle vertices in normalized device coordinates
	float rectangle[VERTEX_SIZE * VERTEX_COUNT] = {
		-0.9f, 0.9f, 0.0f,
		0.0f, 0.9f, 0.0f,
		0.0f, 0.0f, 0.0f,
		-0.9f, 0.0f, 0.0f
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
		glUseProgram(idShaderProgram);
		glBindVertexArray(idVertexArray);
		glDrawElements(GL_TRIANGLES, (VERTEX_COUNT - 2) * 3, GL_UNSIGNED_INT, (void *)0);

		glBindVertexArray(glGetAttribLocation(idShaderProgram, "pos"));

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &idVertexArray);
	glDeleteBuffers(1, &idVertexBuffer);
	glDeleteBuffers(1, &idElementBuffer);
	glDeleteProgram(idShaderProgram);

	glfwTerminate();

	return 0;
}