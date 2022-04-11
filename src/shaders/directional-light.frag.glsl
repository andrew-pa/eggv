#version 450
#include "lighting.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput input_position;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput input_normal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput input_texcoord_mat;

layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform light_u {
    vec4 direction;
    vec3 color;
    int shadow_index;
    mat4 shadow_viewproj;
} light;

layout(set = 0, binding = 3) uniform camera {
    mat4 view;
    mat4 proj;
} cam;

layout(set = 0, binding = 4) buffer materials {
    material data[];
} mats;

layout(set = 0, binding = 5) uniform sampler2DArray shadow_map;

void main() {
    vec4 txc_mat = subpassLoad(input_texcoord_mat);
    if(txc_mat.w < 1.f) discard;
    vec4 view_pos = vec4(subpassLoad(input_position).xyz, 1.0f);

    vec3 L = light.direction.xyz;
    material mat = mats.data[uint(txc_mat.w) - 1];
    vec3 nor = subpassLoad(input_normal).xyz;

    bool in_shadow = false;
    if(light.shadow_index >= 0) {
        vec4 shadow_pos = light.shadow_viewproj * view_pos;
        float v = texture(shadow_map, vec3(shadow_pos.xy, light.shadow_index)).r;
        /* frag_color = vec4(max(v - shadow_pos.z, 0.0), max(-(v-shadow_pos.z), 0.0), 0.0, 1.); */
        /* return; */
        in_shadow = v < shadow_pos.z;
    }

    frag_color = vec4(compute_lighting(nor, L, light.color.rgb, mat, txc_mat.xyz) * (in_shadow ? 0.2 : 1.0),1.0);
    /* float x = texture(shadow_map, vec3(txc_mat.xy, light.shadow_index)).r*0.01; */
    /* frag_color = vec4(x, x, x, 1.f); */
}
