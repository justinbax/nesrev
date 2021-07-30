#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#define COLOR_COMPONENTS 3 // Number of components to a color (RGB is 3, RGBA is 4)

// TODO this interface is shit
// This isn't a "black box" : a caller needs to care about the inner workings of the functions (namely draw and setupPixels) in order to call them
// The callee excepts the caller to set up many variables beforehand (buffers, textures). All of this is tightly tied to the OpenGL API
// Possible solution : init OpenGL objects with a function returning a struct with the context and object IDs and pass that struct to setupPixels and draw

typedef struct {
	unsigned int idVertexArray;
	unsigned int idVertexBuffer;
	unsigned int idElementBuffer;
	// Texture, texture buffer and texture unit used to send color data to fragment shader
	unsigned int idFrameTexture;
	unsigned int idTextureBuffer;

	unsigned int idShaderProgram;

	int verticesCount; // Total number of vertices to draw. Used to call glDrawElements

	// TODO I don't know if this is good
	bool status; // Used to communicate to the callee if an operation succeeded.
} Context;

int initShaders(const char *vertexShaderPath, const char *fragmentShaderPath);
const Context setupPixels(const int width, const int height);
void draw(const Context context, const int width, const int height, const float * const colors);
void terminate(const Context context);

#endif // ifndef GRAPHICS_H