#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

int ctoi(const char *value) {
	int result = 0;
	int multiplier = 1;
	for (char *c = value; *c != '\0'; c++) {
		result += (*c - '0') * multiplier;
		multiplier *= 10;
	}
	return result;
}

uint8_t *readFile(const char *path, int *size) {
	*size = 0;

	FILE *file = fopen(path, "rb");
	if (file == NULL) return NULL;

	fseek(file, 0, SEEK_END);
	*size = ftell(file);
	rewind(file);

	uint8_t *contents = malloc(*size * sizeof(uint8_t));
	if (contents == NULL) {
		*size = 0;
		fclose(file);
		return NULL;
	}

	if (fread(contents, sizeof(uint8_t), *size, file) != *size) {
		*size = 0;
		fclose(file);
		free(contents);
		return NULL;
	}

	fclose(file);
	return contents;
}

bool emptyFill(FILE *output, int fillSize) {
	if (fillSize != 0) {
		uint8_t *padding = calloc(fillSize, sizeof(uint8_t));
		if (padding == NULL)
			return false;
		if (fwrite(padding, sizeof(uint8_t), fillSize, output) != fillSize)
			return false;
	}
	return true;
}

int main(int argc, const char *argv[]) {

	if (argc != 6) {
		printf("Usage : nesgen chr prg mapper mirror out\n");
		printf("See README for more details.\n");
		return -0x01;
	}

	int chrSize, prgSize;
	uint8_t *chr = readFile(argv[1], &chrSize);
	uint8_t *prg = readFile(argv[2], &prgSize);
	if (!chr || !prg) {
		free(chr);
		free(prg);
		printf("Fatal error : couldn't read CHR or PRG file / out of memory.\n");
		return -0x02;
	}

	FILE *output = fopen(argv[5], "wb");
	if (output == NULL) {
		printf("Fatal error : couldn't open or create output file.\n");
		free(chr);
		free(prg);
		return -0x03;
	}

	uint8_t header[16] = {
		0x4E, 0x45, 0x53, 0x1A, // Constant NES + MSDOS EOF
		0x00, 0x00, // PRG & CHR size
		0x00, // Mostly mapper & mirroring
		0x00, // For us, just upper nybble of mapper
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Useless stuff
	};

	uint8_t mapper = ctoi(argv[3]);
	header[4] = ((prgSize - 1) >> 14) + 1; // In 16KiB, or 2^14
	header[5] = ((chrSize - 1) >> 13) + 1; // In 8KiB, or 2^13
	header[6] |= (mapper & 0b1111) << 4;
	header[6] |= (argv[4][0] == '1') || (argv[4][0] == 'v') || (argv[4][0] == 'V'); // '1', 'v' or 'V' for vertical mirroring

	if ((header[6] & 0b1) == 0 && (argv[4][0] != '0') && (argv[4][0] != 'h') && (argv[4][0] != 'H')) {
		// Not '1', 'v', 'V', '0', 'h' or 'H'
		printf("Invalid mirroring type, assuming vertical.\n");
		printf("Possible types : [H]orizontal, [V]ertical.\n");
	}

	header[7] |= mapper & 0b11110000;

	uint8_t status = 0;

	status |= (fwrite(header, sizeof(uint8_t), 16, output) != 16);
	status |= (fwrite(prg, sizeof(uint8_t), prgSize, output) != prgSize);
	status |= !emptyFill(output, (header[4] << 14) - prgSize);
	status |= fwrite(chr, sizeof(uint8_t), chrSize, output);
	status |= !emptyFill(output, (header[4] << 13) - chrSize);

	free(prg);
	free(chr);
	prg = NULL;
	chr = NULL:

	if (status) {
		freopen(NULL, "w", output);
		fclose(output);
		printf("Fatal error : couldn't write to output file.\n");
		return -0x04;
	}

	fclose(output);

	return 0;
}