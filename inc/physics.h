#pragma once

#include "cmmn.h"
#include "ecs.h"
#include "renderer.h"
#include <reactphysics3d/reactphysics3d.h>

struct rigid_body_component {
    reactphysics3d::RigidBody* body;

    rigid_body_component(class rigid_body_system* sys);
};

struct collider_component {
    reactphysics3d::Collider* collider;

    collider_component(class collider_system* sys);
};

class rigid_body_system : public entity_system<rigid_body_component> {
    reactphysics3d::PhysicsCommon phy;
    reactphysics3d::PhysicsWorld* world;
};

class collider_system : public entity_system<collider_component> {
    reactphysics3d::PhysicsCommon phy;
    reactphysics3d::PhysicsWorld* world;
};

// either self has [transform, rigid body, collider] or self has [transform, rigid body] and some
// children have [collider, transform] always sets self's transform

/*struct rigid_body_trait : public trait {
    reactphysics3d::RigidBody* body;
    reactphysics3d::Transform  initial_transform;
    bool                       should_grab_initial_transform;

    rigid_body_trait(
        trait_factory*             p,
        reactphysics3d::RigidBody* b,
        reactphysics3d::Transform  initial_transform,
        bool                       grab_tf = true
    )
        : trait(p), body(b), should_grab_initial_transform(grab_tf),
          initial_transform(initial_transform) {}

    void append_transform(struct scene_object*, mat4& T, frame_state*) override;
    void postprocess_transform(struct scene_object*, const mat4& T, frame_state*) override;
    void build_gui(struct scene_object*, frame_state*) override;
    json serialize() const override;
    void remove_from(struct scene_object*) override;

    void reset_body();
};

struct physics_system : public entity_system<reactphysics3d::RigidBody*> {
    reactphysics3d::PhysicsCommon phy;
    reactphysics3d::PhysicsWorld*  world;

    trait_id id() const override { return TRAIT_ID_RIGID_BODY; }

    std::string name() const override { return "Rigid Body"; }

    void add_to(struct scene_object* obj, void* create_info) override;
    void deserialize(class scene* scene, struct scene_object* obj, json data) override;

    bool dependencies_loaded(struct scene_object* obj, const json& unloaded_trait) override;

    rigid_body_trait_factory(reactphysics3d::PhysicsCommon* phy, reactphysics3d::PhysicsWorld* wrld)
        : phy(phy), world(wrld) {}
};*/

struct physics_debug_shape_render_node_prototype : public render_node_prototype {
    reactphysics3d::PhysicsWorld* world;
    std::unique_ptr<buffer>       geo_buffer;
    void*                         geo_bufmap;

    physics_debug_shape_render_node_prototype(device* dev, reactphysics3d::PhysicsWorld* world);

    std::unique_ptr<render_node_data> initialize_node_data() override;

    void collect_descriptor_layouts(
        render_node*                           node,
        std::vector<vk::DescriptorPoolSize>&   pool_sizes,
        std::vector<vk::DescriptorSetLayout>&  layouts,
        std::vector<vk::UniqueDescriptorSet*>& outputs
    ) override;
    void update_descriptor_sets(
        renderer*                            r,
        render_node*                         node,
        std::vector<vk::WriteDescriptorSet>& writes,
        arena<vk::DescriptorBufferInfo>&     buf_infos,
        arena<vk::DescriptorImageInfo>&      img_infos
    ) override;
    void generate_pipelines(
        renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
    ) override;
    void generate_command_buffer_inline(
        renderer*          r,
        render_node*       node,
        vk::CommandBuffer& cb,
        size_t             subpass_index,
        const frame_state& fs
    ) override;
    void build_gui(renderer* r, render_node* node) override;

    const char* name() const override { return "Physics Debug Shapes"; }

    size_t id() const override { return 0x0000aaaa; }
};
