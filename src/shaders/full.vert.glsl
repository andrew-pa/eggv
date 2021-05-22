#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 view_pos;
layout(location = 1) out vec3 view_nor;
layout(location = 2) out vec2 tex_coord;

layout(push_constant) uniform push_constants {
    mat4 world;
    vec4 color;
} pc;

layout(binding = 0) uniform camera {
    mat4 view;
    mat4 proj;
} cam;

void main() {
    tex_coord = in_tex_coord;
    vec4 _world_pos =  pc.world * vec4(pos, 1.0);
    vec4 _view_pos = cam.view * _world_pos;
    view_pos = _view_pos.xyz;
    view_nor = (cam.view * vec4(nor, 0.0)).xyz;
    gl_Position = (cam.proj * _view_pos);
}
