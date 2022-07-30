#pragma once
#include "app.h"
#include "cmmn.h"
#include "ecs.h"
#include "uuid.h"
#include <filesystem>

struct material {
    uuids::uuid id;
    std::string name;

    vec3                       base_color;
    std::optional<std::string> diffuse_texpath;

    material(
        std::string                name,
        vec3                       base_color      = vec3(.1f),
        std::optional<std::string> diffuse_texpath = {},
        uuids::uuid                id              = uuids::uuid()
    );

    material(uuids::uuid id, json data);

    json serialize() const;

    bool build_gui(frame_state& fs);

    uint32            _render_index;
    vk::DescriptorSet desc_set;
};

// bundle contains assets + code for a project
// - geometry
// - materials
// - textures
// - render graphs
// - scripts
class bundle {
  public:
    std::vector<std::shared_ptr<class geometry_set>> geometry_sets;
    std::vector<std::shared_ptr<material>>           materials;
    std::unordered_map<std::string, json>            render_graphs;
    std::shared_ptr<material>                        selected_material;

    bundle() : materials_changed(true), selected_material(nullptr) {}

    bundle(device* dev, const std::filesystem::path& path);

    void update(frame_state& fs, class app*);
    void build_gui(frame_state& fs);

    json serialize() const;

    bool materials_changed;
};
