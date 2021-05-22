#version 450

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 view_nor;
layout(location = 2) in vec2 tex_coord;

layout(location = 0) out vec4 position_buf;
layout(location = 1) out vec4 normal_buf;
layout(location = 2) out vec4 texture_material_buf;

layout(push_constant) uniform push_constants {
    mat4 world;
    vec4 color;
} pc;

void main() {
    position_buf = vec4(view_pos, 0.0);
    normal_buf = vec4(view_nor, 0.0);
    texture_material_buf = vec4(tex_coord, 0.0, 0.0);
}
