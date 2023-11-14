# NESGen

NESGen is a project separate from NESRev v3.6. It is a single-file utility program to create `.nes` ROM files. It is used to compile custom games into valid files that can be executed by NESRev v3.6 (or any other NES emulator).

A prototype assembly game is located in `sample.asm` to be compiled and bundled. This prototype is designed to use the raw binary file `chr.bin` as CHR (graphical) data.

## Usage

`nesgen [chr] [prg] [nnn] [V/H] [output]`

where:
- `chr` is the path to the raw CHR (graphical) data;
- `prg` is the path to the raw PRG (executable program) data;
- `nnn` is a three-digit number representing the desired mapper number;
- `H/V` selects the nametable mirroring type (`H` for horizontal mirroring, `V` for vertical mirroring); and
- `output` is the desired output file with extension (by convention, usage of a `.nes` extension is recommended)

## Compilation

There is no Makefile provided for this project, as it is a single C file that does not rely on any library. Just compile it like any other C file.

## Assembling for 6502

NESGen expects raw PRG data (the binary data that will be present in the cartridge), so, if you would like to make a game in 6502 assembly, you first have to assemble it. The prototype `sample.asm` was written to be assembled using the vasm assembler targeting 6502 with oldstyle syntax.

For future reference, it was compiled with:

`vasm6502 -Fbin -dotdir -o prg.bin src/asm/sample.asm`