#include "physics.h"
#include "imgui.h"
using namespace reactphysics3d;

void rigid_body_trait::append_transform(scene_object*, mat4& T, frame_state*) {
	// TODO: interpolate between this and the previous frame's transform based on how much time we have left to simulate
	if (body->getType() != BodyType::STATIC) {
		mat4 local_t;
		this->body->getTransform().getOpenGLMatrix(&local_t[0][0]);
		T = local_t;
	}
}

void rigid_body_trait::postprocess_transform(scene_object*, const mat4& T, frame_state*) {
	if (body->getType() == BodyType::STATIC || should_grab_initial_transform) {
		Transform t = Transform::identity();
		t.setFromOpenGL((decimal*)&T[0][0]);
		body->setTransform(t);
		initial_transform = t;
		should_grab_initial_transform = false;
	}
}

const char* body_type_names[] = {
	"Static", "Kinematic", "Dynamic"
};

void rigid_body_trait::build_gui(scene_object*, frame_state*) {
	int body_type = (int)body->getType();
	if (ImGui::Combo("Type", &body_type, body_type_names, 3))
		body->setType((BodyType)body_type);

	auto mass = body->getMass();
	if (ImGui::DragFloat("Mass", &mass, 0.01f))
		body->setMass(mass);
	
	auto pos = initial_transform.getPosition();
	if (ImGui::DragFloat3("Initial Position", &pos.x, 0.01f))
		initial_transform.setPosition(pos);
	auto rot = initial_transform.getOrientation();
	if (ImGui::DragFloat4("Initial Rotation", &rot.x, 0.01f))
		initial_transform.setOrientation(rot);
	if (ImGui::Button("Reset")) {
		body->setTransform(initial_transform);
		body->setLinearVelocity(Vector3());
		body->setAngularVelocity(Vector3());
	}

}

json rigid_body_trait::serialize() const {
	return json{};
}

void rigid_body_trait_factory::add_to(struct scene_object* obj, void* create_info) {
	// this is wrong, we need to sync the scene graph transform and the physics world transform in some way
	obj->traits[id()] = std::make_unique<rigid_body_trait>(this,
		world->createRigidBody(Transform::identity()));
}

void rigid_body_trait_factory::deserialize(struct scene* scene, struct scene_object* obj, json data) {

}

void build_physics_world_gui(frame_state*, bool* window_open, reactphysics3d::PhysicsWorld* world) {
	if (*window_open) {
		ImGui::Begin("Physics World", window_open);
		bool grav_enb = world->isGravityEnabled();
		if (ImGui::Checkbox("Enable Gravity", &grav_enb))
			world->setIsGravityEnabled(grav_enb);
		if (grav_enb) {
			auto grav = world->getGravity();
			if (ImGui::DragFloat3("Gravity", &grav.x, 0.01f))
				world->setGravity(grav);
		}
		ImGui::End();
	}
}
