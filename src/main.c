#include <stdio.h>

#include "GLFW/glfw3.h"

void callbackError(int code, const char *description) {
	printf("GLFW Error %i : %s\n", code, description);
}

int main() {
	printf("NESRev v3.6\n");

	if (!glfwInit())
		return 1;
	printf("GLFW initialized.\n");

	glfwSetErrorCallback(callbackError);

	GLFWwindow *window = glfwCreateWindow(1920, 1090, "NESRev v3.6", NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);

	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}