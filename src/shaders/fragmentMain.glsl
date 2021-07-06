#version 460
uniform vec3 colors[32 * 30]; // TODO delete those magic numbers (HEIGHT_PIXELS and WIDTH_PIXELS) and maybe add default values
in flat int pixelNumber;
out vec4 finalColor;

void main() {
	finalColor = vec4(colors[pixelNumber].xyz, 1.0);
}