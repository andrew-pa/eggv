#pragma once
#include "cmmn.h"
#include "scene_graph.h"
#include "renderer.h"
#include <reactphysics3d/reactphysics3d.h>

const trait_id TRAIT_ID_RIGID_BODY = 0x000a'0001;
struct rigid_body_trait : public trait {
	reactphysics3d::RigidBody* body;
	reactphysics3d::Transform initial_transform;
	bool should_grab_initial_transform;

	rigid_body_trait(trait_factory* p, reactphysics3d::RigidBody* b, reactphysics3d::Transform initial_transform, bool grab_tf = true)
		: trait(p), body(b), should_grab_initial_transform(grab_tf), initial_transform(initial_transform)
	{
	}

	void append_transform(struct scene_object*, mat4& T, frame_state*) override;
	void postprocess_transform(struct scene_object*, const mat4& T, frame_state*) override;
    void build_gui(struct scene_object*, frame_state*) override;
    json serialize() const override;
	void remove_from(struct scene_object*) override;

	void reset_body();
};

struct rigid_body_trait_factory : public trait_factory {
	reactphysics3d::PhysicsCommon* phy;
	reactphysics3d::PhysicsWorld* world;

	trait_id id() const override { return TRAIT_ID_RIGID_BODY; }
	std::string name() const override { return "Rigid Body"; }
    void add_to(struct scene_object* obj, void* create_info) override;
    void deserialize(struct scene* scene, struct scene_object* obj, json data) override;

	rigid_body_trait_factory(reactphysics3d::PhysicsCommon* phy, reactphysics3d::PhysicsWorld* wrld)
		: phy(phy), world(wrld) {}
};

struct physics_debug_shape_render_node_prototype : public render_node_prototype {
	reactphysics3d::PhysicsWorld* world;
	std::unique_ptr<buffer> geo_buffer;
	void* geo_bufmap;

    physics_debug_shape_render_node_prototype(device* dev, reactphysics3d::PhysicsWorld* world);

    void collect_descriptor_layouts(struct render_node*, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) override;
    void update_descriptor_sets(class renderer*, struct render_node*, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos) override;
    vk::UniquePipeline generate_pipeline(class renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass);
    void generate_command_buffer_inline(class renderer*, struct render_node*, vk::CommandBuffer&) override;
	void build_gui(class renderer*, struct render_node* node) override;

    virtual std::unique_ptr<render_node_data> deserialize_node_data(json data) { return nullptr; }
	const char* name() const override {
		return "Physics Debug Shapes";
	}
    size_t id() const override { return 0x0000aaaa; }
};

void build_physics_world_gui(frame_state*, bool* window_open, reactphysics3d::PhysicsWorld* world);
