#version 460
uniform samplerBuffer texture;
in flat int colorLocation;
out vec4 finalColor;

void main() {
	finalColor = vec4(texelFetch(texture, colorLocation).x, texelFetch(texture, colorLocation + 1).x, texelFetch(texture, colorLocation + 2).x, 1.0);
}