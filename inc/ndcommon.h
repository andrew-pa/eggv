#pragma once
#include <glm/glm.hpp>
using namespace glm;

namespace geom_file {
    struct mesh_header {
        size_t vertex_ptr, index_ptr, name_ptr, hull_ptr;
        uint16_t name_length;
        uint16_t num_vertices;
        uint32_t num_indices;
        uint32_t material_index;
        vec3 aabb_min, aabb_max;

        mesh_header(size_t vp = 0, size_t ip = 0, size_t np = 0, size_t hp = 0,
            uint16_t nv = 0, uint32_t ni = 0, uint16_t nl = 0,
            uint32_t mi = 0, vec3 m = vec3(0), vec3 x = vec3(0))
            : vertex_ptr(vp), index_ptr(ip), name_ptr(np), hull_ptr(hp),
            num_vertices(nv), num_indices(ni), name_length(nl),
            material_index(mi), aabb_min(m), aabb_max(x) {}
    };

	struct vertex {
		vec3 position, normal;
		vec2 texcoord;
		vertex(vec3 p = vec3(0.f), vec3 n = vec3(0.f), vec2 tx = vec2(0.f))
			: position(p), normal(n), texcoord(tx) {}
	};
}

typedef uint32_t trait_id;

// --- trait ids ---
const trait_id TRAIT_ID_TRANSFORM = 0x0000'0001;
const trait_id TRAIT_ID_LIGHT = 0x0000'0010;
const trait_id TRAIT_ID_CAMERA = 0x0000'0011;
const trait_id TRAIT_ID_MESH = 0x0001'0001;
const trait_id TRAIT_ID_RIGID_BODY = 0x000a'0001;
