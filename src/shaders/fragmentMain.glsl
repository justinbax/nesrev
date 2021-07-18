#version 460
uniform sampler1D textureSampler;
in flat int pixelNumber;
out vec4 finalColor;

void main() {
	// TODO replace texelFetch with texture (a normalized coordinate will need to be passed instead of pixelNumber)
	// finalColor = vec4(texelFetch(textureSampler, pixelNumber, 0).xyz, 1.0);
	finalColor = vec4(texture(textureSampler, 0.0).xyz, 1.0);
}