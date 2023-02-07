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
    std::optional<std::string> diffuse_tex;

    std::weak_ptr<class bundle> parent_bundle;

    material(
        const std::shared_ptr<class bundle>& parent_bundle,
        std::string                          name,
        vec3                                 base_color  = vec3(.1f),
        std::optional<std::string>           diffuse_tex = {},
        uuids::uuid                          id          = uuids::uuid()
    );

    material(const std::shared_ptr<class bundle>& parent_bundle, uuids::uuid id, json data);

    json serialize() const;

    bool build_gui(frame_state& fs);

    uint32            _render_index;
    vk::DescriptorSet desc_set;
};

struct texture_data {
    uint32_t   width, height;
    vk::Format fmt;
    void*      data;
    size_t     size_bytes;

    texture_data() = default;
    texture_data(const std::filesystem::path& path);

    texture_data(const texture_data&)            = delete;
    texture_data& operator=(const texture_data&) = delete;

    texture_data(texture_data&& other) noexcept
        : width(other.width), height(other.height), fmt(other.fmt), data(other.data),
          size_bytes(other.size_bytes) {
        other.data = nullptr;
    }

    texture_data& operator=(texture_data&& o) noexcept {
        this->width      = o.width;
        this->height     = o.height;
        this->fmt        = o.fmt;
        this->data       = o.data;
        this->size_bytes = o.size_bytes;
        o.data           = nullptr;
        return *this;
    }

    ~texture_data();
};

// bundle contains assets + code for a project
// - geometry
// - materials
// - textures
// - render graphs
// - scripts
EL_OBJ class bundle : public std::enable_shared_from_this<bundle> {
  public:
    std::filesystem::path                                                     root_path;
    std::unordered_map<std::string_view, std::shared_ptr<class geometry_set>> geometry_sets;
    std::vector<std::shared_ptr<material>>                                    materials;
    // TODO: use UUIDs for textures?
    std::unordered_map<std::string, texture_data> textures;
    std::unordered_map<std::string, json>         render_graphs;
    std::shared_ptr<material>                     selected_material;
    bool                                          materials_changed;
    std::string                                   init_script;

    bundle() : selected_material(nullptr), materials_changed(true) {}

    void load(device* dev, const std::filesystem::path& path);
    void save();

    void update(frame_state& fs, class app*);
    void build_gui(frame_state& fs);

    EL_M std::shared_ptr<geometry_set> geometry_set_for_name(std::string_view n) {
        return geometry_sets.at(n);
    }

    EL_M std::shared_ptr<material> material_for_name(std::string_view n) {
        auto f = std::find_if(std::begin(materials), std::end(materials), [&](auto m) {
            return m->name == n;
        });
        return f == std::end(materials) ? nullptr : *f;
    }
};
