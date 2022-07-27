#pragma once
#include "cmmn.h"
#include "ecs.h"
#include "uuid.h"

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

    bool build_gui(frame_state*);

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
    bool materials_changed;

  public:
    std::vector<std::shared_ptr<class geometry_set>> geometry_sets;
    std::vector<std::shared_ptr<material>>           materials;
    std::shared_ptr<material>                        selected_material;

    bundle() : materials_changed(true), selected_material(nullptr) {}

    bundle(class device* dev, const std::filesystem::path& path);

    void update(frame_state* fs, class app*);
    void build_gui(frame_state* fs);

    json serialize() const;
};
