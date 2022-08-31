#pragma once
#include "ecs.h"
#include <glm/gtc/quaternion.hpp>

struct transform {
    vec3 translation, scale;
    quat rotation;

    mat4 world;

    transform(
        vec3 translation = vec3(0.f),
        quat rotation    = quat(0.f, 0.f, 0.f, 1.f),
        vec3 scale       = vec3(1.f)
    )
        : translation(translation), scale(scale), rotation(rotation), world(1) {}
};

class transform_system : public entity_system<transform> {
    void update_world_transforms(world::entity_handle e, const mat4& T);

  public:
    static const system_id id = (system_id)static_systems::transform;

    transform_system(const std::shared_ptr<world>& w) : entity_system<transform>(w) {}

    void update(const frame_state& fs) override;
    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;

    std::string_view name() const override { return "Transform"; }
};

struct light {
    light_type type{};
    vec3       param, color;
    size_t     _render_index{};

    light() : param(1.f, 0.f, 0.f), color(0.f) {}
};

class light_system : public entity_system<light> {
  public:
    static const system_id id = (system_id)static_systems::light;

    light_system(const std::shared_ptr<world>& w) : entity_system<light>(w) {}

    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;

    void generate_viewport_shapes(
        const std::function<void(viewport_shape)>& add_shape, const frame_state& fs
    ) override;

    std::string_view name() const override { return "Light"; }
};

struct camera {
    float fov;
};

class camera_system : public entity_system<camera> {
  public:
    static const system_id id = (system_id)static_systems::camera;

    std::optional<entity_id> active_camera_id = std::nullopt;

    camera_system(const std::shared_ptr<world>& w) : entity_system<camera>(w) {}

    auto active_camera() { return this->get_data_for_entity(this->active_camera_id.value()); }

    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;

    void generate_viewport_shapes(
        const std::function<void(viewport_shape)>& add_shape, const frame_state& fs
    ) override;

    std::string_view name() const override { return "Camera"; }
};
