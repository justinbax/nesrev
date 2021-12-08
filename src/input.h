#ifndef INPUT_H
#define INPUT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "GLFW/glfw3.h"

#define PORT_NONE 0
#define PORT_STDCONTROLLER 1

#define PORT_STROBE 0b1

typedef struct {
	uint8_t inputType;
	uint8_t control;
	uint32_t reg;
	uint8_t currentKey;

	GLFWwindow *window;
	int *keys;
	int keyCount;
} Port;


void initPort(Port *port, uint8_t type, GLFWwindow *window, int *keys, int keyCount);
bool mapKeys(Port *port, int *keys, int count);
uint8_t readController(Port *port);
void writeController(Port *port, uint8_t data);

#endif // ifndef INPUT_H