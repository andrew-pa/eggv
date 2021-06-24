#version 450

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

void main() {
    vec3 L = (cam.view * light.direction).xyz;

    vec3 nor = subpassLoad(input_normal).xyz;
    vec2 txc = subpassLoad(input_texcoord_mat).xy;

    vec3 col = vec3(1.0, 1.0, 0.9);
    vec3 diffuse = max(0., dot(nor, -L)) * col * light.color.xyz;
    vec3 ambient = col*vec3(0.1);
    frag_color = vec4(diffuse+ambient,1.0);
}
