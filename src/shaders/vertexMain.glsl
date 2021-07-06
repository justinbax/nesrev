#version 460
in vec3 pos;
out flat int pixelNumber;

void main() {
	gl_Position = vec4(pos.xyz, 1.0);
	pixelNumber = gl_VertexID / 4;
}