#version 450
layout (vertices = 9) out;

layout(location = 0) in vec3 in_world_pos[];
layout(location = 1) in vec2 in_tex_coord[];

layout(location = 0) out vec3 world_pos[];
layout(location = 1) out vec2 tex_coord[];

void main() {
	tex_coord[gl_InvocationID] = in_tex_coord[gl_InvocationID];
	world_pos[gl_InvocationID] = in_world_pos[gl_InvocationID];

	float lvl = 40.0;
	for(int i = 0; i < 4; ++i)
		gl_TessLevelOuter[i] = lvl;
	gl_TessLevelInner[0] = lvl;
	gl_TessLevelInner[1] = lvl;
}
