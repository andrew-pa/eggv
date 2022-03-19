#version 450

layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform push_constants {
	mat4 world;
	vec4 color;
} pc;

void main() {
   frag_color = pc.color;
}
