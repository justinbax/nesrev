#version 460
uniform vec3 colors[3 * 4]; // TODO delete those magic numbers (HEIGHT_PIXELS and WIDTH_PIXELS) and maybe add default values
in float pixelNumber;
out vec4 finalColor;

void main() {
	finalColor = vec4(colors[int(pixelNumber)], 1.0);
}