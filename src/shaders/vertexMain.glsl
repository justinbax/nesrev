#version 460
in vec3 pos;
in float index;
out float pixelNumber;

void main() {
	gl_Position = vec4(pos.xyz, 1.0);
	pixelNumber = index;
}