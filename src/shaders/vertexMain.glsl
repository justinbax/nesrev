#version 460
in vec2 pos;
out flat int pixelNumber;

void main() {
	gl_Position = vec4(pos.xy, 1.0, 1.0);
	pixelNumber = gl_VertexID / 4;
}