#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "graphics.h"

// Triangle vertices information
#define VERTEX_COUNT 4 // Number of vertices to drawn polygons. For rectangles (representing pixels), this is 4
#define VERTEX_SIZE 2 // Number of components to a position (in 2D space with x and y coordinates, this is 2).
// While OpenGL works in 3D space, VERTEX_SIZE is kept as 2 because all position vectors have a constant 1.0f as z component. We can therefore pass a 2D position to the GPU and leave it to the vertex shader to add the z component

// A polygon with n sides can always be dissected in (n - 2) triangles, so the number of indices for such a dissected
// polygon is its number of triangles multiplied by the number of vertices of a triangle, giving 3(n - 2)
#define INDICES_PER_POLYGON (VERTEX_COUNT - 2) * 3

#define TEXTURE_UNIT 0 // Texture unit used to send pixel data to the fragment shader

const char *readFile(const char *path) {
	// Returns contents of text file in a stack-allocated char *. The returned pointer has to be deallocated with free by callee
	// File is opened in binary mode to avoid problems with reading CRLF (2 chars) when getting file size but only reading LF (1 char) when calling fread
	FILE *input = fopen(path, "rb");
	if (input == NULL)
		return NULL;

	// Allocates enough memory
	fseek(input, 0, SEEK_END);
	long int size = ftell(input);
	rewind(input);
	char * buffer = malloc((size + 1) * sizeof(char));
	if (buffer == NULL) {
		fclose(input);
		return NULL;
	}

	fread(buffer, size * sizeof(char), 1, input);
	buffer[size] = '\0';
	fclose(input);
	return buffer;
}

long int createShader(const char *path, GLenum type) {
	// Creates and compiles a shader from a file
	unsigned int idShader = glCreateShader(type);
	const char *sourceShader = readFile(path);
	if (sourceShader == NULL)
		return -0x01;
	glShaderSource(idShader, 1, &sourceShader, NULL);
	glCompileShader(idShader);
	// TODO this is not elegant, but currently needed
	// glShaderSource doesn't accept non-const strings as source GLSL, but free doesn't accept pointers to const types, so one has to be cast to the other
	free((char *)sourceShader);

	// Error handling
	int status;
	glGetShaderiv(idShader, GL_COMPILE_STATUS, &status);
	if (status == 0) {
		int msgLength;
		glGetShaderiv(idShader, GL_INFO_LOG_LENGTH, &msgLength);
		char *msg = malloc(msgLength);
		glGetShaderInfoLog(idShader, msgLength, &msgLength, msg);
		printf("GL Shader Compilation Error : %s\n", msg);
		free(msg);
		return -0x02;
	}
	return idShader;
}

long int createProgram(unsigned int idVertexShader, unsigned int idFragmentShader) {
	// Creates and returns the final shader program ID
	unsigned int idProgramShader = glCreateProgram();
	glAttachShader(idProgramShader, idVertexShader);
	glAttachShader(idProgramShader, idFragmentShader);
	glLinkProgram(idProgramShader);

	// Error handling
	int status;
	glGetProgramiv(idProgramShader, GL_LINK_STATUS, &status);
	if (status == 0) {
		int msgLength;
		glGetProgramiv(idProgramShader, GL_INFO_LOG_LENGTH, &msgLength);
		char *msg = malloc(msgLength);
		glGetProgramInfoLog(idProgramShader, msgLength, &msgLength, msg);
		printf("GL Program Linking Error : %s\n", msg);
		free(msg);
		return -0x01;
	}
	return idProgramShader;
}


