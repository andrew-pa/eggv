#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nor;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 world_pos;
layout(location = 1) out vec2 tex_coord;

layout(push_constant) uniform push_constants {
	mat4 world;
	vec4 color;
} pc;

layout(binding = 0) uniform camera {
    mat4 viewproj;
} cam;

void main() {
	tex_coord = in_tex_coord;
        vec4 _world_pos =  pc.world * vec4(pos, 1.0);
        world_pos = _world_pos.xyz;
	gl_Position = (cam.viewproj * _world_pos);
        gl_PointSize = 12.0;
}
