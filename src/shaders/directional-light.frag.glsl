#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput input_position;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput input_normal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput input_texcoord_mat;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = vec4(1.0, 0.0, 1.0, 1.0);
}
