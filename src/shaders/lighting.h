
struct material {
    vec3 base_color;
};

vec3 compute_lighting(vec3 nor, vec3 L, vec3 Lcol, material mat) {
    vec3 diffuse = max(0., dot(nor, -L)) * mat.base_color * Lcol;

    return diffuse;
}


