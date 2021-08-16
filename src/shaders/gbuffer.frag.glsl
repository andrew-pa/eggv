#version 450

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 view_nor;
layout(location = 2) in vec2 tex_coord;

layout(location = 0) out vec4 position_buf;
layout(location = 1) out vec4 normal_buf;
layout(location = 2) out vec4 texture_material_buf;

layout(push_constant) uniform push_constants {
    mat4 world;
    uint material_index;
} pc;

layout(set = 1, binding = 0) uniform sampler2D tex_diffuse;

void main() {
    position_buf = vec4(view_pos, tex_coord.x);
    normal_buf = vec4(view_nor, tex_coord.y);
    texture_material_buf = vec4(texture(tex_diffuse, tex_coord).xyz, pc.material_index + 1);
}
