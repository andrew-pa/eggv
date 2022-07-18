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

class bundle {
  public:
    std::vector<std::shared_ptr<class geometry_set>> geometry_sets;
    std::vector<std::shared_ptr<material>>           materials;
    std::shared_ptr<material>                        selected_material;
    bool                                             materials_changed;

    bundle() : selected_material(nullptr), materials_changed(true) {}

    bundle(class device* dev, std::filesystem::path path, json data);

    void update(frame_state* fs, class app*);
    void build_gui(frame_state* fs);

    json serialize() const;
};
