#version 450
#include "lighting.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput input_position;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput input_normal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput input_texcoord_mat;

layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform light_u {
    vec4 direction;
    vec4 color;
} light;

layout(set = 0, binding = 3) uniform camera {
    mat4 view;
    mat4 proj;
} cam;

layout(set = 0, binding = 4) buffer materials {
    material data[];
} mats;

void main() {
    vec4 txc_mat = subpassLoad(input_texcoord_mat);
    if(txc_mat.w < 1.f) discard;

    vec3 L = (cam.view * light.direction).xyz;
    material mat = mats.data[uint(txc_mat.w) - 1];
    vec3 nor = subpassLoad(input_normal).xyz;

    frag_color = vec4(compute_lighting(nor, L, light.color.rgb, mat, txc_mat.xyz),1.0);
}
