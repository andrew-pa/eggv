#version 450

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 view_nor;
layout(location = 2) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform push_constants {
	mat4 world;
	vec4 color;
} pc;

void main() {
   frag_color = pc.color;
}
