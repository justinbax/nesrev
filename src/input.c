#include "input.h"

#include <stdio.h>

void initPort(Port *port, uint8_t type, GLFWwindow *window, int *scancodes, int scancodeCount) {
	// TODO more controllers (inputType)
	// TODO deal with occasional open buses
	port->inputType = type;
	port->window = window;
	port->control = PORT_STROBE; // TODO check default value
	port->currentKey = 0;

	mapKeys(port, scancodes, scancodeCount);
}

bool mapKeys(Port *port, int *keys, int count) {
	if (keys != NULL || count == 0) {
		port->keys = keys;
		port->keyCount = count;
		return true;
	}
	return false;
}

uint8_t readController(Port *port) {
	if (port->reg != 0) {
		//printf("a");
	}
	uint8_t data = 0;
	if (port->keyCount > 0) {
		if (port->control & PORT_STROBE) {
			data = (glfwGetKey(port->window, port->keys[0]) == GLFW_PRESS);
		} else {
			data = port->reg & 1;
			port->reg >>= 1;
			if (++port->currentKey > port->keyCount) {
				port->currentKey--; // We never know, maybe some program reads the controller 2^31 times without polling it again...
				data = 1;
			}
		}
	}
	return data;// | 0xF7;
}

void writeController(Port *port, uint8_t data) {
	if (port->control & PORT_STROBE) {
		// On an original NES controllers (and many other input controllers), the buttons are continuously fed into the internal register while STROBE is high.
		// However, there isn't much to benefit from reading input every frame, as, while STROBE is high, the register isn't shifting to bits other than the first (A), so we just update the register when writing when STROBE is high.
		port->currentKey = 0;
		port->reg = 0;
		for (int i = 0; i < port->keyCount; i++) {
			port->reg |= (glfwGetKey(port->window, port->keys[i]) << i);
		}
	}

	port->control = data;
}