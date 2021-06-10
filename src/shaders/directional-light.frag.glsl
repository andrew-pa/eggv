#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput input_position;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput input_normal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput input_texcoord_mat;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 3) uniform camera {
    mat4 view;
    mat4 proj;
} cam;

void main() {
    vec3 L = (cam.view * vec4(0.1, 0.9, 0.0, 0.0)).xyz;

    vec3 nor = subpassLoad(input_normal).xyz;
    vec2 txc = subpassLoad(input_texcoord_mat).xy;

    frag_color = vec4(max(0., dot(nor, L)) * mix(vec3(1.0,0.5,0.0),vec3(0.0,1.0,0.5),sin(txc.x+txc.y)), 1.0);
}
