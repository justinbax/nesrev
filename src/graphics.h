#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#define COLOR_COMPONENTS 3 // Number of components to a color (RGB is 3, RGBA is 4)

typedef struct {
	unsigned int idVertexArray;
	unsigned int idVertexBuffer;
	unsigned int idElementBuffer;
	// Texture, texture buffer and texture unit used to send color data to fragment shader
	unsigned int idFrameTexture;
	unsigned int idTextureBuffer;

	// A long is used to keep the range of GLenum (unsigned int) while keeping negative numbers for error handling
	long int idShaderProgram;

	unsigned int verticesCount; // Total number of vertices to draw. Used to call glDrawElements.

	bool status; // Used to communicate to the callee if an operation succeeded.
} Context;

long int initShaders(const char *vertexShaderPath, const char *fragmentShaderPath);
const Context setupPixels(const int width, const int height);
void draw(const Context context, const int width, const int height, const uint8_t * const colors);
void terminate(const Context context);

#endif // ifndef GRAPHICS_H