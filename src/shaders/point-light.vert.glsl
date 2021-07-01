#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec2 in_tex_coord;

layout(push_constant) uniform push_constants {
    mat4 world;
    vec4 param, color, light_view_pos;
} pc;

layout(binding = 3) uniform camera {
    mat4 view;
    mat4 proj;
} cam;

void main() {
    vec4 _world_pos =  pc.world * vec4(pos, 1.0);
    vec4 _view_pos = cam.view * _world_pos;
    gl_Position = (cam.proj * _view_pos);
}