// Interface functions
long int initShaders(const char *vertexShaderPath, const char *fragmentShaderPath) {
	// Creates and compiles shaders into a shader program and returns it
	// Because GLenum expands to unsigned int and we need signed ints for error handling, we use longs to cover the range of unsigned ints while keeping negative numbers.
	// This may or may not be necessary, as OpenGL seems to assign IDs from 1 going up by 1. However, the specification says there is no guarantee.
	long int idVertexShader = createShader(vertexShaderPath, GL_VERTEX_SHADER);
	long int idFragmentShader = createShader(fragmentShaderPath, GL_FRAGMENT_SHADER);
	if (idVertexShader < 0 || idFragmentShader < 0) 
		return -0x01;
	long int idShaderProgram = createProgram(idVertexShader, idFragmentShader);
	if (idShaderProgram < 0) 
		return -0x02;
	glUseProgram(idShaderProgram);
	glDeleteShader(idVertexShader);
	glDeleteShader(idFragmentShader);
	return idShaderProgram;
}

const Context setupPixels(const int width, const int height) {
	// Sets up vertex information and sends it to bound array and element array buffers
	// Returns a Context object containing all handlers information needed for drawing and terminating
	Context context;
	context.status = false; // The success flag is only set at the end of the operation so we can safely return the context as it is if we encounter an error.
	// Contrary to the index count, the vertex count is needed when calling glDraw* and is therefore stored in the context object.
	context.verticesCount =  width * height * VERTEX_COUNT * VERTEX_SIZE;
	context.idShaderProgram = initShaders("src/shaders/vertexMain.vert", "src/shaders/fragmentMain.frag");
	if (context.idShaderProgram < 0)
		return context;

	// Generates all buffers and textures
	glGenVertexArrays(1, &context.idVertexArray);
	glGenBuffers(1, &context.idVertexBuffer);
	glGenBuffers(1, &context.idElementBuffer);
	glGenBuffers(1, &context.idTextureBuffer);
	glGenTextures(1, &context.idFrameTexture);
	// At this point, the Context object should be fully initialized.

	glBindVertexArray(context.idVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, context.idVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.idElementBuffer);

	// Sets up communication with the fragment shader via a texture unit
	// A buffer texture is used to send the pixel data to the fragment shader as a sampler
	// By setting the uniform to TEXTURE_UNIT, the sampler will be associated with the texture unit where the texture resides
	glUniform1i(glGetUniformLocation(context.idShaderProgram, "texture"), TEXTURE_UNIT);

	// Indices of vertices for two triangles forming a rectangle
	// This array is dynamically allocated to avoid memory depletion in the stack area for setupPixels
	int indicesCount = INDICES_PER_POLYGON * height * width;
	unsigned int *indices = malloc(sizeof(float) * indicesCount);
	if (indices == NULL)
		return context;

	// This array is also dynamically allocated to avoid memory depletion in the stack area for setupPixels
	float *vertices = malloc(sizeof(float) * context.verticesCount);
	if (vertices == NULL) {
		free(indices);
		return context;
	}

	// Normalized width and height are 2/N instead of 1/N because normalized device coordinates range from [-1.0 ; 1.0], giving a total range of 2
	float pixelWidthNormalized = 2.0f / width;
	float pixelHeightNormalized = 2.0f / height;
	
	// TODO this whole thing looks like magic, but is also very ugly
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			int currentPixel = i * width + j;
			for (int k = 0; k < VERTEX_COUNT; k++) {
				// Iterates over each vertex of the rectangle
				// TODO this assumes rectangle are drawn, which isn't bad but makes the VERTEX_COUNT expression quite worthless
				// k = 0 : bottom-left vertex (-x, -y)
				// k = 1 : bottom-right vertex (+x, -y)
				// k = 2 : top-left vertex (-x, +y)
				// k = 3 : top-right vertex (+x, +y)
				// j + (k & 0b1) adds one to j when k mod 2 == 1. In other words, when k is for one of the right vertices, this adds 1 * pixelWidthNormalized to the x coordinate
				// "Why are you using k & 0b1, you can check parity with the modulo operator" yeah I really don't care.
				vertices[(currentPixel * VERTEX_COUNT + k) * VERTEX_SIZE + 0] = pixelWidthNormalized * (j + (k & 0b1)) - 1; // x coordinate
				// i + (k > 1) adds one to i when k > 1. In other words, when k is for one of the top vertices, this adds 1 * pixelHeightNormalized to the y coordinates
				vertices[(currentPixel * VERTEX_COUNT + k) * VERTEX_SIZE + 1] = pixelHeightNormalized * (i + (k > 1)) - 1; // y coordinate
			}
			// For each rectangle, this fills the indices array with the correct indices to dissect it into two triangles
			indices[currentPixel * INDICES_PER_POLYGON + 0] = currentPixel * VERTEX_COUNT + 0; // first triangle : bottom-left
			indices[currentPixel * INDICES_PER_POLYGON + 1] = currentPixel * VERTEX_COUNT + 1; // first triangle : bottom-right
			indices[currentPixel * INDICES_PER_POLYGON + 2] = currentPixel * VERTEX_COUNT + 2; // first triangle : top-left
			indices[currentPixel * INDICES_PER_POLYGON + 3] = currentPixel * VERTEX_COUNT + 1; // second triangle : bottom-right
			indices[currentPixel * INDICES_PER_POLYGON + 4] = currentPixel * VERTEX_COUNT + 2; // second triangle : top-left
			indices[currentPixel * INDICES_PER_POLYGON + 5] = currentPixel * VERTEX_COUNT + 3; // second triangle : top-right
		}
	}

	// Sends vertices and indices data
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * context.verticesCount, vertices, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(float) * indicesCount, indices, GL_STATIC_DRAW);
	free(indices);
	free(vertices);

	// Specifies the interpretation of vertex data
	unsigned int idPositionLocation = glGetAttribLocation(context.idShaderProgram, "pos");
	glVertexAttribPointer(idPositionLocation, VERTEX_SIZE, GL_FLOAT, false, VERTEX_SIZE * sizeof(float), (void *)0);
	glEnableVertexAttribArray(idPositionLocation);

	// Unbinds all bound objects
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	context.status = true;
	return context;
}

