#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 view_nor;
layout(location = 2) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform push_constants {
	mat4 world;
	vec3 color;
} pc;

layout(set = 1, binding = 0) uniform sampler2D tex_diffuse;

void main() {
   frag_color = vec4(pc.color, 1.f) * texture(tex_diffuse, tex_coord);
}
