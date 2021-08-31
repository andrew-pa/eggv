#pragma once
#include "cmmn.h"
#include "scene_graph.h"

struct vertex {
    vec3 position, normal;
    vec2 texcoord;
    vertex(vec3 p = vec3(0.f), vec3 n = vec3(0.f), vec2 tx = vec2(0.f))
        : position(p), normal(n), texcoord(tx) {}
};

constexpr static vk::VertexInputAttributeDescription vertex_attribute_description[] = {
    vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, position)},
    vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, normal)},
    vk::VertexInputAttributeDescription{2, 0, vk::Format::eR32G32Sfloat,       offsetof(vertex, texcoord)},
};

#include "device.h"
struct mesh {
    std::unique_ptr<buffer> vertex_buffer;
    std::unique_ptr<buffer> index_buffer;
    size_t vertex_count, index_count;

    mesh(device* dev, size_t vcount, size_t vsize, size_t icount, std::function<void(void*)> write_buffer);

    template<typename VertexT>
    mesh(device* dev, const std::vector<VertexT>& vertices, const std::vector<uint16>& indices)
        : mesh(dev, vertices.size(), sizeof(VertexT), indices.size(), [&](void* staging_map) {
            memcpy(staging_map, vertices.data(), sizeof(VertexT)*vertices.size());
            memcpy((char*)staging_map + sizeof(VertexT)*vertices.size(), indices.data(), sizeof(uint16)*indices.size());
        })
    {
    }

};

struct mesh_create_info {
    std::shared_ptr<class geometry_set> geo_src;
    size_t mesh_index;
    std::shared_ptr<material> mat;
};

const trait_id TRAIT_ID_MESH = 0x00010001;
struct mesh_trait : public trait {
    std::shared_ptr<mesh> m;
    std::shared_ptr<class geometry_set> geo_src;
    size_t mesh_index;
    std::shared_ptr<material> mat;
    aabb bounds;
    mesh_trait(trait_factory* p, mesh_create_info* ci);
    void append_transform(struct scene_object*, mat4& T, frame_state*) override {}
    void build_gui(struct scene_object*, frame_state*) override;
    json serialize() const override;
    void collect_viewport_shapes(struct scene_object*, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) override;
};

struct mesh_trait_factory : public trait_factory {
    trait_id id() const override { return TRAIT_ID_MESH; }
    std::string name() const override { return "Mesh"; }
    void deserialize(struct scene*, struct scene_object* obj, json data) override;
    void add_to(scene_object* obj, void* ci) override {
        obj->traits[id()] = std::make_unique<mesh_trait>(this,
                ((mesh_create_info*)ci));
    }
};

namespace geom_file {
    struct mesh_header {
        size_t vertex_ptr, index_ptr, name_ptr;
        uint16_t name_length;
        uint16_t num_vertices;
        uint32_t num_indices;
        uint32_t material_index;
        vec3 aabb_min, aabb_max;

        mesh_header(size_t vp = 0, size_t ip = 0, size_t np = 0, uint16_t nv = 0, uint32_t ni = 0, uint16_t nl = 0, uint32_t mi = 0, vec3 m = vec3(0), vec3 x = vec3(0))
            : vertex_ptr(vp), index_ptr(ip), name_ptr(np), num_vertices(nv), num_indices(ni), name_length(nl), material_index(mi), aabb_min(m), aabb_max(x) {}
    };
}


