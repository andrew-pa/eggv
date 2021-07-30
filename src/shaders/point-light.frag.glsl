#version 450
#include "lighting.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput input_position;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput input_normal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput input_texcoord_mat;

layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform light_u {
    mat4 world;
    vec4 param;
    vec4 color;
    vec4 view_pos;
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
    if(txc_mat.z < 1.f) discard;
    vec3 pos = subpassLoad(input_position).xyz;
    vec3 nor = subpassLoad(input_normal).xyz;

    material mat = mats.data[uint(txc_mat.w)];

    vec3 L = light.view_pos.xyz - pos;
    float d = length(L);
    L /= d;

    vec3 Lcol = light.color.xyz * (1.0 / (1.0 + light.param.x * d * d));
    frag_color = vec4(compute_lighting(nor, -L, Lcol, mat), 1.0);
}
