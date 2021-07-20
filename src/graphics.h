#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "GL/glew.h"
#include "GLFW/glfw3.h"

GLFWwindow *createWindow(const int width, const int height, const char *name);
int initShaders(const char *vertexShaderPath, const char *fragmentShaderPath);
const int setupPixels(const int width, const int height, unsigned int idPositionLocation, unsigned int idSamplerLocation, unsigned int textureUnit);
void draw(const int pixelCount, const float * const colors, const int verticesCount, unsigned int idTextureBuffer, unsigned int idFrameTexture, unsigned int textureUnit);

#endif // ifndef GRAPHICS_H