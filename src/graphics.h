#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#define COLOR_COMPONENTS 3 // Number of components to a color (RGB is 3, RGBA is 4)

// TODO this interface is shit
// This isn't a "black box" : a caller needs to care about the inner workings of the functions (namely draw and setupPixels) in order to call them
// The callee excepts the caller to set up many variables beforehand (buffers, textures). All of this is tightly tied to the OpenGL API
// Possible solution : init OpenGL objects with a function returning a struct with the context and object IDs and pass that struct to setupPixels and draw
int initShaders(const char *vertexShaderPath, const char *fragmentShaderPath);
const int setupPixels(const int width, const int height, unsigned int idPositionLocation, unsigned int idSamplerLocation, unsigned int textureUnit);
void draw(const int pixelCount, const float * const colors, const int verticesCount, unsigned int idTextureBuffer, unsigned int idFrameTexture, const unsigned int textureUnit);

#endif // ifndef GRAPHICS_H