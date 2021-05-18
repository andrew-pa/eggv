#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec3 tang;
layout(location = 3) in vec2 in_tex_coord;

layout(location = 0) out vec3 world_pos;
layout(location = 1) out vec2 tex_coord;

layout(push_constant) uniform push_constants {
	mat4 world;
	vec4 color;
} pc;

void main() {
	tex_coord = in_tex_coord;
	world_pos = (pc.world * vec4(pos, 1.0)).xyz;
}
