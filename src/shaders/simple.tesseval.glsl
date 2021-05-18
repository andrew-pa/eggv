#version 450
layout(quads, equal_spacing, ccw) in;

// NxN patch
#define N 3

layout(location = 0) in vec3 in_world_pos[];
layout(location = 1) in vec2 in_tex_coord[];

layout(location = 0) out vec2 tex_coord;

layout(binding = 0) uniform camera {
	mat4 viewproj;
} cam;


vec3 de_casteljau_1d(in vec3[N*N] P, float u, int offset) {
	vec3 Q[N];
	for(int i = 0; i < N; ++i) Q[i] = P[i + offset];
	for(int k = 1; k < N; ++k) {
		for(int i = 0; i <= N - k; ++i) {
			Q[i] = mix(Q[i], Q[i+1], u);
		}
	}
	return Q[0];
}

void main() {
	vec2 omc = vec2(1.0) - gl_TessCoord.xy;
	vec4 a = vec4(omc.x * omc.y,
		gl_TessCoord.x * omc.y,
		gl_TessCoord.x * gl_TessCoord.y,
		omc.x * gl_TessCoord.y);
	/*tex_coord = in_tex_coord[0] * a.x
		+ in_tex_coord[1] * a.y
		+ in_tex_coord[2] * a.z
		+ in_tex_coord[3] * a.w;
	vec3 world_pos = in_world_pos[0] * a.x
		+ in_world_pos[1] * a.y
		+ in_world_pos[2] * a.z
		+ in_world_pos[3] * a.w;
	vec3 geo_norm = normalize(cross(
		in_world_pos[1] - in_world_pos[0],
		in_world_pos[2] - in_world_pos[0]));
	world_pos += geo_norm*abs(sin(gl_TessCoord.y*6.28*2.0) + cos(gl_TessCoord.x*6.28*2.0))*0.2;
	gl_Position = cam.viewproj * vec4(world_pos, 1.0);*/

	vec3 Q[N * N];
	vec3 P[N * N];
	for(int i = 0; i < 9; ++i) P[i] = in_world_pos[i];
	for(int j = 0; j < N; ++j) {
		Q[j] = de_casteljau_1d(P, gl_TessCoord.x, j * N);
	}
	vec3 world_pos = de_casteljau_1d(Q, gl_TessCoord.y, 0);

	tex_coord = in_tex_coord[0] * a.x
		+ in_tex_coord[2] * a.y
		+ in_tex_coord[6] * a.z
		+ in_tex_coord[8] * a.w;

	gl_Position = cam.viewproj * vec4(world_pos, 1.0);
}
