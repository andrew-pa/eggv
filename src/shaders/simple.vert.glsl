#version 450

layout(location = 0) in vec3 pos;

layout(push_constant) uniform push_constants {
    mat4 world;
    vec4 color;
} pc;

layout(binding = 0) uniform camera {
    mat4 view;
    mat4 proj;
} cam;

void main() {
    vec4 _world_pos =  pc.world * vec4(pos, 1.0);
    vec4 _view_pos = cam.view * _world_pos;
    gl_Position = (cam.proj * _view_pos);
}
