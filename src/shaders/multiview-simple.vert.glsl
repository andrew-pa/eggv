#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec2 in_tex_coord;

layout(push_constant) uniform push_constants {
    mat4 world;
    uint camera_index;
} pc;

struct camera {
    mat4 viewproj;
};

layout(set = 0, binding = 0) buffer cameras_buf {
    camera data[];
} cameras;

void main() {
    vec4 _world_pos =  pc.world * vec4(pos, 1.0);
    gl_Position = (cameras.data[pc.camera_index].viewproj * _world_pos);
}
