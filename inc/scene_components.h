#pragma once
#include "ecs.h"

struct transform {
    vec3 translation, scale;
    quat rotation;

    mat4 world;

    transform(
        vec3 translation = vec3(0.f),
        quat rotation = quat(0.f, 0.f, 0.f, 1.f),
        vec3 scale = vec3(1.f)
    ) : translation(translation), scale(scale), rotation(rotation), world(1) {}
};

class transform_system : public entity_system<transform> {
    void update_world_transforms(world::entity_handle e, const mat4& T);
public:
    static const system_id id = (system_id)static_systems::transform;

    void update(const frame_state& fs, world* w) override;
    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;
};

struct light {
    light_type type;
    vec3 param, color;
    size_t _render_index;
};

class light_system : public entity_system<light> {
public:
    static const system_id id = (system_id)static_systems::light;

    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;

    void generate_viewport_shapes(world* w,
            const std::function<void(viewport_shape)>& add_shape,
            const frame_state& fs) override;
};

struct camera {
    float fov;
};

class camera_system : public entity_system<camera> {
public:
    static const system_id id = (system_id)static_systems::camera;

    std::optional<entity_id> active_camera;

    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;

    void generate_viewport_shapes(world* w,
            const std::function<void(viewport_shape)>& add_shape,
            const frame_state& fs) override;
};
