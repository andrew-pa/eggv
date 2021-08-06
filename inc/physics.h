#pragma once
#include "cmmn.h"
#include "scene_graph.h"
#include <reactphysics3d/reactphysics3d.h>

/*
const trait_id TRAIT_ID_PHY_STATIC = 0x000a'0001;
struct static_body_trait : public trait {
	reactphysics3d::CollisionBody* body;
};

struct static_body_trait_factory : public trait_factory {
	reactphysics3d::PhysicsCommon* phy;
	reactphysics3d::PhysicsWorld* world;

	trait_id id() const override { return TRAIT_ID_PHY_STATIC; }
	std::string name() const override { return "Static Physics Body"; }
    void add_to(struct scene_object* obj, void* create_info) override;
    void deserialize(struct scene* scene, struct scene_object* obj, json data) override;

	static_body_trait_factory(reactphysics3d::PhysicsCommon* phy, reactphysics3d::PhysicsWorld* wrld);
};
*/

const trait_id TRAIT_ID_RIGID_BODY = 0x000a'0001;
struct rigid_body_trait : public trait {
	reactphysics3d::RigidBody* body;
	reactphysics3d::Transform initial_transform;
	bool should_grab_initial_transform;

	rigid_body_trait(trait_factory* p, reactphysics3d::RigidBody* b)
		: trait(p), body(b), should_grab_initial_transform(true)
	{
	}

	void append_transform(struct scene_object*, mat4& T, frame_state*) override;
	void postprocess_transform(struct scene_object*, const mat4& T, frame_state*) override;
    void build_gui(struct scene_object*, frame_state*) override;
    json serialize() const override;
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


void build_physics_world_gui(frame_state*, bool* window_open, reactphysics3d::PhysicsWorld* world);

