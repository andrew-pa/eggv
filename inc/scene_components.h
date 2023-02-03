#pragma once
#include "ecs.h"
#include "emlisp_autobind.h"
#include <glm/gtc/quaternion.hpp>

EL_OBJ struct transform {
    EL_PROP(rw) vec3 translation;
    EL_PROP(rw) vec3 scale;
    EL_PROP(rw) quat rotation;

    EL_PROP(r) mat4 world;

    EL_C transform(
        vec3 translation = vec3(0.f),
        quat rotation    = quat(0.f, 0.f, 0.f, 1.f),
        vec3 scale       = vec3(1.f)
    )
        : translation(translation), scale(scale), rotation(rotation), world(1) {}
};

class transform_system : public entity_system<transform> {
    void update_world_transforms(entity e, const mat4& T);

  public:
    static const system_id id = (system_id)static_systems::transform;

    transform_system(const std::shared_ptr<world>& w) : entity_system<transform>(w) {}

    void update(const frame_state& fs) override;
    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;

    std::string_view name() const override { return "Transform"; }
};

EL_OBJ struct light {
    EL_PROP(rw) light_type type;
    EL_PROP(rw) vec3       param;
    EL_PROP(rw) vec3       color;
    size_t     _render_index{};

    EL_C light() : type(light_type::directional), param(1.f, 0.f, 0.f), color(0.f) {}
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

EL_OBJ struct camera {
    EL_PROP(rw) float fov;

    EL_C camera() = default;
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