void draw(const Context context, const int width, const int height, const uint8_t * const colors) {
	// Draws every pixel with given colors for a single frame
	// This assumes a valid shader program and vertex array object are already in use / bound and vertices / indices data is already sent to the GPU
	// This does not unbind any object already bound by callee, nor does it swap buffers or poll events
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(context.idShaderProgram);
	glBindVertexArray(context.idVertexArray);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_BUFFER, context.idFrameTexture);

	// For some reason, glTexBuffer doesn't accept GL_RGB8, but it does accept GL_R8, so we send all values as colors with a single red component and seperate the red, green and blue components in the shader
	// There are 3 alternatives :
	// GL_RGB32F, using 4 times as much memory but removing the need for OpenGL to normalize the integers (range [0, 255]) to floats (range [0, 1]) AND the need to fetch each red, green and blue component ;
	// GL_R16F, using twice as much memory and using weird half-floats but removing the need for OpenGL to normalize the integers to floats
	// GL_RGBA8, using 33% more memory but removing the need to fetch each red, green and blue component.
	// TODO Some testing may be done to find out which is best
	glBindBuffer(GL_TEXTURE_BUFFER, context.idTextureBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, context.idTextureBuffer);
	glBufferData(GL_TEXTURE_BUFFER, width * height * COLOR_COMPONENTS * sizeof(uint8_t), colors, GL_STATIC_DRAW);

	glDrawElements(GL_TRIANGLES, context.verticesCount, GL_UNSIGNED_INT, (void *)0);

	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glBindVertexArray(0);
}

void terminate(const Context context) {
	glDeleteVertexArrays(1, &context.idVertexArray);
	glDeleteBuffers(1, &context.idVertexBuffer);
	glDeleteBuffers(1, &context.idElementBuffer);
	glDeleteBuffers(1, &context.idTextureBuffer);
	glDeleteTextures(1, &context.idFrameTexture);
	glDeleteProgram(context.idShaderProgram);
}