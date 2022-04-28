#ifndef INES_H
#define INES_H

#include "cartridge.h"

void freeCartridge(Cartridge *cart);
int loadROMFromFile(Cartridge *cart, const char *path);

#endif // ifndef INES_H