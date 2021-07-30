#version 460
in vec2 pos;
out flat int colorLocation;

void main() {
	gl_Position = vec4(pos.xy, 1.0, 1.0);
	colorLocation = gl_VertexID / 4 * 3;
}