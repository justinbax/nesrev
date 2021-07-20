#version 460
uniform sampler1D textureSampler;
in flat int pixelNumber;
out vec4 finalColor;

void main() {
	finalColor = vec4(texelFetch(textureSampler, pixelNumber, 0).xyz, 1.0);
}