#pragma once
#include "cmmn.h"
#include "scene_graph.h"
#include <reactphysics3d/collision/PolygonVertexArray.h>

using geom_file::vertex;

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

struct mesh_trait : public trait {
    std::shared_ptr<mesh> m;
    std::shared_ptr<class geometry_set> geo_src;
    size_t mesh_index;
    std::shared_ptr<material> mat;
    aabb bounds;
    std::optional<reactphysics3d::PolygonVertexArray> convex_hull;
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
