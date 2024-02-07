#ifndef INES_H
#define INES_H

#include <stdbool.h>

#include "cartridge.h"

void freeCartridge(Cartridge *cart);
int loadROMFromFile(Cartridge *cart, const char *path, bool printDetails);

#endif // ifndef INES_H