#version 450

layout(location = 0) in vec3 view_pos;
layout(location = 1) in vec3 view_nor;
layout(location = 2) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;

layout(push_constant) uniform push_constants {
	mat4 world;
	vec4 color;
} pc;

void main() {
    if(pc.color.w == 0.0) {
	    frag_color = pc.color;
    } else if(pc.color.w == 1.0) {
        frag_color = mix(vec4(abs(view_nor),1.), pc.color*0.5,
            0.5+0.5*sin(length(tex_coord*8.0))+0.05*cos(tex_coord.x*20.0)+0.05*cos(tex_coord.y*20.0) );
    }
}
