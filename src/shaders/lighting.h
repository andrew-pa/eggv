
struct material {
    vec4 base_color;
};

vec3 compute_lighting(vec3 nor, vec3 L, vec3 Lcol, material mat, vec3 tex_col) {
    vec3 diffuse = max(0., dot(nor, -L)) * (mat.base_color.xyz*tex_col) * Lcol;

    return diffuse;
}
