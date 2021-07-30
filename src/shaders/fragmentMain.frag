#version 460
uniform samplerBuffer textureSampler;
in flat int pixelNumber;
out vec4 finalColor;

void main() {
	finalColor = vec4(texelFetch(textureSampler, pixelNumber).xyz, 1.0);
}