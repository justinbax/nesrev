# NESRev v3.6

NESRev v3.6 (the v3.6 is part of the name, not the actual version number) is a cycle-accurate emulator for the Nintendo Entertainment System (NES). It is built from the ground up in plain C using OpenGL as graphical interface (and GLEW/GLFW for compatibility).

This project also bundles NESGen, an generator for standardized iNes 1.0 (.nes) files from program data (PRG ROM) and graphical data (CHR ROM). As a demonstration, a custom sample NES game is located in `src/asm/sample.asm` to be assembled, then used by NESGen to create a .nes file readable by NESRev v3.6 or another NES emulator.

## Usage

`nesrev input`

where `input` is the path to a valid iNes (`.nes`) file.

## Compilation

The provided Makefile has three options:

`make debug`: enables debug information, disables optimizations

`make release`: disables debug information, enables medium-high optimizations

`make clean`: removes all compiled binaries and object files from the `bin` folder for clean recompilation

Currently, compilation is supported for Windows and Linux. Windows libraries are already packaged in the `lib/win32` directory, but Linux users should install the [GLFW](https://glfw.org/), [GLEW](http://glew.sourceforge.net/) and [Portaudio](https://www.portaudio.com/) libraries beforehand (ideally through a package manager). Porting the project to MacOS should not be difficult, as those libraries are cross-platform; only the Makefile would need to be modified.

| Library | Arch Linux package | Debian package |
| --- | --- | --- |
| GLEW | [`glew`](https://archlinux.org/packages/extra/x86_64/glew/) | `libglew-dev` |
| GLFW | [`glfw`](https://archlinux.org/packages/extra/x86_64/glfw/) | `libglfw3-dev` |
| Portaudio | [`portaudio`](https://archlinux.org/packages/extra/x86_64/glfw/) | `libportaudio2`

## Emulation

***The next 3 sections explains some of the technical features of the emulator and is not relevant to the build or usage process. The last section ([NESGen](#nesgen)) and [`src/asm/README.md`](src/asm/README.md) refer to the `.nes` packaging utility program bundled in this project.***

### CPU

`src/cpu.h` is the complete source code for emulating the central processing unit (modified 6502 core) in the NES. It handles:

* Correct interrupt handling with all its quirks (e.g. a perfectly timed /NMI hijacking a BRK) and faithful dummy writes
* Per-cycle or per-instruction debug output with information on the address and data buses
* All illegal instructions, including every NOP with correct dummy reads according to its addressing mode
* And of course, all instructions with cycle-accuracy. Every read and write is well timed and emulated, even if it seems wasted (like in the case of read-modify-write instructions).

The accuracy of the CPU is limited to the whole cycle level: half-cycles (ϕ1 and ϕ2) are not taken into account. However, this should not affect the output, because what's important is when the CPU *sends* the address, not when it *reads* the result that has already been sent by the external device (those are two different half-cycles, but are emulated at the same time).

### PPU

Picture processing unit emulation is contained withing `src/ppu.h`. It has a few key components:

* Register interface with the CPU (memory-mapped registers from 0x2000 to 0x2007 and direct memory access)
* Sprite evaluation: the process of evaluating which sprites are to be rendered on the next scanline and fetching corresponding data from memory. In addition to the memory reads, the contents of registers accessible by the CPU (OAMADDR and OAMDATA) are correctly updated every cycle.
* Tile fetching: the continuous (per-tile) process of fetching the next background tile. All reads, useful or not, are cycle-accurate.
* Per-pixel rendering of colors.

Similar to the difference of the "same" color from one NES to the other and mostly from one CRT TV to the other, the appearance of colors is customizable. Of course, a default and arbitrary palette is provided (`/default.pal`).

### Cartridges

The basic cartridge structure is defined in `src/cartridge.h`. All the logic of cartridges (*mappers* in the emulation community) is defined in `src/mapper.h`.

#### Mappers supported

| Name | Mapper Number | Approx. % of games | Notes |
| --- | --- | --- | --- |
| NROM | 000 | 10.0% |  |
| MMC1 | 001 | 27.8% | Some edge cases unhandled |

Currently, RAM and saved (persistent) memory are not supported. This applies to all mappers.

### Input

Controller reading is handled by `src/input.h` and `src/input.c`. Currently, two standard NES controllers are supported, although the second port is left disconnected and without key bindings.

Default key bindings cannot be changed for now and are mapped to:
| NES Button | Keyboard binding |
| --- | --- |
| Up | W |
| Down | S |
| Left | A |
| Right | D |
| A | Spacebar |
| B | Left shift |
| SELECT | Backspace |
| START | Enter |

Other controllers like the Zapper are not handled.

## Graphical interface

NESRev uses a custom pixel-rendering engine, written directly in OpenGL. For the sake of compatibility, GLFW and GLEW are used alongside OpenGL to provide cross-platform support for windows and input (GLFW) and getting pointers to OpenGL functions (GLEW). I am looking forward to learn how both of these work so I may one day replace them with my own code, using only C and OpenGL.

Both libraries and OpenGL are initialized in the main function (naturally), but everything else regarding OpenGL is abstracted in `graphics.h` with simple functions: simply initialize a `Context` with `createContext`, hold onto it, fill an array of the colors you want to draw and `draw` with the context you were given. `terminateContext` assures everything is terminated correctly.

In the case of GLFW, it belongs more to the main function. Because GLFW is already abstracting a lot of technical details in `GLFWWindow`, I felt no need to wrap an interface around this single object. Window creation and handling is left in `main` due to its simplicity and input reading is taken care of in `input.c`.

Additional optional functionalities (mostly error callbacks) are also left in `main` so they can be disabled at free will.

## iNes 1.0 (.nes) format reading

NES games are usually stored in standardized `.nes` files. Currently, NESRev supports reading the crucial components of an `.nes` file in order to get it running. Some features of the iNes standard (I have yet to encounter a .nes file that uses them, given that they're so rare) are not yet implemented.

## NESGen

NESGen (`src/asm/nesgen.c`) is a single-file utility program to create valid `.nes` files from information about a cartridge. It is completely seperated from NESRev, so must be compiled manually. Additionally, the assembly code for a sample "game" of my creation is given alongside (`src/asm/sample.asm`) to test it and see it in action (and also because it helped me debug NESRev. Also it was fun making it). You'll need a 6502 assembler (like [vasm](http://sun.hasenbraten.de/vasm/) - oldstyle version) to turn it into PRG ROM. As for CHR, you should be able to create a binary file yourself: the program only uses background tile 0 (CHR 0x1000 - 0x100F) and sprite tile 1 (CHR 0x0010 - 0x001F).

Refer to the [README](src/asm/README.md) in `src/asm` for more information.

### NESGen Usage

`nesgen chr prg nnn M output`

where `chr` is the path to the video memory (CHR ROM for CHaRacter Read-Only Memory);

`prg` is the path to the program memory (PRG ROM for PRoGram Read-Only Memory);

`nnn` is the [mapper](https://wiki.nesdev.org/w/index.php?title=Mapper) [number](https://wiki.nesdev.org/w/index.php/List_of_mappers) (basically the type of cartridge; 000 for nothing special)'

`M` is the [nametable mirroring](https://wiki.nesdev.org/w/index.php?title=Mirroring#Nametable_Mirroring) type, either `H` for horizontal or `V` for vertical;

`output` is the desired output file with extension (if you do not type the .nes, it will not be added, but your file will still be valid).
