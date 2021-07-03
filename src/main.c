#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

// Triangle vertices information
#define VERTEX_COUNT 4
#define VERTEX_SIZE 4
#define RGB_LENGTH 3
#define HEIGHT_PIXELS 3
#define WIDTH_PIXELS 4

// A polygon with n sides can always be dissected in (n - 2) triangles, so the number of indices for such a dissected
// polygon is its number of triangles multiplied by the number of vertices of a triangle, giving 3(n - 2)
#define INDICES_PER_POLYGON (VERTEX_COUNT - 2) * 3

void callbackErrorGL(GLenum source, GLenum type, GLenum id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
	printf("GL Callback 0x%X : %s (severity : 0x%X)\n", type, message, severity);
}

void callbackErrorGLFW(int code, const char *description) {
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
	
	int status;
	glGetShaderiv(idShader, GL_COMPILE_STATUS, &status);
	if (status == 0) {
		int msgLength;
		glGetShaderiv(idShader, GL_INFO_LOG_LENGTH, &msgLength);
		char *msg = malloc(msgLength);
		glGetShaderInfoLog(idShader, msgLength, &msgLength, msg);
		printf("GL Shader Compilation Error : %s\n", msg);
		free(msg);
	}

	// TODO not elegant
	free((char *)sourceShader);
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

	int status;
	glGetProgramiv(idProgramShader, GL_LINK_STATUS, &status);
	if (status == 0) {
		int msgLength;
		glGetProgramiv(idProgramShader, GL_INFO_LOG_LENGTH, &msgLength);
		char *msg = malloc(msgLength);
		glGetProgramInfoLog(idProgramShader, msgLength, &msgLength, msg);
		printf("GL Program Linking Error : %s\n", msg);
		free(msg);
	}

	glDeleteShader(idVertexShader);
	glDeleteShader(idProgramShader);
	return idProgramShader;
}

int main() {
	printf("NESRev v3.6\n");

	// Initializes GLFW
	glfwSetErrorCallback(callbackErrorGLFW);
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
	
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(callbackErrorGL, NULL);

	// Shader creation and compiling
	int idShaderProgram = createProgram("src/shaders/vertexMain.glsl", "src/shaders/fragmentMain.glsl");

	// Indices of vertices for two triangles forming the rectangle
	unsigned int indices[INDICES_PER_POLYGON * HEIGHT_PIXELS * WIDTH_PIXELS];

	// Vertex data, buffers and attributes set up
	// All rectangle vertices in normalized device coordinates and their corresponding rectangle index
	// TODO deal with VERTEX_SIZE
	float vertices[HEIGHT_PIXELS][WIDTH_PIXELS][VERTEX_COUNT][VERTEX_SIZE];
	float pixelWidthNormalized = 1.0f / WIDTH_PIXELS;
	float pixelHeightNormalized = 1.0f / HEIGHT_PIXELS;

	for (int i = 0; i < HEIGHT_PIXELS; i++) {
		for (int j = 0; j < WIDTH_PIXELS; j++) {
			int currentPixel = i * HEIGHT_PIXELS + j;
			for (int k = 0; k < VERTEX_COUNT; k++) {
				// TODO if possible, deal with repetition
				vertices[i][j][k][0] = pixelWidthNormalized * (j - 1); // x coordinate
				vertices[i][j][k][1] = pixelHeightNormalized * (i - 1); // y coordinate
				vertices[i][j][k][2] = 1.0f; // z coordinate
				vertices[i][j][k][3] = (float)currentPixel; // rectangle index
			}
			// For each rectangle, this fills the indices array with the correct indices to dissect it into two triangles
			// TODO this looks like magic
			indices[currentPixel * INDICES_PER_POLYGON] = currentPixel * INDICES_PER_POLYGON - 1;
			indices[currentPixel * INDICES_PER_POLYGON] = currentPixel * INDICES_PER_POLYGON;
			indices[currentPixel * INDICES_PER_POLYGON] = currentPixel * INDICES_PER_POLYGON + 1;
			indices[currentPixel * INDICES_PER_POLYGON] = currentPixel * INDICES_PER_POLYGON - 1;
			indices[currentPixel * INDICES_PER_POLYGON] = currentPixel * INDICES_PER_POLYGON + 1;
			indices[currentPixel * INDICES_PER_POLYGON] = currentPixel * INDICES_PER_POLYGON + 2;
		}
	}

	// Creates and binds a vertex array object containing a vertex buffer object and an element buffer object to pass vertex data
	unsigned int idVertexArray, idVertexBuffer, idElementBuffer;
	glGenVertexArrays(1, &idVertexArray);
	glGenBuffers(1, &idVertexBuffer);
	glGenBuffers(1, &idElementBuffer);

	glBindVertexArray(idVertexArray);

	// Sends vertex data
	glBindBuffer(GL_ARRAY_BUFFER, idVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Sends indices data
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idElementBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Specifies the interpretation of vertex data
	glUseProgram(idShaderProgram);
	unsigned int idPosition = glGetAttribLocation(idShaderProgram, "pos");
	unsigned int idIndex = glGetAttribLocation(idShaderProgram, "index");
	glVertexAttribPointer(idPosition, VERTEX_SIZE - 1, GL_FLOAT, false, VERTEX_SIZE * sizeof(float), (void *)0);
	glEnableVertexAttribArray(idPosition);
	glVertexAttribPointer(idIndex, 1, GL_FLOAT, true, VERTEX_SIZE * sizeof(float), (void *)((VERTEX_SIZE - 1) * sizeof(float)));
	glEnableVertexAttribArray(idIndex);

	// Unbinds vertex buffer and vertex array objects
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


	unsigned int idColors = glGetAttribLocation(idShaderProgram, "colors");
	float colors[HEIGHT_PIXELS * WIDTH_PIXELS];
	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);

		// Drawing
		glUseProgram(idShaderProgram);
		glBindVertexArray(idVertexArray);
		
		// Updates every pixel's color
		for (int i = 0; i < HEIGHT_PIXELS; i++) {
			for (int j = 0; j < WIDTH_PIXELS; j++) {
				colors[HEIGHT_PIXELS * i + j] = HEIGHT_PIXELS / i;
			}
		}
		// TODO the type casting is ugly
		glUniform1fv(idColors, HEIGHT_PIXELS * WIDTH_PIXELS, (const float *)&colors);

		glDrawElements(GL_TRIANGLES, (VERTEX_COUNT - 2) * 3, GL_UNSIGNED_INT, (void *)0);

		glBindVertexArray(0);

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